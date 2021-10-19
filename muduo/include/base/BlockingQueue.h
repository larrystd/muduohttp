// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

// BlockingQueue 阻塞队列
// 实际上就是实现了一个队列，提供入队put，出队take，且保证线程安全。

/*
std::move实现，首先，通过右值引用传递模板实现，利用引用折叠原理将右值经过T&&传递类型保持不变还是右值，而左值经过T&&变为普通的左值引用
以保证模板可以传递任意实参，且保持类型不变。然后我们通过static_cast<>进行强制类型转换返回T&&右值引用，
而static_cast<T>之所以能使用类型转换，是通过remove_refrence<T>::type模板移除T&&，T&的引用，获取具体类型T。

*/
#ifndef MUDUO_BASE_BLOCKINGQUEUE_H
#define MUDUO_BASE_BLOCKINGQUEUE_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"

#include <deque>
#include <assert.h>

namespace muduo
{

template<typename T>
class BlockingQueue : noncopyable
{
 public:
  BlockingQueue()
    : mutex_(),
      notEmpty_(mutex_),
      queue_()
  {
  }

  void put(const T& x)  // put 一个左值(引用)
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(x);

    /// 队列非空条件变量
    notEmpty_.notify(); // wait morphing saves us
    // http://www.domaigne.com/blog/computing/condvars-signal-with-mutex-locked-or-not/
  }

  void put(T&& x) // put一个右值(引用)
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(std::move(x));
    notEmpty_.notify();  // 加入元素，唤醒等待序列
  }

  T take()
  {
    MutexLockGuard lock(mutex_);
    // always use a while-loop, due to spurious wakeup
    while (queue_.empty())
    {
      notEmpty_.wait();
    }
    assert(!queue_.empty());
    /// front是队首元素
    T front(std::move(queue_.front())); 
    queue_.pop_front(); // 队首元素出队列
    return front;
  }

  size_t size() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

 private:
  mutable MutexLock mutex_;
  Condition         notEmpty_ GUARDED_BY(mutex_);
  std::deque<T>     queue_ GUARDED_BY(mutex_);
};

}  // namespace muduo

#endif  // MUDUO_BASE_BLOCKINGQUEUE_H
