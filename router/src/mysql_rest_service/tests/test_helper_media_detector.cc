/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <gtest/gtest.h>

#include "helper/media_detector.h"
#include "helper/string/random.h"

using helper::MediaDetector;
using helper::MediaType;
using testing::Test;
using testing::Values;
using testing::WithParamInterface;

namespace test {

std::string normalize(unsigned char payload) {
  std::string result;
  result += payload;

  return result;
}

std::string normalize(int payload) {
  std::string result;
  result += static_cast<char>(payload);

  return result;
}

std::string normalize(char payload) {
  std::string result;
  result += payload;

  return result;
}

std::string normalize(const unsigned char *payload) {
  return reinterpret_cast<const char *>(payload);
}

std::string normalize(const char *payload) { return payload; }

char generate_other_character(char value) {
  auto result = value;
  while (result == value) {
    result = helper::generate_string<1>()[0];
  }
  return result;
}

}  // namespace test

template <typename... Args>
static std::string make_payload(Args &&...args) {
  std::string result;
  result = (... + test::normalize(args));
  return result;
}

class MediaDetectorTests : public Test {
 public:
  MediaDetector sut_;
};

struct PayloadAndExpectedResult {
  MediaType expected_result;
  std::string payload;
};

void PrintTo(const PayloadAndExpectedResult &v, ::std::ostream *os) {
  (*os) << (int)v.expected_result << ",size:" << v.payload.size();
}

class ParametricStartSeqMediaDetectorTests
    : public MediaDetectorTests,
      public WithParamInterface<PayloadAndExpectedResult> {
 public:
};

INSTANTIATE_TEST_SUITE_P(
    PrefixHandlers, ParametricStartSeqMediaDetectorTests,
    Values(
        PayloadAndExpectedResult{MediaType::typeJpg, make_payload(0xff, 0xD8)},
        PayloadAndExpectedResult{MediaType::typePng, make_payload("\x89PNG")},
        PayloadAndExpectedResult{MediaType::typeBmp, make_payload(0x42, 0x4d)},
        PayloadAndExpectedResult{MediaType::typeGif, make_payload("GIF8")}));

TEST_P(ParametricStartSeqMediaDetectorTests, broken_header) {
  auto p = GetParam();
  for (std::size_t i = 0; i < p.payload.length(); ++i) {
    auto payload = p.payload;
    payload[i] = test::generate_other_character(payload[i]);
    ASSERT_EQ(MediaType::typeUnknownBinary, sut_.detect(payload));
  }
}

TEST_P(ParametricStartSeqMediaDetectorTests, detect_file) {
  auto p = GetParam();
  ASSERT_EQ(p.expected_result, sut_.detect(p.payload));
  ASSERT_EQ(p.expected_result,
            sut_.detect(p.payload + helper::generate_string<1>()));

  ASSERT_EQ(p.expected_result,
            sut_.detect(p.payload + helper::generate_string<10>()));

  ASSERT_EQ(p.expected_result,
            sut_.detect(p.payload + helper::generate_string<100>()));
}

struct TwoPayloadsAndExpectedResult : public PayloadAndExpectedResult {
  std::string payload_second;
};

void PrintTo(const TwoPayloadsAndExpectedResult &v, ::std::ostream *os) {
  (*os) << (int)v.expected_result << ",size1:" << v.payload.size()
        << ",size2:" << v.payload_second.size();
}

class ParametricTwoSequencesMediaDetectorTests
    : public MediaDetectorTests,
      public WithParamInterface<TwoPayloadsAndExpectedResult> {};

INSTANTIATE_TEST_SUITE_P(
    TwoPrefixesHandlers, ParametricTwoSequencesMediaDetectorTests,
    Values(
        TwoPayloadsAndExpectedResult{{MediaType::typeAvi, make_payload("RIFF")},
                                     make_payload("AVI ")},
        TwoPayloadsAndExpectedResult{{MediaType::typeWav, make_payload("RIFF")},
                                     make_payload("WAVEfmt")}));

TEST_P(ParametricTwoSequencesMediaDetectorTests, FOURCC_broken_header) {
  auto p = GetParam();
  /* FOURCC RIFF format:
   * 4B - FOURCC with RIFF value.
   * 4B - data_size (include file_type size)
   * 4B - file-type FOURCC
   */
  auto header_data_length_with_random_data = 8 - p.payload.length();
  auto header = p.payload +
                helper::generate_string(header_data_length_with_random_data) +
                p.payload_second;

  for (std::size_t i = 0; i < header.length(); ++i) {
    // We only break file-type-id (the FOURCC codes, not the data_size field).
    if (i >= 4 && i < 8) continue;
    auto payload = header;
    payload[i] = test::generate_other_character(payload[i]);
    ASSERT_EQ(MediaType::typeUnknownBinary, sut_.detect(payload));
  }
}

TEST_P(ParametricTwoSequencesMediaDetectorTests, FOURCC_detect_file) {
  auto p = GetParam();
  /* FOURCC RIFF format:
   * 4B - FOURCC with RIFF value.
   * 4B - data_size (include file_type size)
   * 4B - file-type FOURCC
   */
  auto header = p.payload + helper::generate_string(8 - p.payload.length()) +
                p.payload_second;
  ASSERT_EQ(p.expected_result, sut_.detect(header));
  ASSERT_EQ(p.expected_result,
            sut_.detect(header + helper::generate_string<1>()));

  ASSERT_EQ(p.expected_result,
            sut_.detect(header + helper::generate_string<10>()));

  ASSERT_EQ(p.expected_result,
            sut_.detect(header + helper::generate_string<100>()));
}
