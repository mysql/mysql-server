// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <gtest/gtest.h>
#include <bitset>
#include <sstream>
#include <string>

#include "mysql/serialization/primitive_type_codec.h"

/// @file
/// Experimental API header

namespace mysql::serialization {

static constexpr bool debug_print = false;

// This function will return the binary representation of provided vector
// with the most significant bit first, with byte boundaries indicated by
// spaces.
// The input vector is treated as an arbitrary-length unsigned integer in
// little-endian (least significant byte first).
inline std::string to_binary_string(const std::vector<uint8_t> &bytes) {
  std::stringstream ss;
  for (auto it = bytes.rbegin(); it != bytes.rend(); ++it) {
    ss << std::bitset<8>(*it).to_string();
    if (it != --bytes.rend()) {
      ss << " ";
    }
  }
  return ss.str();
}

template <typename Type>
void test_one_value(const Type &value, const std::string &answer = "") {
  std::vector<uint8_t> arr(10);
  std::size_t bytes_written = detail::write_varlen_bytes(arr.data(), value);
  arr.resize(bytes_written);
  if (debug_print) {
    std::cout << (std::is_signed_v<Type> ? " " : "u") << "int"
              << (sizeof(Type) * 8) << "_t ";
    if (sizeof(Type) == 1)
      std::cout << std::setw(21) << (int)value;
    else
      std::cout << std::setw(20) << value;
    std::cout << " encoded with " << bytes_written
              << " bytes, ";  // << std::endl
  }
  if (answer != "") {
    std::string bytes_str = to_binary_string(arr);
    ASSERT_EQ(bytes_str, answer);
  }
  std::size_t bytes_num = detail::get_size_integer_varlen(value);
  ASSERT_EQ(bytes_written, bytes_num);
  Type read_value = 0;
  std::size_t bytes_read =
      detail::read_varlen_bytes(arr.data(), 10, read_value);
  ASSERT_EQ(bytes_read, bytes_num);
  ASSERT_EQ(read_value, value);
}

TEST(PrimitiveTypeCodec, EncodeUnsigned) {
  // answers in BE
  std::vector<std::pair<uint64_t, const char *>> tests_and_answers = {
      {0ULL, "00000000"},
      {1ULL, "00000010"},
      {2ULL, "00000100"},
      {127ULL, "11111110"},
      {256ULL, "00000100 00000001"},
      {65535ULL, "00000111 11111111 11111011"},
      {0x00FFFFFFFFFFFFFFULL,
       "11111111 11111111 11111111 11111111 11111111 11111111 11111111 "
       "01111111"},
      {0x0100000000000000ULL,
       "00000001 00000000 00000000 00000000 00000000 00000000 00000000 "
       "00000000 11111111"},
      {0xFFFFFFFFFFFFFFFFULL,
       "11111111 11111111 11111111 11111111 11111111 11111111 11111111 "
       "11111111 11111111"}};
  for (const auto &[test, answer] : tests_and_answers) {
    test_one_value(test, answer);
  }
}

TEST(PrimitiveTypeCodec, EncodeSignedPositive) {
  // answers in BE
  std::vector<std::pair<int64_t, const char *>> tests_and_answers = {
      {0LL, "00000000"},
      {1LL, "00000100"},
      {2LL, "00001000"},
      {127LL, "00000011 11111001"},
      {256LL, "00001000 00000001"},
      {65535LL, "00001111 11111111 11110011"},
      {0x00FFFFFFFFFFFFFFLL,
       "00000001 11111111 11111111 11111111 11111111 "
       "11111111 11111111 11111110 11111111"},
      {0x0100000000000000LL,
       "00000010 00000000 00000000 00000000 00000000 "
       "00000000 00000000 00000000 11111111"}};
  for (const auto &[test, answer] : tests_and_answers) {
    test_one_value(test, answer);
  }
}

TEST(PrimitiveTypeCodec, EncodeSignedNegative) {
  // answers in BE
  std::vector<std::pair<int64_t, const char *>> tests_and_answers = {
      {0LL, "00000000"},
      {-1LL, "00000010"},
      {-2LL, "00000110"},
      {-128LL, "00000011 11111101"},
      {-256LL, "00000111 11111101"},
      {-65535LL, "00001111 11111111 11101011"},
      {-65536LL, "00001111 11111111 11111011"},
      {-0x00FFFFFFFFFFFFFFLL,
       "00000001 11111111 11111111 11111111 11111111 "
       "11111111 11111111 11111101 11111111"},
      {-0x0100000000000000LL,
       "00000001 11111111 11111111 11111111 11111111 "
       "11111111 11111111 11111111 11111111"},
      {-0x7FFFFFFFFFFFFFFFLL,
       "11111111 11111111 11111111 11111111 11111111 "
       "11111111 11111111 11111101 11111111"},
      {-0x7FFFFFFFFFFFFFFFLL - 1,
       "11111111 11111111 11111111 11111111 11111111 "
       "11111111 11111111 11111111 11111111"}};
  for (const auto &[test, answer] : tests_and_answers) {
    test_one_value(test, answer);
  }
}

TEST(PrimitiveTypeCodec, EncodeAllWidths) {
  uint64_t delta_table[] = {0xFFFFFFFFFFFFFFFFULL, 0, 1};
  uint64_t sign_table[] = {0xFFFFFFFFFFFFFFFFULL, 1};

  for (int bit = 0; bit < 64; ++bit) {
    for (const auto &delta : delta_table) {
      for (const auto &sign : sign_table) {
        if (debug_print) {
          std::cout << "==== (bit(" << bit << ") + " << (int)delta << ") * "
                    << (int)sign << " = " << ((1ull << bit) + delta) * sign
                    << " ====" << std::endl;
        }
        uint64_t value = ((1ull << bit) + delta) * sign;
        test_one_value(uint64_t(value));
        test_one_value(int64_t(value));
        if (bit <= 32) {
          test_one_value(uint32_t(value));
          test_one_value(int32_t(value));
          if (bit <= 16) {
            test_one_value(uint16_t(value));
            test_one_value(int16_t(value));
            if (bit <= 8) {
              test_one_value(uint8_t(value));
              test_one_value(int8_t(value));
            }
          }
        }
      }
    }
  }
}

}  // namespace mysql::serialization
