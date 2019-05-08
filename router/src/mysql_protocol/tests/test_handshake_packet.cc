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
using namespace mysql_protocol;

/**
 * @file
 * @brief Unit tests to test Handshake Response Packet, and PROTOCOL41 parser
 */

class HandshakeResponsePacketTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}
};

TEST_F(HandshakeResponsePacketTest, DefaultConstructor) {
  mysql_protocol::HandshakeResponsePacket p{};

  std::vector<unsigned char> exp{
      0x4d, 0x00, 0x00, 0x00, 0x8d, 0xa2, 0x03, 0x00, 0x00, 0x00, 0x00, 0x40,
      0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x14, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71,
      0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x00, 0x6d,
      0x79, 0x73, 0x71, 0x6c, 0x5f, 0x6e, 0x61, 0x74, 0x69, 0x76, 0x65, 0x5f,
      0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x00,
  };

  ASSERT_THAT(p, ContainerEq(exp));
}

TEST_F(HandshakeResponsePacketTest, Constructor) {
  std::vector<unsigned char> auth_response = {0x50, 0x51, 0x50,
                                              0x51, 0x50, 0x51};
  {
    // Setting the username; empty password
    mysql_protocol::HandshakeResponsePacket p(1, auth_response, "ROUTERTEST",
                                              "");

    std::vector<unsigned char> exp{
        0x57, 0x00, 0x00, 0x01, 0x8d, 0xa2, 0x03, 0x00, 0x00, 0x00, 0x00, 0x40,
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x52, 0x4f, 0x55, 0x54, 0x45, 0x52, 0x54, 0x45, 0x53, 0x54, 0x00, 0x14,
        0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71,
        0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x00, 0x6d, 0x79, 0x73,
        0x71, 0x6c, 0x5f, 0x6e, 0x61, 0x74, 0x69, 0x76, 0x65, 0x5f, 0x70, 0x61,
        0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x00,
    };

    ASSERT_THAT(p, ContainerEq(exp));
  }

  {
    // Database set
    mysql_protocol::HandshakeResponsePacket p(1, auth_response, "ROUTERTEST",
                                              "", "router_db");

    std::vector<unsigned char> exp{
        0x60, 0x00, 0x00, 0x01, 0x8d, 0xa2, 0x03, 0x00, 0x00, 0x00, 0x00, 0x40,
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x52, 0x4f, 0x55, 0x54, 0x45, 0x52, 0x54, 0x45, 0x53, 0x54, 0x00, 0x14,
        0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71,
        0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x72, 0x6f, 0x75, 0x74,
        0x65, 0x72, 0x5f, 0x64, 0x62, 0x00, 0x6d, 0x79, 0x73, 0x71, 0x6c, 0x5f,
        0x6e, 0x61, 0x74, 0x69, 0x76, 0x65, 0x5f, 0x70, 0x61, 0x73, 0x73, 0x77,
        0x6f, 0x72, 0x64, 0x00};

    ASSERT_THAT(p, ContainerEq(exp));
  }

  {
    // Character set
    mysql_protocol::HandshakeResponsePacket p(1, auth_response, "ROUTERTEST",
                                              "", "router_db", 80);

    std::vector<unsigned char> exp{
        0x60, 0x00, 0x00, 0x01, 0x8d, 0xa2, 0x03, 0x00, 0x00, 0x00, 0x00, 0x40,
        0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x52, 0x4f, 0x55, 0x54, 0x45, 0x52, 0x54, 0x45, 0x53, 0x54, 0x00, 0x14,
        0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71,
        0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x72, 0x6f, 0x75, 0x74,
        0x65, 0x72, 0x5f, 0x64, 0x62, 0x00, 0x6d, 0x79, 0x73, 0x71, 0x6c, 0x5f,
        0x6e, 0x61, 0x74, 0x69, 0x76, 0x65, 0x5f, 0x70, 0x61, 0x73, 0x73, 0x77,
        0x6f, 0x72, 0x64, 0x00};

    ASSERT_THAT(p, ContainerEq(exp));
  }

  {
    // Character set
    mysql_protocol::HandshakeResponsePacket p(1, auth_response, "ROUTERTEST",
                                              "", "router_db", 8,
                                              "router_auth_plugin");

    std::vector<unsigned char> exp{
        0x5d, 0x00, 0x00, 0x01, 0x8d, 0xa2, 0x03, 0x00, 0x00, 0x00, 0x00,
        0x40, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x52, 0x4f, 0x55, 0x54, 0x45, 0x52, 0x54, 0x45,
        0x53, 0x54, 0x00, 0x14, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71,
        0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71,
        0x71, 0x71, 0x72, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x5f, 0x64, 0x62,
        0x00, 0x72, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x5f, 0x61, 0x75, 0x74,
        0x68, 0x5f, 0x70, 0x6c, 0x75, 0x67, 0x69, 0x6e, 0x00,
    };

    ASSERT_THAT(p, ContainerEq(exp));
  }
}

