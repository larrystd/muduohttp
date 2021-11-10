#include "muduo/net/Timer.h"

using namespace muduo;
using namespace muduo::net;

void Timer::restart(Timestamp now) {
  if (repeat_)  // 如果定时器是重复的
  {
    expiration_ = addTime(now, interval_);  // 当前时间加上事件间隔 计算新的超时时间
  }
  else {
    expiration_ = Timestamp::invalid();
  }
}
