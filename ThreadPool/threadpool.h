#ifndef WEBSERVER_THREADPOOL_H
#define WEBSERVER_THREADPOOL_H

#include <cstdio>
#include <pthread.h>
#include <list>
#include <exception>
#include "../Lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class ThreadPool {
 public:
  // 初始化线程池，thread_number 是线程池中线程的数量， max_requests 是请求队列最
  // 多允许的、等待处理的请求的数量
  ThreadPool(int actor_model, ConnectionPool* connection_pool, 
             int thread_number = 8, int max_request = 10000);
  ~ThreadPool();
  
  /// @brief 往工作队列里添加任务
  /// @param request 任务对象 
  /// @param state 是读还是写 ？
  /// @return 成功添加返回 true
  bool Append(T* request, int state);

  /// @brief 往工作队列中添加任务
  /// @param request 任务对象
  /// @return 成功添加返回 true
  bool Append_p(T* request);

 private:
  /// @brief 工作线程运行的函数，他不断从工作队列中取出任务并执行
  /// @param arg 线程的参数 
  /// @return 返回线程池对象
  static void* Worker(void* arg);
    
  void Run();

 private:
  int actor_model_;       // 模式切换
  int thread_number_;     // 线程池中线程的数量
  int max_requests_;      // 请求队列中允许的最大请求数
  pthread_t* threads_;    // 描述线程池的数组
  std::list<T*> work_queue_; //请求队列
  Locker queue_locker_;   // 保护请求队列的互斥锁
  Sem queue_stat_;         // 是否有任务需要处理
  ConnectionPool* conn_pool_; // 数据库连接池
};


template <typename T>
ThreadPool<T>::ThreadPool(int actor_model, ConnectionPool* connection_pool,
                          int thread_number, int max_requests)
    : actor_model_(actor_model),
      thread_number_(thread_number),
      max_requests_(max_requests),
      threads_(NULL),
      conn_pool_(connection_pool) {
  // 检查参数合法性
  if (thread_number <= 0 || max_requests <= 0) 
    throw std::exception();
  

  // 创建长度为 therad_number_ 的线程数组
  threads_ = new pthread_t[thread_number_];
  if (!threads_)
    throw std::exception();

  // 创建 thread_number 个线程
  for (int i = 0; i < thread_number; ++i) {
    if (pthread_create(threads_ + i, NULL, Worker, this) != 0) {
      delete[] threads_;
      throw std::exception();
    }
    // 分离每个子线程，使其自动回收资源
    if (pthread_detach(threads_[i])) {
      delete[] threads_;
      throw std::exception();
    }
  }
}

template <typename T>
ThreadPool<T>::~ThreadPool() {
  delete[] threads_;
}

template <typename T>
bool ThreadPool<T>::Append_p(T* request) { 
  // 上锁
  queue_locker_.Lock();
  // 判断请求的数量是否到达了上限
  if (work_queue_.size() >= max_requests_) {
    // 释放锁
    queue_locker_.Unlock();
    return false;
  }
  // 添加进工作队列
  work_queue_.push_back(request);
  // 释放锁
  queue_locker_.Unlock();
  // 通知消费者有任务可取
  queue_stat_.Post();
}

template <typename T>
bool ThreadPool<T>::Append(T* request, int state) {
  // 上锁
  queue_locker_.Lock();
  // 判断请求的数量是否到达了上限
  if (work_queue_.size() >= max_requests_) {
    // 释放锁
    queue_locker_.Unlock();
    return false;
  }
  // 修改对象的状态
  request->state_ = state;
  // 添加进工作队列
  work_queue_.push_back(request);
  // 释放锁
  queue_locker_.Unlock();
  // 通知消费者有任务可取
  queue_stat_.Post();
}

template <typename T>
void* ThreadPool<T>::Worker(void* arg) {
  ThreadPool* pool = (ThreadPool*)arg;
  pool->Run();
  return pool;
}

template <typename T>
void ThreadPool<T>::Run() {
  while (true) {
    // 信号量减 1
    queue_stat_.Wait();
    queue_locker_.Lock();
    if (work_queue_.empty()) {
      queue_locker_.Unlock();
      continue;
    }
    // 取出任务
    T* request = work_queue_.front();
    work_queue_.pop_front();
    queue_locker_.Unlock();
    if (!request)
      continue;

    // 还有很多问题没弄懂
    if (1 == actor_model_) { // ET模式
      if (0 == request->state_) { // 需要读数据
        if (request->ReadOnce()) {
          request->improv_ = 1;
          ConnectionRAII mysql_con(&request->mysql_, conn_pool_);
          request->Process();
        } else {
         request->improv_ = 1;
         request->timer_flag_ = 1; 
        }
      } else {
        if (request->Write()) {
          request->improv_ = 1;
        } else {
          request->improv_ = 1;
          request->timer_flag_ = 1;
        }
      }
    } else { // LT 模式
      ConnectionRAII mysql_con(&request->mysql_, conn_pool_);
      request->Process();
    }
  }
}
#endif