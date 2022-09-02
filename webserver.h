#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"
// 最大文件描述符
const int MAX_FD = 65536;
// 最大事件数
const int MAX_EVENT_NUMBER = 10000;
// 最小超时时间
const int TIMESLOT = 5;

class WebServer
{
    public:
    WebServer();
    ~WebServer();
    void init(int port, string user, string passwd, string databaseName,
                int long_write, int opt_linger, int tirgmode, int sql_num,
                int thread_num, int close_log, int actor_model);
    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in client_data);
    void adjust_timer(util_timer* timer);
    void deal_timer(util_timer* timer, int sockfd);
    bool dealclientdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

    public:
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;
    
    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    // 数据库相关
    connection_pool* m_connPool;  // 数据库连接池
    string m_user;  // 数据库用户名
    string m_passwd;  // 用户密码
    string m_databaseName;  // 所使用的数据库
    int m_sql_num; // 最大连接数量

    // 线程池相关
    threadpool<http_conn> * m_pool;
    int m_thread_num; // 线程池中线程数量

    // 用来epoll_wait中接受就绪的事件 只需遍历该数据 提高了事件索引效率
    epoll_event events[MAX_EVENT_NUMBER]; 
    int m_listenfd;  // 保存主线程创建的socket监听文件描述符
    int m_OPT_LINGER; // 这里控制close关闭socket的行为 优雅关闭连接
    int m_TRIGMode;  // 取值0 1 2 3 分别为 （m_LISTENTrigMode,m_CONNTrigMode）所组成的二进制的状态
    int m_LISTENTrigMode;  // Socket监听事件的触发方式 0 LT 1 ET
    int m_CONNTrigMode;  // Socket连接描述符的触发方式 0 LT 1 ET

    // 定时器相关
    client_data *users_timer;
    Utils utils;
};
#endif