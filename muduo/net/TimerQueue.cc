#ifndef __STDC_LIMIT_MACROS // 允许C++程序使用C99标准中指定的 stdint.h 宏
#define __STDC_LIMIT_MACROS
#endif

#include "muduo/net/TimerQueue.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"

namespace muduo
{
namespace net
{
namespace detail
{

int createTimerfd() // 创建timefd
{
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK | TFD_CLOEXEC); // 创建timerfd
  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}


struct timespec howMuchTimeFromNow(Timestamp when)  // when在now之后, 返回timespec时间间隔
{
  int64_t microseconds = when.microSecondsSinceEpoch()  
                         - Timestamp::now().microSecondsSinceEpoch(); // 用毫秒表示的now到when的毫秒数
  if (microseconds < 100)
  {
    microseconds = 100;
  }
  struct timespec ts; // 将毫秒microseconds封装成timespec
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  return ts;
}

void readTimerfd(int timerfd, Timestamp now)  // 读timerfd
{
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany);  // 读取timerfd, 返回到howmany中
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
  if (n != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
}

void resetTimerfd(int timerfd, Timestamp expiration)   // 重新设置timerfd的过期时间
{
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  memZero(&newValue, sizeof newValue);
  memZero(&oldValue, sizeof oldValue);

  newValue.it_value = howMuchTimeFromNow(expiration); // expiration距离现在的事件
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);  // 设置timerfd为新值, timerfd将在时间过去newValue后发出可读通知
  if (ret)
  {
    LOG_SYSERR << "timerfd_settime()";
  }
}

}  // namespace detail
}  // namespace net
}  // namespace muduo

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop* loop) // 使用loop指针构造TimerQueue, 一个eventloop会对应TimerQueue对象
  : loop_(loop),
    timerfd_(createTimerfd()),  // 创建timerfd_
    timerfdChannel_(loop, timerfd_),  // 通过loop, timerfd创建timerfdChannel_(channel对象)
    timers_(),  // 定时对象
    callingExpiredTimers_(false)
{
  timerfdChannel_.setReadCallback(
      std::bind(&TimerQueue::handleRead, this));  // timerfdChannel_设置读回调函数
  timerfdChannel_.enableReading();  // timerfdChannel_可读
}

TimerQueue::~TimerQueue()
{
  timerfdChannel_.disableAll(); // timerfdChannel的disable和移除
  timerfdChannel_.remove();

  ::close(timerfd_);  // 关闭timerfd_
  // do not remove channel, since we're in EventLoop::dtor();
  for (const Entry& timer : timers_)  // 析构 timer的第二个成员Timer*
  {
    delete timer.second;
  }
}


TimerId TimerQueue::addTimer(TimerCallback cb,
                             Timestamp when,
                             double interval) // 添加某个定时任务到定时集合
{

  Timer* timer = new Timer(std::move(cb), when, interval);  // 根据cb, when, interval构造定时器对象Timer
  loop_->runInLoop(
      std::bind(&TimerQueue::addTimerInLoop, this, timer)); // &TimerQueue::addTimerInLoop在loop所在的线程中执行, 将timer加入到定时器集合中
  
  return TimerId(timer, timer->sequence()); // 返回TimerId
}

void TimerQueue::cancel(TimerId timerId)
{
  loop_->runInLoop(
      std::bind(&TimerQueue::cancelInLoop, this, timerId)); // 在loop所在的线程中执行&TimerQueue::cancelInLoop, 即从定时集合删除timerId, 析构对应的timer
}


void TimerQueue::addTimerInLoop(Timer* timer)
{
  loop_->assertInLoopThread();
  bool earliestChanged = insert(timer); // 将timer插入到定时集合中, 返回定时集合最早时间是否改变

  if (earliestChanged)  // 如果改变了, 需要用现在定时器最早超时时间修改到timerfd中
  {
    resetTimerfd(timerfd_, timer->expiration());
  }
}

