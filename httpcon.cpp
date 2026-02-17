#include<arpa/inet.h>
#include"httpcon.h"
#include<unistd.h>
#include<errno.h>
#include<sys/epoll.h>
#include<cstring>
#include<sys/mman.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<cstdlib>
#include<cstdio>
#include<stdarg.h>
#include<sys/uio.h>
#include"timer.h"
#include"log.h"

int httpcon::m_user_count = 0;     // 必须定义
int httpcon::m_epollfd = -1;       // 必须定义

const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

// 网站的根目录
const char* doc_root = "/home/nowcoder/Linux/server_with_features/resources";



void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLRDHUP | EPOLLONESHOT;
    epoll_ctl (epollfd, EPOLL_CTL_MOD, fd, &event);
}

httpcon::httpcon() {
    init();
}
httpcon::~httpcon() {
    closecon();
}
void httpcon::init() {
    m_read_index = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_sockfd = -1;
    m_address = {0};    
    m_write_index = 0;
    m_start_line = 0;
    m_checked_index = 0;
    m_iv_count = 0;
    m_content_length = 0;
    m_method = GET;
    m_linger = false;
    m_timer = nullptr;
}
void httpcon::reset() {
    m_read_index = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_write_index = 0;
    m_start_line = 0;
    m_checked_index = 0;
    m_iv_count = 0;
    m_content_length = 0;
    m_method = GET;
    m_linger = false;
    // 不要动 m_sockfd / m_address / m_timer
}
void httpcon::init (int sockfd, sockaddr_in addr) {
    init();
    m_sockfd = sockfd;
    m_address = addr;
    m_user_count++;
}

void httpcon::closecon() {
    if (m_sockfd != -1) {
        close (m_sockfd);
        m_sockfd = -1;
        m_user_count --;
    }
}

bool httpcon::read() {
    if (m_read_index >= READ_BUFFER_SIZE) {
        return false;
    }
    while (1) {
        int byte_read = recv (m_sockfd, m_read_buffer + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);
        if (byte_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }else{
                return false;
            }
        }else if (byte_read == 0) {
            return false;
        }
        m_read_index += byte_read;
    }
    return true;
}

