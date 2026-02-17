#include "log.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <errno.h>

Log::Log() {
    memset(dir_name, 0, sizeof(dir_name));
    memset(log_name, 0, sizeof(log_name));

    m_fp = nullptr;
    m_max_lines = 0;
    m_count = 0;
    m_today = 0;
    m_log_level = INFO;

    m_is_async = false;
    m_stop = false;
    m_max_queue_size = 0;
    m_tid = 0;

    m_cur_year = 0;
    m_cur_month = 0;
    m_cur_day = 0;
    m_file_index = 0;
}

Log::~Log() {
    // 尽量优雅退出异步线程
    if (m_is_async) {
        m_stop = true;
        // 唤醒线程，让其退出并尽可能刷掉队列
        m_queue_sem.post();
        // join 可能在信号终止时来不及走到这里，但正常退出可回收资源
        pthread_join(m_tid, nullptr);
    }

    if (m_fp != nullptr) {
        fflush(m_fp);
        fclose(m_fp);
        m_fp = nullptr;
    }
}

Log* Log::get_instance() {
    static Log instance;
    return &instance;
}

void Log::format_time(char* buffer, size_t size, int& year, int& month, int& day) {
    struct timeval now {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm tm_now {};
    localtime_r(&t, &tm_now);

    year = tm_now.tm_year + 1900;
    month = tm_now.tm_mon + 1;
    day = tm_now.tm_mday;

    snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
             year, month, day,
             tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
             (long)now.tv_usec);
}

bool Log::open_log_file(int year, int month, int day, int index) {
    char full_name[512] = {0};
    if (index <= 0) {
        snprintf(full_name, sizeof(full_name), "%s%04d_%02d_%02d_%s",
                 dir_name, year, month, day, log_name);
    } else {
        snprintf(full_name, sizeof(full_name), "%s%04d_%02d_%02d_%s.%d",
                 dir_name, year, month, day, log_name, index);
    }

    FILE* new_fp = fopen(full_name, "a");
    if (new_fp == nullptr) {
        return false;
    }

    // 先开新文件成功，再关闭旧文件，避免切换失败导致 m_fp 为空
    if (m_fp != nullptr) {
        fflush(m_fp);
        fclose(m_fp);
    }

    m_fp = new_fp;
    return true;
}

