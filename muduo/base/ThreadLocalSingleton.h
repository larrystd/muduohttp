#ifndef MUDUO_BASE_THREADLOCALSINGLETON_H_
#define MUDUO_BASE_THREADLOCALSINGLETON_H_

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
    if (!t_value_)  // 线程安全, 因为t_value_本身就是线程内部对象,
    {
      t_value_ = new T(); // 每个线程都可以具有单例模式, 单例对每个线程来说的
      deleter_.set(t_value_); /// t_value_作为线程私有变量置入线程, 其为线程内部指针指向单例对象
    }
    return *t_value_;
  }

  // point()函数返回指针T*
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
    // 存储线程私有变量&key, 析构调用&ThreadLocalSingleton::destructor
      pthread_key_create(&pkey_, &ThreadLocalSingleton::destructor);
    }

    ~Deleter()
    {
      // 析构
      pthread_key_delete(pkey_);
    }
    // 构造函数已经创建了线程私有变量, 这里给其赋值为T* newObj
    void set(T* newObj)
    {
      assert(pthread_getspecific(pkey_) == NULL);
      /// 线程私有变量赋值
      pthread_setspecific(pkey_, newObj);
    }

    pthread_key_t pkey_;
  };

  // T* t_value_ 是线程内部变量, 指向对象
  static __thread T* t_value_;
  static Deleter deleter_;   // 析构对象
};

template<typename T>
__thread T* ThreadLocalSingleton<T>::t_value_ = 0;

template<typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::deleter_;

}  // namespace muduo
#endif  // MUDUO_BASE_THREADLOCALSINGLETON_H
