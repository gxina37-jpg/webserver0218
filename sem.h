#ifndef SEM_H
#define SEM_H
#include<semaphore.h>

class sem {
    public:
        sem();
        ~sem();
        sem(int x);
        bool wait();
        bool post();
    private:
        sem_t m_sem;
};


#endif