bool Log::init(const char* file_name,
               int max_lines,
               LogLevel log_level,
               bool is_async,
               int max_queue_size) {
    m_max_lines = max_lines;
    m_log_level = log_level;

    // 解析路径：确保 dir_name/log_name 都有正确的 \0 结尾
    memset(dir_name, 0, sizeof(dir_name));
    memset(log_name, 0, sizeof(log_name));

    const char* p = strrchr(file_name, '/');
    if (p == nullptr) {
        // 没有路径
        strncpy(log_name, file_name, sizeof(log_name) - 1);
        dir_name[0] = '\0';
    } else {
        size_t dir_len = (size_t)(p - file_name + 1);
        if (dir_len >= sizeof(dir_name)) dir_len = sizeof(dir_name) - 1;
        strncpy(dir_name, file_name, dir_len);
        dir_name[dir_len] = '\0';
        strncpy(log_name, p + 1, sizeof(log_name) - 1);
    }

    // 初始化日期与文件
    struct timeval now {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm tm_now {};
    localtime_r(&t, &tm_now);

    m_cur_year = tm_now.tm_year + 1900;
    m_cur_month = tm_now.tm_mon + 1;
    m_cur_day = tm_now.tm_mday;
    m_today = m_cur_day;
    m_file_index = 0;
    m_count = 0;

    if (!open_log_file(m_cur_year, m_cur_month, m_cur_day, 0)) {
        return false;
    }

    // 异步开关
    m_is_async = is_async;
    m_stop = false;
    m_max_queue_size = (max_queue_size > 0 ? max_queue_size : 1024);

    if (m_is_async) {
        int ret = pthread_create(&m_tid, nullptr, async_flush_thread, this);
        if (ret != 0) {
            // 创建线程失败：降级为同步，不影响服务器启动
            m_is_async = false;
            // 不在这里写日志，避免递归；打印到 stderr
            fprintf(stderr, "[Log] failed to create async thread, fallback to sync. errno=%d\n", ret);
        }
    }

    return true;
}

void* Log::async_flush_thread(void* arg) {
    Log* self = static_cast<Log*>(arg);
    self->async_write_loop();
    return nullptr;
}

void Log::async_write_loop() {
    while (true) {
        m_queue_sem.wait();

        // 先把队列中的消息取出来
        LogItem item;
        bool has_item = false;
        m_queue_mutex.lock();
        if (!m_queue.empty()) {
            item = std::move(m_queue.front());
            m_queue.pop_front();
            has_item = true;
        }
        bool should_stop = m_stop && m_queue.empty();
        m_queue_mutex.unlock();

        if (has_item) {
            write_line(item.year, item.month, item.day, item.line);
        }

        if (should_stop) {
            break;
        }
    }

    // 退出前再尝试刷掉残留
    while (true) {
        LogItem item;
        m_queue_mutex.lock();
        if (m_queue.empty()) {
            m_queue_mutex.unlock();
            break;
        }
        item = std::move(m_queue.front());
        m_queue.pop_front();
        m_queue_mutex.unlock();

        write_line(item.year, item.month, item.day, item.line);
    }
}

void Log::write_line(int year, int month, int day, const std::string& line) {
    m_mutex.lock();

    if (m_fp == nullptr) {
        m_mutex.unlock();
        return;
    }

    // 日期变化：切换到新日期文件
    if (year != m_cur_year || month != m_cur_month || day != m_cur_day) {
        m_cur_year = year;
        m_cur_month = month;
        m_cur_day = day;
        m_today = day;
        m_file_index = 0;
        m_count = 0;
        if (!open_log_file(year, month, day, 0)) {
            // 打不开新文件则继续使用旧文件（若有）
        }
    }

    // 行数超过上限：同一天分片
    if (m_max_lines > 0 && m_count >= m_max_lines) {
        m_file_index++;
        m_count = 0;
        if (!open_log_file(year, month, day, m_file_index)) {
            // 打不开新文件则继续使用旧文件（若有）
        }
    }

    if (m_fp == nullptr) {
        m_mutex.unlock();
        return;
    }

    // 写入
    fputs(line.c_str(), m_fp);
    fflush(m_fp);
    m_count++;

    m_mutex.unlock();
}

void Log::write_log(LogLevel level, const char* format, ...) {
    // 如果日志级别低于设定级别，不记录
    if (level < m_log_level) {
        return;
    }

    // 时间戳
    char time_str[64] = {0};
    int year = 0, month = 0, day = 0;
    format_time(time_str, sizeof(time_str), year, month, day);

    // 内容
    char msg_buf[1024] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(msg_buf, sizeof(msg_buf), format, args);
    va_end(args);

    // 组合成完整行
    std::string line;
    line.reserve(64 + 16 + strlen(msg_buf) + 4);
    line.append("[").append(time_str).append("] ");
    line.append("[").append(get_level_str(level)).append("] ");
    line.append(msg_buf);
    line.append("\n");

    if (!m_is_async) {
        write_line(year, month, day, line);
        return;
    }

    // 异步：入队
    bool queued = false;
    m_queue_mutex.lock();
    if ((int)m_queue.size() < m_max_queue_size) {
        m_queue.push_back(LogItem{year, month, day, std::move(line)});
        queued = true;
    }
    m_queue_mutex.unlock();

    if (queued) {
        m_queue_sem.post();
    } else {
        // 队列满：降级同步写入，避免丢日志（也避免阻塞业务线程等待队列空间）
        write_line(year, month, day, line);
    }
}

void Log::flush() {
    m_mutex.lock();
    if (m_fp) fflush(m_fp);
    m_mutex.unlock();
}

void Log::set_log_level(LogLevel level) {
    m_mutex.lock();
    m_log_level = level;
    m_mutex.unlock();
}

const char* Log::get_level_str(LogLevel level) {
    switch (level) {
        case DEBUG: return "DEBUG";
        case INFO:  return "INFO ";
        case WARN:  return "WARN ";
        case ERROR: return "ERROR";
        default:    return "UNKN ";
    }
}
