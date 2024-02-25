/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <gtest/gtest.h>
#include <vector>
#include "libbinlogevents/include/codecs/binary.h"
#include "libbinlogevents/include/codecs/factory.h"

namespace binary_log {
namespace codecs {
namespace unittests {

class HeartbeatCodecTest : public ::testing::Test {
 protected:
  HeartbeatCodecTest() {}

  void run_codec_idempotency_test(
      std::unique_ptr<binary_log::codecs::Codec> &codec) {
    std::vector<uint64_t> positions{UINT8_MAX, UINT16_MAX, UINT32_MAX,
                                    UINT64_MAX};
    for (auto pos : positions)
      HeartbeatCodecTest::codec_idempotency_test(codec, "binlog.1000000", pos);
  }

  static void codec_idempotency_test(
      std::unique_ptr<binary_log::codecs::Codec> &codec,
      const std::string logname, uint64_t pos) {
    const Format_description_event fde(BINLOG_VERSION, "8.0.22");

    // encoded buffer
    unsigned char enc_buffer[1024];
    memset(enc_buffer, 0, sizeof(enc_buffer));

    // decoded buffer
    Heartbeat_event_v2 original;
    original.set_log_filename(logname);
    original.set_log_position(pos);

    // -------------------------------------
    // encoding
    // -------------------------------------
    auto enc_result = codec->encode(original, enc_buffer, sizeof(enc_buffer));

    // encoded internal header without errors
    ASSERT_FALSE(enc_result.second);

    // -------------------------------------
    // decoding by hand
    // -------------------------------------

    Heartbeat_event_v2 decoded;

    // decode the post LOG_EVENT header
    auto buffer = enc_buffer;
    size_t buffer_size = sizeof(enc_buffer);

    auto dec_result = codec->decode(buffer, buffer_size, decoded);

    ASSERT_FALSE(dec_result.second);
    ASSERT_TRUE(
        original.get_log_filename().compare(decoded.get_log_filename()) == 0);
    ASSERT_EQ(original.get_log_position(), decoded.get_log_position());
  }
};

TEST_F(HeartbeatCodecTest, EncodeDecodeIdempotencyBinaryTest) {
  auto codec = binary_log::codecs::Factory::build_codec(HEARTBEAT_LOG_EVENT_V2);
  HeartbeatCodecTest::run_codec_idempotency_test(codec);
}

}  // namespace unittests
}  // namespace codecs
}  // namespace binary_log
