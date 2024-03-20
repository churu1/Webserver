#include "config.h"
int main(int argc, char* argv[]) {
   // 设置数据库信息: 登录名,密码和库名
   string usr = "root"; 
   string passwd = "ct281314_";
   string database_name = "churudb";

   // 创建配置文件对象并进行命令解析
   Config config;
   config.ParseArg(argc, argv);

   WebServer server;

   // 初始化
   server.Init(config.port_, usr, passwd, database_name, config.log_write_,
               config.opt_linger_, config.trig_mode_, config.sql_pool_num_,
               config.thread_pool_num_, config.close_log_, config.acotr_model_);

   // 开启日志
   server.LogWrite();

   // 开启数据库
   server.SqlPool();

   // 开启线程池
   server.CreateThreadPool();
   
   // 选择触发模式
   server.TrigMode();

   // 监听事件
   server.EventListen();

   // 开始事件循环
   server.EventLoop();

   return 0;
}