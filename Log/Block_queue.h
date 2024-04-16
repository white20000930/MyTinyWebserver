#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../Lock/Lockers.h"
using namespace std;

template <class T>
class Block_queue
{

public:
    Block_queue(int max_size = 1000);
    ~Block_queue();

    void clear();

    bool full();
    bool empty();
    bool front(T &value);
    bool back(T &value);
    int size();

    int max_size();
    bool push(const T &item);

    bool pop(T &item);
    bool pop(T &item, int ms_timeout);

private:
    Locker m_mutex;
    Cond m_cond;

    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};

template <class T>
Block_queue<T>::Block_queue(int max_size)
{
    if (max_size <= 0)
    {
        exit(-1);
    }

    this->m_max_size = max_size;
    this->m_array = new T[max_size];
    this->m_size = 0;
    this->m_front = -1;
    this->m_back = -1;
}

template <class T>
Block_queue<T>::~Block_queue()
{
    this->m_mutex.lock();
    if (this->m_array != NULL)
    {
        delete[] this->m_array;
    }
    this->m_mutex.unlock();
}

template <class T>
void Block_queue<T>::clear()
{
    this->m_mutex.lock();
    this->m_size = 0;
    this->m_front = -1;
    this->m_back = -1;
    this->m_mutex.unlock();
}

template <class T>
bool Block_queue<T>::full()
{
    this->m_mutex.lock();
    if (this->m_size >= this->m_max_size)
    {
        this->m_mutex.unlock();
        return true;
    }
    this->m_mutex.unlock();
    return false;
}

template <class T>
bool Block_queue<T>::empty()
{
    this->m_mutex.lock();
    if (this->m_size == 0)
    {
        this->m_mutex.unlock();
        return true;
    }
    this->m_mutex.unlock();
    return false;
}

template <class T>
bool Block_queue<T>::front(T &value)
{
    this->m_mutex.lock();
    if (this->m_size == 0)
    {
        this->m_mutex.unlock();
        return false;
    }
    value = this->m_array[this->m_front];
    this->m_mutex.unlock();
    return false;
}

template <class T>
bool Block_queue<T>::back(T &value)
{
    this->m_mutex.lock();
    if (this->m_size == 0)
    {
        this->m_mutex.unlock();
        return false;
    }
    value = this->m_array[this->m_back];
    this->m_mutex.unlock();
    return false;
}

template <class T>
int Block_queue<T>::size()
{
    int temp = 0;
    this->m_mutex.lock();
    temp = this->m_size;
    this->m_mutex.unlock();
    return temp;
}

template <class T>
int Block_queue<T>::max_size()
{
    int temp = 0;
    this->m_mutex.lock();
    temp = this->m_max_size;
    this->m_mutex.unlock();
    return temp;
}

template <class T>
bool Block_queue<T>::push(const T &item) /// push to back
{
    this->m_mutex.lock();
    if (this->m_size >= this->m_max_size)
    {
        this->m_cond.broadcast();
        this->m_mutex.unlock();
        return false;
    }

    this->m_back = (this->m_back + 1) % this->m_max_size;
    this->m_array[this->m_back] = item;

    this->m_size++;

    this->m_cond.broadcast();
    this->m_mutex.unlock();
    return true;
}

template <class T>
bool Block_queue<T>::pop(T &item) // pop from front
{
    this->m_mutex.lock();
    while (this->m_size <= 0)
    {
        if (!this->m_cond.wait(this->m_mutex.get())) // make this thread wait until m_cond is woke up in a short time
        {
            this->m_mutex.unlock();
            return false;
        }
    }

    this->m_front = (this->m_front + 1) % this->m_max_size;
    item = this->m_array[this->m_front];
    this->m_size--;

    this->m_mutex.unlock();
    return true;
}

template <class T>
bool Block_queue<T>::pop(T &item, int ms_timeout)
{
    struct timespec t = {0, 0};
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    this->m_mutex.lock();
    if (this->m_size <= 0)
    {
        t.tv_sec = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;
        if (!this->m_cond.timewait(m_mutex.get(), t))
        {
            this->m_mutex.unlock();
            return false;
        }
    }

    if (m_size <= 0)
    {
        m_mutex.unlock();
        return false;
    }

    this->m_front = (this->m_front + 1) % this->m_max_size;
    item = this->m_array[this->m_front];
    this->m_size--;

    this->m_mutex.unlock();
    return true;
}

#endif