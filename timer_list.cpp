#include"timer_list.h"
#include<cstdio>
#include<time.h>
#include"macro.h"
#include"log.h"
timer_list::timer_list() {
    head = nullptr;
    tail = nullptr;
}
timer_list::~timer_list() {
    timer* tmp = head;
    while (tmp) {
        head = head -> ne;
        delete tmp;
        tmp = head;
    }
}

void timer_list::add_timer(timer* t) {
    if (!t) {
        return ;
    }
    if (!head) {
        head = t;
        tail = t;
        return ;
    }
    tail -> ne = t;
    t -> pre = tail;
    tail = t;

}


void timer_list::del_timer(timer* t) {
    if (!t) {
        return ;
    }
    if (t == head && t == tail) {
        delete t;
        head = nullptr;
        tail = nullptr;
        return  ;
    }
    if (t == head) {
        head = head -> ne;
        head -> pre = nullptr;
        delete t;
        return ;
    }
    if (t == tail) {
        tail = tail -> pre;
        tail -> ne = nullptr;
        delete t;
        return ;
    }
    t -> pre -> ne = t -> ne;
    t -> ne -> pre = t -> pre;
    delete t;
}

void timer_list::tick() {
    if (!head) {
        return ;
    }
    printf("timer tick\n");
    time_t cur = time(NULL);
    timer* tmp = head;
    while (tmp) {
        if (cur < tmp -> expire) {
            break;
        }
        printf("sockfd %d expired\n", tmp -> user_data -> sockfd);
        LOG_INFO("sockfd %d closed", tmp -> user_data -> sockfd);
        tmp -> cb_func(tmp -> user_data);
        head = tmp -> ne;
        if (head) {
            head -> pre = nullptr;
        }     
        delete tmp;
        tmp = head;
    }
}