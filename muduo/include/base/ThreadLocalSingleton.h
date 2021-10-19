// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCALSINGLETON_H
#define MUDUO_BASE_THREADLOCALSINGLETON_H

#include "muduo/base/noncopyable.h"

#include <assert.h>
#include <pthread.h>

namespace muduo
{

template<typename T>
class ThreadLocalSingleton : noncopyable
{
 public:
  // thread_local是单例的，不提供构造函数和析构函数
  ThreadLocalSingleton() = delete;
  ~ThreadLocalSingleton() = delete;

  /// 获取线程私有变量的唯一intance
  static T& instance()
  {
    if (!t_value_)
    {
      t_value_ = new T(); // 线程内部对象
      deleter_.set(t_value_); /// 置入线程
    }
    return *t_value_;
  }

  static T* pointer()
  {
    return t_value_;
  }

 private:
  static void destructor(void* obj)
  {
    assert(obj == t_value_);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;
    delete t_value_;
    t_value_ = 0;
  }

  class Deleter
  {
   public:
   
    Deleter()
    {
    // 创建单独的线程变量的键pkey_, 清理函数为destructor线程释放该线程存储的时候被调用。
      pthread_key_create(&pkey_, &ThreadLocalSingleton::destructor);
    }

    ~Deleter()
    {
      pthread_key_delete(pkey_);
    }
    // 需要存储特殊值调用 pthread_setspcific()
    void set(T* newObj)
    {
      assert(pthread_getspecific(pkey_) == NULL);
      /// 设置线程私有变量
      pthread_setspecific(pkey_, newObj);
    }

    pthread_key_t pkey_;
  };

  /// 前置__thread说明t_value_是私有变量
  static __thread T* t_value_;
  static Deleter deleter_;  
};

template<typename T>
__thread T* ThreadLocalSingleton<T>::t_value_ = 0;

template<typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::deleter_;

}  // namespace muduo
#endif  // MUDUO_BASE_THREADLOCALSINGLETON_H
