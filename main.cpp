#include<iostream>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<unistd.h>
#include"macro.h"
#include"httpcon.h"
#include"pool.h"
#include<fcntl.h>
#include<sys/time.h>
#include"timer_list.h"
#include"timer.h"
#include"log.h"
#include"access_control.h"
#include <string>
#include <cstring>

timer_list timerlist;
static void setnonblocking(int fd) {
    int old = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old | O_NONBLOCK);
}

void addfd (int epollfd, int fd, bool oneshoot) {
    setnonblocking(fd); 
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    if (oneshoot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl (epollfd, EPOLL_CTL_ADD, fd, &event);
}
static void print_usage(const char* prog) {
    std::cout
        << "Usage: " << prog << " [options]\n"
        << "Options:\n"
        << "  --log-async                 Enable async logging\n"
        << "  --log-sync                  Enable sync logging (default)\n"
        << "  --whitelist <file>           Load IP whitelist from file (default: ./whitelist.conf)\n"
        << "  --blacklist <file>           Load IP blacklist from file (default: ./blacklist.conf)\n"
        << "  -h, --help                   Show help\n";
}

int main (int argc, char* argv[]) {
    // --------- parse args ---------
    bool async_log = true;
    int log_queue_size = 1024;
    std::string whitelist_file = "./whitelist.conf";
    std::string blacklist_file = "./blacklist.conf";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--log-async") == 0) {
            async_log = true;
        } else if (strcmp(argv[i], "--log-sync") == 0) {
            async_log = false;
        } else if (strcmp(argv[i], "--whitelist") == 0 && i + 1 < argc) {
            whitelist_file = argv[++i];
        } else if (strcmp(argv[i], "--blacklist") == 0 && i + 1 < argc) {
            blacklist_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    // --------- init log ---------
    // 初始化日志系统：支持同步/异步
    if (!Log::get_instance()->init("./ServerLog", 50000, DEBUG, async_log, log_queue_size)) {
        std::cerr << "Failed to initialize log system" << std::endl;
        return 1;
    }
    LOG_INFO("========== Server Start ==========");

    LOG_INFO("Log mode: %s", async_log ? "ASYNC" : "SYNC");

    // --------- access control ---------
    AccessControl access;
    int wl = access.load_whitelist(whitelist_file);
    int bl = access.load_blacklist(blacklist_file);
    if (wl > 0) {
        LOG_INFO("Whitelist loaded: %d entries (%s)", wl, whitelist_file.c_str());
    } else {
        LOG_INFO("Whitelist file not loaded or empty (%s)", whitelist_file.c_str());
    }
    if (bl > 0) {
        LOG_INFO("Blacklist loaded: %d entries (%s)", bl, blacklist_file.c_str());
    } else {
        LOG_INFO("Blacklist file not loaded or empty (%s)", blacklist_file.c_str());
    }
    LOG_INFO("Access control enabled: %s", access.enabled() ? "YES" : "NO");
    
    int fd = socket (AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        LOG_ERROR("Failed to create socket");
        return 1;
    }
    LOG_INFO("Socket created successfully, fd=%d", fd);
    
    sockaddr_in saddr;
    httpcon *users = new httpcon[MAX_USER_NUMBER];
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons (PORT);
    pool<httpcon> *m_pool = nullptr;
    m_pool = new pool<httpcon>;
    LOG_INFO("Thread pool created successfully");
    
    if (bind (fd, (const sockaddr*) &saddr, sizeof (saddr)) == -1) {
        LOG_ERROR("Failed to bind socket to port %d", PORT);
        return 1;
    }
    LOG_INFO("Socket bound to port %d", PORT);
    
    if (listen (fd, 10) == -1) {
        LOG_ERROR("Failed to listen on socket");
        return 1;
    }
    std::cout << "Server is listening on port " << PORT << std::endl;
    LOG_INFO("Server is listening on port %d", PORT);
    int epfd = epoll_create (1);
    if (epfd == -1) {
        LOG_ERROR("Failed to create epoll");
        return 1;
    }
    LOG_INFO("Epoll created successfully, epfd=%d", epfd);
    httpcon::m_epollfd = epfd;
    addfd (epfd, fd, false);
    epoll_event events[MAX_USER_NUMBER];
    while (1) {
        int number = epoll_wait (epfd, events, MAX_USER_NUMBER, 1000);
        timerlist.tick();
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == fd) {
                sockaddr_in caddr;
                socklen_t len = sizeof (caddr);
                int connfd = accept (fd, (sockaddr*) &caddr, &len);
                if (connfd == -1) {
                    LOG_ERROR("Failed to accept connection");
                    continue;
                }

                // 访问控制：白名单/黑名单
                if (access.enabled() && !access.allow(caddr)) {
                    std::string ip = AccessControl::ip_to_string(caddr);
                    LOG_WARN("Access denied for %s:%d, connfd=%d", ip.c_str(), ntohs(caddr.sin_port), connfd);
                    close(connfd);
                    continue;
                }
                
                if (httpcon::m_user_count >= MAX_USER_NUMBER) {
                    LOG_WARN("Max user limit reached, rejecting connection");
                    close (connfd);
                    continue;
                }
                
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &caddr.sin_addr, client_ip, INET_ADDRSTRLEN);
                LOG_INFO("New connection from %s:%d, connfd=%d", 
                client_ip, ntohs(caddr.sin_port), connfd);                    
                addfd (epfd, connfd, true);
                users[connfd].init (connfd, caddr);
                timer* temp = new timer;
                temp -> init(caddr, connfd);
                temp -> cb_func = timer::default_cb_func;
                timerlist.add_timer(temp);
                users[connfd].m_timer = temp;
                std::cout << "New connection accepted" << std::endl;
            }else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                LOG_INFO("Connection closed, sockfd=%d", sockfd);
                std::cout << "Connection closed" << std::endl;
                timerlist.del_timer(users[sockfd].m_timer);
                users[sockfd].closecon();
            }else if (events[i].events & EPOLLIN) {
                LOG_DEBUG("EPOLLIN event on sockfd=%d", sockfd);
                timer* temp = users[sockfd].m_timer;
                if (temp) {
                    temp -> set_timer(temp, TIMEOUT);
                }
                if (users[sockfd].read()) {
                    if (m_pool -> append (users + sockfd)) {
                        LOG_DEBUG("Request appended to thread pool, sockfd=%d", sockfd);
                        std::cout << "Request appended to the thread pool" << std::endl;
                    }else{
                        LOG_ERROR("Failed to append request to thread pool, sockfd=%d", sockfd);
                        timerlist.del_timer(users[sockfd].m_timer);
                        users[sockfd].closecon();
                    }
                }else{
                    LOG_WARN("Read failed, closing connection, sockfd=%d", sockfd);
                    timerlist.del_timer(users[sockfd].m_timer);
                    users[sockfd].closecon();
                }
            }else if (events[i].events & EPOLLOUT) {
                LOG_DEBUG("EPOLLOUT event on sockfd=%d", sockfd);
                timer* temp = users[sockfd].m_timer;
                if (temp) {
                    temp -> set_timer(temp, TIMEOUT);
                }
                if (!users[sockfd].write()) {
                    LOG_WARN("Write failed, closing connection, sockfd=%d", sockfd);
                    timerlist.del_timer(users[sockfd].m_timer);
                    users[sockfd].closecon();
                }else{
                    LOG_DEBUG("Response sent successfully, sockfd=%d", sockfd);
                }
            }
        }
    }
    LOG_INFO("Server shutting down...");
    close (fd);
    close (epfd);
    delete m_pool;
    delete[] users;
    LOG_INFO("========== Server Stop ==========");
    return 0;
}