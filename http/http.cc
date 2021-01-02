#include "http.h"
#include <mysql/mysql.h>
#include <fstream>

//Http状态信息
const char *ok_200_title    = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form  = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form  = "You do not have permission to get file from this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form  = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form  = "There was an unusual problem serving the request file.\n";

// static locker m_lock;
// static std::map<std::string,std::string> users;
locker m_lock;
std::map<std::string,std::string> users;

//http class中初始化mysql类
void http_conn::initmysql_result(connection_pool *connPool)
{
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql,connPool);//由conRAII类~函数来释放

    //非零值表示错误,(居然吧sql语句写错了...
    if (mysql_query(mysql,"select * from user"))
    {
        LOG_ERROR("select error: %s\n",mysql_error(mysql));//mysql_error拿到上一次出错的信息
    }
    
    // 拿到查询结果集合
    MYSQL_RES *result = mysql_store_result(mysql);

    //获取列数 ??? unused var
    // int num_fields = mysql_num_fields(result);

    //结果集的列的每一个字段 ??? unused var
    // MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //拿到Line(查询结果)
    while(MYSQL_ROW row = mysql_fetch_row(result))
    {
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        users[temp1] = temp2;//users定义unit的上面
    }
}

int setnonblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd,bool one_shot,int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    
    if (TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else //epoll默认是水平触发
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

//从内核事件表中去掉事件描述符 fd
void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,NULL);
    close(fd);
}

