#ifndef HTTP_H
#define HTTP_H

#include "common.h"

#include "locker.h"
#include <lst_timer.h>
#include <log.h>
#include <sql.h>

class http_conn
{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;  
    enum METHOD //http请求类型
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE //
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE //HTTP状态码
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS //从状态机 
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd,const sockaddr_in &addr,char*,int,int,std::string,std::string,std::string);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address() { return &m_address; }
    //数据库连接池初始化
    void initmysql_result(connection_pool *);
    int timer_flag;
    int improv;

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *); //请求行
    HTTP_CODE parse_headers(char*); //请求头  
    HTTP_CODE parse_content(char*); // 空行+请求内容 
    HTTP_CODE do_request(); //执行请求 
    char *get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    void unmap();
    bool add_response(const char*,...);
    bool add_content(const char*);
    bool add_status_line(int status,const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state;

//HTTP类存储的变量
private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    int m_write_idx;
    CHECK_STATE m_check_state; //从 状态机 状态
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi; //是否启用POST
    char *m_string; //POST时包的主体的内容
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;

    std::map<std::string,std::string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif
