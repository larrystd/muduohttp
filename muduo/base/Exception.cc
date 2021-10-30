#include "muduo/base/Exception.h"
#include "muduo/base/CurrentThread.h"

namespace muduo
{

// 发生异常, 输入msg, stack_为当前线程栈帧情况, 来自CurrentThread::stackTrace
Exception::Exception(string msg)
  : message_(std::move(msg)),
    stack_(CurrentThread::stackTrace(false))
{
}

}  // namespace muduo
