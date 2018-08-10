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

/**
 * BUG22104451 Router hangs when config value length > 256 characters
 *
 */

#include "mysql/harness/config_parser.h"

////////////////////////////////////////
// Third-party include files
#include "gtest/gtest.h"

using mysql_harness::Config;

class Bug22104451 : public ::testing::Test {
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(Bug22104451, ReadLongValues) {
  std::stringstream c;
  std::string long_destinations =
      "localhost:13005,localhost:13003,"
      "localhost:13004,localhost:17001,localhost:17001,localhost:17001,"
      "localhost:17001,localhost:17001,localhost:17001,localhost:17001,"
      "localhost:17001,localhost:17001,localhost:17001,localhost:17001,"
      "localhost:17001,localhost:17001,localhost:17001,localhost:17001,"
      "localhost:17001,localhost:17001";

  c << "[routing:c]\n"
    << "bind_address = 127.0.0.1:7006\n"
    << "destinations = " << long_destinations << "\n"
    << "mode = read-only\n";

  EXPECT_NO_THROW({
    Config config(Config::allow_keys);
    std::istringstream input(c.str());
    config.read(input);
    EXPECT_EQ(long_destinations,
              config.get("routing", "c").get("destinations"));
  });
}
