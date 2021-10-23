#include "muduo/base/FileUtil.h"
#include "muduo/base/Logging.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;

///AppendFile构造器根据文件名,  a表示可追加写, 返回得到fp
FileUtil::AppendFile::AppendFile(StringArg filename)
  : fp_(::fopen(filename.c_str(), "ae")),  // 'e' for O_CLOEXEC
    writtenBytes_(0)
{
  /// 设置文件流的缓冲区, fp与缓冲区关联
  assert(fp_);
  ::setbuffer(fp_, buffer_, sizeof buffer_);
  // posix_fadvise POSIX_FADV_DONTNEED ?
}

/// 关闭文件指针fp
FileUtil::AppendFile::~AppendFile()
{
  ::fclose(fp_);
}

/// 在文件后写入len字节的数据, 实际上是logline的数据写入到fp的缓冲区中
void FileUtil::AppendFile::append(const char* logline, const size_t len)
{
  size_t written = 0;

  while (written != len)
  {
    size_t remain = len - written;
    /// 调用FileUtil write函数将logline的数据写入到fp的缓冲区中
    size_t n = write(logline + written, remain);
    if (n != remain)
    {
      int err = ferror(fp_);
      if (err)
      {
        fprintf(stderr, "AppendFile::append() failed %s\n", strerror_tl(err));
        break;
      }
    }
    written += n;
  }
  // 文件缓冲区已经写入的字节
  writtenBytes_ += written;
}

/// fp_缓冲区数据刷新到持久化磁盘文件中
void FileUtil::AppendFile::flush()
{
  /// 将fp关联的缓冲区内的数据写回fp_指定的文件中
  ::fflush(fp_);
}

// 将logline指的缓冲区写入到fp_文件流的缓冲区
size_t FileUtil::AppendFile::write(const char* logline, size_t len)
{
  // #undef fwrite_unlocked
  /// 无锁写入, 将logline指向内存的len字节写入到fp_指向的文件中
  return ::fwrite_unlocked(logline, 1, len, fp_);
}

// ReadSmallFile构造, open打开文件返回文件描述符,注意区分fopen和open,  int open(const char *path, int oflags,mode_t mode);
// FILE文件指针结构包含一个缓冲区和一个文件描述符, 是对文件描述符的封装
FileUtil::ReadSmallFile::ReadSmallFile(StringArg filename)
  : fd_(::open(filename.c_str(), O_RDONLY | O_CLOEXEC)),
    err_(0)
{
  buf_[0] = '\0';
  if (fd_ < 0)
  {
    err_ = errno;
  }
}

// 关闭文件描述符
FileUtil::ReadSmallFile::~ReadSmallFile()
{
  if (fd_ >= 0)
  {
    ::close(fd_); // FIXME: check EINTR
  }
}

// return errno
// 读取文件到content String
// stat 储存一些文件的属性, 例如
//   dev_t         st_dev;       //文件的设备编号 
//   ino_t         st_ino;       //节点 
//    mode_t        st_mode;      //文件的类型和存取的权限
// time_t        st_atime;     //最后一次访问时间 
//    time_t        st_mtime;     //最后一次修改时间
template<typename String>
int FileUtil::ReadSmallFile::readToString(int maxSize,
                                          String* content,
                                          int64_t* fileSize,
                                          int64_t* modifyTime,
                                          int64_t* createTime)
{
  static_assert(sizeof(off_t) == 8, "_FILE_OFFSET_BITS = 64");
  assert(content != NULL);
  int err = err_;
  if (fd_ >= 0)
  {
    content->clear();

    if (fileSize)
    {
      struct stat statbuf;
      /// 获取fd_的stat保存到statbuf中, 0表示成功
      // 同时将相关信息存储到createTime等变量中
      if (::fstat(fd_, &statbuf) == 0)
      {
        if (S_ISREG(statbuf.st_mode))
        {
          *fileSize = statbuf.st_size;

          /// content开辟fileSize或maxSize的空间
          content->reserve(static_cast<int>(std::min(implicit_cast<int64_t>(maxSize), *fileSize)));
        }
        else if (S_ISDIR(statbuf.st_mode))
        {
          err = EISDIR;
        }
        if (modifyTime)
        {
          *modifyTime = statbuf.st_mtime;
        }
        if (createTime)
        {
          *createTime = statbuf.st_ctime;
        }
      }
      else
      {
        err = errno;
      }
    }

    while (content->size() < implicit_cast<size_t>(maxSize))
    {
      // 可读的大小
      size_t toRead = std::min(implicit_cast<size_t>(maxSize) - content->size(), sizeof(buf_));
      /// fd_读取文件数据到buf_中
      ssize_t n = ::read(fd_, buf_, toRead);
      /// buf_数据再到content
      if (n > 0)
      {
        content->append(buf_, n);
      }
      else
      {
        if (n < 0)
        {
          err = errno;
        }
        break;
      }
    }
  }
  return err;
}

/// 读取文件到buf
int FileUtil::ReadSmallFile::readToBuffer(int* size)
{
  int err = err_;
  if (fd_ >= 0)
  {
    // 在指定偏移offset位置开始读取count个字节，pread 可以保证线程安全, 因为是基于offset读取(相当于fseek), 多线程下文件指针没有改变
    ssize_t n = ::pread(fd_, buf_, sizeof(buf_)-1, 0);
    /// 读取成功
    if (n >= 0)
    {
      if (size)
      {
        *size = static_cast<int>(n);
      }
      buf_[n] = '\0';
    }
    else
    {
      err = errno;
    }
  }
  return err;
}

template int FileUtil::readFile(StringArg filename,
                                int maxSize,
                                string* content,
                                int64_t*, int64_t*, int64_t*);

template int FileUtil::ReadSmallFile::readToString(
    int maxSize,
    string* content,
    int64_t*, int64_t*, int64_t*);

