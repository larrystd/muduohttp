#include "muduo/base/LogFile.h"

#include "muduo/base/FileUtil.h"
#include "muduo/base/ProcessInfo.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>

using namespace muduo;

LogFile::LogFile(const string& basename,
                 off_t rollSize,
                 bool threadSafe,
                 int flushInterval,
                 int checkEveryN)
  : basename_(basename),
    rollSize_(rollSize),
    flushInterval_(flushInterval),
    checkEveryN_(checkEveryN),
    count_(0),
    mutex_(threadSafe ? new MutexLock : NULL),
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0)
{
  assert(basename.find('/') == string::npos);
  rollFile();
}

LogFile::~LogFile() = default;


// 无锁将logline加入到日志缓冲区中
void LogFile::append(const char* logline, int len)
{
  if (mutex_) // 加锁写日志
  {
    MutexLockGuard lock(*mutex_);
    append_unlocked(logline, len);
  }
  else
  {
    append_unlocked(logline, len);
  }
}

void LogFile::flush()
{
  if (mutex_)
  {
    /// 将缓冲区内的数据写回file_中fp_指定的文件中
    MutexLockGuard lock(*mutex_);
    file_->flush();
  }
  else
  {
    file_->flush();
  }
}

// 无锁将logline加入到日志缓冲区中
void LogFile::append_unlocked(const char* logline, int len)
{
  /// 在会将数据写入file_ 文件指针的缓冲区中
  file_->append(logline, len);
  //// 数据加入缓冲区以后, 缓冲区写的字节大于滚动的大小
  if (file_->writtenBytes() > rollSize_)
  {
    rollFile();
  }
  else
  {
    ++count_;
    if (count_ >= checkEveryN_) // 连续写入次数超过checkEveryN_, 就要检查要滚动日志, flush了
    {
      count_ = 0;
      time_t now = ::time(NULL);
      time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
      if (thisPeriod_ != startOfPeriod_)  // 表示距离上次滚动时间超过了kRollPerSeconds_
      {
        rollFile();
      }
      /// 距离上次flush时间超出flushInterval_
      else if (now - lastFlush_ > flushInterval_)
      {
        lastFlush_ = now;
        // 刷入文件中
        file_->flush();
      }
    }
  }
}


/// 日志滚动
bool LogFile::rollFile()
{
  time_t now = 0;
  /// filename
  string filename = getLogFileName(basename_, &now);
  time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;
  // 当前时间>lastRoll_, 需要滚动
  if (now > lastRoll_)
  {
    // 重新设置时间
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = start;
    // 创建新的文件对象, 作为日志文件
    file_.reset(new FileUtil::AppendFile(filename));
    return true;
  }
  return false;
}

/// 生成logfile的名字, log文件命名规则, 文件名+时间+主机名+进程名+.log
string LogFile::getLogFileName(const string& basename, time_t* now)
{
  string filename;
  filename.reserve(basename.size() + 64);
  // 文件名
  filename = basename;

  char timebuf[32];
  struct tm tm;
  *now = time(NULL);
  gmtime_r(now, &tm); // FIXME: localtime_r ?
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
  filename += timebuf;

  filename += ProcessInfo::hostname();  // 获取进程的主机名

  char pidbuf[32];
  snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());
  filename += pidbuf;

  filename += ".log";

  return filename;
}

