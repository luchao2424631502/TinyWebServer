#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//信号量
class sem
{
public:
    sem();
    sem(int);
    ~sem();
public:
    bool wait();
    bool post();
private:
    sem_t m_sem;
};

//互斥量
class locker
{
public:
    locker();
    ~locker();
    bool lock();
    bool unlock();
    pthread_mutex_t *get();
private:
    pthread_mutex_t m_mutex;
};

//条件变量
class cond
{
public:
    cond();
    ~cond();
    bool wait(pthread_mutex_t *);
    bool timewait(pthread_mutex_t *,struct timespec);
    bool signal();
    bool broadcast();
private:
    pthread_cond_t m_cond;
};

#endif
