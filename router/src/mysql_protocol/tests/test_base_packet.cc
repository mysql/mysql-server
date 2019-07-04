/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gmock/gmock.h>

#include <string.h>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "helpers/router_test_helpers.h"
#include "mysqlrouter/mysql_protocol.h"
#include "mysqlrouter/utils.h"

using ::testing::ContainerEq;
using ::testing::NotNull;
using std::string;

using mysql_protocol::Packet;
using namespace mysql_protocol;

class MySQLProtocolPacketTest : public ::testing::Test {
 public:
  Packet::vector_t case1 = {0x04, 0x0, 0x0, 0x01, 't', 'e', 's', 't'};

 protected:
  virtual void SetUp() {}
};

TEST_F(MySQLProtocolPacketTest, Constructors) {
  {
    auto p = Packet();
    EXPECT_EQ(0, p.get_sequence_id());
    EXPECT_EQ(0U, p.get_capabilities().bits());
    EXPECT_EQ(0UL, p.get_payload_size());
  }

  {
    auto p = Packet(2);
    EXPECT_EQ(2, p.get_sequence_id());
    EXPECT_EQ(0U, p.get_capabilities().bits());
    EXPECT_EQ(0U, p.get_payload_size());
  }

  {
    auto p = Packet(2, Capabilities::PROTOCOL_41);
    EXPECT_EQ(2, p.get_sequence_id());
    EXPECT_EQ(Capabilities::PROTOCOL_41, p.get_capabilities());
    EXPECT_EQ(0U, p.get_payload_size());
  }
}

TEST_F(MySQLProtocolPacketTest, CopyConstructor) {
  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32});
    Packet p_copy(p);
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(0U, p_copy.get_capabilities().bits());
  }

  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32}, Capabilities::PROTOCOL_41);
    Packet p_copy(p);
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(Capabilities::PROTOCOL_41, p_copy.get_capabilities());
  }
}

TEST_F(MySQLProtocolPacketTest, CopyAssignment) {
  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32});
    Packet p_copy{};
    p_copy = p;
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(p.get_payload_size(), p_copy.get_payload_size());
    ASSERT_EQ(0U, p_copy.get_capabilities().bits());
  }

  {
    Packet p({0x1, 0x0, 0x0, 0x9, 0x32}, Capabilities::PROTOCOL_41);
    Packet p_copy{};
    p_copy = p;
    ASSERT_EQ(p.size(), p_copy.size());
    ASSERT_EQ(p.get_sequence_id(), p_copy.get_sequence_id());
    ASSERT_EQ(p.get_payload_size(), p_copy.get_payload_size());
    ASSERT_EQ(p.get_capabilities(), p_copy.get_capabilities());
  }
}

TEST_F(MySQLProtocolPacketTest, MoveConstructor) {
  Packet::vector_t buffer = {0x1, 0x0, 0x0, 0x9, 0x32};
  {
    Packet p(buffer, Capabilities::PROTOCOL_41);
    Packet q(std::move(p));

    ASSERT_EQ(buffer.size(), q.size());
    ASSERT_EQ(Capabilities::PROTOCOL_41, q.get_capabilities());
    ASSERT_EQ(9U, q.get_sequence_id());
    ASSERT_EQ(1U, q.get_payload_size());

    // original should be empty and re-set
    ASSERT_EQ(0U, p.size());
    ASSERT_EQ(0U, p.get_capabilities().bits());
    ASSERT_EQ(0U, p.get_sequence_id());
    ASSERT_EQ(0U, p.get_payload_size());
  }
}

TEST_F(MySQLProtocolPacketTest, MoveAssigment) {
  Packet::vector_t buffer = {0x1, 0x0, 0x0, 0x9, 0x32};
  {
    Packet p(buffer, Capabilities::PROTOCOL_41);
    Packet q{};
    q = std::move(p);

    ASSERT_EQ(buffer.size(), q.size());
    ASSERT_EQ(Capabilities::PROTOCOL_41, q.get_capabilities());
    ASSERT_EQ(9U, q.get_sequence_id());
    ASSERT_EQ(1U, q.get_payload_size());

    // original should be empty and re-set
    ASSERT_EQ(0U, p.size());
    ASSERT_EQ(0U, p.get_capabilities().bits());
    ASSERT_EQ(0U, p.get_sequence_id());
    ASSERT_EQ(0U, p.get_payload_size());
  }
}

TEST_F(MySQLProtocolPacketTest, ConstructWithBuffer) {
  {
    auto p = Packet(case1);
    ASSERT_THAT(p, ContainerEq(case1));
    ASSERT_EQ(4UL, p.get_payload_size());
    ASSERT_EQ(1UL, p.get_sequence_id());
  }

  {
    Packet::vector_t incomplete = {0x04, 0x0, 0x0};
    auto p = Packet(incomplete);
    ASSERT_THAT(p, ContainerEq(incomplete));
    ASSERT_EQ(0UL, p.get_payload_size());
    ASSERT_EQ(0UL, p.get_sequence_id());
  }
}

