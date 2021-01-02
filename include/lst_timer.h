#ifndef LST_TIMER_H
#define LST_TIMER_H

#include "common.h"

//定时器类
class util_timer;

//用户数据
struct client_data
{
    struct sockaddr_in address;
    int sockfd;
    util_timer *timer;
};

class util_timer
{
public:
    util_timer():prev(NULL),next(NULL) {}

public:
    time_t expire; //超时时间

    void (*cb_func)(client_data*);//回调函数指针
    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

//定时器链表?
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer*);
    void adjust_timer(util_timer *);
    void del_timer(util_timer *);
    void tick();

private:
    void add_timer(util_timer *,util_timer*);
    
    util_timer *head;
    util_timer *tail;
};

// 工具集类
class Utils
{
public:
    Utils() {}
    ~Utils() {}

public:
    void init(int);    
    //设置描述符非阻塞
    int setnonblocking(int fd);
    //向内核事件表中添加EPOLLIN事件,ET模式
    void addfd(int epollfd,int fd,bool one_shot,int TRIGMode);
    //信号处理函数 
    static void sig_handler(int signo);
    //设置信号处理函数
    void addsig(int signo,void(handler)(int),bool restart = true);
    //定时处理任务
    void timer_handler();

    void show_error(int connfd,const char* info);

public:
    static int *u_pipefd;
    sort_timer_lst m_timer_lst; //链表
    static int u_epollfd;
    int m_TIMESLOT;
};

//回调函数声明
void cb_func(client_data*);

#endif
