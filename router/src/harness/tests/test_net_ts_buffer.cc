/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/net_ts/buffer.h"

#include <cstring>  // memset
#include <list>

#include <gtest/gtest.h>

static_assert(net::is_mutable_buffer_sequence<net::mutable_buffer>::value,
              "net::mutable_buffer MUST be a mutable_buffer_sequence");

static_assert(net::is_const_buffer_sequence<net::const_buffer>::value,
              "net::const_buffer MUST be a const_buffer_sequence");

static_assert(net::is_const_buffer_sequence<net::mutable_buffer>::value);
static_assert(!net::is_mutable_buffer_sequence<net::const_buffer>::value);

static_assert(
    net::is_const_buffer_sequence<std::vector<net::const_buffer>>::value,
    "std::vector<net::const_buffer> MUST be a const_buffer_sequence");

static_assert(
    net::is_mutable_buffer_sequence<std::vector<net::mutable_buffer>>::value,
    "std::vector<net::mutable_buffer> MUST be a mutable_buffer_sequence");

static_assert(net::is_dynamic_buffer<net::dynamic_string_buffer<
                  std::string::value_type, std::string::traits_type,
                  std::string::allocator_type>>::value,
              "dynamic_string_buffer MUST be a dynamic-buffer");

static_assert(net::is_dynamic_buffer<net::dynamic_vector_buffer<
                  std::vector<uint8_t>::value_type,
                  std::vector<uint8_t>::allocator_type>>::value,
              "dynamic_vector_buffer MUST be a dynamic-buffer");

static_assert(
    net::is_const_buffer_sequence<
        net::prepared_buffers<net::const_buffer>>::value,
    "net::prepared_buffers<net::const_buffer> MUST be a const_buffer_sequence");

TEST(net_buffer, from_string_view) {
  std::string_view o{"abc"};
  auto b = net::buffer(o);

  EXPECT_EQ(b.size(), o.size());
  EXPECT_EQ(b.data(), o.data());
}

TEST(net_buffer, from_empty_string_view) {
  std::string_view o;
  auto b = net::buffer(o);

  EXPECT_EQ(b.size(), o.size());
  EXPECT_EQ(b.data(), nullptr);
}

TEST(net_buffer, from_string) {
  std::string o{"abc"};
  auto b = net::buffer(o);

  EXPECT_EQ(b.size(), o.size());
  EXPECT_EQ(b.data(), o.data());
}

TEST(net_buffer, from_empty_string) {
  std::string o;
  auto b = net::buffer(o);

  EXPECT_EQ(b.size(), o.size());
  EXPECT_EQ(b.data(), nullptr);
}

TEST(net_buffer, from_vector) {
  std::vector<char> o{'a', 'b', 'c'};
  auto b = net::buffer(o);

  EXPECT_EQ(b.size(), o.size());
  EXPECT_EQ(b.data(), o.data());
}

TEST(net_buffer, from_empty_vector) {
  std::vector<char> o;
  auto b = net::buffer(o);

  EXPECT_EQ(b.size(), o.size());
  EXPECT_EQ(b.data(), nullptr);
}

TEST(dynamic_string_buffer, size_empty) {
  std::string s;
  auto sb = net::dynamic_buffer(s);

  EXPECT_EQ(sb.size(), s.size());
  EXPECT_EQ(sb.capacity(), s.capacity());
}

TEST(dynamic_string_buffer, size_non_empty) {
  std::string s("aaaaaaaa");
  auto sb = net::dynamic_buffer(s);

  EXPECT_EQ(sb.size(), s.size());
  EXPECT_EQ(sb.capacity(), s.capacity());
}

TEST(dynamic_string_buffer, grow_from_empty) {
  std::string s;
  auto dyn_buf = net::dynamic_buffer(s);

  EXPECT_EQ(dyn_buf.size(), 0);

  dyn_buf.grow(16);

  EXPECT_EQ(dyn_buf.size(), 16);

  dyn_buf.grow(16);

  EXPECT_EQ(dyn_buf.size(), 32);
}