void TimerQueue::cancelInLoop(TimerId timerId)  // 根据timerId
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  /// 根据timerId构造timer对象(其实是std::pair<Timer*, int64_t>)

  ActiveTimer timer(timerId.timer_, timerId.sequence_); // timerId含有timer_和seq两个成员, 组成ActiveTimer对象
  ActiveTimerSet::iterator it = activeTimers_.find(timer);  // 从activeTimers_集合寻找timer
  if (it != activeTimers_.end())  // 找到
  {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));  // 从timers_集合中删除定时任务对象
     (void)n;
    assert(n == 1);
    delete it->first; // 析构timer对象
    activeTimers_.erase(it);  // 从activeTimers_擦除timer
  }
  else if (callingExpiredTimers_)
  {
    cancelingTimers_.insert(timer);
  }
  assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead() // timerfd可读(时间到了)调用之, 一般是在eventloop中调用之
{
  loop_->assertInLoopThread();  // 这个函数实际上在eventloop的loop循环中被loop所属的线程调用

  Timestamp now(Timestamp::now());  // 当前时间
  readTimerfd(timerfd_, now); // now时刻读取timerfd活跃内容

  std::vector<Entry> expired = getExpired(now); // 找到比now早的定时序列到vector expired中

  callingExpiredTimers_ = true;
  cancelingTimers_.clear();
  // safe to callback outside critical section
  /// 执行超期定时序列的任务
  for (const Entry& it : expired)
  {
    it.second->run(); // 立刻运行所有定时任务(子线程执行)
  }
  callingExpiredTimers_ = false;
  reset(expired, now);  // 根据now时刻处理expired, 以及重置timerfd
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)  // now时刻超时的定时器
{
  assert(timers_.size() == activeTimers_.size());

  std::vector<Entry> expired; // 超时的定时任务列表

  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX)); // 当前时间戳, Timer构造的sentry

  TimerList::iterator end = timers_.lower_bound(sentry);  // 从定时集合找到查找第一个大于或等于sentry的数字， 返回迭代器
  assert(end == timers_.end() || now < end->first); // end结果未越界
  std::copy(timers_.begin(), end, back_inserter(expired));  // 将[begin, end)区间的数据倒序拷贝到expired。expired列表现在时间戳是从大到小排列的, 越靠前越接近Now

  timers_.erase(timers_.begin(), end); // timers_擦除[begin(),end) 的元素 

  for (const Entry& it : expired) // 从activeTimers_删除超时的定时任务
  {
    ActiveTimer timer(it.second, it.second->sequence());  // 拷贝构造timer
    size_t n = activeTimers_.erase(timer);  
    (void)n;
    assert(n == 1);
  }
  assert(timers_.size() == activeTimers_.size());
  return expired; // 返回超时的任务列表
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)  // 修改expired的定时任务
{
  Timestamp nextExpire;

  for (const Entry& it : expired) // 遍历超时的定时对象
  {
    ActiveTimer timer(it.second, it.second->sequence());  // 构造活跃的定时对象
    if (it.second->repeat()
        && cancelingTimers_.find(timer) == cancelingTimers_.end())  // 如果该定时对象是要重读执行的
    {
      it.second->restart(now);  // 重置定时时间, 加入到定时集合中
      insert(it.second);
    }
    else
    {
      delete it.second; // 直接析构timer
    }
  }
  if (!timers_.empty()) // 得到下一个超时时间, 即timers的第一个元素的超时时间
  {
    nextExpire = timers_.begin()->second->expiration();
  }

  if (nextExpire.valid())
  {
    resetTimerfd(timerfd_, nextExpire); // 用下一个超时时间重置timerfd
  }
}


/// 在定时集合activeTimers_中增加timer
bool TimerQueue::insert(Timer* timer) // 将timer插入到
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  bool earliestChanged = false;
  Timestamp when = timer->expiration(); // timer的超时时间
  TimerList::iterator it = timers_.begin(); // 取出定时集合std::set<Entry> TimerList的第一个元素(时间戳最早)

  if (it == timers_.end() || when < it->first)  // 如果timer超时时间小于定时集合最早时间戳, 最早时间戳应该改为timer->expiration()
  {
    earliestChanged = true;
  }

  {
    std::pair<TimerList::iterator, bool> result
      = timers_.insert(Entry(when, timer)); // 将timer超时时间, timer插入到定时集合中
    assert(result.second);  // result.second应不为空
    (void)result;
  }

  {
    std::pair<ActiveTimerSet::iterator, bool> result
      = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));  // timer*. timer的序号加入到activeTimers_集合中
    (void)result;
    assert(result.second);
  }

  assert(timers_.size() == activeTimers_.size()); // timers_大小应该和activeTimers_应该一样
  return earliestChanged; // 是否需要修改最早时间戳
}