class HandshakeResponseParseTest : public ::testing::Test {
 public:
  void SetUp() override {}
};

/** @brief Converts string of hex values into bytes
 *
 * For example, supplied string like so "11223344" will yield an array
 * containing bytes {0x11, 0x22, 0x33, 0x44}. Spaces between bytes are ignored,
 * so "11 22     3344" is equivalent to the previous string. OTOH, spaces
 * between hexadecimals of the same byte are not allowed - "1 1223344" will
 * throw an assertion.
 *
 * @param hex_text Input string with bytes written as hex
 * @return std::vector<uint32_t>
 */
std::vector<uint8_t> str2bytes(const std::string &hex_text) {
  std::vector<uint8_t> result;
  bool have_high_hex = false;

  auto append = [&](uint8_t hex) {
    assert(hex <= 15);

    static uint8_t high_hex;

    if (have_high_hex)
      result.push_back(16 * high_hex + hex);
    else
      high_hex = hex;

    have_high_hex = !have_high_hex;
  };

  for (const char c : hex_text) {
    // allow spaces between bytes (sets of two hexadecimals)
    if (c == ' ')
      if (have_high_hex)
        assert(0);  // don't allow <space> between high and low hexadecimals of
                    // the same byte
      else
        continue;

    // append hexadecimal
    else if ('0' <= c && c <= '9')
      append(c - '0');
    else if ('a' <= c && c <= 'f')
      append(c - 'a' + 10);

    // unrecognised token
    else
      assert(0);
  }

  return result;
}

constexpr bool kAutoPayloadParse = true;
constexpr bool kNoPayloadParse = false;

/**
 * @test So far we require PROTOCOL_41 to be spoken by both client and server
 */
TEST_F(HandshakeResponseParseTest, server_does_not_support_PROTOCOL_41) {
  EXPECT_THROW(
      HandshakeResponsePacket({}, kAutoPayloadParse, Capabilities::ALL_ZEROS),
      std::runtime_error);
}

/**
 * @test Verify behavior on missing CLIENT_PROTOCOL_41 flag
 */
TEST_F(HandshakeResponseParseTest, no_PROTOCOL_41) {
  // EOF
  {
    // missing capability flags ------------------------vvvvvvvvv
    std::vector<uint8_t> bytes = str2bytes("0000 0001            ");

    EXPECT_THROW_LIKE(
        HandshakeResponsePacket pkt(bytes, kAutoPayloadParse,
                                    Capabilities::PROTOCOL_41),
        std::runtime_error,
        "HandshakeResponsePacket: tried reading capability flags past EOF");
  }

  // no PROTOCOL_41 capability flag
  {
    // Note that PROTOCOL_41 flag is stored in the first (low) 16 bits, thus
    // providing the other 16 bits of flags is not required. Below we only
    // provide those low 16 bits.

    // the missing flag is here flag (0x0200) -------------v
    std::vector<uint8_t> bytes = str2bytes("0200 0001   fffd");

    EXPECT_THROW_LIKE(HandshakeResponsePacket pkt(bytes, kAutoPayloadParse,
                                                  Capabilities::PROTOCOL_41),
                      std::runtime_error,
                      "Handshake response packet: Protocol is version 320, "
                      "which is not implemented atm");
  }
}

