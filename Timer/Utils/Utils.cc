#include "Utils.h"

void Utils::Init(int time_slot) {
  time_slot_ = time_slot;
}

// 对文件描述符设置非阻塞
int Utils::SetNonBlocking(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

// 向内核时间表注册读事件， ET 模式，选择开启 EPOLLONESHOT
void Utils::Addfd(int epollfd, int fd, bool one_shot, int trig_mode) {
  epoll_event event;
  event.data.fd = fd;

  if (1 == trig_mode) {
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
  } else {
      event.events = EPOLLIN | EPOLLRDHUP;
  }

  if (one_shot) {
    event.events |= EPOLLONESHOT;
  }

  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  SetNonBlocking(fd);
}

// 信号处理函数
void Utils::SigHandler(int sig) {
  // 为了保证函数的可重入性，保留原来的 errno 什么是可重入性？
  int save_errno = errno;
  int msg = sig;
  send(pipefd_[1], (char*)&msg, 1, 0);
  errno = save_errno;
}

// 设置信号函数
void Utils::AddSig(int sig, void(handler)(int), bool restart) {
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = handler;
  if (restart) {
    sa.sa_flags |= SA_RESTART;
  }
  sigfillset(&sa.sa_mask);
  assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::TimerHandler() {
  timer_list_.Tick();
  alarm(time_slot_);
}

// 服务器连接数量达到上限，通知客户端并关闭连接
void Utils::ShowError(int connfd, const char* info) {
  send(connfd, info, strlen(info), 0);
  close(connfd);
}

int* Utils::pipefd_ = 0;
int Utils::epollfd_ = 0;
