#ifndef WEBSERVER_TIMER_H
#define WEBSERVER_TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
//#include "../Utils/Utils.h"
#include "../../Log/log.h"


class UtilTimer;

// 用户数据结构
struct ClientData {
  sockaddr_in address;
  int sockfd;
  UtilTimer* timer;
};


// 链表节点类
class UtilTimer {
 public:
  UtilTimer() : prev_(NULL),
                next_(NULL) {}

 public:

  void (* cb_func)(ClientData*);

  time_t expire_;
  ClientData* user_data_;
  UtilTimer* prev_;
  UtilTimer* next_;
};

// 双向排序链表
class SortTimerLst{
 public:
  SortTimerLst();
  ~SortTimerLst();

  void AddTimer(UtilTimer* timer);
  void AdjustTimer(UtilTimer* tiemr);
  void DelTimer(UtilTimer* timer);
  void Tick();

 private:
  void AddTimer(UtilTimer* timer, UtilTimer* lst_head);

  UtilTimer* head_;
  UtilTimer* tail_;
};


void cb_func(ClientData* user_data);


#endif

