#ifndef MUDUO_NET_HTTP_HTTPCONTEXT_H_
#define MUDUO_NET_HTTP_HTTPCONTEXT_H_

#include "muduo/include/base/copyable.h"

#include "http/HttpRequest.h"

namespace muduo
{
namespace net
{

class Buffer;

class HttpContext : public muduo::copyable
{
 public:
 /// http 请求的解析状态
  enum HttpRequestParseState
  {
    kExpectRequestLine,
    kExpectHeaders,
    kExpectBody,
    kGotAll,
  };

  /// 设置初始化状态为kExpectRequestLine
  HttpContext()
    : state_(kExpectRequestLine)
  {
  }

  bool parseRequest(Buffer* buf, Timestamp receiveTime);

  bool gotAll() const
  { return state_ == kGotAll; }

  void reset()
  {
    state_ = kExpectRequestLine;
    HttpRequest dummy;
    request_.swap(dummy);
  }

  const HttpRequest& request() const
  { return request_; }

  HttpRequest& request()
  { return request_; }

 private:
  bool processRequestLine(const char* begin, const char* end);

  HttpRequestParseState state_; // 解析状态
  HttpRequest request_; // 封装的HttpRequest
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_HTTP_HTTPCONTEXT_H_
