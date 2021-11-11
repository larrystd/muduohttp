#ifndef MUDUO_NET_CONNECTOR_H_
#define MUDUO_NET_CONNECTOR_H_

#include <functional>
#include <memory>

#include "muduo/base/noncopyable.h"
#include "muduo/net/InetAddress.h"

namespace muduo
{
namespace net
{
class Channel;
class EventLoop;

class Connector : noncopyable,
                  public std::enable_shared_from_this<Connector>  // 这里继承std::enable_shared_from_this<T>, 下面可以调用shared_from
{
 public:
  typedef std::function<void (int sockfd)> NewConnectionCallback; // 新连接回调函数

  Connector(EventLoop* loop, const InetAddress& serverAddr);
  ~Connector();

  void setNewConnectionCallback(const NewConnectionCallback& cb)   // 设置连接回调函数
  { newConnectionCallback_ = cb; }

  void start();  // can be called in any thread
  void restart();  // must be called in loop thread
  void stop();  // can be called in any thread

  const InetAddress& serverAddress() const { return serverAddr_; }

 private:
  enum States { kDisconnected, kConnecting, kConnected };
  static const int kMaxRetryDelayMs = 30*1000;
  static const int kInitRetryDelayMs = 500;

  void setState(States s) { state_ = s; }
  void startInLoop();
  void stopInLoop();
  void connect();
  void connecting(int sockfd);
  
  void handleWrite();
  void handleError();
  void retry(int sockfd);
  int removeAndResetChannel();
  void resetChannel();

  EventLoop* loop_;   // 所属的loop, 用来处理建立的连接(client用eventloop有点大材小用)
  InetAddress serverAddr_;  // IP地址和断开
  bool connect_; // atomic
  States state_;  // 枚举变量
  std::unique_ptr<Channel> channel_;  // 用unique_ptr维护一个Channel
  
  NewConnectionCallback newConnectionCallback_;
  int retryDelayMs_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_CONNECTOR_H_
