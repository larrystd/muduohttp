#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"

#include <functional>
#include <map>

#include <stdio.h>
#include <unistd.h>
#include <sys/timerfd.h>

using namespace muduo;
using namespace muduo::net;

void print(const char* msg)
{
  static std::map<const char*, Timestamp> lasts;
  Timestamp& last = lasts[msg];
  Timestamp now = Timestamp::now();
  printf("%s tid %d %s delay %f\n", now.toString().c_str(), CurrentThread::tid(),
         msg, timeDifference(now, last));
  last = now;
}

namespace muduo
{
namespace net
{
namespace detail
{
int createTimerfd();
void readTimerfd(int timerfd, Timestamp now);
}
}
}

// Use relative time, immunized to wall clock changes.
class PeriodicTimer
{
 public:
  PeriodicTimer(EventLoop* loop, double interval, const TimerCallback& cb)
    : loop_(loop),
      timerfd_(muduo::net::detail::createTimerfd()),  // 定时器fd
      timerfdChannel_(loop, timerfd_),  // 定时器channel
      interval_(interval),   // 间隔
      cb_(cb) // 定时任务
  { 
    // 设置定时fd channel的读回调函数
    timerfdChannel_.setReadCallback(
        std::bind(&PeriodicTimer::handleRead, this));
    timerfdChannel_.enableReading();
  }

  void start()
  {
    struct itimerspec spec;
    memZero(&spec, sizeof spec);
    spec.it_interval = toTimeSpec(interval_);
    spec.it_value = spec.it_interval;
    // 设置定时
    int ret = ::timerfd_settime(timerfd_, 0 /* relative timer */, &spec, NULL);
    if (ret)
    {
      LOG_SYSERR << "timerfd_settime()";
    }
  }

  ~PeriodicTimer()
  {
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
  }

 private:
  void handleRead()
  {
    loop_->assertInLoopThread();
    muduo::net::detail::readTimerfd(timerfd_, Timestamp::now());
    // 执行cb
    if (cb_)
      cb_();
  }

  static struct timespec toTimeSpec(double seconds)
  {
    struct timespec ts;
    memZero(&ts, sizeof ts);
    const int64_t kNanoSecondsPerSecond = 1000000000;
    const int kMinInterval = 100000;
    int64_t nanoseconds = static_cast<int64_t>(seconds * kNanoSecondsPerSecond);
    if (nanoseconds < kMinInterval)
      nanoseconds = kMinInterval;
    ts.tv_sec = static_cast<time_t>(nanoseconds / kNanoSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(nanoseconds % kNanoSecondsPerSecond);
    return ts;
  }

  EventLoop* loop_;
  const int timerfd_;
  Channel timerfdChannel_;
  const double interval_; // in seconds
  TimerCallback cb_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid()
           << " Try adjusting the wall clock, see what happens.";
  EventLoop loop;
  PeriodicTimer timer(&loop, 1, std::bind(print, "PeriodicTimer")); // 注册定时fd到loop并且可读
  timer.start();  // 设置timefd 1s中间隔化触发

  loop.runEvery(1, std::bind(print, "EventLoop::runEvery"));  // 使用loop的runEvery来执行定时任务, 这一步和上面的是等价的
  loop.loop();
}
