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

// #include "./threadpool/threadpool.h"
// "./http/http_coon.h"

const int g_kMaxFd = 65536;           // 最大文件描述符
const int g_kMaxEventNumber = 10000;  // 最大事件数
const int g_TimeSlot = 5;             // 最小超时单位

class WebServer{
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
  void init(int port, string user, string password, string database_name, 
            int log_write, int opt_linger, int trig_mode, int sql_pool_num,
            int trehad_pool_num, int close_log, int actor_model);
};

#endif