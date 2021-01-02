#ifndef CONFIG_H
#define CONFIG_H

#include <unistd.h>
#include <stdlib.h>
#include <iostream>

class Config
{
public:
    Config();
    ~Config(){}
public:
    void Print();

public:
    void parse_arg(int,char* []);

    //端口号
    int PORT;

    //日志写入方式
    int LOGWrite;

    //触发组合模式
    int TRIGMode;

    //listenfd 触发模式
    int LISTENTrigmode;

    //connfd 触发模式 
    int CONNTrigmode;

    //是否优雅关闭连接
    int OPT_LINGER;

    //数据库连接池数量 
    int sql_num;

    //线程池线程数量 
    int thread_num;

    //是否关闭日志 
    int close_log;

    //并发模型选择 
    int actor_model;
};

#endif
