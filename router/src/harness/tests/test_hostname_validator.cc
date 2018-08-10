/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "hostname_validator.h"

////////////////////////////////////////
// Test system include files

////////////////////////////////////////
// Third-party include files
#include "gmock/gmock.h"

////////////////////////////////////////
// Standard include files

using mysql_harness::is_valid_hostname;

/**
 * @file
 * @brief Unit tests for simple hostname validator
 */

/**
 * @test verify that is_valid_hostname() returns true for valid hostnames
 *
 * @note Please see function documentation for expected behavior, it may be
 *       surprising.
 */
TEST(TestHostnameValidator, valid_hostname) {
  EXPECT_TRUE(is_valid_hostname("foo"));
  EXPECT_TRUE(is_valid_hostname("foo.BAR"));
  EXPECT_TRUE(is_valid_hostname("foo-BAR_baz"));
  EXPECT_TRUE(is_valid_hostname("1.2.3.4"));
  EXPECT_TRUE(is_valid_hostname("x"));
}

/**
 * @test verify that is_valid_hostname() returns false for invalid hostnames
 *
 * @note Please see function documentation for expected behavior, it may be
 *       surprising.
 */
TEST(TestHostnameValidator, invalid_hostname) {
  EXPECT_FALSE(is_valid_hostname(""));
  EXPECT_FALSE(is_valid_hostname(" "));
  EXPECT_FALSE(is_valid_hostname("foo bar"));
  EXPECT_FALSE(is_valid_hostname("^"));
  EXPECT_FALSE(is_valid_hostname("foo^bar"));
}

/**
 * @note Things that pass at the time of implementing is_valid_hostname(), but
 *       probably shouldn't. This testcase is disabled, its sole purpose is to
 *       make the developer aware of this problem.
 */
TEST(TestHostnameValidator, DISABLED_known_mishandled_cornercases) {
  // NOTE: If any of these start failing one day, that's probably a good thing!
  //       Please see function description for details.
  EXPECT_TRUE(is_valid_hostname(".foo"));
  EXPECT_TRUE(is_valid_hostname("foo."));
  EXPECT_TRUE(is_valid_hostname(".foo.bar."));
  EXPECT_TRUE(is_valid_hostname("."));
  EXPECT_TRUE(is_valid_hostname("-"));
  EXPECT_TRUE(is_valid_hostname("1_2-3.4"));
  EXPECT_TRUE(is_valid_hostname("foo.bar.1.2"));
}
