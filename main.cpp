#include "config.h"
#include <iostream>
using namespace std;
int main(int argc, char *argv[])
{
    //请修改的数据库信息
    // 登录名
    string user = "root";
    // 密码
    string passwd = "MyHqSql.543";
    // 库名
    string databasename = "webserver";

    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    //初始化
    server.init(config.port, user, passwd, databasename, config.log_write, 
                config.opt_linger, config.trig_mode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    
    //日志
    server.log_write();
    std::cout<<"log_write OK" << endl;
    //数据库
    server.sql_pool();
    std::cout<<"sql_pool OK" << endl;
    //线程池
    server.thread_pool();
    std::cout<<"thread_pool OK" << endl;
    //触发模式
    server.trig_mode();
    std::cout<<"trig_mode OK" << endl;
    //监听
    server.eventListen();
    std::cout<<"eventListen OK" << endl;
    //运行
    std::cout<<"start eventLoop" << endl;    
    server.eventLoop();
    return 0;
}