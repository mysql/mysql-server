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

#include "utilities.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundef"
#endif

////////////////////////////////////////
// Third-party include files
#include "gtest/gtest.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

////////////////////////////////////////
// Standard include files
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

using std::make_pair;
using std::map;
using std::pair;
using std::string;

using mysql_harness::utility::find_range_first;
using mysql_harness::utility::make_range;
using mysql_harness::utility::strip;
using mysql_harness::utility::strip_copy;

TEST(TestUtilities, Strip) {
  const char *strings[][2] = {
      {
          "foo",
          "foo",
      },
      {
          " foo",
          "foo",
      },
      {
          "foo ",
          "foo",
      },
      {
          " \tfoo \t\t",
          "foo",
      },
      {"", ""},
  };

  for (auto sample : make_range(strings, sizeof(strings) / sizeof(*strings))) {
    std::string str = strip_copy(sample[0]);
    EXPECT_EQ(sample[1], str);
  }
}

TEST(TestUtilities, FindRangeFirst) {
  using Map = map<pair<string, string>, string>;
  Map assoc;
  assoc.emplace(make_pair("one", "first"), "alpha");
  assoc.emplace(make_pair("one", "second"), "beta");
  assoc.emplace(make_pair("two", "first"), "gamma");
  assoc.emplace(make_pair("two", "second"), "delta");
  assoc.emplace(make_pair("two", "three"), "epsilon");

  auto rng1 = find_range_first(assoc, "one");
  ASSERT_NE(rng1.first, assoc.end());
  EXPECT_NE(rng1.second, assoc.end());
  EXPECT_EQ(2, distance(rng1.first, rng1.second));
  EXPECT_EQ("alpha", rng1.first++->second);
  EXPECT_EQ("beta", rng1.first++->second);
  EXPECT_EQ(rng1.second, rng1.first);

  auto rng2 = find_range_first(assoc, "two");
  ASSERT_NE(rng2.first, assoc.end());
  EXPECT_EQ(rng2.second, assoc.end());
  EXPECT_EQ(3, distance(rng2.first, rng2.second));
  EXPECT_EQ("gamma", rng2.first++->second);
  EXPECT_EQ("delta", rng2.first++->second);
  EXPECT_EQ("epsilon", rng2.first++->second);
  EXPECT_EQ(rng2.second, rng2.first);

  // Check for ranges that do not exist
  auto rng3 = find_range_first(assoc, "aardvark");
  EXPECT_EQ(0, distance(rng3.first, rng3.second));

  auto rng4 = find_range_first(assoc, "xyzzy");
  EXPECT_EQ(rng4.first, assoc.end());
  EXPECT_EQ(0, distance(rng4.first, rng4.second));
}