/**
 * @test Verify behavior on bad payload count in header
 */
TEST_F(HandshakeResponseParseTest, bad_payload_length) {
  // bad payload length (should be 08) ---vv          <-- payload ---------->
  std::vector<uint8_t> bytes = str2bytes("5500 0000   11 22 33 44   0000 0000");

  EXPECT_THROW_LIKE(HandshakeResponsePacket pkt(bytes, kAutoPayloadParse,
                                                Capabilities::PROTOCOL_41),
                    std::runtime_error,
                    "Incorrect payload size (was 12; should be at least 85)");
}

/**
 * @test Verify behavior on bad sequence number in the header
 */
TEST_F(HandshakeResponseParseTest, bad_seq_number) {
  // bad sequence nr (should be 01) -------------vv   <cap.flags>
  std::vector<uint8_t> bytes = str2bytes("0800 0099   11 22 33 44   0000 0000");

  EXPECT_THROW_LIKE(
      HandshakeResponsePacket pkt(bytes, kAutoPayloadParse,
                                  Capabilities::PROTOCOL_41),
      std::runtime_error,
      "Handshake response packet: sequence number different than 1");
}

/**
 * @test Verify parsing of max packet size
 */
TEST_F(HandshakeResponseParseTest, max_packet_size) {
  constexpr size_t kOffset = 8;
  constexpr size_t kLength = 4;

  // EOF
  {
    // missing max packet size -------------------------------------vvvvvvvvv
    std::vector<uint8_t> bytes = str2bytes("0000 0000   0002 0000            ");

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    EXPECT_THROW_LIKE(pkt.position_ = kOffset; prs.part1_max_packet_size(),
                                               std::runtime_error,
                                               "start or end beyond EOF");
  }

  // ok
  {
    //                                          max packet size --vvvvvvvvv
    std::vector<uint8_t> bytes = str2bytes("0800 0000   0002 0000 0000 0040");

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    pkt.position_ = kOffset;
    prs.part1_max_packet_size();
    EXPECT_EQ(kOffset + kLength, pkt.position_);
    EXPECT_EQ(0x40000000u, pkt.max_packet_size_);
  }
}

/**
 * @test Verify parsing of character set
 */
TEST_F(HandshakeResponseParseTest, character_set) {
  constexpr size_t kOffset = 12;
  constexpr size_t kLength = 1;

  // EOF
  {
    // missing char set ----------------------------------------------------vv
    std::vector<uint8_t> bytes =
        str2bytes("0000 0000   0000 0000 0000 0000   ");

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    EXPECT_THROW_LIKE(pkt.position_ = kOffset; prs.part2_character_set(),
                                               std::runtime_error,
                                               "start or end beyond EOF");
  }

  // ok
  {  //                                                           char set --vv
    std::vector<uint8_t> bytes =
        str2bytes("0000 0000   0000 0000 0000 0000 42");

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    pkt.position_ = kOffset;
    prs.part2_character_set();
    EXPECT_EQ(kOffset + kLength, pkt.position_);
    EXPECT_EQ(0x42u, pkt.char_set_);
  }
}

/**
 * @test Verify parsing of 23 byte zero field
 */
