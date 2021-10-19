// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_BASE_FILEUTIL_H
#define MUDUO_BASE_FILEUTIL_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/StringPiece.h"
#include <sys/types.h>  // for off_t

namespace muduo
{
namespace FileUtil
{

// read small file < 64KB
/// 读取小文件
class ReadSmallFile : noncopyable
{
 public:
  ReadSmallFile(StringArg filename);
  ~ReadSmallFile();

  // return errno
  /// File内容到String
  template<typename String>
  int readToString(int maxSize,
                   String* content,
                   int64_t* fileSize,
                   int64_t* modifyTime,
                   int64_t* createTime);

  /// Read at maxium kBufferSize into buf_
  /// 读入缓冲
  // return errno
  int readToBuffer(int* size);

  const char* buffer() const { return buf_; }

  static const int kBufferSize = 64*1024;

 private:
  int fd_;
  int err_;
  char buf_[kBufferSize];
};

// read the file content, returns errno if error happens.
template<typename String>
int readFile(StringArg filename,
             int maxSize,
             String* content,
             int64_t* fileSize = NULL,
             int64_t* modifyTime = NULL,
             int64_t* createTime = NULL)
{
  ReadSmallFile file(filename);
  return file.readToString(maxSize, content, fileSize, modifyTime, createTime);
}

// not thread safe
/// 增加文件数据
class AppendFile : noncopyable
{
 public:
  explicit AppendFile(StringArg filename);

  ~AppendFile();
  /// 在文件后面写入数据
  void append(const char* logline, size_t len);
  /// flush持久化, 将缓冲区数据写入的文件中
  void flush();

  off_t writtenBytes() const { return writtenBytes_; }

 private:
  /// 向文件中写入字节
  size_t write(const char* logline, size_t len);
  /// 维护的文件指针
  FILE* fp_;

  /// 缓冲区
  char buffer_[64*1024];
  /// 已经写入的字节数
  off_t writtenBytes_;
};

}  // namespace FileUtil
}  // namespace muduo

#endif  // MUDUO_BASE_FILEUTIL_H

