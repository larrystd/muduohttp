#ifndef MUDUO_NET_EVENTLOOPTHREAD_H_
#define MUDUO_NET_EVENTLOOPTHREAD_H_

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"
#include "muduo/base/Thread.h"

// EventLoopThread，既要有eventloop，也要有thread
// 注册一个回调函数，创建新线程后执行回调void(EventLoop*) ThreadInitCallback
// 新线程会创建一个eventloop, 执行loop循环, 当创建loop之后主线程获取并返回之
namespace muduo
{
namespace net
{

class EventLoop;

class EventLoopThread : noncopyable
{
 public:
 /// eventloop thread创建之后的回调函数, 参数为void(EventLoop*)
  typedef std::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const string& name = string());
  ~EventLoopThread();
  EventLoop* startLoop();

 private:
  void threadFunc();

  /// 线程指向的loop对象
  EventLoop* loop_ GUARDED_BY(mutex_);
  bool exiting_;
  
  Thread thread_; // EventloopThread对象持有thread对象
  MutexLock mutex_; // 持有锁对象
  Condition cond_ GUARDED_BY(mutex_); // 持有条件变量对象, 使用之必须先获得mutex_
  ThreadInitCallback callback_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_EVENTLOOPTHREAD_H_

