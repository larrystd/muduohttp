#ifndef MUDUO_BASE_NONCOPYABLE_H
#define MUDUO_BASE_NONCOPYABLE_H

// 继承noncopyable类的子类，不能被拷贝构造
// 只能被子类调用，因此构造函数设置为protected
// 将拷贝构造函数和Operator=操作符设置为delete
// 在函数声明后上“=delete;”，就可将该函数禁用。

namespace muduo
{

class noncopyable

/// 拷贝构造函数和操作符设置delete
/// 构造函数析构函数protected+ default
{
 public:
  noncopyable(const noncopyable&) = delete;
  void operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};

}  // namespace muduo

#endif  // MUDUO_BASE_NONCOPYABLE_H
