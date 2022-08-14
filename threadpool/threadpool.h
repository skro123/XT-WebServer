#ifndef THREADPOOL_H
#define THREADPOOL_h

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
    public:
    threadpool(int actor_model, connection_pool* connPool, int thread_number = 8, 
            int max_request = 10000);
    ~threadpool();
    bool append(T* request, int state);
    bool append_p(T* request);

    private:
    static void * worker(void* arg);
    void run();

    private:
    int m_thread_num;               // 线程池容量
    int m_max_requests;             // 请求队列最大请求数
    pthread_t* m_threads;           // 线程标识符数组指针
    std::list<T*> m_work_queue;     // 请求队列
    locker m_queue_locker;          // 保护请求队列的互斥锁
    sem m_queue_stat;                 // 是否有任务需要处理
    connection_pool* m_conn_pool;   // 数据库 
    int m_actor_model;              // 模型切换
};

template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool* conn_pool, 
        int thread_number, int max_requests) 
        : m_actor_model(actor_model), m_thread_num(thread_number), 
            m_conn_pool(conn_pool), m_max_requests(max_requests)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_num];
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {
        // 创建线程
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete [] m_threads;
            throw std::exception();
        }
        // 分离线程
        if (pthread_detach(m_threads[i]))
        {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete [] m_threads;
}

template <typename T>
bool threadpool<T>::append(T* request, int state)
{
    m_queue_locker.lock();
    if (m_work_queue.size() >= m_max_requests)
    {
        m_queue_locker.unlock();
        return false;
    }
    request->m_state = state;
    m_work_queue.push_back(request);
    m_queue_locker.unlock();
    m_queue_stat.post();
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T* request)
{
    m_queue_locker.lock();
    if (m_work_queue.size() >= m_max_requests)
    {
        m_queue_locker.unlock();
        return false;
    }
    m_work_queue.push_back(request);
    m_queue_locker.unlock();
    m_queue_stat.post();
    return true;
}

template <typename T>
void* threadpool<T>::worker(void* arg)
{
    threadpool* pool = (threadpool *) arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while(true)
    {
        m_queue_stat.wait();
        m_queue_locker.lock();
        if ( m_work_queue.empty())
        {
            m_queue_locker.unlock();
            continue;
        }
        T* request = m_work_queue.front();
        m_work_queue.pop_front();
        m_queue_locker.unlock();

        if(!request)
            continue;
        
        if ( 1 == m_actor_model)
        {
            if ( 0 == request->m_state)
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connection_rall mySqlCon(&request->mysql, m_conn_pool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else 
        {
            connection_rall my_sql_conn(&request->mysql, m_conn_pool);
            request->process();
        }
    }
}





#endif