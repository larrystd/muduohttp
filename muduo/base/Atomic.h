// 原子类, 基于CASm copy and write, CAS是硬件支持的原子性。

#ifndef MUDUO_BASE_ATOMIC_H
#define MUDUO_BASE_ATOMIC_H

#include "muduo/base/noncopyable.h"

#include <stdint.h>

namespace muduo
{

namespace detail
{
template<typename T>
class AtomicIntegerT : noncopyable
{
 public:
  AtomicIntegerT()
    : value_(0)
  {
  }

  T get()
  {
    // in gcc >= 4.7: __atomic_load_n(&value_, __ATOMIC_SEQ_CST)
    return __sync_val_compare_and_swap(&value_, 0, 0);  // bool __sync_bool_compare_and_swap (type *ptr, type oldval type newval
  }

  T getAndAdd(T x)
  {
    // in gcc >= 4.7: __atomic_fetch_add(&value_, x, __ATOMIC_SEQ_CST)
    return __sync_fetch_and_add(&value_, x);  // 原子操作, value = value+x
  }

  T addAndGet(T x)
  {
    return getAndAdd(x) + x;  // 返回操作之后的值
  }

  T incrementAndGet()
  {
    return addAndGet(1);  // value++
  }

  T decrementAndGet()
  {
    return addAndGet(-1);
  }

  void add(T x)
  {
    getAndAdd(x);
  }

  void increment()
  {
    incrementAndGet();
  }

  void decrement()
  {
    decrementAndGet();
  }

  T getAndSet(T newValue)
  {
    // in gcc >= 4.7: __atomic_exchange_n(&value_, newValue, __ATOMIC_SEQ_CST)
    return __sync_lock_test_and_set(&value_, newValue); // value设置为newvalue, 返回设置前的值
  }

 private:
  volatile T value_;  // 限制编译优化, 复用的变量编译器可能优化成一次访存,多线程出现问题。对 volatile 变量的访问, 该访问内存就访问内存, 不进行寄存器优化(即使存储在寄存器中也通过访存读)
  // C++ volatile的问题是只是针对编译顺序, 然而CPU的乱序执行, 硬件上面的支持没有。因此不能完全处理多线程
};
}  // namespace detail

typedef detail::AtomicIntegerT<int32_t> AtomicInt32;
typedef detail::AtomicIntegerT<int64_t> AtomicInt64;

}  // namespace muduo

#endif  // MUDUO_BASE_ATOMIC_H
