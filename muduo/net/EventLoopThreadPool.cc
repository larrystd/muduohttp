#include "muduo/net/EventLoopThreadPool.h"

#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const string& nameArg)  // 用baseloop(主线程的loop)构建eventloopthreadpool对象
  : baseLoop_(baseLoop),
    name_(nameArg),
    started_(false),
    numThreads_(0),
    next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
  // Don't delete loop, it's stack variable
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) // 线程池的start, 传入线程创建好的回调函数
{
  assert(!started_);
  baseLoop_->assertInLoopThread();  // 执行的线程必须是baseLoop所在线程, 也就是main线程
  started_ = true;
  for (int i = 0; i < numThreads_; ++i) // 依次创建线程
  {

    char buf[name_.size() + 32];
    snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);  // 线程名储存到buf中
    EventLoopThread* t = new EventLoopThread(cb, buf);  // 创建一个EventLoopThread
    threads_.push_back(std::unique_ptr<EventLoopThread>(t));  // 线程指针t用unique_ptr维护, 加入的线程列表中
    loops_.push_back(t->startLoop()); // t->startLoop()返回的loop_指针加入到执行loop列表中
  }
  if (numThreads_ == 0 && cb) // 执行线程池创建好的回调函数cb
  {
    cb(baseLoop_);
  }
}

EventLoop* EventLoopThreadPool::getNextLoop() // 返回下一个loop指针(可用的指针), 给tcpserver和client
{
  baseLoop_->assertInLoopThread();
  assert(started_);
  EventLoop* loop = baseLoop_;

  if (!loops_.empty())  // 如果loop不为空
  {
    loop = loops_[next_];
    ++next_;
    if (implicit_cast<size_t>(next_) >= loops_.size())
    {
      next_ = 0;  // 重置next_
    }
  }
  return loop;  // loop对象指针
}

EventLoop* EventLoopThreadPool::getLoopForHash(size_t hashCode)
{
  baseLoop_->assertInLoopThread();
  EventLoop* loop = baseLoop_;

  if (!loops_.empty())
  {
    loop = loops_[hashCode % loops_.size()];
  }
  return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()  // 所有的loops返回为vector列表
{
  baseLoop_->assertInLoopThread();
  assert(started_);
  if (loops_.empty())
  {
    return std::vector<EventLoop*>(1, baseLoop_);
  }
  else
  {
    return loops_;
  }
}