#include "muduo/base/Timestamp.h"

#include <sys/time.h>
#include <stdio.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>

using namespace muduo;

/// 时间戳格式化返回, 形式**秒,**微秒的形式
string Timestamp::toString() const {
  char buf[32] = {0};
  int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
  int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
  snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
  return buf;
}

/// 格式化字符串, 将microSecondsSinceEpoch_格式化成年月日形式返回
string Timestamp::toFormattedString(bool showMicroseconds) const
{
  char buf[64] = {0};
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
  struct tm tm_time;
  gmtime_r(&seconds, &tm_time); // 将日历时间timep转换为用UTC时间表示的时间, UTC+0就是格林威治时间,UTC+8为中国时间

  if (showMicroseconds)
  {
    int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday, // 时间表示形式, 月份从0开始所以+1变成1开始
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
             microseconds);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
  }
  return buf;
}

/// 当前时间时间戳
Timestamp Timestamp::now()
{
  struct timeval tv;
  /// gettimeofday获取当前时间, 1970年1月1日到现在的时间, 精确到微秒
  gettimeofday(&tv, NULL);
  int64_t seconds = tv.tv_sec;
  // 获得一个时间戳
  return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