//将fd事件的属性重置为 EPOLLONESHOT
void modfd(int epollfd,int fd,int ev,int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    if (TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else 
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

//初始化class static var变量
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

//关闭连接
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n",m_sockfd);
        removefd(m_epollfd,m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::init(int sockfd,const sockaddr_in &addr,char *root,
        int TRIGMode,int close_log,std::string user,std::string passwd,
        std::string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;   //客户的地址
    
    addfd(m_epollfd,sockfd,true,m_TRIGMode);
    m_user_count++; //建立连接用户数+1

    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user,user.c_str());
    strcpy(sql_passwd,passwd.c_str());
    strcpy(sql_name,sqlname.c_str());
    
    init();
}

//
void http_conn::init()
{
    mysql               = NULL;
    bytes_to_send       = 0;
    bytes_have_send     = 0;
    m_check_state       = CHECK_STATE_REQUESTLINE;
    m_linger            = false;
    m_method            = GET;
    m_url               = 0;
    m_version           = 0;
    m_content_length    = 0;
    m_host              = 0;
    m_start_line        = 0;
    m_checked_idx       = 0;
    m_read_idx          = 0;
    m_write_idx         = 0;
    cgi                 = 0; //是否启用POST
    m_state             = 0; //static var 读=0,写=1
    timer_flag          = 0;
    improv              = 0;

    memset(m_read_buf,'\0',READ_BUFFER_SIZE);
    memset(m_write_buf,'\0',WRITE_BUFFER_SIZE);
    memset(m_real_file,'\0',FILENAME_LEN);
}

/*从状态机 
    GET / HTTP/1.1\r\nHost: www.sina.com.cn\r\nConnection: close\r\n\r\n
*/
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; m_checked_idx++)
    {
        temp = m_read_buf[m_checked_idx];
        //http的换行回车是window格式的\r\n
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
            {
                return LINE_OPEN;
            }
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {//截断字符串,已经读取了一行
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;//其他的情况就是error了
        }
        else if (temp == '\n')
        {
            //上一个是\r(>1表示一开始不可能就是\r\n
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;//继续读取,此次读取的数据已经解析完毕
}

// nonblock+et 需要一次性读取完数据
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    
    //LT模式
    if (!m_TRIGMode)
    {
        //TCP return 0 表示流结束(对方关闭) -1 is error
        bytes_read = 
            recv(m_sockfd,m_read_buf + m_read_idx,READ_BUFFER_SIZE - m_read_idx,0);
        m_read_idx += bytes_read;
        
        if (bytes_read <= 0)
        {
            return false;
        }
        return true;
    }
    //ET模式 nonblock要一直recv直到 
    else 
    {
        while(true)
        {
            bytes_read = 
                recv(m_sockfd,m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx,0);
            if (bytes_read == -1)
            {
                // #define EWOULDBLOCK EAGAIN
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}

/*主状态机,解析http请求行
 GET / HTTP/1.1\r\nHost: www.sina.com.cn\r\nConnection: close\r\n\r\n
*/
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text," \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    /* 拿到url的起始地址和 完整的请求方法
     * GET /source
     * GET\0/source
     * */
    char *method = text;
    if (strcasecmp(method,"GET") == 0)
        m_method = GET;
    else if (strcasecmp(method,"POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else 
        return BAD_REQUEST;
    
    m_url += strspn(m_url," \t"); //跳过可能有的空格
    m_version = strpbrk(m_url," \t"); 
    if (!m_version)
        return BAD_REQUEST;
    
    *m_version++ = '\0';//空格变回车
    m_version += strspn(m_version," \t");
    if (strcasecmp(m_version,"HTTP/1.1") != 0)
        return BAD_REQUEST; //非1.1版本则error
    
    if (strncasecmp(m_url,"http://",7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url,'/');
    }
    if (strncasecmp(m_url,"https://",8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url,'/');
    }
    
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;

    if (strlen(m_url) == 1) // url is / 默认的页面是judge.html
        strcat(m_url,"judge.html"); 

    //http包的请求行解析完毕,主状态机状态变化为 解析请求头部字段
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/*
 * 解析头部字段
> Host:\0192.168.56.101:8080\0\0
> User-Agent:\-curl/7.68.0\0\0
> Accept: \0\0
*/
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;//状态转移:请求头部->请求content
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text,"Connection:",11) == 0)
    {
        text += 11;
        text += strspn(text," \t");
        if (strcasecmp(text,"keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text,"Content-length:",15) == 0)
    {
        text += 15;
        text += strspn(text," \t");
        m_content_length = atol(text); //POST请求
    }
    else if (strncasecmp(text,"Host:",5) == 0)
    {
        text += 5;
        text += strspn(text," \t");
        m_host = text;
    }
    else 
    {
        LOG_INFO("unknown headers: %s",text);
    }
    return NO_REQUEST;
}

/*
 * 拿到包的主体的内容
*/
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    //包的请求主体还没检查
    if ((m_checked_idx + m_content_length) <= m_read_idx)
    {
        text[m_content_length] = '\0';
        
        m_string = text; 
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

/*
 * 处理整个http请求
 */
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = NULL;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
            ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO("%s",text);
        switch(m_check_state) //判断主状态机的状态
        {
            case CHECK_STATE_REQUESTLINE:
                ret = parse_request_line(text);
                if (ret == BAD_REQUEST)
                    return ret;
                break;//只会执行一次
            case CHECK_STATE_HEADER:
                ret = parse_headers(text);
                if (ret == BAD_REQUEST)
                    return ret;
                else if (ret == GET_REQUEST) //GET请求
                    return do_request();
                break;//NO_REQUEST继续parse_line,没有数据子状态机状态为LINE_OPEN
            case CHECK_STATE_CONTENT: //POST请求
                ret = parse_content(text);
                if (ret == GET_REQUEST)
                    return do_request();
                line_status = LINE_OPEN; //内容没有完整拿到
                break;
            default:
                return INTERNAL_ERROR;
        }
    }
    //LINE_OPEN:数据为接收完毕则while退出
    return NO_REQUEST;
}

/*
 * 处理http请求
 * */
http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file,doc_root);
    int len = strlen(doc_root);
    const char *p = strrchr(m_url,'/'); //拿到url中最后一个/开头的地址

    // /2CGISQL.CGI 2cgi是login功能,3cgi是register功能
    if (cgi == 1 && (*(p+1) == '2' || *(p+1) == '3'))
    {
        // char flag = m_url[1];
        
        // char *m_url_real = (char*)malloc(sizeof(200));
        // strcpy(m_url_real,"/");
        // strcat(m_url_real,m_url + 2); //CGISQL.CGI
        // strncpy(m_real_file + len,m_url_real,FILENAME_LEN - len - 1); //在根目录路径后添加实际文件路径
        // free(m_url_real);

        char name[100],password[100];
        // user=xxxx&password=xxxx
        int i;
        for (i=5; m_string[i] != '&'; i++)
        {
            name[i-5] = m_string[i];
        }
        name[i - 5] = '\0';
	int j=0;
        for (i+=10; m_string[i] != '\0'; i++,j++)
        {
            password[j] = m_string[i];
        }
        password[j] = '\0';

        if (*(p+1) == '3')//register
        {
            char *sql_insert = (char*)malloc(200);
            strcpy(sql_insert,"insert into user values(");
            strcat(sql_insert,"'");
            strcat(sql_insert,name);
            strcat(sql_insert,"','");
            strcat(sql_insert,password);
            strcat(sql_insert,"')");

            if (users.find(name) == users.end())
            {
                m_lock.lock();
                int res = mysql_query(mysql,sql_insert);
                users.insert(std::pair<std::string,std::string>(name,password));
                m_lock.unlock();

                if (!res)//success 修改路由
                    strcpy(m_url,"/log.html");
                else
                    strcpy(m_url,"/registerError.html");
                    
            }
            else//帐号已经被注册了(后期可修改的地方)
                strcpy(m_url,"/registerError.html");
        }
        else if(*(p + 1) == '2')//login 
        {
            //success
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url,"/welcome.html");
            else
                strcpy(m_url,"/logError.html");
        }
    }

    char *m_url_real = (char*)malloc(200);
    bool inout = 1;

    //注意0 和 1也是post(后期修改),但是request body中没有数据,所以直接通过url分辨即可 
    if (*(p+1) == '0') //register
    {
        strcpy(m_url_real,"/register.html");
    }
    else if (*(p+1) == '1') //login
    {
        strcpy(m_url_real,"/log.html");
    }
    else if (*(p+1) == '5') //jpg
    {
        strcpy(m_url_real,"/picture.html");
    }
    else if (*(p+1) == '6') //avi
    {
        strcpy(m_url_real,"/video.html");
    }
    else if (*(p+1) == '7') //其他
    {
        strcpy(m_url_real,"/fans.html");
    }
    else
    {
        inout = 0;
        strncpy(m_real_file + len,m_url,FILENAME_LEN - len - 1);
    }

    if (inout)
        strncpy(m_real_file + len,m_url_real,strlen(m_url_real));
    free(m_url_real);

    //获取文件状态
    if (stat(m_real_file,&m_file_stat) < 0)
        return NO_RESOURCE;

    //文件权限不够
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    //请求的是文件夹
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file,O_RDONLY);
    //将磁盘文件映射到内存中
    m_file_address = (char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}

//取消html文件的映射
void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address = 0;
    }
}

