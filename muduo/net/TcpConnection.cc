#include "muduo/net/TcpConnection.h"

#include <errno.h>

#include "muduo/base/Logging.h"
#include "muduo/base/WeakCallback.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Socket.h"
#include "muduo/net/SocketsOps.h"

using namespace muduo;
using namespace muduo::net;

void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)  // 默认连接回调函数
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
  // do not call conn->forceClose(), because some users want to register message callback only.
}

void muduo::net::defaultMessageCallback(const TcpConnectionPtr&,
                                        Buffer* buf,
                                        Timestamp)  // 默认信息传来回调函数, 传入TcpConnectionPtr, buf, 时间戳, 将可读信息写入buf中
{
  buf->retrieveAll(); 
}

TcpConnection::TcpConnection(EventLoop* loop,
                             const string& nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr) // 构造函数, 用所属loop对象指针, sockfd, Addr
  : loop_(loop), // loop指针
    name_(nameArg),
    state_(kConnecting),
    reading_(true),
    socket_(new Socket(sockfd)),  // 用sockfd创建Socket
    channel_(new Channel(loop, sockfd)), // 用loop指针和sockfd创建Channel
    localAddr_(localAddr),
    peerAddr_(peerAddr),
    highWaterMark_(64*1024*1024)
{
  // tcpconnection的回调函数注册到channel中
  channel_->setReadCallback(
      std::bind(&TcpConnection::handleRead, this, _1)); // 设置Channel可读回调函数&TcpConnection::handleRead
  channel_->setWriteCallback(
      std::bind(&TcpConnection::handleWrite, this));  // Channel可写回调函数&TcpConnection::handleWrite
  
  channel_->setCloseCallback(
      std::bind(&TcpConnection::handleClose, this));  // Channel关闭回调函数&TcpConnection::handleClose
  channel_->setErrorCallback(
      std::bind(&TcpConnection::handleError, this));  // Channel错误回调函数
  LOG_DEBUG << "TcpConnection::ctor[" <<  name_ << "] at " << this
            << " fd=" << sockfd;
  socket_->setKeepAlive(true);  // 设置socket_为keepAlive
}

TcpConnection::~TcpConnection()
{
  LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << " fd=" << channel_->fd()
            << " state=" << stateToString();
  assert(state_ == kDisconnected);
}

bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const
{
  return socket_->getTcpInfo(tcpi); // 调用socket_得到tcp的信息储存在tcp_info中
}

string TcpConnection::getTcpInfoString() const
{
  char buf[1024];
  buf[0] = '\0';
  socket_->getTcpInfoString(buf, sizeof buf); // tcp信息放入buf中返回
  return buf;
}

void TcpConnection::send(const void* data, int len) // send函数
{
  send(StringPiece(static_cast<const char*>(data), len));
}

void TcpConnection::send(const StringPiece& message) /// 发送信息， StringPiece格式
{
  if (state_ == kConnected) // 如果TcpConnection已经连接
  {
    if (loop_->isInLoopThread())  // 在loop所在线程运行
    {
      sendInLoop(message);  // 调用sendInLoop发送
    }
    else
    {
      void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop; // 函数指针, 指向传入message函数, &TcpConnection::sendInLoop
      loop_->runInLoop(
          std::bind(fp,
                    this, 
                    message.as_string()));  // 加入到loop所在线程的等待队列
    }
  }
}

void TcpConnection::send(Buffer* buf) // 发送字符串到Channel(fd)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(buf->peek(), buf->readableBytes());  // 调用sendInLoop发送信息
      buf->retrieveAll();   // 重置buf
    }
    else
    {
      void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop; // 函数指针运行fp
      loop_->runInLoop(
          std::bind(fp,
                    this, 
                    buf->retrieveAllAsString()));
    }
  }
}

void TcpConnection::sendInLoop(const StringPiece& message)  // 调用TcpConnection::sendInLoop(const void* data, size_t len)
{
  sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const void* data, size_t len)  // 发送data数据到fd write缓冲区
{
  loop_->assertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool faultError = false;
  if (state_ == kDisconnected)
  {
    LOG_WARN << "disconnected, give up writing";
    return;
  }
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) // channel通道没有在写, 且没有要读的字节(读写索引一致)
  {
    nwrote = sockets::write(channel_->fd(), data, len); // 直接调用socket::write向channel_的fd写data数据

    if (nwrote >= 0)  //  写成功, 写了nwrote个字节
    {
      remaining = len - nwrote; // 是否有没写完的字节
      if (remaining == 0 && writeCompleteCallback_) // 全部字节已经写完
      {
        loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this())); // 调用写毕回调, 将writeCompleteCallback_回调函数加入所属loop队列的中, 唤醒子线程执行
      }
    }
    else // nwrote < 0, 写入失败
    {
      nwrote = 0;
      if (errno != EWOULDBLOCK) //是否有EWOULDBLOCK错误
      {
        LOG_SYSERR << "TcpConnection::sendInLoop";
        if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
        {
          faultError = true;
        }
      }
    }
  }
  assert(remaining <= len);
  if (!faultError && remaining > 0) // 这可能是有要读的字节, 或者sockets::write一次没写完
  {
    size_t oldLen = outputBuffer_.readableBytes();  // outputBuffer的可读字节数(TcpConnection去写)
    if (oldLen + remaining >= highWaterMark_
        && oldLen < highWaterMark_
        && highWaterMarkCallback_)
    {
      loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
    }

    outputBuffer_.append(static_cast<const char*>(data)+nwrote, remaining);     // 没写完的字节remaining放入outputbuffer中, 先不调用write
    if (!channel_->isWriting())
    {
      channel_->enableWriting();  // TcpConnection设置channel可写监听(能写了好调用handleWrite继续写) 
    }
  }
}
void TcpConnection::shutdown() // TcpConnection执行&TcpConnection::shutdownInLoop
{
  if (state_ == kConnected)
  {
    setState(kDisconnecting);
    loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
  }
}
void TcpConnection::shutdownInLoop()
{
  loop_->assertInLoopThread();
  if (!channel_->isWriting()) // channel不在写
  {
    // we are not writing
    socket_->shutdownWrite(); // 关闭写
  }
}

