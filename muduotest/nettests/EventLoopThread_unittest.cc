#include "muduo/net/EventLoopThread.h"
#include "muduo/net/EventLoop.h"
#include "muduo/base/Thread.h"
#include "muduo/base/CountDownLatch.h"

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;
// pid 进程id， tid线程id
//线程是轻量级的进程，轻量级体现在所有的进程切换都需要清除所有的表、进程间的共享信息也比较麻烦，一般来说通过管道或者共享内存，
// 如果是 fork 函数后的父子进程则使用共享文件，然而线程切换不需要像进程一样具有昂贵的开销，而且线程通信起来也更方便。
// 线程分为两种：用户级线程和内核级线程

void print(EventLoop* p = NULL)
{
  printf("print: pid = %d, tid = %d, loop = %p\n",
         getpid(), CurrentThread::tid(), p);
}

void quit(EventLoop* p)
{
  print(p);
  p->quit();
}

int main()
{
  print();

  {
  EventLoopThread thr1;  // never start, no loop
  }

  {
  // dtor calls quit()
  EventLoopThread thr2;
  EventLoop* loop = thr2.startLoop();
  loop->runInLoop(std::bind(print, loop));
  CurrentThread::sleepUsec(500 * 1000);
  }

  {
  // quit() before dtor
  EventLoopThread thr3;
  EventLoop* loop = thr3.startLoop();
  loop->runInLoop(std::bind(quit, loop));
  CurrentThread::sleepUsec(500 * 1000);
  }
}

