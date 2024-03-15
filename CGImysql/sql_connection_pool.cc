#include "sql_connection_pool.h"
#include <pthread.h>

ConnectionPool::ConnectionPool() {
  cur_connection_ = 0;
  free_connection_ = 0;
}

ConnectionPool* ConnectionPool::GetInstance() {
  static ConnectionPool connection_pool;
  return &connection_pool;
}

// 构造初始化
void ConnectionPool::init(string url, string user, string password, 
                          string database_name,int port, int max_connection, 
                          int close_log) {
  url_ = url;
  port_ = port;
  user_ = user;
  passwd_ = password;
  datebase_name_ = database_name;
  close_log_ = close_log;

  for(int i = 0; i < max_connection; ++i) {
    // 初始化连接对象
    MYSQL* connection = NULL;
    connection = mysql_init(connection);

    if (connection == NULL) {
      // TODO 使用日志类相关函数将错误信息到日志文件中
    }

    // 连接到 mysql 服务器
    connection = mysql_real_connect(connection, url.c_str(), user.c_str(), 
                                    password.c_str(), database_name.c_str(), 
                                    port, NULL, 0);

    if(connection == NULL) {
      // TODO 使用日志类函数将错误信息输出到日志文件中
    }

    connection_list_.push_back(connection);
    ++free_connection_;
  }

}

MYSQL* ConnectionPool::GetConnection() {
  MYSQL* connection = NULL;

  if(0 == connection_list_.size())
    return NULL;
  
  reserve_.Wait();

  lock_.Lock();

  connection = connection_list_.front();
  connection_list_.pop_front();

  --free_connection_;
  ++cur_connection_;

  lock_.Unlock();
  return connection;
}

bool ConnectionPool::ReleaseConnection(MYSQL* connection) {
  if(NULL == connection) {
    return false;
  }

  lock_.Lock();

  connection_list_.push_back(connection);
  ++free_connection_;
  --cur_connection_;

  lock_.Unlock();

  reserve_.Post();
  return true;
}


void ConnectionPool::DestroyPool() {
  lock_.Lock();
  if(connection_list_.size() > 0) {
    list<MYSQL*>::iterator it;
    for(auto it : connection_list_) {
      mysql_close(it);
    }
    cur_connection_ = 0;
    free_connection_ = 0;
  }

  lock_.Unlock();
}

int ConnectionPool::GetFreeConnection() {
  return this->free_connection_;
}

ConnectionPool::~ConnectionPool() {
  DestroyPool();
}

ConnectionRAII::ConnectionRAII(MYSQL** sql, ConnectionPool* connection_pool) {
  *sql = connection_pool->GetConnection();

  connRAII = *sql;
  poolRAII = connection_pool;
}

ConnectionRAII::~ConnectionRAII() {
  poolRAII->ReleaseConnection(connRAII);
}