#include "muduo/base/CountDownLatch.h"

using namespace muduo;

CountDownLatch::CountDownLatch(int count)
  : mutex_(),
    condition_(mutex_),
    count_(count)
{
}

// 执行完wait, count_已经等于0了
void CountDownLatch::wait()
{
  MutexLockGuard lock(mutex_);

  /// 一直等待到count_==0
  while (count_ > 0)
  {
    // 释放锁mutex_并等待
    condition_.wait();
  }
}

/// 执行一次countDown倒计时count_减1, 如果count_=0唤醒condition_
void CountDownLatch::countDown()
{
  MutexLockGuard lock(mutex_);
  --count_;
  if (count_ == 0)
  {
    condition_.notifyAll();
  }
}

// 等到当前的count
int CountDownLatch::getCount() const
{
  MutexLockGuard lock(mutex_);
  return count_;
}

