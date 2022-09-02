#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"
#include "../log/log.h"
using namespace std;

// 类的构造函数 
connection_pool::connection_pool()
{
    m_cur_conn = 0;
    m_free_conn = 0;
}

// 得到实例
connection_pool* connection_pool::get_instance()
{
    static connection_pool conn_pool;
    return &conn_pool;
}

// 初始化函数
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
            LOG_ERROR("MySQL Error, connect init failed!");
            exit(1);
        }
        con = mysql_real_connect(con, url.c_str(), user.c_str(), password.c_str(),
                        db_name.c_str(), port, NULL, 0);
        if (NULL == con) 
        {
            LOG_ERROR("MySQL Error, real connect failed!");
            exit(1);
        }         
        // 向连接池中加入连接对象  
        m_conn_list.push_back(con);             
        ++m_free_conn;
    }
    // 初始化信号量
    this->m_reserve = sem(m_free_conn);
    m_max_conn = m_free_conn;
}

// 从数据库中返回一个可用连接并更新使用和空闲连接数
MYSQL* connection_pool::get_connection()
{
    MYSQL* conn = NULL;
    if (0 == m_conn_list.size())
    {
        return NULL;
    }
    m_reserve.wait();       // 余量信号量减一
    
    m_lock.lock();      // 保护连接池和连接数的变量的互斥锁 加锁

    conn = m_conn_list.front();
    m_conn_list.pop_front();

    --m_free_conn;
    ++m_cur_conn;

    m_lock.unlock();     // 保护连接池和连接数的变量的互斥锁 解锁

    return conn;
}
// 归还连接资源
bool connection_pool::release_connection(MYSQL* con)
{
    if (NULL == con)
    {
        return false;
    }
    m_lock.lock();      // 共享资源加锁
    m_conn_list.push_back(con);
    ++m_free_conn;
    --m_cur_conn;
    m_lock.unlock();    // 共享资源解锁
    m_reserve.post();   // 余量信号量加一
    return true;
}
// 销毁连接池
void connection_pool::destroy_pool()
{
    m_lock.lock();
    if (m_conn_list.size() > 0)
    {
        // 连接池中存放的连接指针需要逐一取出销毁
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