TEST(dynamic_string_buffer, commit) {
  std::string s;
  auto dyn_buf = net::dynamic_buffer(s);

  EXPECT_EQ(dyn_buf.size(), 0);

  dyn_buf.grow(16);

  {
    auto b = dyn_buf.data(0, 16);

    // prepare should return a buffer of the expected size
    EXPECT_EQ(b.size(), 16);

    std::memset(b.data(), 'a', b.size());

    // underlying storage should have the expected content.
    EXPECT_STREQ(s.c_str(), "aaaaaaaaaaaaaaaa");
  }

  SCOPED_TRACE("// prepare next block");

  {
    dyn_buf.grow(16);

    auto b = dyn_buf.data(16, 16);

    EXPECT_EQ(b.size(), 16);

    std::memset(b.data(), 'b', b.size());
  }

  EXPECT_EQ(dyn_buf.size(), 32);
  EXPECT_EQ(s.size(), 32);

  EXPECT_STREQ(s.c_str(), "aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbb");
}

// consume() always succeeds
TEST(dynamic_string_buffer, consume_from_empty) {
  std::string s;
  auto dyn_buf = net::dynamic_buffer(s);
  EXPECT_EQ(dyn_buf.size(), 0);

  dyn_buf.consume(0);

  EXPECT_EQ(dyn_buf.size(), 0);
  dyn_buf.consume(16);

  EXPECT_EQ(dyn_buf.size(), 0);
}

TEST(dynamic_string_buffer, consume_from_non_empty) {
  std::string s("aabb");
  auto dyn_buf = net::dynamic_buffer(s);

  EXPECT_EQ(dyn_buf.size(), 4);

  dyn_buf.consume(0);

  EXPECT_EQ(dyn_buf.size(), 4);
  dyn_buf.consume(2);

  EXPECT_EQ(dyn_buf.size(), 2);
  EXPECT_EQ(s.size(), 2);
  EXPECT_STREQ(s.c_str(), "bb");
  dyn_buf.consume(16);

  EXPECT_EQ(dyn_buf.size(), 0);
  EXPECT_EQ(s.size(), 0);
}

TEST(dynamic_string_buffer, grow_and_consume) {
  std::string s;
  auto dyn_buf = net::dynamic_buffer(s);

  // add 'aaaa' into the string
  {
    auto orig_size = dyn_buf.size();
    auto grow_size = 4;
    dyn_buf.grow(grow_size);
    EXPECT_EQ(dyn_buf.size(), orig_size + grow_size);

    auto b = dyn_buf.data(orig_size, grow_size);
    std::memset(b.data(), 'a', b.size());
  }
  EXPECT_EQ(s, "aaaa");

  EXPECT_EQ(dyn_buf.size(), 4);

  {
    auto orig_size = dyn_buf.size();
    auto grow_size = 4;

    dyn_buf.grow(grow_size);
    EXPECT_EQ(dyn_buf.size(), orig_size + grow_size);

    auto b = dyn_buf.data(orig_size, grow_size);
    std::memset(b.data(), 'b', b.size());
  }
  EXPECT_EQ(s, "aaaabbbb");
  EXPECT_EQ(dyn_buf.size(), 8);

  // consume 2 bytes
  dyn_buf.consume(2);
  EXPECT_EQ(dyn_buf.size(), 6);

  EXPECT_EQ(s, "aabbbb");

  // and append something again
  {
    auto orig_size = dyn_buf.size();
    auto grow_size = 2;

    dyn_buf.grow(grow_size);
    EXPECT_EQ(dyn_buf.size(), orig_size + grow_size);

    auto b = dyn_buf.data(orig_size, grow_size);
    std::memset(b.data(), 'a', b.size());
  }

  EXPECT_EQ(dyn_buf.size(), 8);
  EXPECT_EQ(s, "aabbbbaa");
}

class ConsumingBuffers : public ::testing::Test {
 public:
  using T = std::list<std::string>;

  void SetUp() override {
    bufs.emplace_back("0123");
    bufs.emplace_back("45");
    bufs.emplace_back("6789");
  }

  void TearDown() override { bufs.clear(); }
  T bufs;
};

