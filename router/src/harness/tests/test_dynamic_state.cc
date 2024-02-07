/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/dynamic_state.h"

////////////////////////////////////////
// Standard include files
#include <sstream>
#include <stdexcept>
#include <string>

////////////////////////////////////////
// Third-party include files
#include <gmock/gmock.h>
#include <gtest/gtest.h>

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

using mysql_harness::DynamicState;

class DynamicConfigTest : public ::testing::Test {};

namespace {

std::string conf_to_str(DynamicState &conf, bool pretty = false) {
  std::stringstream ss;
  conf.save_to_stream(ss, false, pretty);
  return ss.str();
}

}  // namespace

TEST_F(DynamicConfigTest, Empty) {
  DynamicState conf{"test.json"};
  EXPECT_EQ("{\"version\":\"1.0.0\"}", conf_to_str(conf));
}

TEST_F(DynamicConfigTest, SimpleUpdate) {
  DynamicState conf{"test.json"};

  conf.update_section("a", mysql_harness::JsonValue("b"));
  EXPECT_EQ("{\"a\":\"b\",\"version\":\"1.0.0\"}", conf_to_str(conf));

  conf.update_section("a", mysql_harness::JsonValue("c"));
  EXPECT_EQ("{\"a\":\"c\",\"version\":\"1.0.0\"}", conf_to_str(conf));
}

TEST_F(DynamicConfigTest, MultipleSectionsUpdate) {
  DynamicState conf{"test.json"};

  conf.update_section("a", mysql_harness::JsonValue("b"));
  conf.update_section("c", mysql_harness::JsonValue("d"));
  EXPECT_EQ("{\"a\":\"b\",\"c\":\"d\",\"version\":\"1.0.0\"}",
            conf_to_str(conf));

  conf.update_section("a", mysql_harness::JsonValue("b2"));
  EXPECT_EQ("{\"a\":\"b2\",\"c\":\"d\",\"version\":\"1.0.0\"}",
            conf_to_str(conf));

  conf.update_section("c", mysql_harness::JsonValue("d2"));
  EXPECT_EQ("{\"a\":\"b2\",\"c\":\"d2\",\"version\":\"1.0.0\"}",
            conf_to_str(conf));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