TEST_F(HandshakeResponseParseTest, reserved) {
  constexpr size_t kOffset = 13;
  constexpr size_t kLength = 23;

  // EOF
  {
    std::vector<uint8_t> bytes = str2bytes(
        "0000 0000   0000 0000 0000 0000 00"
        "" /* missing all 00 */);

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    EXPECT_THROW_LIKE(pkt.position_ = kOffset; prs.part3_reserved(),
                                               std::runtime_error,
                                               "start or end beyond EOF");
  }

  // reserved field is too short
  {
    std::vector<uint8_t> bytes = str2bytes(
        "0000 0000   0000 0000 0000 0000 00"
        "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "00 00" /* missing one 00 */);

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    EXPECT_THROW_LIKE(
        pkt.position_ = kOffset;
        prs.part3_reserved(), std::runtime_error, "start or end beyond EOF"

    );
  }

  // reserved field contains non-zeros
  {
    // each iteration sets a different byte of the 23-byte range to non-zero
    for (size_t one = kOffset; one < kOffset + kLength; one++) {
      std::vector<uint8_t> bytes = str2bytes(
          "0000 0000   0000 0000 0000 0000 00"
          "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
          "00");
      bytes[one] = 1;

      HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

      HandshakeResponsePacket::Parser41 prs(pkt);
      EXPECT_THROW_LIKE(pkt.position_ = kOffset;
                        prs.part3_reserved(), std::runtime_error,
                        "Handshake response packet: found non-zero value in "
                        "reserved 23-byte field");
    }
  }

  // reserved field ok
  {
    std::vector<uint8_t> bytes = str2bytes(
        "0000 0000   0000 0000 0000 0000 00"
        "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00");

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    pkt.position_ = kOffset;
    prs.part3_reserved();
    EXPECT_EQ(kOffset + kLength, pkt.position_);
  }
}

/**
 * @test Verify parsing of user name
 */
TEST_F(HandshakeResponseParseTest, username) {
  const std::vector<uint8_t> bytes_before_username = str2bytes(
      "0000 0000   0000 0000 0000 0000 00"
      "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00");
  const size_t kOffset = bytes_before_username.size();

  // EOF
  {
    std::vector<uint8_t> bytes =
        bytes_before_username;  // no username bytes follow

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    EXPECT_THROW_LIKE(pkt.position_ = kOffset; prs.part4_username(),
                                               std::runtime_error,
                                               "start beyond EOF");
  }

  // any non-zero chars will do, we only validate size
  std::vector<uint8_t> username32 = str2bytes(
      "01020304050607080910 11121314151617181920 21222324252627282930 3132");

  // username missing zero-terminator
  {
    std::vector<uint8_t> bytes = bytes_before_username;
    bytes.insert(bytes.end(), username32.begin(), username32.end());

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    EXPECT_THROW_LIKE(pkt.position_ = kOffset; prs.part4_username(),
                                               std::runtime_error,
                                               "zero-terminator not found");
  }

  // username ok
  {
    std::vector<uint8_t> bytes = bytes_before_username;
    bytes.insert(bytes.end(), username32.begin(), username32.end());
    bytes.push_back(0);  // terminator

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    pkt.position_ = kOffset;
    prs.part4_username();
    EXPECT_EQ(kOffset + username32.size() + 1, pkt.position_);
    EXPECT_EQ(std::string(username32.begin(), username32.end()), pkt.username_);
  }
}

/**
 * @test Verify parsing of auth response (partial implementation)
 */
TEST_F(HandshakeResponseParseTest, auth_response) {
  const std::vector<uint8_t> bytes_before_auth_response = str2bytes(
      "0000 0000   0000 0000 0000 0000 00"
      "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
      "11 22 33 44 00" /* username */);
  const size_t kOffset = bytes_before_auth_response.size();

  // EOF
  {
    std::vector<uint8_t> bytes =
        bytes_before_auth_response;  // no auth-response bytes follow

    for (Capabilities::Flags flags : {
             Capabilities::PROTOCOL_41 |
                 Capabilities::PLUGIN_AUTH_LENENC_CLIENT_DATA,
             Capabilities::PROTOCOL_41 | Capabilities::SECURE_CONNECTION,
         }) {
      HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

      HandshakeResponsePacket::Parser41 prs(pkt);
      prs.effective_capability_flags_ = flags;
      EXPECT_THROW_LIKE(pkt.position_ = kOffset;
                        prs.part5_auth_response(), std::runtime_error,
                        "beyond EOF"  // can be "start beyond EOF" or "start or
                                      // end beyond EOF"
      );
    }
  }

  // unsupported capability flags : both PLUGIN_AUTH_LENENC_CLIENT_DATA and
  // SECURE_CONNECTION missing
  {
    std::vector<uint8_t> bytes = bytes_before_auth_response;
    bytes.push_back(0);  // what value we add doesn't matter for this test

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    EXPECT_THROW_LIKE(pkt.position_ = kOffset;
                      prs.part5_auth_response(), std::runtime_error,
                      "Handshake response packet: capabilities "
                      "PLUGIN_AUTH_LENENC_CLIENT_DATA and SECURE_CONNECTION "
                      "both missing is not implemented atm");
  }

  // PLUGIN_AUTH_LENENC_CLIENT_DATA : ok
  {
    std::vector<uint8_t> bytes = bytes_before_auth_response;
    std::vector<uint8_t> auth_response{0x11, 0x22, 0x00, 0x33, 0x00};

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);
    pkt.seek(pkt.size());
    size_t uint_len = pkt.write_lenenc_uint(auth_response.size());
    pkt.write_bytes(auth_response);

    HandshakeResponsePacket::Parser41 prs(pkt);
    prs.effective_capability_flags_ =
        Capabilities::PLUGIN_AUTH_LENENC_CLIENT_DATA;
    pkt.position_ = kOffset;
    prs.part5_auth_response();
    EXPECT_EQ(kOffset + auth_response.size() + uint_len, pkt.position_);
    EXPECT_EQ(auth_response, pkt.auth_response_);
  }

  // SECURE_CONNECTION : ok
  {
    std::vector<uint8_t> bytes = bytes_before_auth_response;
    std::vector<uint8_t> auth_response{0x11, 0x22, 0x00, 0x33, 0x00};

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);
    pkt.seek(pkt.size());
    pkt.write_int<uint8_t>(auth_response.size());
    pkt.write_bytes(auth_response);

    HandshakeResponsePacket::Parser41 prs(pkt);
    prs.effective_capability_flags_ = Capabilities::SECURE_CONNECTION;
    pkt.position_ = kOffset;
    prs.part5_auth_response();
    EXPECT_EQ(kOffset + auth_response.size() + 1, pkt.position_);
    EXPECT_EQ(auth_response, pkt.auth_response_);
  }
}

