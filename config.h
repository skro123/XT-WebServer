/*

    配置类 配置服务器运行参数

*/

#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>
#include <getopt.h>
#include "webserver.h"

using namespace std;

class Config
{
    public:
    Config();
    ~Config(){};

    void parse_arg(int argc, char* argv[]);

    // 端口号
    int port;
    // 日志写入方式
    int log_write;
    // 触发组合模式
    int trig_mode;
    // listenfd触发模式
    int listen_trigMode;
    // connfg触发模式
    int conn_trig_mode;
    // 优雅关闭连接？
    int opt_linger;
    // 数据库连接池
    int sql_num;
    // 线程池内线程数量
    int thread_num;
    // 是否关闭日志
    int close_log;
    // 并发模型选择
    int actor_model;

};

#endif