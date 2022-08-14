#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

connection_pool::connection_pool()
{
    m_cur_conn = 0;
    m_free_conn = 0;
}

connection_pool* connection_pool::get_instance()
{
    static connection_pool conn_pool;
    return &conn_pool;
}

void connection_pool::init(string url, string user, string password,
        string db_name, int port, int max_conn, int close_log)
{
    m_url = url;
    m_user = user;
    m_port = port;
    m_password = password;
    m_database_name = db_name;
    m_close_log = close_log;

    for (int i = 0; i <max_conn; ++i)
    {
        MYSQL* con = NULL;
        con = mysql_init(con);
        if (NULL == con) 
        {
            // log
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), user.c_str(), password.c_str(),
                        db_name.c_str(), port, NULL, 0);
        if (NULL == con) 
        {
            // log
            exit(1);
        }           
        m_conn_list.push_back(con);             
        ++m_free_conn;
    }
    this->m_reserve = sem(m_free_conn);
    m_max_conn = m_free_conn;
}

MYSQL* connection_pool::get_connection()
{
    MYSQL* conn = NULL;
    if (0 == m_conn_list.size())
    {
        return NULL;
    }
    m_reserve.wait();
    
    m_lock.lock();
    conn = m_conn_list.front();
    m_conn_list.pop_front();

    --m_free_conn;
    ++m_cur_conn;

    m_lock.unlock();
    return conn;
}

bool connection_pool::release_connection(MYSQL* con)
{
    if (NULL == con)
    {
        return false;
    }
    m_lock.lock();
    m_conn_list.push_back(con);
    ++m_free_conn;
    --m_cur_conn;
    m_lock.unlock();
    m_reserve.post();
    return true;
}

void connection_pool::destroy_pool()
{
    m_lock.lock();
    if (m_conn_list.size() > 0)
    {
        for(list<MYSQL*>::iterator it = m_conn_list.begin(); it != m_conn_list.end();
                ++it)
        {
            MYSQL* con = *it;
            mysql_close(con);
        }
        m_free_conn = 0;
        m_cur_conn = 0;
        m_conn_list.clear();
    }
    m_lock.unlock();
}

int connection_pool::get_free_conn()
{
    return this->m_free_conn;
}

connection_pool::~connection_pool()
{
    destroy_pool();
}

connection_rall::connection_rall(MYSQL **SQL, connection_pool* conn_pool)
{
    *SQL = conn_pool->get_connection();
    conn_rall = *SQL;
    pool_rall = conn_pool;
}

connection_rall::~connection_rall()
{
    pool_rall->release_connection(conn_rall);
}