bool httpcon::write() {
    int bytes_to_send = 0;
    if (m_iv_count >= 1) bytes_to_send += (int)m_iv[0].iov_len;
    if (m_iv_count == 2) bytes_to_send += (int)m_iv[1].iov_len;

    if (bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        reset();
        return true;
    }

    while (1) {
        int temp = writev(m_sockfd, m_iv, m_iv_count);
        if (temp <= -1) {
            if (errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        // 根据写了多少，调整 iovec（关键！）
        if (temp >= (int)m_iv[0].iov_len) {
            temp -= (int)m_iv[0].iov_len;
            m_iv[0].iov_len = 0;

            if (m_iv_count == 2) {
                m_iv[1].iov_base = (char*)m_iv[1].iov_base + temp;
                m_iv[1].iov_len  -= temp;
            }
        } else {
            m_iv[0].iov_base = (char*)m_iv[0].iov_base + temp;
            m_iv[0].iov_len  -= temp;
        }

        bytes_to_send = 0;
        if (m_iv_count >= 1) bytes_to_send += (int)m_iv[0].iov_len;
        if (m_iv_count == 2) bytes_to_send += (int)m_iv[1].iov_len;

        if (bytes_to_send <= 0) {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);
            reset();
            return true;
        }
    }
}


httpcon::LINE_STATUS httpcon::parse_line() {
    char temp;
    for ( ; m_checked_index < m_read_index; ++m_checked_index ) {
        temp = m_read_buffer[ m_checked_index  ];
        if ( temp == '\r' ) {
            if ( ( m_checked_index + 1 ) == m_read_index ) {
                return LINE_OPEN;
            } else if ( m_read_buffer[ m_checked_index  + 1 ] == '\n' ) {
                m_read_buffer[ m_checked_index ++ ] = '\0';
                m_read_buffer[ m_checked_index ++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if( temp == '\n' )  {
            if( ( m_checked_index  > 1) && ( m_read_buffer[ m_checked_index  - 1 ] == '\r' ) ) {
                m_read_buffer[ m_checked_index -1 ] = '\0';
                m_read_buffer[ m_checked_index ++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}


httpcon::HTTP_CODE httpcon::do_request()
{
    strcpy( m_real_file, doc_root );
    int len = strlen( doc_root );
    strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
    
    LOG_INFO("Requesting file: %s", m_real_file);
    
    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if ( stat( m_real_file, &m_file_stat ) < 0 ) {
        LOG_WARN("File not found: %s", m_real_file);
        return NO_RESOURCE;
    }

    // 判断访问权限
    if ( ! ( m_file_stat.st_mode & S_IROTH ) ) {
        LOG_WARN("Permission denied: %s", m_real_file);
        return FORBIDDEN_REQUEST;
    }

    // 判断是否是目录
    if ( S_ISDIR( m_file_stat.st_mode ) ) {
        LOG_WARN("Request is a directory: %s", m_real_file);
        return BAD_REQUEST;
    }

    // 以只读方式打开文件
    int fd = open( m_real_file, O_RDONLY );
    // 创建内存映射
    m_file_address = ( char* ) mmap ( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    
    LOG_INFO("File request successful: %s, size=%ld bytes", m_real_file, m_file_stat.st_size);
    return FILE_REQUEST;
}

// 对内存映射区执行munmap操作
void httpcon::unmap() {
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = 0;
    }
}

httpcon::HTTP_CODE httpcon::parse_request_line(char* text) {
    // GET /index.html HTTP/1.1
    LOG_DEBUG("Parsing request line: %s", text);
    m_url = strpbrk(text, " \t"); // 判断第二个参数中的字符哪个在text中最先出现
    if (! m_url) { 
        LOG_WARN("Bad request: no URL found");
        return BAD_REQUEST;
    }
    // GET\0/index.html HTTP/1.1
    *m_url++ = '\0';    // 置位空字符，字符串结束符
    char* method = text;
    if ( strcasecmp(method, "GET") == 0 ) { // 忽略大小写比较
        m_method = GET;
    } else {
        return BAD_REQUEST;
    }
    // /index.html HTTP/1.1
    // 检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
    m_version = strpbrk( m_url, " \t" );
    if (!m_version) {
        return BAD_REQUEST;
    }
    // /index.html\0HTTP/1.1
    *m_version++ = '\0';
    if (strcasecmp( m_version, "HTTP/1.1") != 0 ) {
        return BAD_REQUEST;
    }
    /**
     * http://192.168.110.129:10000/index.html
    */
    if (strncasecmp(m_url, "http://", 7) == 0 ) {   
        m_url += 7;
        // 在参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置。
        m_url = strchr( m_url, '/' );
    }
    if ( !m_url || m_url[0] != '/' ) {
        LOG_WARN("Bad request: invalid URL format");
        return BAD_REQUEST;
    }
    LOG_INFO("HTTP Request: %s %s %s", method, m_url, m_version);
    m_check_state = CHECK_STATE_HEADER; // 检查状态变成检查头
    return NO_REQUEST;
}

// 解析HTTP请求的一个头部信息
httpcon::HTTP_CODE httpcon::parse_headers(char* text) {   
    // 遇到空行，表示头部字段解析完毕
    if( text[0] == '\0' ) {
        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
        // 状态机转移到CHECK_STATE_CONTENT状态
        if ( m_content_length != 0 ) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        // 否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    } else if ( strncasecmp( text, "Connection:", 11 ) == 0 ) {
        // 处理Connection 头部字段  Connection: keep-alive
        text += 11;
        text += strspn( text, " \t" );
        if ( strcasecmp( text, "keep-alive" ) == 0 ) {
            m_linger = true;
        }
    } else if ( strncasecmp( text, "Content-Length:", 15 ) == 0 ) {
        // 处理Content-Length头部字段
        text += 15;
        text += strspn( text, " \t" );
        m_content_length = atol(text);
    } else if ( strncasecmp( text, "Host:", 5 ) == 0 ) {
        // 处理Host头部字段
        text += 5;
        text += strspn( text, " \t" );
        m_host = text;
    } else {
        //printf( "oop! unknow header %s\n", text );
    }
    return NO_REQUEST;
}

httpcon::HTTP_CODE httpcon::parse_content( char* text ) {
    if ( m_read_index >= ( m_content_length + m_checked_index ) )
    {
        text[ m_content_length ] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
httpcon::HTTP_CODE httpcon::process_READ() {
    HTTP_CODE ret = NO_REQUEST;
    LINE_STATUS line_status = LINE_OK;
    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) || ((line_status = parse_line()) == LINE_OK)) {
        char* text = get_line();
        printf("got 1 http line: %s\n", text);
        m_start_line = m_checked_index;
        switch(m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                ret = parse_headers(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }else if (ret == GET_REQUEST) {
                    return do_request();
                }
                break;
            }
            case CHECK_STATE_CONTENT: {
                ret = parse_content(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }else if (ret == GET_REQUEST) {
                    return do_request();
                }
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

bool httpcon::add_response( const char* format, ... ) {
    if( m_write_index >= WRITE_BUFFER_SIZE ) {
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_write_buffer + m_write_index, WRITE_BUFFER_SIZE - 1 - m_write_index, format, arg_list );
    if( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_index ) ) {
        return false;
    }
    m_write_index += len;
    va_end( arg_list );
    return true;
}

bool httpcon::add_statu_line( int status, const char* title ) {
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

bool httpcon::add_headers(int content_len) {
    return add_content_length(content_len) &&
           add_content_type() &&
           add_linger() &&
           add_blank_line();
}


bool httpcon::add_content_length(int content_len) {
    return add_response( "Content-Length: %d\r\n", content_len );
}

bool httpcon::add_linger()
{
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}

bool httpcon::add_blank_line()
{
    return add_response( "%s", "\r\n" );
}

bool httpcon::add_content( const char* content )
{
    return add_response( "%s", content );
}

static const char* get_mime_type(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(dot, ".png")) return "image/png";
    if (!strcasecmp(dot, ".gif")) return "image/gif";
    if (!strcasecmp(dot, ".html") || !strcasecmp(dot, ".htm")) return "text/html";
    if (!strcasecmp(dot, ".css")) return "text/css";
    if (!strcasecmp(dot, ".js")) return "application/javascript";
    return "application/octet-stream";
}

bool httpcon::add_content_type() {
    return add_response("Content-Type: %s\r\n", get_mime_type(m_real_file));
}



bool httpcon::process_WRITE(httpcon::HTTP_CODE ret) {
    switch (ret)
    {
        case INTERNAL_ERROR:
            add_statu_line( 500, error_500_title );
            add_headers( strlen( error_500_form ) );
            if ( ! add_content( error_500_form ) ) {
                return false;
            }
            break;
        case BAD_REQUEST:
            add_statu_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) ) {
                return false;
            }
            break;
        case NO_RESOURCE:
            add_statu_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) ) {
                return false;
            }
            break;
        case FORBIDDEN_REQUEST:
            add_statu_line( 403, error_403_title );
            add_headers(strlen( error_403_form));
            if ( ! add_content( error_403_form ) ) {
                return false;
            }
            break;
        case FILE_REQUEST:
            add_statu_line(200, ok_200_title );
            add_headers(m_file_stat.st_size);
            m_iv[ 0 ].iov_base = m_write_buffer;
            m_iv[ 0 ].iov_len = m_write_index;
            m_iv[ 1 ].iov_base = m_file_address;
            m_iv[ 1 ].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            return true;
        default:
            return false;
    }

    m_iv[ 0 ].iov_base = m_write_buffer;
    m_iv[ 0 ].iov_len = m_write_index;
    m_iv_count = 1;
    return true;
}

void httpcon::process() {
    HTTP_CODE read_ret = process_READ();
    if (read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return ;
    }
    
    LOG_DEBUG("Processing request, result code: %d", read_ret);
    
    bool write_ret = process_WRITE(read_ret);
    if (!write_ret) {
        LOG_ERROR("Response generation failed, closing connection");
        closecon();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

