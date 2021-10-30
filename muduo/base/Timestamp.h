// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_TIMESTAMP_H_
#define MUDUO_BASE_TIMESTAMP_H_

#include "muduo/base/copyable.h"
#include "muduo/base/Types.h"

#include <boost/operators.hpp>

// 处理时间的类

namespace muduo
{

// UTC时间, 世界标准时间, 基于原子钟
/// Time stamp in UTC, in microseconds resolution.


// 多重继承,继承boost, 针对时间戳的等价比较，小于比较
class Timestamp : public muduo::copyable,
                  public boost::equality_comparable<Timestamp>,
                  public boost::less_than_comparable<Timestamp>
{
 public:

  /// 构造一个无效时间戳, 时间戳就是维护的一个microSecondsSinceEpoch_
  Timestamp()
    : microSecondsSinceEpoch_(0)
  {
  }

  ///
  /// Constucts a Timestamp at specific time, 传入参数需为微秒
  explicit Timestamp(int64_t microSecondsSinceEpochArg)
    : microSecondsSinceEpoch_(microSecondsSinceEpochArg)
  {
  }

  /// 交换两个对象，其实是交换内部microSecondsSinceEpoch_成员
  void swap(Timestamp& that)
  {
    std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
  }

  // default copy/assignment/dtor are Okay

  string toString() const;
  string toFormattedString(bool showMicroseconds = true) const;

  /// 有效的时间戳
  bool valid() const { return microSecondsSinceEpoch_ > 0; }

  // for internal usage.
  int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

  /// microSecondsSinceEpoch_转换成秒
  time_t secondsSinceEpoch() const
  { return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond); }

  ///
  /// Get time of now.
  ///
  /// 当前时间戳
  static Timestamp now();
  static Timestamp invalid()
  {
    return Timestamp();
  }

  static Timestamp fromUnixTime(time_t t)
  {
    return fromUnixTime(t, 0);
  }

  static Timestamp fromUnixTime(time_t t, int microseconds)
  {
    return Timestamp(static_cast<int64_t>(t) * kMicroSecondsPerSecond + microseconds);
  }

  static const int kMicroSecondsPerSecond = 1000 * 1000;

 private:
  int64_t microSecondsSinceEpoch_;
};

/// operator比较操作符
inline bool operator<(Timestamp lhs, Timestamp rhs)
{
  return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs)
{
  return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}


/// 时间差, 单位为秒， double
inline double timeDifference(Timestamp high, Timestamp low)
{
  int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
  return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}

/// 基于当前时间, 增加多少微秒
inline Timestamp addTime(Timestamp timestamp, double seconds)
{
  int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
  return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

}  // namespace muduo

#endif  // MUDUO_BASE_TIMESTAMP_H
