#include "lst_timer.h"
#include "http.h"

sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst()
{
    //单个定时器变量
    util_timer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

//添加定时器
void sort_timer_lst::add_timer(util_timer *timer)
{
    if (!timer)
    {
        return ;
    }
    //当前链表为空
    if (!head)
    {
        head = tail = timer;
        return ;
    }
    //根据绝对超时时间来插入到链表中
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return ;
    }
    //其他的情况调用add_timer(timer,head);
    add_timer(timer,head);
}

void sort_timer_lst::adjust_timer(util_timer *timer)
{
    if (!timer) //空
    {
        return ;
    }
    util_timer *tmp = timer->next;
    // 是链表最后一个 或者 顺序是正确的 
    if (!tmp || (timer->expire < tmp->expire))
    {
        return ;
    }
    
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer,head); //从列表中抽走,然后重新排序
    }
    else 
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer,timer->next); //从timer后面开始重新插入
    }
}

//按expire值来插入
void sort_timer_lst::add_timer(util_timer *timer,util_timer *lst_head)
{
    util_timer *prev = lst_head;
    util_timer *tmp  = prev->next;
    while (tmp)
    {
        //插入到2者中间
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = tmp;
            tmp->prev = timer;
            break;//插入成功
        }
        //继续下一轮
        prev = tmp;
        tmp = tmp->next;
    }
    //判断是否遍历完还没有插入,则插入到最后一个
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

//删除定时器
void sort_timer_lst::del_timer(util_timer *timer)
{
    if (!timer)
    {
        return ;
    }
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return ;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return ;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return ;
    }
    //在链表中间
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::tick()
{
    if (!head)
    {
        return ;
    }
    
    //拿到当前系统时间
    time_t cur = time(NULL);
    util_timer *tmp = head;
    while(tmp)
    {
        //tmp还没有超时
        if (cur < tmp->expire)
        {
            break;
        }
        tmp->cb_func(tmp->user_data);//执行回调函数
        head = tmp->next; //将tmp移定时器队列中
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

//
void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

//设置文件描述符非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

//向内核epoll表中注册EPOLLIN事件,ET模式,选择开启EPOLLONESHOT
void Utils::addfd(int epollfd,int fd,bool one_shot,int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
    {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    else //默认是level 触发
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if (one_shot)
    {
        event.events |= EPOLLONESHOT;
    }

    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

//统一IO事件来源
void Utils::sig_handler(int signo)
{
    int save_errno = errno;
    int msg = signo;
    send(u_pipefd[1],(char*)&msg,1,0);
    errno = save_errno;
}

//修改信号处理函数
void Utils::addsig(int signo,void(handler)(int),bool restart)
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(signo,&sa,NULL) != -1);
}

//处理定时任务,不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT); //设置下次SIGALRM信号到来时间
}

//show error
void Utils::show_error(int connfd,const char* info)
{
    send(connfd,info,strlen(info),0);
    close(connfd);
}

//static变量在class外初始化
int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
//回调函数
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);
    close(user_data->sockfd);
    //http class还没有实现,所以先注释掉 
    http_conn::m_user_count--;
}


