#ifndef WEBSERVER_CONFIG_H
#define WEBSERVER_CONFIG_H

#include "webserver.h"

class Config {
 public:
  Config();
  ~Config(){};

  /// @brief 对终端输入的命令参数做出解析,以选择不同的配置
  /// @param argc 参数个数
  /// @param argv 命令行参数
  void ParseArg(int argc, char* argv[]);
  
  // 端口号
  int port_;

  // 日志写入的方式
  int log_write_;

  // 触发组合模式
  int trig_mode_;

  // listenfd 触发模式
  int listen_trig_mode_;

  // connfd 触发模式
  int conn_trig_mode_;

  // 优雅关闭连接
  int opt_linger_;

  // 数据库连接池数量
  int sql_pool_num_;

  // 线程池内的线程数量
  int thread_pool_num_;

  // 是否关闭日志
  int close_log_;

  // 并发模式选择
  int acotr_model_;
};


#endif