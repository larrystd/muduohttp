// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOPTHREAD_H
#define MUDUO_NET_EVENTLOOPTHREAD_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"
#include "muduo/base/Thread.h"

// EventLoopThread，既要有eventloop，也要有thread
// 注册一个回调函数，创建新线程后执行回调void(EventLoop*) ThreadInitCallback
// 新线程会创建一个eventloop, 执行loop循环, 当创建loop之后主线程获取并返回之

namespace muduo
{
namespace net
{

class EventLoop;

class EventLoopThread : noncopyable
{
 public:
 /// eventloop thread创建之后的回调函数, 参数为void(EventLoop*)
  typedef std::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const string& name = string());
  ~EventLoopThread();

  /// EventloopThread的startLoop()
  EventLoop* startLoop();

 private:
  void threadFunc();

  /// 线程内部的loop对象
  EventLoop* loop_ GUARDED_BY(mutex_);
  bool exiting_;
  
  // 新thread创建
  Thread thread_;
  MutexLock mutex_;
  Condition cond_ GUARDED_BY(mutex_);
  ThreadInitCallback callback_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_EVENTLOOPTHREAD_H

