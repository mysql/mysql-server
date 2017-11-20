/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <gtest/gtest.h>
#include <mysql/service_my_snprintf.h>

#include "mysqlxclient/xdecimal.h"


namespace xpl {
namespace test {

TEST(xdecimal, str) {
  EXPECT_EQ(std::string("\x00\x1C", 2), xcl::Decimal::from_str("1").to_bytes());
  EXPECT_EQ(std::string("\x00\x12\x7c", 3),
            xcl::Decimal::from_str("127").to_bytes());
  EXPECT_EQ(std::string("\x05\x12\x34\x51\x23\x45\xc0", 7),
            xcl::Decimal::from_str("12345.12345").to_bytes());

  EXPECT_EQ("12345.12345", xcl::Decimal::from_str("12345.12345").to_string());
  EXPECT_EQ("1234.12345", xcl::Decimal::from_str("1234.12345").to_string());
  EXPECT_EQ("1234.1234", xcl::Decimal::from_str("1234.1234").to_string());
  EXPECT_EQ("1.1", xcl::Decimal::from_str("1.1").to_string());
  EXPECT_EQ("12.34", xcl::Decimal::from_str("12.34").to_string());
  EXPECT_EQ("-1.1", xcl::Decimal::from_str("-1.1").to_string());
  EXPECT_EQ("-12.34", xcl::Decimal::from_str("-12.34").to_string());
  EXPECT_EQ("1.1", xcl::Decimal::from_str("+1.1").to_string());
  EXPECT_EQ("12.34", xcl::Decimal::from_str("+12.34").to_string());
  EXPECT_EQ("1", xcl::Decimal::from_str("1").to_string());
  EXPECT_EQ("12", xcl::Decimal::from_str("12").to_string());
  EXPECT_EQ("-1", xcl::Decimal::from_str("-1").to_string());
  EXPECT_EQ("-12", xcl::Decimal::from_str("-12").to_string());
  EXPECT_EQ("1", xcl::Decimal::from_str("+1").to_string());
  EXPECT_EQ("12", xcl::Decimal::from_str("+12").to_string());
}

TEST(xdecimal, bytes) {
  EXPECT_EQ("-1234567",
            xcl::Decimal::from_bytes(
                xcl::Decimal::from_bytes(std::string("\x00\x12\x34\x56\x7d", 5))
                    .to_bytes()).to_string());
  EXPECT_EQ("-123456",
            xcl::Decimal::from_bytes(
                xcl::Decimal::from_bytes(std::string("\x00\x12\x34\x56\xd0", 5))
                    .to_bytes()).to_string());
  EXPECT_EQ("1234567",
            xcl::Decimal::from_bytes(
                xcl::Decimal::from_bytes(std::string("\x00\x12\x34\x56\x7c", 5))
                    .to_bytes()).to_string());
  EXPECT_EQ("123456",
            xcl::Decimal::from_bytes(
                xcl::Decimal::from_bytes(std::string("\x00\x12\x34\x56\xc0", 5))
                    .to_bytes()).to_string());
  EXPECT_EQ("-1234567.00",
            xcl::Decimal::from_bytes(
                xcl::Decimal::from_bytes(std::string("\x02\x12\x34\x56\x70\x0d",
                                                     6)).to_bytes()).to_string());
  EXPECT_EQ("-123456.11",
            xcl::Decimal::from_bytes(
                xcl::Decimal::from_bytes(std::string("\x02\x12\x34\x56\x11\xd0",
                                                     6)).to_bytes()).to_string());
  EXPECT_EQ("1234567.20",
            xcl::Decimal::from_bytes(
                xcl::Decimal::from_bytes(std::string("\x02\x12\x34\x56\x72\x0c",
                                                     6)).to_bytes()).to_string());
  EXPECT_EQ("123456.34",
            xcl::Decimal::from_bytes(
                xcl::Decimal::from_bytes(std::string("\x02\x12\x34\x56\x34\xc0",
                                                     6)).to_bytes()).to_string());
}

TEST(xdecimal, invalid) {
  ASSERT_FALSE(xcl::Decimal::from_str("bla").is_valid());
  ASSERT_FALSE(xcl::Decimal::from_str("042423x").is_valid());
  ASSERT_FALSE(xcl::Decimal::from_str("--042423").is_valid());
  ASSERT_FALSE(xcl::Decimal::from_str("-").is_valid());
  ASSERT_FALSE(xcl::Decimal::from_str("+").is_valid());
  ASSERT_FALSE(xcl::Decimal::from_str("+-").is_valid());
  ASSERT_TRUE(xcl::Decimal::from_str("-.0").is_valid());
  ASSERT_FALSE(xcl::Decimal::from_str("0.rewq").is_valid());
  ASSERT_FALSE(xcl::Decimal::from_str("0.0.0").is_valid());
  ASSERT_FALSE(xcl::Decimal::from_str("0.0.").is_valid());
  ASSERT_FALSE(xcl::Decimal::from_str("0f").is_valid());
}

}  // namespace test
}  // namespace xpl
