#include "muduo/net/Acceptor.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

using namespace muduo;
using namespace muduo::net;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
  : loop_(loop),
    acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),  // 监听socket对象
    acceptChannel_(loop, acceptSocket_.fd()), // 监听通道
    listening_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))  // /dev/null的fd
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true);
  acceptSocket_.setReusePort(reuseport);

  acceptSocket_.bindAddress(listenAddr);  // 绑定地址
  acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this)); // 设置监听channel的可读回调函数
}

Acceptor::~Acceptor()
{
  acceptChannel_.disableAll();  // 监听通道disable
  acceptChannel_.remove();  // 移除监听
  ::close(idleFd_);
}

void Acceptor::listen()
{
  loop_->assertInLoopThread();  // 在loop所在的线程执行
  listening_ = true;

  acceptSocket_.listen(); // 调用监听socket的listen()
  acceptChannel_.enableReading(); // accept通道将可读事件注册到poll(loop所在的), 可以被poll_wait接收响应(accept)
}

// 该函数注册到accept channel的可读回调, 一旦可读说明有连接到来, 通过handleRead处理连接
void Acceptor::handleRead()
{
  loop_->assertInLoopThread(); // 在loop线程中执行
  InetAddress peerAddr;
  int connfd = acceptSocket_.accept(&peerAddr); // accept连接, 返回fd和peerAddr
  if (connfd >= 0) {
    if (newConnectionCallback_) // 连接回调函数,就是创建连接TcpConnection提供的
    {
      newConnectionCallback_(connfd, peerAddr);
    }
    else
    {
      sockets::close(connfd); // 直接连接关闭
    }
  }else { // accept失败
      LOG_SYSERR << "in Acceptor::handleRead";
      // Read the section named "The special problem of
      // accept()ing when you can't" in libev's doc.
      // By Marc Lehmann, author of libev.
      if (errno == EMFILE)  // errno中有错误信息
      {
        ::close(idleFd_);
        idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
        ::close(idleFd_);
        idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
      }
  }

}