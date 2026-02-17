#ifndef TIMER_LIST_H
#define TIMER_LIST_H

#include"timer.h"
class timer_list {
    public:
        timer_list();
        ~timer_list();
        void add_timer(timer* t);
        void del_timer(timer* t);
        void tick();
        timer* get_header() {return head;}
        timer* head;
        timer* tail;
};
#endif