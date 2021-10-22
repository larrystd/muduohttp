#ifndef MUDUO_BASE_LOGGING_H_
#define MUDUO_BASE_LOGGING_H_

#include "muduo/base/LogStream.h"
#include "muduo/base/Timestamp.h"

namespace muduo
{

class TimeZone;   // TimeZone

class Logger
{
 public:

 /// 日志层级, 依次递增,枚举常量
  enum LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    NUM_LOG_LEVELS,
  };

  // compile time calculation of basename of source file
  // 两种构造方法，(1)通过传入数组的引用  (2)通过const char*
  // 存储文件名和长度
  // int &array[] array与[]匹配, 等价于(int&) array[], array是一个数组, 数组元素是引用(int&)
  // int (&array)[], array是一个引用, 引用的对象是数组
  class SourceFile {
   public:
    template<int N>
    /// 传入的是数组调用之 char arr[10]这种
    SourceFile(const char (&arr)[N]) /// arr先和&匹配成引用, 引用的对象是数组。
      : data_(arr), size_(N-1)
    {
      const char* slash = strrchr(data_, '/');  // strrchr参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置。
      if (slash) {
        data_ = slash + 1;  // '/'之后的第一个字符, 也就是文件名的指针
        size_ -= static_cast<int>(data_ - arr); /// data_指针文件名处
      }
    }
    /// 传入的是指针, 加上explicit保证只能通过SourceFile(p)调用构造函数, 而不会发生隐式转换
    explicit SourceFile(const char* filename) 
      : data_(filename)
    {
      const char* slash = strrchr(filename, '/');
      if (slash) {
        data_ = slash + 1;
        size_ -= static_cast<int> (data_ - filename);
      }
    }

    // data_存储的是文件名
    const char* data_;
    int size_;  
  };

  // 声明Logger类的构造函数
  /// 基于Source file, LogLevel
  Logger(SourceFile file, int line);
  Logger(SourceFile file, int line, LogLevel level);
  Logger(SourceFile file, int line, LogLevel level, const char* func);
  Logger(SourceFile file, int line, bool toAbort);
  ~Logger();

  /// 返回Logger的流对象引用
  LogStream& stream() { return impl_.stream_; }

  static LogLevel logLevel();
  static void setLogLevel(LogLevel level);

  /// 函数指针类型
  typedef void (*OutputFunc)(const char* msg, int len);
  // Flushh函数
  typedef void (*FlushFunc)();

  // 设置函数
  static void setOutput(OutputFunc);
  static void setFlush(FlushFunc);
  static void setTimeZone(const TimeZone& tz);

 private:

  // Logger内部的Impl是其核心成员
  class Impl {
   public:
    typedef Logger::LogLevel LogLevel;  // 枚举类
    Impl (LogLevel level, int old_errno, const SourceFile& file, int line);
    void formatTime();
    void finish();

    // 时间戳
    Timestamp time_;
    // 日志流
    LogStream stream_;
    // 日志level
    LogLevel level_;
    // 执行代码的位置
    int line_;
    SourceFile basename_;
  };

  Impl impl_;
};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel()
{
  return g_logLevel;
}

// 利用宏定义将LOG_TRACE, LOG_INFO替换成对应的流对象
// 用__FILE__, __LINE__, muduo::Logger::TRACE, __func__ 构造Logger对象,注意设置好了日志等级 
// 分别表示当前语句的文件，行数，函数名
// .stream()返回日志流, 可以接收<<传入
//g_logLevel是最小日志等级, 如果设为INFO, 则不会输出TRACE。由环境变量getenv决定
#define LOG_TRACE if (muduo::Logger::logLevel() <= muduo::Logger::TRACE) \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::TRACE, __func__).stream()
#define LOG_DEBUG if (muduo::Logger::logLevel() <= muduo::Logger::DEBUG) \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::DEBUG, __func__).stream()
#define LOG_INFO if (muduo::Logger::logLevel() <= muduo::Logger::INFO) \
  muduo::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN muduo::Logger(__FILE__, __LINE__, muduo::Logger::WARN).stream()
#define LOG_ERROR muduo::Logger(__FILE__, __LINE__, muduo::Logger::ERROR).stream()
#define LOG_FATAL muduo::Logger(__FILE__, __LINE__, muduo::Logger::FATAL).stream()
#define LOG_SYSERR muduo::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL muduo::Logger(__FILE__, __LINE__, true).stream()


const char* strerror_tl(int savedErrno);

}  // namespace muduo

#endif  // MUDUO_BASE_LOGGING_H_
