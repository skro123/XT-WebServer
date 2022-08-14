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
    MYSQL* get_connection();
    bool release_connection(MYSQL* conn);
    int get_free_conn();
    void destroy_pool();

    static connection_pool* get_instance();

    void init(string url, string user, string password, string m_database_name,
                int port, int max_conn, int close_log);

    public:
    string m_url;
    string m_port;
    string m_user;
    string m_password;
    string m_database_name;
    int m_close_log;

    private:
    connection_pool();
    ~connection_pool();

    int m_max_conn;
    int m_cur_conn;
    int m_free_conn;
    locker m_lock;
    list<MYSQL* > m_conn_list;
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