#ifndef MUDUO_BASE_CONDITION_H_
#define MUDUO_BASE_CONDITION_H_

#include <pthread.h>

#include "muduo/base/Mutex.h"

namespace muduo
{

class Condition : noncopyable
{
 public:

  explicit Condition(MutexLock& mutex)  // 比如用MutexLock对象显示构造
    : mutex_(mutex)
  {
    MCHECK(pthread_cond_init(&pcond_, NULL)); // pthread_cond_init的返回值(成功会return 0)送到MCHECK, 失败了会终止程序。
  }

  ~Condition()
  {
    MCHECK(pthread_cond_destroy(&pcond_));  // 成功返回0
  }

  void wait()
  {
    MutexLock::UnassignGuard ug(mutex_);  // 这里设置mutex_对象的holder=0

    MCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex())); // 传入pcond_和*mutex, 会先释放锁, 然后等待notify
  }

  // returns true if time out, false otherwise.
  bool waitForSeconds(double seconds);

  /// 唤醒wait()的线程
  void notify()
  {
    MCHECK(pthread_cond_signal(&pcond_)); // 唤醒pthread_cond_signal
  }

  void notifyAll()
  {
    MCHECK(pthread_cond_broadcast(&pcond_));  // 唤醒pthread_cond_broadcast
  }

 private:
  MutexLock& mutex_;  // MutexLock对象
  pthread_cond_t pcond_;  // pthread_cond_t 结构体
};

}  // namespace muduo

#endif  // MUDUO_BASE_CONDITION_H
