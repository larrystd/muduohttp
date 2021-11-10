#include "muduo/net/EventLoop.h"

#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/Channel.h"
#include "muduo/net/Poller.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/net/TimerQueue.h"

using namespace muduo;
using namespace muduo::net;

namespace
{
__thread EventLoop* t_loopInThisThread = 0; // 线程局部变量, 本线程的loop对象指针

const int kPollTimeMs = 10000;  // poll的时间, 毫秒

// eventfd在内核里的核心是一个计数器counter，它是一个uint64_t的整形变量counter，初始值为initval。
// read操作 如果当前counter > 0，那么read返回counter值，并重置counter为0；如果当前counter等于0，那么read 1)阻塞直到counter大于0
// write操作 write尝试将value加到counter上。write可以多次连续调用，但read读一次即可清零
int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); // 创建eventfd
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
    ::signal(SIGPIPE, SIG_IGN); // 发送信号SIGPIPE, 处理方式为SIG_IGN
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;
}  // namespace

EventLoop* EventLoop::getEventLoopOfCurrentThread() // 得到当前线程的loop指针(线程内部变量)
{
  return t_loopInThisThread;
}

EventLoop::EventLoop()
  : looping_(false),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    iteration_(0),
    threadId_(CurrentThread::tid()),  // 创建eventloop对象的子线程(eventloop为线程栈对象), eventLoop在子线程的线程池中就已经创建
    poller_(Poller::newDefaultPoller(this)),  // 用loop*创建pooler, 是pool存在用unique_ptr维护的指针
    timerQueue_(new TimerQueue(this)),  // 通过loop*创建timerQueue, 赋给timerQueue_指针
    wakeupFd_(createEventfd()), // 创建wakeupFd_
    wakeupChannel_(new Channel(this, wakeupFd_)), // 基于loop* 和wakeupFd创建wakeup通道
    currentActiveChannel_(NULL) // poll之后的活跃通道
{
  LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
  if (t_loopInThisThread) // 线程中存在loop指针(构造eventloop线程不应该持有loop指针)
  {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  }
  else
  {
    t_loopInThisThread = this;  // 设置线程变量t_loopInThisThread指向本eventloop对象
  }
  wakeupChannel_->setReadCallback(
      std::bind(&EventLoop::handleRead, this)); // wakeupfd可读的回调函数为&EventLoop::handleRead
  wakeupChannel_->enableReading();  // 设置wakeupChannel_可读触发, 注册到poll中
}

EventLoop::~EventLoop()
{
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
            << " destructs in thread " << CurrentThread::tid();
  wakeupChannel_->disableAll(); // wakeup通道disable和remove
  wakeupChannel_->remove();
  ::close(wakeupFd_); // 关闭wakeupfd
  t_loopInThisThread = NULL;  // 线程不持有t_loopInThisThread
}

void EventLoop::loop()  // 主要函数, 线程内部线程栈有eventloop对象, 线程一直处于loop循环中。
{
  assert(!looping_);
  assertInLoopThread(); // 线程内部loop只能由这个线程执行
  looping_ = true;
  quit_ = false;
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_)  // 只要quit不为true就循环
  {

    activeChannels_.clear();  // 先清空std::vector<Channel*> ChannelList 活跃channel
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);  // 线程一般会阻塞在poll中, 内部是epoll_wait, 等待触发的事件, 返回的触发事件列表
    /// 打印活跃的channel
    if (Logger::logLevel() <= Logger::TRACE)  // 打印活跃的channel
    {
      printActiveChannels();
    }
    eventHandling_ = true;
    for (Channel* channel : activeChannels_)
    {
      currentActiveChannel_ = channel;
      /// 处理handleEvent函数
      currentActiveChannel_->handleEvent(pollReturnTime_); // 调用channel的handleEvent函数, 实际上根据poll的事件类型调用channel的回调函数。回调函数包括应用层逻辑
    }
    // 清空ActiveChannel_
    currentActiveChannel_ = NULL;
    eventHandling_ = false;

    /// 待执行的任务队列
    doPendingFunctors();  // 执行需要该线程执行的任务队列, 包括Tcp连接建立&TcpConnection::connectEstablished, 定时器任务等
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

void EventLoop::quit() // 退出loop循环, 这个主线程执行
{
  quit_ = true; // 设置quit_ = true,阻断loop()循环
  if (!isInLoopThread())
  {
    wakeup(); // 这里防止子线程阻塞在epoll_wait, 唤醒之
  }
}

void EventLoop::runInLoop(Functor cb)
{
  if (isInLoopThread()) // 如果线程是loop所在线程, 执行之
  {
    cb();
  }
  else
  {
    queueInLoop(std::move(cb)); // 线程不是loop所在线程, 调用queueInLoop加入到loop线程的loop对象的执行队列中
  }
}

/// 将任务加入到任务队列中
void EventLoop::queueInLoop(Functor cb)
{
  {
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(std::move(cb));  // 多个线程可以将任务加入到同一个线程的pendingFunctors_, 因此需要同步
  }
  // 这里还是主线程, 调用wakeup唤醒子线程(阻塞在epollwait呢)
  if (!isInLoopThread() || callingPendingFunctors_) // 不是此线程且
  {
    wakeup(); // 调用wakeup唤醒epoll_wait的子线程
  }
}

size_t EventLoop::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return pendingFunctors_.size(); // 等待队列的大小, 要加锁, 有可能别人在写
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb)
{
  return timerQueue_->addTimer(std::move(cb), time, 0.0); // 将cb, time, 0.0表示在time时刻执行cb的定时
}

TimerId EventLoop::runAfter(double delay, TimerCallback cb)
{
  Timestamp time(addTime(Timestamp::now(), delay)); // 设置时间戳未time+delay
  return runAt(time, std::move(cb));  // 加入定时器
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(std::move(cb), time, interval);  // 有间隔的时间戳加入定时器
}

void EventLoop::cancel(TimerId timerId) // 根据timerId取消定时任务
{
  return timerQueue_->cancel(timerId);
}

void EventLoop::updateChannel(Channel* channel) // 调用poller更新channel->fd的事件监听清空
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();

  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) // 删除channel , 并调用poller删除对应的fd
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


bool EventLoop::hasChannel(Channel* channel)  // has_channel, 调用poller_判断是否存在监听的channel
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

void EventLoop::wakeup()  // 发送一个写操作, 以触发wakeupFd_的可读事件。解除loop阻塞在epoll_wait。
{
  uint64_t one = 1;
  /// 这里是主线程调用, 但wakeupFd_却是子线程的, 结果就是主线程调sockets::write(wakeupFd_, 被对应子线程的poller接收, 起到了唤醒的作用
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

void EventLoop::handleRead()  // 读wakeupFd_回调
{
  uint64_t one = 1;
  ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

void EventLoop::doPendingFunctors() // 运行等待队列的函数
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

