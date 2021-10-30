// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/TimeZone.h"

#include <assert.h>
#include <endian.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "muduo/base/noncopyable.h"
#include "muduo/base/Date.h"

namespace muduo
{
namespace detail
{

struct Transition
{
  time_t gmttime; // 1970之后Greenwich Mean Time 格林威治标准时间是指位于英国伦敦郊区的皇家格林威治天文台的标准时间，因为本初子午线被定义在通过那裡的经线。
  time_t localtime; // 当地时间
  int localtimeIdx;

  Transition(time_t t, time_t l, int localIdx)
    : gmttime(t), localtime(l), localtimeIdx(localIdx)
  { }

};

struct Comp
{
  bool compareGmt;

  Comp(bool gmt)
    : compareGmt(gmt)
  {
  }

  bool operator()(const Transition& lhs, const Transition& rhs) const
  {
    if (compareGmt)
      return lhs.gmttime < rhs.gmttime;
    else
      return lhs.localtime < rhs.localtime;
  }

  bool equal(const Transition& lhs, const Transition& rhs) const
  {
    if (compareGmt)
      return lhs.gmttime == rhs.gmttime;
    else
      return lhs.localtime == rhs.localtime;
  }
};

struct Localtime
{
  time_t gmtOffset; // gmt时间到local的偏移
  bool isDst;
  int arrbIdx;

  Localtime(time_t offset, bool dst, int arrb)
    : gmtOffset(offset), isDst(dst), arrbIdx(arrb)
  { }
};

inline void fillHMS(unsigned seconds, struct tm* utc)   // tm是日期+时间结构体, 将输入的秒数转为时, 分, 秒
{
  utc->tm_sec = seconds % 60;
  unsigned minutes = seconds / 60;
  utc->tm_min = minutes % 60;
  utc->tm_hour = minutes / 60;
}

}  // namespace detail

const int kSecondsPerDay = 24*60*60;  // 一天的秒数
}  // namespace muduo

using namespace muduo;
using namespace std;

struct TimeZone::Data // data结构体, 存储transitions和localtimes
{
  vector<detail::Transition> transitions; // 时间数组
  vector<detail::Localtime> localtimes; // 本地时间相对于gmt的偏移
  vector<string> names;
  string abbreviation;  // 简写
};

namespace muduo
{
namespace detail
{


// 读取时区文件
class File : noncopyable
{
 public:
  File(const char* file)
    : fp_(::fopen(file, "rb"))
  {
  }

  ~File()
  {
    if (fp_)
    {
      ::fclose(fp_);
    }
  }

  bool valid() const { return fp_; }

  string readBytes(int n)
  {
    char buf[n];
    ssize_t nr = ::fread(buf, 1, n, fp_);
    if (nr != n)
      throw logic_error("no enough data");
    return string(buf, n);
  }

  int32_t readInt32()
  {
    int32_t x = 0;
    ssize_t nr = ::fread(&x, 1, sizeof(int32_t), fp_);
    if (nr != sizeof(int32_t))
      throw logic_error("bad int32_t data");
    return be32toh(x);
  }

  uint8_t readUInt8()
  {
    uint8_t x = 0;
    ssize_t nr = ::fread(&x, 1, sizeof(uint8_t), fp_);
    if (nr != sizeof(uint8_t))
      throw logic_error("bad uint8_t data");
    return x;
  }

 private:
  FILE* fp_;
};


// 时区文件读取, 将内容合理加入到TimeZone::Data中, 即data->transitions, data->localtimes,data->abbreviation
bool readTimeZoneFile(const char* zonefile, struct TimeZone::Data* data)
{
  File f(zonefile);
  if (f.valid())
  {
    try
    {
      string head = f.readBytes(4);
      if (head != "TZif")
        throw logic_error("bad head");
      string version = f.readBytes(1);
      f.readBytes(15);

      int32_t isgmtcnt = f.readInt32();
      int32_t isstdcnt = f.readInt32();
      int32_t leapcnt = f.readInt32();
      int32_t timecnt = f.readInt32();
      int32_t typecnt = f.readInt32();
      int32_t charcnt = f.readInt32();

      vector<int32_t> trans;
      vector<int> localtimes;
      trans.reserve(timecnt);
      for (int i = 0; i < timecnt; ++i)
      {
        trans.push_back(f.readInt32());
      }

      for (int i = 0; i < timecnt; ++i)
      {
        uint8_t local = f.readUInt8();
        localtimes.push_back(local);
      }

      for (int i = 0; i < typecnt; ++i)
      {
        int32_t gmtoff = f.readInt32();
        uint8_t isdst = f.readUInt8();
        uint8_t abbrind = f.readUInt8();

        data->localtimes.push_back(Localtime(gmtoff, isdst, abbrind));
      }

      for (int i = 0; i < timecnt; ++i)
      {
        int localIdx = localtimes[i];
        time_t localtime = trans[i] + data->localtimes[localIdx].gmtOffset;
        data->transitions.push_back(Transition(trans[i], localtime, localIdx));
      }

      data->abbreviation = f.readBytes(charcnt);

      // leapcnt
      for (int i = 0; i < leapcnt; ++i)
      {
        // int32_t leaptime = f.readInt32();
        // int32_t cumleap = f.readInt32();
      }
      // FIXME
      (void) isstdcnt;
      (void) isgmtcnt;
    }
    catch (logic_error& e)
    {
      fprintf(stderr, "%s\n", e.what());
    }
  }
  return true;
}

// 从localtimes查找本地时区, 获取Localtime*,  里面有相对gmt的偏移量等信息
const Localtime* findLocaltime(const TimeZone::Data& data, Transition sentry, Comp comp)
{
  const Localtime* local = NULL;

  if (data.transitions.empty() || comp(sentry, data.transitions.front()))
  {
    // FIXME: should be first non dst time zone
    local = &data.localtimes.front();
  }
  else
  {
    // 从data.transitions二分查找到sentry
    vector<Transition>::const_iterator transI = lower_bound(data.transitions.begin(),
                                                            data.transitions.end(),
                                                            sentry,
                                                            comp);
    // 如果找到了
    if (transI != data.transitions.end())
    {
      if (!comp.equal(sentry, *transI))
      {
        assert(transI != data.transitions.begin());
        --transI;
      }
      // 找到本地时间
      local = &data.localtimes[transI->localtimeIdx];
    }
    else
    {
      // FIXME: use TZ-env
      local = &data.localtimes[data.transitions.back().localtimeIdx];
    }
  }

  return local;
}

}  // namespace detail
}  // namespace muduo


