/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "dd/string_type.h"


#include <vector>
#include <algorithm>
#include <map>
#include <unordered_map>

/*
  Tests of dd::String_type (TODO: String_type_alias for now).
*/


namespace dd_string_type_unit_test {

// TODO: dd::String_type_alias -> dd::String_type
typedef dd::String_type_alias s_t;

class DDStringTypeTest : public ::testing::Test
{};

// Basic string usage
TEST(DDStringTypeTest, BasicTest)
{
  s_t x("foobar");
  x += "_tag";
  EXPECT_EQ(10U, x.size());
}

// Create string using stringstream
TEST(DDStringTypeTest, StreamTest)
{
  typedef dd::Stringstream_type_alias ss_t;

  ss_t ss;
  double d= 42.65;
  ss << "The value of d is " << d
     << " this is an integer " << 42 << std::endl;

  s_t x("Stream result: ");
  x += ss.str();
  EXPECT_EQ(61U, x.size());
}

// Strings in vector
TEST(DDStringTypeTest, VectorTest)
{
  typedef std::vector<s_t> sv_t;

  sv_t words = { "Mary", "had", "a", "little", "Lamb" };
  std::sort(words.begin(), words.end());
  ASSERT_TRUE(std::is_sorted(words.begin(), words.end()));
}

// Strings as keys and values in maps
TEST(DDStringTypeTest, MapTest)
{
  typedef std::map<s_t,s_t> sm_t;

  sm_t dict= {{"large", "great"}, {"small", "little"},
              {"medium", "average"}};

  EXPECT_EQ(3U, dict.size());
  EXPECT_EQ("great", dict["large"]);
  EXPECT_EQ("little", dict["small"]);
  EXPECT_EQ("average", dict["medium"]);
}


// Strings as keys and values in unordered (hash-based) maps
TEST(DDStringTypeTest, UnorderedMapTest)
{
  typedef std::unordered_map<s_t,s_t> sm_t;

  sm_t dict= {{"large", "great"}, {"small", "little"},
              {"medium", "average"}};

  EXPECT_EQ(3U, dict.size());
  EXPECT_EQ("great", dict["large"]);
  EXPECT_EQ("little", dict["small"]);
  EXPECT_EQ("average", dict["medium"]);
}
} // namespace dd_string_type_unit_test
