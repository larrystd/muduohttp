#ifndef MUDUO_NET_POLLER_H_
#define MUDUO_NET_POLLER_H_

#include <map>
#include <vector>

#include "muduo/base/Timestamp.h"
#include "muduo/net/EventLoop.h"

namespace muduo
{
namespace net
{

class Channel;

///
/// Base class for IO Multiplexing
///
/// This class doesn't own the Channel objects.
class Poller : noncopyable
{
 public:
  typedef std::vector<Channel*> ChannelList;  //  poll监听的channel列表

  Poller(EventLoop* loop);  // 用loop构造
  virtual ~Poller();

  /// Polls the I/O events.
  /// Must be called in the loop thread.
  virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0; // poll, 返回封装的activeChannel纯虚函数, 需要覆盖。

  /// Changes the interested I/O events.
  /// Must be called in the loop thread.
  /// 更新channel的函数
  virtual void updateChannel(Channel* channel) = 0; // 更新Channel的状态,

  /// Remove the channel, when it destructs.
  /// Must be called in the loop thread.
  virtual void removeChannel(Channel* channel) = 0; // 根据channel移除监听

  virtual bool hasChannel(Channel* channel) const;

  static Poller* newDefaultPoller(EventLoop* loop);

  void assertInLoopThread() const
  {
    ownerLoop_->assertInLoopThread();
  }

 protected:
 /// 一个fd->channel 的map
  typedef std::map<int, Channel*> ChannelMap; // 根据连接的fd找到其Channel
  ChannelMap channels_; // channels_是一个map

 private:
  EventLoop* ownerLoop_;  // 指向持有poll的loop
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_POLLER_H_
