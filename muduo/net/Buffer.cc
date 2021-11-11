#include "muduo/net/Buffer.h"

#include "muduo/net/SocketsOps.h"

#include <errno.h>
#include <sys/uio.h>

using namespace muduo;
using namespace muduo::net;

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;

ssize_t Buffer::readFd(int fd, int* savedErrno) // 从fd的文件描述符写数据到buffer中(inputbuffer)
{
  // saved an ioctl()/FIONREAD call to tell how much to read
  char extrabuf[65536];
  struct iovec vec[2];
  const size_t writable = writableBytes();

  /// 设置批数据结构, vec维度为2
  vec[0].iov_base = begin()+writerIndex_; /// 可写地址
  vec[0].iov_len = writable;  /// 可写空间长度

  vec[1].iov_base = extrabuf; // 额外的缓冲区
  vec[1].iov_len = sizeof extrabuf;
  const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
  const ssize_t n = sockets::readv(fd, vec, iovcnt);  // 批数据处理

  if (n < 0)  
  {
    *savedErrno = errno;  // 错误消息
  }
  else if (implicit_cast<size_t>(n) <= writable)
  {
    writerIndex_ += n;  // 更新可写的索引位置
  }
  else
  {
    writerIndex_ = buffer_.size();  // buffer空间不够, 先设置writerIndex再append扩大空间
    append(extrabuf, n - writable);
  }
  return n;
}

