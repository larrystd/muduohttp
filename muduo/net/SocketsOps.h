#ifndef MUDUO_NET_SOCKETSOPS_H_
#define MUDUO_NET_SOCKETSOPS_H_

#include <arpa/inet.h>

namespace muduo
{
namespace net
{
namespace sockets
{

///
/// Creates a non-blocking socket file descriptor,
/// abort if any error.
int createNonblockingOrDie(sa_family_t family); // unsigned short int

// 操作sockaddr*
int  connect(int sockfd, const struct sockaddr* addr);  // 某个fd作为去往addr的连接, sockaddr是包含char[14]的结构体
void bindOrDie(int sockfd, const struct sockaddr* addr);    // server, 将serverfd绑定在某个addr
void listenOrDie(int sockfd);   // 在serverfd上监听
int  accept(int sockfd, struct sockaddr_in6* addr); // accept某个连接,addr是客户端的
ssize_t read(int sockfd, void *buf, size_t count);  // read count 字节到buf
ssize_t readv(int sockfd, const struct iovec *iov, int iovcnt); // 成批的读取并格式化为iovcnt个iovec
ssize_t write(int sockfd, const void *buf, size_t count);   // write count个字节到buf中
void close(int sockfd); // 关闭某个fd连接
void shutdownWrite(int sockfd);

void toIpPort(char* buf, size_t size,
              const struct sockaddr* addr);
void toIp(char* buf, size_t size,
          const struct sockaddr* addr);

void fromIpPort(const char* ip, uint16_t port,
                struct sockaddr_in* addr);
void fromIpPort(const char* ip, uint16_t port,
                struct sockaddr_in6* addr);

int getSocketError(int sockfd);

// sockaddr_in和sockaddr的相互转换， sockaddr和sockaddr_in包含的数据都是一样的，但他们在使用上有区别：
// 程序员应使用sockaddr_in来表示地址，sockaddr_in区分了地址和端口, sockaddr是操作系统使用
const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr);
const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
struct sockaddr* sockaddr_cast(struct sockaddr_in6* addr);
const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr);
const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr* addr);

// 得到某个socket的本地地址和外地地址
struct sockaddr_in6 getLocalAddr(int sockfd);
struct sockaddr_in6 getPeerAddr(int sockfd);
bool isSelfConnect(int sockfd);

}  // namespace sockets
}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_SOCKETSOPS_H_