TEST_F(ConsumingBuffers, prepare_nothing) {
  net::consuming_buffers<T, net::const_buffer> buf_seq(bufs);

  // prepare nothing
  auto b = buf_seq.prepare(0);
  EXPECT_EQ(b.size(), 0);

  // nothing is consumed
  EXPECT_EQ(buf_seq.total_consumed(), 0);
}

TEST_F(ConsumingBuffers, prepare_one_buf) {
  net::consuming_buffers<T, net::const_buffer> buf_seq(bufs);

  // prepare something, which spans one buffer

  auto b = buf_seq.prepare(1);
  EXPECT_EQ(b.size(), 1);
  EXPECT_LE(b.size(), b.max_size());

  // nothing is consumed
  EXPECT_EQ(buf_seq.total_consumed(), 0);
}

TEST_F(ConsumingBuffers, prepare_two_buf) {
  net::consuming_buffers<T, net::const_buffer> buf_seq(bufs);

  // prepare something which spans 2 buffers

  auto b = buf_seq.prepare(5);
  EXPECT_EQ(b.size(), 2);
  EXPECT_LE(b.size(), b.max_size());

  // nothing is consumed
  EXPECT_EQ(buf_seq.total_consumed(), 0);
}

// prepare something which spans 3 buffers
TEST_F(ConsumingBuffers, prepare_3_buf) {
  net::consuming_buffers<T, net::const_buffer> buf_seq(bufs);

  auto b = buf_seq.prepare(7);
  EXPECT_EQ(b.size(), 3);
  EXPECT_LE(b.size(), b.max_size());

  // nothing is consumed
  EXPECT_EQ(buf_seq.total_consumed(), 0);
}

TEST_F(ConsumingBuffers, prepare_all) {
  net::consuming_buffers<T, net::const_buffer> buf_seq(bufs);

  // prepare all
  auto b = buf_seq.prepare(1024);
  EXPECT_EQ(b.size(), 3);
  EXPECT_LE(b.size(), b.max_size());
  buf_seq.consume(0);

  // nothing is consumed
  EXPECT_EQ(buf_seq.total_consumed(), 0);
}

TEST_F(ConsumingBuffers, consume_none) {
  net::consuming_buffers<T, net::const_buffer> buf_seq(bufs);

  buf_seq.consume(0);

  // nothing is consumed
  EXPECT_EQ(buf_seq.total_consumed(), 0);
}

TEST_F(ConsumingBuffers, consume_some_1) {
  net::consuming_buffers<T, net::const_buffer> buf_seq(bufs);

  // skip one
  buf_seq.consume(1);

  // prepare one
  auto prep_bufs = buf_seq.prepare(1);
  EXPECT_EQ(prep_bufs.size(), 1);

  auto cur = prep_bufs.begin();
  EXPECT_EQ(std::vector<char>(
                static_cast<const uint8_t *>(cur->data()),
                static_cast<const uint8_t *>(cur->data()) + cur->size()),
            (std::vector<char>{'1'}));

  EXPECT_EQ(buf_seq.total_consumed(), 1);
}

TEST_F(ConsumingBuffers, consume_some_2) {
  net::consuming_buffers<T, net::const_buffer> buf_seq(bufs);

  // skip one
  buf_seq.consume(1);

  // prepare something which spans 2 buffers
  auto prep_bufs = buf_seq.prepare(5);
  EXPECT_EQ(prep_bufs.size(), 2);

  auto cur = prep_bufs.begin();
  EXPECT_EQ(std::vector<char>(
                static_cast<const uint8_t *>(cur->data()),
                static_cast<const uint8_t *>(cur->data()) + cur->size()),
            (std::vector<char>{'1', '2', '3'}));

  std::advance(cur, 1);

  EXPECT_EQ(std::vector<char>(
                static_cast<const uint8_t *>(cur->data()),
                static_cast<const uint8_t *>(cur->data()) + cur->size()),
            (std::vector<char>{'4', '5'}));

  // prepare doesn't consume
  EXPECT_EQ(buf_seq.total_consumed(), 1);
}

