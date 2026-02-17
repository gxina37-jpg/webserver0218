#include"timer.h"
#include<sys/time.h>
#include<cstdio>
#include<unistd.h>
#include"macro.h"

timer::timer() {
    pre = nullptr;
    ne = nullptr;
}

timer::~timer() {
    delete user_data;
    user_data = nullptr;
}


void timer::default_cb_func(client_data* user_data) {
    if (user_data) {
        close(user_data->sockfd);
    }
}


void timer::set_timer(timer* t, int timeout) {
    struct timeval now;
    gettimeofday(&now, NULL);
    t -> expire = now.tv_sec + timeout;
}

void timer::init(sockaddr_in addr, int sockfd) {
    this -> user_data = new client_data;
    set_timer(this, TIMEOUT);
    user_data -> address = addr;
    user_data -> sockfd = sockfd;
}