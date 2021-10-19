// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/Timestamp.h"

#include <functional>
#include <memory>

namespace muduo
{
namespace net
{

class EventLoop;

/// channel 网络连接的抽象
/// A selectable I/O channel.
///
/// 关键作用1, 设置回调函数ReadCallback， WriteCallback, CloseCallback, ErrorCallback
/// 关键作用2, 设置channel可读可写, 具体的通过update poll event实现
/// 关键作用3, 基于poll传回的revent选择调用对应回调函数进行处理
/// 作用4, 将events_, revents_ 格式化成字符串输出之

class Channel : noncopyable
{
 public:
  // event回调函数类型，包括写回调，关闭回调，错误回调
  typedef std::function<void()> EventCallback;
  // 读事件回调函数
  typedef std::function<void(Timestamp)> ReadEventCallback;

  // 构造函数，传入EventLoop和监听的fd
  Channel(EventLoop* loop, int fd);
  ~Channel();

  // 事件处理函数，传入接收时间
  void handleEvent(Timestamp receiveTime);

  // 设置回调函数，分别是写回调，读回调，关闭回调，错误回调
  void setReadCallback(ReadEventCallback cb)
  { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb)
  { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb)
  { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb)
  { errorCallback_ = std::move(cb); }

  /// 传入shared_ptr对象 绑定const std::shared_ptr<void>& obj
  void tie(const std::shared_ptr<void>&);


  int fd() const { return fd_; }
  int events() const { return events_; }

  void set_revents(int revt) { revents_ = revt; } // used by pollers
  // int revents() const { return revents_; }
  bool isNoneEvent() const { return events_ == kNoneEvent; }
  
  // 设置该通道的event可读，可写等，并向poller中更新之
  void enableReading() { events_ |= kReadEvent; update(); }
  void disableReading() { events_ &= ~kReadEvent; update(); }
  void enableWriting() { events_ |= kWriteEvent; update(); }
  void disableWriting() { events_ &= ~kWriteEvent; update(); }
  void disableAll() { events_ = kNoneEvent; update(); }

  /// 当前channel是否可写
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }

  // for Poller
  int index() { return index_; }
  /// 设置
  void set_index(int idx) { index_ = idx; }

  // for debug
  string reventsToString() const;
  string eventsToString() const;

  void doNotLogHup() { logHup_ = false; }

  EventLoop* ownerLoop() { return loop_; }
  void remove();

 private:
  static string eventsToString(int fd, int ev);
  /// 更新channel
  void update();

  void handleEventWithGuard(Timestamp receiveTime);
  /// 三种event, 空事件, 读事件, 写事件
  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  // eventloop对象
  EventLoop* loop_;
  // 文件描述符
  const int  fd_;

  int        events_;
  // poll 传回的事件
  int        revents_; // it's the received event types of epoll or poll
  /// poll要对fd进行的操作，例如kdeleted等
  int        index_; // used by Poller.
  bool       logHup_;

  // 绑定对象的软连接
  std::weak_ptr<void> tie_;
  bool tied_;
  bool eventHandling_;
  bool addedToLoop_;

  // 回调函数
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_CHANNEL_H
