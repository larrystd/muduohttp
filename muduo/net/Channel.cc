#include "muduo/net/Channel.h"

#include <sstream> 
#include <poll.h>

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

using namespace muduo;
using namespace muduo::net;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI; // 001 | 010 = 011, POLLPRI 急切地读
const int Channel::kWriteEvent = POLLOUT; // 100

Channel::Channel(EventLoop* loop, int fd__) // 构造函数, 构造所属loop_指针, fd
  : loop_(loop),
    fd_(fd__),
    events_(0),
    revents_(0),
    index_(-1),
    logHup_(true),
    tied_(false),
    eventHandling_(false),
    addedToLoop_(false)
{
}

Channel::~Channel()
{
  assert(!eventHandling_);
  assert(!addedToLoop_);
  if (loop_->isInLoopThread())
  {
    assert(!loop_->hasChannel(this));
  }
}

void Channel::tie(const std::shared_ptr<void>& obj) // Channel地weak_ptr绑定TcpConnection
{
  tie_ = obj;
  tied_ = true;
}

void Channel::update() // 向poll更新event(即channel)
{
  addedToLoop_ = true;
  loop_->updateChannel(this); // 实际调用poll
}

void Channel::remove() // 向poll删除channel对应地fd
{
  assert(isNoneEvent());
  addedToLoop_ = false;
  loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)  // 当poll有活跃channel会调用这个函数
{
  std::shared_ptr<void> guard;
  if (tied_)
  {
    guard = tie_.lock();  // 绑定地TcpConnection是否存活
    if (guard)
    {
      handleEventWithGuard(receiveTime);
    }
  }
  else
  // tie_还为false, 一般这里是执行tcpConnection的构造
  {
    handleEventWithGuard(receiveTime);
  }
}

void Channel::handleEventWithGuard(Timestamp receiveTime) // channel活跃之后最终执行这个函数
{
  eventHandling_ = true;
  LOG_TRACE << reventsToString(); // 根据活跃channel的事件类型等选择调用函数
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) // 关闭连接的回调函数
  {
    if (logHup_)
    {
      LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
    }
    if (closeCallback_) closeCallback_(); // 关闭连接
  }

  if (revents_ & POLLNVAL)
  {
    LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
  }
  if (revents_ & (POLLERR | POLLNVAL))  // 错误回调
  {
    if (errorCallback_) errorCallback_();
  }
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))  // 可读回调
  {
    if (readCallback_) readCallback_(receiveTime);
  }
  if (revents_ & POLLOUT) // 可写回调
  {
    if (writeCallback_) writeCallback_();
  }
  eventHandling_ = false;
}

string Channel::reventsToString() const // tostrings()
{
  return eventsToString(fd_, revents_);
}

string Channel::eventsToString() const
{
  return eventsToString(fd_, events_);
}

string Channel::eventsToString(int fd, int ev)  // 将fd和event内容格式化到string中
{
  std::ostringstream oss;
  oss << fd << ": ";
  if (ev & POLLIN)
    oss << "IN ";
  if (ev & POLLPRI)
    oss << "PRI ";
  if (ev & POLLOUT)
    oss << "OUT ";
  if (ev & POLLHUP)
    oss << "HUP ";
  if (ev & POLLRDHUP)
    oss << "RDHUP ";
  if (ev & POLLERR)
    oss << "ERR ";
  if (ev & POLLNVAL)
    oss << "NVAL ";

  return oss.str(); // 返回格式化的string
}