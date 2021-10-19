// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/Acceptor.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

/// 初始化包括两部分, 服务端socket有两种，(1)建立服务端监听socket
/// (2) 设置服务端连接channel, 并设置可读回调函数。一旦监听到socket，可读，即调用回调函数
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
  : loop_(loop),
  /// 创建一个监听socket 即acceptSocket_对象, 封装成acceptChannel_对象
    acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),
    acceptChannel_(loop, acceptSocket_.fd()),
    listening_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true);
  acceptSocket_.setReusePort(reuseport);

  //// 监听地址
  acceptSocket_.bindAddress(listenAddr);
  // 该通道可读的回调函数,loop中调用。一旦该通道可读即调用
  acceptChannel_.setReadCallback(
      std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
  acceptChannel_.disableAll();
  acceptChannel_.remove();
  ::close(idleFd_);
}

/// 监听客户端socket
void Acceptor::listen()
{
  loop_->assertInLoopThread();
  listening_ = true;

  /// listen函数使用主动连接套接口变为被连接套接口，使得一个进程可以接受其它进程的请求，从而成为一个服务器进程。
  /// listen把连接放到一个队列中, 当服务器端accept了，就从listen的队列中取出一个连接
  acceptSocket_.listen();
  // 设置acceptChannel_监听连接通道可读, 注册到loop的poller对象里
  // listen相当于epoll_wait系统调用, 当有链接服务器执行回调, 没有则等待。服务器是消费者而listen可以视为生产者。
  acceptChannel_.enableReading(); 
}

/// 可读回调函数，建立连接。一旦poller到acceptChannel_活跃，会执行该回调函数
void Acceptor::handleRead()
{
  loop_->assertInLoopThread();
  InetAddress peerAddr;
  //接受连接, 并得到一个connfd
  int connfd = acceptSocket_.accept(&peerAddr); // accept socket
  if (connfd >= 0)
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (newConnectionCallback_)
    {

      /// 得到connfd之后, 调用newConnectionCallback_处理connfd
      newConnectionCallback_(connfd, peerAddr);
    }
    else
    {
      sockets::close(connfd);
    }
  }
  else
  {
    LOG_SYSERR << "in Acceptor::handleRead";
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of libev.
    if (errno == EMFILE)
    {
      ::close(idleFd_);
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}

