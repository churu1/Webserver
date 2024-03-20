#include "webserver.h"

WebServer::WebServer() {
  // http_conn 类对象
  users_ = new HttpConn[g_kMaxFd];

  // root 文件夹
  char server_path[200];
  getcwd(server_path, 200);
  char root[6] = "/root";
  root_ = (char*)malloc(strlen(server_path) + strlen(root) + 1);
  strcpy(root_, server_path);
  strcat(root_, root);


  // 定时器
  users_timer_ = new ClientData[g_kMaxFd];
}

WebServer::~WebServer() {
  close(epollfd_);
  close(pipefd_[1]);
  close(pipefd_[0]);
  close(listenfd_);
  delete[] users_;
  delete[] users_timer_;
  delete pool_;
}


void WebServer::Init(int port, string user, string passwd, string database_name,
                     int log_write, int opt_linger, int trigmode, int sql_num,
                     int thread_num, int close_log, int actor_model) {
  port_ = port;
  user_ = user;
  passwd_ = passwd;
  database_name_ = database_name;
  sql_num_ = sql_num;
  thread_num_ = thread_num;
  log_write = log_write_;
  opt_linger_ = opt_linger;
  trig_mode_ = trigmode;
  close_log_ = close_log;
  actor_model = actor_model_;
}

void WebServer::SqlPool() {
  // 初始化数据库连接池
  connection_pool_ = ConnectionPool::GetInstance();
  connection_pool_->Init("localhost", user_, passwd_, database_name_, 3306,
                         sql_num_, close_log_);

  // 初始化数据库读取表
  users_->InitMySqlResult(connection_pool_);
}

void WebServer::TrigMode() {
  // Listen + epoll
  // LT + LT
  if (0 == trig_mode_) {
    listen_trig_mode_ = 0;
    conn_trig_mode = 0;
  } else if (1 == trig_mode_) {
    // LT + ET
    listen_trig_mode_ = 0;
    conn_trig_mode = 1;
  } else if (2 == trig_mode_) {
    // ET + LT
    listen_trig_mode_ = 1;
    conn_trig_mode = 0;
  } else if (3 == trig_mode_) {
    // ET + ET
    listen_trig_mode_ = 1;
    conn_trig_mode = 1;
  }
}

void WebServer::LogWrite() {
  if ( 0 == close_log_) {
    // 初始化日志
    if (1 == log_write_) {
      // 异步写日志
      Log::GetInstance()->Init("./ServerLog", close_log_, 2000, 800000, 800);
    } else {
      // 同步写日志
      Log::GetInstance()->Init("./ServerLog", close_log_, 2000, 800000, 0);
    }
  }
}


void WebServer::CreateThreadPool() {
  pool_ = new ThreadPool<HttpConn>(actor_model_, connection_pool_, thread_num_);

}

void WebServer::EventListen() {
  // 网络编程基础步骤
  // 创建套接字
  listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
  assert(listenfd_ >= 0);

  // 是否立即关闭连接
  if (0 == opt_linger_) {
    struct linger tmp = {0, 1}; // 不启用 SO_LINGER 选项
    // 不发送缓存中的数据，并立即关闭连接
    setsockopt(listenfd_, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
  } else if(1 == opt_linger_){
    struct linger tmp = {1, 1}; // 启用 SO_LINGER 选项，在套接字关闭时，操作系统
    // 会等待一段时间，这里是 1 s，直到发送缓冲区中的数据被发送完毕或者超过指定时间
    // 后强制关闭连接
  }

  int ret = 0;
  struct sockaddr_in address;
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port_);

  int flag = 1;
  // 设置端口复用
  setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  // 绑定套接字地址
  ret = bind(listenfd_, (struct sockaddr*) &address, sizeof(address));
  assert(ret >= 0); // 断言
  // 开始监听
  ret = listen(listenfd_, 5);
  assert(ret >= 0);

  // 初始化定时器工具类
  utils_.Init(g_kTimeSlot);

  // epoll 创建内核事件表
  epoll_event events[g_kMaxEventNumber]; // 不是已经有一个这样的成员属性了吗？
  epollfd_ = epoll_create(5);
  assert(epollfd_ != -1);
  
  //使用定时器工具类来添加文件描述符到内核时间表中，因为我们需要管理每个连接的超时时间
  utils_.Addfd(epollfd_, listenfd_, false, listen_trig_mode_);
  HttpConn::epoll_fd_ = epollfd_;

  // 创建双向管道
  ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd_);
  assert(ret != -1);

  // 设置管道写端为非阻塞写
  utils_.SetNonBlocking(pipefd_[1]);
  utils_.Addfd(epollfd_, pipefd_[0], false, 0);
  
  // 忽略 SIGPIPE 信号
  utils_.AddSig(SIGPIPE, SIG_IGN);
  // 添加定时器信号
  utils_.AddSig(SIGALRM, utils_.SigHandler, false);
  // 添加终止线程信号
  utils_.AddSig(SIGTERM, utils_.SigHandler, false);

  // 开始定时
  alarm(g_kTimeSlot);

  // 工具类，信号和描述符基本操作
  Utils::pipefd_ = pipefd_;
  Utils::epollfd_ = epollfd_;

}

