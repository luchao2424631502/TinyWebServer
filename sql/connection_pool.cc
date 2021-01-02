#include "sql.h"

connection_pool::connection_pool()
{
    m_CurConn = 0;
    m_FreeConn = 0;
}

connection_pool::~connection_pool()
{
    DestroyPool();
}

//单例的获得函数
connection_pool *connection_pool::GetInstance()
{
    static connection_pool connPool;//共享内存变量
    return &connPool;
}

//初始化函数
void connection_pool::init(std::string url,std::string User,
        std::string PassWord,std::string DataBaseName,
        int Port,int MaxConn,int close_log)
{
    m_url = url;
    m_Port = Port; // ??? int->std::string ???
    m_User = User;
    m_PassWord = PassWord;
    m_DatabaseName = DataBaseName;
    m_close_log = close_log;

    for (int i=0; i < MaxConn; i++)
    {
        MYSQL *con = NULL;
        con = mysql_init(con); //获得句柄

        if (!con)
        {
            LOG_ERROR("MYSQL ERROR");
            exit(1);
        }
        
        //连接数据库
        con = mysql_real_connect(con,url.c_str(),User.c_str(),PassWord.c_str(),
                DataBaseName.c_str(),Port,NULL,0);

        if (!con)
        {
            LOG_ERROR("MYSQL CONN ERROR");
            exit(1);
        }

        connList.push_back(con);
        ++m_FreeConn;
    }
    reserve = sem(m_FreeConn);

    m_MaxConn = m_FreeConn;
}

//sql pool中全是可用的sql连接
MYSQL *connection_pool::GetConnection()
{
    MYSQL *con = NULL;
    if (!connList.size()) //是否pool为空
        return NULL;
    
    //只能运行 n 个线程进入(在释放MYSQL连接时调用reserve.post()
    reserve.wait();
    lock.lock();//多个线程的临界区

    con = connList.front();
    connList.pop_front();

    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();
    return con;
}

//释放当前使用的连接
bool connection_pool::ReleaseConnection(MYSQL *conn)
{
    if (!conn)
        return false;
    lock.lock();
    
    
    connList.push_back(conn);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();
    reserve.post();
    return true;
}

//销毁sql pool 
void connection_pool::DestroyPool()
{
    lock.lock();
    if (connList.size() > 0)
    {
        std::list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); it++)
        {
            MYSQL *con = *it;
            mysql_close(con); //mysql/mysql.h接口关闭连接
        }

        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();//释放内存
    }
    lock.unlock();
}

//返回当前空闲的连接数
int connection_pool::GetFreeConn()
{
    return this->m_FreeConn;
}

connectionRAII::connectionRAII(MYSQL **sql,connection_pool *connPool)
{
    *sql = connPool->GetConnection();
    conRAII = *sql;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}