TEST_F(MySQLProtocolPacketTest, seek_and_tell) {
  Packet p;

  // test seek/tell at beginning + add payload
  p.seek(0);
  EXPECT_EQ(0u, p.tell());
  p.write_int<uint8_t>(11);
  p.write_int<uint8_t>(12);
  p.write_int<uint8_t>(13);
  p.write_int<uint8_t>(14);

  // test seek/tell in the middle
  p.seek(2);
  EXPECT_EQ(2u, p.tell());
  EXPECT_EQ(13u, p.read_int<uint8_t>());
  EXPECT_EQ(3u, p.tell());

  // seek to EOF
  EXPECT_NO_THROW(p.seek(p.size()));

  // seek past EOF
  EXPECT_THROW_LIKE(p.seek(p.size() + 1), std::range_error, "seek past EOF");
}

TEST_F(MySQLProtocolPacketTest, PackInt1Bytes) {
  {
    Packet p{};
    p.seek(0);
    p.write_int<uint8_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0}));

    p.write_int<uint8_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x86}));

    p.write_int<uint8_t>(255);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x86, 0xff}));
  }

  {
    // signed
    Packet p{};
    p.seek(0);
    p.write_int<int8_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0}));

    p.write_int<int8_t>(static_cast<int8_t>(-134));
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x7a}));

    p.write_int<int8_t>(static_cast<int8_t>(-254));
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x7a, 0x02}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt2Bytes) {
  {
    Packet p{};
    p.seek(0);
    p.write_int<uint16_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00}));

    // Do not change the 0x0086 constant. Accidentally, it tests for
    // optimization-related bugs in some versions of GCC.
    p.write_int<uint16_t>(0x0086);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x86, 0x00}));

    p.write_int<uint16_t>(300);
    ASSERT_THAT(
        p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x86, 0x00, 0x2c, 0x1}));

    p.write_int<uint16_t>(UINT16_MAX);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x86, 0x00, 0x2c,
                                                    0x1, 0xff, 0xff}));
  }

  {
    // signed
    Packet p{};
    p.seek(0);
    p.write_int<int16_t>(INT16_MIN);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x80}));

    p = {};
    p.seek(0);
    p.write_int<int16_t>(INT16_MAX);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0x7f}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt3BytesUnsigned) {
  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>(0, 3);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x00}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>(134, 3);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x86, 0x00, 0x00}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>(500, 3);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xf4, 0x1, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>(53123, 3);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x83, 0xcf, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>((1ULL << (CHAR_BIT * 3)) - 1, 3);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0xff}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt3BytesSigned) {
  Packet p;
  p.seek(0);
  p.write_int<int32_t>(-8388608, 3);
  ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x00, 0x00, 0x80}));

  p = {};
  p.seek(0);
  p.write_int<int32_t>(-1234567, 3);
  ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x79, 0x29, 0xed}));

  p = {};
  p.seek(0);
  p.write_int<int32_t>(8388607, 3);
  ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0x7f}));
}

TEST_F(MySQLProtocolPacketTest, PackInt4ByteUnsigned) {
  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x00, 0x00}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x86, 0x00, 0x00, 0x00}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>(500);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xf4, 0x1, 0x00, 0x00}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>(53123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x83, 0xcf, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>(2253123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x43, 0x61, 0x22, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint32_t>(UINT32_MAX);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0xff, 0xff}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt4ByteSigned) {
  {
    Packet p;
    p.seek(0);
    p.write_int<int32_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x00, 0x00, 0x00}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int32_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x86, 0x00, 0x00, 0x00}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int32_t>(-500);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0c, 0xfe, 0xff, 0xff}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int32_t>(53123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x83, 0xcf, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int32_t>(-2253123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xbd, 0x9e, 0xdd, 0xff}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int32_t>(INT32_MIN);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x00, 0x00, 0x00, 0x80}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int32_t>(INT32_MAX);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0xff, 0x7f}));
  }
}

TEST_F(MySQLProtocolPacketTest, write_int_range_test) {
  using V8 = std::vector<uint8_t>;

  // construct template packet
  Packet p_template;
  p_template.seek(0);
  p_template.write_bytes(V8{101, 102, 103, 104});
  ASSERT_EQ(V8({101, 102, 103, 104}), p_template);

  {
    Packet p = p_template;
    p.seek(0);
    p.write_int<int16_t>(0x0201);
    V8 expected = {1, 2, 103, 104};
    EXPECT_EQ(expected, p);
  }
  {
    Packet p = p_template;
    p.seek(1);
    p.write_int<int16_t>(0x0201);
    V8 expected = {101, 1, 2, 104};
    EXPECT_EQ(expected, p);
  }
  {
    Packet p = p_template;
    p.seek(2);
    p.write_int<int16_t>(0x0201);
    V8 expected = {101, 102, 1, 2};
    EXPECT_EQ(expected, p);
  }
  {
    Packet p = p_template;
    p.seek(3);
    p.write_int<int16_t>(0x0201);
    V8 expected = {101, 102, 103, 1, 2};
    EXPECT_EQ(expected, p);
  }
  {
    Packet p = p_template;
    p.seek(4);
    p.write_int<int16_t>(0x0201);
    V8 expected = {101, 102, 103, 104, 1, 2};
    EXPECT_EQ(expected, p);
  }

  // no test past EOF is necessary, because it's not possible to seek that far
}