/**
 * @test Verify parsing of database name
 */
TEST_F(HandshakeResponseParseTest, database) {
  const std::vector<uint8_t> bytes_before_database = str2bytes(
      "0000 0000   0000 0000 0000 0000 00"
      "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
      "00 00" /* reserved 23 bytes */
      "11 22 33 44 00" /* username */ "00" /* auth response */);
  const size_t kOffset = bytes_before_database.size();
  constexpr Capabilities::Flags flags = Capabilities::CONNECT_WITH_DB;

  // capability flag not set
  {
    std::vector<uint8_t> bytes = bytes_before_database;
    bytes.push_back(0);  // terminator

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    pkt.position_ = kOffset;
    prs.part6_database();
    EXPECT_EQ(kOffset, pkt.position_);
    EXPECT_EQ(std::string(""), pkt.database_);
  }

  // EOF
  {
    std::vector<uint8_t> bytes =
        bytes_before_database;  // no database bytes follow

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    prs.effective_capability_flags_ = flags;
    EXPECT_THROW_LIKE(pkt.position_ = kOffset; prs.part6_database(),
                                               std::runtime_error,
                                               "start beyond EOF");
  }

  // any non-zero chars will do, we only validate size
  std::vector<uint8_t> database = str2bytes(
      "01020304050607080910 11121314151617181920 21222324252627282930"
      "31323334353637383940 41424344454647484950 51525354555657585960 "
      "61626364");

  // database missing zero-terminator
  {
    std::vector<uint8_t> bytes = bytes_before_database;
    bytes.insert(bytes.end(), database.begin(), database.end());

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    prs.effective_capability_flags_ = flags;
    EXPECT_THROW_LIKE(pkt.position_ = kOffset; prs.part6_database(),
                                               std::runtime_error,
                                               "zero-terminator not found");
  }

  // database ok
  {
    std::vector<uint8_t> bytes = bytes_before_database;
    bytes.insert(bytes.end(), database.begin(), database.end());
    bytes.push_back(0);  // terminator

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    prs.effective_capability_flags_ = flags;
    pkt.position_ = kOffset;
    prs.part6_database();
    EXPECT_EQ(kOffset + database.size() + 1, pkt.position_);
    EXPECT_EQ(std::string(database.begin(), database.end()), pkt.database_);
  }
}

/**
 * @test Verify parsing of auth plugin name
 */
