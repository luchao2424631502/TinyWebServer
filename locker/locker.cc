#include "locker.h"

sem::sem()
{
    if (sem_init(&m_sem,0,0) != 0)
    {
        throw std::exception();
    }
}

sem::sem(int num)
{
    if (sem_init(&m_sem,0,num) != 0)
    {
        throw std::exception();
    }
}

sem::~sem()
{
    sem_destroy(&m_sem);
}

bool sem::wait()
{
    // all of theres functions return 0 on success;
    return sem_wait(&m_sem) == 0;
}

bool sem::post()
{
    return sem_post(&m_sem) == 0;
}

locker::locker()
{
    if (pthread_mutex_init(&m_mutex,NULL) != 0)
    {
        throw std::exception();
    }
}

locker::~locker()
{
    pthread_mutex_destroy(&m_mutex);
}

bool locker::lock()
{
    return pthread_mutex_lock(&m_mutex) == 0;
}

bool locker::unlock()
{
    return pthread_mutex_unlock(&m_mutex) == 0;
}

pthread_mutex_t *locker::get()
{
    return &m_mutex;
}

cond::cond()
{
    if (pthread_cond_init(&m_cond,NULL) != 0)
    {
        throw std::exception();
    }
}

cond::~cond()
{
    pthread_cond_destroy(&m_cond);
}

bool cond::wait(pthread_mutex_t *m_mutex)
{
    int ret = 0;
    //将线程挂起+unlock mutex结合成原子操作
    ret = pthread_cond_wait(&m_cond,m_mutex);
    return ret == 0;
}

bool cond::timewait(pthread_mutex_t *m_mutex,struct timespec t)
{
    int ret = 0;
    ret = pthread_cond_timedwait(&m_cond,m_mutex,&t);
    return ret == 0;
}

bool cond::signal()
{
    return pthread_cond_signal(&m_cond) == 0;
}

bool cond::broadcast()
{
    return pthread_cond_broadcast(&m_cond) == 0;
}