TEST_F(MySQLProtocolPacketTest, PackLenEncodedInt) {
  using V8 = std::vector<uint8_t>;

  // 1-byte
  {
    Packet buf;
    buf.seek(0);
    EXPECT_EQ(1u, buf.write_lenenc_uint(0u));
    EXPECT_EQ(V8({0u}), buf);
  }
  {
    Packet buf;
    buf.seek(0);
    EXPECT_EQ(1u, buf.write_lenenc_uint(250u));
    EXPECT_EQ(V8({250u}), buf);
  }

  // 3-byte
  {
    Packet buf;
    buf.seek(0);
    EXPECT_EQ(3u, buf.write_lenenc_uint(251u));
    EXPECT_EQ(V8({0xfc, 251u, 0u}), buf);
  }
  {
    Packet buf;
    buf.seek(0);
    EXPECT_EQ(3u, buf.write_lenenc_uint(0x1234));
    EXPECT_EQ(V8({0xfc, 0x34, 0x12}), buf);
  }
  {
    Packet buf;
    buf.seek(0);
    EXPECT_EQ(3u, buf.write_lenenc_uint(0xffff));
    EXPECT_EQ(V8({0xfc, 0xff, 0xff}), buf);
  }

  // 4-byte
  {
    Packet buf;
    buf.seek(0);
    EXPECT_EQ(4u, buf.write_lenenc_uint(0x010000));
    EXPECT_EQ(V8({0xfd, 0u, 0u, 1u}), buf);
  }
  {
    Packet buf;
    buf.seek(0);
    EXPECT_EQ(4u, buf.write_lenenc_uint(0x123456));
    EXPECT_EQ(V8({0xfd, 0x56, 0x34, 0x12}), buf);
  }
  {
    Packet buf;
    buf.seek(0);
    EXPECT_EQ(4u, buf.write_lenenc_uint(0xffffff));
    EXPECT_EQ(V8({0xfd, 0xff, 0xff, 0xff}), buf);
  }

  // 9-byte
  {
    Packet buf;
    buf.seek(0);
    EXPECT_EQ(9u, buf.write_lenenc_uint(0x01000000));
    EXPECT_EQ(V8({0xfe, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u}), buf);
  }
  {
    Packet buf;
    buf.seek(0);
    EXPECT_EQ(9u, buf.write_lenenc_uint(0x1234567890abcdef));
    EXPECT_EQ(V8({0xfe, 0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12}), buf);
  }

  // range tests
  {
    // construct template packet
    Packet p_template;
    p_template.seek(0);
    p_template.write_bytes(V8{101, 102, 103, 104, 105, 106});
    ASSERT_EQ(V8({101, 102, 103, 104, 105, 106}), p_template);

    {
      Packet p = p_template;
      p.seek(0);
      p.write_lenenc_uint(0x030201);
      V8 expected = {0xfd, 1, 2, 3, 105, 106};
      EXPECT_EQ(expected, p);
    }
    {
      Packet p = p_template;
      p.seek(1);
      p.write_lenenc_uint(0x030201);
      V8 expected = {101, 0xfd, 1, 2, 3, 106};
      EXPECT_EQ(expected, p);
    }
    {
      Packet p = p_template;
      p.seek(2);
      p.write_lenenc_uint(0x030201);
      V8 expected = {101, 102, 0xfd, 1, 2, 3};
      EXPECT_EQ(expected, p);
    }
    {
      Packet p = p_template;
      p.seek(3);
      p.write_lenenc_uint(0x030201);
      V8 expected = {101, 102, 103, 0xfd, 1, 2, 3};
      EXPECT_EQ(expected, p);
    }
    {
      Packet p = p_template;
      p.seek(4);
      p.write_lenenc_uint(0x030201);
      V8 expected = {101, 102, 103, 104, 0xfd, 1, 2, 3};
      EXPECT_EQ(expected, p);
    }
    {
      Packet p = p_template;
      p.seek(5);
      p.write_lenenc_uint(0x030201);
      V8 expected = {101, 102, 103, 104, 105, 0xfd, 1, 2, 3};
      EXPECT_EQ(expected, p);
    }
    {
      Packet p = p_template;
      p.seek(6);
      p.write_lenenc_uint(0x030201);
      V8 expected = {101, 102, 103, 104, 105, 106, 0xfd, 1, 2, 3};
      EXPECT_EQ(expected, p);
    }

    // no test past EOF is necessary, because it's not possible to seek that far
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt8BytesUnsigned) {
  {
    Packet p;
    p.seek(0);
    p.write_int<uint64_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x0, 0x0, 0x0, 0x0,
                                                    0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint64_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x86, 0x0, 0x0, 0x0, 0x0,
                                                    0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint64_t>(500);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xf4, 0x1, 0x0, 0x0, 0x0,
                                                    0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint64_t>(53123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x83, 0xcf, 0x0, 0x0, 0x0,
                                                    0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint64_t>(2253123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x43, 0x61, 0x22, 0x0, 0x0,
                                                    0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint64_t>(361417177240330563UL);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x43, 0x61, 0x22, 0x1, 0x2,
                                                    0x3, 0x4, 0x5}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<uint64_t>(4294967295);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0xff, 0xff, 0x0,
                                                    0x0, 0x0, 0x0}));
  }
}

