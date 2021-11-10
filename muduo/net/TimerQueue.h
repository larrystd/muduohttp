// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMERQUEUE_H_
#define MUDUO_NET_TIMERQUEUE_H_

#include <set>
#include <vector>

#include "muduo/base/Mutex.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/Channel.h"

namespace muduo
{
namespace net
{

class EventLoop;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
/// 定时器队列
///
class TimerQueue : noncopyable
{
 public:
  explicit TimerQueue(EventLoop* loop);   // 只能用eventloop指针显示构造
  ~TimerQueue();

  TimerId addTimer(TimerCallback cb, Timestamp when, double interval);  // 将cb, when, interval增加定时任务

  void cancel(TimerId timerId); // 根据timerId取消定时任务

 private:

  typedef std::pair<Timestamp, Timer*> Entry; // Entry是pair<Timestamp, Timer*>, 时间戳和Timer*指针

  typedef std::set<Entry> TimerList;  // 用set保存TimerList定时任务列表, set默认定时任务根据时间戳从小到大排列

  typedef std::pair<Timer*, int64_t> ActiveTimer; // 活跃的定时任务, pair<Timer*, int64_t>, Timer指针。可以通过timerId构造
  typedef std::set<ActiveTimer> ActiveTimerSet; // 活跃定时任务的集合, 根据Timer*地址从小到大排列

  void addTimerInLoop(Timer* timer);  // 将Timer加入到loop
  void cancelInLoop(TimerId timerId);
  // called when timerfd alarms
  void handleRead();  // timerfd可读的回调函数
  // move out all expired timers
  /// 得到超期的定时事件
  std::vector<Entry> getExpired(Timestamp now); // 根据时间找到超时的定时事件, 返回列表
  void reset(const std::vector<Entry>& expired, Timestamp now);

  /// 插入timer
  bool insert(Timer* timer);

  EventLoop* loop_; // 定时器所在的loop对象
  const int timerfd_; // 用于定时的timerfd
  Channel timerfdChannel_;  // 定时的监听通道
  TimerList timers_;  // 时间戳, 定时任务的列表, 根据时间戳排序

  // for cancel()
  ActiveTimerSet activeTimers_;
  bool callingExpiredTimers_; /* atomic */
  ActiveTimerSet cancelingTimers_;
};

}  // namespace net
}  // namespace muduo
#endif  // MUDUO_NET_TIMERQUEUE_H_
