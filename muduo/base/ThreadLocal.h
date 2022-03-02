#ifndef MUDUO_BASE_THREADLOCAL_H
#define MUDUO_BASE_THREADLOCAL_H

#include "muduo/base/Mutex.h"
#include "muduo/base/noncopyable.h"

#include <pthread.h>

namespace muduo
{

// T为多线程共享变量的类型
template<typename T>
class ThreadLocal : noncopyable
{
 public:
  ThreadLocal() // 多线程私有数据
  {
    /// 线程内部变量定义pKey, 多线程私有数据就是根据key拿到的, 析构时调用destructor
    MCHECK(pthread_key_create(&pkey_, &ThreadLocal::destructor));
  }

  ~ThreadLocal()
  {
    MCHECK(pthread_key_delete(pkey_));
  }

  T& value()  // 得到线程内部对象的值
  {
    T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));  // 读取私有数据, 获取线程内部储存的值
    
    if (!perThreadValue)  // 如果线程内没有该对象，则重新创建一个
    {
      T* newObj = new T();  // 创建一个私有对象
      MCHECK(pthread_setspecific(pkey_, newObj)); // 赋值pKey
      perThreadValue = newObj;
    }
    return *perThreadValue;
  }

 private:

  static void destructor(void *x)
  {
    /// x强制转型为T*, 也就是指针类型
    T* obj = static_cast<T*>(x);

    /// 如果sizeof(T) == 0(不正确), 则编译器会报错T_must_be_complete_type[-1]
    /// 下面这句话只允许sizeof(T) != 0
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;
    /// delete obj指向的对象
    delete obj;
  }

 private:
 /// 线程私有变量
  pthread_key_t pkey_;
};

}  // namespace muduo

#endif  // MUDUO_BASE_THREADLOCAL_H
