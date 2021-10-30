#ifndef MUDUO_BASE_COPYABLE_H_
#define MUDUO_BASE_COPYABLE_H_

namespace muduo
{
/// 构造函数和析构函数设置为protected + default, 继承者可以正常实现构造函数和析构函数
class copyable
{
 protected:
  copyable() = default;
  ~copyable() = default;
};

}  // namespace muduo

#endif  // MUDUO_BASE_COPYABLE_H_
