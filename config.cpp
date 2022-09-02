#include "config.h"

Config::Config()
{
    // 服务端口号 请不要设为为已使用或者知名端口号上(0~1023)
    port = 9006;

    // 日志写入方式 0表示同步
    log_write = 0;

    // 触发组合模式 控制下面两个取值
    trig_mode = 0;

    // socket监听的事件触发模式 在向epoll内核事件添加时使用
    listen_trigMode = 0;

    // socket连接的事件触发模式
    conn_trig_mode = 0;

    // 控制close行为 优雅关闭连接
    opt_linger = 0;

    // 数据库连接池大小
    sql_num = 8;

    // 线程池内的线程数量
    thread_num = 8;

    // 是否关闭日志
    close_log = 0;

    // 事件处理模型 0为proactor 1为reactor
    actor_model = 0;
}

void Config::parse_arg(int argc, char* argv[])
{
    int opt;

    const char* str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch(opt)
        {
            case 'p':
            {
                port = atoi(optarg);
                break;
            }
            case 'l':
            {
                log_write = atoi(optarg);
                break;
            }
            case 'm':
            {
                trig_mode = atoi(optarg);
                break;
            }
            case 'o':
            {
                opt_linger = atoi(optarg);
                break;
            }
            case 's':
            {
                sql_num = atoi(optarg);
                break;
            }
            case 't':
            {
                thread_num = atoi(optarg);
                break;
            }
            case 'c':
            {
                close_log = atoi(optarg);
                break;
            }
            case 'a':
            {
                actor_model = atoi(optarg);
                break;
            }
            default:
                break;
        }
    }
}