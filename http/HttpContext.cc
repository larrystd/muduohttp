#include "muduo/include/net/Buffer.h"
#include "http/HttpContext.h"
#include <assert.h>
using namespace muduo;
using namespace muduo::net;

bool HttpContext::processRequestLine(const char* begin, const char* end) // 解析请求行
{
  bool succeed = false;
  const char* start = begin;
  const char* space = std::find(start, end, ' ');
  /// 可以解析出Method
  if (space != end && request_.setMethod(start, space))
  {
    start = space+1;
    space = std::find(start, end, ' ');

    /// 解析出path
    if (space != end)
    {
      const char* question = std::find(start, space, '?');
      if (question != space)
      {
        request_.setPath(start, question);
        request_.setQuery(question, space);
      }
      else
      {
        request_.setPath(start, space);
      }
      start = space+1;

      /// 解析http版本
      succeed = end-start == 8 && std::equal(start, end-1, "HTTP/1.");
      if (succeed)
      {
        if (*(end-1) == '1')
        {
          request_.setVersion(HttpRequest::kHttp11);
        }
        else if (*(end-1) == '0')
        {
          request_.setVersion(HttpRequest::kHttp10);
        }
        else
        {
          succeed = false;
        }
      }
    }
  }
  return succeed;
}

// return false if any error
/// 解析http请求
/*
请求行, 请求头部, 请求报文
GET /favicon.ico HTTP/1.1\r\nHost: 172.20.109.213:9006\r\nConnection: keep-alive\r\nPragma: no-cache\r\nCache-Control: no-cache\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/93.0.4577.82 Safari/537.36\r\nAccept: image/avif,image/webp,image/apng,image/svg+xml,image/*
*/
bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime)
{
  bool ok = true;
  bool hasMore = true;
  /// 一行一行解析
  /// state_ = muduo::net::HttpContext::kExpectRequestLine
  while (hasMore)
  {
    /// 解析请求行
    if (state_ == kExpectRequestLine)
    {
      /// 找CR LR \r\n, 也就是请求行结束的位置
      const char* crlf = buf->findCRLF();
      if (crlf)
      {
        /// 从buf中找到crlf
        // buf中存储的字符如"GET / HTTP/1.1\r\nHost: 127.0.0.1:8000\r\nUser-Agent: curl/7.61.0\r\nAccept: 
        /// 可以解析出method, httpversion
        ok = processRequestLine(buf->peek(), crlf); 
        if (ok)
        {
          request_.setReceiveTime(receiveTime);
          /// 读后更新buffer索引, 也就是更新buf->peek()可读位置
          buf->retrieveUntil(crlf + 2);
          /// kExpectHeaders
          state_ = kExpectHeaders;
        }
        else
        {
          hasMore = false;
        }
      }
      else
      {
        hasMore = false;
      }
    }
    /// 该解析请求头了
    else if (state_ == kExpectHeaders)  // 解析请求头
    {
      /// 请求头结束的地方
      const char* crlf = buf->findCRLF();
      if (crlf)
      {
        /// 在buf->peek()到crlf寻找:, peek是可读buf地址
        const char* colon = std::find(buf->peek(), crlf, ':');  
        if (colon != crlf)  // 如果可以找到
        {
          request_.addHeader(buf->peek(), colon, crlf);
        }
        else
        {
          /// 这一行说明该到Body了
          // empty line, end of header
          state_ = kExpectBody;
          
        }
        /// crlf + 位置已经读完
        buf->retrieveUntil(crlf + 2); /// 跳过\r\n
      }
      else
      {
        hasMore = false;
      }
    }

    else if (state_ == kExpectBody)
    {
      /// 这个直接让用户解析, 把Body给用户
      //if (buf->peek() != buf->beginWrite())

      if (buf->readableBytes() > 0 ) /// 还没有读完
        request_.setBody(buf->peek(), buf->beginWrite());

      state_ = kGotAll; /// 已经读完
      hasMore = false;
    }
  }
  return ok;
}
