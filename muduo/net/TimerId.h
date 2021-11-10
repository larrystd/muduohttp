#ifndef MUDUO_NET_TIMERID_H_
#define MUDUO_NET_TIMERID_H_

#include "muduo/base/copyable.h"

namespace muduo
{
namespace net
{

class Timer;

/// An opaque identifier, for canceling Timer.
class TimerId : public muduo::copyable
{
 public:
  TimerId()
    : timer_(NULL),
      sequence_(0)
  {
  }

  TimerId(Timer* timer, int64_t seq)  // 用timer指针和seq来构造TimerId
    : timer_(timer),
      sequence_(seq)
  {
  }

  friend class TimerQueue;  // TimerQueue可以调用TimerId的private成员

 private:
  // 维护两个变量, Timer*和seq
  Timer* timer_;
  int64_t sequence_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_TIMERID_H_
