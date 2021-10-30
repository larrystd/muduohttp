#include "muduo/base/Logging.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sstream>

#include "muduo/base/CurrentThread.h"
#include "muduo/base/Timestamp.h"
#include "muduo/base/TimeZone.h"

namespace muduo
{

// 线程内部变量. 错误信息
__thread char t_errnobuf[512];
// 时间信息
__thread char t_time[64];
__thread time_t t_lastSecond;

/// 错误信息
const char* strerror_tl(int savedErrno)
{
  return strerror_r(savedErrno, t_errnobuf, sizeof t_errnobuf);
}

Logger::LogLevel initLogLevel()
{
  if (::getenv("MUDUO_LOG_TRACE"))
    return Logger::TRACE;
  else if (::getenv("MUDUO_LOG_DEBUG"))
    return Logger::DEBUG;
  else
    return Logger::INFO;
}

// g_logLevel先设置为初始化的level
Logger::LogLevel g_logLevel = initLogLevel();

/// const* char []
const char* LogLevelName[Logger::NUM_LOG_LEVELS] =
{
  "TRACE ",
  "DEBUG ",
  "INFO  ",
  "WARN  ",
  "ERROR ",
  "FATAL ",
};

// helper class for known string length at compile time
class T
{
 public:
  T(const char* str, unsigned len)
    :str_(str),
     len_(len)
  {
    assert(strlen(str) == len_);
  }

  const char* str_;
  const unsigned len_;
};

// 将T v添加入日志流
inline LogStream& operator<<(LogStream& s, T v)
{
  s.append(v.str_, v.len_);
  return s;
}

// 通过v.data_将文件名加入到LogStream的buf中
inline LogStream& operator<<(LogStream& s, const Logger::SourceFile& v)
{
  /// 对LogStream处理, 将data加入到LogStream的buffer中
  s.append(v.data_, v.size_);
  return s;
}

// 默认输出, 用fwrite将msg刷入stdout
void defaultOutput(const char* msg, int len)
{
  /// 将msg刷新到stdout
  //size_t n = fwrite(msg, 1, len, stdout);
  fwrite(msg, 1, len, stdout);
}

/// fwrite是写入到流的缓冲区, fflush刷新流的缓冲区
void defaultFlush()
{
  /// 刷新到stdout, 缓存输出到终端
  fflush(stdout);
}

/// 设置输出和刷函数
Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

//时区
TimeZone g_logTimeZone;

}  // namespace muduo

using namespace muduo;

// 设置Logger的level, file
Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile& file, int line)
  : time_(Timestamp::now()),
    stream_(),
    level_(level),
    line_(line),
    basename_(file)
{
  formatTime();
  CurrentThread::tid();
  // 向流注入CurrentThread信息
  stream_ << T(CurrentThread::tidString(), CurrentThread::tidStringLength());
  // 向刘注入level信息
  stream_ << T(LogLevelName[level], 6);
  if (savedErrno != 0)  /// errno=0表示程序没有错误
  {
    stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";
  }
}

/// 格式化时间
void Logger::Impl::formatTime()
{
  // 相关时间的设置
  int64_t microSecondsSinceEpoch = time_.microSecondsSinceEpoch();
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / Timestamp::kMicroSecondsPerSecond);
  int microseconds = static_cast<int>(microSecondsSinceEpoch % Timestamp::kMicroSecondsPerSecond);
  if (seconds != t_lastSecond)
  {
    t_lastSecond = seconds;
    struct tm tm_time;
    if (g_logTimeZone.valid())
    {
      tm_time = g_logTimeZone.toLocalTime(seconds);
    }
    else
    {
      ::gmtime_r(&seconds, &tm_time); // FIXME TimeZone::fromUtcTime
    }
  /// 将时间信息按照年月日时分秒 根据"%4d%02d%02d %02d:%02d:%02d"格式化, 输出到t_time中
    snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
        tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    //assert(len == 17);
  }

  if (g_logTimeZone.valid())
  {
    Fmt us(".%06d ", microseconds);
    assert(us.length() == 8);
    // t_time时间信息加入到流中
    stream_ << T(t_time, 17) << T(us.data(), 8);
  }
  else
  {
    Fmt us(".%06dZ ", microseconds);
    assert(us.length() == 9);
    stream_ << T(t_time, 17) << T(us.data(), 9);
  }
}

void Logger::Impl::finish()
{
  /// 将- 日志文件名:行数加入到stream的缓冲区中
  stream_ << " - " << basename_ << ':' << line_ << '\n';
}


/// Logger构造函数, 进而调用impl_
Logger::Logger(SourceFile file, int line)
  : impl_(INFO, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level, const char* func)
  : impl_(level, 0, file, line)
{
  impl_.stream_ << func << ' '; // func加入到流中
}

Logger::Logger(SourceFile file, int line, LogLevel level)
  : impl_(level, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, bool toAbort)
  : impl_(toAbort?FATAL:ERROR, errno, file, line)
{
}


/// 在Logger析构的时候, flush日志数据
Logger::~Logger()
{
  /// 加入结束信息
  impl_.finish();
  /// 将stream().buffer()拷贝到新的buf中, 先创建对象再根据拷贝构造初始化对象返回对象的引用
  const LogStream::Buffer& buf(stream().buffer());

  /// g_output指向defaultOutput对buf, 即将msg刷新到stdout
  g_output(buf.data(), buf.length());

  /// 如果日志FATAL
  if (impl_.level_ == FATAL)
  {
    /// 全部刷新到g_flush中
    g_flush();
    // 结束当前线程程序执行
    abort();
  }
}

/// 设置日志level
void Logger::setLogLevel(Logger::LogLevel level)
{
  g_logLevel = level;
}

// 设置outFunc函数
void Logger::setOutput(OutputFunc out)
{
  g_output = out;
}
// 设置刷新函数
void Logger::setFlush(FlushFunc flush)
{
  g_flush = flush;
}
// 设置时间工具TimeZone
void Logger::setTimeZone(const TimeZone& tz)
{
  g_logTimeZone = tz;
}
