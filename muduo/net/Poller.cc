#include "muduo/net/Poller.h"

#include "muduo/net/Channel.h"

using namespace muduo;
using namespace muduo::net;

// loop表示持有pool的loop
Poller::Poller(EventLoop* loop)
  : ownerLoop_(loop)
{
}

Poller::~Poller() = default;

/// 是否拥有(监听)某个channel, 使用find查找
bool Poller::hasChannel(Channel* channel) const
{
  assertInLoopThread();
  ChannelMap::const_iterator it = channels_.find(channel->fd());
  return it != channels_.end() && it->second == channel;
}

