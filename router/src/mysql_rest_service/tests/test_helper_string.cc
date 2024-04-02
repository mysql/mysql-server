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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "helper/string/contains.h"
#include "helper/string/hex.h"
#include "helper/string/trim.h"
#include "mrs/database/entry/auth_user.h"

using helper::string::hex;

TEST(helper_string, hex_c_array_one_byte_with_zeros1) {
  uint8_t buffer[1]{0x0A};
  ASSERT_EQ("0a", hex(buffer));
}

TEST(helper_string, hex_c_array_one_byte_with_zeros2) {
  uint8_t buffer[1]{0xA0};
  ASSERT_EQ("a0", hex(buffer));
}

TEST(helper_string, hex_c_array_one_byte) {
  uint8_t buffer[1]{0xAA};
  ASSERT_EQ("aa", hex(buffer));
}

TEST(helper_string, hex_c_array_several_bytes) {
  uint8_t buffer[3]{0xAA, 0xcd, 0x12};
  ASSERT_EQ("aacd12", hex(buffer));
}

class UserIdContainer {
 public:
  using UserId = mrs::database::entry::AuthUser::UserId;
  auto begin() const { return std::begin(user_id_.raw); }
  auto end() const { return std::end(user_id_.raw); }
  void push_back(uint8_t value) { user_id_.raw[push_index_++] = value; }
  auto get_user_id() const { return user_id_; }

 private:
  UserId user_id_;
  uint64_t push_index_{0};
};

TEST(helper_string, icontains_returns_true) {
  using helper::icontains;

  ASSERT_TRUE(icontains("Some STRING with NUMBERS 1 23 456", "Some"));
  ASSERT_TRUE(icontains("Some STRING with NUMBERS 1 23 456", "STRING"));
  ASSERT_TRUE(icontains("Some STRING with NUMBERS 1 23 456", "with"));
  ASSERT_TRUE(icontains("Some STRING with NUMBERS 1 23 456", "23"));

  ASSERT_TRUE(icontains("Some STRING with NUMBERS 1 23 456", "SOME"));
  ASSERT_TRUE(icontains("Some STRING with NUMBERS 1 23 456", "string"));
  ASSERT_TRUE(icontains("Some STRING with NUMBERS 1 23 456", "With"));
  ASSERT_TRUE(icontains("Some STRING with NUMBERS 1 23 456", "23"));
  ASSERT_TRUE(icontains("Some STRING with NUMBERS 1 23 456", "456"));

  ASSERT_TRUE(
      icontains("Some STRING with NUMBERS 1 23 456", "e STRING with NUM"));
  ASSERT_TRUE(
      icontains("Some STRING with NUMBERS 1 23 456", "E STRING WITH NUM"));
}

TEST(helper_string, icontains_returns_false) {
  using helper::icontains;

  ASSERT_FALSE(icontains("Some STRING with NUMBERS 1 23 456", "SomeX"));
  ASSERT_FALSE(icontains("Some STRING with NUMBERS 1 23 456", "XSTRING"));
  ASSERT_FALSE(icontains("Some STRING with NUMBERS 1 23 456", "with X"));
  ASSERT_FALSE(icontains("Some STRING with NUMBERS 1 23 456", "23-"));

  ASSERT_FALSE(icontains("Some STRING with NUMBERS 1 23 456", "-SOME"));
  ASSERT_FALSE(icontains("Some STRING with NUMBERS 1 23 456", "string-"));
  ASSERT_FALSE(icontains("Some STRING with NUMBERS 1 23 456", "-With"));
  ASSERT_FALSE(icontains("Some STRING with NUMBERS 1 23 456", "23-"));
  ASSERT_FALSE(icontains("Some STRING with NUMBERS 1 23 456", "456X"));

  ASSERT_FALSE(
      icontains("Some STRING with NUMBERS 1 23 456", "e STRING withXNUM"));
  ASSERT_FALSE(
      icontains("Some STRING with NUMBERS 1 23 456", "E STRING WITH NUMX"));
}

