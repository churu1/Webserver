#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

// 定义 http 响应的一些状态信息
const char* kOk200Title = "OK";
const char* kError400Title = "Bad Request";
const char* kError400Form = "Your request has bad syntax or is inherently "
                             "impossible to staisfy.\n"; 
const char* kError403Title = "Forbidden";
const char* kError403Form = "You do not have permission to get file from this "
                            "server.\n";
const char* kError404Title = "Not Found";
const char* kError404Form = "The requested file was not found on this server.\n";
const char* kError500Title = "Internal Error";
const char* kError500Form = "There was an unusual problem serving this request "
                            "file.\n";

Locker lock_; // 等价于静态成员变量
map<string, string> users_; // 等价于静态成员变量

void HttpConn::InitMySqlResult(ConnectionPool* connection_pool) {
  // 先从连接池获取一个连接
  MYSQL* mysql = NULL;
  ConnectionRAII my_sql_conneciont(&mysql, connection_pool);

  // 在 user 表中检索 username, passwd 数据，这些数据从浏览器端输入
  if (mysql_query(mysql, "SELECT username, passwd FROM user")) {
    LOG_ERROR("SELECT Error:%s\n", mysql_error(mysql));
    exit(-1);
  }


  // 从表中检索完成的结果集
  MYSQL_RES* result = mysql_store_result(mysql);

  // 返回结果集中的列数
  int num_fields = mysql_num_fields(result);

  // 返回所有字段结构的数组
  MYSQL_FIELD* fields = mysql_fetch_fields(result);

  // 从结果集中获取下一行，将对应的用户和密码存入 map 中
  while (MYSQL_ROW row = mysql_fetch_row(result)) {
    string temp1(row[0]);
    string temp2(row[1]);

    users_[temp1] = temp2;
  }


}
          
