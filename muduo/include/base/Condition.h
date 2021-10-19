// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

// 条件变量
// 基于pthread封装实现
// 主要函数为wait() notify() notifyAll()
// notify也就是pthread_cond_signal， notifyAll是pthread_cond_broadcast

#ifndef MUDUO_BASE_CONDITION_H
#define MUDUO_BASE_CONDITION_H

#include "muduo/base/Mutex.h"

#include <pthread.h>

namespace muduo
{

class Condition : noncopyable
{
 public:
 // 初始化条件变量
 // 信号配合着mutex使用
 // unique_lock<mutex> lck(mtx);
 // cv.wait(mtx);
 // 这样，不论程序员是忘记了解锁，还是线程发生了异常，unique_lock的析构函数都会自动解锁，能够保证线程的异常安全。
  explicit Condition(MutexLock& mutex)
    : mutex_(mutex)
  {
    MCHECK(pthread_cond_init(&pcond_, NULL));
  }

  ~Condition()
  {
    MCHECK(pthread_cond_destroy(&pcond_));
  }
  /// wait pcond_变为true
  void wait()
  {
    MutexLock::UnassignGuard ug(mutex_);
    // int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
    // wait() 释放mutex并阻塞
    // wait被唤醒后，需要重新获得mutex才继续执行
    MCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()));
  }

  // returns true if time out, false otherwise.
  bool waitForSeconds(double seconds);

  /// 唤醒wait()的线程
  void notify()
  {
    MCHECK(pthread_cond_signal(&pcond_));
  }

  void notifyAll()
  {
    MCHECK(pthread_cond_broadcast(&pcond_));
  }

 private:
  MutexLock& mutex_;
  pthread_cond_t pcond_;
};

}  // namespace muduo

#endif  // MUDUO_BASE_CONDITION_H
