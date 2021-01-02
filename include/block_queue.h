#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "locker.h"

template<typename T>
class block_queue
{
public:
    block_queue(int max_size = 1000);
    ~block_queue();
    void clear();
    bool full();    //判断队列是否满了 
    bool empty();   //判断队列是否为空
    bool front(T&);   //返回队首元素
    bool back(T&);    //返回队尾元素
    int size();
    int max_size();
    bool push(const T&);
    bool pop(T&);
    bool pop(T&,int);

private:
    locker m_mutex; //使用封装的mutex class 
    cond m_cond;    //封装的条件变量class 
    
    T *m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;
};

template<typename T>
block_queue<T>::block_queue(int max_size)
{
    if (max_size <= 0)
    {
        exit(-1);
    }

    m_max_size = max_size;      //赋数组max值
    m_array = new T[max_size];  //申请数组内存
    m_size = 0;                 //当前大小
    m_front = -1;               //front元素下标
    m_back = -1;
}

template<typename T>
block_queue<T>::~block_queue()
{
    m_mutex.lock();
    if (m_array != NULL)
    {
        delete[] m_array;
    }
    m_mutex.unlock();
}

template<typename T>
void block_queue<T>::clear()
{
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
}

template<typename T>
bool block_queue<T>::full()
{
    m_mutex.lock();
    if (m_size >= m_max_size)
    {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template<typename T>
bool block_queue<T>::empty()
{
    m_mutex.lock();
    if (m_size == 0)
    {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template<typename T>
bool block_queue<T>::front(T& value)
{
    m_mutex.lock();
    if (m_size == 0)
    {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
}

template<typename T>
bool block_queue<T>::back(T& value)
{
    m_mutex.lock();
    if (m_size == 0)
    {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
}

template<typename T>
int block_queue<T>::size()
{
    int tmp = 0;
    m_mutex.lock();
    tmp = m_size;
    m_mutex.unlock();
    return tmp;
}

template<typename T>
int block_queue<T>::max_size()
{
    int tmp = 0;
    m_mutex.lock();
    tmp = m_max_size;
    m_mutex.unlock();
    return tmp;
}

//向队列中添加元素
template<typename T>
bool block_queue<T>::push(const T& item)
{
    m_mutex.lock();
    if (m_size >= m_max_size)
    {
        //通过所有等待条件变量的线程
        m_cond.broadcast();
        m_mutex.unlock();
        return false;//添加失败
    }

    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;
    m_size++;

    m_cond.broadcast();
    m_mutex.unlock();
    return true;
}

template<typename T>
bool block_queue<T>::pop(T& item)
{
    m_mutex.lock();
    while(m_size <= 0)
    {
        //没有元素,等待条件变量,
        if (!m_cond.wait(m_mutex.get()))
        {
            //此时array应该有元素?
            m_mutex.unlock();
            return false;
        }
    }
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}

template<typename T>
bool block_queue<T>::pop(T& item,int ms_timeout)
{
    struct timespec t = {0,0};
    struct timeval now = {0,0};
    gettimeofday(&now,NULL);
    m_mutex.lock();
    if (m_size <= 0)
    {
        t.tv_sec = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;
        if (!m_cond.timewait(m_mutex.get(),t))
        {
            m_mutex.unlock();
            return false;
        }
    }

    //等待条件变量失败(一直没有得到通知
    if (m_size <= 0)
    {
        m_mutex.unlock();
        return false;
    }

    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}

#endif
