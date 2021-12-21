#ifndef MUDUO_NET_BUFFER_H_
#define MUDUO_NET_BUFFER_H_

#include "muduo/base/copyable.h"
#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"

#include "muduo/net/Endian.h"

#include <algorithm>
#include <vector>

#include <assert.h>
#include <string.h>

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
  static const size_t kCheapPrepend = 8;  // kCheapPrepend的大小, 可以方便可能存在的前面插入数据
  static const size_t kInitialSize = 1024;  // buf的总大小

  explicit Buffer(size_t initialSize = kInitialSize)   // 显示初始化, 用初始化的大小来构造Buffer
    : buffer_(kCheapPrepend + initialSize),
      readerIndex_(kCheapPrepend),  // 开始readIndex_和writerIndex_都在初始, 被写入后writerIndex后增长, 读取后readIndex_增长。当writeIndex=readIndex说明写入的都被读了。但是注意保证先写后读
      writerIndex_(kCheapPrepend)
  {
    assert(readableBytes() == 0);
    assert(writableBytes() == initialSize);
    assert(prependableBytes() == kCheapPrepend);
  }

  void swap(Buffer& rhs)  // Buffer swap对象
  {
    buffer_.swap(rhs.buffer_); // 交换内部的std::vector<char> buffer
    std::swap(readerIndex_, rhs.readerIndex_); // 交换索引
    std::swap(writerIndex_, rhs.writerIndex_);
  }
  size_t readableBytes() const   // 还可读的字节数
  { return writerIndex_ - readerIndex_; }
 
  size_t writableBytes() const // 返回可写的字节数
  { return buffer_.size() - writerIndex_; }

  size_t prependableBytes() const   // 读空间之前的prepend部分字节数
  { return readerIndex_; }
  
  const char* peek() const // peek() readerIndex_所处的索引(开始读的索引)
  { return begin() + readerIndex_; }

  // 在可读区域找CR LR \r\n, http报文不同段的分隔符
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
  // 寻找\n符号
  const char* findEOL() const
  {
    const void* eol = memchr(peek(), '\n', readableBytes());  // memchr 从peek()寻找, 返回一个指向匹配字节'\n'的指针
    return static_cast<const char*>(eol);
  }
  const char* findEOL(const char* start) const
  {
    assert(peek() <= start);
    assert(start <= beginWrite());
    const void* eol = memchr(start, '\n', beginWrite() - start);
    return static_cast<const char*>(eol);
  }

  void retrieve(size_t len) // 修改索引, 读后数据更新索引, 长度为len, 表示已经读了len长度
  {
    assert(len <= readableBytes());
    // readerIndex_前移len，
    if (len < readableBytes())
    {
      readerIndex_ += len;  // 修改可读索引
    }
    // 如果len == readableBytes(), 表示当前readerIndex已经完全读完, 因此重置readerIndex_
    else
    {
      retrieveAll();
    }
  }

  void retrieveUntil(const char* end) // 根据end修改索引
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

  void retrieveAll()  // 重置readerIndex_和writerIndex_
  {
    readerIndex_ = kCheapPrepend;
    writerIndex_ = kCheapPrepend;
  }

  string retrieveAllAsString()
  {

    return retrieveAsString(readableBytes());
  }

  string retrieveAsString(size_t len) // 从buffer读数据
  {
    assert(len <= readableBytes());
    string result(peek(), len); // 读取了len长度的字符串
    retrieve(len);  // 修改索引
    return result;
  }

  // 将可读区域封装成StringPiece返回
  StringPiece toStringPiece() const
  {
    return StringPiece(peek(), static_cast<int>(readableBytes()));
  }

  // 向buffer写数据，将data append()可写区域之后
  void append(const StringPiece& str)
  {
    append(str.data(), str.size());
  }

  void append(const char* data, size_t len) // 从写索引开始append数据
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

  void hasWritten(size_t len)   // 更新可写的索引
  {
    assert(len <= writableBytes());
    writerIndex_ += len;
  }

  void unwrite(size_t len)
  {
    assert(len <= readableBytes());
    writerIndex_ -= len;
  }

  /// Append int64_t using network endian
  void appendInt64(int64_t x) // 将整数写入, 注意需要将主机字节序转为网络字节序
  {
    /// 在添加数据前, 调用主机转网络字节序
    int64_t be64 = sockets::hostToNetwork64(x);
    append(&be64, sizeof be64);
  }

  /// Append int32_t using network endian
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
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  int64_t readInt64()
  {
    /// 需要转字节序
    int64_t result = peekInt64();
    retrieveInt64();
    return result;
  }

  /// Read int32_t from network endian
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

  /// Peek int64_t from network endian
  /// Require: buf->readableBytes() >= sizeof(int64_t)
  int64_t peekInt64() const // 从peek读取数据
  {
    assert(readableBytes() >= sizeof(int64_t));
    int64_t be64 = 0;
    ::memcpy(&be64, peek(), sizeof be64);
    //// 网络转主机字节
    return sockets::networkToHost64(be64);
  }

  /// Peek int32_t from network endian
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

  /// Prepend int64_t using network endian
  void prependInt64(int64_t x)
  {
    /// x 转换字节序
    int64_t be64 = sockets::hostToNetwork64(x);
    prepend(&be64, sizeof be64);
  }

  /// Prepend int32_t using network endian
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

  void prepend(const void* data, size_t len)   // 在readerIndex_前面，也就是prependable区域插入数据
  {
    assert(len <= prependableBytes());
    readerIndex_ -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d, d+len, begin()+readerIndex_);
  }

  void shrink(size_t reserve)   // 新开辟区域
  {
    Buffer other;
    other.ensureWritableBytes(readableBytes()+reserve);
    /// 可读空间加入到other中
    other.append(toStringPiece());
    /// 交换区域
    swap(other);
  }

  size_t internalCapacity() const   // 缓冲区全部容量
  {
    return buffer_.capacity();
  }

  /// Read data directly into buffer.
  /// It may implement with readv(2)
  ssize_t readFd(int fd, int* savedErrno);  // 从fd中读取数据到buffer中

 private:

  char* begin()   // 缓冲区起始地址
  { return &*buffer_.begin(); }

  const char* begin() const
  { return &*buffer_.begin(); }

  // 如可写空间不足，则开辟之，其实调用vector的resize函数
  void makeSpace(size_t len)
  {
    // 这种情况必须要重新开辟空间
    if (writableBytes() + prependableBytes() < len + kCheapPrepend)
    {
      // 另开辟足够大的可写空间
      buffer_.resize(writerIndex_+len);
    }
    else
    // 由于prependableBytes空间较大可以容纳写区域，这时候移动数据即可
    {
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
  std::vector<char> buffer_; // 缓冲区用vector作为底层容器

  // 当前读写索引
  size_t readerIndex_;
  size_t writerIndex_;

  static const char kCRLF[];
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_BUFFER_H
