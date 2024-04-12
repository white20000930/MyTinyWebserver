#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <pthread.h>
#include "../Lock/Lockers.h"
#include "../CGIMysql/Sql_connection_pool.h"
template <typename T>
class Threadpool
{
public:
    Threadpool(Connection_pool *connPool, int thread_number = 8, int max_requests = 10000);
    ~Threadpool();

    bool append(T *request); // add request to m_workqueue

private:
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;
    int m_max_requests;
    pthread_t *m_threads;       // Array of thread pool, size = m_thread_number
    std::list<T *> m_workqueue; // Request queue
    Locker m_queuelocker;       // to ensure only 1 thread can operate on m_workqueue
    Sem m_queuestate;           //~~~~~~~~~~~~~
    bool m_stop;
    Connection_pool *m_connPool; // Database connection pool
};

#endif

template <typename T>
Threadpool<T>::Threadpool(Connection_pool *connPool, int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL), m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }

    this->m_threads = new pthread_t[thread_number];
    if (!this->m_threads)
    {
        throw std::exception();
    }

    for (int i = 0; i < thread_number; i++)
    {
        if (pthread_create(this->m_threads + i, NULL, this->worker, this) != 0) // create a thread in POSIX Threads
        {
            delete[] this->m_threads;
            throw std::exception();
        }

        if (pthread_detach(this->m_threads[i]) != 0) // detach the thread
        {
            delete[] this->m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
Threadpool<T>::~Threadpool()
{
    delete[] this->m_threads;
    this->m_stop = true;
}

template <typename T>
bool Threadpool<T>::append(T *request)
{
    this->m_queuelocker.lock(); // wait until the lock becomes available
    if (m_workqueue.size() >= m_max_requests)
    {
        this->m_queuelocker.unlock();
        return false;
    }
    this->m_workqueue.push_back(request);
    this->m_queuelocker.unlock();
    m_queuestate.post(); // increase the semaphore

    return true;
}

template <typename T>
void *Threadpool<T>::worker(void *arg)
{
    Threadpool *pool = (Threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void Threadpool<T>::run()
{
    while (!this->m_stop)
    {
        this->m_queuestate.wait();  // wait until the semaphore > 0
        this->m_queuelocker.lock(); // wait until the lock becomes available

        if (this->m_workqueue.empty())
        {
            this->m_queuelocker.unlock();
            continue;
        }

        T *request = this->m_workqueue.front();
        this->m_workqueue.pop_front();
        this->m_queuelocker.unlock();

        if (!request)
        {
            continue;
        }

        // connectionRAII mysqlcon(&request->mysql, m_connPool);

        // request->process();
    }
}
