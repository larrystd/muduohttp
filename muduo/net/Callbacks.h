// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_CALLBACKS_H_
#define MUDUO_NET_CALLBACKS_H_

#include <functional>
#include <memory>

#include "muduo/base/Timestamp.h"

namespace muduo
{
// 回调函数类

// 使用三个placeholders::, 用在bind中
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

// should really belong to base/Types.h, but <memory> is not included there.

template<typename T>
inline T* get_pointer(const std::shared_ptr<T>& ptr)  // 返回shared_ptr内部的裸指针
{
  return ptr.get();
}

template<typename T>
inline T* get_pointer(const std::unique_ptr<T>& ptr)  // 返回unique_ptr内部的裸指针
{
  return ptr.get();
}

// Adapted from google-protobuf stubs/common.h
// see License in muduo/base/Types.h
template<typename To, typename From>
inline ::std::shared_ptr<To> down_pointer_cast(const ::std::shared_ptr<From>& f) {  // 转型, From类型到To类型
  if (false)
  {
    implicit_cast<From*, To*>(0);
  }

#ifndef NDEBUG
  assert(f == NULL || dynamic_cast<To*>(get_pointer(f)) != NULL);
#endif
  return ::std::static_pointer_cast<To>(f);
}

namespace net
{

// All client visible callbacks go here.

class Buffer;
class TcpConnection;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
typedef std::function<void()> TimerCallback;  // 定时器回调


typedef std::function<void (const TcpConnectionPtr&)> ConnectionCallback; // 连接回调函数std::function<void (const TcpConnectionPtr&)>

typedef std::function<void (const TcpConnectionPtr&)> CloseCallback;  // 关闭连接回调函数function<void (const TcpConnectionPtr&)>
typedef std::function<void (const TcpConnectionPtr&)> WriteCompleteCallback;  // write完毕回调函数function<void (const TcpConnectionPtr&)>
typedef std::function<void (const TcpConnectionPtr&, size_t)> HighWaterMarkCallback; 

// the data has been read to (buf, len)
typedef std::function<void (const TcpConnectionPtr&,
                            Buffer*,
                            Timestamp)> MessageCallback;  // 信息到达回调函数

void defaultConnectionCallback(const TcpConnectionPtr& conn); // 默认连接回调函数
void defaultMessageCallback(const TcpConnectionPtr& conn,
                            Buffer* buffer,
                            Timestamp receiveTime); // Message回调函数

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_CALLBACKS_H_
