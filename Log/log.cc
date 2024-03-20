#include "log.h"
#include <string.h>
#include <time.h>

Log::Log() {
  count_ = 0;
  is_async_ = false;
}

Log::~Log() {
  if (fp_ != NULL) {
    fclose(fp_);
  }
}

// 异步写入则需要设置阻塞队列的长度，同步则不需要设置
bool Log::Init(const char* file_name, int close_log, int log_buf_size,
               int split_lines, int max_queue_size) {
  // 如果设置了 max_queue_size， 则设置为异步
  if (max_queue_size >= 1) {
    is_async_ = true;
    log_queue_ = new BlockQueue<string>(max_queue_size);
    pthread_t tid;
    // FlushLogThread为回调函数，这里表示创建线程的异步写日志
    pthread_create(&tid, NULL, FlushLogThread, NULL);
  }

  close_log_ = close_log;
  log_buf_size_ = log_buf_size;
  buf_ = new char[log_buf_size_];
  memset(buf_, '\0', log_buf_size_);
  split_lines_ = split_lines;

  time_t t = time(NULL);
  struct tm* sys_tm = localtime(&t);
  struct tm my_tm = *sys_tm;

  // 找到 file_name 中最后一个 / 所在的位置，用于划分 目录名和日志文件名
  // ./ServerLog
  const char* p = strrchr(file_name, '/');
  char log_full_name[256] = {0}; // 存储日志文件的系统路径
  if (p == NULL) {
    // YYYY_MM_DD_file_name
    snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900,
             my_tm.tm_mon + 1, my_tm.tm_mday, file_name); 
  } else {
    strcpy(log_name_, p + 1); // ServerLog
    strncpy(dir_name_, file_name, p - file_name + 1); // ./
    snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", // ./YYYY_MM_DD_ServerLog
             dir_name_, my_tm.tm_year + 1900,
             my_tm.tm_mon + 1, my_tm.tm_mday, log_name_);
  }

  today_ = my_tm.tm_mday;

  // 以追加的形式打开文件，如果没有则创建
  fp_ = fopen(log_full_name, "a");
  if (fp_ == NULL) {
    return false;
  }

  return true;
}

void Log::WriteLog(int level, const char* format, ...) {
  struct timeval now = {0, 0};
  gettimeofday(&now, NULL);
  time_t t = now.tv_sec;
  struct tm* sys_tm = localtime(&t);
  struct tm my_tm = *sys_tm;
  char s[16] = {0};
  switch(level) {
    case 0 : {
      strcpy(s, "[debug]:");
      break;
    }
    case 1 : {
      strcpy(s, "[info]:");
      break;
    }
    case 2 : {
      strcpy(s, "[warn]:");
      break;
    }
    case 3 : {
      strcpy(s, "[erro]:");
      break;
    }
    default: {
      strcpy(s, "[info]:");
    }
  }

  // 写入一个log，对m_count++
  mutex_.Lock();
  count_++;

  // 如果当前日期与记录日期不同或单个日志文件达到了设定的行数，要重新创建一个日志文件
  if (today_ != my_tm.tm_mday || count_ % split_lines_ == 0) {
    char new_log[256] = {0};
    fflush(fp_);
    fclose(fp_);
    char tail[16] = {0};

    snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1,
            my_tm.tm_mday);
    if (today_ != my_tm.tm_mday) {
      snprintf(new_log, 255, "%s%s%s", dir_name_, tail, log_name_);
      today_ = my_tm.tm_mday;
      count_ = 0;
    } else {
      snprintf(new_log, 255, "%s%s%s.%lld", dir_name_, tail,log_name_,
               count_ / split_lines_); 
    }
    fp_ = fopen(new_log, "a");
  }

  mutex_.Unlock();

  // 可以接收可变数量参数的类型 va_list
  va_list va_lst;
  // 初始化 va_list 对象， 第二参数为最后一个非可变参数的参数名
  va_start(va_lst, format); 

  string log_str;
  mutex_.Lock();

  int n = snprintf(buf_, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s",
                   my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                   my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_sec, s);

  // 类似于 snprintf, 将 valst 对象中的数据按照 format 格式写入 buf_
  int m = vsnprintf(buf_ + n, log_buf_size_ - n - 1, format, va_lst);
  buf_[n + m] = '\n';
  buf_[n + m + 1] = '\0';
  log_str = buf_;

  mutex_.Unlock();
  if (is_async_ && log_queue_->IsFull()) {
    log_queue_->Push(log_str);
  } else {
    mutex_.Lock();
    fputs(log_str.c_str(), fp_);
    mutex_.Unlock();
  }

  va_end(va_lst); // 清除 va_list对象
}

void Log::Flush(void) {
  mutex_.Lock();
  fflush(fp_);
  mutex_.Unlock();
}