void TcpConnection::forceClose() // 强制关闭, 执行&TcpConnection::forceCloseInLoop
{ 
  // FIXME: use compare and swap
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    setState(kDisconnecting);
    loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
  }
}

void TcpConnection::forceCloseWithDelay(double seconds)
{
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    setState(kDisconnecting);
    loop_->runAfter(
        seconds,
        makeWeakCallback(shared_from_this(),
                         &TcpConnection::forceClose));  // not forceCloseInLoop to avoid race condition
  }
}

void TcpConnection::forceCloseInLoop()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    handleClose();  // 关闭处理函数
  }
}

const char* TcpConnection::stateToString() const
{
  switch (state_)
  {
    case kDisconnected:
      return "kDisconnected";
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    default:
      return "unknown state";
  }
}

void TcpConnection::setTcpNoDelay(bool on)
{
  socket_->setTcpNoDelay(on);
}

void TcpConnection::startRead() // 开始读
{
  loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop, this));
}

void TcpConnection::startReadInLoop()
{
  loop_->assertInLoopThread();
  if (!reading_ || !channel_->isReading())   // 设置channel可读
  {
    channel_->enableReading();
    reading_ = true;
  }
}

void TcpConnection::stopRead()
{
  loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, this));
}

void TcpConnection::stopReadInLoop()
{
  loop_->assertInLoopThread();
  if (reading_ || channel_->isReading())
  {
    channel_->disableReading();
    reading_ = false;
  }
}

// master线程将该方法放入工作线程的队列, 使工作线程执行该方法, 将连接事件注册到epoll 红黑树中, 并设置连接回调函数
void TcpConnection::connectEstablished()  // Tcp连接建立, Acceptor的accept成功后自动调用
{
  loop_->assertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);
  channel_->tie(shared_from_this());  // channel绑定到Connection,实现channel到Connection的调用
  channel_->enableReading();  // 设置channel_监听可读事件, 注册channel到poll中。可读可写回调函数在创建TcpConnection的时候就已注册, enableReading只是将fd注入
  connectionCallback_(shared_from_this());   // 连接回调函数
}

void TcpConnection::connectDestroyed() // 连接销毁， 关闭channel
{ 
  loop_->assertInLoopThread();

  if (state_ == kConnected)
  {
    setState(kDisconnected);
    channel_->disableAll();     // 关闭channel, 设置channel为空事件
    connectionCallback_(shared_from_this());
  }
  channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime) // handleRead, Channel可读事件后调用这个函数。该函数先read数据到缓冲区, 再调用合理的messageCallback_处理函数
{
  loop_->assertInLoopThread();
  int savedErrno = 0;
  // 事件可读, 自动读取channel_->fd()的数据到inputBuffer_中
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0)
  {
    // messageCallback_是用户传入的数据读取函数, 基于当前inputBuffer_进行数据解析操作
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  }
  else if (n == 0) // 没有字节说明需要关闭连接
  {
    handleClose();
  }
  else
  {
    errno = savedErrno; // 出现错误(n<0 ??)
    LOG_SYSERR << "TcpConnection::handleRead";
    handleError();
  }
}

void TcpConnection::handleWrite() // Channel可写事件触发后会回调函数, (用户将数据写到了outputbuffer), 将outputbuffer数据发给对面
{
  loop_->assertInLoopThread();
  if (channel_->isWriting())   // channel可写事件触发
  {
    // 这里的sockets::write实际是从读索引后移
    ssize_t n = sockets::write(channel_->fd(),
                               outputBuffer_.peek(),
                               outputBuffer_.readableBytes());  // 向channel->fd写入outputBuffer的数据, 位于peek(读索引处),大小为readableBytes
    if (n > 0)
    {
      outputBuffer_.retrieve(n);  // 更新outputBuffer_读写索引
      if (outputBuffer_.readableBytes() == 0) // 没有可读的了
      {
        channel_->disableWriting(); // 设置channel不可写监听(因为已经写完了)
        if (writeCompleteCallback_)
        {
          loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));  // 执行写毕回调函数
        }
        if (state_ == kDisconnecting)
        {
          shutdownInLoop();
        }
      }
    }
    else
    {
      LOG_SYSERR << "TcpConnection::handleWrite";
    }
  }
  else
  {
    LOG_TRACE << "Connection fd = " << channel_->fd()
              << " is down, no more writing";
  }
}

void TcpConnection::handleClose() // 关闭连接的事件回调函数
{
  loop_->assertInLoopThread();
  LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
  assert(state_ == kConnected || state_ == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  setState(kDisconnected);
  /// 关闭channel通道
  channel_->disableAll();

  TcpConnectionPtr guardThis(shared_from_this());
  connectionCallback_(guardThis);
  // must be the last line
  closeCallback_(guardThis);
}

void TcpConnection::handleError() // 错误回调函数
{
  int err = sockets::getSocketError(channel_->fd());
  LOG_ERROR << "TcpConnection::handleError [" << name_
            << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

