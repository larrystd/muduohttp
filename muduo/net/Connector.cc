#include "muduo/net/Connector.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/SocketsOps.h"

#include <errno.h>

using namespace muduo;
using namespace muduo::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)  // 构造函数, 用loop和server的i地址构造
  : loop_(loop),
    serverAddr_(serverAddr),
    connect_(false),
    state_(kDisconnected),  // 状态
    retryDelayMs_(kInitRetryDelayMs)
{
  LOG_DEBUG << "ctor[" << this << "]";
}

Connector::~Connector()
{
  LOG_DEBUG << "dtor[" << this << "]";
  assert(!channel_);
}

void Connector::start()
{
  connect_ = true;  /// 设置连接状态为true, 没连接上可以重试，一直没连接上重新设置为false
  loop_->runInLoop(std::bind(&Connector::startInLoop, this)); // 在loop所在线程中执行&Connector::startInLoop
}

void Connector::startInLoop()
{
  loop_->assertInLoopThread();   // 保证在loop所在的线程执行
  assert(state_ == kDisconnected);
  if (connect_) // 连接状态
  {
    connect();  // 执行connect()连接函数
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

void Connector::stop()
{
  connect_ = false;
  loop_->queueInLoop(std::bind(&Connector::stopInLoop, this));
}

void Connector::stopInLoop()
{
  loop_->assertInLoopThread();
  if (state_ == kConnecting)
  {
    setState(kDisconnected);
    int sockfd = removeAndResetChannel();
    retry(sockfd);
  }
}

/// 执行连接
void Connector::connect()
{
  int sockfd = sockets::createNonblockingOrDie(serverAddr_.family()); // 创建客户端socketfd
  int ret = sockets::connect(sockfd, serverAddr_.getSockAddr());  // 调用sockets connect, 返回状态码
  int savedErrno = (ret == 0) ? 0 : errno;  // errno存储错误信号

  switch (savedErrno)   // 注意socket是一次性的，一旦发生错误就不可恢复，只能关闭重来
  {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
      connecting(sockfd);     // 以上没问题，调用connecting封装连接到channel
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH: 
      retry(sockfd);     // 以上错误延时重试
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno; // 以上错误需要关闭sockfd
      sockets::close(sockfd); 
      break;

    default:
      LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);
      break;
  }
}

void Connector::restart() // 重新调用startInLoop()连接
{
  loop_->assertInLoopThread();
  setState(kDisconnected);
  retryDelayMs_ = kInitRetryDelayMs;
  connect_ = true;
  startInLoop();
}

void Connector::connecting(int sockfd) // 连接成功 将建立连接的sockfd封装成channel
{
  setState(kConnecting);  // 设置连接状态
  assert(!channel_);

  channel_.reset(new Channel(loop_, sockfd));   // channel用unique_ptr维护, 将loop, sockfd封装成channel对象
  channel_->setWriteCallback(
      std::bind(&Connector::handleWrite, this)); // 在channel_中设置写回调和错误回调
  channel_->setErrorCallback(
      std::bind(&Connector::handleError, this));

  channel_->enableWriting();  // 设置channel_可写, 并注册到epoll中
}

/// remove channel
int Connector::removeAndResetChannel()
{
  channel_->disableAll();
  channel_->remove();
  int sockfd = channel_->fd();
  // Can't reset channel_ here, because we are inside Channel::handleEvent
  loop_->queueInLoop(std::bind(&Connector::resetChannel, this)); // FIXME: unsafe
  return sockfd;
}

void Connector::resetChannel()
{
  channel_.reset(); // unique_str的reset, 析构channel
}

void Connector::handleWrite() // 写事件回调函数, 注意如果连接成功, 可写触发。但可写触发不一定表示真的连接成功
{
  LOG_TRACE << "Connector::handleWrite " << state_;

  if (state_ == kConnecting)
  {
    int sockfd = removeAndResetChannel(); /// channel被销毁
    int err = sockets::getSocketError(sockfd);
    if (err)  // err = 0说明没有错误
    {
      LOG_WARN << "Connector::handleWrite - SO_ERROR = "
               << err << " " << strerror_tl(err);
      retry(sockfd);
    }
    else if (sockets::isSelfConnect(sockfd))     // 判断自连接，即(sourceIp, sourcePort) = (desIp, desPort)的情况
    {
      LOG_WARN << "Connector::handleWrite - Self connect";
      retry(sockfd);   // 重新连接
    }
    else
    {
      setState(kConnected);     // 这回是真的连接成功
      if (connect_)
      {
        /// 连接回调函数
        newConnectionCallback_(sockfd);
      }
      else
      {
        sockets::close(sockfd);
      }
    }
  }
  else
  {
    // what happened?
    assert(state_ == kDisconnected);
  }
}

void Connector::handleError() // 错误回调函数
{
  LOG_ERROR << "Connector::handleError state=" << state_;
  if (state_ == kConnecting)
  {
    int sockfd = removeAndResetChannel();
    int err = sockets::getSocketError(sockfd);
    LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
    retry(sockfd);
  }
}

void Connector::retry(int sockfd) // 重新连接
{
  sockets::close(sockfd);   // 先关闭sockfd
  setState(kDisconnected);
  if (connect_) /// 定时重试, 通过定时器完成
  {
    LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
             << " in " << retryDelayMs_ << " milliseconds. ";
    loop_->runAfter(retryDelayMs_/1000.0,
                    std::bind(&Connector::startInLoop, shared_from_this()));  // 加入到自己的eventloop中等待被执行, 定时触发任务
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