TEST_F(HandshakeResponseParseTest, auth_plugin) {
  const std::vector<uint8_t> bytes_before_auth_plugin = str2bytes(
      "0000 0000   0000 0000 0000 0000 00"
      "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
      "00 00"          /* reserved 23 bytes */
      "11 22 33 44 00" /* username */
      "00" /* auth response */ "" /* database */);
  const size_t kOffset = bytes_before_auth_plugin.size();
  constexpr Capabilities::Flags flags = Capabilities::PLUGIN_AUTH;

  // capability flag not set
  {
    std::vector<uint8_t> bytes = bytes_before_auth_plugin;
    bytes.push_back(0);  // terminator

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    pkt.position_ = kOffset;
    prs.part7_auth_plugin();
    EXPECT_EQ(kOffset, pkt.position_);
    EXPECT_EQ(std::string(""), pkt.auth_plugin_);
  }

  // EOF
  {
    std::vector<uint8_t> bytes =
        bytes_before_auth_plugin;  // no auth plugin name bytes follow

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    prs.effective_capability_flags_ = flags;
    EXPECT_THROW_LIKE(pkt.position_ = kOffset; prs.part7_auth_plugin(),
                                               std::runtime_error,
                                               "start beyond EOF");
  }

  // any non-zero chars will do, we only validate size
  std::vector<uint8_t> auth_plugin = str2bytes(
      "01020304050607080910 11121314151617181920 21222324252627282930"
      "31323334353637383940 41424344454647484950 51525354555657585960 "
      "61626364");

  // auth plugin missing zero-terminator
  {
    std::vector<uint8_t> bytes = bytes_before_auth_plugin;
    bytes.insert(bytes.end(), auth_plugin.begin(), auth_plugin.end());

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    prs.effective_capability_flags_ = flags;
    EXPECT_THROW_LIKE(pkt.position_ = kOffset; prs.part7_auth_plugin(),
                                               std::runtime_error,
                                               "zero-terminator not found");
  }

  // auth plugin name ok
  {
    std::vector<uint8_t> bytes = bytes_before_auth_plugin;
    bytes.insert(bytes.end(), auth_plugin.begin(), auth_plugin.end());
    bytes.push_back(0);  // terminator

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    prs.effective_capability_flags_ = flags;
    pkt.position_ = kOffset;
    prs.part7_auth_plugin();
    EXPECT_EQ(kOffset + auth_plugin.size() + 1, pkt.position_);
    EXPECT_EQ(std::string(auth_plugin.begin(), auth_plugin.end()),
              pkt.auth_plugin_);
  }
}

/**
 * @test Verify parsing of connection attributes (unimplemented atm)
 */
TEST_F(HandshakeResponseParseTest, connection_attrs) {
  const std::vector<uint8_t> bytes_before_connection_attrs = str2bytes(
      "0000 0000   0000 0000 0000 0000 00"
      "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
      "00 00"          /* reserved 23 bytes */
      "11 22 33 44 00" /* username */
      "00"             /* auth response */
      "" /* database */ "" /* auth plugin name */);
  const size_t kOffset = bytes_before_connection_attrs.size();
  constexpr Capabilities::Flags flags = Capabilities::CONNECT_ATTRS;

  // CONNECT_ATTRS is not implemented atm
  {
    std::vector<uint8_t> bytes = bytes_before_connection_attrs;

    HandshakeResponsePacket pkt(bytes, kNoPayloadParse);

    HandshakeResponsePacket::Parser41 prs(pkt);
    prs.effective_capability_flags_ = flags;
    EXPECT_THROW_LIKE(pkt.position_ = kOffset;
                      prs.part8_connection_attrs(), std::runtime_error,
                      "Handshake response packet: capability CONNECT_ATTRS is "
                      "not implemented atm");
  }
}

/**
 * @test A complete test that verifies parsing of everything we support in one
 * shot
 */
