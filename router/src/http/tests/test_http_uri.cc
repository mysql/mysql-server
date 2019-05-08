/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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
 * Test of HttpUri.
 */
#include "mysqlrouter/http_common.h"

#include <gmock/gmock.h>

using ConanicalizeTestParam = std::tuple<const std::string,  // test-name
                                         const std::string,  // input
                                         const std::string   // output
                                         >;

class ConanicalizeTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<ConanicalizeTestParam> {};
/**
 * @test ensure PasswordFrontent behaves correctly.
 */
TEST_P(ConanicalizeTest, ensure) {
  EXPECT_EQ(http_uri_path_canonicalize(std::get<1>(GetParam())),
            std::get<2>(GetParam()));
}

// cleanup test-names to satisfy googletest's requirements
static std::string sanitise(const std::string &name) {
  std::string out{name};

  for (auto &c : out) {
    if (!isalnum(c)) {
      c = '_';
    }
  }

  return out;
}

INSTANTIATE_TEST_CASE_P(
    Spec, ConanicalizeTest,
    ::testing::Values(
        std::make_tuple("canonical case, single slash", "/", "/"),
        std::make_tuple("canonical case, no trailing slash", "/a", "/a"),
        std::make_tuple("canonical case", "/a/", "/a/"),
        std::make_tuple("no escape root, no trailing slash", "/..", "/"),
        std::make_tuple("no escape root", "/../", "/"),
        std::make_tuple("no escape root, no leading slash", "..", "/"),
        std::make_tuple("double-slash is ignored", "//", "/"),
        std::make_tuple("empty", "", "/"),
        std::make_tuple("single dot", "/./", "/"),
        std::make_tuple("single dot, no trailing slash", "/.", "/"),
        std::make_tuple("one up", "/a/../", "/"),
        std::make_tuple("same level", "/a/./", "/a/")),
    [](testing::TestParamInfo<ConanicalizeTestParam> param_info) {
      return sanitise(std::get<0>(param_info.param));
    });

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
