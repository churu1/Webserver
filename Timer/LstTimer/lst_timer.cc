#include "lst_timer.h"
#include "../Utils/Utils.h"
#include "../../Http/http_conn.h"

SortTimerLst::SortTimerLst() {
  head_ = NULL;
  tail_ = NULL;
}

SortTimerLst::~SortTimerLst() {
  UtilTimer* tmp = head_;
  while (tmp) {
    head_ = tmp->next_;
    delete tmp;
    tmp = head_;
  }
}

void SortTimerLst::AddTimer(UtilTimer* timer) {
  // 结点不存在
  if (!timer) {
    return;
  }
  // 链表为空
  if (!head_) {
    head_ = tail_ = timer;
    return;
  }
  // 要插入的位置在头结点前面
  if (timer->expire_ < head_->expire_) {
    timer->next_ = head_;
    head_->prev_ = timer;
    head_ = timer;
    return; 
  }

  // 剩余情况
  AddTimer(timer, head_);
}

void SortTimerLst::AddTimer(UtilTimer* timer, UtilTimer* lst_head) {
  UtilTimer* prev = lst_head;
  UtilTimer* tmp = prev->next_;

  while (tmp) {
    if (timer->expire_ < tmp->expire_) {
      prev->next_ = timer;
      timer->next_ = tmp;
      tmp->prev_ = timer;
      timer->prev_ = prev;
      break;
    }
    prev = tmp;
    tmp = tmp->next_;
  }

  if (!tmp) {
    prev->next_ = timer;
    timer->prev_ = prev;
    timer->next_ = NULL;
    tail_ = timer;
  }
}

void SortTimerLst::AdjustTimer(UtilTimer* timer) {
  // 结点不存在
  if (!timer) {
    return;
  }
  UtilTimer* tmp = timer->next_;
  // 需要调整的已经是尾结点或者该定时器新的超时时间任然小于其下一个定时器的值，则不用调整
  if (!tmp || (timer->expire_ < tmp->expire_)) {
    return;
  }
  // 如果需要调整的结点是头节点
  if (timer == head_) {
    head_ = head_->next_;
    head_->prev_ = NULL;
    timer->next_ = NULL;
    AddTimer(timer, head_);
  } else {
    timer->prev_->next_ = timer->next_;
    timer->next_->prev_ = timer->prev_;
    AddTimer(timer, timer->next_);
  }
}

void SortTimerLst::DelTimer(UtilTimer* timer) {
  if (!timer) {
    return;
  }

  // 链表中只有一个结点
  if ((timer == head_) || (timer == tail_)) {
    delete timer;
    head_ = NULL;
    tail_ = NULL;
    return;
  }

  if (timer == head_) {
    head_ = head_->next_;
    head_->prev_ = NULL;
    delete timer;
    return;
  }

  if (timer == tail_) {
    tail_ = tail_->prev_;
    tail_->next_ = NULL;
    delete timer;
    return;
  }

  timer->prev_->next_ = timer->next_;
  timer->next_->prev_ = timer->prev_;
  delete timer;
}

void SortTimerLst::Tick() {
  if (!head_) {
    return;
  }

  time_t cur = time(NULL); // 获取当前时间
  UtilTimer* tmp = head_;
  while(tmp) {
    if (cur < tmp->expire_) {
      break;
    }
    tmp->cb_func(tmp->user_data_);
    head_ = head_->next_;
    if (head_) {
      head_->prev_ = NULL;
    }
    delete tmp;
    tmp = head_;
  }
}

class Utils;
void cb_func(ClientData* user_data) {
  if(epoll_ctl(Utils::epollfd_, EPOLL_CTL_DEL, user_data->sockfd, 0) != 0) {
    perror("epoll_ctl");
  }
  assert(user_data);
  close(user_data->sockfd);
  HttpConn::user_count_--;
}