#include "muduo/net/Buffer.h"

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

// boost的单元测试库

using muduo::string;
using muduo::net::Buffer;

// 测试case1
BOOST_AUTO_TEST_CASE(testBufferAppendRetrieve)
{
  Buffer buf;
  // 单元测试
  BOOST_CHECK_EQUAL(buf.readableBytes(), 0);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize);
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend);

  const string str(200, 'x'); // 包含200个x的string
  buf.append(str);  // 放入buf
  BOOST_CHECK_EQUAL(buf.readableBytes(), str.size()); // 可读大小正好为str.size()
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize - str.size());  // 可写的大小
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend);

  const string str2 =  buf.retrieveAsString(50);  // 从buf中读取50个字符
  BOOST_CHECK_EQUAL(str2.size(), 50);
  BOOST_CHECK_EQUAL(buf.readableBytes(), str.size() - str2.size()); // 可读字节大小变成了200-50
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize - str.size());
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend + str2.size());
  BOOST_CHECK_EQUAL(str2, string(50, 'x'));

  buf.append(str);
  BOOST_CHECK_EQUAL(buf.readableBytes(), 2*str.size() - str2.size());
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize - 2*str.size());
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend + str2.size());

  const string str3 =  buf.retrieveAllAsString();
  BOOST_CHECK_EQUAL(str3.size(), 350);
  BOOST_CHECK_EQUAL(buf.readableBytes(), 0);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize);
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend);
  BOOST_CHECK_EQUAL(str3, string(350, 'x'));
}

// 测试case2
BOOST_AUTO_TEST_CASE(testBufferGrow)
{
  Buffer buf;
  buf.append(string(400, 'y'));
  BOOST_CHECK_EQUAL(buf.readableBytes(), 400);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize-400);

  buf.retrieve(50);
  BOOST_CHECK_EQUAL(buf.readableBytes(), 350);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize-400);
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend+50);

  buf.append(string(1000, 'z'));
  BOOST_CHECK_EQUAL(buf.readableBytes(), 1350);
  BOOST_CHECK_EQUAL(buf.writableBytes(), 0);
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend+50); // FIXME

  buf.retrieveAll();
  BOOST_CHECK_EQUAL(buf.readableBytes(), 0);
  BOOST_CHECK_EQUAL(buf.writableBytes(), 1400); // FIXME
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend);
}

BOOST_AUTO_TEST_CASE(testBufferInsideGrow)
{
  Buffer buf;
  buf.append(string(800, 'y'));
  BOOST_CHECK_EQUAL(buf.readableBytes(), 800);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize-800);

  buf.retrieve(500);
  BOOST_CHECK_EQUAL(buf.readableBytes(), 300);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize-800);
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend+500);

  buf.append(string(300, 'z'));
  BOOST_CHECK_EQUAL(buf.readableBytes(), 600);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize-600);
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend);
}

BOOST_AUTO_TEST_CASE(testBufferShrink)
{
  Buffer buf;
  buf.append(string(2000, 'y'));
  BOOST_CHECK_EQUAL(buf.readableBytes(), 2000);
  BOOST_CHECK_EQUAL(buf.writableBytes(), 0);
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend);

  buf.retrieve(1500);
  BOOST_CHECK_EQUAL(buf.readableBytes(), 500);
  BOOST_CHECK_EQUAL(buf.writableBytes(), 0);
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend+1500);

  buf.shrink(0);
  BOOST_CHECK_EQUAL(buf.readableBytes(), 500);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize-500);
  BOOST_CHECK_EQUAL(buf.retrieveAllAsString(), string(500, 'y'));
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend);
}

BOOST_AUTO_TEST_CASE(testBufferPrepend)
{
  Buffer buf;
  buf.append(string(200, 'y'));
  BOOST_CHECK_EQUAL(buf.readableBytes(), 200);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize-200);
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend);

  int x = 0;
  buf.prepend(&x, sizeof x);
  BOOST_CHECK_EQUAL(buf.readableBytes(), 204);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize-200);
  BOOST_CHECK_EQUAL(buf.prependableBytes(), Buffer::kCheapPrepend - 4);
}

BOOST_AUTO_TEST_CASE(testBufferReadInt)
{
  Buffer buf;
  buf.append("HTTP");

  BOOST_CHECK_EQUAL(buf.readableBytes(), 4);
  BOOST_CHECK_EQUAL(buf.peekInt8(), 'H');
  int top16 = buf.peekInt16();
  BOOST_CHECK_EQUAL(top16, 'H'*256 + 'T');
  BOOST_CHECK_EQUAL(buf.peekInt32(), top16*65536 + 'T'*256 + 'P');

  BOOST_CHECK_EQUAL(buf.readInt8(), 'H');
  BOOST_CHECK_EQUAL(buf.readInt16(), 'T'*256 + 'T');
  BOOST_CHECK_EQUAL(buf.readInt8(), 'P');
  BOOST_CHECK_EQUAL(buf.readableBytes(), 0);
  BOOST_CHECK_EQUAL(buf.writableBytes(), Buffer::kInitialSize);

  buf.appendInt8(-1);
  buf.appendInt16(-2);
  buf.appendInt32(-3);
  BOOST_CHECK_EQUAL(buf.readableBytes(), 7);
  BOOST_CHECK_EQUAL(buf.readInt8(), -1);
  BOOST_CHECK_EQUAL(buf.readInt16(), -2);
  BOOST_CHECK_EQUAL(buf.readInt32(), -3);
}

BOOST_AUTO_TEST_CASE(testBufferFindEOL)
{
  Buffer buf;
  buf.append(string(100000, 'x'));
  const char* null = NULL;
  BOOST_CHECK_EQUAL(buf.findEOL(), null);
  BOOST_CHECK_EQUAL(buf.findEOL(buf.peek()+90000), null);
}

void output(Buffer&& buf, const void* inner)
{
  Buffer newbuf(std::move(buf));
  // printf("New Buffer at %p, inner %p\n", &newbuf, newbuf.peek());
  BOOST_CHECK_EQUAL(inner, newbuf.peek());
}

// NOTE: This test fails in g++ 4.4, passes in g++ 4.6.
BOOST_AUTO_TEST_CASE(testMove)
{
  Buffer buf;
  buf.append("muduo", 5); // 添加字符到缓冲区
  const void* inner = buf.peek(); // 可读索引
  // printf("Buffer at %p, inner %p\n", &buf, inner);
  output(std::move(buf), inner);  // 调用out
}
