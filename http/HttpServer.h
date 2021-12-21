#ifndef MUDUO_NET_HTTP_HTTPSERVER_H_
#define MUDUO_NET_HTTP_HTTPSERVER_H_

#include "muduo/include/net/TcpServer.h"

namespace muduo
{
namespace net
{

class HttpRequest;
class HttpResponse;

/// A simple embeddable HTTP server designed for report status of a program.
/// It is not a fully HTTP 1.1 compliant server, but provides minimum features
/// that can communicate with HttpClient and Web browser.
/// It is synchronous, just like Java Servlet.
class HttpServer : noncopyable
{
 public:
 /// httpCallback, 设置回调函数用, 传入HttpRequest&, HttpResponse*。前者不可修改, 后者可修改
  typedef std::function<void (const HttpRequest&,
                              HttpResponse*)> HttpCallback;

  HttpServer(EventLoop* loop,
             const InetAddress& listenAddr,
             const string& name,
             TcpServer::Option option = TcpServer::kNoReusePort);

  /// 获取tcpserver的loop
  EventLoop* getLoop() const { return server_.getLoop(); }

  void setHttpCallback(const HttpCallback& cb)  // 用户自定义的逻辑函数
  {
    httpCallback_ = cb;
  }

  void setThreadNum(int numThreads)
  {
    server_.setThreadNum(numThreads);
  }

  void start();

 private:
 /// 维护回调函数
  void onConnection(const TcpConnectionPtr& conn);
  void onMessage(const TcpConnectionPtr& conn,
                 Buffer* buf,
                 Timestamp receiveTime);
  void onRequest(const TcpConnectionPtr&, const HttpRequest&);

  TcpServer server_;  // httpServer维护一个TcpServer对象
  HttpCallback httpCallback_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_HTTP_HTTPSERVER_H_
