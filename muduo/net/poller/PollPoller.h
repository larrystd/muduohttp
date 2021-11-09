#ifndef MUDUO_NET_POLLER_POLLPOLLER_H_
#define MUDUO_NET_POLLER_POLLPOLLER_H_

#include <vector>

#include "muduo/net/Poller.h"

struct pollfd;

namespace muduo
{
namespace net
{

///
/// IO Multiplexing with poll(2).
///
class PollPoller : public Poller {
 public:
  PollPoller(EventLoop* loop);
  ~PollPoller() override; // 覆盖继承类的虚函数

  Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
  void updateChannel(Channel* channel) override;
  void removeChannel(Channel* channel) override;

 private:
  void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
  typedef std::vector<struct pollfd> PollFdList;
  PollFdList pollfds_;
};

}  // namespace net
}  // namespace muduo
#endif  // MUDUO_NET_POLLER_POLLPOLLER_H
