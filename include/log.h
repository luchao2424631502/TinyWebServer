#ifndef LOG_H
#define LOG_H

#include "common.h"
#include "block_queue.h"

class Log
{
public:
    //单例模式
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
        return NULL;
    }

    //
    bool init(const char *file_path,int close_log,
            int log_buf_size=8192,int split_lines = 5000000,int max_queue_size = 0);

    void write_log(int level,const char *format,...);

    void flush();

private:
    Log();
    virtual ~Log();

    //异步写入日志文件
    void *async_write_log()
    {
        std::string single_log;
        //从block_queue中取出日志string,写入log file 
        while(m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(),m_fp);  //待优化???
            m_mutex.unlock();
        }
        return NULL;
    }

private:
    char dir_name[100];
    char log_name[100];

    // char dir_name[128]; //路径名
    // char log_name[128]; //日志文件名
    int  m_split_lines; //日志文件的最多行数
    int  m_log_buf_size;//日志缓冲区大小
    long long m_count;  //日志行数记录
    int  m_today;       //day time
    FILE *m_fp;         //C FILE打开文件指针
    char *m_buf;
    bool m_is_async;    //是否异步标志
    int  m_close_log;   //日志是否关闭

    block_queue<std::string> *m_log_queue; //阻塞队列
    locker m_mutex;     //互斥锁
};

#define LOG_DEBUG(format,...) \
    if(m_close_log == 0) {\
        Log::get_instance()->write_log(0,format,##__VA_ARGS__);\
        Log::get_instance()->flush();}
#define LOG_INFO(format,...) \
    if(m_close_log == 0) {\
        Log::get_instance()->write_log(1,format,##__VA_ARGS__);\
        Log::get_instance()->flush();}
#define LOG_WARN(format,...) \
    if(m_close_log == 0){\
        Log::get_instance()->write_log(2,format,##__VA_ARGS__);\
        Log::get_instance()->flush();}
#define LOG_ERROR(format,...) \
    if(m_close_log == 0) {\
        Log::get_instance()->write_log(3,format,##__VA_ARGS__);\
        Log::get_instance()->flush();}

#endif
