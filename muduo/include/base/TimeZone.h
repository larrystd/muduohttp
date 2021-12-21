#ifndef MUDUO_BASE_TIMEZONE_H_
#define MUDUO_BASE_TIMEZONE_H_


#include <time.h>
#include <memory>

#include "muduo/base/copyable.h"

namespace muduo
{

class TimeZone : public muduo::copyable // 时区
{
 public:
  struct Data;

  explicit TimeZone(const char* zonefile);  // explicit要求必须按照此构造函数的类型构造, 构造函数参数是double, 输入的是int也不行
  TimeZone(int eastofUtc, const char* tzname);
  TimeZone() = default;
  // 可以进行拷贝构造和拷贝赋值

  bool valid() const {
    return static_cast<bool> (data_);
  }

  struct tm toLocalTime(time_t secondsSinceEpoch) const;
  time_t fromLocalTime(const struct tm&) const; //  time_t实际上长整型long int; 保存从1970年1月1日0时0分0秒到现在时刻的秒

    // gmtime(3)
  static struct tm toUtcTime(time_t secondsSinceEpoch, bool yday = false);
  // timegm(3)
  static time_t fromUtcTime(const struct tm&);
  // year in [1900..2500], month in [1..12], day in [1..31]
  static time_t fromUtcTime(int year, int month, int day,
                            int hour, int minute, int seconds);

 private:
  std::shared_ptr<Data> data_;
};

} // namespace muduo

#endif  // MUDUO_BASE_TIMEZONE_H_
