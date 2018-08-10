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

#include "gmock/gmock.h"

#include "posix_re.h"

class PosixReTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}
};

TEST_F(PosixReTest, search) {
  SCOPED_TRACE("// parse");

  EXPECT_TRUE(PosixRE("bcd").search("abcde"));
  EXPECT_FALSE(PosixRE("^bcd").search("abcde"));
  EXPECT_FALSE(PosixRE("bcd$").search("abcde"));
  EXPECT_TRUE(PosixRE("^bcd").search("bcde"));
}

TEST_F(PosixReTest, construct) {
  SCOPED_TRACE("// parse");

  // either a PosixREError or a std::regex_error
  EXPECT_THROW(PosixRE("[a-z]["), std::runtime_error);
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
