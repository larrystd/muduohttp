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

  /// 新连接一旦到达, 自动回调TcpServer::newConnection封装为connection, 
  /// 将该channel fd绑定到执行线程, 负责处理该连接。
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
    /// 线程池启动, 创建若干线程执行loop.
    /// thread对象和loop对象均存储在threadPool对应的列表中
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
  loop_->assertInLoopThread();
  /// 来了一个连接, 分配一个固定eventloop thread处理之
  /// 连接绑定eventloop, eventloop绑定thread, 因此连接绑定了thread
  /// 这样就不会线程静态了, 因为固定的连接由固定的线程处理
  /// 主线程的作用只是刚开始建立连接，以后的处理通话等由特定的工作线程进行

  /// 返回threadPool_ loop列表的下一个loop, (每个loop来自不同线程)
  EventLoop* ioLoop = threadPool_->getNextLoop();
  char buf[64];
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  /// 封装IP地址
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  
  // 封装新连接到TcpConnection对象(ioLoop,sockfd, addr)，用std::shared_ptr<TcpConnection>维护，储存到map中 connections_[connName] = conn
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
  
  /// connections_回调函数, 主要是处理信息传递的回调函数
  connections_[connName] = conn;
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
      std::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe

  // 在ioLoop的线程(创建loop的子线程)中执行&TcpConnection::connectEstablished, 
  // 连接建立主要是注册channel到ioLoop 的poller
  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
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

