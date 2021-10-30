// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_MUTEX_H
#define MUDUO_BASE_MUTEX_H

#include "muduo/base/CurrentThread.h"
#include "muduo/base/noncopyable.h"
#include <assert.h>
#include <pthread.h>

// Thread safety annotations {
// https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define THREAD_ANNOTATION_ATTRIBUTE__(x)   __attribute__((x))   // __attribute__是一个编译属性，用于向编译器描述特殊的标识、错误检查或高级优化。
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x)   // no-op
#endif

// 使用一些宏定义对是否加锁进行检查
#define CAPABILITY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define SCOPED_CAPABILITY \
  THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define GUARDED_BY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))
//guarded_by属性是为了保证线程安全，使用该属性后，线程要使用相应变量，必须先获得mutex_。

#define PT_GUARDED_BY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define ACQUIRED_BEFORE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define ACQUIRED_AFTER(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define REQUIRES(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define REQUIRES_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))
// 静态分析安全
#define ACQUIRE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define ACQUIRE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define RELEASE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define RELEASE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define TRY_ACQUIRE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

#define TRY_ACQUIRE_SHARED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define EXCLUDES(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define ASSERT_CAPABILITY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define ASSERT_SHARED_CAPABILITY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define RETURN_CAPABILITY(x) \
  THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define NO_THREAD_SAFETY_ANALYSIS \
  THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

// End of thread safety annotations }

#ifdef CHECK_PTHREAD_RETURN_VALUE

#ifdef NDEBUG
__BEGIN_DECLS
extern void __assert_perror_fail (int errnum,
                                  const char *file,
                                  unsigned int line,
                                  const char *function)
    noexcept __attribute__ ((__noreturn__));
__END_DECLS
#endif

#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
                       if (__builtin_expect(errnum != 0, 0))    \
                         __assert_perror_fail (errnum, __FILE__, __LINE__, __func__);})

#else  // CHECK_PTHREAD_RETURN_VALUE
// ret不等于0会终止程序
// __typeof__ 获取参数类型
#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
                       assert(errnum == 0); (void) errnum;})

#endif // CHECK_PTHREAD_RETURN_VALUE

namespace muduo
{

class CAPABILITY("mutex") MutexLock : noncopyable
{
 public:
// 初始化MutexLock
  MutexLock()
    : holder_(0)
  {
    MCHECK(pthread_mutex_init(&mutex_, NULL));  // 成功返回0
  }
// 析构MutexLock
  ~MutexLock()
  {
    assert(holder_ == 0); // 没有任何线程获得锁
    MCHECK(pthread_mutex_destroy(&mutex_));
  }

  // must be called when locked, i.e. for assertion, CurrentThread::tid()运行时获得当前正在执行的线程
  bool isLockedByThisThread() const
  {
    return holder_ == CurrentThread::tid();
  }

  void assertLocked() const ASSERT_CAPABILITY(this)
  {
    assert(isLockedByThisThread());
  }

  // internal usage

  // 加锁,获得锁, ACQUIRE()表示不需要任何参数, 会有一定的静态分析
  void lock() ACQUIRE()
  {
    MCHECK(pthread_mutex_lock(&mutex_));  // 成功返回0
    assignHolder(); // 设置hold为当前线程(刚刚获取mutex对象)
  }
  // 释放锁
  void unlock() RELEASE()
  {
    unassignHolder(); // 取消当前线程的holder
    MCHECK(pthread_mutex_unlock(&mutex_));
  }

  // 返回指向mutex_的指针, 方便在condition中调用
  pthread_mutex_t* getPthreadMutex() /* non-const */
  {
    return &mutex_;
  }

 private:
  // Condition对象也可以调用MutexLock的内部变量
  friend class Condition;

  class UnassignGuard : noncopyable
  {
   public:
    explicit UnassignGuard(MutexLock& owner)  // 用于Condition的wait, wait需要先释放锁, 包括先设置hold_ =0 
      : owner_(owner)
    {
      owner_.unassignHolder();  // 暂时设置传入参数owner_.holder_ =0
    }

    ~UnassignGuard()
    {
      owner_.assignHolder();
    }

   private:
    MutexLock& owner_;
  };

  void unassignHolder()
  {
    holder_ = 0;
  }

  void assignHolder()
  {
    holder_ = CurrentThread::tid();
  }

  // MutexLock核心维护一个pthread_mutex_t mutex_结构体
  pthread_mutex_t mutex_;
  pid_t holder_;
};


// RAII的mutex，实际上是实现了构造函数和析构函数。调用析构函数就可以释放锁
class SCOPED_CAPABILITY MutexLockGuard : noncopyable
{
 public:
  explicit MutexLockGuard(MutexLock& mutex) ACQUIRE(mutex)  // 需要mutex的显示构造函数
    : mutex_(mutex)
  {
    mutex_.lock();
  }

  ~MutexLockGuard() RELEASE() // 析构函数
  { 
    mutex_.unlock();
  }

 private:

  MutexLock& mutex_;
};

}  // namespace muduo

// Prevent misuse like:
// MutexLockGuard(mutex_);
// 如果传入x了但没有调用RAII对象, 输出传入对象错误
#define MutexLockGuard(x) error "Missing guard object name"

#endif  // MUDUO_BASE_MUTEX_H
