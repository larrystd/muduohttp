#ifndef MUDUO_NET_POLLER_EPOLLPOLLER_H_
#define MUDUO_NET_POLLER_EPOLLPOLLER_H_

#include "muduo/net/Poller.h"

#include <vector>

struct epoll_event;

namespace muduo
{
namespace net
{

///
/// IO Multiplexing with epoll(4). 使用epoll的IO多路复用
///
class EPollPoller : public Poller
{
 public:
  EPollPoller(EventLoop* loop); // 持有epoll的loop对象
  ~EPollPoller() override;

  Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;  // 重写poll
  void updateChannel(Channel* channel) override;
  void removeChannel(Channel* channel) override;

 private:
  static const int kInitEventListSize = 16;

  static const char* operationToString(int op);
  /// 找到活跃的channel
  void fillActiveChannels(int numEvents,
                          ChannelList* activeChannels) const;
  void update(int operation, Channel* channel); // 更新监听的channel, 例如可读可写

  // epoll_event结构体含有events和data, events 是uint32_t指代的 epoll 注册的事件，比如EPOLLIN、EPOLLOUT等等
  // data 是一个union epoll_data_t,包含epoll的对象, 比如data.fd文件描述符
  typedef std::vector<struct epoll_event> EventList;  // 持有的epoll_event, 用vector维护

  int epollfd_;   // epoll文件描述符
  EventList events_;  // 储存持有的events list
};

}  // namespace net
}  // namespace muduo
#endif  // MUDUO_NET_POLLER_EPOLLPOLLER_H_
