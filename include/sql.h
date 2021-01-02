#ifndef CONNECTION_SQL_H
#define CONNECTION_SQL_H

#include "common.h"

#include <mysql/mysql.h>

#include "locker.h"
#include "log.h"

//mysql连接池类
class connection_pool
{
public:
    MYSQL *GetConnection();                 //获取数据库连接
    bool ReleaseConnection(MYSQL *conn);    //释放连接
    int GetFreeConn();                      //获取连接
    void DestroyPool();                     //摧毁所有连接

    //单例模式
    static connection_pool *GetInstance();

    void init(std::string url,std::string User,
            std::string PassWord,std::string DataBaseName,
            int Port,int MaxConn,int close_log);
private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;                          //最大连接数
    int m_CurConn;                          //已使用连接数
    int m_FreeConn;                         //空闲连接数
    locker lock;
    std::list<MYSQL *> connList;
    sem reserve;

public:
    std::string m_url;                      //主机地址??
    std::string m_Port;                     //数据库的端口号??
    std::string m_User;                     //登陆mysql的用户名
    std::string m_PassWord;                 //
    std::string m_DatabaseName;             //使用的库name
    int m_close_log;                        //日志开关
};

//mysql连接实例类
class connectionRAII
{
public:
    connectionRAII(MYSQL **con,connection_pool *connPool);
    ~connectionRAII();
private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};

#endif
