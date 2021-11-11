#include "muduo/net/EventLoopThread.h"

#include "muduo/net/EventLoop.h"

using namespace muduo;
using namespace muduo::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,  
                                 const string& name)  // 用线程初始化回调函数构造函数
  : loop_(NULL),  // loop_是指针, 指向子线程所有的loop_对象
    exiting_(false),
    thread_(std::bind(&EventLoopThread::threadFunc, this), name), //构造线程对象 注册绑定线程执行的函数, EventLoopThread::threadFunc
    mutex_(),
    cond_(mutex_),
    callback_(cb) // 回调函数
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
  {
    // still a tiny chance to call destructed object, if threadFunc exits just now.
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    loop_->quit();
    thread_.join();
  }
}

EventLoop* EventLoopThread::startLoop() // (主线程)开启线程(执行函数)
{
  assert(!thread_.started());
  thread_.start(); // 新线程t执行start() , 执行threadFunc, 主线程进行向下执行

  EventLoop* loop = NULL;
  {
    MutexLockGuard lock(mutex_);
    /// 条件变量主线程等待loop_
    while (loop_ == NULL)
    {
      cond_.wait();
    }
    // loop对象有了(新线程创建的), 返回loop指针(给loops_列表)
    loop = loop_;
  }
  return loop;
}

void EventLoopThread::threadFunc() // 新线程创建后执行的函数
{
  // 新线程栈上创建eventloop对象, 关键啊, 这里也会配置好loop对象的threadId_
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);     // 执行线程初始化回调函数ThreadInitCallback
  }

  {
    MutexLockGuard lock(mutex_); 
    loop_ = &loop;     // loop_指向子线程内部的loop对象
    cond_.notify(); // 唤醒主线程已经有了loop_(主线程有指向子线程loop对象的指针, 可以退出阻塞)
  }
  // 新线程执行loop循环, 子线程会阻塞在这里, loop不会析构
  loop.loop();
  // 子线程执行loop()完毕
  MutexLockGuard lock(mutex_);
  loop_ = NULL;
  // 退出, 子线程析构loop对象
}

