cmake_minimum_required(VERSION 2.6)

project(http C CXX)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
enable_testing()


FIND_PATH(myHeader mysql/mysql.h)
IF(myHeader)
INCLUDE_DIRECTORIES(${myHeader})
ENDIF(myHeader)

# make install 命令安装的位置
find_package(Boost REQUIRED)


include_directories(/home/larry/myproject/muduohttp)  # muduo头文件
link_directories(/home/larry/myproject/muduohttp/muduo/lib)
# target_link_libraries不会将静态库合并, 下次还得连接
#add_definitions(-DMUDUO_BUILD_EXAMPLES)

#include_directories(${Boost_INCLUDE_DIRS})
find_library(BOOSTTEST_LIBRARY NAMES boost_unit_test_framework)
include_directories(${Boost_INCLUDE_DIRS})
option(MUDUO_BUILD_EXAMPLES "Build Muduo examples" ON)
option(BOOSTTEST_LIBRARY "Build Muduo test examples" ON)


add_executable(threadPool_test  basetests/ThreadPool_test.cc)
target_link_libraries(threadPool_test muduo_net muduo_base pthread)

#if(BOOSTTEST_LIBRARY)
#add_executable(httprequest_unittest tests/HttpRequest_unittest.cc)
#target_link_libraries(httprequest_unittest muduo_base muduo_http boost_unit_test_framework)
#endif()


