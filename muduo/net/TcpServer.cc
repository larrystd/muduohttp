#include "muduo/net/TcpServer.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Acceptor.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/SocketsOps.h"

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const string& nameArg,
                     Option option)
  : loop_(loop),  // tcpserver的主loop
    ipPort_(listenAddr.toIpPort()),
    name_(nameArg),
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)), // 初始化acceptor监听socket,(调用listen(才开始监听)
    threadPool_(new EventLoopThreadPool(loop, name_)),     // 构造threadPool对象
    /// 初始化connection回调函数和message回调函数
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    nextConnId_(1)
{
  acceptor_->setNewConnectionCallback(
      std::bind(&TcpServer::newConnection, this, _1, _2)); // 新连接一旦到达, 自动回调TcpServer::newConnection封装为connection
}

TcpServer::~TcpServer()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

  for (auto& item : connections_) // 每一个TcpConnection, 一个server可用有多个tcpConnection
  {
    TcpConnectionPtr conn(item.second);
    item.second.reset();
    conn->getLoop()->runInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn));
  }
}

void TcpServer::setThreadNum(int numThreads)
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() // server创建线程池loop, 运行监听
{
  if (started_.getAndSet(1) == 0)
  { 
    threadPool_->start(threadInitCallback_); // 线程池启动, 结果是创建若干线程, 每个线程创建loop对象, 子线程执行loop()阻塞到里poll中
    assert(!acceptor_->listening());
    loop_->runInLoop(
        std::bind(&Acceptor::listen, get_pointer(acceptor_)));  // 主线程运行&Acceptor::listen监听, 新连接到来会调用&TcpServer::newConnection创建TcpConnection对象
  }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)  // 新连接到来会调用该函数, 基于sockfd
{
  loop_->assertInLoopThread();   // 这个loop_是主线程的, 主线程执行
  // 这里根本不用处理线程池, 而是直接分配一个ioLoop指针, 就可以让对应的子线程自动处理对象
  EventLoop* ioLoop = threadPool_->getNextLoop(); // 从线程池中找到一个可用的ioLoop, 这个ioLoop指向的loop对象在子线程里, 子线程还阻塞在eventloop的epoll_wait
  char buf[64];
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);  
  ++nextConnId_;
  string connName = name_ + buf;  // 创建connName

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  InetAddress localAddr(sockets::getLocalAddr(sockfd));   // 封装IP地址

  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr)); // 来了一个连接就创建一个TcpConnection,用子线程的loop指针, 该对象用shared_ptr维护
  
  connections_[connName] = conn;  // coonection name->conn的map
  conn->setConnectionCallback(connectionCallback_); // 设置tcpconnection的连接回调函数, 来自用户自定义。以下同样
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
      std::bind(&TcpServer::removeConnection, this, _1));

  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn)); // 主线程将&TcpConnection::connectEstablished加入到子线程的loop对象的任务队列中, 使子线程自动执行任务
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));  // tcpserver移除该连接
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();  
  size_t n = connections_.erase(conn->name());  // tcpconnection从tcpserver的连接map中擦除
  (void)n;
  assert(n == 1);
  EventLoop* ioLoop = conn->getLoop();  // tcpconnection所属的loop
  ioLoop->queueInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn)); // 在loop所属的线程中执行&TcpConnection::connectDestroyed关闭tcpconnection
}