TEST(helper_string_ends_with, basic) {
  ASSERT_FALSE(helper::ends_with("my first string", ""));
  ASSERT_FALSE(helper::ends_with("my first string", "first"));
  ASSERT_FALSE(helper::ends_with("my first string", "my"));
  ASSERT_FALSE(helper::ends_with("my first string", "something"));

  ASSERT_TRUE(helper::ends_with("my first string", "g"));
  ASSERT_TRUE(helper::ends_with("my first string", "ing"));
  ASSERT_TRUE(helper::ends_with("my first string", "string"));
  ASSERT_TRUE(helper::ends_with("my first string", "first string"));
  ASSERT_TRUE(helper::ends_with("my first string", "my first string"));
}

TEST(helper_string_unhex, basic) {
  auto user_id =
      helper::string::unhex<UserIdContainer>("11ed67759d414ca7b69502001709c99c")
          .get_user_id();

  ASSERT_EQ(0x11, user_id.raw[0]);
  ASSERT_EQ(0xed, user_id.raw[1]);
  ASSERT_EQ(0x67, user_id.raw[2]);
  ASSERT_EQ("11ed67759d414ca7b69502001709c99c", user_id.to_string());
}

using Function = void (*)(std::string *);
using MakeFunction = std::string (*)(const std::string &);

template <Function f>
std::string test_make(const std::string &str) {
  std::string result = str;
  f(&result);
  return result;
}

class StringTrimSuite : public testing::TestWithParam<MakeFunction> {
 public:
  void SetUp() override { sut_ = GetParam(); }

  MakeFunction sut_;
};

class LeftStringTrimSuite : public StringTrimSuite {};

TEST_P(LeftStringTrimSuite, basic) {
  ASSERT_EQ("", sut_(""));
  ASSERT_EQ("", sut_("   "));
  ASSERT_EQ("first", sut_(" first"));
  ASSERT_EQ("first", sut_("     first"));
  ASSERT_EQ("first   ", sut_(" first   "));
  ASSERT_EQ("a   second", sut_("     a   second"));
  ASSERT_EQ("a   second   ", sut_("     a   second   "));
}

INSTANTIATE_TEST_SUITE_P(Instantation, LeftStringTrimSuite,
                         testing::Values(test_make<helper::left>,
                                         helper::make_left));

class RightStringTrimSuite : public StringTrimSuite {};

TEST_P(RightStringTrimSuite, basic) {
  ASSERT_EQ("", sut_(""));
  ASSERT_EQ("", sut_("   "));
  ASSERT_EQ(" first", sut_(" first"));
  ASSERT_EQ("first", sut_("first "));
  ASSERT_EQ("first", sut_("first    "));
  ASSERT_EQ(" first", sut_(" first   "));
  ASSERT_EQ("a   second", sut_("a   second   "));
  ASSERT_EQ("   a   second", sut_("   a   second   "));
}

INSTANTIATE_TEST_SUITE_P(Instantation, RightStringTrimSuite,
                         testing::Values(test_make<helper::right>,
                                         helper::make_right));

class TrimStringTrimSuite : public StringTrimSuite {};

TEST_P(TrimStringTrimSuite, basic) {
  ASSERT_EQ("", sut_(""));
  ASSERT_EQ("", sut_("   "));
  ASSERT_EQ("first", sut_(" first"));
  ASSERT_EQ("first", sut_("first "));
  ASSERT_EQ("first", sut_("first    "));
  ASSERT_EQ("first", sut_(" first   "));
  ASSERT_EQ("a   second", sut_("a   second   "));
  ASSERT_EQ("a   second", sut_("   a   second   "));
}

INSTANTIATE_TEST_SUITE_P(Instantation, TrimStringTrimSuite,
                         testing::Values(test_make<helper::trim>,
                                         helper::make_trim));
