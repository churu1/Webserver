#ifndef WEBSERVER_UTILS_H
#define WEBSERVER_UTILS_H
/*
定时器类的工具类，也可以说是对外接口。
*/
#include "../LstTimer/lst_timer.h"
// #include "../WheelTimer/WheelTimers.h"
class Utils {
 public:
  Utils() {}
  ~Utils() {}

  // 初始化工具类
  void Init(int time_slot);

  // 对文件描述符设置非阻塞
  int SetNonBlocking(int fd);

  // 将内核事件表注册读事件，ET 模式，选择开启EPOLLONESHOT
  void Addfd(int epollfd, int fd, bool one_shot, int trig_mode);

  // 信号处理函数
  static void SigHandler(int sig);

  // 设置信号的函数
  void AddSig(int sig, void(handler)(int), bool restart = true);

  // 定时处理函数，重新定时以不断触发 SIGALARM 信号
  void TimerHandler();

  void ShowError(int confd, const char* info);
 public:
  static int* pipefd_;
  SortTimerLst timer_list_;
  //TimeWheel timer_wheel_;
  static int epollfd_;
  int time_slot_;

};

#endif