TEST_F(MySQLProtocolPacketTest, PackInt8BytesSigned) {
  {
    Packet p;
    p.seek(0);
    p.write_int<uint64_t>(0);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0, 0x0, 0x0, 0x0, 0x0,
                                                    0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int64_t>(134);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x86, 0x0, 0x0, 0x0, 0x0,
                                                    0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int64_t>(-500);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x0c, 0xfe, 0xff, 0xff,
                                                    0xff, 0xff, 0xff, 0xff}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int64_t>(53123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x83, 0xcf, 0x0, 0x0, 0x0,
                                                    0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int64_t>(-2253123);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xbd, 0x9e, 0xdd, 0xff,
                                                    0xff, 0xff, 0xff, 0xff}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int64_t>(361417177240330563L);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x43, 0x61, 0x22, 0x1, 0x2,
                                                    0x3, 0x4, 0x5}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int64_t>(-361417177240330563L);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xbd, 0x9e, 0xdd, 0xfe,
                                                    0xfd, 0xfc, 0xfb, 0xfa}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int64_t>(4294967295);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0xff, 0xff, 0xff, 0xff, 0x0,
                                                    0x0, 0x0, 0x0}));
  }

  {
    Packet p;
    p.seek(0);
    p.write_int<int64_t>(-4294967295LL);
    ASSERT_THAT(p, ContainerEq(std::vector<uint8_t>{0x01, 0x0, 0x0, 0x0, 0xff,
                                                    0xff, 0xff, 0xff}));
  }
}

TEST_F(MySQLProtocolPacketTest, write_bytes) {
  using V8 = std::vector<uint8_t>;
  V8 bytes = {1, 2, 3};

  // construct template packet
  Packet p_template;
  p_template.seek(0);
  p_template.write_bytes(V8{101, 102, 103, 104, 105});
  ASSERT_EQ(V8({101, 102, 103, 104, 105}), p_template);
  EXPECT_EQ(5u, p_template.tell());

  // write at 0
  {
    Packet p = p_template;
    p.seek(0);
    p.write_bytes(bytes);

    V8 expected = {1, 2, 3, 104, 105};
    EXPECT_EQ(expected, p);
    EXPECT_EQ(3u, p.tell());
  }

  // write at 1
  {
    Packet p = p_template;
    p.seek(1);
    p.write_bytes(bytes);

    V8 expected = {101, 1, 2, 3, 105};
    EXPECT_EQ(expected, p);
    EXPECT_EQ(4u, p.tell());
  }

  // write at 2
  {
    Packet p = p_template;
    p.seek(2);
    p.write_bytes(bytes);

    V8 expected = {101, 102, 1, 2, 3};
    EXPECT_EQ(expected, p);
    EXPECT_EQ(5u, p.tell());
  }

  // write at 3
  {
    Packet p = p_template;
    p.seek(3);
    p.write_bytes(bytes);

    V8 expected = {101, 102, 103, 1, 2, 3};
    EXPECT_EQ(expected, p);
    EXPECT_EQ(6u, p.tell());
  }

  // write at 4
  {
    Packet p = p_template;
    p.seek(4);
    p.write_bytes(bytes);

    V8 expected = {101, 102, 103, 104, 1, 2, 3};
    EXPECT_EQ(expected, p);
    EXPECT_EQ(7u, p.tell());
  }

  // write at 5 (EOF)
  {
    Packet p = p_template;
    p.seek(5);
    p.write_bytes(bytes);

    V8 expected = {101, 102, 103, 104, 105, 1, 2, 3};
    EXPECT_EQ(expected, p);
    EXPECT_EQ(8u, p.tell());
  }

  // write empty at 0
  {
    Packet p = p_template;
    p.seek(0);
    p.write_bytes({});

    V8 expected = {101, 102, 103, 104, 105};
    EXPECT_EQ(expected, p);
    EXPECT_EQ(0u, p.tell());
  }

  // write empty at 3
  {
    Packet p = p_template;
    p.seek(3);
    p.write_bytes({});

    V8 expected = {101, 102, 103, 104, 105};
    EXPECT_EQ(expected, p);
    EXPECT_EQ(3u, p.tell());
  }

  // write empty at 5 (EOF)
  {
    Packet p = p_template;
    p.seek(5);
    p.write_bytes({});

    V8 expected = {101, 102, 103, 104, 105};
    EXPECT_EQ(expected, p);
    EXPECT_EQ(5u, p.tell());
  }

  // no test past EOF is necessary, because it's not possible to seek that far
}

