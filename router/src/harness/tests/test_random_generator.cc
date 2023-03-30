/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "random_generator.h"

namespace {
const std::string kAlphabetDigits = "0123456789";
const std::string kAlphabetLowercase = "abcdefghijklmnopqrstuvwxyz";
const std::string kAlphabetUppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const std::string kAlphabetSpecial = "~@#$^&*()-=+]}[{|;:.>,</?";
const std::string kAlphabetAll = kAlphabetDigits + kAlphabetLowercase +
                                 kAlphabetUppercase + kAlphabetSpecial;
}  // namespace

TEST(UtilsTests, generate_identifier_ok) {
  using RandGen = mysql_harness::RandomGenerator;
  RandGen generator;
  const unsigned kTestLen = 100u;

  // digits only
  {
    std::string s =
        generator.generate_identifier(kTestLen, RandGen::AlphabetDigits);
    EXPECT_EQ(std::string::npos, s.find_first_not_of(kAlphabetDigits));
    EXPECT_EQ(kTestLen, s.size());
  }

  // lowercase letters only
  {
    std::string s =
        generator.generate_identifier(kTestLen, RandGen::AlphabetLowercase);
    EXPECT_EQ(std::string::npos, s.find_first_not_of(kAlphabetLowercase));
    EXPECT_EQ(kTestLen, s.size());
  }

  // uppercase letters only
  {
    std::string s =
        generator.generate_identifier(kTestLen, RandGen::AlphabetUppercase);
    EXPECT_EQ(std::string::npos, s.find_first_not_of(kAlphabetUppercase));
    EXPECT_EQ(kTestLen, s.size());
  }

  // special characters only
  {
    std::string s =
        generator.generate_identifier(kTestLen, RandGen::AlphabetSpecial);
    EXPECT_EQ(std::string::npos, s.find_first_not_of(kAlphabetSpecial));
    EXPECT_EQ(kTestLen, s.size());
  }

  // digits and lowercase only
  {
    std::string s = generator.generate_identifier(
        kTestLen, RandGen::AlphabetLowercase | RandGen::AlphabetDigits);
    EXPECT_EQ(std::string::npos,
              s.find_first_not_of(kAlphabetDigits + kAlphabetLowercase));
    EXPECT_EQ(kTestLen, s.size());
  }

  // length = 0
  {
    std::string s = generator.generate_identifier(0);
    EXPECT_EQ(0u, s.size());
  }

  // length = 1
  {
    std::string s = generator.generate_identifier(1);
    EXPECT_EQ(1u, s.size());
  }
}

TEST(UtilsTests, generate_identifier_wrong_alphabet_mask) {
  using RandGen = mysql_harness::RandomGenerator;
  RandGen generator;
  const unsigned kTestLen = 100u;

  {
    try {
      generator.generate_identifier(kTestLen, 0);
      FAIL() << "Expected exception";
    } catch (const std::invalid_argument &exc) {
      EXPECT_STREQ("Wrong alphabet mask provided for generate_identifier(0)",
                   exc.what());
    } catch (...) {
      FAIL() << "Invalid exception, expected std::invalid_argument";
    }
  }
}

TEST(UtilsTests, generate_identifier_check_symbols_usage) {
  // check that all the symbols from the alphabet are being used
  using RandGen = mysql_harness::RandomGenerator;
  RandGen generator;
  // number large enough so that (in practice) at least one representative of
  // each possible random char will be present in the output.  Obviously nothing
  // is 100% guaranteed, the idea is to make random test failures very very very
  // unlikely.
  constexpr unsigned kBigNumber = 10 * 1000;

  std::string s =
      generator.generate_identifier(kBigNumber, RandGen::AlphabetAll);
  for (const char &c : kAlphabetAll) {
    EXPECT_NE(std::string::npos, s.find(c));
  }
}

TEST(UtilsTests, generate_strong_password_ok) {
  mysql_harness::RandomGenerator generator;
  const unsigned kTestLen = 8u;

  const std::string pass = generator.generate_strong_password(kTestLen);

  EXPECT_EQ(kTestLen, pass.size());

  // at least one digit
  EXPECT_NE(std::string::npos, pass.find_first_of(kAlphabetDigits));
  // at least one lowercase letter
  EXPECT_NE(std::string::npos, pass.find_first_of(kAlphabetLowercase));
  // at least one uppercase letter
  EXPECT_NE(std::string::npos, pass.find_first_of(kAlphabetUppercase));
  // at least one spacial char
  EXPECT_NE(std::string::npos, pass.find_first_of(kAlphabetSpecial));

  // check that all the chars are from the alphabet
  EXPECT_EQ(std::string::npos, pass.find_first_not_of(kAlphabetAll));
}

TEST(UtilsTests, generate_strong_password_too_short) {
  mysql_harness::RandomGenerator generator;
  const unsigned kTestLen = 7u;

  try {
    generator.generate_strong_password(kTestLen);
    FAIL() << "Expected exception";
  } catch (const std::invalid_argument &exc) {
    EXPECT_STREQ("The password needs to be at least 8 charactes long",
                 exc.what());
  } catch (...) {
    FAIL() << "Invalid exception, expected std::invalid_argument";
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
