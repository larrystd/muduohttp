#ifndef MUDUO_BASE_NONCOPYABLE_H
#define MUDUO_BASE_NONCOPYABLE_H

namespace muduo
{

class noncopyable

// 注意建立一个子对象实际上包含父类成分和子类成分
// 派生类调构造函数会先调父类构造函数, 拷贝构造也一样; 而析构时先调用子类的析构函数析构子类成分，再调用父类的析构函数析构父类成分
// 拷贝构造函数和操作符设置delete, 继承者不能使用构造函数, 且编译期就报错; 如果将其设置为private, 则是运行期报错。
// 构造函数析构函数protected+ default, 继承者可以正常调用构造函数和析构函数
{
 public:
  noncopyable(const noncopyable&) = delete;
  void operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};

}  // namespace muduo

#endif  // MUDUO_BASE_NONCOPYABLE_H
