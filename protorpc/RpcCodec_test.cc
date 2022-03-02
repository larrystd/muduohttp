#include "protorpc/RpcCodec.h"
#include "protorpc/rpc.pb.h"
#include "protorpc/muduo_protobuf_codec/ProtobufCodecLite.h" // 来自protorpc
#include "muduo/net/Buffer.h"

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

void rpcMessageCallback(const TcpConnectionPtr&,
                        const RpcMessagePtr&,
                        Timestamp)  // message callback
{

}

MessagePtr g_msgptr;

void messageCallback(const TcpConnectionPtr&,
                    const MessagePtr& msg,
                    Timestamp)
{
  g_msgptr = msg;
}

void print(const Buffer& buf) {
  printf("encoded to %zd bytes\n", buf.readableBytes());  // 可读的字节
  for (size_t i = 0; i < buf.readableBytes(); ++i) {  // 打印buf的内容
    unsigned char ch = static_cast<unsigned char>(buf.peek()[i]);

    printf("%2zd:  0x%02x  %c\n", i, ch, isgraph(ch) ? ch : ' ');
  }
}

char rpctag[] = "RPC0";


int main() {
  RpcMessage message; // RpcMessage是protobuf的 message class
  message.set_type(REQUEST);  // 设置一些内容
  message.set_id(2);
  
  char wire[] = "\0\0\0\x13" "RPC0" "\x08\x01\x11\x02\0\0\0\0\0\0\0" "\x0f\xef\x01\x32";
  string expected(wire, sizeof(wire)-1);
  string s1, s2;
  Buffer buf1, buf2;  // 定义缓冲区

  // 以下在测试编码
  {
    RpcCodec codec(rpcMessageCallback); // 使用RpcCodec, ProtobufCodecLiteT<RpcMessage, rpctag>
    codec.fillEmptyBuffer(&buf1, message);  // 这里将codec对象序列化到buf1中
    print(buf1);  // 序列化之后的对象
    s1 = buf1.toStringPiece().as_string();
  }

  {
    ProtobufCodecLite codec(&RpcMessage::default_instance(), "RPC0", messageCallback);
    codec.fillEmptyBuffer(&buf2, message);
    print(buf2);
    s2 = buf2.toStringPiece().as_string();
    codec.onMessage(TcpConnectionPtr(), &buf1, Timestamp::now());

    assert(g_msgptr);
    assert(g_msgptr->DebugString() == message.DebugString());
    g_msgptr.reset();
  }

  assert(s1 == s2);
  assert(s1 == expected);
  assert(s2 == expected);

  {
    Buffer buf;
    ProtobufCodecLite codec(&RpcMessage::default_instance(), "XYZ", messageCallback);
    codec.fillEmptyBuffer(&buf, message); // 序列化，并写入到buf中
    print(buf); // buf的内容

    s2 = buf.toStringPiece().as_string();
    codec.onMessage(TcpConnectionPtr(), &buf, Timestamp::now());
    assert(g_msgptr);
    assert(g_msgptr->DebugString() == message.DebugString());
  }

  google::protobuf::ShutdownProtobufLibrary();
}
