#ifndef MUDUO_NET_CHANNEL_H_
#define MUDUO_NET_CHANNEL_H_

#include <functional>
#include <memory>

#include "muduo/base/noncopyable.h"
#include "muduo/base/Timestamp.h"

namespace muduo
{
namespace net
{

class EventLoop;

/// channel 网络连接的抽象
/// A selectable I/O channel.
///
/// 设置回调函数ReadCallback， WriteCallback, CloseCallback, ErrorCallback
/// channel可读可写, 具体的通过update poll event实现
/// 基于poll传回的revent会调用对应回调函数进行处理

class Channel : noncopyable
{
 public:
  // event回调函数类型，包括写回调，关闭回调，错误回调
  typedef std::function<void()> EventCallback;
  // 读事件回调函数, 形参为时间戳
  typedef std::function<void(Timestamp)> ReadEventCallback;

  // 构造函数，传入EventLoop和监听的fd
  Channel(EventLoop* loop, int fd);
  ~Channel();

  void handleEvent(Timestamp receiveTime);   // 事件处理函数，传入接收时间

  void setReadCallback(ReadEventCallback cb)  // 设置读时间回调
  { readCallback_ = std::move(cb);  }
  void setWriteCallback(EventCallback cb) // 写回调函数
  { writeCallback_ = std::move(cb); }
  
  void setCloseCallback(EventCallback cb) // Channel关闭回调函数
  { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) // 错误回调函数
  { errorCallback_ = std::move(cb); }

  void tie(const std::shared_ptr<void>&); // Channel对应的shared_ptr维护的TcpConnection对象

  int fd() const { return fd_; }  // Channel的fd_
  int events() const { return events_; }  // Channel负责的事件

  void set_revents(int revt) { revents_ = revt; } // poll返回的事件used by pollers
  bool isNoneEvent() const { return events_ == kNoneEvent; }  // 空事件
  
  // 设置通道的events_为可读，可写等事件，并向poller中注册该事件
  void enableReading() { events_ |= kReadEvent; update(); }
  void disableReading() { events_ &= ~kReadEvent; update(); }
  void enableWriting() { events_ |= kWriteEvent; update(); }
  void disableWriting() { events_ &= ~kWriteEvent; update(); }
  // disableAll(), 设置为空事件
  void disableAll() { events_ = kNoneEvent; update(); }

  /// 当前channel是否可写可读
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }

  // for Poller
  int index() { return index_; }  // Channel的操作

  void set_index(int idx) { index_ = idx; } // 设置Channel的操作

  // for debug
  string reventsToString() const;
  string eventsToString() const;

  void doNotLogHup() { logHup_ = false; }

  EventLoop* ownerLoop() { return loop_; }  // Channel被哪个eventloo负责
  void remove();

 private:
  static string eventsToString(int fd, int ev);
  void update();    // 向poll里更新channel

  void handleEventWithGuard(Timestamp receiveTime);
  
  /// 三种event, 空事件, 读事件, 写事件
  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_; // Channel会被某个eventloop负责
  const int  fd_; // Channel维护的fd

  int        events_; // Channel的事件
  int        revents_;   // poll 传回的事件
  int        index_;   // poll要对fd进行的操作，例如kdeleted等
  bool       logHup_;

  std::weak_ptr<void> tie_; // 用weakptr链接TcpConnection
  bool tied_;
  bool eventHandling_;
  bool addedToLoop_;
  ReadEventCallback readCallback_;   // 回调函数
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_CHANNEL_H_
