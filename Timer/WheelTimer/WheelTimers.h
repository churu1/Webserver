#ifndef TIME_WHEEL_TIMER
#define TIME_WHEEL_TIMER
#include <time.h>
#include <netinet/in.h>
#include <stdio.h>

#define BUFFER_SIZE 64
class TwTimer;

// 绑定 socket 和定时器
struct ClientData {
  sockaddr_in address;
  int sockfd;
  char buf[BUFFER_SIZE];
  TwTimer* timer;
};

// 定时器类
class TwTimer{
 public:
  TwTimer(int rot, int ts) : next_(NULL),
                             prev_(NULL),
                             rotation_(rot),
                             time_slot_(ts){}

 public:
  // 记录定时器在时间轮转多少圈后生效  
  int rotation_;
  // 记录定时器属于时间轮上的哪个槽（对应的链表）
  int time_slot_;
  // 定时器的回调函数
  void (*cb_func)(ClientData*);
  // 用户数据
  ClientData* user_data_;
  // 指向下一个定时器
  TwTimer* next_;
  // 指向前一个定时器
  TwTimer* prev_;
};

class TimeWheel{
 public:
  TimeWheel() : cur_slot_(0) {
    for (int i = 0; i < kN_; ++i) {
      slots_[i] = NULL; // 初始化每个槽点的头节点
    }
  }

  ~TimeWheel() {
    // 遍历每个槽，并销毁其中的定时器
    for (int i = 0; i < kN_; ++i) {
      TwTimer* tmp = slots_[i];
      while (tmp) {
        slots_[i] = tmp->next_;
        delete tmp;
        tmp = slots_[i];
      }
    }
  }

  // 根据定时值创建一个定时器，并把它插入合适的槽中
  TwTimer* AddTimer(int timeout) {
    if (timeout < 0) {
      return NULL;
    }

    int ticks = 0;
    /*
    下面根据待插入的定时器的超时时间计算它将在时间轮转动多少个滴答后被触发，并将该滴答数
    存储在变量 ticks 中，如果待插入定时器的超时时间值小于时间轮的槽间隔 kSI_, 则将
    ticks 向上取整为1，否则就将 ticks 向下取整为 timeout / SI
    */

    if (timeout < kSI_) {
      ticks = 1;
    } else {
      ticks = timeout / kSI_;
    }
    // 计算待插入的定时器在时间轮转动多少圈后被触发
    int rotation = ticks / kN_;
    // 计算待插入的定时器应该插入哪个槽中
    int ts = (cur_slot_ + (ticks % kN_)) % kN_;
    // 创建新的定时器，它在时间轮转动 rotation 圈后被触发，且位于第 ts 个槽上
    TwTimer* timer = new TwTimer(rotation, ts);
    // 如果第 ts 个槽中尚无任何定时器，则把新建的定时器插入其中，并将该定时器
    // 设置为该槽的头节点
    if (!slots_[ts]) {
      printf("add timer, totation is%d, ts is %d, cur_slot_ is%d\n", rotation,
             ts, cur_slot_);
      slots_[ts] = timer;
    } else { // 否则，将定时器插入第 ts 个槽中 头插法
      timer->next_ = slots_[ts];
      slots_[ts]->prev_ = timer;
      slots_[ts] = timer;
    }
    return timer;
  }

  // 删除目标定时器
  void DelTimer(TwTimer* timer) {
    if (!timer) {
      return;
    }
    int ts = timer->time_slot_;
    // slots_[ts] 是目标定时器所在槽的头结点。如果目标定时器就是该头结点
    // 则需要重新设置第 ts 个槽的头结点
    if (timer == slots_[ts]) {
      slots_[ts] = slots_[ts]->next_;
      if (slots_[ts]) {
        slots_[ts]->prev_ = NULL;
      }
      delete timer;
    } else {
      timer->prev_->next_ = timer->next_;
      if (timer->next_) {
        timer->next_->prev_ = timer->prev_;
      }
      delete timer;
    }
  }

  void Tick() {
    TwTimer* tmp = slots_[cur_slot_]; // 取得时间轮上当前槽的头结点
    printf("current slot is%d\n", cur_slot_);
    while (tmp) {
      printf("tick the time once\n");
      // 如果定时器的 rotation 值大于0，则它在这一轮中不起作用
      if (tmp->rotation_ > 0) {
        --tmp->rotation_;
        tmp = tmp->next_;
      } else {
        // 否则， 说明定时器已经到期，于是执行定时任务，然后删除该定时器
        tmp->cb_func(tmp->user_data_);
        if (tmp == slots_[cur_slot_]) { // 是头结点
          printf("delete header in cur_slot\n");
          slots_[cur_slot_] = tmp->next_;
          delete tmp;

          if (slots_[cur_slot_]) {
            slots_[cur_slot_]->prev_ = NULL;
          }
          tmp = slots_[cur_slot_];
        } else { // 不是头结点
          tmp->prev_->next_ = tmp->next_;
          if (tmp->next_) {
            tmp->next_->prev_ = tmp->prev_;
          }
          TwTimer* tmp2 = tmp->next_;
          delete tmp;
          tmp = tmp2;
        }
      }
    }
    // 更新时间轮的当前槽，以反映时间轮的转动
    cur_slot_ = ++cur_slot_ % kN_;
  }


 private:
  // 时间轮上槽的数目
  static const int kN_ = 60;
  // 每1 s时间轮转动一次，即槽间间隔为1 s
  static const int kSI_ = 1;
  // 时间轮的槽，其中每个元素指向一个定时器链表，链表无序
  TwTimer* slots_[kN_];
  // 时间轮的当前槽
  int cur_slot_; 

};

#endif