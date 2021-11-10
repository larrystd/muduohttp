#include "muduo/net/poller/EPollPoller.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"

using namespace muduo;
using namespace muduo::net;

// EPollPoller, 维护一个map(fd, channel), updateChannel先处理map信息, 再调用update处理epoll维护fd 数据结构中的信息

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
//  int epoll_create(int size)
// int epoll_ctl(int epfd， int op， int fd， struct epoll_event *event)
//  int epoll_wait(int epfd， struct epoll_event *events， int maxevents， int timeout)
//static_assert，可以在编译期间发现更多的错误，用编译器来强制保证一些契约，尤其是用于模板的时候。
static_assert(EPOLLIN == POLLIN,        "epoll uses same flag values as poll");
static_assert(EPOLLPRI == POLLPRI,      "epoll uses same flag values as poll"); 
static_assert(EPOLLOUT == POLLOUT,      "epoll uses same flag values as poll");
static_assert(EPOLLRDHUP == POLLRDHUP,  "epoll uses same flag values as poll");
static_assert(EPOLLERR == POLLERR,      "epoll uses same flag values as poll");
static_assert(EPOLLHUP == POLLHUP,      "epoll uses same flag values as poll");

namespace
{
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
}

// 构造函数, 构建父类Poller, 成员epollfd_(epoll本身占有的描述符epollfd)
EPollPoller::EPollPoller(EventLoop* loop)
  : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize) // EPOLL_CLOEXEC 有利于关闭进程的无用描述符
{
  if (epollfd_ < 0) {
    LOG_SYSFATAL << "EPollPoller::EPollPoller";
  }
}

/// 关闭epollfd_
EPollPoller::~EPollPoller()
{
  ::close(epollfd_);
}

/// poll返回活跃的channel
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
  LOG_TRACE << "fd total count " << channels_.size(); // 当前注册到poll的channel数量
  /// 调用epoll_wait从获取活跃的事件存储到events_中, 第一个参数epollfd, 第二个参数活跃事件返回的地址, 第三个为文件描述符数量, 最后一个是延迟
  int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs); // 根据起始地址和长度就可以描述一个地址
  int savedErrno = errno;
  Timestamp now(Timestamp::now());  // 当前时间
  // epoll_wait返回的numEvents数量>0, 有活跃的fd
  if (numEvents > 0)
  {
    LOG_TRACE << numEvents << " events happened";
    fillActiveChannels(numEvents, activeChannels);  // 将已经得到的活跃fd封装到avtiveChannels, 活跃事件已经外语events_中了
    
    if (implicit_cast<size_t>(numEvents) == events_.size())
    {
      events_.resize(events_.size()*2);
    }
  }
  else if (numEvents == 0)
  {
    LOG_TRACE << "nothing happened";
  }
  else
  {
    // error happens, log uncommon ones
    if (savedErrno != EINTR)
    {
      errno = savedErrno;
      LOG_SYSERR << "EPollPoller::poll()";
    }
  }
  return now; // 返回当前时间
}


void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList* activeChannels) const
{
  assert(implicit_cast<size_t>(numEvents) <= events_.size());
  
  for (int i = 0; i < numEvents; ++i) // numEvents, 活跃的事件数
  {
    /// 转型events_[i].data.ptr指针转为channel指针, 显然events_[i].data.ptr指向的是Channel对象
    Channel* channel = static_cast<Channel*>(events_[i].data.ptr);  // 这句话让channel有了data数据, 在update中已经有了event.data.ptr = channel
  /// events是epoll注册的事件
    channel->set_revents(events_[i].events);  // 将已经活跃的事件注册到对应的channel中, 让channel有了events数据
    activeChannels->push_back(channel); // 活跃channel列表
  }
}

/// 更新channel, 也就是调用update更新fd监听的事件
/// fd和events构成了监听对象
void EPollPoller::updateChannel(Channel* channel) // 根据传入的channel, 在poll中更新
{
  Poller::assertInLoopThread(); // one loop one thread, 执行poll的线程必须是poll所属eventloop对象所在的那个线程(空间)
  const int index = channel->index();  // 传入channel的index
  LOG_TRACE << "fd = " << channel->fd()
    << " events = " << channel->events() << " index = " << index;
  if (index == kNew || index == kDeleted) // 根据channel->index()可以判断操作是什么, 如果是kNew或者kDelete
  {
    // a new one, add with EPOLL_CTL_ADD
    int fd = channel->fd(); 
    if (index == kNew)
    {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;  // 将pair(fd, channel)对加入到channels_中
    }
    else // index == kDeleted
    {
      assert(channels_.find(fd) != channels_.end());  // 保证pair(fd, channel)必须已经存在于channels中
      assert(channels_[fd] == channel);
    }
    channel->set_index(kAdded); // channel操作信息

    /// 在epoll中添加注册channel的fd
    update(EPOLL_CTL_ADD, channel);
  }
  else
  {
    // update existing one with EPOLL_CTL_MOD/DEL
    int fd = channel->fd();
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(index == kAdded);
    if (channel->isNoneEvent())   // channel没有事件信息 
    {
      update(EPOLL_CTL_DEL, channel); // 执行delete操作
      channel->set_index(kDeleted);
    }
    else
    {
      update(EPOLL_CTL_MOD, channel); // 修改channel的状态
    }
  }
}

/// 在channelmap中删除关联
void EPollPoller::removeChannel(Channel* channel)
{
  Poller::assertInLoopThread(); // 必须在所属loop所在的线程空间的线程执行
  int fd = channel->fd();
  LOG_TRACE << "fd = " << fd;
  assert(channels_.find(fd) != channels_.end());
  assert(channels_[fd] == channel);
  assert(channel->isNoneEvent());
  int index = channel->index();
  assert(index == kAdded || index == kDeleted); // index只能为kadd或kdelete

  size_t n = channels_.erase(fd); // 从channels_ map删除fd对
  assert(n == 1);
  if (index == kAdded)
  {
    update(EPOLL_CTL_DEL, channel); // 从epoll列表中删除channel
  }
  channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel) // 根据operation(EPOLL_CTL_ADD, EPOLL_CTL_MOD)等修改channel
{
  struct epoll_event event; // epoll_event结构体, 主要是data和event
  memZero(&event, sizeof event);  // event清零

  /// 事件元素, 设置event.events, event.data.ptr
  event.events = channel->events(); // 设置event的events
  event.data.ptr = channel; // 设置event.data.ptr
  int fd = channel->fd();
  LOG_TRACE << "epoll_ctl op = " << operationToString(operation)
    << " fd = " << fd << " event = { " << channel->eventsToString() << " }";

  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) // 调用epoll_ctl对epollfd_监控的fd, 设置操作方式operation, 其fd对应的信息变为epoll_event
  {
    // epoll_ctl返回0说明成功, 不会执行if内部信息
    if (operation == EPOLL_CTL_DEL)
    {
      LOG_SYSERR << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
    }
    else
    {
      LOG_SYSFATAL << "epoll_ctl op =" << operationToString(operation) << " fd =" << fd;
    }
  }
}

const char* EPollPoller::operationToString(int op)
{
  switch (op)
  {
    case EPOLL_CTL_ADD:
      return "ADD";
    case EPOLL_CTL_DEL:
      return "DEL";
    case EPOLL_CTL_MOD:
      return "MOD";
    default:
      assert(false && "ERROR op");
      return "Unknown Operation";
  }
}
