# Install script for directory: /home/larry/myproject/muduohttp/muduo/base

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/larry/myproject/muduohttp/http/lib/libmuduo_base.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/muduo/base" TYPE FILE FILES
    "/home/larry/myproject/muduohttp/muduo/base/AsyncLogging.h"
    "/home/larry/myproject/muduohttp/muduo/base/Atomic.h"
    "/home/larry/myproject/muduohttp/muduo/base/BlockingQueue.h"
    "/home/larry/myproject/muduohttp/muduo/base/BoundedBlockingQueue.h"
    "/home/larry/myproject/muduohttp/muduo/base/Condition.h"
    "/home/larry/myproject/muduohttp/muduo/base/CountDownLatch.h"
    "/home/larry/myproject/muduohttp/muduo/base/CurrentThread.h"
    "/home/larry/myproject/muduohttp/muduo/base/Date.h"
    "/home/larry/myproject/muduohttp/muduo/base/Exception.h"
    "/home/larry/myproject/muduohttp/muduo/base/FileUtil.h"
    "/home/larry/myproject/muduohttp/muduo/base/GzipFile.h"
    "/home/larry/myproject/muduohttp/muduo/base/LogFile.h"
    "/home/larry/myproject/muduohttp/muduo/base/LogStream.h"
    "/home/larry/myproject/muduohttp/muduo/base/Logging.h"
    "/home/larry/myproject/muduohttp/muduo/base/Mutex.h"
    "/home/larry/myproject/muduohttp/muduo/base/ProcessInfo.h"
    "/home/larry/myproject/muduohttp/muduo/base/Singleton.h"
    "/home/larry/myproject/muduohttp/muduo/base/StringPiece.h"
    "/home/larry/myproject/muduohttp/muduo/base/Thread.h"
    "/home/larry/myproject/muduohttp/muduo/base/ThreadLocal.h"
    "/home/larry/myproject/muduohttp/muduo/base/ThreadLocalSingleton.h"
    "/home/larry/myproject/muduohttp/muduo/base/ThreadPool.h"
    "/home/larry/myproject/muduohttp/muduo/base/TimeZone.h"
    "/home/larry/myproject/muduohttp/muduo/base/Timestamp.h"
    "/home/larry/myproject/muduohttp/muduo/base/Types.h"
    "/home/larry/myproject/muduohttp/muduo/base/WeakCallback.h"
    "/home/larry/myproject/muduohttp/muduo/base/copyable.h"
    "/home/larry/myproject/muduohttp/muduo/base/noncopyable.h"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/larry/myproject/muduohttp/http/muduo/base/tests/cmake_install.cmake")

endif()

