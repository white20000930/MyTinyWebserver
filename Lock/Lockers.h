#ifndef LOCKER_H
#define LOCKER_H

#include <semaphore.h>
#include <exception>
#include <pthread.h>

class Sem
{
public:
    Sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            throw std::exception();
        }
    };
    Sem(int num)
    {
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    };

    ~Sem()
    {
        sem_destroy(&m_sem);
    };

    bool wait()
    { // when m_sem > 0, return true
        return sem_wait(&m_sem) == 0;
        // when m_sem > 0,  sem_wait return 0
    };
    bool post()
    { // when m_sem + 1 successfully, return true
        return sem_post(&m_sem) == 0;
        // when m_sem + 1 successfully, return 0
    };

private:
    sem_t m_sem;
};

class Locker
{
public:
    Locker()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    };

    ~Locker()
    {
        pthread_mutex_destroy(&m_mutex);
    };

    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    };

    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    };

    pthread_mutex_t *get()
    {
        return &m_mutex;
    };

private:
    pthread_mutex_t m_mutex;
};

class Cond
{
public:
    Cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            throw std::exception();
        }
    };

    ~Cond()
    {
        pthread_cond_destroy(&m_cond);
    };

    bool wait(pthread_mutex_t *m_mutex)
    {
        return pthread_cond_wait(&m_cond, m_mutex) == 0;
    }

    bool timewait(pthread_mutex_t *m_mutex, struct timespec time)
    {
        return pthread_cond_timedwait(&m_cond, m_mutex, &time) == 0;
    }

    bool signal() // wake up one thread on the m_cond
    {
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool broadcast() // wake up all threads on the m_cond
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};
#endif