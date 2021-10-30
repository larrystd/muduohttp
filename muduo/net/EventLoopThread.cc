// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/EventLoopThread.h"

#include "muduo/net/EventLoop.h"

using namespace muduo;
using namespace muduo::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const string& name)
  : loop_(NULL),  // loop_是指针, 指向子线程所运行的loop_对象
    exiting_(false),
    //初始化时 注册绑定线程执行的函数, EventLoopThread::threadFunc
    thread_(std::bind(&EventLoopThread::threadFunc, this), name),
    mutex_(),
    cond_(mutex_),
    callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
  {
    // still a tiny chance to call destructed object, if threadFunc exits just now.
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    loop_->quit();
    thread_.join();
  }
}

// （主线程)开启线程(执行函数)
EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  // 新线程t执行start() 线程执行绑定好的函数(新线程内部有同样的EventLoopThread，但是一个引用, 和主线程都是一个对象), 即threadFunc()。这句话本身创建一个线程执行指定函数, 主线程进行向下执行
  thread_.start();

  EventLoop* loop = NULL;
  {
    MutexLockGuard lock(mutex_);
    /// main线程等待loop_
    while (loop_ == NULL)
    {
      cond_.wait();
    }
    /// loop有了(新线程创建的), 返回loop对象
    loop = loop_;
  }
  /// 
  return loop;
}

// 新线程执行的函数
void EventLoopThread::threadFunc()
{
   /// 新线程栈上创建eventloop对象, 关键啊
  EventLoop loop;

  if (callback_)
  {
    // 执行回调函数ThreadInitCallback
    callback_(&loop);
  }

  {
    MutexLockGuard lock(mutex_);
    // loop_指向线程内部的loop对象
    loop_ = &loop;
    /// 唤醒主线程返回loop
    cond_.notify();
  }
  // 新线程执行loop循环
  loop.loop();
  //assert(exiting_);
  MutexLockGuard lock(mutex_);
  loop_ = NULL;
}