TEST_F(MySQLProtocolPacketTest, write_string) {
  using V8 = std::vector<uint8_t>;
  std::string str = "abc";

  auto as_string = [](Packet packet) {
    packet.seek(0);
    V8 v = packet.read_bytes_eof();
    return std::string(v.begin(), v.end());
  };

  // construct template packet
  Packet p_template;
  p_template.seek(0);
  p_template.write_string("12345");
  ASSERT_EQ("12345", as_string(p_template));
  EXPECT_EQ(5u, p_template.tell());

  // write at 0
  {
    Packet p = p_template;
    p.seek(0);
    p.write_string(str);
    EXPECT_EQ("abc45", as_string(p));
    EXPECT_EQ(3u, p.tell());
  }

  // write at 1
  {
    Packet p = p_template;
    p.seek(1);
    p.write_string(str);
    EXPECT_EQ("1abc5", as_string(p));
    EXPECT_EQ(4u, p.tell());
  }

  // write at 2
  {
    Packet p = p_template;
    p.seek(2);
    p.write_string(str);
    EXPECT_EQ("12abc", as_string(p));
    EXPECT_EQ(5u, p.tell());
  }

  // write at 3
  {
    Packet p = p_template;
    p.seek(3);
    p.write_string(str);
    EXPECT_EQ("123abc", as_string(p));
    EXPECT_EQ(6u, p.tell());
  }

  // write at 4
  {
    Packet p = p_template;
    p.seek(4);
    p.write_string(str);
    EXPECT_EQ("1234abc", as_string(p));
    EXPECT_EQ(7u, p.tell());
  }

  // write at 5 (EOF)
  {
    Packet p = p_template;
    p.seek(5);
    p.write_string(str);
    EXPECT_EQ("12345abc", as_string(p));
    EXPECT_EQ(8u, p.tell());
  }

  // write empty at 0
  {
    Packet p = p_template;
    p.seek(0);
    p.write_string("");

    EXPECT_EQ("12345", as_string(p));
    EXPECT_EQ(0u, p.tell());
  }

  // write empty at 3
  {
    Packet p = p_template;
    p.seek(3);
    p.write_string("");

    EXPECT_EQ("12345", as_string(p));
    EXPECT_EQ(3u, p.tell());
  }

  // write empty at 5 (EOF)
  {
    Packet p = p_template;
    p.seek(5);
    p.write_string("");

    EXPECT_EQ("12345", as_string(p));
    EXPECT_EQ(5u, p.tell());
  }

  // no test past EOF is necessary, because it's not possible to seek that far
}

