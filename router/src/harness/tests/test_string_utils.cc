/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/string_utils.h"

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

////////////////////////////////////////
// Third-party include files
#include "gmock/gmock.h"

////////////////////////////////////////
// Standard include files

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using ::testing::ContainerEq;
using mysql_harness::split_string;
using std::string;

TEST(StringUtilsTests, SplitStringWithEmpty) {
  std::vector<string> exp;
  std::string tcase;

  exp = {"val1", "val2"};
  EXPECT_THAT(exp, ContainerEq(split_string("val1;val2", ';')));

  exp = {"", "val1", "val2"};
  EXPECT_THAT(exp, ContainerEq(split_string(";val1;val2", ';')));

  exp = {"val1", "val2", ""};
  EXPECT_THAT(exp, ContainerEq(split_string("val1;val2;", ';')));

  exp = {};
  EXPECT_THAT(exp, ContainerEq(split_string("", ';')));

  exp = {"", ""};
  EXPECT_THAT(exp, ContainerEq(split_string(";", ';')));

  // No trimming
  exp = {"  val1", "val2  "};
  EXPECT_THAT(exp, ContainerEq(split_string("  val1&val2  ", '&')));
}

TEST(StringUtilsTests, SplitStringWithoutEmpty) {
  std::vector<string> exp;
  std::string tcase;

  exp = {"val1", "val2"};
  EXPECT_THAT(exp, ContainerEq(split_string("val1;val2", ';', false)));

  exp = {"val1", "val2"};
  EXPECT_THAT(exp, ContainerEq(split_string(";val1;val2", ';', false)));

  exp = {"val1", "val2"};
  EXPECT_THAT(exp, ContainerEq(split_string("val1;val2;", ';', false)));

  exp = {};
  EXPECT_THAT(exp, ContainerEq(split_string("", ';', false)));

  exp = {};
  EXPECT_THAT(exp, ContainerEq(split_string(";", ';', false)));

  // No trimming
  exp = {"  val1", "val2  "};
  EXPECT_THAT(exp, ContainerEq(split_string("  val1&val2  ", '&', false)));
}
