// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/ThreadPool.h"

#include "muduo/base/Exception.h"

#include <assert.h>
#include <stdio.h>

using namespace muduo;

/// 线程池初始化, 包括构造mutex_, condition
ThreadPool::ThreadPool(const string& nameArg)
  : mutex_(),
    notEmpty_(mutex_),
    notFull_(mutex_),
    name_(nameArg),
    maxQueueSize_(0),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop();
  }
}

/// 线程池start
void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;  // running
  // std::vector<std::unique_ptr>分配空间, 即capacity的值 
  threads_.reserve(numThreads);

  /// 多线程一起执行runInThread, 也就是任务队列中的任务
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i+1);

    /// 线程对象初始化, 设置执行runInThread函数(这时候还没有真正创建线程), 添加到线程池vector
    threads_.emplace_back(new muduo::Thread(
          std::bind(&ThreadPool::runInThread, this), name_+id));
    /// 调用threads_, pthread_create创建线程并运行运行runInThread函数
    threads_[i]->start();
  }

  /// 线程数目为0
  if (numThreads == 0 && threadInitCallback_)
  {
    threadInitCallback_();
  }
}



void ThreadPool::stop()
{
  // 可在任一线程或多个线程中调用stop, 一旦共享变量running_为false, 所有线程会慢慢停止
  {
    MutexLockGuard lock(mutex_);
    running_ = false; // 控制变量未false
    notEmpty_.notifyAll();  // 防止线程阻塞到noyEmpty或notFull_中
    notFull_.notifyAll();
  }

  /// 子线程调用join(), 主线程会等待调用join()的线程执行完毕
  for (auto& thr : threads_)
  {
    thr->join();
  }
}

/// 返回线程池的size
size_t ThreadPool::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return queue_.size();
}

// 生产者
void ThreadPool::run(Task task)
{
  /// 如果线程为空, 主线程执行task
  if (threads_.empty())
  {
    task();
  }
  else
  {
    MutexLockGuard lock(mutex_);
    while (isFull() && running_)
    {
      /// 等待notFull_ is true
      notFull_.wait();
    }
    if (!running_) return;
    assert(!isFull());
    /// 任务task入队列
    queue_.push_back(std::move(task));

    /// 发出任务队列非空条件变量
    notEmpty_.notify();
  }
}

// 任务队列出队列, 返回Task类型, 也就是std::function<void ()>。相当于消费者, 使用前要wait队列非空(才能消费), 使用后需要释放队列未满的信号量给生产者
ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  while (queue_.empty() && running_)
  {
    /// 等待任务队列非空, notEmpty_ is true
    notEmpty_.wait();
  }
  Task task;
  if (!queue_.empty())
  {
    /// 任务出队列
    task = queue_.front();
    queue_.pop_front();

    if (maxQueueSize_ > 0)
    {
      /// 释放任务队列未满条件变量
      notFull_.notify();
    }
  }
  return task;
}

bool ThreadPool::isFull() const
{
  mutex_.assertLocked();
  /// queue_.size() >= maxQueueSize_说明队列满了, 注意maxQueueSize_ > 0才存在队列满, maxQueueSize_ <= 0 意味着队列无限大
  return maxQueueSize_ > 0 && queue_.size() >= maxQueueSize_;
}

/// 线程池的线程执行函数, 消费者
void ThreadPool::runInThread()
{
  try
  {
    if (threadInitCallback_)  // 可以设置线程初始化回调函数
    {
      threadInitCallback_();
    }
    // while 循环控制线程的执行, 只有running_ == true才执行之
    while (running_)
    {
      /// 任务队列队列出队, 构建成task。注意出队是需要同步
      Task task(take());

      /// 执行任务, 线程池多线程执行的是task, 消费者, 而不是生产者
      if (task)
      {
        task(); // 本身task就是std::function<void ()>, 因此可以直接调用task()执行
      }
    }
  }
  // 出现异常
  catch (const Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}