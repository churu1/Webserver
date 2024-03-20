/*
循环数组实现阻塞队列：back_ = (back_ + 1) % max_size_
*/

#ifndef WEBSERVER_BLOCKQUEUE_H
#define WEBSERVER_BLOCKQUEUE_H

#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <iostream>
#include "../Lock/locker.h"

template<typename T>
class BlockQueue {
 public:
  BlockQueue(int max_size = 1000) {
    if (max_size <= 0) {
      exit(-1);
    }    
    max_size_ = max_size;
    array_ = new T[max_size];
    size_ = 0;
    front_ = -1;
    back_ = -1;
  }

  // 清除队列中的内容
  void Clear() {
    mutex_.Lock();
    size_ = 0;
    front_ = -1;
    back_ = -1;
    mutex_.Unlock();
  }

  ~BlockQueue() {
    mutex_.Lock();
    if (array_ != NULL) 
      delete []array_;
    mutex_.Unlock();
  }

  bool IsFull() {
    mutex_.Lock();
    if (size_ >= max_size_) {
      mutex_.Unlock();
      return true;
    }
    mutex_.Unlock();
    return false;
  }

  // 判断队列是否为空
  bool IsEmpty() {
    mutex_.Lock();
    if (0 == size_) {
      mutex_.Unlock();
      return true;
    }
    mutex_.Unlock();
    return false;
  }

  // 返回队首元素
  bool Front(T& value) {
    mutex_.Lock();
    if (0 == size_) {
      mutex_.Unlock();
      return false;
    }
    value = array_[front_];
    mutex_.Unlock();
    return true;
  }

  // 返回队尾部元素
  bool Back(T& value) {
    mutex_.Lock();
    if (0 == size_) {
      mutex_.Unlock();
      return false;
    }
    value = array_[back_];
  }

  // 获得当前阻塞队列的大小
  int Size() {
    int tmp = 0;
    mutex_.Lock();
    tmp = size_;
    mutex_.Unlock();
    return tmp;
  }

 // 获得阻塞队列最大容量
 int MaxSize() {
    int tmp = 0;

    mutex_.Lock();
    tmp = max_size_;
    mutex_.Unlock();
    return tmp;
 }  

 // 往队列中添加元素，需要将所有使用列的线程先唤醒
 // 当元素 push 进队列，相当于生产者生产了一个元素
 // 若当前没有线程等待条件变量，则唤醒无意义
 bool Push(const T& item) {
  mutex_.Lock();
  if (size_ >= max_size_) {
    cond_.Broadcast();
    mutex_.Unlock();
    return false;
  }

  back_ = (back_ + 1) % max_size_;
  array_[back_] = item;

  ++size_;
  cond_.Broadcast();
  mutex_.Unlock();
  return true;
 }

 // 从队列中取出队列末尾的元素，如果当前队列没有元素，将会等待条件变量
 bool Pop(T& item) {
  mutex_.Lock();
  while (size_ <= 0) {
    if (!cond_.Wait(mutex_.Get())) {
      mutex_.Unlock();
      return false;
    }
  }

  front_ = (front_ + 1) % max_size_;
  item = array_[front_];
  --size_;
  mutex_.Unlock();
  return true;
 }

 // 增加了超时处理
 bool Pop(T& item, int ms_timeout) {
  struct timespec t = {0, 0};
  struct timeval now = {0, 0};
  gettimeofday(&now, NULL);
  mutex_.Lock();
  if (size_ <= 0) {
    t.tv_sec = now.tv_sec + ms_timeout / 1000; // s
    t.tv_nsec = (ms_timeout % 1000) * 1000; // ms
    if (!cond_.TimeWait(mutex_.Get(), t)) { // 不会一致阻塞，而是阻塞一定的时间
      mutex_.Unlock();
      return false;
    }
  }

  if (size_ <= 0) {
    mutex_.Unlock();
    return false;
  }
  
  front_ = (front_ + 1) % max_size_;
  item = array_[front_];
  --size_;
  mutex_.Unlock();
  return true;
 }





 private:
  Locker mutex_;
  Cond cond_;

  T* array_;
  int size_;
  int max_size_;
  int front_;
  int back_;
};



#endif