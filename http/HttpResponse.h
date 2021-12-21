#ifndef MUDUO_NET_HTTP_HTTPRESPONSE_H_
#define MUDUO_NET_HTTP_HTTPRESPONSE_H_

#include "muduo/include/base/copyable.h"
#include "muduo/include/base/Types.h"

#include <map>

namespace muduo
{
namespace net
{

class Buffer;
class HttpResponse : public muduo::copyable
{
 public:
 /// http状态码
  enum HttpStatusCode
  {
    kUnknown,
    k200Ok = 200,
    k301MovedPermanently = 301,
    k400BadRequest = 400,
    k404NotFound = 404,
  };

  explicit HttpResponse(bool close)
    : statusCode_(kUnknown),
      closeConnection_(close)
  {
  }

  void setStatusCode(HttpStatusCode code)
  { statusCode_ = code; }

  void setStatusMessage(const string& message)
  { statusMessage_ = message; }

  void setCloseConnection(bool on)
  { closeConnection_ = on; }

  bool closeConnection() const
  { return closeConnection_; }

  //// contentType
  void setContentType(const string& contentType)
  { addHeader("Content-Type", contentType); }
  /// header
  // FIXME: replace string with StringPiece
  void addHeader(const string& key, const string& value)
  { headers_[key] = value; }
  /// body
  void setBody(const string& body)
  { body_ = body; }

  void appendToBuffer(Buffer* output) const;

  string body_;
 private:
  std::map<string, string> headers_;
  HttpStatusCode statusCode_;
  // FIXME: add http version
  string statusMessage_;
  bool closeConnection_;
  
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_HTTP_HTTPRESPONSE_H_
