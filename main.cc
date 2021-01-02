#include "config.h"
#include <iostream>
#include "common.h"
#include "webserver.h"

int main(int argc,char* argv[])
{
    //数据库的用户名和数据库的密码,以及哪个数据库
    std::string user = "test";
    std::string passwd = "123456";
    std::string databasename = "test";

    Config config;
    config.parse_arg(argc,argv);

    WebServer server;
    server.init(config.PORT,user,passwd,databasename,config.LOGWrite,
            config.OPT_LINGER,config.TRIGMode,config.sql_num,
            config.thread_num,config.close_log,config.actor_model);

    //写日志
    server.log_write();
    
    server.sql_pool();

    server.thread_pool();

    //配置监听模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}
