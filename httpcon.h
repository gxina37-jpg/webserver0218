#ifndef HTTPCON_H
#define HTTPCON_H

#include<arpa/inet.h>
#include<unistd.h>
#include<sys/stat.h>
#include<fcntl.h>
#include"timer.h"

class httpcon {
    public:
        enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};
        enum METHOD {GET = 0, POST, HEAD, PUT,  DELETE, TRACE, OPTIONS, CONNECT};
        enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
        enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
        static const int FILENAME_LEN = 200;
        static const int READ_BUFFER_SIZE = 2048;
        static const int WRITE_BUFFER_SIZE = 1024;
        static int m_epollfd;
        static int m_user_count;
        httpcon();
        ~httpcon();
        void init(int sockfd, sockaddr_in addr);
        void closecon();
        void init();
        bool read();
        void reset();
        void process();
        bool write();
        timer* m_timer;
    private:
        int m_sockfd;
        sockaddr_in m_address;
        HTTP_CODE process_READ();
        bool process_WRITE(HTTP_CODE ret);
        HTTP_CODE parse_request_line(char* text);
        HTTP_CODE parse_headers(char* text);
        HTTP_CODE parse_content(char* text);
        HTTP_CODE do_request();
        char m_read_buffer[READ_BUFFER_SIZE];
        char m_write_buffer[WRITE_BUFFER_SIZE];
        int m_read_index;
        int m_write_index;
        int m_start_line;
        int m_checked_index;
        int m_iv_count;
        CHECK_STATE m_check_state;
        char* get_line() {return m_read_buffer + m_start_line;}
        char* m_file_address;
        char* m_version;
        char* m_url;
        bool m_linger;
        int m_content_length;
        METHOD m_method;
        char m_real_file[FILENAME_LEN];
        char* m_host;
        LINE_STATUS parse_line();
        void unmap();
        bool add_response(const char* format, ...);
        bool add_content(const char* content);
        bool add_content_type();
        bool add_statu_line(int status, const char* title);
        bool add_headers(int content_length);
        bool add_content_length (int content_length);
        bool add_blank_line();
        bool add_linger();
        struct stat m_file_stat;                // 目标文件的状态。通过它我们可以判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息
        struct iovec m_iv[2];                   // 我们将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量。
};



#endif