#ifndef HTTPCONNCTION_H
#define HTTPCONNCTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn
{
    public:
    // 文件名的最大长度
    static const int FILENAME_LEN = 200;
    // 读缓冲区的大小
    static const int READ_BUFFER_SIZE = 2048;
    // 写缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;
    // HTTP的请求方法
    enum METHOD
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
    // 解析客户请求时，主机所处的状态
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    // 服务器处理http请求的结果
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        FORBIDDED_REQUEST,
        FILE_RQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    // 行的读取状态
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

    public:
    http_conn() {}
    ~http_conn() {}

    public:
    /* 初始化新接收的连接 */
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, 
            string user, string passwd, string sqlname);
    /* 关闭连接 */
    void close_conn(bool real_close = true);
    // 处理客户请求
    void process();
    // 非阻塞读取
    bool read_once();
    // 非阻塞写操作
    bool write();
    sockaddr_in* get_address()
    {
        return &m_address;
    }
    void initmysql_result(connection_pool* connPool);
    int timer_flag;
    int improv;
    
    private:
    // 初始化连接
    void init();
    // 解析HTTP请求
    HTTP_CODE process_read();
    // 填充HTTP应答
    bool process_write(HTTP_CODE ret);
    // 下面几个函数被process_read调用以分析HTTP请求
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    char *get_line() {return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    // 下面几个函数被process_write调用以填充HTTP应答
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

    public:
    // 所有socket上的事件都被注册到一个epoll内核事件表中，
    static int m_epollfd;
    // 统计用户数量
    static int m_user_count;
    MYSQL* mysql;
    int m_state;

    private:
    // 该http连接的socket和对方的socker地址
    int m_sockfd;
    sockaddr_in m_address;
    // 读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];
    // 标识读缓冲区中已经读入的客户数据的最后一个字节的下一个位置
    int m_read_idx;
    // 目前正在分析的字符在读缓冲区中的位置
    int m_check_idx;
    // 当前正在解析行的起始位置
    int m_start_line;
    // 写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];
    // 写缓冲区中待发送的字节数
    int m_write_idx;

    // 当前主机所处的状态
    CHECK_STATE m_check_state;
    // 请求方法
    METHOD m_method;
    // 客户请求的目标文件的绝对路径
    char m_real_file[FILENAME_LEN];
    // 客户请求的目标文件名
    char* m_url;
    // http的版本号
    char* m_version;
    // 主机名
    char* m_hosts;
    // HTTP请求的消息体的长度
    int m_content_length;
    // HTTP请求是否要保持连接
    bool m_linger;
    
    // 客户请求的目标文件被mmap到内存中的起始位置
    char* m_file_address;
    // 目标文件的状态
    struct stat m_file_stat;
    // 我们采用writev来执行写操作，
    // iovec 中有指向数据的指针和数据长度
    struct iovec m_iv[2];
    // m_iv_count标识被写内存块的数量
    int m_iv_count;

    int cgi;
    char* m_string;
    int bytes_to_send;
    int bytes_have_send;
    char* doc_root;
    
    map<string, string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];

};

#endif