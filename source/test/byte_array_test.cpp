#include "StarByteArray.hpp"
#include "StarEncode.hpp"
#include "StarDataStreamDevices.hpp"

#include "gtest/gtest.h"

using namespace Star;

TEST(ByteArrayTest, EmptyFillDoesNotAllocateOrPassNullStorageToMemset) {
  ByteArray bytes;
  bytes.fill('a'); bytes.fill(0, 'b');
  EXPECT_TRUE(bytes.empty()); EXPECT_EQ(bytes.ptr(), nullptr); EXPECT_EQ(bytes.capacity(), 0u);
  ByteArray constructed(0, 'c');
  EXPECT_TRUE(constructed.empty()); EXPECT_EQ(constructed.ptr(), nullptr); EXPECT_EQ(constructed.capacity(), 0u);
}

TEST(ByteArrayTest, EmptyFillKeepsReservedStorageWithoutWritingItsInactiveBytes) {
  ByteArray bytes(8, 'a'); auto storage = bytes.ptr(); auto capacity = bytes.capacity();
  bytes.fill(0, 'b'); bytes.fill('c');
  EXPECT_TRUE(bytes.empty()); EXPECT_EQ(bytes.ptr(), storage); EXPECT_EQ(bytes.capacity(), capacity);
  // All eight bytes were explicitly initialized before reducing the size.
  for (size_t i = 0; i < 8; ++i) EXPECT_EQ(storage[i], 'a');
  bytes.fill(3, 'd'); EXPECT_EQ(bytes.size(), 3u);
  for (auto value : bytes) EXPECT_EQ(value, 'd');
}

TEST(ByteArrayTest, FillWithoutResizeStillOverwritesEveryActiveByte) {
  ByteArray bytes(8, 'a'); auto storage = bytes.ptr(); auto capacity = bytes.capacity();
  bytes.fill('b');
  EXPECT_EQ(bytes.size(), 8u); EXPECT_EQ(bytes.ptr(), storage); EXPECT_EQ(bytes.capacity(), capacity);
  for (auto value : bytes) EXPECT_EQ(value, 'b');
}

TEST(ByteArrayTest, TakingStreamDataResetsTheMovedFromBufferIncludingASecondEmptyTake) {
  DataStreamBuffer stream; stream << int64_t(42);
  auto bytes = stream.takeData(); ASSERT_FALSE(bytes.empty());
  EXPECT_EQ(DataStreamBuffer::deserialize<int64_t>(bytes), 42);
  EXPECT_TRUE(stream.data().empty()); EXPECT_EQ(stream.pos(), 0u);
  auto empty = stream.takeData(); EXPECT_TRUE(empty.empty()); EXPECT_EQ(empty.ptr(), nullptr);
}

TEST(ByteArrayTest, All) {
  auto res = ByteArray::fromCString("foobar");
  res.insert(2, 'a');
  res.insert(6, 'b');
  res.push_back('c');
  res.insert(9, 'd');
  EXPECT_EQ(res, ByteArray::fromCString("foaobabrcd"));

  auto a = hexDecode("0a0a0a");
  auto b = hexDecode("a0a0a0");
  auto c = hexDecode("818181");
  auto d = hexDecode("aaaaaa");
  auto e = hexDecode("000000");
  auto f = hexDecode("212121");

  auto g = hexDecode("a0a0a0");
  auto h = hexDecode("8181818181");
  auto i = hexDecode("2121218181");

  EXPECT_EQ(a.andWith(b), e);
  EXPECT_EQ(a.orWith(b), d);
  EXPECT_EQ(b.xorWith(c), f);
  EXPECT_EQ(g.xorWith(h), f);
  EXPECT_EQ(g.xorWith(h, true), i);
  EXPECT_EQ(h.xorWith(g, true), i);
}
