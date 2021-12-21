cmake_minimum_required(VERSION 2.6)

project(http C CXX)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set(CMAKE_PREFIX_PATH ${PROJECT_BINARY_DIR})

FIND_PATH(myHeader mysql/mysql.h)
IF(myHeader)
INCLUDE_DIRECTORIES(${myHeader})
ENDIF(myHeader)

set(http_SRCS
  HttpServer.cc
  HttpResponse.cc
  HttpContext.cc
  )

add_library(muduo_http ${http_SRCS})

# muduo_http库安装的位置
install(TARGETS muduo_http DESTINATION lib)

set(HEADERS
  HttpContext.h
  HttpRequest.h
  HttpResponse.h
  HttpServer.h
  )
install(FILES ${HEADERS} DESTINATION include)
find_package(Boost REQUIRED)

include_directories(~/myproject/muduohttp)  # muduo头文件
include_directories(~/myproject/muduohttp/include)  # muduo头文件
include_directories(~/myproject/muduohttp/http/mysqlConn)  # 数据库头文件
link_directories(~/myproject/muduohttp/muduo/lib)

target_link_libraries(muduo_http muduo_net muduo_base pthread)  # 整合muduo_net muduo_base pthread到muduo_http必须库
# target_link_libraries不会将静态库合并, 下次还得连接

find_library(BOOSTTEST_LIBRARY NAMES boost_unit_test_framework)
include_directories(${Boost_INCLUDE_DIRS})

add_library(mysqllib mysqlConn/mysql_conn.cc)
target_link_libraries(mysqllib mysqlcppconn)

add_executable(httpserver_test  HttpServer_test.cc)
target_link_libraries(httpserver_test muduo_http mysqllib)