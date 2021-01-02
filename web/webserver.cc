#include "webserver.h"

WebServer::WebServer()
{
    //http_conn对象数组
    users = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path,200); //得到main进程的工作目录
    char root[6] = "/root";
    m_root = (char*)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root,server_path);
    strcat(m_root,root);

    //定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

//
void WebServer::init(int port,std::string user,std::string passWord,
        std::string databaseName,int log_write,int opt_linger,int trigmode,int sql_num,
        int thread_num,int close_log,int actor_model)
{
    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_close_log = close_log;
    m_actormodel = actor_model;

}

//监听和传输fd 的触发模式
void WebServer::trig_mode()
{
    //LT + LT
    switch(m_TRIGMode)
    {
    case 0:
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
        break;
    //LT + ET
    case 1:
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
        break;
    //ET + LT 
    case 2:
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
        break;
    //ET + ET
    case 3:
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
        break;
    default:
        break;
    }
}

/*
 * 如果默认开启日志后 启动进程然后seg fault,一定是当前目录的 权限问题
*/
void WebServer::log_write()
{
    //0表示开启日志
    if (!m_close_log)
    {
        // 0表示同步写入 1表示异步写入
        if (m_log_write)
            //2000 log_buf_size 800000分割行数 800异步队列长度
            Log::get_instance()->init("./ServerLog",m_close_log,2000,800000,800);
        else 
            Log::get_instance()->init("./ServerLog",m_close_log,2000,800000,0);
    }
}

//初始化sql连接池
void WebServer::sql_pool()
{
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost",m_user,m_passWord,m_databaseName,3306,m_sql_num,m_close_log);

    //http类拿到查询结果
    users->initmysql_result(m_connPool);
}

//创建线程池
void WebServer::thread_pool()
{
    m_pool = new threadpool<http_conn>(m_actormodel,m_connPool,m_thread_num);
}

void WebServer::eventListen()
{
    m_listenfd = socket(AF_INET,SOCK_STREAM,0);
    assert(m_listenfd >= 0);

    if (m_OPT_LINGER == 0)
    {
        //0 ~ :close立即返回,由os发送完剩下的包并且回收socket资源
        struct linger tmp = {0,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }
    else if(m_OPT_LINGER == 1)
    {
        //1 b:在时间b内发送完数据包则close()正确返回,否则返回错误值,未发送数据丢失
        struct linger tmp = {1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    memset(&address,0,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY); //服务器端网卡IP
    address.sin_port = htons(m_port);

    int flag = 1;
    //服务端的tcp socket解决TIME_WAIT问题,bind错误问题
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));
    ret = bind(m_listenfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd,5);
    assert(ret >= 0);

    utils.init(TIMESLOT);
    
    // epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    
    utils.addfd(m_epollfd,m_listenfd,false,m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    //统一事件源用的全双工管道
    ret = socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd);
    assert(ret != -1);

    utils.setnonblocking(m_pipefd[1]); //用来写的描述符non blocking
    utils.addfd(m_epollfd,m_pipefd[0],false,0); //监听用来读取的描述符是否有事件发生

    //注册信号处理事件
    utils.addsig(SIGPIPE,SIG_IGN);
    utils.addsig(SIGALRM,utils.sig_handler,false);
    utils.addsig(SIGTERM,utils.sig_handler,false);

    alarm(TIMESLOT);

    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd,struct sockaddr_in client_address)
{
    //初始化http对象
    users[connfd].init(connfd,client_address,m_root,m_CONNTrigmode,
            m_close_log,m_user,m_passWord,m_databaseName);
    
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func; //lst_timer中实现

    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    //指定client_data中的定时器
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer); //加入到定时器管理链表中
}

//调整定时器在链表中的位置
void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s","adjust timer once");
}

//执行call back函数
void WebServer::deal_timer(util_timer *timer,int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }
    
    LOG_INFO("close fd %d",users_timer[sockfd].sockfd);
}

// 
bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    //LT
    if (m_LISTENTrigmode == 0)
    {
        int connfd = accept(m_listenfd,(struct sockaddr*)&client_address,&client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("accept error: errno is:%d",errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd,"Internal server busy");
            LOG_ERROR("Internal server busy");
            return false;
        }
        timer(connfd,client_address);
	//printf("LT accept\n");
    }
    //ET
    else 
    {
        while (1)
        {
            int connfd = accept(m_listenfd,(struct sockaddr*)&client_address,&client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("accept error: errno is:%d",errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd,"Internal server busy");
                LOG_ERROR("Internal server busy");
                break;
            }
            timer(connfd,client_address); //设置基本数据
        }
	//printf("ET accept\nreturn false;");
        return false;//ET 永远是return false,accept is true
	//return flag ? true: false;
    }
    //printf("return true\n");
    return true;
}


bool WebServer::dealwithsignal(bool &timeout,bool &stop_server)
{
    char signals[1024];
    int ret =  recv(m_pipefd[0],signals,sizeof(signals),0); //signal -> IO
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else 
    {
        //接受了多少个信号
        for (int i=0; i<ret; i++)
        {
            switch(signals[i])
            {
            case SIGALRM:
                timeout = true;
                break;
            case SIGTERM:
                stop_server = true;
                break;
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    //从client_data中拿到
    util_timer *timer = users_timer[sockfd].timer;

    //reactor
    if (m_actormodel == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        //向线程池中加入请求队列
        m_pool->append(users + sockfd,0); 

        while (true)
        {
            if (users[sockfd].improv)
            {
                if (users[sockfd].timer_flag)
                {
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    //pro actor
    else 
    {
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)",inet_ntoa(users[sockfd].get_address()->sin_addr));

            m_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else 
        {
            //执行call back函数后从timer list中删除
            deal_timer(timer,sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    //re actor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_pool->append(users+sockfd,1);

        while(true)
        {
            if (users[sockfd].improv)
            {
                if (users[sockfd].timer_flag)
                {
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }//pro actor
    else 
    {
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)",inet_ntoa(users[sockfd].get_address()->sin_addr));
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else 
        {
            deal_timer(timer,sockfd);
        }
    }
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        //没有超时时间,一直阻塞 
        int number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);

        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("epoll failure");
            break;
        }

        //处理事件
        for (int i=0; i<number; i++)
        {
            int sockfd = events[i].data.fd;
            
            // new connection 
            if (sockfd == m_listenfd)
            {
                bool flag = dealclientdata();
                if (flag == false)
                    continue;
            }
            //发生事件类型是否是如下几种
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer,sockfd);
            }
            //信号(IO事件)
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(timeout,stop_server);
                if (flag == false)
                    LOG_ERROR("dealclientdata failure");
            }
            //处理接受到的数据
            else if (events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            //写事件
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        //SIGALRM被dealwithsignal处理后,重新注册SIGALRM信号的时间 
        if (timeout)
        {
            utils.timer_handler();
            LOG_INFO("timer tick");
            timeout = false;
        }
    }
}
