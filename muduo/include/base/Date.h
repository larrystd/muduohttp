#ifndef MUDUO_BASE_DATE_H_
#define MUDUO_BASE_DATE_H_

#include "muduo/base/copyable.h"
#include "muduo/base/Types.h"

struct tm;

namespace muduo
{

class Date : public muduo::copyable
{
 public:
  /// 日期 年月日
  struct YearMonthDay
  {
    int year; // [1900..2500]
    int month;  // [1..12]
    int day;  // [1..31]
  };

  static const int kDaysPerWeek = 7;
  static const int kJulianDayOf1970_01_01;

  Date()
    : julianDayNumber_(0)
  {}

  Date(int year, int month, int day);

  // 通过Julian Day Number构造类, 公元前4713年1月1日
  explicit Date(int julianDayNum)
    : julianDayNumber_(julianDayNum)
  {}

  // Constucts a Date from struct tm
  explicit Date(const struct tm&);

  // default copy/assignment/dtor are Okay
  void swap(Date& that)
  {
    std::swap(julianDayNumber_, that.julianDayNumber_);
  }

  bool valid() const { return julianDayNumber_ > 0; }
  // Converts to yyyy-mm-dd format.
  string toIsoString() const;

  struct YearMonthDay yearMonthDay() const;

  int year() const
  {
    return yearMonthDay().year;
  }

  int month() const
  {
    return yearMonthDay().month;
  }

  int day() const
  {
    return yearMonthDay().day;
  }

  // 返回星期[0, 1, ..., 6] => [Sunday, Monday, ..., Saturday ]
  int weekDay() const
  {
    return (julianDayNumber_+1) % kDaysPerWeek;
  }
  int julianDayNumber() const { return julianDayNumber_; }

 private:
 //  儒略号
  int julianDayNumber_;
};

inline bool operator<(Date x, Date y)
{
  return x.julianDayNumber() < y.julianDayNumber();
}

inline bool operator==(Date x, Date y)
{
  return x.julianDayNumber() == y.julianDayNumber();
}

}  // namespace muduo

#endif  // MUDUO_BASE_DATE_H_
