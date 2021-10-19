// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_BUFFER_H
#define MUDUO_NET_BUFFER_H

#include "muduo/base/copyable.h"
#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"

#include "muduo/net/Endian.h"

#include <algorithm>
#include <vector>

#include <assert.h>
#include <string.h>
//#include <unistd.h>  // ssize_t
/// 缓冲buffer的设计
namespace muduo
{
namespace net
{

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
class Buffer : public muduo::copyable
{
 public:
 // kCheapPrepend不为0, 可以方便可能存在的前面插入数据
  static const size_t kCheapPrepend = 8;
  static const size_t kInitialSize = 1024;

  // 显示初始化
  explicit Buffer(size_t initialSize = kInitialSize)
    : buffer_(kCheapPrepend + initialSize),
    /// 开始readIndex_和writerIndex_都在初始, 被写入后writerIndex后增长, 读取后readIndex_增长
      readerIndex_(kCheapPrepend),
      writerIndex_(kCheapPrepend)
  {
    assert(readableBytes() == 0);
    assert(writableBytes() == initialSize);
    assert(prependableBytes() == kCheapPrepend);
  }

  // swap Buffer rhs
  void swap(Buffer& rhs)
  {
    /// 交换空间
    buffer_.swap(rhs.buffer_);
    /// 交换索引
    std::swap(readerIndex_, rhs.readerIndex_);
    std::swap(writerIndex_, rhs.writerIndex_);
  }
  // 可读的字节数
  size_t readableBytes() const
  { return writerIndex_ - readerIndex_; }
  // 可写的字节数
  size_t writableBytes() const
  { return buffer_.size() - writerIndex_; }
  // 前空间字节数
  size_t prependableBytes() const
  { return readerIndex_; }


  // peek() readerIndex_所处的地址
  const char* peek() const
  { return begin() + readerIndex_; }

  
  // 找CR LR \r\n
  // 在可读区域寻找 \r\n
  const char* findCRLF() const
  {
    const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
    return crlf == beginWrite() ? NULL : crlf;
  }

  const char* findCRLF(const char* start) const
  {
    assert(peek() <= start);
    assert(start <= beginWrite());
    /// std::search 序列[first1，last1)中搜索由[first2，last2)定义的子序列的第一个匹配项
    const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
    return crlf == beginWrite() ? NULL : crlf;
  }
  // 寻找
  const char* findEOL() const
  {
    const void* eol = memchr(peek(), '\n', readableBytes());
    return static_cast<const char*>(eol);
  }
  // memchr 返回一个指向匹配字节的指针
  // 寻找\n
  const char* findEOL(const char* start) const
  {
    assert(peek() <= start);
    assert(start <= beginWrite());
    const void* eol = memchr(start, '\n', beginWrite() - start);
    return static_cast<const char*>(eol);
  }

  // retrieve 读后数据更新索引, 长度为len, 表示已经读了len长度
  // 调用retrieveAll， 用kCheapPrepend重新设置readerIndex_和writerIndex_
  // 读后若len < readableBytes(), 后移动readerIndex_

  //  若len = readableBytes， 说明缓冲区数据全部读完，重置索引为初值
  void retrieve(size_t len)
  {
    assert(len <= readableBytes());
    // readerIndex_前移len，
    if (len < readableBytes())
    {
      readerIndex_ += len;
    }
    // 如果len == readableBytes(), 表示当前readerIndex已经完全读完, 因此重置readerIndex_
    else
    {
      retrieveAll();
    }
  }

  void retrieveUntil(const char* end)
  {
    assert(peek() <= end);
    assert(end <= beginWrite());
    retrieve(end - peek());
  }

  void retrieveInt64()
  {
    retrieve(sizeof(int64_t));
  }

  void retrieveInt32()
  {
    retrieve(sizeof(int32_t));
  }

  void retrieveInt16()
  {
    retrieve(sizeof(int16_t));
  }

  void retrieveInt8()
  {
    retrieve(sizeof(int8_t));
  }

  void retrieveAll()
  {
    readerIndex_ = kCheapPrepend;
    writerIndex_ = kCheapPrepend;
  }

  string retrieveAllAsString()
  {

    return retrieveAsString(readableBytes());
  }

  string retrieveAsString(size_t len)
  {
    assert(len <= readableBytes());
    string result(peek(), len);

    // 读了一些数据, 需要把readerIndex移动到未读的区域
    retrieve(len);
    return result;
  }

  // 将可读区域封装成StringPiece返回
  StringPiece toStringPiece() const
  {
    return StringPiece(peek(), static_cast<int>(readableBytes()));
  }

  // 写数据，将data append()可写区域之后
  void append(const StringPiece& str)
  {
    append(str.data(), str.size());
  }

  void append(const char* data, size_t len)
  {
    // 这行保证是可写的，有可写空间对于len来说
    ensureWritableBytes(len);
    std::copy(data, data+len, beginWrite());
    hasWritten(len);
  }

  void append(const void* data, size_t len)
  {
    append(static_cast<const char*>(data), len);
  }
  // 确保有可写空间，没有的话开辟之
  void ensureWritableBytes(size_t len)
  {
    // 空间不够，开辟空间
    if (writableBytes() < len)
    {
      makeSpace(len);
    }
    assert(writableBytes() >= len);
  }
  // 开始可写的地址, 也就是writeIndex_
  char* beginWrite()
  { return begin() + writerIndex_; }

  const char* beginWrite() const
  { return begin() + writerIndex_; }
  
