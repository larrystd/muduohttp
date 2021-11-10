#ifndef MUDUO_NET_ACCEPTOR_H_
#define MUDUO_NET_ACCEPTOR_H_

#include <functional>

#include "muduo/net/Channel.h"
#include "muduo/net/Socket.h"

namespace muduo
{
namespace net
{

class EventLoop;
class InetAddress;

// 对于服务器, listen以及accept连接
class Acceptor : noncopyable
{
 public:
  typedef std::function<void (int sockfd, const InetAddress&)> NewConnectionCallback;

  Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);

  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }

  void listen();  // 阻塞监听连接到来
  bool listening() const {
    return listening_;
  }

 private:
  void handleRead();

  EventLoop* loop_;  // 被哪个eventloop执行
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_;
  bool listening_;
  int idleFd_;
};

} // namespace net
} // namespace muduo

#endif  // MUDUO_NET_ACCEPTOR_H_
