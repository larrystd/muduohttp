// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

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
  : loop_(loop),
    ipPort_(listenAddr.toIpPort()),
    name_(nameArg),
    /// 初始化acceptor监听连接,(调用listen(才开始监听)
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
    /// 初始化threadPool
    threadPool_(new EventLoopThreadPool(loop, name_)),

    /// 初始化connection回调函数和message回调函数
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    nextConnId_(1)
{
  /// 设置acceptor对象的NewConnectionCallback为&TcpServer::newConnection

  /// 新连接一旦到达, 自动回调TcpServer::newConnection封装为connection
  acceptor_->setNewConnectionCallback(
      std::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

  for (auto& item : connections_)
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

void TcpServer::start()
{
  if (started_.getAndSet(1) == 0)
  {
    /// 线程池启动, 结果是创建若干线程执行loop, 这些线程都阻塞到里poll中。此外这些线程在自己内部创建loop对象
    threadPool_->start(threadInitCallback_);

    assert(!acceptor_->listening());

    /// 在eventloop进程(即主线程)中运行listen监听
    /// 新连接到来会调用&TcpServer::newConnection
    loop_->runInLoop(
        std::bind(&Acceptor::listen, get_pointer(acceptor_)));
  }
}

/// 一旦新连接到达，调用之
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
  // 这个loop_是主线程的
  loop_->assertInLoopThread();

  /// 这里根本不用处理线程池, 而是直接分配一个ioLoop指针, 就可以让对应的子线程自动处理对象
  EventLoop* ioLoop = threadPool_->getNextLoop(); // 从线程池中找到一个可用的ioLoop, 这个ioLoop指向的loop对象在子线程里, 子线程还阻塞在eventloop
  char buf[64];
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  /// 封装IP地址
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  
  // 用ioLoop, sockfd等构造TcpConnection对象, 用shared_ptr维护得到conn
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
  
  /// connections_是一个std::map<string, TcpConnectionPtr> ConnectionMap;
  connections_[connName] = conn;
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
      std::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe

  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn)); // 这里主线程通过处理loop(loop指向对象在子线程上), 就可以间接让loop所指loop对象的子线程自动执行任务
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  // FIXME: unsafe
  loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();
  /// 从TcpServer连接map中删除
  size_t n = connections_.erase(conn->name());
  (void)n;
  assert(n == 1);
  /// 得到ioLoop
  EventLoop* ioLoop = conn->getLoop();
  /// 执行TcpConnectio的connectDestroyed
  ioLoop->queueInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn));
}

