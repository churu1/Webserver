#ifndef WEBSERVER_CONNECTION_POOL_H
#define WEBSERVER_CONNECTION_POOL_H

#include <stdio.h>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include <list>
#include "../Lock/locker.h"
#include "../Log/log.h"

using std::string;
using std::list;

class ConnectionPool{
 public:

  /// @brief 获取数据库连接
  /// @return 数据库连接
  MYSQL* GetConnection(); 

  /// @brief 释放数据库连接
  /// @param connection 某个数据库连接
  /// @return 成功时返回true
  bool ReleaseConnection(MYSQL* connection);

  /// @brief  获取连接池的单例对象
  /// @return 返回创建的单例对象
  static ConnectionPool* GetInstance();

  /// @brief 获取空闲连接数量
  /// @return 返回空闲连接数量
  int GetFreeConnection();

  /// @brief 销毁线程池中的所有连接
  void DestroyPool();

  /// @brief 初始化线程池相关属性
  /// @param url 数据库的主机地址
  /// @param user 登录数据库用户名
  /// @param password 登录数据库密码
  /// @param datebase_name 使用的数据库名称
  /// @param port 数据库端口
  /// @param max_connection 最大连接数量 
  /// @param close_log // 日志开关
  void init(string url, string user, string password, string datebase_name,
            int port, int max_connection, int close_log);

 private:
  ConnectionPool();
  ~ConnectionPool();
  ConnectionPool(const ConnectionPool& other);
  ConnectionPool& operator=(const ConnectionPool& other);

  int max_connection_;
  int cur_connection_;
  int free_connection_; // 当前空闲的连接数
  Locker lock_;
  list<MYSQL*> connection_list_;
  Sem reserve_;


 public:
  string url_;
  string port_;
  string user_;
  string passwd_;
  string datebase_name_;
  int close_log_;
};


/// @brief 对数据库连接池的一层封装,用于实现: 获取连接对象时初始化
class ConnectionRAII {
 public:
  ConnectionRAII(MYSQL** connection, ConnectionPool* ConnectionPool);
  ~ConnectionRAII();

 private:
  MYSQL* connRAII;
  ConnectionPool* poolRAII;
};

#endif