// 以下是TimeZone 类的实现
// 用时区文件构造
TimeZone::TimeZone(const char* zonefile)
  : data_(new TimeZone::Data)
{
  if (!detail::readTimeZoneFile(zonefile, data_.get()))
  {
    data_.reset();
  }
}

// 直接将某个时区输入进去
TimeZone::TimeZone(int eastOfUtc, const char* name)
  : data_(new TimeZone::Data)
{
  data_->localtimes.push_back(detail::Localtime(eastOfUtc, false, 0));  // eastOfUtc 本地时间加入到localtimes之中
  data_->abbreviation = name;
}

struct tm TimeZone::toLocalTime(time_t seconds) const // 将gmt的time_t转为本地时间tm
{
  struct tm localTime;
  memZero(&localTime, sizeof(localTime)); // 赋0
  assert(data_ != NULL);
  const Data& data(*data_);

  detail::Transition sentry(seconds, 0, 0); // 创建时间转换对象sentry
  const detail::Localtime* local = findLocaltime(data, sentry, detail::Comp(true));
  // 找到了本地时区
  if (local)
  {
    time_t localSeconds = seconds + local->gmtOffset; // 转换成本地时间的秒数
    ::gmtime_r(&localSeconds, &localTime); // FIXME: fromUtcTime, 将time_t转为结构体tm
    // 本地时间结构体tm
    localTime.tm_isdst = local->isDst;
    localTime.tm_gmtoff = local->gmtOffset;
    localTime.tm_zone = &data.abbreviation[local->arrbIdx];
  }

  return localTime;
}

time_t TimeZone::fromLocalTime(const struct tm& localTm) const // 本地时间tm转为gmt的time_t
{
  assert(data_ != NULL);
  const Data& data(*data_);

  struct tm tmp = localTm;
  time_t seconds = ::timegm(&tmp); // FIXME: toUtcTime
  detail::Transition sentry(0, seconds, 0);
  const detail::Localtime* local = findLocaltime(data, sentry, detail::Comp(false));
  if (localTm.tm_isdst)
  {
    struct tm tryTm = toLocalTime(seconds - local->gmtOffset);
    if (!tryTm.tm_isdst
        && tryTm.tm_hour == localTm.tm_hour
        && tryTm.tm_min == localTm.tm_min)
    {
      // FIXME: HACK
      seconds -= 3600;
    }
  }
  return seconds - local->gmtOffset;
}

struct tm TimeZone::toUtcTime(time_t secondsSinceEpoch, bool yday)  // secondsSinceEpoch转为Utc时间
{
  struct tm utc;
  memZero(&utc, sizeof(utc));
  utc.tm_zone = "GMT";
  int seconds = static_cast<int>(secondsSinceEpoch % kSecondsPerDay); // 只有days和seconds
  int days = static_cast<int>(secondsSinceEpoch / kSecondsPerDay);
  if (seconds < 0)
  {
    seconds += kSecondsPerDay;
    --days;
  }
  detail::fillHMS(seconds, &utc);
  Date date(days + Date::kJulianDayOf1970_01_01);
  Date::YearMonthDay ymd = date.yearMonthDay();
  utc.tm_year = ymd.year - 1900;
  utc.tm_mon = ymd.month - 1;
  utc.tm_mday = ymd.day;
  utc.tm_wday = date.weekDay();

  if (yday)
  {
    Date startOfYear(ymd.year, 1, 1);
    utc.tm_yday = date.julianDayNumber() - startOfYear.julianDayNumber();
  }
  return utc;
}

time_t TimeZone::fromUtcTime(const struct tm& utc)
{
  return fromUtcTime(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                     utc.tm_hour, utc.tm_min, utc.tm_sec);
}

time_t TimeZone::fromUtcTime(int year, int month, int day,
                             int hour, int minute, int seconds)
{
  Date date(year, month, day);
  int secondsInDay = hour * 3600 + minute * 60 + seconds;
  time_t days = date.julianDayNumber() - Date::kJulianDayOf1970_01_01;
  return days * kSecondsPerDay + secondsInDay;
}