// 初始化用户数据并为该用户创建一个定时器
void WebServer::Timer(int connfd, struct sockaddr_in client_address) {
  // 为该用户初始化
  users_[connfd].Init(connfd, client_address, root_, conn_trig_mode, close_log_,
                     user_, passwd_, database_name_);

  // 初始化 client_data 数据
  // 创建定时器，设置回调函数和超时事件，绑定用户数据，将定时器添加到链表中
  users_timer_[connfd].address = client_address;
  users_timer_[connfd].sockfd = connfd;
  UtilTimer* timer = new UtilTimer;
  timer->cb_func = cb_func;
  time_t cur = time(NULL);
  // 为每个连接设置一个超时时间：cur + 3 * g_TimerSlot
  // 当这个连接有新的读写时间则再更新这个连接的超时时间
  timer->expire_ = cur + 3 * g_kTimeSlot;
  users_timer_[connfd].timer = timer;
  utils_.timer_list_.AddTimer(timer);
}

// 某个连接上有数据的读写，则定时器往后延迟三个单位
// 并对新的定时器在链表上的位置进行调整
void WebServer::AdjustTimer(UtilTimer* timer) {
  time_t cur = time(NULL);
  timer->expire_ = cur + 3 * g_kTimeSlot;
  utils_.timer_list_.AdjustTimer(timer);

  LOG_INFO("%s", "adjust timer once");

}

void WebServer::DealTimer(UtilTimer* timer, int sockfd) {
  timer->cb_func(&users_timer_[sockfd]);
  if (timer) { // listenfd LT 模式
    utils_.timer_list_.DelTimer(timer);
  }

  LOG_INFO("close fd %d", users_timer_[sockfd].sockfd);
}

// 根据不同的监听模式进行不同的客户端连接处理
// 对于 LT 的监听模式，一次只需要读取一个客户端连接即可。对于 ET 的监听模式
// 一次性要将所有的客户端连接都读取出来
// 共性的情况：1.accept函数调用失败->输出错误信息
//            2.accept函数调用成功，但是服务器的连接已经达到上限->通知客户端相关
//                信息，并关闭连接
//            3.accept函数调用成功，并且服务器没有达到连接上限->通过Timer()进行
//                用户数据初始化并为该用户创建一个定时器以检测是否是非活跃连接
bool WebServer::DealClientData() {
  struct sockaddr_in client_address;
  socklen_t client_address_length = sizeof(client_address);
  if (0 == listen_trig_mode_) {
    int connfd = accept(listenfd_, (struct sockaddr*)&client_address, 
                        &client_address_length);
    if (connfd < 0) {
      LOG_ERROR("%s:errno is:%d", "accept error", errno);
      return false;
    }
    if (HttpConn::user_count_ >= g_kMaxFd ) {
      utils_.ShowError(connfd, "Internal server busy");
      // 使用日志类打印相关错误信息
      LOG_ERROR("%s", "Internal server busy");
      return false;
    }
    Timer(connfd, client_address);
  } else {
    while(1) {
      int connfd = accept(listenfd_, (struct sockaddr*)&client_address, 
                          &client_address_length);
      if (connfd < 0) {
        LOG_ERROR("%s:errno is:%d", "accept error", errno);
        break;
      }
      if (HttpConn::user_count_ >= g_kMaxFd  ) {
        utils_.ShowError(connfd, "Internal server busy");
        LOG_ERROR("%s", "Internal server busy");
        break;
      }
      Timer(connfd, client_address);
    }
    return false;
  }
  return false;
}

