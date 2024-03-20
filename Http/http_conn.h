#ifndef WEBSERVER_HTTP_CONNECTION_H
#define WEBSERVER_HTTP_CONNECTION_H

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
#include <map>

#include "../Lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../Timer/LstTimer/lst_timer.h"
#include "../Log/log.h"

using std::map;
using std::pair;
using std::string;

class HttpConn {
 public:
  enum Method {
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATH
  };

  enum CheckState {
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONTENT
  };

  enum HttpCode {
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSE_CONNECTION
  };

  enum LineStatus {
    LINE_OK = 0,
    LINE_BAD,
    LINE_OPEN
  };

  static const int kFileNameLen = 200;
  static const int kReadBufferSize = 2048;
  static const int kWriteBufferSize = 1024;

 public:
  HttpConn() {}
  ~HttpConn() {}

 public:
  void Init(int sockfd, const sockaddr_in& addr, char*, int, int, string user,
            string passwd, string sql_name);
  void CloseConnection(bool real_close = true);
  void Process();
  bool ReadOnce();
  bool Write();
  sockaddr_in* GetAddress() {
    return &address_;
  }
  void InitMySqlResult(ConnectionPool* connection_pool);
  int timer_flag_;
  int improv_; // ?



 private:
  void Init();
  HttpCode ProcessRead();
  bool ProcessWrite(HttpCode ret);
  HttpCode ParseRequestLine(char* text);
  HttpCode ParseHeaders(char* text);
  HttpCode ParseContent(char* text);
  HttpCode DoRequest();
  char* GetLine() {return read_buf_ + start_line_;}
  LineStatus ParseLine();
  void Unmap();
  bool AddResponse(const char* format, ...);
  bool AddContent(const char* content);
  bool AddStatusLine(int status, const char* title);
  bool AddHeaders(int content_length);
  bool AddContentType();
  bool AddContentLength(int content_length);
  bool AddLinger(); // 设置close状态
  bool AddBlankLine();
 
 public:
  static int epoll_fd_;
  static int user_count_;
  MYSQL* mysql_;
  int state_; // 读为0，写为1

 private:
  int sockfd_;
  sockaddr_in address_;
  char read_buf_[kReadBufferSize];
  long read_idx_;
  long checked_idx_;
  int start_line_;
  char write_buf_[kWriteBufferSize];
  int write_idx_;
  CheckState check_state_;
  Method method_;
  char real_file_[kFileNameLen];
  char* url_;
  char* version_;
  char* host_;
  long content_length_;
  bool linger_;
  char* file_address_;
  struct stat file_stat_; // ?
  struct iovec iv_[2]; // ?
  int iv_count_;
  int cgi_; // 是否启用的 POST
  char* string_; // 存储请求头数据
  int bytes_to_send_;
  int bytes_have_send_;
  char* doc_root_;

  //map<string, string> users_;
  int trig_mode_;
  int close_log_;

  char sql_user_[100];
  char sql_passwd_[100];
  char sql_name_[100];
};




#endif