#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <string>
#include <stdarg.h>
#include <list>
#include <pthread.h>
#include "locker.h"
#include "sem.h"

// 日志级别
enum LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    ERROR
};

class Log {
public:
    // 获取日志单例
    static Log* get_instance();
    
    // 初始化日志系统
    // log_file_name: 日志文件名
    // max_lines: 日志文件最大行数，超过后自动分割
    // log_level: 最小日志级别
    // is_async: 是否启用异步日志
    // max_queue_size: 异步队列容量（仅在 is_async=true 时生效）
    bool init(const char* log_file_name,
              int max_lines = 50000,
              LogLevel log_level = INFO,
              bool is_async = false,
              int max_queue_size = 1024);
    
    // 写日志
    void write_log(LogLevel level, const char* format, ...);
    
    // 刷新缓冲区
    void flush();
    
    // 设置日志级别
    void set_log_level(LogLevel level);

private:
    Log();
    ~Log();
    
    // 异步线程入口
    static void* async_flush_thread(void* arg);

    // 异步线程循环
    void async_write_loop();

    // 生成时间戳字符串，并返回 (year, month, day)
    void format_time(char* buffer, size_t size, int& year, int& month, int& day);

    // 打开/切换日志文件（同一天的分片通过 index 区分）
    bool open_log_file(int year, int month, int day, int index);

    // 写入一行日志（包含分片/按天切换逻辑）
    void write_line(int year, int month, int day, const std::string& line);
    
    // 获取日志级别字符串
    const char* get_level_str(LogLevel level);

private:
    char dir_name[128];          // 日志文件目录
    char log_name[128];          // 日志文件名
    FILE* m_fp;                  // 日志文件指针
    int m_max_lines;             // 日志最大行数
    long long m_count;           // 当前日志行数
    int m_today;                 // 记录当前日期
    locker m_mutex;              // 互斥锁
    LogLevel m_log_level;        // 日志级别

    // --- async ---
    bool m_is_async;             // 是否异步
    bool m_stop;                 // 线程退出标记
    int m_max_queue_size;        // 异步队列容量
    pthread_t m_tid;             // 异步写线程

    struct LogItem {
        int year;
        int month;
        int day;
        std::string line;
    };
    std::list<LogItem> m_queue;  // 异步队列
    locker m_queue_mutex;        // 队列锁
    sem m_queue_sem;             // 队列计数信号量

    // --- rotation state ---
    int m_cur_year;
    int m_cur_month;
    int m_cur_day;
    int m_file_index;            // 当天分片序号，从 0 开始
};

// 便捷宏定义
#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(DEBUG, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  Log::get_instance()->write_log(INFO, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  Log::get_instance()->write_log(WARN, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(ERROR, format, ##__VA_ARGS__)

#endif
