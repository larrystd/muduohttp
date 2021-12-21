#include "http/HttpResponse.h"
#include "muduo/include/net/Buffer.h"

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

/// 将要回复的状态码等信息放置到output buffer中, 也就是Reponse对象序列化到Buffer中
void HttpResponse::appendToBuffer(Buffer* output) const
{
  char buf[32];
  snprintf(buf, sizeof buf, "HTTP/1.1 %d ", statusCode_);
  output->append(buf);
  output->append(statusMessage_);
  output->append("\r\n");

  if (closeConnection_)
  {
    output->append("Connection: close\r\n");
  }
  else
  {
    snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", body_.size());
    output->append(buf);
    output->append("Connection: Keep-Alive\r\n");
  }

  /// 响应头部
  for (const auto& header : headers_)
  {
    output->append(header.first);
    output->append(": ");
    output->append(header.second);
    output->append("\r\n");
  }

  output->append("\r\n");
  /// 设置Body
  output->append(body_);
}
