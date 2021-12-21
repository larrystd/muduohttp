#include "muduo/net/Timer.h"

using namespace muduo;
using namespace muduo::net;

AtomicInt64 Timer::s_numCreated_;   // 这行代码重要, 静态变量在头文件class的只能叫声明, 换言之, class的成员变量都要在.cc中定义一遍。因为别的文件#include头文件可以获得声明, 但是链接时离不开.cc的定义。尽管变量定义和声明都可以是int a; 但.h中的int a就是声明, 而.cc的int a则是定义

void Timer::restart(Timestamp now) {
  if (repeat_)  // 如果定时器是重复的
  {
    expiration_ = addTime(now, interval_);  // 当前时间加上事件间隔 计算新的超时时间
  }
  else {
    expiration_ = Timestamp::invalid();
  }
}
