#ifndef MUDUO_BASE_THREAD_H_
#define MUDUO_BASE_THREAD_H_

#include <pthread.h>

#include <functional>
#include <memory>

#include "muduo/base/Atomic.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Types.h"

namespace muduo
{
// thread对象, 不仅维护了用户输入信息, 例如线程执行的函数, 线程名。还维护了线程tid。
class Thread : noncopyable
{
 public:
  typedef std::function<void()> ThreadFunc;

  explicit Thread(ThreadFunc, const string& name = string());
  ~Thread();

  void start();
  int join(); // return pthread_join(), 主线程等待子线程执行完并回收资源

  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return tid_; }  // thread_id
  const string& name() const { return name_; }

  static int numCreated() { return numCreated_.get(); }

 private:
  void setDefaultName();

  bool       started_;
  bool       joined_;
  pthread_t  pthreadId_;  // pthreadId_, pthread_t认
  pid_t      tid_;  // tid_, 全局使用, 保证每个线程号都是唯一的
  ThreadFunc func_;
  string     name_;
  CountDownLatch latch_;  // 倒计时

  static AtomicInt32 numCreated_; // 原子类, 记录创建的线程个数
};

}  // namespace muduo
#endif  // MUDUO_BASE_THREAD_H_
