#ifndef MUDUO_BASE_LOGFILE_H_
#define MUDUO_BASE_LOGFILE_H_

#include <memory>

#include "muduo/base/Mutex.h"
#include "muduo/base/Types.h"

namespace muduo
{

namespace FileUtil
{
class AppendFile;       // 前向声明, 文件类AppendFile
}

class LogFile : noncopyable {
 public:
  LogFile(const string& basename, off_t rollFile,
        bool threadSafe = true, int flushInterval = 3,
        int checkEveryN = 1024);

  ~LogFile();

    // 在缓冲区后添加数据, 内部调用append_unlocked
  void append(const char* logline, int len);
  /// flush到文件
  void flush();
  bool rollFile();  // 滚动日志, 就是建新的日志文件写日志

 private:

  void append_unlocked(const char* logline, int len); // 添加logline到日志文件fp缓冲区, 满足要求可flush
  
  static string getLogFileName(const string& basename, time_t* now);  // 日志文件名

  const string basename_; // 文件名
  const off_t rollSize_;  // 日志fp缓冲区数据如果超过rollSize_, 就要用新的文件写日志(roll)
  const int flushInterval_; // flush间隔
  const int checkEveryN_; // 连续写日志的次数, 连续写入次数超过checkEveryN_, 就要检查要滚动日志, flush了

  int count_;

  std::unique_ptr<MutexLock> mutex_;     // MutexLock是自定义的互斥量

  // time_t 长整型, 
  // 时间1970年1月1日00时00分00秒(也称为Linux系统的Epoch时间)到当前时刻的秒数。
  time_t startOfPeriod_;
  time_t lastRoll_;   // 最后Roll_时刻
  time_t lastFlush_;  // 最后Flush_时刻

  std::unique_ptr<FileUtil::AppendFile> file_;  // unique_ptr维护的文件对象

  const static int kRollPerSeconds_ = 60*60*24; // static常量, 写新日志文件的周期
};      // LogFile

}  // namespace muduo
#endif  // MUDUO_BASE_LOGFILE_H_
