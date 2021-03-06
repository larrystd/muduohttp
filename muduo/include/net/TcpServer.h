#ifndef MUDUO_NET_TCPSERVER_H_
#define MUDUO_NET_TCPSERVER_H_

#include <map>

#include "muduo/base/Atomic.h"
#include "muduo/base/Types.h"
#include "muduo/net/TcpConnection.h"

namespace muduo
{
namespace net
{

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

/// TCP server, supports single-threaded and thread-pool models.
class TcpServer : noncopyable
{
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;
  enum Option
  {
    kNoReusePort,
    kReusePort,
  };

  TcpServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg,
            Option option = kNoReusePort);  // 构造函数
  ~TcpServer();  // force out-line dtor, for std::unique_ptr members.

  const string& ipPort() const { return ipPort_; }
  const string& name() const { return name_; }
  EventLoop* getLoop() const { return loop_; }

  void setThreadNum(int numThreads);  // 线程个数
  void setThreadInitCallback(const ThreadInitCallback& cb)
  { threadInitCallback_ = cb; }
  std::shared_ptr<EventLoopThreadPool> threadPool() // 线程池对象
  { return threadPool_; }

  /// Starts the server if it's not listening.
  ///
  /// It's harmless to call it multiple times.
  /// Thread safe.
  void start();

  // 设置回调函数, 用户在httpserver中手动设置
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; } // 连接完成回调函数
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }  // 通信回调函数
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }  // 写毕回调函数

 private:
  /// Not thread safe, but in loop, 新的连接
  void newConnection(int sockfd, const InetAddress& peerAddr);
  /// Thread safe.
  void removeConnection(const TcpConnectionPtr& conn);
  /// Not thread safe, but in loop
  void removeConnectionInLoop(const TcpConnectionPtr& conn);
  typedef std::map<string, TcpConnectionPtr> ConnectionMap; // string到tcpconnection的映射

  EventLoop* loop_;  // the acceptor loop
  const string ipPort_;
  const string name_;

  std::unique_ptr<Acceptor> acceptor_; // avoid revealing Acceptor
  std::shared_ptr<EventLoopThreadPool> threadPool_; // eventloopthread线程池

  // 回调函数, 对应于TcpConnection, 注册到Channel
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  ThreadInitCallback threadInitCallback_;
  AtomicInt32 started_;

  int nextConnId_;  // 下一个连接
  ConnectionMap connections_;   // map维护的connections, 包含若干TcpConnection
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_TCPSERVER_H_
