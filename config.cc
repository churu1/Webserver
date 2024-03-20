#include "config.h"

Config::Config() {
  // 端口号，默认9000
  port_ = 9000;

  // 日志写入方式，默认同步写入
  log_write_ = 0;

  // 触发组合模式，默认 listenfd LT + connfd LT
  trig_mode_ = 0;

  // listenfd 触发模式，默认 LT
  listen_trig_mode_ = 0;

  // connfd 触发模式，默认 LT
  conn_trig_mode_ = 0;

  // 优雅关闭连接，默认不使用
  opt_linger_ = 0;

  // 数据库连接池数量，默认8
  sql_pool_num_ = 8;

  // 线程池内的线程数量，默认8
  thread_pool_num_ = 8;

  // 是否关闭日志，默认不关闭
  close_log_ = 0;

  // 并发模型，默认是 proactor
  acotr_model_ = 0;
}

// 通过 getopt() 来解析命令参数中的不同字段, 然后通过 swtich case 语句将字段
// 赋值给对应的系统属性参数
void Config::ParseArg(int argc,char* argv[]) {
  int opt;
  const char* str = "p:l:m:o:s:t:c:a:";
  
  // 解析命令行参数
  while ((opt = getopt(argc, argv, str)) != -1) {
    switch (opt) {
      case 'p': {
        port_ = atoi(optarg);
        break;
      }
      case 'l': {
        log_write_ = atoi(optarg);
        break;
      }
      case 'm': {
        trig_mode_ = atoi(optarg);
        break;
      }
      case 'o': {
        opt_linger_ = atoi(optarg);
      }
      case 's': {
        sql_pool_num_ = atoi(optarg);
      }
      case 't': {
        thread_pool_num_ = atoi(optarg);
      }
      case 'c': {
        close_log_ = atoi(optarg);
      }
      case 'a': {
        acotr_model_ = atoi(optarg);
      }
      default:
        assert(false);
    }
  }
}