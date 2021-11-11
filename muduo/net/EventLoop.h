#ifndef MUDUO_NET_EVENTLOOP_H_
#define MUDUO_NET_EVENTLOOP_H_

#include <atomic>
#include <functional>
#include <vector>

#include <boost/any.hpp>

#include "muduo/base/Mutex.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/TimerId.h"

namespace muduo
{
namespace net
{

class Channel;
class Poller;
class TimerQueue;

///
/// Reactor, at most one per thread.
/// one thread on loop一个网络IO线程一个循环，循环里面做着一样的事情。
// 每个线程有的eventloop对象, 核心是自己的poller_, 自己的任务队列pendingFunctors_, 它就干这两件事。一个poller触发的事件, 一个任务队列的事件
// 线程的eventloop对象的指针可以从loops_获得, 主函数只操作loops_, 就是将任务放到这个loops的pendingFunctors_, 使用这个loops_的wakefd唤醒负责loops_的线程, 主线程就可以走了。
// 负责loops_的线程被唤醒后会执行自己的pendingFunctors_(其中包括调用poller epoll_ctl fd, 使用std将函数和参数绑定后所有函数均可视为std::function<void()>)
// 子线程继续循环eventloop, 这时候如果自己负责的fd可读就可以处理了。

// 一个连接会有一个channel, 和TcpConnection, channel偏底层, Connection用来封装上层, 实际是还是poller直接触发channel调Connection的用户自定义函数。poller有连接到channel(进一步线程有链接到channel), poller活跃意味着所属的channel出现活跃
// Connection对象在堆中创建, 具有std::unique_ptr<Channel>, 它会把用户定义函数以及非用户定义函数(比如read)用参数注册到Channel中。poll活跃, 线程会调用handleEventWithGuard自动处理Channel中的回调函数
// 通信过程实际上只有eventloop, poller, channel三者交互, 执行channel的回调函数是TcpConnection定义的。
// 
// Acceptor, Connection是建立连接时调用的, TcpServer和TcpConnection起到调度线程和协调从建立连接的Acceptor到通信Channel的功能, 算是主线程执行的工作。
///
/// This is an interface class, so don't expose too much details.
class EventLoop : noncopyable
{
 public:
  typedef std::function<void()> Functor;

  EventLoop();
  ~EventLoop();  // force out-line dtor, for std::unique_ptr members.

  /// Loops forever.
  ///
  /// Must be called in the same thread as creation of the object.
  void loop();
  /// This is not 100% thread safe, if you call through a raw pointer,
  /// better to call through shared_ptr<EventLoop> for 100% safety.
  void quit();

  /// Time when poll returns, usually means data arrival.
  Timestamp pollReturnTime() const { return pollReturnTime_; }

  int64_t iteration() const { return iteration_; }

  /// Runs callback immediately in the loop thread.
  /// It wakes up the loop, and run the cb.
  /// If in the same loop thread, cb is run within the function.
  /// Safe to call from other threads.
  void runInLoop(Functor cb);
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
  void queueInLoop(Functor cb);

  size_t queueSize() const;

  // timers, 设置定时器任务
  /// Runs callback at 'time'.
  /// Safe to call from other threads.
  TimerId runAt(Timestamp time, TimerCallback cb);
  /// Runs callback after @c delay seconds.
  /// Safe to call from other threads.
  TimerId runAfter(double delay, TimerCallback cb);
  /// Runs callback every @c interval seconds.
  /// Safe to call from other threads.
  TimerId runEvery(double interval, TimerCallback cb);
  /// Cancels the timer.
  /// Safe to call from other threads.
  void cancel(TimerId timerId); // 取消定时器

  // internal usage
  /// 更新channel，就是更新需要poll的连接，因此调用poller内部函数实现
  void wakeup();
  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  bool hasChannel(Channel* channel);

  // pid_t threadId() const { return threadId_; }
  void assertInLoopThread()
  {
    if (!isInLoopThread())
    {
      abortNotInLoopThread();
    }
  }
  /// 当前线程idCurrentThread::tid() 为 构建thread_loop线程的id
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); } 
  // bool callingPendingFunctors() const { return callingPendingFunctors_; }
  bool eventHandling() const { return eventHandling_; }

  void setContext(const boost::any& context)
  { context_ = context; }

  const boost::any& getContext() const
  { return context_; }

  boost::any* getMutableContext()
  { return &context_; }

  static EventLoop* getEventLoopOfCurrentThread();

 private:
  // EventLoop对象创建者并非本线程
  void abortNotInLoopThread();
  // read waked up fd_
  void handleRead();  // waked up
  // 运行等待的任务
  void doPendingFunctors();
  // 打印ChannelList activeChannels_;
  void printActiveChannels() const; // DEBUG

  typedef std::vector<Channel*> ChannelList;
  // 调用EventLoop，设置为true
  bool looping_; /* atomic */
  std::atomic<bool> quit_;
  // 处理事件中
  bool eventHandling_; /* atomic */
  bool callingPendingFunctors_; /* atomic */
  // 迭代次数
  int64_t iteration_;
  /// tid
  const pid_t threadId_;
  // pollReturnTime_ 时间戳
  Timestamp pollReturnTime_;
  /// poller和时间队列
  std::unique_ptr<Poller> poller_;
  std::unique_ptr<TimerQueue> timerQueue_;

  int wakeupFd_;
  // unlike in TimerQueue, which is an internal class,
  // we don't expose Channel to client.
  std::unique_ptr<Channel> wakeupChannel_;

  boost::any context_;

  // scratch variables
  ChannelList activeChannels_;
  Channel* currentActiveChannel_;

  mutable MutexLock mutex_;
  /// 任务队列, 建立在线程栈中的
  std::vector<Functor> pendingFunctors_ GUARDED_BY(mutex_);
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_EVENTLOOP_H_