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

    // 在缓冲区后添加数据
  void append(const char* logline, int len);
  /// flush到文件
  void flush();
  // 日志滚动, 也就是旧的删除, 新的顶上来。这是文件层面的, 比如最新的1.log会变化
  bool rollFile();

 private:
 // 添加logline
  void append_unlocked(const char* logline, int len);
  
  static string getLogFileName(const string& basename, time_t* now);

  const string basename_;
  const off_t rollSize_;
  const int flushInterval_;
  const int checkEveryN_;

  int count_;

  std::unique_ptr<MutexLock> mutex_;     // MutexLock是自定义的互斥量

  // time_t 长整型, 
  // 时间1970年1月1日00时00分00秒(也称为Linux系统的Epoch时间)到当前时刻的秒数。
  time_t startOfPeriod_;
  // 最后Roll_时刻
  time_t lastRoll_;
  // 最后Flush_时刻
  time_t lastFlush_;

  std::unique_ptr<FileUtil::AppendFile> file_;  // unique_ptr维护的文件对象

  const static int kRollPerSeconds_ = 60*60*24; // static常量
};      // LogFile

}  // namespace muduo
#endif  // MUDUO_BASE_LOGFILE_H