// 对文件描述符设置非阻塞
int SetNonBlocking(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启 EPOLLONESHOT
void AddFd(int epollfd, int fd, bool one_shot, int trig_mode) {
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

// 从内核时间表中删除描述符
void Removefd(int epollfd, int fd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
  close(fd);
}

// 将事件重置为 EPOLLONESHOT
void ModFd(int epollfd, int fd, int ev, int trig_mode) {
  epoll_event event;
  event.data.fd = fd;

  if (1 == trig_mode) {
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
  } else {
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
  }

  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int HttpConn::user_count_ = 0;
int HttpConn::epoll_fd_ = -1;

// 关闭一个连接，关闭一个连接，客户总量减 1
void HttpConn::CloseConnection(bool real_close) {
  if (real_close && (sockfd_ != -1)) {
    LOG_INFO("close %d\n", sockfd_);
    Removefd(epoll_fd_, sockfd_);
    sockfd_ = -1;
    --user_count_;
  }
} 

// 初始化连接，外部调用初始化套接字地址
void HttpConn::Init(int sockfd, const sockaddr_in &addr, char* root, 
                    int trig_mode, int close_log, string user, string passwd,
                    string sql_name) {
  sockfd_ = sockfd;
  address_ = addr;

  AddFd(epoll_fd_, sockfd, true, trig_mode_);
  ++user_count_;

  // 当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或访问的文件
  // 中的内容完全为空

  doc_root_ = root;
  trig_mode_ = trig_mode;
  close_log_ = close_log;

  strcpy(sql_user_, user.c_str());
  strcpy(sql_passwd_, passwd.c_str());
  strcpy(sql_name_, sql_name.c_str());

  Init();
}

// 初始化新接收的连接
// check_state 默认为分析请求行状态
void HttpConn::Init() {
  mysql_ = NULL;
  bytes_to_send_ = 0;
  bytes_have_send_ = 0;
  check_state_ = CHECK_STATE_REQUESTLINE;
  linger_ = false;
  method_ = GET;
  url_ = 0;
  version_ = 0;
  content_length_ = 0;
  host_ = 0;
  start_line_ = 0;
  checked_idx_ = 0;
  read_idx_ = 0;
  write_idx_ = 0;
  cgi_ = 0;
  state_ = 0;
  timer_flag_ = 0;
  improv_ = 0;

  memset(read_buf_, '\0', kReadBufferSize);
  memset(write_buf_, '\0', kWriteBufferSize);
  memset(real_file_, '\0', kFileNameLen);
}


// 从状态机，用于分析出一行内容
// 返回值为行的读取状态，有 LINE_OK,LINE_BAD,LINE_OPEN 三种状态
HttpConn::LineStatus HttpConn::ParseLine() {
  char temp;
  for (; checked_idx_ < read_idx_; ++checked_idx_) {
    // 获得当前行中的第 idx 的字符
    temp = read_buf_[checked_idx_];
    if ('\r' == temp) {
      // '\r' 是当前读缓冲区中的第 checked_idx 个字符
      if ((checked_idx_ + 1) == read_idx_) {
        return LINE_OPEN;
      } else if (read_buf_[checked_idx_ + 1] == '\n') {
        read_buf_[checked_idx_++] = '\0';
        read_buf_[checked_idx_++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    } else if ('\n' == temp) { // "\r\n"是当前行的最后两个字符说明读到了完整的一行
      if (checked_idx_ > 1 && read_buf_[checked_idx_ - 1] == '\r') {
        read_buf_[checked_idx_ - 1] = '\0';
        read_buf_[checked_idx_++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_OPEN;
}

// 循环读取用户数据，直到无数据可读或对方关闭连接
// 非阻塞ET工作模式下，需要一次性将数据读完
bool HttpConn::ReadOnce() {
  if (read_idx_ >= kReadBufferSize) {
    // 这里不需要输出错误信息吗？
    return false;
  }

  int bytes_read = 0;

  // LT模式读取数据
  if (0 == trig_mode_) {
    bytes_read = recv(sockfd_, read_buf_ + read_idx_, 
                      kReadBufferSize - read_idx_, 0);
    read_idx_ += bytes_read;

    if (bytes_read <= 0) {
      // 这里也不需要往日志输出吗?
      return false;
    }

    return true;
  } else { // ET 模式读数据
    while (true) {
      bytes_read = recv(sockfd_, read_buf_ + read_idx_,
                        kReadBufferSize, 0);
      if (bytes_read == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          break;
        }
        return false;
      } else if (bytes_read == 0) {
        return false;
      }
      read_idx_ += bytes_read;
    }
    return true;
  }
}

// 解析 http 请求行，获取请求方法，目标 url 以及 http 版本号
HttpConn::HttpCode HttpConn::ParseRequestLine(char* text) {
  // 在 text 中搜索 “ \t” 中任意一个字符的第一个出现的位置
  // GET / HTTP1.1
  // *url_ = " ";
  url_ = strpbrk(text, " \t");
  if (!url_) {
    return BAD_REQUEST;
  }
  *url_++ = '\0'; // 此时url_ 指向 "/"
  char* method = text;
  if (strcasecmp(method, "GET") == 0) {
    method_ = GET;
  } else if (strcasecmp(method, "POST") == 0) {
    method_ = POST;
    cgi_ = 1;
  } else {
    return BAD_REQUEST;
  }

  url_ += strspn(url_, " \t"); // 此时 url_ 指向第二个空格
  version_ = strpbrk(url_, " \t");// version_ 也指向第二个空格
  if (!version_) {
    return BAD_REQUEST;
  }
  *version_++ = '\0';// 此时 version_ 指向 'H' 并将 url_ 限制在了中间
  version_ += strspn(version_, " \t");

  if (strcasecmp(version_, "HTTP/1.1") != 0) {
    return BAD_REQUEST;
  }
  if (strncasecmp(url_, "http://", 7) == 0) {
    url_ += 7;
    url_ = strchr(url_, '/');
  }

  if (strncasecmp(url_, "https://", 8) == 0) {
    url_ += 8;
    url_ = strchr(url_, '/');
  }

  if (!url_ || url_[0] != '/') {
    return BAD_REQUEST;
  }

  // 当 url为 '/' 时，显示判断界面
  if (strlen(url_) == 1) {
    strcat(url_, "judge.html");
  }

  check_state_ = CHECK_STATE_HEADER;
  return NO_REQUEST;
}

// 解析一个 HTTP 请求的头部信息
HttpConn::HttpCode HttpConn::ParseHeaders(char* text) {
  // 遇到一个空行 说明得到了一个正确的 HTTP 请求
  if (text[0] == '\0') {
    if (content_length_ != 0) {
      check_state_ = CHECK_STATE_CONTENT;
      return NO_REQUEST;
    }
    return GET_REQUEST;
  } else if (strncasecmp(text, "Connection:", 11) == 0) {
    text += 11;
    // 跳过制表符或空格
    text += strspn(text, " \t");
    if (strcasecmp(text, "keep-alive") == 0) {
      linger_ = true;
    }
  } else if (strncasecmp(text, "Content-length:", 15) == 0) {
    text += 15;
    text += strspn(text, " \t");

    //将请求内容给的长度写入相关字段
    content_length_ = atol(text);
  } else if (strncasecmp(text, "Host:", 5) == 0) {
    text += 5;
    text += strspn(text, " \t");
    host_ = text;
  } else {
    LOG_INFO("oop! unkonw header: %s", text);
  }
  return NO_REQUEST;
}

// 判断 HTTP 请求是否被完整读入
HttpConn::HttpCode HttpConn::ParseContent(char* text) {
  if (read_idx_ >= (content_length_ + checked_idx_)) {
    text[content_length_] = '\0';
    // POST 请求中最后为输入的用户名和密码
    string_ = text;
    return GET_REQUEST;
  }
  return NO_REQUEST;
}

HttpConn::HttpCode HttpConn::ProcessRead() {
  LineStatus line_status = LINE_OK;
  HttpCode ret = NO_REQUEST;
  char* text = 0;

  while ((check_state_ == CHECK_STATE_CONTENT && line_status == LINE_OK)
          || ((line_status = ParseLine()) == LINE_OK)) {
    text = GetLine();
    start_line_ = checked_idx_;
    LOG_INFO("%s", text);
    switch (check_state_) {
      case CHECK_STATE_REQUESTLINE : {
        ret = ParseRequestLine(text);
        if (ret == BAD_REQUEST) {
          return BAD_REQUEST;
        }
        break;
      }
      case CHECK_STATE_HEADER : {
        ret = ParseHeaders(text);
        if (ret == BAD_REQUEST) {
          return BAD_REQUEST;
        } else if (ret == GET_REQUEST) {
          return DoRequest();
        }
        break;
      }
      case CHECK_STATE_CONTENT : {
        ret = ParseContent(text);
        if (ret == GET_REQUEST) {
          return DoRequest();
        }
        line_status = LINE_OPEN;
        break;
      }
      default : {
        return INTERNAL_ERROR;
      }
    }
  }
  return NO_REQUEST;
}

// 处理请求
// 当得到一个完整的、正确的 HTTP 请求时，我们就分析目标文件的属性。如果目标文件存在、
// 对所有用户可读，且不是目录，则使用 mmap 将其映射到内存地址 file_address_处，并
// 告诉调用者获取文件成功
HttpConn::HttpCode HttpConn::DoRequest() {
  strcpy(real_file_, doc_root_);
  int len = strlen(doc_root_);
  // 找到最后一个 '/'
  const char* p = strrchr(url_, '/');

  // 处理 cgi
  if (cgi_ == 1 && (*(p + 1) == '2') || *(p + 1) == '3') {
    // 根据标志判断是登录检测还是注册检测
    char flag = url_[1];
    char* url_real = (char*)malloc(sizeof(char)* 200);
    strcpy(url_real, "/");
    strcat(url_real, url_ + 2);
    strncpy(real_file_ + len, url_real, kFileNameLen - len - 1);
    free(url_real);

    // 将用户名和密码提出出来
    // user=123&passwd=123
    char name[100], password[100];
    int i;
    for (i = 5; string_[i] != '&'; ++i) {
      name[i - 5] = string_[i];
    }
    name[i - 5] = '\0';

    int j = 0;
    for (i = i + 10; string_[i] != '\0'; ++i, ++j) {
      password[j] = string_[i];
    }
    password[j] = '\0'; 

    // p 指向的是字符，不是数字！
    if (*(p + 1) == '3') {
      // 如果是注册，先检测数据库中是否有重名
      // 没有重名，进行增加数据
      // 为什么不用 string ？
      char* sql_insert = (char*)malloc(sizeof(char) * 200);
      strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
      strcat(sql_insert, "'");
      strcat(sql_insert, name);
      strcat(sql_insert, "', '");
      strcat(sql_insert, password);
      strcat(sql_insert, "')");


      // 每次新的连接进入都要查询一遍mysql中的user表，可否用 redis 优化？
      if (users_.find(name) == users_.end()) {
        lock_.Lock();
        int res = mysql_query(mysql_, sql_insert);
        users_.insert(pair<string, string>(name, password));
        lock_.Unlock();

        if (!res) {
          strcpy(url_, "/log.html");
        } else {
          strcpy(url_, "/registerError.html");
        }
      } else {
        strcpy(url_, "/registerError.html");
      }
    } else if (*(p + 1) == '2') {
      // 如果是登录，直接判断
      // 若浏览器输入的用户名和密码在表中可以查找到，返回 1，否则返回 0
      if (users_.find(name) != users_.end() && users_[name] == password) {
        strcpy(url_, "/welcome.html");
      } else {
        strcpy(url_, "/logError.html");
      }
    }
  }
  // 文件不存在
  if (stat(real_file_, &file_stat_) < 0) {
    return NO_RESOURCE;
  }

  // 判断是否拥有读取权限
  if (!(file_stat_.st_mode & S_IROTH)) {
    return FORBIDDEN_REQUEST;
  }

  // 判断是否是根目录
  if (S_ISDIR(file_stat_.st_mode)) {
    return BAD_REQUEST;
  }

  // 将文件映射到内存中进行操作
  int fd = open(real_file_, O_RDONLY);
  file_address_ = (char*)mmap(0, file_stat_.st_size, PROT_READ, MAP_PRIVATE,
                    fd, 0);
  close(fd);
  return FILE_REQUEST;
}

void HttpConn::Unmap() {
  if (file_address_) {
    munmap(file_address_, file_stat_.st_size);
    file_address_ = 0;
  }
}

bool HttpConn::Write() {
  int temp = 0;

  if (bytes_to_send_ == 0) {
    ModFd(epoll_fd_, sockfd_, EPOLLIN, trig_mode_);
    Init();
    return true;
  }

  while (1) {
    temp = writev(sockfd_, iv_, iv_count_);

    if (temp < 0) {
      if (errno == EAGAIN) {
        ModFd(epoll_fd_, sockfd_, EPOLLOUT, trig_mode_);
        return true;
      }
      Unmap();
      return false;
    }

    bytes_have_send_ += temp;
    bytes_to_send_ -= temp;

    if (bytes_have_send_ >= iv_[0].iov_len) {
      iv_[0].iov_len = 0;
      iv_[1].iov_base = file_address_ + (bytes_have_send_ - write_idx_);
      iv_[1].iov_len = bytes_to_send_;
    } else {
      iv_[0].iov_base = write_buf_ + bytes_have_send_;
      iv_[0].iov_len = iv_[0].iov_len - bytes_have_send_;
    }

    if (bytes_to_send_ <= 0) {
      Unmap();
      ModFd(epoll_fd_, sockfd_, EPOLLIN, trig_mode_);

      if (linger_) {
        Init();
        return true;
      } else {
        return false;
      }
    }
  }
}


bool HttpConn::AddResponse(const char* format, ...) {
  if (write_idx_ >= kWriteBufferSize) {
    return false;
  }
  va_list arg_list;
  va_start(arg_list, format);
  int len = vsnprintf(write_buf_ + write_idx_, kWriteBufferSize - 1 - write_idx_,
                      format, arg_list);
  if (len >= (kWriteBufferSize - 1 - write_idx_)) {
    va_end(arg_list);
    return false;
  }
  write_idx_ += len;
  va_end(arg_list);


  // 为什么要在AddResponse 函数中输出 请求信息？
  LOG_INFO("response:%s", write_buf_);
  return true;
}

bool HttpConn::AddStatusLine(int status, const char* title) {
  AddResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConn::AddHeaders(int content_len) {
  return AddContentLength(content_len) && AddLinger() && AddBlankLine();
}

bool HttpConn::AddContentLength(int content_len) {
  return AddResponse("Content-Length:%d\r\n", content_len);
}

bool HttpConn::AddContentType() {
  return AddResponse("Content-Type:%s\r\n", "text/html");
}

bool HttpConn::AddLinger() {
  return AddResponse("Connection:%s\r\n",
                     (linger_ == true) ? "keep-alive" : "close");
}

bool HttpConn::AddBlankLine() {
  return AddResponse("%s", "\r\n");
}

bool HttpConn::AddContent(const char* content) {
  return AddResponse("%s", content);
}

bool HttpConn::ProcessWrite(HttpCode ret) {
  switch (ret) {
    case INTERNAL_ERROR : {
      AddStatusLine(500, kError500Title);
      AddHeaders(strlen(kError500Form));
      if (!AddContent(kError500Form)) {
        return false;
      }
      break;
    }
    case BAD_REQUEST : {
      AddStatusLine(404, kError404Title);
      AddHeaders(strlen(kError404Form));
      if (!AddContent(kError404Form)) {
        return false;
      }
      break;
    }
    case FORBIDDEN_REQUEST : {
      AddStatusLine(403, kError403Title);
      AddHeaders(strlen(kError403Form));
      if (!AddContent(kError403Form)) {
        return false;
      }
      break;
    }
    case FILE_REQUEST : {
      // 添加响应行
      AddStatusLine(200, kOk200Title);
      if (file_stat_.st_size != 0) {
        // 添加响应头
        AddHeaders(file_stat_.st_size);
        iv_[0].iov_base = write_buf_;
        iv_[0].iov_len = write_idx_;
        iv_[1].iov_base = file_address_;
        iv_[1].iov_len = file_stat_.st_size;
        iv_count_ = 2;
        bytes_to_send_ = write_idx_ + file_stat_.st_size;
        return true;
      } else {
        const char* ok_string = "<html><body></body></html>";
        AddHeaders(strlen(ok_string));
        if (!AddContent(ok_string)) {
          return false;
        }
      }
    }
    default : {
      return false;
    }
  }

  iv_[0].iov_base = write_buf_;
  iv_[0].iov_len = write_idx_;
  iv_count_ = 1;
  bytes_to_send_ = write_idx_;
  return true;
}

void HttpConn::Process() {
  HttpCode read_ret = ProcessRead();

  // 没有请求
  if (read_ret == NO_REQUEST) {
    ModFd(epoll_fd_, sockfd_, EPOLLIN, trig_mode_);
    return;
  }

  bool write_ret = ProcessWrite(read_ret);
  if (!write_ret) {
    CloseConnection();
  }
  ModFd(epoll_fd_, sockfd_, EPOLLOUT, trig_mode_);
}