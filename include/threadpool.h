#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <stdio.h>
#include <exception>
#include <pthread.h>
#include "locker.h"
#include "sql.h"

template<typename T>
class threadpool 
{
public:
    threadpool(int,connection_pool *,int thread_number=8,int max_requests=1000000);
    ~threadpool();
    bool append(T *,int);
    bool append_p(T *);
private:
    static void *worker(void *arg);
    void run();

private:
    std::list<T *> m_workqueue;//请求队列
    locker m_queuelocker;   //
    sem m_queuestat;        //判断是否有任务需要执行(默认初始化
    int m_actor_model;
    connection_pool *m_connPool;
    int m_thread_number;    //线程数
    int m_max_requests;     //请求队列最大请求数
    pthread_t *m_threads;   //线程池
};

template<typename T>
threadpool<T>::threadpool(int actor_model,connection_pool *connPool,
        int thread_number,int max_requests):
    m_actor_model(actor_model),m_connPool(connPool),
    m_thread_number(thread_number),m_max_requests(max_requests),m_threads(NULL)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    //线程标记空间
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    for (int i=0; i<thread_number; i++)
    {
        if (pthread_create(m_threads+i,NULL,worker,this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template<typename T>
bool threadpool<T>::append(T *request,int state)
{
    m_queuelocker.lock();
    //处理队列已满
    if (m_workqueue.size() >= (long unsigned int)m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request); //加入队列中
    m_queuelocker.unlock();

    m_queuestat.post(); //信号量增加,通知处理

    return true;
}

template<typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= (long unsigned int)m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while (true)
    {
        m_queuestat.wait(); //等待sem>0,则表示有任务需要执行
        m_queuelocker.lock(); //在临界区操作工作等待队列
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock(); 
        if (!request)
            continue;
	// 1:reactor
        if (m_actor_model)
        {
            //http请求目前处于的读写状态 0=read 1=write
            if (request->m_state == 0)
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql,m_connPool);
                    request->process();
                }
                //读取失败
                else 
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            //http请求处理完毕,开始写,返回给client
            else 
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else 
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
	// 0:proactor
        else 
        {
            connectionRAII mysqlcon(&request->mysql,m_connPool);
            request->process();
        }
    }
}
#endif
