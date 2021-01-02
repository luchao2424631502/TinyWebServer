#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "common.h"
#include "http.h"
#include "threadpool.h"

const int MAX_FD = 65536;   //最大文件描述符数量
const int MAX_EVENT_NUMBER = 10000; //最大事件数量
const int TIMESLOT = 5;     //最小超时单位

class WebServer
{
public:
    WebServer();
    ~WebServer();

    void init(int port,std::string user,std::string passWord,
            std::string databaseName,int log_write,int opt_linger,
            int trigmode,int sql_num,int thread_num,int close_log,int actor_model);
    
    //整个进程的主要工作流程,调用的函数
    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd,struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer,int sockfd); 
    bool dealclientdata();
    bool dealwithsignal(bool &timeout,bool &stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);
public:
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;

    int m_pipefd[2]; //信号由管道来
    int m_epollfd;
    http_conn *users;

    //sql
    connection_pool *m_connPool;
    std::string m_user;
    std::string m_passWord;
    std::string m_databaseName;
    int m_sql_num;

    //线程池
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //epool_event
    epoll_event events[MAX_EVENT_NUMBER]; //所有监听的事件列表

    //配置
    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //timer
    client_data *users_timer;
    Utils utils;
};

#endif
