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
  void loop();
  void quit();

  Timestamp pollReturnTime() const { return pollReturnTime_; }  // poll触发返回的时间戳

  int64_t iteration() const { return iteration_; }

  void runInLoop(Functor cb); // 保证在loop所属线程中执行某函数, 如果在线程立即执行, 如果不在调用queueInLoop

  void queueInLoop(Functor cb); // 放入到loop对象的等待队列中并触发loop对象所属线程执行

  size_t queueSize() const;

  // timers, 设置定时器任务
  TimerId runAt(Timestamp time, TimerCallback cb);  // 某个时刻执行定时任务
  TimerId runAfter(double delay, TimerCallback cb); // 再过某个时间段执行的定时任务
  TimerId runEvery(double interval, TimerCallback cb);  // 设置一个循环定时器
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
  // 当前线程idCurrentThread::tid() 为 构建thread_loop线程的id
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

  void abortNotInLoopThread();   // EventLoop对象创建者并非本线程
  void handleRead();    // wakefd触发的回调函数
  void doPendingFunctors();   // 运行等待的任务
  void printActiveChannels() const; // 打印当前被触发的channel

  typedef std::vector<Channel*> ChannelList;  // 已经使用poll注册监听的channel
  bool looping_; // 是否处于循环
  std::atomic<bool> quit_;  // 设置原子的(这里没啥用, 因为loop对象不是线程共享的)
  bool eventHandling_; // 在处理事件
  bool callingPendingFunctors_; // 在执行PendingFunctors_
  int64_t iteration_; // loop的迭代次数
  const pid_t threadId_;    /// tid, loop对象所属线程的tid
  Timestamp pollReturnTime_;   // pollReturnTime_ poll返回的时间戳
  std::unique_ptr<Poller> poller_;  // poller_, IO多路复用
  std::unique_ptr<TimerQueue> timerQueue_;  // 定时器队列

  int wakeupFd_;
  // unlike in TimerQueue, which is an internal class,
  // we don't expose Channel to client.
  std::unique_ptr<Channel> wakeupChannel_;
  boost::any context_;

  // scratch variables
  ChannelList activeChannels_;
  Channel* currentActiveChannel_;

  mutable MutexLock mutex_;

  std::vector<Functor> pendingFunctors_ GUARDED_BY(mutex_);   // 任务队列, 建立在线程栈中
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_EVENTLOOP_H_