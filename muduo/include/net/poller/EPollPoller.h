// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_POLLER_EPOLLPOLLER_H
#define MUDUO_NET_POLLER_EPOLLPOLLER_H

#include "muduo/net/Poller.h"

#include <vector>

struct epoll_event;

namespace muduo
{
namespace net
{

///
/// IO Multiplexing with epoll(4).
///
class EPollPoller : public Poller
{
 public:
  EPollPoller(EventLoop* loop);
  ~EPollPoller() override;
  /// 重写方法
  Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
  void updateChannel(Channel* channel) override;
  void removeChannel(Channel* channel) override;

 private:
  static const int kInitEventListSize = 16;

  static const char* operationToString(int op);
  /// 找到活跃的channel
  void fillActiveChannels(int numEvents,
                          ChannelList* activeChannels) const;
  // update channel
  void update(int operation, Channel* channel);
  /// event 列表, event是一个epoll_event 结构体
  /// epoll_event储存events和data, 存储一个监听的时间
  /// events 是 epoll 注册的事件，比如EPOLLIN、EPOLLOUT等等
  /// data 是一个联合体,用来传递参数
  typedef std::vector<struct epoll_event> EventList;
  /// epoll文件描述符
  int epollfd_;
  /// events_是一个epoll_event结构体的vector
  EventList events_;
};

}  // namespace net
}  // namespace muduo
#endif  // MUDUO_NET_POLLER_EPOLLPOLLER_H
