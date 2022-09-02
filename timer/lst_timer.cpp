#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst()
{
    this->head = NULL;
    this->tail = NULL;
}
sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while(tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}
void sort_timer_lst::add_timer(util_timer* timer)
{
    if (!timer)
    {
        return ;
    }
    if (!head)
    {
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}
void sort_timer_lst::adjust_timer(util_timer* timer)
{
    if (!timer)
    {
        return;
    }
    util_timer *tmp = timer->next;
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}
void sort_timer_lst::del_timer(util_timer* timer)
{
    if (!timer)
    {
        return;
    }
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
    return;
}
void sort_timer_lst::tick()
{
    if (!head)
    {
        return;
    }
    time_t cur = time(NULL);
    util_timer* tmp = head;
    while(tmp)
    {
        if (cur < tmp->expire)
        {
            break;
        }
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if (head)
        {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
    util_timer* prev = lst_head;
    util_timer* tmp = prev->next;
    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::init(int time_slot)
{
    m_TIMESLOT = time_slot;
}

// 使用fcntl系统调用将传入的文件描述符fd设为非阻塞的
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 向EPoll内核事件中注册事件 并将fd设为非阻塞的
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event; // 事件
    // epoll_event.data 是用户数据
    // epoll_event.data.fd 指定事件所从属的目标文件描述符
    event.data.fd = fd;

    // 指定是否是ET模式  ET模式需要额外设置EPOLLET
    // epoll_event.events 指定Epoll事件类型
    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    else
        event.events = EPOLLIN | EPOLLRDHUP;
    // 注册EPOLLONESHOT事件的文件操作符 最多触发一次 
    // 防止同一个Socket连接被多个线程分开处理
    if (one_shot)
        event.events |= EPOLLONESHOT;

    // epoll_ctl用来控制内核事件表
    // param_1：内核事件表的描述符,
    // param_2：ADD、MOD和DEL指定操作类型
    // param_3: 要操作的文件描述符
    // param_4: 选项由上面指定
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    // 使用fcntl系统调用将fd设为非阻塞的
    setnonblocking(fd); 
}

// 信号处理函数
void Utils::sig_handler(int sig)
{
    // 保留原来的errno 在函数最后恢复 以保证函数的可重入性
    int save_errno = errno;
    int msg = sig;
    // 将信号写入管道，已通知主循环
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}
// 设置信号函数
void Utils::add_sig(int sig, void(handler)(int), bool restart)
{
    struct  sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
    {
        sa.sa_flags |= SA_RESETHAND;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) !=  -1);
}

// 处理定时任务，并且重新定时以再次触发
void Utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char* info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int* Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data* user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}


