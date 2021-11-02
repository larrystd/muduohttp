#include "muduo/base/ThreadPool.h"
#include "muduo/base/CountDownLatch.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/Logging.h"

#include <stdio.h>
#include <unistd.h>  // usleep

void print()
{
  printf("tid=%d\n", muduo::CurrentThread::tid());
}

void printString(const std::string& str)
{
  LOG_INFO << str;
  usleep(100*1000);
}

// 生产者单线程(只有主线程), 消费者多线程
void test(int maxSize)
{
  LOG_WARN << "Test ThreadPool with max queue size = " << maxSize;
  muduo::ThreadPool pool("MainThreadPool");
  // 设置线程池大小
  pool.setMaxQueueSize(maxSize);
  pool.start(5);   // 线程池执行任务(消费者), 等待任务队列非空。消费者有5个

  LOG_WARN << "Adding"; // 加入任务队列, 线程池会执行print
  pool.run(print);
  pool.run(print);
  for (int i = 0; i < 100; ++i)
  {
    char buf[32];
    snprintf(buf, sizeof buf, "task %d", i);
    // 执行任务
    pool.run(std::bind(printString, std::string(buf))); // 这里主线程作为生产者, 相当于循环生产了100task, 供五个消费者消费
  }
  LOG_WARN << "Done";

  muduo::CountDownLatch latch(1);
  pool.run(std::bind(&muduo::CountDownLatch::countDown, &latch));
  latch.wait();
  pool.stop();
}

/*
 * Wish we could do this in the future.
void testMove()
{
  muduo::ThreadPool pool;
  pool.start(2);

  std::unique_ptr<int> x(new int(42));
  pool.run([y = std::move(x)]{ printf("%d: %d\n", muduo::CurrentThread::tid(), *y); });
  pool.stop();
}
*/

void longTask(int num)
{
  LOG_INFO << "longTask " << num;
  muduo::CurrentThread::sleepUsec(3000000);
}

void test2()
{
  LOG_WARN << "Test ThreadPool by stoping early.";
  muduo::ThreadPool pool("ThreadPool");
  pool.setMaxQueueSize(5);
  pool.start(3);  // 3个消费者

  muduo::Thread thread1([&pool]() // thread1为生产者
  {
    for (int i = 0; i < 20; ++i)
    {
      pool.run(std::bind(longTask, i));
    }
  }, "thread1");
  thread1.start();

  //muduo::CurrentThread::sleepUsec(5000000);
  LOG_WARN << "stop pool";
  pool.stop();  // early stop

  thread1.join(); // 等待消费者线程消费完
  // run() after stop()
  //pool.run(print);
  LOG_WARN << "test2 Done";
}

void do_produce(std::function<void()> task) {
  task();
}

void test3(int maxSize)
{
  LOG_WARN << "Test ThreadPool with max queue size = " << maxSize;
  muduo::ThreadPool pool_comsumer("comsumer");
  // 设置线程池大小
  pool_comsumer.setMaxQueueSize(maxSize);
  pool_comsumer.start(5);   // 线程池执行任务(消费者), 等待任务队列非空。消费者有5个

  std::function<void()> task = std::bind(&muduo::ThreadPool::run, &pool_comsumer, print);
  //std::function<void()> f = std::bind(&muduo::ThreadPool::run, &pool_comsumer);
  //task();
  LOG_INFO<<"PRODUCE";
  for (int i = 0; i < 2; i++) {
    muduo::ThreadPool pool_produce("proceuce");
    muduo::Thread t(task, "produce");
    t.start();
  }

  //for (int i = 0; i < 100; ++i)
  //{
  //  char buf[32];
  //  snprintf(buf, sizeof buf, "task %d", i);
    // 执行任务
  //  pool.run(std::bind(printString, std::string(buf))); // 这里主线程作为生产者, 相当于循环生产了100task, 供五个消费者消费
  //}
  LOG_WARN << "Done";
  pool_comsumer.stop();
}

int main()
{
  //test(0);
  // test(1);
  //test(5);
  //test(10);
  //test(50);
  test3(5);
  return 0;
}
