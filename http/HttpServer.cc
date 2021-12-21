#include "http/HttpServer.h"

#include "muduo/include/base/Logging.h"
#include "http/HttpContext.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

using namespace muduo;
using namespace muduo::net;

namespace muduo
{
namespace net
{
namespace detail
{

void defaultHttpCallback(const HttpRequest&, HttpResponse* resp)
{
  resp->setStatusCode(HttpResponse::k404NotFound);
  resp->setStatusMessage("Not Found");
  resp->setCloseConnection(true);
}

}  // namespace detail
}  // namespace net
}  // namespace muduo

/// 构造函数
/// 设置用户定义的回调函数, TcpServer也是参数
HttpServer::HttpServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       const string& name,
                       TcpServer::Option option)
  : server_(loop, listenAddr, name, option),
    httpCallback_(detail::defaultHttpCallback)
{
  // 设置tcpserver的回调函数
  server_.setConnectionCallback(
      std::bind(&HttpServer::onConnection, this, _1));
    ///封装到tcp的messageBack
  server_.setMessageCallback(
      std::bind(&HttpServer::onMessage, this, _1, _2, _3));
}

// httpserver::start, 转调用tcpserver的start
void HttpServer::start()
{
  LOG_WARN << "HttpServer[" << server_.name()
    << "] starts listening on " << server_.ipPort();
  server_.start();
}

// 有连接. 调用连接
void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    //// 向tcpconnection中set context
    conn->setContext(HttpContext());
  }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp receiveTime)
{
  // 直接将conn->getMutableContext()转为HttpContext类型
  HttpContext* context = boost::any_cast<HttpContext>(conn->getMutableContext());

  // 调用context->parseRequest解析存在Buffer里的请求, 不能解析执行400 bad request
  // 将请求字符串的信息设置为request的属性
  if (!context->parseRequest(buf, receiveTime))
  {
    // 直接发送400
    conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
    conn->shutdown();
  }

  // 调用context->gotAll()解析完毕， 调用onRequest
  if (context->gotAll())
  {
    // 调用onRequest
    onRequest(conn, context->request());
    context->reset();
  }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
  // 处理request
  const string& connection = req.getHeader("Connection");
  bool close = connection == "close" ||
    (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");

  HttpResponse response(close);

  /// 执行用户自定义回调函数
  httpCallback_(req, &response);
  
  Buffer buf;
  /// 状态码, contentType, header, body等由用户设置
  /// 将response对象序列化
  response.appendToBuffer(&buf);
  /*response 格式
  {<muduo::copyable> = {<No data fields>}, headers_ = std::map with 2 elements = {["Content-Type"] = "text/html",
    ["Server"] = "Muduo"}, statusCode_ = muduo::net::HttpResponse::k200Ok, statusMessage_ = "OK",
  closeConnection_ = false,
  body_ = "<html><head><title>This is title</title></head><body><h1>Hello</h1>Now is 20210913 08:12:43.152553</body></html>"}
  */
  /// 发送
  conn->send(&buf);

  if (response.closeConnection())
  {
    conn->shutdown();
  }
}

