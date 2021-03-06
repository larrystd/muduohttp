### 封装一个pthread

首先，我们需要建立一个thread类，该类不能被拷贝。

常见的thread功能要有，比如start, join, id, name。
构造函数和析构函数要有，thread类需要传入一个function用来进行执行。也就是std::function<void ()>

这样我们就可以有了thread的.h文件。
```cpp
class Thread : noncopyable {
public:
    typedef std::function<void()> ThreadFunc;
    explicit Thread (ThreadFunc, const string& name = string());
    ~Thread();

    void start();
    int join();

    // 一些判断函数，
    bool started() const { return started_; }
    ...
private:
    void setDefaultName();
    bool       started_;
    bool       joined_;
    pthread_t  pthreadId_;
    ...
};

下面是Thread.cc，实现部分。

首先需要一个struct ThreadData，来封装一些线程私有变量。
```cpp
  ThreadData(ThreadFunc func,
             const string& name,
             pid_t* tid,
             CountDownLatch* latch)
    : func_(std::move(func)),
      name_(name),
      tid_(tid),
      latch_(latch)
  { }
// 具体执行线程内函数
void runInThread()
// 开启线程，会执行runInThread()
void* startThread(void* obj)

```
注意创建线程的方法
```cpp
// 构造函数，初始化传入变量
Thread::Thread(ThreadFunc func, const string& n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(0),
    func_(std::move(func)),
    name_(n),
    latch_(1)
{
  setDefaultName();
}

void Thread::start()
{
    ....
    // 创建ThreadData类型
    detail::ThreadData* data = new detail::ThreadData(func_, name_, &tid_, &latch_);
    // pthread_create调用startThread， 参数为ThreadData* data。注意保存pthreadId_
  if (pthread_create(&pthreadId_, NULL, &detail::startThread, data))
  // On success, pthread_create() returns 0; 因此如果创建成功不会执行if语句
  {
    started_ = false;
    delete data; // or no delete?
    LOG_SYSFATAL << "Failed in pthread_create";
  }
  else
  // 创建成功
  {
    latch_.wait();  // 倒计时等待，默认为1
    assert(tid_ > 0);
  }

// join的实现，注意验证started，以及!joined
int Thread::join()
{
  assert(started_);
  assert(!joined_);
  joined_ = true;
  return pthread_join(pthreadId_, NULL);
}
```

### 处理thread_local 线程私有变量
线程局部存储(Thread Local Storage，TLS)是一种存储期(storage duration)，对象的存储是在线程开始时分配，线程结束时回收，每个线程有该对象自己的实例。
换句话说，是在多线程编程的环境中给全局或静态的变量每个线程分配不同的存储空间互不干扰。

Thread_local类是一个单例类，需要把构造函数和析构函数禁用。
将对象设置为static类型，保证了单例。同时可以通过类名直接调用对象。

```cpp
  static __thread T* t_value_;
  static Deleter deleter_;  // 可以直接在外界调用deleter_, 而不用手动new
// 通过类名直接调用t_value_和deleter_
template<typename T>
__thread T* ThreadLocalSingleton<T>::t_value_ = 0;

template<typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::deleter_;

// 通过pthread来控制处理线程内部变量

pthread_key_create
pthread_key_delete
pthread_setspecific

// 获取单例，首先设置好t_value_变量，并交由deleter_加入到线程内部。返回加入后该变量的地址。
  static T& instance()
  {
    if (!t_value_)
    {
      t_value_ = new T(); // 线程内部对象
      deleter_.set(t_value_);
    }
    return *t_value_;
  }
```

### 封装一个thread_pool线程池
首先线程池需要封装一个vector的线程，Task组成的队列
Task就是std::function<void()>

run(Task task)函数设置执行task，但倘若有其他任务正在执行，当前task入队列

start函数线程池执行，实际上是对线程vector依次执行。执行函数为runInThread，也就是muduo::Thread(std::bind(&ThreadPool::runInThread, this)

执行task的函数为ThreadPool::runInThread(),在执行前调用task()从队列中拿出任务，也就是一个thread执行队列中的一个任务，如果没有任务需要等待（当前线程也就不会执行直到任务push)。

running 变量控制线程的执行，stop()会设置running=false,从而停止执行

在执行时只需要不断调用run(Task task)来执行新的task，注意线程池内的线程执行完后会退出，也就是size大小的线程池最多执行size个任务。
```cpp

class ThreadPool : noncopyable
{
  typedef std::function<void ()> Task;

  explicit ThreadPool(const string& nameArg = string("ThreadPool"));
  ~ThreadPool();

  std::vector<std::unique_ptr<muduo::Thread>> threads_;
  std::deque<Task> queue_ GUARDED_BY(mutex_);


void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;
  // 分配空间
  threads_.reserve(numThreads);
  for (int i = 0; i < numThreads; ++i)
  {
    threads_.emplace_back(new muduo::Thread(
          std::bind(&ThreadPool::runInThread, this), name_+id));
    threads_[i]->start();
  }
}

void ThreadPool::run(Task task)
{
  if (threads_.empty())
  {
    task();
  }
  // 入队列
  else
  {
    MutexLockGuard lock(mutex_);
    while (isFull() && running_)
    {
      notFull_.wait();
    }
    if (!running_) return;
    assert(!isFull());

    queue_.push_back(std::move(task));
    notEmpty_.notify();
  }
}

void ThreadPool::runInThread()
{
  try
  {
    while (running_)
    {
      Task task(take());
      if (task)
      {
        task();
      }
    }
  }

ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  while (queue_.empty() && running_)
  {
    notEmpty_.wait();
  }
  Task task;
  if (!queue_.empty())
  {
    task = queue_.front();
    queue_.pop_front();
    if (maxQueueSize_ > 0)
    {
      notFull_.notify();
    }
  }
  return task;
}
```


