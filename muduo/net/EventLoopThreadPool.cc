// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/EventLoopThreadPool.h"

#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThread.h"

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const string& nameArg)
  : baseLoop_(baseLoop),
  /// 线程名
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

/// 开启线程池
void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
  assert(!started_);
  /// 当前loop创建线程, 也就是主线程
  baseLoop_->assertInLoopThread();
  // 开启线程池
  started_ = true;
  // 创建threads和loops
  for (int i = 0; i < numThreads_; ++i)
  {
    /// buf, 储存线程名
    char buf[name_.size() + 32];
    snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);

    // 构造EventLoopThread对象, 初始化thread对象执行函数
    EventLoopThread* t = new EventLoopThread(cb, buf);
    /// t加入threads 列表, 注意t对象在堆上
    threads_.push_back(std::unique_ptr<EventLoopThread>(t));
    /// 新线程t执行startLoop()函数,子线程创建loop对象并执行这个loop(), 主线程得到子线程的loop对象指针, 
    // 线程指针加入loops_。loops是一个std::vector<EventLoop*>, 子线程正阻塞在loop中。
    // 这里不知不觉的主线程创建的EventLoopThread对象交由了子线程
    loops_.push_back(t->startLoop());
  }
  /// 不创建新线程， 当前线程执行cb
  if (numThreads_ == 0 && cb)
  {
    cb(baseLoop_);
  }
}

/// loop 是一个vector, 得到下一个loop(每个子线程一个loop对象, 下一个loop每个loop都来自不同的线程)
EventLoop* EventLoopThreadPool::getNextLoop()
{
  baseLoop_->assertInLoopThread();
  assert(started_);
  EventLoop* loop = baseLoop_;

  if (!loops_.empty())
  {
    // round-robin
    loop = loops_[next_];
    ++next_;
    if (implicit_cast<size_t>(next_) >= loops_.size())
    {
      next_ = 0;
    }
  }
  return loop;
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

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
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
