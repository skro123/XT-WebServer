#include "webserver.h"

WebServer::WebServer()
{
    // http_conn对象
    users = new http_conn[MAX_FD];
    // root文件夹路径
    char server_path[200];
    // 将当前工作目录的绝对路径复制到参数buffer所指的内存空间中,参数maxlen为buffer的空间大小。
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[0]);
    close(m_pipefd[1]);
    delete [] users;
    delete [] users_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string passwd, string databaseName, int log_write,
                    int opt_linger, int trigmode, int sql_num, int thread_num, int close_log,
                    int actor_model)
{
    m_port = port;
    m_user = user;
    m_passwd = passwd;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_model;
}

// 根据总体模式分别设置监听描述符和连接描述符的模式
void WebServer::trig_mode()
{
    if (0 == m_TRIGMode)
    {
        m_LISTENTrigMode = 0;
        m_CONNTrigMode = 0;
    }
    else if (1 == m_TRIGMode)
    {
        // LT + ET
        m_LISTENTrigMode = 0;
        m_CONNTrigMode = 1;
    }
    else if (2 == m_TRIGMode)
    {
        // ET + LT
        m_LISTENTrigMode = 1;
        m_CONNTrigMode = 0;
    }
    else if (3 == m_TRIGMode)
    {
        //ET + ET
        m_LISTENTrigMode = 1;
        m_CONNTrigMode = 1;
    }
}

void WebServer::log_write()
{
    if (0 == m_close_log)
    {
        //初始化日志
        if (1 == m_log_write)
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

void WebServer::sql_pool()
{
    // 初始化数据库连接池
    m_connPool = connection_pool::get_instance();
    // m_connPool = connection_pool::GetInstance();

    m_connPool->init("127.0.0.1", m_user, m_passwd, m_databaseName, 3306,
                m_sql_num, m_close_log);
    users->initmysql_result(m_connPool);
}

void WebServer::thread_pool()
{
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::eventListen()
{
    //---------------------------1.网络部分--------------------------------//
    // 创建socket监听描述符
    // PE_INET使用IPV4协议簇 若使用IPV6则为PF_INET6 UNIX本地协议簇则为PF_UNIX
    // SOCK_STREAM为流服务，即传输层使用TCP协议，若用UDP则为SOCK_UGRAM
    // 最后一个参数 0表示使用默认的具体协议
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0); // 描述符是一个正整数 创建失败返回-1并设置error

    // 设置close TCP连接时的行为 
    // 默认情况下系统调用close 关闭一个socket时 close立即返回 TCP模块将残留数据发送完
    if (0 == m_OPT_LINGER)
    {
        // l_onoff为0 表示不使用该选项 使用系统默认行为
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_OPT_LINGER)
    {
        // l_onoff为1 启用该选项 第二参数l_linger值若为0则close立即返回并TCP丢弃残留数据
        // 第二参数值大于0 close将等待l_linger长时间，非阻塞则立即返回
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    // 准备命名Socket的信息
    int ret = 0;
    struct sockaddr_in address; // IPV4 专用socket地址结构体
    bzero(&address, sizeof(address)); // 填充为0
    address.sin_family = AF_INET; // IPV4
    address.sin_addr.s_addr = htonl(INADDR_ANY); // IPV4地址
    address.sin_port = htons(m_port); // 端口号

    // 重用可能处于TIME_WAIT状态的socket地址
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    // 命令Socket 
    ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);
    // 创建一个长度为5的监听队列存放待处理的客户连接
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);
    //---------------------------网络部分-完--------------------------------//

    //---------------------------2.统一信号源-------------------------------//
    // 初始化定时器 设定定时器时隙
    utils.init(TIMESLOT);

    // Epoll创建一个内核事件表 并返回对应的文件描述符
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    // 将Socket监听符上的事件注册到Epoll内核事件表
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigMode);
    http_conn::m_epollfd = m_epollfd;

    // 创造一对未命名的、相互连接的UNIX域套接字。
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, this->m_pipefd);
    assert(ret != -1);
    // m_pipefd[1] 是写端 m_pipefd[0]是读端
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    // 忽略SIGPIPE信号 使用sigaction函数设置
    utils.add_sig(SIGPIPE, SIG_IGN);
    // 来自Alarm函数的定时器信号 使用自定义信号处理函数处理 写入管道
    utils.add_sig(SIGALRM, utils.sig_handler, false);
    // 软件终止信号 同样使用自定义的信号处理函数处理 写入管道
    utils.add_sig(SIGTERM, utils.sig_handler, false);
    //---------------------------统一信号源-完-------------------------------//

    // 开始定时
    alarm(TIMESLOT);

    // epoll事件表的文件描述符和用以通知定时和关闭信号的管道
    Utils::u_epollfd = m_epollfd;
    Utils::u_pipefd = m_pipefd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_data)
{
    users[connfd].init(connfd, client_data, m_root, m_CONNTrigMode, m_close_log,
                m_user, m_passwd, m_databaseName);
    
    users_timer[connfd].address = client_data;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

// 若有数据传输，则定时器往后延迟3个单位
// 并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer* timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    // utils.m_timer_lst.add_timer(timer); 
    utils.m_timer_lst.adjust_timer(timer); 

    LOG_INFO("$s", "adjust timer once");
}

void WebServer::deal_timer(util_timer * timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}
bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t clietn_addr_length = sizeof(client_address);
    if (0 == m_LISTENTrigMode) // LT
    {
        int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &clietn_addr_length);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }
    else 
    {
        while(1)
        {
            // 有一个监听队列 在ET模式下 不处理完下次就不通知了
            int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &clietn_addr_length);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (-1 == ret)
    {
        return false;
    }
    else if (0 == ret)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
                case SIGALRM:
                {
                    timeout = true;
                    break;
                }
                case SIGTERM:
                {
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    util_timer* timer = users_timer[sockfd].timer;

    // reactor模式
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        // 如果监听到读事件，将该事件放到请求队列
        // users + sockfd 代表一个http_conn对象 0表示读操作
        m_pool->append(users + sockfd, 0);
        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        // Proactor
        // 主线程先进行读
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            // 然后将该事件加入请求队列 让工作线程处理数据
            m_pool->append_p(users + sockfd);
            if (timer)
            {
                adjust_timer(timer);    
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    // reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        // 如果监听到写事件，将该事件放到请求队列
        m_pool->append(users + sockfd, 1);
        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        // Proactor
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        // 逐个处理events中的事件
        for (int i = 0; i < number; ++i)
        {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclientdata();
                if (false == flag)
                    continue;
            }
            // EPOLLRDHUP 事件,代表TCP连接被对方关闭或者对方关闭了写操作
            // EPOLLHUP 挂起 写端被关闭            
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 移除其对应的定时器
                util_timer* timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            // 处理关闭和超时信号
            // 采用统一信号源技术，内核发出的信号将由Utils::sig_handler处理，其将外部信号和定时器信号写入m_pipefd[1]
            // 这里dealwithsignal从m_pipefd[0]读入并对于SIGALRM和SIGTERM分别改写timeout和stop_server的状态
            // 在循环末尾以及下次循环开始分别处理超时和判断服务器是否停止
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            else if (events[i].events & EPOLLIN)  // 这里处理从客户连接上接受的数据
            {
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)  // // 这里处理往客户连接上写数据
            {
                dealwithwrite(sockfd);
            }
        }
        // 超时则处理定时任务并再次定时
        if (timeout)
        {
            utils.timer_handler();
            LOG_INFO("%s", "time tick");
            timeout = false;
        }
    }
}
