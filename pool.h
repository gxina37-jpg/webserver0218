#include<unistd.h>
#include<list>
#include"sem.h"
#include"locker.h"
#include<list>
#include"macro.h"
template<typename T>
class pool {
    public:
        pool(int thread_number = 10, int max_requests = 10000);
        ~pool();
        bool append(T* request);
    private:
        static void *worker(void* arg);
        void run();
        int m_thread_number;
        bool m_stop;
        pthread_t * m_threads;
        int m_max_requests;
        sem m_queuestat;
        locker m_queuelocker;
        std::list<T*> m_workqueue;
        int m_queue_size;
};


template<typename T>
pool<T>::pool(int thread_number, int max_requests) {
    m_thread_number = thread_number;
    m_max_requests = max_requests;
    m_queue_size = 0;
    m_threads = new pthread_t[m_thread_number];
    m_stop = false;
    for (int i = 0; i < m_thread_number; ++i) {
        pthread_create(&m_threads[i], nullptr, worker, this);
        pthread_detach(m_threads[i]);
    }
}

template<typename T>
pool<T>::~pool() {
    delete[] m_threads;
}

template<typename T>
void* pool<T>::worker(void* args) {
    pool* p = (pool*)args;
    p -> run();
    return p;
} 


template< typename T >
void pool<T>::run() {
    while(!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_stop) {
            m_queuelocker.unlock();
            break;
        }
        T* request = nullptr;
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queue_size--;
        m_queuelocker.unlock();
        request -> process();
    }
}

template<typename T> 
bool pool<T>:: append(T* request) {
    m_queuelocker.lock();
    if (m_queue_size >= MAX_QUEUE_SIZE) {
        m_queuelocker.unlock();
        return false;
    }else{
        m_workqueue.push_back (request);
        m_queue_size++;
        m_queuelocker.unlock();
        m_queuestat.post();
        return true;
    }
}
