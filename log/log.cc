#include "log.h"

Log::Log()
{
    m_count = 0; //日志行数
    m_is_async = false;
}

Log::~Log()
{
    if (m_fp)
        fclose(m_fp);
}

//异步则block queue长度>=1,同步每次有日志就会直接写,队列长度=0
bool Log::init(const char* file_path,int close_log,
        int log_buf_size,int split_lines,int max_queue_size)
{
    if (max_queue_size)
    {
        m_is_async = true;
        m_log_queue = new block_queue<std::string>(max_queue_size);//申请阻塞队列
        pthread_t tid;
        pthread_create(&tid,NULL,flush_log_thread,NULL);
    }
    
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf,'\0',m_log_buf_size);
    m_split_lines = split_lines;

    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);//拿到年月日等 
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_path,'/');//拿到最后/开头的字符串
    char log_full_name[256] = {0}; //完整的文件名 
    if (!p) //传入的就是当前路径下的文件名
    {
        snprintf(log_full_name,255,"%d_%02d_%02d_%s",my_tm.tm_year+1990,
                my_tm.tm_mon+1,my_tm.tm_mday,file_path);
    }
    else//给出了完整的路径 
    {
        strcpy(log_name,p+1); //得到日志文件名
        strncpy(dir_name,file_path,p-file_path+1); //得到日志文件的路径
        snprintf(log_full_name,255,"%s%d_%02d_%02d_%s",dir_name,my_tm.tm_year+1900,
                my_tm.tm_mon+1,my_tm.tm_mday,log_name);
    }

    m_today = my_tm.tm_mday;

    //生成日志文件
    m_fp = fopen(log_full_name,"a");
    if (!m_fp)
    {
        return false;
    }
    return true;
}

void Log::write_log(int level,const char *format,...)
{
    struct timeval now = {0,0};
    gettimeofday(&now,NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch(level)
    {
    case 0:
        strcpy(s,"[DEBUG]:");
        break;
    case 1:
        strcpy(s,"[INFO]:");
        break;
    case 2:
        strcpy(s,"[WARN]:");
        break;
    case 3:
        strcpy(s,"[ERR]:");
        break;
    default:
        strcpy(s,"[DEFAULT]:");
        break;
    }

    //写入一行log,对m_count++,
    m_mutex.lock();
    m_count++;
    //超过日志文件行数限制 or 日期变了 
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
    {
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        //日期
        snprintf(tail,16,"%d_%02d_%02d_",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday);

        if (m_today != my_tm.tm_mday)//日期变了 
        {
            snprintf(new_log,255,"%s%s%s",dir_name,tail,log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else//超过行数 
        {
            snprintf(new_log,255,"%s%s%s.%lld",dir_name,tail,log_name,m_count / m_split_lines);
        }
        m_fp = fopen(new_log,"a");//生成文件
    }
    m_mutex.unlock();

    //生成具体日志内容
    va_list valst;
    va_start(valst,format);
    std::string log_str;
    m_mutex.lock();

    int n = snprintf(m_buf,48,"%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
            my_tm.tm_year + 1900,my_tm.tm_mon + 1,my_tm.tm_mday,
            my_tm.tm_hour,my_tm.tm_min,my_tm.tm_sec,now.tv_usec,s);
    int m = vsnprintf(m_buf + n,m_log_buf_size - 1,format,valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf; //??强制类型转换??

    m_mutex.unlock();

    //如果是异步并且 block queue没有满的话提交任务
    if (m_is_async && !m_log_queue->full())
    {
        m_log_queue->push(log_str);
    }
    else//异步 block queue满了则改成同步写入 
    {
        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }

    va_end(valst); //可变参数结束
}

void Log::flush()
{
    m_mutex.lock();
    fflush(m_fp); //刷新c流的缓冲区
    m_mutex.unlock();
}