bool WebServer::DealWithSignal(bool& timeout, bool& stop_server) {
  int ret = 0;
  int sig;
  char signals[1024];
  ret = recv(pipefd_[0], signals, sizeof(signals), 0);
  if (-1 == ret) {
    return false;
  } else if (0 == ret) {
    return false;
  } else {
    for (int i = 0; i < ret; ++i) {
      switch (signals[i]) {
        case SIGALRM : {
          timeout = true;
          break;
        }
        case SIGTERM : {
          stop_server = true;
          break;
        }
      }
    }
  }
  return true;
}

void WebServer::DealWithRead(int sockfd) {
  UtilTimer* timer = users_timer_[sockfd].timer;

  // reactor 反应堆模型
  if (1 == actor_model_) {
    if (timer) {
      AdjustTimer(timer);
    }

    // 若检测到读事件，将该事件放入请求队列
    pool_->Append(users_ + sockfd, 0);
    while (true) {
      if (1 == users_[sockfd].improv_) {
        if (1 == users_[sockfd].timer_flag_) {
          DealTimer(timer, sockfd);
        }
        users_[sockfd].improv_ = 0;
        break;
      }
    }
  } else {
    // proactor 前置器模式
    if (users_[sockfd].ReadOnce()) {
      // 日志类打印相关信息
      LOG_INFO("deal with the clietn(%s)", inet_ntoa(users_[sockfd].GetAddress()->sin_addr));
      // 若检测到读事件，将该事件放入请求队列
      pool_->Append_p(users_ + sockfd);

      if (timer) {
        AdjustTimer(timer);
      }
    } else {
      DealTimer(timer, sockfd);
    }
  }
}

void WebServer::DealWithWrite(int sockfd) {
  UtilTimer* timer = users_timer_[sockfd].timer;
  // reactor 反应堆模式
  if (1 == actor_model_) {
    if (timer) {
      AdjustTimer(timer);
    }

    pool_->Append(users_ + sockfd, 1);

    while (true) {
      if (1 == users_[sockfd].improv_) {
        if (1 == users_[sockfd].timer_flag_) {
          DealTimer(timer, sockfd);
          users_[sockfd].timer_flag_ = 0;
        }
        users_[sockfd].improv_ = 0;
        break;
      }
    }
  } else {
    // proactor 前置器模式
    if (users_[sockfd].Write()) {
      // 日志类打印相关信息
      LOG_INFO("send data to the client(%s)", inet_ntoa(users_[sockfd].GetAddress()->sin_addr));
      if (timer) {
        AdjustTimer(timer);
      }
    } else {
      DealTimer(timer, sockfd);
    }
  }
}


// 事件循环
// 事件类型有：信号（定时器）事件、IO事件
void WebServer::EventLoop() {
  bool timeout = false;
  bool stop_server = false;

  while(!stop_server) {
    int number = epoll_wait(epollfd_, events_, g_kMaxEventNumber, -1);
    if (number < 0 && errno != EINTR) {
      LOG_ERROR("%s", "epoll failure");
      break;
    }

    for (int i = 0; i < number; ++i) {
      int sockfd = events_[i].data.fd;

      // 处理新接到的客户连接
      if (sockfd == listenfd_) {
        bool flag = DealClientData();
        if (false == flag)
          continue;  
      } else if (events_[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        // 服务端关闭连接，移除对应的定时器
        UtilTimer* timer = users_timer_[sockfd].timer;
        DealTimer(timer, sockfd);
      } else if ((sockfd == pipefd_[0]) && (events_[i].events & EPOLLIN)) {
        // 处理信号
        bool flag = DealWithSignal(timeout, stop_server);
      } else if (events_[i].events & EPOLLIN) {
        // 处理客户端连接上收到的数据
        DealWithRead(sockfd);
      } else if (events_[i].events & EPOLLOUT) {
        // 处理工作线程写入的数据
        DealWithWrite(sockfd);
      }
    }

    // 最后处理定时器事件
    if (timeout) {
      utils_.TimerHandler();
      LOG_INFO("%s", "timer tick");
      timeout = false;
    }
  }
}