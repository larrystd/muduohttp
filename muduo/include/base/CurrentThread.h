#ifndef MUDUO_BASE_CURRENTTHREAD_H_
#define MUDUO_BASE_CURRENTTHREAD_H_

#include "muduo/base/Types.h"

// 关于当前线程信息, 注意这是个namespace, 不是一个类, 用来支持Thread类。但为了防止重名还是要#ifndef。
namespace muduo
{
namespace CurrentThread
{
  // 声明为extern外界可以访问
  // __thread说明这是线程内部的变量, 在线程内部创建。可以认为是创建在线程内部的编译期常量, 函数退出也不会消失, 但不同线程看到的不一样, 一个线程执行到这得到的数, 另一个看不到。
  extern __thread int t_cachedTid;  // tid
  extern __thread char t_tidString[32];
  extern __thread int t_tidStringLength;
  extern __thread const char* t_threadName; // thread_nane
  void cacheTid();

  // 返回相关信息
  inline int tid()
  {
    if (__builtin_expect(t_cachedTid == 0, 0))  // __builtin_expect(EXP, N)用来告诉编译器EXP==N可能性大, 也就是t_cachedTid几乎不可能为0
    {
      cacheTid(); // 调用cacheTid(得到线程t_cachedTid, t_tidString, t_tidStringLength)等变量
    }
    return t_cachedTid; // tid
  }

  inline const char* tidString() // for logging
  {
    return t_tidString;
  }

  inline int tidStringLength() // for logging
  {
    return t_tidStringLength;
  }

  inline const char* name()
  {
    return t_threadName;
  }

  // 是否为主线程
  bool isMainThread();

  void sleepUsec(int64_t usec);  // for testing

  string stackTrace(bool demangle);
}  // namespace CurrentThread
}  // namespace muduo

#endif  // MUDUO_BASE_CURRENTTHREAD_H