TEST_F(MySQLProtocolPacketTest, UnpackUInt8) {
  {
    Packet buf{0x10};
    EXPECT_EQ(16u, buf.read_int_from<uint8_t>(0));
  }
  {
    Packet buf{0x10, 0x20};
    EXPECT_EQ(32u, buf.read_int_from<uint8_t>(1));
  }

  {
    Packet buf{0x10};
    EXPECT_EQ(16u, buf.read_int_from<uint8_t>(0, 1));
  }
  {
    Packet buf{0x10, 0x20};
    EXPECT_EQ(16u, buf.read_int_from<uint8_t>(0, 2));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackUInt16) {
  {
    Packet buf{0x10, 0x00};
    EXPECT_EQ(16u, buf.read_int_from<uint16_t>(0, 2));
  }

  {
    Packet buf{0x10, 0x20};
    EXPECT_EQ(8208u, buf.read_int_from<uint16_t>(0));
  }

  {
    Packet buf{0x10, 0x20, 0x30};
    EXPECT_EQ(8208u, buf.read_int_from<uint16_t>(0, 2));
  }

  {
    Packet buf{0xab, 0xba};
    EXPECT_EQ(47787u, buf.read_int_from<uint16_t>(0));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackInt3Bytes) {
  // unsigned
  {
    Packet buf{0x10, 0x00, 0x00};
    EXPECT_EQ(16U, buf.read_int_from<uint32_t>(0, 3));
  }

  {
    Packet buf{0x10, 0x20, 0x00};
    EXPECT_EQ(8208U, buf.read_int_from<uint32_t>(0, 3));
  }

  {
    Packet buf{0x10, 0x20, 0x30};
    EXPECT_EQ(3153936U, buf.read_int_from<uint32_t>(0, 3));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackInt4bytes) {
  // unsigned
  {
    Packet buf({0x10, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(16U, buf.read_int_from<uint32_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x00, 0x00}, true);
    EXPECT_EQ(8208U, buf.read_int_from<uint32_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40}, true);
    EXPECT_EQ(1076895760U, buf.read_int_from<uint32_t>(0, 4));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40, 0x50}, true);
    EXPECT_EQ(1076895760U, buf.read_int_from<uint32_t>(0, 4));
  }

  // signed
  {
    Packet buf({0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(-1, buf.read_int_from<int>(0));
  }

  {
    Packet buf({0xf2, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(-14, buf.read_int_from<int>(0));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0xfe}, true);
    EXPECT_EQ(-16777217, buf.read_int_from<int>(0));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0x7f}, true);
    EXPECT_EQ(2147483647, buf.read_int_from<int>(0, 4));
  }

  {
    Packet buf({0x02, 0x00, 0x00, 0x80}, true);
    EXPECT_EQ(-2147483646, buf.read_int_from<int>(0, 4));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackUInt64) {
  {
    Packet buf({0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(16UL, buf.read_int_from<uint64_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(8208UL, buf.read_int_from<uint64_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(1076895760UL, buf.read_int_from<uint64_t>(0, 8));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(4294967295UL, buf.read_int_from<uint64_t>(0));
  }

  {
    Packet buf({0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80, 0x90}, true);
    EXPECT_EQ(9223372381529055248UL, buf.read_int_from<uint64_t>(0));
  }

  {
    Packet buf({0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(18446744073709551615UL, buf.read_int_from<uint64_t>(0));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackInt_invalid_input) {
  Packet buf10({0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0},
               true);

  // supported sizes
  {
    for (size_t i : {1, 2, 3, 4, 8})
      buf10.read_int_from<uint64_t>(0, i);  // shouldn't die
  }

  // unsuppposed sizes
  {
    for (size_t i : {0, 5, 6, 7, 9}) {  // doesn't compile without {} on VS2015
      EXPECT_DEATH_IF_SUPPORTED(buf10.read_int_from<uint64_t>(0, i), "");
    }
  }

  // start beyond EOF
  {
    Packet buf{};
    EXPECT_THROW_LIKE(buf.read_int_from<uint64_t>(0, 1), std::range_error,
                      "start or end beyond EOF");
  }
  { EXPECT_NO_THROW(buf10.read_int_from<uint64_t>(9, 1)); }
  {
    EXPECT_THROW_LIKE(buf10.read_int_from<uint64_t>(10, 1), std::range_error,
                      "start or end beyond EOF");
  }

  // end beyond EOF
  { EXPECT_NO_THROW(buf10.read_int_from<uint64_t>(6, 4)); }
  {
    EXPECT_THROW_LIKE(buf10.read_int_from<uint64_t>(7, 4), std::range_error,
                      "start or end beyond EOF");
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackLenEncodedInt) {
  {
    Packet buf(Packet::vector_t{0xfa}, true);
    EXPECT_EQ(250U, buf.read_lenenc_uint_from(0).first);
    EXPECT_EQ(1U, buf.read_lenenc_uint_from(0).second);
  }

  {
    Packet buf({0xfc, 0xfb, 0x00}, true);
    EXPECT_EQ(251U, buf.read_lenenc_uint_from(0).first);
    EXPECT_EQ(3U, buf.read_lenenc_uint_from(0).second);
  }

  {
    Packet buf({0xfc, 0xff, 0xff}, true);
    EXPECT_EQ(65535U, buf.read_lenenc_uint_from(0).first);
    EXPECT_EQ(3U, buf.read_lenenc_uint_from(0).second);
  }

  {
    Packet buf({0xfd, 0x00, 0x00, 0x01}, true);
    EXPECT_EQ(65536U, buf.read_lenenc_uint_from(0).first);
    EXPECT_EQ(4U, buf.read_lenenc_uint_from(0).second);
  }

  {
    Packet buf({0xfd, 0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(16777215U, buf.read_lenenc_uint_from(0).first);
    EXPECT_EQ(4U, buf.read_lenenc_uint_from(0).second);
  }

  // this test has special significance: if we parsed according to protocol
  // v3.20 (which we don't implement atm), this would have to return 5U instead
  // of 9U
  {
    Packet buf({0xfe, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}, true);
    EXPECT_EQ(16777216U, buf.read_lenenc_uint_from(0).first);
    EXPECT_EQ(9U, buf.read_lenenc_uint_from(0).second);
  }

  {
    Packet buf({0xfe, 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80, 0x90},
               true);
    EXPECT_EQ(9223372381529055248UL, buf.read_lenenc_uint_from(0).first);
    EXPECT_EQ(9U, buf.read_lenenc_uint_from(0).second);
  }

  {
    Packet buf({0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, true);
    EXPECT_EQ(ULLONG_MAX, buf.read_lenenc_uint_from(0).first);
    EXPECT_EQ(9U, buf.read_lenenc_uint_from(0).second);
  }
}

TEST_F(MySQLProtocolPacketTest, read_lenenc_uint_from) {
  // ok
  {
    Packet buf({0xfe, 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80}, true);
    EXPECT_NO_THROW(buf.read_lenenc_uint_from(0));
  }

  // start beyond EOF
  {
    Packet buf({0xfe, 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80}, true);
    EXPECT_THROW_LIKE(buf.read_lenenc_uint_from(10), std::range_error,
                      "start beyond EOF");
  }

  // end beyond EOF
  {
    Packet buf({0xfe, 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00}, true);
    EXPECT_THROW_LIKE(buf.read_lenenc_uint_from(0), std::range_error,
                      "end beyond EOF");
  }

  // illegal first byte
  {
    Packet buf({0xfb}, true);
    EXPECT_THROW_LIKE(buf.read_lenenc_uint_from(0), std::runtime_error,
                      "illegal value at first byte");
  }
  {
    Packet buf({0xff}, true);
    EXPECT_THROW_LIKE(buf.read_lenenc_uint_from(0), std::runtime_error,
                      "illegal value at first byte");
  }
}

TEST_F(MySQLProtocolPacketTest, read_lenenc_uint) {
  Packet buf({0xfe, 0x10, 0x20, 0x30, 0x40, 0x50, 0x00, 0x00, 0x80, 0xfe},
             true);
  buf.seek(0);
  EXPECT_NO_THROW(buf.read_lenenc_uint());
  EXPECT_EQ(9u, buf.tell());

  EXPECT_THROW_LIKE(buf.read_lenenc_uint(), std::range_error, "end beyond EOF");
  EXPECT_EQ(9u, buf.tell());
}

TEST_F(MySQLProtocolPacketTest, UnpackString) {
  {
    Packet p({'h', 'a', 'm', 0x0, 's', 'p', 'a', 'm'}, true);
    auto res = p.read_string_from(0);
    EXPECT_EQ(string("ham"), res);
    res = p.read_string_from(res.size() + 1UL);
    EXPECT_EQ(string("spam"), res);
    res = p.read_string_from(0, p.size());
    EXPECT_EQ(string("ham"), res);
  }

  {
    Packet p{};
    EXPECT_EQ(string{}, p.read_string_from(0));
  }

  {
    Packet p({'h', 'a', 'm', 's', 'p', 'a', 'm'}, true);
    EXPECT_EQ(string("hamspam"), p.read_string_from(0));
  }

  {
    Packet p({'h', 'a', 'm'}, true);
    EXPECT_EQ(string(""), p.read_string_from(30));
  }
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthFixed) {
  Packet p({'h', 'a', 'm', 's', 'p', 'a', 'm'}, true);

  {
    auto res = p.read_string_from(0, 3);
    EXPECT_EQ(res, string("ham"));
  }

  {
    auto res = p.read_string_from(0, 2);
    EXPECT_EQ(res, string("ha"));
  }

  {
    auto res = p.read_string_from(3, 4);
    EXPECT_EQ(res, string("spam"));
  }
}

TEST_F(MySQLProtocolPacketTest, read_string_nul_from) {
  Packet p({'s', 'o', 'm', 'e', 0x0, 'n', 'o', 'z', 'e', 'r', 'o'}, true);

  EXPECT_EQ("some", p.read_string_nul_from(0));
  EXPECT_EQ("ome", p.read_string_nul_from(1));
  EXPECT_EQ("", p.read_string_nul_from(4));
  EXPECT_THROW_LIKE(p.read_string_nul_from(5), std::runtime_error,
                    "zero-terminator not found");
  EXPECT_THROW_LIKE(p.read_string_nul_from(10), std::runtime_error,
                    "zero-terminator not found");
  EXPECT_THROW_LIKE(p.read_string_nul_from(11), std::range_error,
                    "start beyond EOF");
}

TEST_F(MySQLProtocolPacketTest, read_string_nul) {
  Packet p({'s', 'o', 'm', 'e', 0x0, 's', 't', 'r', 'i', 'n', 'g', 0x0, 'n',
            'o', 'z', 'e', 'r', 'o'},
           true);
  p.seek(0);

  EXPECT_EQ("some", p.read_string_nul());
  EXPECT_EQ(5u, p.tell());

  EXPECT_EQ("string", p.read_string_nul());
  EXPECT_EQ(12u, p.tell());

  EXPECT_THROW_LIKE(p.read_string_nul(), std::runtime_error,
                    "zero-terminator not found");
  EXPECT_EQ(12u, p.tell());
}

TEST_F(MySQLProtocolPacketTest, read_bytes_from) {
  Packet p({0x1, 0x0, 0x0, 0x9, 0x32});
  using V = std::vector<uint8_t>;

  EXPECT_EQ(V{}, p.read_bytes_from(0, 0));
  EXPECT_EQ(V{0x1}, p.read_bytes_from(0, 1));

  {
    V exp = {0x1, 0x0, 0x0, 0x9};  // doesn't build inline
    EXPECT_EQ(exp, p.read_bytes_from(0, 4));
  }
  {
    V exp = {0x0, 0x0, 0x9, 0x32};
    EXPECT_EQ(exp, p.read_bytes_from(1, 4));
  }

  EXPECT_THROW_LIKE(p.read_bytes_from(2, 4), std::range_error,
                    "start or end beyond EOF");

  EXPECT_EQ(V{}, p.read_bytes_from(5, 0));
}

TEST_F(MySQLProtocolPacketTest, read_bytes) {
  Packet p({1, 0, 0, 9, 32});
  using V = std::vector<uint8_t>;

  p.seek(0);

  V exp = {1, 0, 0};  // doesn't build inline
  EXPECT_EQ(exp, p.read_bytes(3));
  EXPECT_EQ(3u, p.tell());

  EXPECT_THROW_LIKE(p.read_bytes(3), std::runtime_error,
                    "start or end beyond EOF");
  EXPECT_EQ(3u, p.tell());
}

TEST_F(MySQLProtocolPacketTest, read_bytes_eof_from) {
  Packet p({0x0, 0x9, 0x32, 0x0}, true);
  using V = std::vector<uint8_t>;

  {
    V exp = {0x0, 0x9, 0x32, 0x0};  // doesn't build inline
    EXPECT_EQ(exp, p.read_bytes_eof_from(0));
  }
  {
    V exp = {0x0};
    EXPECT_EQ(exp, p.read_bytes_eof_from(3));
  }

  EXPECT_THROW_LIKE(p.read_bytes_eof_from(4), std::range_error,
                    "start beyond EOF");
}

TEST_F(MySQLProtocolPacketTest, read_bytes_eof) {
  Packet p({0x0, 0x9, 0x32, 0x0}, true);
  using V = std::vector<uint8_t>;

  p.seek(0);
  V exp = {0x0, 0x9, 0x32, 0x0};  // doesn't build inline

  EXPECT_EQ(exp, p.read_bytes_eof());
  EXPECT_EQ(4u, p.tell());

  EXPECT_THROW_LIKE(p.read_bytes_eof(), std::range_error, "start beyond EOF");
}

TEST_F(MySQLProtocolPacketTest, UnpackBytesLengthEncoded1Byte) {
  Packet p({0x07, 'h', 'a', 'm', 's', 'p', 'a', 'm', 'f', 'o', 'o'}, true);
  auto pr = p.read_lenenc_bytes_from(0);
  EXPECT_THAT(pr.first,
              ContainerEq(Packet::vector_t{'h', 'a', 'm', 's', 'p', 'a', 'm'}));
  EXPECT_EQ(8u, pr.second);
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthEncoded3Bytes) {
  size_t length = 316;
  unsigned char filler = 0x4d;

  std::vector<uint8_t> data(length + 3, filler);
  data[0] = 0xfc;
  data[1] = 0x3c;
  data[2] = 0x01;
  Packet p(data, true);

  auto pr = p.read_lenenc_bytes_from(0);
  EXPECT_EQ(pr.first.size(), length);
  EXPECT_EQ(pr.first[0], filler);
  EXPECT_EQ(pr.first[length - 1], filler);
  EXPECT_EQ(length + 3, pr.second);
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthEncoded8Bytes) {
  size_t length = 16777216;
  unsigned char filler = 0x4d;

  std::vector<uint8_t> data(length + 9, filler);
  std::vector<uint8_t> enc_length{0xfe, 0x0, 0x0, 0x0, 0x01,
                                  0x0,  0x0, 0x0, 0x0};
  std::copy(enc_length.begin(), enc_length.end(), data.begin());
  Packet p(data, true);

  auto pr = p.read_lenenc_bytes_from(0);
  EXPECT_EQ(pr.first.size(), length);
  EXPECT_EQ(pr.first[length - 1], filler);
  EXPECT_EQ(length + 9, pr.second);
}

TEST_F(MySQLProtocolPacketTest, UnpackStringLengthEncoded8BytesWithNulByte) {
  size_t length = 16777216;
  unsigned char filler = 0x4d;

  std::vector<uint8_t> data(length + 9, filler);
  std::vector<uint8_t> enc_length{0xfe, 0x0, 0x0, 0x0, 0x01,
                                  0x0,  0x0, 0x0, 0x0};
  std::copy(enc_length.begin(), enc_length.end(), data.begin());
  data[length / 2] = 0x0;
  Packet p(data, true);

  auto pr = p.read_lenenc_bytes_from(0);
  EXPECT_EQ(pr.first.size(), length);
  EXPECT_EQ(pr.first[length - 1], filler);
  EXPECT_EQ(length + 9, pr.second);
}

TEST_F(MySQLProtocolPacketTest, read_lenenc_bytes_from) {
  // throw scenarios for length-encoded uint are tested by
  // read_lenenc_uint_from() test, so here we only need to test for the cases of
  // payload being beyond EOF

  Packet buf({4, 0x10, 0x20, 0x30, 0x40}, true);

  EXPECT_NO_THROW(buf.read_lenenc_bytes_from(0));

  buf.pop_back();
  EXPECT_THROW_LIKE(buf.read_lenenc_bytes_from(0), std::range_error,
                    "start or end beyond EOF");
}

TEST_F(MySQLProtocolPacketTest, read_lenenc_bytes) {
  Packet buf({4, 0x10, 0x20, 0x30, 0x40, 2, 0x11, 0x22, 0x99}, true);
  buf.seek(0);
  EXPECT_NO_THROW(buf.read_lenenc_bytes());
  EXPECT_EQ(5u, buf.tell());
  EXPECT_NO_THROW(buf.read_lenenc_bytes());
  EXPECT_EQ(8u, buf.tell());

  EXPECT_THROW_LIKE(buf.read_lenenc_bytes(), std::range_error,
                    "end beyond EOF");
  EXPECT_EQ(8u, buf.tell());
}

TEST_F(MySQLProtocolPacketTest, append_bytes) {
  Packet buf({0x10, 0x20, 0x30, 0x40}, true);

  // add 0 bytes
  buf.seek(buf.size());
  buf.append_bytes(0, 0x99);
  EXPECT_EQ(4u, buf.tell());

  // add 3 bytes
  buf.append_bytes(3, 0x99);
  Packet exp({0x10, 0x20, 0x30, 0x40, 0x99, 0x99, 0x99}, true);
  EXPECT_EQ(7u, buf.tell());
  EXPECT_EQ(exp, buf);

  // not at EOF
  buf.seek(6);
  EXPECT_THROW_LIKE(buf.append_bytes(3, 0x99), std::range_error, "not at EOF");
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
