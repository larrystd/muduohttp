#ifndef MUDUO_BASE_COUNTDOWNLATCH_H_
#define MUDUO_BASE_COUNTDOWNLATCH_H_

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"

namespace muduo
{

// 倒计时
class CountDownLatch : noncopyable
{
 public:

  explicit CountDownLatch(int count); // 只能用int构造

  void wait();

  void countDown();

  int getCount() const;

 private:
  // 三个属性, mutex_, condition_, count_
  mutable MutexLock mutex_;   // mutable MutexLock, mutable在类中修饰成员变量, 那么const函数中可以修改这个成员变量(mutex自然是mutable)
  Condition condition_ GUARDED_BY(mutex_);  // GUARDED_BY(mutex_), 表示在使用condition_之前必须用mutex_保护
  int count_ GUARDED_BY(mutex_);  
};

}  // namespace muduo
#endif  // MUDUO_BASE_COUNTDOWNLATCH_H_