  // 更新可写的索引
  void hasWritten(size_t len)
  {
    assert(len <= writableBytes());
    writerIndex_ += len;
  }

  void unwrite(size_t len)
  {
    assert(len <= readableBytes());
    writerIndex_ -= len;
  }

  ///
  /// Append int64_t using network endian
  ///
  void appendInt64(int64_t x)
  {
    /// 在添加数据前, 调用主机转网络字节序
    int64_t be64 = sockets::hostToNetwork64(x);
    append(&be64, sizeof be64);
  }

  ///
  /// Append int32_t using network endian
  ///
  void appendInt32(int32_t x)
  {
    int32_t be32 = sockets::hostToNetwork32(x);
    append(&be32, sizeof be32);
  }

  void appendInt16(int16_t x)
  {
    int16_t be16 = sockets::hostToNetwork16(x);
    append(&be16, sizeof be16);
  }

  void appendInt8(int8_t x)
  {
    append(&x, sizeof x);
  }

  /// 读数据，从readableBytes读取一个整数
  /// Read int64_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)

  int64_t readInt64()
  {
    /// 需要转字节序
    int64_t result = peekInt64();
    retrieveInt64();
    return result;
  }

  ///
  /// Read int32_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  int32_t readInt32()
  {
    int32_t result = peekInt32();
    retrieveInt32();
    return result;
  }

  int16_t readInt16()
  {
    int16_t result = peekInt16();
    retrieveInt16();
    return result;
  }

  int8_t readInt8()
  {
    int8_t result = peekInt8();
    retrieveInt8();
    return result;
  }

  ///
  /// Peek int64_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int64_t)
  /// 读数据
  int64_t peekInt64() const
  {
    assert(readableBytes() >= sizeof(int64_t));
    int64_t be64 = 0;
    ::memcpy(&be64, peek(), sizeof be64);
    //// 网络转主机字节
    return sockets::networkToHost64(be64);
  }

  ///
  /// Peek int32_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  int32_t peekInt32() const
  {
    assert(readableBytes() >= sizeof(int32_t));
    int32_t be32 = 0;
    ::memcpy(&be32, peek(), sizeof be32);
    return sockets::networkToHost32(be32);
  }

  int16_t peekInt16() const
  {
    assert(readableBytes() >= sizeof(int16_t));
    int16_t be16 = 0;
    ::memcpy(&be16, peek(), sizeof be16);
    return sockets::networkToHost16(be16);
  }

  int8_t peekInt8() const
  {
    assert(readableBytes() >= sizeof(int8_t));
    int8_t x = *peek();
    return x;
  }

  ///
  /// Prepend int64_t using network endian
  ///
  void prependInt64(int64_t x)
  {
    /// x 转换字节序
    int64_t be64 = sockets::hostToNetwork64(x);
    prepend(&be64, sizeof be64);
  }

  ///
  /// Prepend int32_t using network endian
  ///
  void prependInt32(int32_t x)
  {
    int32_t be32 = sockets::hostToNetwork32(x);
    prepend(&be32, sizeof be32);
  }

  void prependInt16(int16_t x)
  {
    int16_t be16 = sockets::hostToNetwork16(x);
    prepend(&be16, sizeof be16);
  }

  void prependInt8(int8_t x)
  {
    prepend(&x, sizeof x);
  }
  // 在readerIndex_前面，也就是prependable区域插入数据
  void prepend(const void* data, size_t len)
  {
    assert(len <= prependableBytes());
    readerIndex_ -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d, d+len, begin()+readerIndex_);
  }

  // 新开辟区域
  void shrink(size_t reserve)
  {
    Buffer other;
    other.ensureWritableBytes(readableBytes()+reserve);
    /// 可读空间加入到other中
    other.append(toStringPiece());
    /// 交换区域
    swap(other);
  }
  // 缓冲区全部容量
  size_t internalCapacity() const
  {
    return buffer_.capacity();
  }

  /// Read data directly into buffer.
  ///
  /// It may implement with readv(2)
  /// @return result of read(2), @c errno is saved
  ssize_t readFd(int fd, int* savedErrno);

 private:
  // 缓冲区起始地址
  char* begin()
  { return &*buffer_.begin(); }

  const char* begin() const
  { return &*buffer_.begin(); }

  // 如可写空间不足，则开辟之，其实调用vector的resize函数
  void makeSpace(size_t len)
  {
    // 这种情况必须要重新开辟空间
    if (writableBytes() + prependableBytes() < len + kCheapPrepend)
    {
      // FIXME: move readable data
      // 另开辟足够大的可写空间
      buffer_.resize(writerIndex_+len);
    }
    else
    // 由于prependableBytes空间较大可以容纳写区域，这时候移动数据即可
    {
      // move readable data to the front, make space inside buffer
      assert(kCheapPrepend < readerIndex_);
      size_t readable = readableBytes();
      // std::copy(start, end, container.begin());
      // 将当前的readerIndex_移动到初始化的kCheapPrepend，从而给后面空下空间
      std::copy(begin()+readerIndex_,
                begin()+writerIndex_,
                begin()+kCheapPrepend);
      readerIndex_ = kCheapPrepend;
      writerIndex_ = readerIndex_ + readable;
      assert(readable == readableBytes());
    }
  }

 private:
 // 缓冲区用vector作为底层容器
  std::vector<char> buffer_;

  /// 当前读写索引
  size_t readerIndex_;
  size_t writerIndex_;

  static const char kCRLF[];
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_BUFFER_H
