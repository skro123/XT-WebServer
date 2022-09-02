/*

 该类实现了一个数据库连接池，避免重复连接数据库带来的开销
 并使用RALL机制管理资源

*/
#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"


using namespace std;

class connection_pool
{
    public:
        MYSQL* get_connection();    // 获得数据库的连接
        bool release_connection(MYSQL* conn);   // 释放数据库的连接
        int get_free_conn();        // 获取连接
        void destroy_pool();        // 销毁所有连接

        static connection_pool* get_instance();     // 单例模式

        void init(string url, string user, string password, string database_name,
                    int port, int max_conn, int close_log);

    public:
        string m_url;       // 主机地址
        string m_port;      // 端口号
        string m_user;      // 用户名
        string m_password;      // 用户密码
        string m_database_name;     // 所用数据库名
        int m_close_log;        // 日志开关

    private:
        connection_pool();
        ~connection_pool();

        int m_max_conn;     // 最大连接数量
        int m_cur_conn;     // 当前连接数量
        int m_free_conn;    // 空闲连接数量
        locker m_lock;      // 保护连接池和连接数的变量的互斥锁
        list<MYSQL* > m_conn_list;      // 连接池
        sem m_reserve;
};

class connection_rall
{
    public:
    connection_rall(MYSQL** con, connection_pool* conn_pool);
    ~connection_rall();

    private:
    MYSQL* conn_rall;
    connection_pool* pool_rall;
};

#endif