TEST_F(ConsumingBuffers, consume_some_3) {
  net::consuming_buffers<T, net::const_buffer> buf_seq(bufs);

  // skip first block
  buf_seq.consume(4);

  // prepare something which spans 2 buffers
  auto prep_bufs = buf_seq.prepare(6);
  EXPECT_EQ(prep_bufs.size(), 2);

  auto cur = prep_bufs.begin();
  EXPECT_EQ(std::vector<char>(
                static_cast<const uint8_t *>(cur->data()),
                static_cast<const uint8_t *>(cur->data()) + cur->size()),
            (std::vector<char>{'4', '5'}));

  std::advance(cur, 1);

  EXPECT_EQ(std::vector<char>(
                static_cast<const uint8_t *>(cur->data()),
                static_cast<const uint8_t *>(cur->data()) + cur->size()),
            (std::vector<char>{'6', '7', '8', '9'}));

  // prepare doesn't consume
  EXPECT_EQ(buf_seq.total_consumed(), 4);
}

TEST_F(ConsumingBuffers, consume_some_all) {
  net::consuming_buffers<T, net::const_buffer> buf_seq(bufs);

  // consume all
  buf_seq.consume(10);

  // consumed more than we had
  EXPECT_EQ(buf_seq.total_consumed(), 10);
}

/**
 * a socket (SyncStream) which would-block after some bytes are written.
 *
 * satisfies the requirements of SyncWriteStream.
 */
class WouldBlockSyncStream {
 public:
  WouldBlockSyncStream(size_t block_after) : block_after_{block_after} {}

  template <class ConstBufferSequence>
  stdx::expected<size_t, std::error_code> write_some(
      const ConstBufferSequence &buffer_seq) {
    const auto buf_size = net::buffer_size(buffer_seq);

    // if there is nothing to write(), return 0
    if (buf_size == 0) return 0;

    // time to block?
    if (block_after_ == 0) {
      return stdx::unexpected(
          make_error_code(std::errc::operation_would_block));
    }

    const auto written = std::min(buf_size, block_after_);

    block_after_ -= written;

    return written;
  }

 private:
  size_t block_after_{};
};

/**
 * check a write which blocks directly returns the expected error-code.
 *
 * - ConstBufferSequence.
 */
TEST(NetWrite, write_would_block_const_buffer) {
  WouldBlockSyncStream sock{0};

  // just some data.
  std::vector<uint8_t> buf{0x00, 0x01, 0x02, 0x03};

  const auto res = net::write(sock, net::buffer(buf));
  EXPECT_EQ(
      res, stdx::unexpected(make_error_code(std::errc::operation_would_block)));
}

/**
 * check a write which blocks directly returns the expected error-code.
 *
 * - DynamicBuffer.
 */
TEST(NetWrite, write_would_block_dynamic_buffer) {
  WouldBlockSyncStream sock{0};

  // just some data.
  std::vector<uint8_t> buf{0x00, 0x01, 0x02, 0x03};

  const auto res = net::write(sock, net::dynamic_buffer(buf));
  EXPECT_EQ(
      res, stdx::unexpected(make_error_code(std::errc::operation_would_block)));
}

/**
 * check a partial write, returns the right written-count.
 *
 * - ConstBufferSequence.
 */
TEST(NetWrite, write_some_const_buffer) {
  WouldBlockSyncStream sock{2};

  // just some data.
  std::vector<uint8_t> buf{0x00, 0x01, 0x02, 0x03};

  const auto res = net::write(sock, net::buffer(buf));
  ASSERT_TRUE(res) << res.error();

  EXPECT_EQ(res.value(), 2);
}

/**
 * check a partial write, returns the right written-count.
 *
 * - DynamicBuffer.
 */
TEST(NetWrite, write_some_dynamic_buffer) {
  WouldBlockSyncStream sock{2};

  // just some data.
  std::vector<uint8_t> buf{0x00, 0x01, 0x02, 0x03};

  const auto res = net::write(sock, net::dynamic_buffer(buf));
  ASSERT_TRUE(res) << res.error();

  EXPECT_EQ(res.value(), 2);
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
