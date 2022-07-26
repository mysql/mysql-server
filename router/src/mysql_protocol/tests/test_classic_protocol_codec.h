/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_TEST_CLASSIC_PROTOCOL_CODEC_H_
#define MYSQLROUTER_TEST_CLASSIC_PROTOCOL_CODEC_H_

#include <gtest/gtest.h>

#include "mysqlrouter/classic_protocol_codec_base.h"

// succeeding encode/decode tests
//
template <class T>
struct CodecParam {
  const char *test_name;

  T decoded;
  classic_protocol::capabilities::value_type caps;

  std::vector<uint8_t> encoded;
};

template <class T>
class CodecTest : public ::testing::Test,
                  public ::testing::WithParamInterface<CodecParam<T>> {
 public:
  using codec_type = T;

  void test_encode(const CodecParam<T> &test_param) {
    std::vector<uint8_t> encoded;

    const auto res = classic_protocol::encode(
        test_param.decoded, test_param.caps, net::dynamic_buffer(encoded));

    ASSERT_TRUE(res);
    EXPECT_EQ(*res, test_param.encoded.size());
    EXPECT_EQ(encoded, test_param.encoded);
  }

  template <class... Args>
  void test_decode(const CodecParam<T> &test_param, Args... args) {
    const auto res = classic_protocol::Codec<codec_type>::decode(
        net::buffer(test_param.encoded), test_param.caps,
        std::forward<Args>(args)...);

    ASSERT_TRUE(res) << res.error() << ", msg: " << res.error().message();
    EXPECT_EQ(res->first, test_param.encoded.size());
    EXPECT_EQ(res->second, test_param.decoded);
  }
};

// failing encode/decode tests

struct CodecFailParam {
  const char *test_name;

  std::vector<uint8_t> encoded;
  classic_protocol::capabilities::value_type caps;

  std::error_code expected_error_code;
};

template <class T>
class CodecFailTest : public ::testing::Test,
                      public ::testing::WithParamInterface<CodecFailParam> {
 public:
  using codec_type = T;

  template <class... Args>
  void test_decode(const CodecFailParam &test_param, Args... args) {
    auto decode_res = classic_protocol::Codec<codec_type>::decode(
        net::buffer(test_param.encoded), test_param.caps,
        std::forward<Args>(args)...);

    ASSERT_FALSE(decode_res);
    EXPECT_EQ(decode_res.error(), test_param.expected_error_code);
  }
};

#endif
