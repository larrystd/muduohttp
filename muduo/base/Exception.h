#ifndef MUDUO_BASE_EXCEPTION_H_
#define MUDUO_BASE_EXCEPTION_H_

#include <exception>

#include "muduo/base/Types.h"

namespace muduo
{
// 继承自std::exception, 所有标准 C++ 异常的父类。
class Exception : public std::exception
{
 public:
  Exception(string what); // 构造函数要有what, 发生错误的原因
  ~Exception() noexcept override = default;
  // default copy-ctor and operator= are okay.

  const char* what() const noexcept override
  {
    return message_.c_str();
  }
  // 打印当前线程栈帧情况
  const char* stackTrace() const noexcept
  {
    return stack_.c_str();
  }

 private:
  string message_;

  /// 当前的栈情况
  string stack_;
};

}  // namespace muduo

#endif  // MUDUO_BASE_EXCEPTION_H_
