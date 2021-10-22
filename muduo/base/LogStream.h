#ifndef MUDUO_BASE_LOGSTREAM_H_
#define MUDUO_BASE_LOGSTREAM_H_

#include <assert.h>
#include <string.h> // memcpy

#include "muduo/base/noncopyable.h"
#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"

namespace muduo
{

namespace detail
{
/// 日志缓冲区大小
const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000*1000;

// 日志定长缓冲区, 维护一个连续内存data_, 当前的地址cur_, 增加数据直接再cur_之后添加
// 操作cur_指针的函数, add(len), reset(), 清空data_函数bzero
// cookie是一个函数指针,
template <int SIZE>
class FixedBuffer : noncopyable
{
 public:
  FixedBuffer() : cur_(data_){
  }
  ~FixedBuffer() {
  }

  void append(const char* buf, size_t len) {
    if (implicit_cast<size_t>(avail()) > len){  // implicit_cast作用相当于static_cast
      memcpy(cur_, buf, len); /// 以字节为单位存储
      cur_ += len;
    } /// 空间足够
  }

  const char* data() const {
    return data_;
  }
  /// 使用的长度
  int length() const {
    return static_cast<int>(cur_ - data_);
  }

  char* current() {
    return cur_;
  }
  /// 还可以用的长度
  int avail() const {
    return static_cast<int>(end() - cur_);
  }

  void add(size_t len) {
    cur_ += len;
  }

  /// 重置
  void reset() {
    cur_ = data_;
  }
  void bzero() {
    memZero(data_, sizeof data_);
  }

  const char* debugString();

  string toString() const {
    return string(data_, length());
  }

  StringPiece toStringPiece() const {
    return StringPiece(data_, length());
  }

 private:
  const char* end() const {
    return data_ + sizeof(data_);
  }
  char data_[SIZE];
  char* cur_; 
};

}  // namespace detail


/// 日志流
class LogStream : noncopyable
{
  typedef LogStream self;
 public:
  // 创建kSmallBuffer的内存
  typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;

  /// LogStram对象重载operator<< 操作符
  LogStream& operator<<(bool v) {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }
  /// 重载不同的类型
  LogStream& operator<<(short);
  LogStream& operator<<(unsigned short);
  LogStream& operator<<(int);
  LogStream& operator<<(unsigned int);
  LogStream& operator<<(long);
  LogStream& operator<<(unsigned long);
  LogStream& operator<<(long long);
  LogStream& operator<<(unsigned long long);

  LogStream& operator<<(const void*);

  LogStream& operator<<(float v) {
    *this << static_cast<double>(v);  /// 调用operator<<(double)处理
    return *this;
  }

  LogStream& operator<<(double);
  LogStream& operator<<(char v) {
    buffer_.append(&v, 1);  /// 直接将char加入buffer_
    return *this;
  }
  

  LogStream& operator<<(const char* str) {
    if (str) {
      buffer_.append(str, strlen(str)); ///const char*容易处理, 当成连续的char
    }else{
      buffer_.append("(null)", 6);  /// 加入(null)
    }

    return *this;
  }

  LogStream& operator<<(const unsigned char* str) {
    return operator<<(reinterpret_cast<const char*>(str));  /// reinterpret_cast用于指针的重新的解释, 反正指针都是4个字节
  }

  LogStream& operator<<(const string& v) {  /// string改成const char*处理
    buffer_.append(v.c_str(), v.size());
    return *this;
  }

  self& operator<<(const StringPiece& v)
  {
    buffer_.append(v.data(), v.size());
    return *this;
  }

  self& operator<<(const Buffer& v)
  {
    *this << v.toStringPiece();
    return *this;
  }

  void append(const char* data, int len) {  ///流的append调用底层缓冲区的append
    buffer_.append(data, len);
  }
  /// 返回的引用一定是const&, 防止用户修改错误
  const Buffer& buffer() const {
    return buffer_;
  }
  void resetBuffer() {
    buffer_.reset();
  }

 private:
 /// 静态检测
  void staticCheck();

  template<typename T>
  void formatInteger(T);  /// 格式化

  /// 日志流对应的缓冲区, 来自FixBuffer
  Buffer buffer_; 
  ///static const, 可用类名调用的常量
  static const int kMaxNumericSize = 32;
};

/// 可以被LogStream<< 流输出的对象
class Fmt
{
 public:
  template<typename T>
  Fmt(const char* fmt, T val);  /// 模板构造

  const char* data() const {  /// 返回的引用, 指针加const防止变量被错误修改造成的错误
    return buf_;
  }
  int length() const {
    return length_;
  }
 private:
  char buf_[32];
  int length_;
};  // Fmt

/// LogStream 对象可以<<格式化输出fmt
inline LogStream& operator<<(LogStream& s, const Fmt& fmt)
{
  s.append(fmt.data(), fmt.length());
  return s;
}

// Format quantity n in SI units (k, M, G, T, P, E).
// The returned string is atmost 5 characters long.
// Requires n >= 0
string formatSI(int64_t n);

// Format quantity n in IEC (binary) units (Ki, Mi, Gi, Ti, Pi, Ei).
// The returned string is atmost 6 characters long.
// Requires n >= 0
string formatIEC(int64_t n);

}  // namespace muduo

#endif  // MUDUO_BASE_LOGSTREAM_H
