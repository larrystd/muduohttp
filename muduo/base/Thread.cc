#include <errno.h>    /// 错误事件中的某些库函数表明了什么发生了错误。
#include <stdio.h>
#include <unistd.h>   /// 是POSIX标准定义的unix类系统定义符号常量的头文件，包含了许多UNIX系统服务的函数原型
#include <sys/prctl.h>
#include <sys/syscall.h>  /// 系统调用
#include <sys/types.h>
#include <linux/unistd.h>

#include <type_traits>  /// 一些关于模板编程的支持

#include "muduo/base/Thread.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/Exception.h"
#include "muduo/base/Logging.h"

namespace muduo
{
namespace detail
{

/// 得到线程的tid
pid_t gettid()
{
  // 使用pthread_t各个进程独立，所以会有不同进程中线程号相同节的情况, syscall(SYS_gettid)得到的实际线程唯一id
  // 等价于gettid()
  return static_cast<pid_t>(::syscall(SYS_gettid)); 
}

void afterFork()
{
  muduo::CurrentThread::t_cachedTid = 0;
  muduo::CurrentThread::t_threadName = "main";
  CurrentThread::tid();
  // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

class ThreadNameInitializer
{
 public:
  ThreadNameInitializer()
  {
    muduo::CurrentThread::t_threadName = "main";
    CurrentThread::tid();
    pthread_atfork(NULL, NULL, &afterFork);
  }
};

ThreadNameInitializer init;

struct ThreadData
{
  typedef muduo::Thread::ThreadFunc ThreadFunc;
  ThreadFunc func_;
  string name_;
  pid_t* tid_;
  CountDownLatch* latch_;

  // 关于线程的一些变量, 要执行的函数, name, tid, latch
  ThreadData(ThreadFunc func,
             const string& name,
             pid_t* tid,
             CountDownLatch* latch)
    : func_(std::move(func)),
      name_(name),
      tid_(tid),
      latch_(latch)
  { }

  // ThreadData内部包含线程的函数执行流程
  void runInThread()
  {
    *tid_ = muduo::CurrentThread::tid();  // 调用CurrentThread::tid()会得到线程内部的信息, 将其t_cachedTid保存到thread对象*tid中
    tid_ = NULL;
    latch_->countDown();  // 倒计时
    latch_ = NULL;

    muduo::CurrentThread::t_threadName = name_.empty() ? "muduoThread" : name_.c_str(); // 设置muduo::CurrentThread::t_threadName
    ::prctl(PR_SET_NAME, muduo::CurrentThread::t_threadName); // 进程重命名, PR_SET_NAME,设置为CurrentThread相关信息
    // 执行线程内部函数func_()
    try
    {
      func_();
      muduo::CurrentThread::t_threadName = "finished";
    }
    catch (const Exception& ex)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      // 将字符串拷贝到标准错误流
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      fprintf(stderr, "stack trace: %s\n", ex.stackTrace());  // 栈情况
      abort();
    }
    catch (const std::exception& ex)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
      fprintf(stderr, "reason: %s\n", ex.what());
      abort();
    }
    catch (...)
    {
      muduo::CurrentThread::t_threadName = "crashed";
      fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
      throw; // rethrow
    }
  }
};

// 开启线程, pthread_create执行的函数
void* startThread(void* obj)
{
  ThreadData* data = static_cast<ThreadData*>(obj);
  data->runInThread();  // 调用runInThread
  delete data;
  return NULL;
}

}  // namespace detail

// 获得当前线程的id， 这个会被CurrentThread.h中调用成为线程内部信息
void CurrentThread::cacheTid()
{
  if (t_cachedTid == 0)
  {
    t_cachedTid = detail::gettid(); // t_cachedTid是线程内部变量
    t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid); // 将t_cachedTid格式化到t_tidString, 也是线程内部变量
  }
}

// tid()=getpid()的线程是主线程
bool CurrentThread::isMainThread()
{
  return tid() == ::getpid(); // 取得目前进程的进程识别码
}

void CurrentThread::sleepUsec(int64_t usec)
{
  struct timespec ts = { 0, 0 };
  ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
  ::nanosleep(&ts, NULL);
}

AtomicInt32 Thread::numCreated_;

Thread::Thread(ThreadFunc func, const string& n)
// Thread对象初始化的一些变量
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(0),
    func_(std::move(func)),
    name_(n), 
    latch_(1)
{
  setDefaultName();
}

Thread::~Thread()
{
  if (started_ && !joined_) // 如果开启了线程没有joined, 就detach
  {
    pthread_detach(pthreadId_);
  }
}

void Thread::setDefaultName()
{
  int num = numCreated_.incrementAndGet();
  if (name_.empty())
  {
    char buf[32];
    snprintf(buf, sizeof buf, "Thread%d", num);
    name_ = buf;
  }
}

// 创建线程并执行, pthread_create方法来进行创建，最终会调用到do_fork方法
// 线程从内核层面来看其实也是一种特殊的进程，它跟父进程共享了打开的文件和文件系统信息，共享了地址空间和信号处理函数]
// pthread_create创建的线程的getpid()和父线程一致, gettid()不一致
void Thread::start()
{
  assert(!started_);
  started_ = true;
  // FIXME: move(func_)
  detail::ThreadData* data = new detail::ThreadData(func_, name_, &tid_, &latch_);  // 构建threadData对象(func_也放进去了), 执行startThread函数, 传参为data
  if (pthread_create(&pthreadId_, NULL, &detail::startThread, data)) // 后面的参数是执行的函数和传参, 创建失败，返回1
  {
    started_ = false;
    delete data; // or no delete?
    LOG_SYSFATAL << "Failed in pthread_create";
  }
  else
  // 线程创建成功
  {
    latch_.wait();  // 倒计时执行
    assert(tid_ > 0);
  }
}

// pthread_join
int Thread::join()
{
  assert(started_); // 必须已经start了
  assert(!joined_); // 没有join过
  joined_ = true;
  return pthread_join(pthreadId_, NULL);  // 调用join, 第二个参数是可返回的
}

}  // namespace muduo
