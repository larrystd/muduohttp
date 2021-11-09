#ifndef MUDUO_NET_ENDIAN_H_
#define MUDUO_NET_ENDIAN_H_

#include <stdint.h>
#include <endian.h>

namespace muduo
{
namespace net
{
namespace sockets
{

// the inline assembler code makes type blur,
// so we disable warnings for a while.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"

/// convert the byte encoding of integer values from the byte order that the current CPU (the "host") uses,

/// 网络字节序采用big endian排序方式。常见主机则是小端字节序

/// 字节序输入输出前后都是uint64_t, htobe64表示64位主机字节序转网络字节, be64toh表示64位网络字节转主机字节

/// 01111111 00000000 00000000 00000001 =   2130706433   （主机字节序）

/// 00000001 00000000 00000000 01111111 =   16777343 （网络字节序）

inline uint64_t hostToNetwork64(uint64_t host64)
{
  return htobe64(host64);
}

inline uint32_t hostToNetwork32(uint32_t host32)
{
  return htobe32(host32);
}

inline uint16_t hostToNetwork16(uint16_t host16)
{
  return htobe16(host16);
}

inline uint64_t networkToHost64(uint64_t net64)
{
  return be64toh(net64);
}

inline uint32_t networkToHost32(uint32_t net32)
{
  return be32toh(net32);
}

inline uint16_t networkToHost16(uint16_t net16)
{
  return be16toh(net16);
}

#pragma GCC diagnostic pop

}  // namespace sockets
}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_ENDIAN_H
