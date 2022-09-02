/*

    该文件主要实现了一个升序定时器链表，将其中的定时器按照超时时间做升序排序。

*/

#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
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
#include <time.h>

#include "../log/log.h"

class util_timer;

// 用户数据结构
struct client_data
{
    sockaddr_in address;    // 客户端socket地址
    int sockfd;     // socket文件描述符
    util_timer* timer;  // 定时器
};

class util_timer
{
    public:
        util_timer() : prev(NULL), next(NULL) {}

    public:
        time_t expire;  // 失效时间
        client_data* user_data;     // 用户数据 由定时器的执行者传递给回调函数
        void (* cb_func)(client_data *);    // 任务回调函数
        util_timer *prev;       // 指向前一个定时器
        util_timer *next;       // 指向下一个定时器
};

// 定时器链表 带有头节点和尾节点的升序的双向链表，
class sort_timer_lst
{
    public:
        sort_timer_lst();
        ~sort_timer_lst();

        void add_timer(util_timer* timer);
        void adjust_timer(util_timer* timer);
        void del_timer(util_timer* timer);
        void tick();  // SIGALRM信号每次被触发就在其信号处理函数上执行一次tick函数

    private:
    void add_timer(util_timer* timer, util_timer* lst_head);

    util_timer* head;
    util_timer* tail;
};

class Utils
{
    public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    int setnonblocking(int fd);

    void addfd(int epollfd, int fd, bool one_shot, int TRIGMOde);

    static void sig_handler(int sig);

    void add_sig(int sig, void(handler)(int), bool restart = true);

    void timer_handler();

    void show_error(int connfd, const char* info);

    public:
    static int* u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);



#endif