//最后返回给客户端的调用的函数(发给客户端的Http包组装好了)
bool http_conn::write()
{
    int temp = 0;

    if (bytes_to_send == 0)//没有内容需要发送给客户端就改为监听状态
    {
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
        init();
        return true;
    }

    int mv0len = m_iv[0].iov_len;

    //发送给客户端
    while(1)
    {
        temp = writev(m_sockfd,m_iv,m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                //继续注册写事件,缓冲区的内容没发送完
                modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMode);
                return true;
            }
            //其他的错误
            unmap();
            return false;
        }
        
        bytes_have_send += temp;
        bytes_to_send -= temp;
        //第一个缓冲区发送完毕
        if (bytes_have_send >= mv0len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
            printf("1: >= \n");
        }
        else 
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - temp;
            printf("2: < \n");
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);

            if (m_linger)
            {
                init();
                return true;
            }
            else 
            {
                return false;
            }
        }
    }
}

/*将http response包的内容写入到m_write_buf中
 * m_write_idx记m_iv[0]的长度
*/
bool http_conn::add_response(const char* format,...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    va_start(arg_list,format);

    //WRITE_BUFFER_SIZE-1-m_write_idx是建议值,当后面str长度大于它则无效
    int len = vsnprintf(m_write_buf+m_write_idx,
            WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) //stack smashing,程序直接终止
    {
        va_end(arg_list);
        return false;
    }
    
    m_write_idx += len;

    va_end(arg_list);
    LOG_INFO("response: %s",m_write_buf);

    return true;
}

//返回包的状态行
bool http_conn::add_status_line(int status,const char *title)
{
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

//包头
bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() && add_blank_line();
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n",content_len);
}

bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n","text/html");
}

//是否保持连接?
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n",(m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line()
{
    return add_response("%s","\r\n");
}

bool http_conn::add_content(const char* content)
{
    return add_response("%s",content);
}



//由do_request()函数转移过来
bool http_conn::process_write(HTTP_CODE ret)
{
    switch(ret)
    {
    //500内部错误:解析错误
    case INTERNAL_ERROR:
        add_status_line(500,error_500_title);//状态行
        add_headers(strlen(error_500_form));//头部字段+空行
        if (!add_content(error_500_form))//包的主题内容 
            return false;
        break;
    //404 not found
    case BAD_REQUEST:
        add_status_line(404,error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form))
            return false;
        break;
    //403:服务器拒绝请求(文件权限问题
    case FORBIDDEN_REQUEST:
        add_status_line(403,error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form))
            return false;
        break;
    //200:成功
    case FILE_REQUEST: 
        add_status_line(200,ok_200_title);
        if (m_file_stat.st_size != 0)//html文件肯定不为空
        {
            add_headers(m_file_stat.st_size);//body是html文件的长度
            m_iv[0].iov_base    = m_write_buf;//buf中是http状态行和头部字段
            m_iv[0].iov_len     = m_write_idx;
            m_iv[1].iov_base    = m_file_address;
            m_iv[1].iov_len     = m_file_stat.st_size; 
            m_iv_count = 2;
            //http status_line + headers + html size
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else 
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
            break;
        }
    default:
        return false;
    }

    //除了FILE_REQUEST外,其他的情况http返回包已经组装好,所以发送缓冲区就1个
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

void http_conn::process()
{
    //处理http请求包
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
        return ;
    }
    //将解析后的http包的请求结果生成http返回包
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd,m_sockfd,EPOLLOUT,m_TRIGMode);//监听事件类型改为OUT
}
