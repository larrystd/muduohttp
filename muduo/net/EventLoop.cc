// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)


#include "muduo/net/EventLoop.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/Channel.h"
#include "muduo/net/Poller.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/net/TimerQueue.h"

#include <algorithm>

#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{

/// 线程局部变量，event_loop
__thread EventLoop* t_loopInThisThread = 0;

const int kPollTimeMs = 10000;

/// eventfd
// 在Linux系统中，eventfd是一个用来通知事件的文件描述符，timerfd是的定时器事件的文件描述符
/// eventfd用来触发事件通知，timerfd用来触发将来的事件通知。

// eventfd在内核里的核心是一个计数器counter，它是一个uint64_t的整形变量counter，初始值为initval。
// read操作 如果当前counter > 0，那么read返回counter值，并重置counter为0；如果当前counter等于0，那么read 1)阻塞直到counter大于0
// write操作 write尝试将value加到counter上。write可以多次连续调用，但read读一次即可清零
int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

// 忽视旧C风格类型转换
#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
 public:
  IgnoreSigPipe()
  {
    ::signal(SIGPIPE, SIG_IGN);
    // LOG_TRACE << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;
}  // namespace

// 当前线程的eventloop
EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

EventLoop::EventLoop()
  : looping_(false),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    iteration_(0),
    /// 构造thread_loop对象的线程id, 一般就是子线程
    threadId_(CurrentThread::tid()),
    // 创建一个poller_对象, 一个子线程会有一个eventloop, 一个eventloop会有一个poller_, 
    poller_(Poller::newDefaultPoller(this)),
    timerQueue_(new TimerQueue(this)),
    /// 创建wakeupFd_
    wakeupFd_(createEventfd()),
    /// 创建一个wakeChannel, 用来接收wakeup的socket
    wakeupChannel_(new Channel(this, wakeupFd_)),
    currentActiveChannel_(NULL)
{
  LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
  /// 当前线程已经有eventloop对象了
  if (t_loopInThisThread)
  {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  }
  else
  {
    t_loopInThisThread = this;
  }
  /// wakeChannel如果被唤醒, 调用&EventLoop::handleRead
  wakeupChannel_->setReadCallback(
      std::bind(&EventLoop::handleRead, this));
  // 设置wakeupfd可读, 注意在eventloop创建时就设置了, 也就是说这是在子线程内部loop中设置一个wakechannel
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
            << " destructs in thread " << CurrentThread::tid();
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = NULL;
}

// EventLoop的loop循环
/// loop是针对服务器的, 需要处理大量客户端的处理的回调函数。执行loop管理线程需要one thread one loop, 但执行这些回调函数是可以多线程的

/// 会有个线程池执行大量eventloop, 同时这些线程可以执行任务队列的函数

void EventLoop::loop()
{
  assert(!looping_);
  /// 该线程是创建eventloop的线程(每个子线程都可以)
  assertInLoopThread();
  looping_ = true;
  quit_ = false;  // FIXME: what if someone calls quit() before loop() ?
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_)
  /// loop循环未终止
  {

    /// 使用poller_->poll获取活跃的fd所属的activeChannels_，调用注册回调函数handleEvent。
    activeChannels_.clear();
    /// 一般的会阻塞在poll直到有活跃的channel, 注意这只是影响的channel对象
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); 
    ++iteration_;
    /// 打印活跃的channel
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels();
    }
    // TODO sort channel by priority
    // 执行活跃函数的回调函数，注意对每一个子函数
    eventHandling_ = true;
    for (Channel* channel : activeChannels_)
    {
      currentActiveChannel_ = channel;
      /// 处理handleEvent函数
      currentActiveChannel_->handleEvent(pollReturnTime_); 
    }
    // 清空ActiveChannel_
    currentActiveChannel_ = NULL;
    eventHandling_ = false;

    /// 执行需要在io线程中执行的函数(防止多线程竞态)
    /// 注意如果此时该EventLoop中迟迟没有事件触发，那么epoll_wait一直就会阻塞。 这样会导致，pendingFunctors_迟迟不能被执行了。
    /// 这时候需要唤醒epoll_wait，随机触发一个事件让poller_->poll解除阻塞

    /// 待执行的任务队列
    doPendingFunctors();
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

