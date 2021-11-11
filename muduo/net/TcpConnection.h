#ifndef MUDUO_NET_TCPCONNECTION_H_
#define MUDUO_NET_TCPCONNECTION_H_

#include <memory>
#include <boost/any.hpp>

#include "muduo/base/noncopyable.h"
#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/InetAddress.h"

struct tcp_info;  // tcp_info的信息

namespace muduo
{
namespace net
{

// 更上一层的Tcp连接
// 用户编写的回调函数位于此, TcpConnection再将注册到到Channel中

class Channel;
class EventLoop;
class Socket;

/// TCP connection, for both client and server usage.
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection>  // 可使用shared_from_this
{
 public:
  /// Constructs a TcpConnection with a connected sockfd
  TcpConnection(EventLoop* loop,  
                const string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
  ~TcpConnection();

  // connection的loop_
  EventLoop* getLoop() const { return loop_; }
  const string& name() const { return name_; }
  const InetAddress& localAddress() const { return localAddr_; }
  const InetAddress& peerAddress() const { return peerAddr_; }

  bool connected() const { return state_ == kConnected; }
  bool disconnected() const { return state_ == kDisconnected; }
  // return true if success.
  bool getTcpInfo(struct tcp_info*) const;  
  string getTcpInfoString() const;  // tcpinfo信息

  void send(const void* message, int len);   // 发送message
  void send(const StringPiece& message);
  void send(Buffer* message);  // this one will swap data

  void shutdown(); // NOT thread safe, no simultaneous calling
  // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
  void forceClose();
  void forceCloseWithDelay(double seconds);
  void setTcpNoDelay(bool on);
  // reading or not
  void startRead();
  void stopRead();

  bool isReading() const { return reading_; };

  /// context
  void setContext(const boost::any& context)
  { context_ = context; }

  const boost::any& getContext() const
  { return context_; }

  boost::any* getMutableContext()
  { return &context_; }

  void setConnectionCallback(const ConnectionCallback& cb)   // 连接回调函数
  { connectionCallback_ = cb; }

  void setMessageCallback(const MessageCallback& cb)   // 信息回调函数
  { messageCallback_ = cb; }

  void setWriteCompleteCallback(const WriteCompleteCallback& cb)   // 写毕回调函数
  { writeCompleteCallback_ = cb; }

  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
  { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

  Buffer* inputBuffer() // 可读的信息会自动读取放入inputBuffer中
  { return &inputBuffer_; }
  Buffer* outputBuffer()  // 写出的信息先放入outputBuffer
  { return &outputBuffer_; }

  /// Internal use only.
  void setCloseCallback(const CloseCallback& cb)
  { closeCallback_ = cb; }

  // called when TcpServer accepts a new connection
  void connectEstablished();   // should be called only once
  // called when TcpServer has removed me from its map
  void connectDestroyed();  // should be called only once

 private:
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting }; // TcpConnection的连接

  void handleRead(Timestamp receiveTime);   // 可读处理函数
  void handleWrite(); // 可写处理
  void handleClose();
  void handleError();

  void sendInLoop(const StringPiece& message);  // 在loop所在的线程中执行
  void sendInLoop(const void* message, size_t len);
  void shutdownInLoop();

  void forceCloseInLoop();
  void setState(StateE s) { state_ = s; }
  const char* stateToString() const;
  void startReadInLoop();
  void stopReadInLoop();

  EventLoop* loop_; // TcpConnection所属的EventLoop
  const string name_;
  StateE state_;  // FIXME: use atomic variable
  bool reading_;

  std::unique_ptr<Socket> socket_;  // socket unique_ptr
  std::unique_ptr<Channel> channel_;  // TcpConnection的Channel通道

  const InetAddress localAddr_;   // Tcp连接的本地IP地址
  const InetAddress peerAddr_;  // 外地Ip地址
  

  ConnectionCallback connectionCallback_;   // 连接回调函数
  MessageCallback messageCallback_; // 信息回调函数
  WriteCompleteCallback writeCompleteCallback_;   // 写毕回调函数
  HighWaterMarkCallback highWaterMarkCallback_;
  CloseCallback closeCallback_; // 连接关闭回调函数
  size_t highWaterMark_;

  // inputBuffer和outputBuffer_, 一个是读缓存, 一个是写缓存
  Buffer inputBuffer_;
  Buffer outputBuffer_;

  boost::any context_;  // context
};

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;  // 用shared_ptr维护的TcpConnection

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_TCPCONNECTION_H_
