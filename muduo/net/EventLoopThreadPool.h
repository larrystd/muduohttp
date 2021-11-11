#ifndef MUDUO_NET_EVENTLOOPTHREADPOOL_H_
#define MUDUO_NET_EVENTLOOPTHREADPOOL_H_

#include "muduo/base/noncopyable.h"
#include "muduo/base/Types.h"

#include <functional>
#include <memory>
#include <vector>

namespace muduo
{

namespace net
{

class EventLoop;
class EventLoopThread;  

class EventLoopThreadPool : noncopyable
{
 public:
  typedef std::function<void(EventLoop*)> ThreadInitCallback;  // 线程初始化回调函数

  EventLoopThreadPool(EventLoop* baseLoop, const string& nameArg);
  ~EventLoopThreadPool();
  void setThreadNum(int numThreads) { numThreads_ = numThreads; }
  void start(const ThreadInitCallback& cb = ThreadInitCallback());

  EventLoop* getNextLoop(); // 返回一个可用的loop*

  /// with the same hash code, it will always return the same EventLoop
  EventLoop* getLoopForHash(size_t hashCode);

  std::vector<EventLoop*> getAllLoops();  // Evnetloop线程池对象持有指向所有loop对象的vector

  bool started() const
  { return started_; }

  const string& name() const
  { return name_; }

 private:

  EventLoop* baseLoop_;
  string name_;
  
  bool started_;
  int numThreads_;
  int next_;

  std::vector<std::unique_ptr<EventLoopThread>> threads_; // eventloopthread线程指针列表, 线程对象在堆上
  std::vector<EventLoop*> loops_; // eventloop对象指针的vector, loop对象建在线程对象内部
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_EVENTLOOPTHREADPOOL_H_
