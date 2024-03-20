#ifndef WEBSERVER_WEBSERVER_H
#define WEBESRVER_WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <iostream>
using std::string;

#include "./ThreadPool/threadpool.h"
#include "./Timer/Utils/Utils.h"
#include  "./Http/http_conn.h"


const int g_kMaxFd = 65536;           // 最大文件描述符
const int g_kMaxEventNumber = 10000;  // 最大事件数
const int g_kTimeSlot = 5;             // 最小超时单位

class WebServer {
 public:
  WebServer();
  ~WebServer();

  /// @brief 初始化服务器相关配置
  /// @param port 端口号
  /// @param user 用户名
  /// @param password 密码 
  /// @param database_name 数据库名称 
  /// @param log_write 日志的写入模式
  /// @param opt_linger 优雅关闭连接
  /// @param trig_mode  触发组合模式
  /// @param sql_pool_num 数据库连接池中数据库连接的数量
  /// @param trehad_pool_num 线程池中线程的数量
  /// @param close_log  是否关闭日志
  /// @param actor_model 并发模型
  void Init(int port, string user, string password, string database_name, 
            int log_write, int opt_linger, int trig_mode, int sql_pool_num,
            int trehad_pool_num, int close_log, int actor_model);

  void CreateThreadPool();
  void SqlPool();
  void LogWrite();
  void TrigMode();
  void EventListen();
  void EventLoop();
  void Timer(int confd, struct sockaddr_in client_address);
  void AdjustTimer(UtilTimer* timer);
  void DealTimer(UtilTimer* timer, int sockfd);
  bool DealClientData();
  bool DealWithSignal(bool& timeout, bool& stop_server);
  void DealWithRead(int sockfd);
  void DealWithWrite(int sockfd);

 public:
  // 基础
  int port_;
  char* root_;
  int log_write_;
  int close_log_;
  int actor_model_;

  int pipefd_[2];
  int epollfd_;
  HttpConn* users_;

  // 数据相关
  ConnectionPool* connection_pool_;
  string user_; // 登录数据库的用户名
  string passwd_; // 登录数据库的密码
  string database_name_; // 使用数据库名
  int sql_num_;
  // 线程池相关
  ThreadPool<HttpConn>* pool_;
  int thread_num_;

  // epoll_event 相关
  epoll_event events_[g_kMaxEventNumber];

  int listenfd_;
  int opt_linger_;
  int trig_mode_;
  int listen_trig_mode_;
  int conn_trig_mode;

  // 定时器相关
  ClientData* users_timer_;
  Utils utils_;
};

#endif