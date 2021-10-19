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
#include <mysql/mysql.h>

/*
ubuntu 开发库默认开发位置
gcc -I/usr/include/mysql *.c -L/usr/lib/mysql -lmysqlclient -o *
*/

using namespace std;

using namespace muduo;
using namespace muduo::net;

bool benchmark = false;
//string staticFilePath = "/home/larry/myproject/myc++proj/muduostd/http/static"; 
string staticFilePath = "../../login";

void finish_with_error(MYSQL *con)
{
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  ///exit(1);
}

/// 读取html等静态网页文件
string readFileContent(string file)
{
    string content = "";
    std::ifstream infile; 
    infile.open(file.data());   //将文件流对象与文件连接起来 
    //assert(infile.is_open());   //若失败,则输出错误消息,并终止程序运行 

    string s;
    while(getline(infile,s))
    {
        content += s + "\r\n";
    }
    infile.close();             //关闭文件输入流 

    return content;
}

// 实际的请求处理
void onRequest(const HttpRequest& req, HttpResponse* resp)
{
  std::cout << "Headers " << req.methodString() << " " << req.path() << std::endl;
  if (!benchmark)
  {
    const std::map<string, string>& headers = req.headers();
    for (std::map<string, string>::const_iterator it = headers.begin();
         it != headers.end();
         ++it)
    {
      std::cout << it->first << ": " << it->second << std::endl;
    }
  }

  /// 根据解析的req.path
  string resPath = staticFilePath;

  if (req.path() == "/")
  {
    resPath += "/judge.html";
  }else if(req.path() == "/0"){
    resPath += "/log.html";
  } else if (req.path() == "/login_submit") {
    /// 处理数据库
    cout << "login_submit"<<endl;
    /// 先得到username和password
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
    cout << name << endl;
    cout << password<<endl;

    /// 存储到数据库中

    MYSQL *mysql;
    char *sql_insert = (char *)malloc(sizeof(char) * 200);
    strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
    strcat(sql_insert, "'");
    strcat(sql_insert, name.c_str());
    strcat(sql_insert, "', '");
    strcat(sql_insert, password.c_str());
    strcat(sql_insert, "')");

    /// 执行mysql检索
    ///根据用户名和密码
    MYSQL *con = mysql_init(NULL);

    if (con == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        /// exit(1);
    }

    if (mysql_real_connect(con, "localhost", "root", "root",
            "mydb", 0, NULL, 0) == NULL)
    {
        finish_with_error(con);
    }
    int res = mysql_query(con, sql_insert);



    if (!res)
        resPath+="/log.html";
    else
        resPath+="/registerError.html";
    mysql_close(con);
  }

  std::ifstream fileIsExist(resPath.data());
  if(!fileIsExist)
  {
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
  }
  else
  {
      string image = "img";
      /// 返回字符串str中第一次出现子串substr的地址
      /// 找图片, 可能会有多次请求
      if(strstr(resPath.c_str(),image.c_str()))
      {
        /// 打开文件
        FILE * f = fopen(resPath.data(),"rb");
        if (f)
        {
          /// f储存到buf中
          fseek(f,0,SEEK_END);
          size_t size = ftell(f); /// 相对于文件首的移动字节数
          char buf[size];
          fseek(f,0,SEEK_SET);
          memset(buf,0,size);
          size_t nRead = fread(buf,sizeof(char),size,f);
          fclose(f);
          /// 设置response
          resp->setStatusCode(HttpResponse::k200Ok);
          resp->setStatusMessage("OK");
          resp->addHeader("Server", "Jackster");
          /// 设置buf的string为返回对象
          resp->setBody(string(buf, sizeof buf));
        }
      }
      else
      {

        /// 读取html文件
        string content = readFileContent(resPath);
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        //resp->setContentType("text/html");
        resp->addHeader("Server", "Jackster");
        resp->setBody(content);
      }
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