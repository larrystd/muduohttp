#ifndef MUDUO_NET_TIMER_H_
#define MUDUO_NET_TIMER_H_

#include "muduo/base/Atomic.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Callbacks.h"

namespace muduo
{
namespace net
{

///
/// Internal class for timer event.
/// 定时事件对象
class Timer : noncopyable 
{
 public:
  Timer(TimerCallback cb, Timestamp when, double interval)
    : callback_(std::move(cb)),
      expiration_(interval),
      interval_(interval),
      repeat_(interval > 0.0),
      sequence_(s_numCreated_.incrementAndGet())  //s_numCreated_++作为sequence序号+1
  { }

  void run() const {
    callback_();  // 运行回调函数(定时任务)
  }

  Timestamp expiration() const { return expiration_; }
  bool repeat() const { return repeat_; }
  int64_t sequence() const { return sequence_; }

  void restart(Timestamp now);

  static int64_t numCreated() { return s_numCreated_.get(); }
    
 private:
  const TimerCallback callback_;  // 定时回调函数 typedef std::function<void()> TimerCallback
  Timestamp expiration_;  // 超时时间戳(时刻)
  const double interval_; // 定时间隔, 如果是0表示不重复
  const bool repeat_; // 是否重复
  const int64_t sequence_;  // 编号seq

  static AtomicInt64 s_numCreated_; // 静态原子变量， 作为全局序号变量
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_TIMER_H_