TEST_F(HandshakeResponseParseTest, all) {
  ////////////////////////////////////////////////////////////////////////////////
  //
  //  Packet format is as follows:
  //
  //    4              capability flags, CLIENT_PROTOCOL_41 always set
  //    4              max-packet size
  //    1              character set
  //    string[23]     reserved (all [0])
  //    string[NUL]    username
  //
  //    if capabilities & CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA {
  //      lenenc-int     length of auth-response
  //      string[n]      auth-response
  //    } else if capabilities & CLIENT_SECURE_CONNECTION {
  //      1              length of auth-response
  //      string[n]      auth-response
  //    } else {
  //      string[NUL]    auth-response
  //    }
  //
  //    if capabilities & CLIENT_CONNECT_WITH_DB {
  //      string[NUL]    database
  //    }
  //
  //    if capabilities & CLIENT_PLUGIN_AUTH {
  //      string[NUL]    auth plugin name
  //    }
  //
  //    if capabilities & CLIENT_CONNECT_ATTRS {
  //      lenenc-int     length of all key-values
  //      lenenc-str     key
  //      lenenc-str     value
  //      if-more data in 'length of all key-values', more keys and value pairs
  //    }
  //
  ////////////////////////////////////////////////////////////////////////////////

  // below fields are in order of appearance in the packet
  const uint8_t seq_id = 1;
  Capabilities::Flags cap_flags =
      Capabilities::PROTOCOL_41 | Capabilities::PLUGIN_AUTH_LENENC_CLIENT_DATA |
      Capabilities::CONNECT_WITH_DB | Capabilities::PLUGIN_AUTH;
  // static fields
  const uint32_t max_packet_size = 0x12345678;
  const uint8_t char_set = 0x42;
  /* reserved 23 zero bytes - no variable needed for this one */
  const std::string username = "some_user";

  // conditional fields              v-- length of following bytes
  std::vector<uint8_t> auth_response{5, 0x11, 0x22, 0x00, 0x33, 0x00};
  const std::string database = "some_database";
  const std::string auth_plugin = "some_auth_plugin";

  std::vector<uint8_t> bytes;

  // construct packet content
  {
    auto bytes_push_back_uint32 = [&bytes](uint32_t value) {
      for (size_t i = 0; i < sizeof(uint32_t); i++) {
        bytes.push_back(value & 0xff);
        value >>= CHAR_BIT;
      }
    };

    // add header
    bytes.insert(bytes.end(), 3, 0);  // payload size placeholder
    bytes.push_back(seq_id);

    // add capability flags
    bytes_push_back_uint32(cap_flags.bits());

    // add static fields
    bytes_push_back_uint32(max_packet_size);
    bytes.push_back(char_set);
    bytes.insert(bytes.end(), 23, 0);
    bytes.insert(bytes.end(), username.begin(), username.end());
    bytes.insert(bytes.end(), 0);  // username zero-terminator

    // add conditional fields
    bytes.insert(bytes.end(), auth_response.begin(), auth_response.end());
    bytes.insert(bytes.end(), database.begin(), database.end());
    bytes.insert(bytes.end(), 0);  // database zero-terminator
    bytes.insert(bytes.end(), auth_plugin.begin(), auth_plugin.end());
    bytes.insert(bytes.end(), 0);  // auth_plugin zero-terminator

    // update payload counter
    assert(bytes.size() <
           251);  // ensure that size can be encoded in a single byte
    bytes[0] = bytes.size() - 4;  // -4 because header doesn't count
  }

  // construct packet
  HandshakeResponsePacket pkt(bytes, kAutoPayloadParse, Capabilities::ALL_ONES);

  // verify that fields parsed correctly
  {
    // header
    EXPECT_EQ(bytes.size(), pkt.size());
    EXPECT_EQ(seq_id, pkt.sequence_id_);

    // capability flags
    EXPECT_EQ(cap_flags, pkt.capability_flags_);

    // static fields
    EXPECT_EQ(max_packet_size, pkt.max_packet_size_);
    EXPECT_EQ(char_set, pkt.char_set_);
    EXPECT_EQ(username, pkt.username_);

    // conditional fields
    auth_response.erase(auth_response.begin());  // erase string-length byte
    EXPECT_EQ(auth_response, pkt.auth_response_);
    EXPECT_EQ(database, pkt.database_);
    EXPECT_EQ(auth_plugin, pkt.auth_plugin_);
  }
}
