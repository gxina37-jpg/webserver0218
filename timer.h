#ifndef TIMER_H
#define TIMER_H

#include<netinet/in.h>

class client_data {
    public:
        sockaddr_in address;
        int sockfd;
};

class timer {
    public:
        timer();
        ~timer();
        void init(sockaddr_in addr, int sockfd);
        time_t expire;
        client_data* user_data;
        void set_timer(timer* t, int timeout);
        void (*cb_func) (client_data*);
        static void default_cb_func(client_data* user_data);
        timer* pre;
        timer* ne;
};



#endif