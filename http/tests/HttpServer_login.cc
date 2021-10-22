#include <http/HttpServer.h>
#include <http/HttpRequest.h>
#include <http/HttpResponse.h>
#include "muduo/net/EventLoop.h"
#include "muduo/base/Logging.h"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <cstdio>

#include <cstdio>
#include <cstdlib>
#include <cassert>

// #include "mysql_connection_up.h"
#include "mysql_conn.h"
#include <unistd.h>
#include <memory>

/*
ubuntu 开发库默认开发位置
gcc -I/usr/include/mysql *.c -L/usr/lib/mysql -lmysqlclient -o *
*/

using namespace std;

using namespace muduo;
using namespace muduo::net;

bool benchmark = false;
///string staticFilePath = "../../login";
const string staticFilePath = "/home/larry/myproject/muduohttp/http/login";

ConnectionPool* pool = ConnectionPool::getInstance();

constexpr std::uint32_t hash_str_to_uint32(const char* data)
{
    std::uint32_t h(0);
    for (int i = 0; data && ('\0' != data[i]); i++)
        h = (h << 6) ^ (h >> 26) ^ data[i];
    return h;
}

/// 读取html等静态网页文件
HttpResponse::HttpStatusCode readFileContent(const char* file, string& content)
{
    std::ifstream infile;
    infile.open(file);
    
    string s;
    /// 从stream读取一行, 储存到缓冲s中
    while(getline(infile, s)) {
      content += s + "\r\n";
    }
    infile.close();

    return HttpResponse::k200Ok;
}

/// 读取图片文件, 使用C语言接口, 返回string
HttpResponse::HttpStatusCode readImgContent(const char* imgPath, string& content) {

  FILE* f = fopen(imgPath, "rb");  /// 读二进制文件
  if (f) {
    fseek(f, 0, SEEK_END);  /// FILE*指针指向最后
    size_t size = ftell(f); /// 返回当前文件指针相对于文件首的移动字节数

    char buf[size];
    fseek(f, 0, SEEK_SET);  /// FILE*指针返回文件开始位置
    memset(buf, 0, size);

    size_t nRead = fread(buf, sizeof(char), size, f); /// 读取流f中数据到buf中
    fclose(f);
    content = string(buf, sizeof(buf));
    return HttpResponse::k200Ok;
  }
  
}


// 实际的请求处理
void onRequest(const HttpRequest& req, HttpResponse* resp)
{
  LOG_INFO << "Headers " << req.methodString() << " " << req.path();


  const std::map<string, string>& headers = req.headers();
  for (std::map<string, string>::const_iterator it = headers.begin();
        it != headers.end();
        ++it)
  {
    LOG_INFO << it->first << ": " << it->second;
  }


  /// 根据解析的req.path
  string resPath = staticFilePath;

/// 解析path, 从req.path到resPath

  string path(req.path());
  switch(hash_str_to_uint32(path.data())) {
    case hash_str_to_uint32("/"): {
      resPath += "/judge.html";
      break;
    }

    case hash_str_to_uint32("/1"): {
      resPath += "/log.html"; /// 登录
      break;
    }

    case hash_str_to_uint32("/0"): {
      resPath += "/register.html";
      break;
    }

    /// 图
    case hash_str_to_uint32("/bg.jpg"): {
      resPath += "/bg.jpg"; /// 登录
      break;
    }
    //// 登录
    case hash_str_to_uint32("/login_submit"): {
    /// 处理数据库
        cout << "login_submit"<<endl;
        /// 从req.body_中得到password, 具体字符串将string对象转为const char* 处理明显更简单了
        size_t len = req.body_.size();
        const char* start = req.body_.c_str();
        const char* end = start + len;
        const char* gap = std::find(start, end, '&');

        char* sym_e1 = const_cast<char*>(gap);
        char* sym_e2 = sym_e1+1;
        while (*sym_e1 != '=')
          sym_e1--;
        while (*sym_e2 != '=')
          sym_e2++;

        string name(sym_e1+1, gap-sym_e1-1);
        string password(sym_e2+1, strlen(sym_e2));
        LOG_INFO << name << " " << password;

        string sql_query =  "select count(*) from user where username='" + name + "' and passwd='"+password+"';";
        Statement* state;
        ResultSet* result;

        /// 获得一个数据库连接

        ///std::unique_ptr<Connection, delFunc> conn = pool->getConn();
        std::shared_ptr<Connection> conn = pool->getConn();
        LOG_WARN << "num of conn"<<pool->getPoolSize();
        state = conn->createStatement();
        // 使用数据库
        state->execute("use mydb");

        // 查询语句
        result = state->executeQuery(sql_query);
        if (result->next()) {
            /// int id = result->getInt("uid");
            /// std::string name = result->getString("username");
            /// std::cout << " name:" << name << std::endl;
            resPath +=  "/bg.jpg";
        }
        pool->retConn(std::move(conn)); // 归还连接
        LOG_WARN << "num of conn" << pool->getPoolSize();
        break;
    }

    case hash_str_to_uint32("/register_submit"): {
    /// 处理数据库
        cout << "register_submit"<<endl;
        /// 从req.body_中得到password, 具体字符串将string对象转为const char* 处理明显更简单了
        size_t len = req.body_.size();
        const char* start = req.body_.c_str();
        const char* end = start + len;
        const char* gap = std::find(start, end, '&');

        char* sym_e1 = const_cast<char*>(gap);
        char* sym_e2 = sym_e1+1;
        while (*sym_e1 != '=')
          sym_e1--;
        while (*sym_e2 != '=')
          sym_e2++;

        string name(sym_e1+1, gap-sym_e1-1);
        string password(sym_e2+1, strlen(sym_e2));
        LOG_INFO << name << " " << password;

        string sql_insert =  "INSERT INTO user(username, passwd) VALUES ('" +name;
        sql_insert += "', '"+password+"');";
        Statement* state;

        /// 获得一个数据库连接

        ///std::unique_ptr<Connection, delFunc> conn = pool->getConn();
        std::shared_ptr<Connection> conn = pool->getConn();
        std::cout << "num of conn"<<pool->getPoolSize() << std::endl;
        state = conn->createStatement();
        // 使用数据库
        state->execute("use mydb");

        // 查询语句
        state->executeUpdate(sql_insert);
        resPath +=  "/log.html";

        pool->retConn(std::move(conn)); // 归还连接
        LOG_INFO << "num of conn" << pool->getPoolSize();
        break;
    }
  }


  std::ifstream fileIsExist(resPath.data());
  if(!fileIsExist)  /// 无法打开文件
  {
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
  }
  else
  {
      /// 返回字符串str中第一次出现子串substr的地址
      /// 找图片, 可能会有多次请求
      string content("");
      HttpResponse::HttpStatusCode status;
      if(strstr(resPath.c_str(), ".jpg"))
      {
        /// 打开文件
        status = readImgContent(resPath.data(), content);
      }else
      {
        status = readFileContent(resPath.data(), content);
      }
      resp->setStatusCode(status);
      resp->setStatusMessage("OK");
      resp->addHeader("Server", "Jackster");
      /// 设置buf的string为返回对象
      resp->setBody(content);
  }
  fileIsExist.close();
}

int main(int argc, char* argv[])
{
  int numThreads = 0;
  if (argc > 1)
  {
    benchmark = true;
    Logger::setLogLevel(Logger::WARN);
    numThreads = atoi(argv[1]);
  }
  EventLoop loop;
  HttpServer server(&loop, InetAddress(8000), "Jackster");
  server.setHttpCallback(onRequest);
  server.setThreadNum(numThreads);
  server.start();
  loop.loop();
}