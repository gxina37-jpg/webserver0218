#ifndef LOCKER_H
#define LOCKER_H
#include<pthread.h>
class locker {
    public:
        locker();
        ~locker();
        bool lock();
        bool unlock();
    private:
        pthread_mutex_t m_mutex;
};


#endif