/// 退出loop循环
void EventLoop::quit()
{
  quit_ = true;
  // 当前的线程不是创建eventloop对象的线程, 需要等待
  // 创建eventloop对象的线程负责销毁它
  if (!isInLoopThread())
  {
    wakeup();
  }
}

void EventLoop::runInLoop(Functor cb)
{
  /// 创建eventloop的线程中执行cb函数, 保证始终只有一个线程执行该函数, 是创建该eventloop对象的线程
  /// 
  if (isInLoopThread())
  {
    cb();
  }
  else

  {
    ///任务先入队列，等eventloop线程自动调用之
    queueInLoop(std::move(cb));
  }
}

/// 将任务加入到任务队列中
void EventLoop::queueInLoop(Functor cb)
{
  /// 主线程会调用runInLoop, 进而调用queueInLoop, 但loop对象是子线程的, 因此cb会到子线程的pendingFunctors_中
  {
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(std::move(cb));
  }
  // 这里头还是主线程, 会调用wakeup
  if (!isInLoopThread() || callingPendingFunctors_)
  {
    /// 唤醒eventloop的epoll_wait等待事件触发, 从而让eventloop线程自动执行任务队列pendingFunctors_中的函数
    wakeup();
  }
}

/// 等待io执行的函数大小
size_t EventLoop::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return pendingFunctors_.size();
}

/// 定时器, 在time时刻执行TimerCallback
TimerId EventLoop::runAt(Timestamp time, TimerCallback cb)
{
  return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

/// 定时器, 延迟执行TimerCallback
TimerId EventLoop::runAfter(double delay, TimerCallback cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, std::move(cb));
}

/// 定时器, 每间隔interval执行TimerCallback
TimerId EventLoop::runEvery(double interval, TimerCallback cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(std::move(cb), time, interval);
}

/// 定时器取消timerId
void EventLoop::cancel(TimerId timerId)
{
  return timerQueue_->cancel(timerId);
}
/// 更新此eventloop 监听的channel,基于poller_修改fd, 被channel调用
void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();

  poller_->updateChannel(channel);
}

/// 删除channel, 基于poller_删除注册的fd, 被channel调用
void EventLoop::removeChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

/// eventloop has_channel, 实际是调用poller_判断监听的channel
bool EventLoop::hasChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  return poller_->hasChannel(channel);
}

/// 非loop线程执行, 抛弃此次执行
void EventLoop::abortNotInLoopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

/// 对这个wakeupFd_进行写操作，以触发wakeupFd_的可读事件。解除loop阻塞在epoll_wait。
/// 直接发送一个socket
void EventLoop::wakeup()
{
  uint64_t one = 1;
  /// 主线程调用, 但wakeupFd_却是子线程的, 结果就是主线程调sockets::write(wakeupFd_, 被对应子线程的poller接收, 起到了唤醒的作用
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

/// 读wakeupFd_
void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

/// 运行等待的函数
void EventLoop::doPendingFunctors()
{
  /// 新建一个functor
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;
  /// CAS操作， 有可能线程正在执行pendingFunctors_, 因此需要CAS防止竞态
  {
  MutexLockGuard lock(mutex_);
  functors.swap(pendingFunctors_);
  }

  /// 全部执行完
  for (const Functor& functor : functors)
  {
    functor();
  }
  callingPendingFunctors_ = false;
}

/// 打印activeChannels_
void EventLoop::printActiveChannels() const
{
  for (const Channel* channel : activeChannels_)
  {
    LOG_TRACE << "{" << channel->reventsToString() << "} ";
  }
}

