#include "config.h"
#include <string>

using std::string;

int mian(int argc, char* argv[]) {
   // 设置数据库信息: 登录名,密码和库名
   string usr = "root"; 
   string passwd = "ct281314_";
   string database_name = "churudb";

   // 创建配置文件对象并进行命令解析
   Config config;
   config.ParseArg(argc, argv);
}