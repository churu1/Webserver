// Author: churu
// 这个头文件包含三个类：锁类、信号量类以及条件变量，用于实现并发控制
#ifndef WEBSERVER_LOCKER_H
#define WEBSERVER_LOCKER_H

#include <pthread.h>
#include <semaphore.h>
#include <exception>

class Sem {
 public:
  Sem() {
    if (sem_init(&sem_, 0, 0) != 0) { // 申请信号量
      throw std::exception();
    }
  }

  explicit Sem(int num) {
    if (sem_init(&sem_, 0, num) != 0) { // 申请信号量并设置 num
      throw std::exception();
    }
  }

  ~Sem() { // 释放资源
    sem_destroy(&sem_);
  }

  bool Wait() { // 对信号量加锁，调用一次信号量的值就减1，如果值为0就阻塞
    return sem_wait(&sem_) == 0;
  }

  bool Post() { // 对信号量解锁，调用一次对信号量的值加1
    return sem_post(&sem_) == 0;
  }

 private:
  sem_t sem_;

};

class Locker {
 public:
  Locker() {
    if (pthread_mutex_init(&mutex_, NULL) != 0) { // 获取互斥锁资源
        throw std::exception();
    }
  }

  ~Locker() { // 释放互斥锁资源
    pthread_mutex_destroy(&mutex_); 
  }

  bool Lock() { // 对互斥锁上锁
    return pthread_mutex_lock(&mutex_) == 0; 
  }

  bool Unlock() { // 对互斥锁解锁
    return pthread_mutex_unlock(&mutex_) == 0;   
  }

  pthread_mutex_t *Get() { // 获取互斥锁
    return &mutex_; 
  }

 private:
  pthread_mutex_t mutex_;
};

class Cond {
 public:
  Cond() { // 初始化条件变量
    if (pthread_cond_init(&cond_, NULL) != 0) {
        throw std::exception();
    }
  }

  ~Cond() {
    pthread_cond_destroy(&cond_);
  }

  bool Wait(pthread_mutex_t* mutex) {
    int ret = 0;
    ret = pthread_cond_wait(&cond_, mutex);
    return ret == 0;
  }
 
  bool TimeWait(pthread_mutex_t* mutex, struct timespec time) {
    int ret = 0;
    ret = pthread_cond_timedwait(&cond_, mutex, &time);
    return ret == 0;
  }

  bool Signal() {
    return pthread_cond_signal(&cond_) == 0;
  }

  bool Broadcast() {
    return pthread_cond_broadcast(&cond_) == 0;
  }

 private:
  pthread_cond_t cond_;
};

#endif  // WEBSERVER_LOCKER_Hnclude <pthread.h>
