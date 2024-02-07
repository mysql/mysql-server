/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>

#include "plugin/group_replication/include/member_version.h"

namespace member_version_unittest {

class MemberVersionTest : public ::testing::Test {
 protected:
  MemberVersionTest() = default;

  void SetUp() override {
    version = new Member_version(0x010206);  // version: 1.2.6
  }

  void TearDown() override { delete version; }

  Member_version *version;
};

TEST_F(MemberVersionTest, AssertFullVersion) {
  ASSERT_EQ(0x010206, (int)this->version->get_version());
}

TEST_F(MemberVersionTest, AssertMajorVersion) {
  ASSERT_EQ(1, (int)this->version->get_major_version());
}

TEST_F(MemberVersionTest, AssertMinorVersion) {
  ASSERT_EQ(2, (int)this->version->get_minor_version());
}

TEST_F(MemberVersionTest, AssertPatchVersion) {
  ASSERT_EQ(6, (int)this->version->get_patch_version());
}

TEST_F(MemberVersionTest, AssertEqualsOperator) {
  Member_version another_version(0x010206);  // version: 1.2.6

  ASSERT_TRUE(*version == another_version);
}

TEST_F(MemberVersionTest, AssertLtOperator) {
  Member_version same_version(0x010206);  // version: 1.2.6

  ASSERT_FALSE(*version < same_version);

  Member_version major_major_version(0x020206);  // version: 2.2.6

  ASSERT_TRUE(*version < major_major_version);

  Member_version major_minor_version(0x010306);  // version: 1.3.6

  ASSERT_TRUE(*version < major_minor_version);

  Member_version major_patch_version(0x010207);  // version: 1.2.7

  ASSERT_TRUE(*version < major_patch_version);
}

TEST_F(MemberVersionTest, AssertGtOperator) {
  Member_version same_version(0x010206);  // version: 1.2.6

  ASSERT_FALSE(*version > same_version);

  Member_version minor_major_version(0x000206);  // version: 0.2.6

  ASSERT_TRUE(*version > minor_major_version);

  Member_version minor_minor_version(0x010106);  // version: 1.1.6

  ASSERT_TRUE(*version > minor_minor_version);

  Member_version minor_patch_version(0x010205);  // version: 1.2.5

  ASSERT_TRUE(*version > minor_patch_version);
}

TEST_F(MemberVersionTest, AssertGtEqualsOperator) {
  Member_version same_version(0x010206);  // version: 1.2.6

  ASSERT_TRUE(*version >= same_version);

  Member_version lower_version(0x010205);  // version: 1.2.5

  ASSERT_TRUE(*version >= lower_version);
}

TEST_F(MemberVersionTest, AssertLtEqualsOperator) {
  Member_version same_version(0x010206);  // version: 1.2.6

  ASSERT_TRUE(*version <= same_version);

  Member_version higher_version(0x010207);  // version: 1.2.7

  ASSERT_TRUE(*version <= higher_version);
}

#ifndef NDEBUG
TEST_F(MemberVersionTest, IncrementMajor) {
  Member_version a(0x080400);  // version: 8.4.0
  a.increment_major_version();
  ASSERT_EQ("9.4.0", a.get_version_string());
  ASSERT_EQ(0x090400, (int)a.get_version());

  Member_version b(0x090000);  // version: 9.0.0
  b.increment_major_version();
  ASSERT_EQ("10.0.0", b.get_version_string());
  ASSERT_EQ(0x100000, (int)b.get_version());

  Member_version c(0x100000);  // version: 10.0.0
  c.increment_major_version();
  ASSERT_EQ("11.0.0", c.get_version_string());
  ASSERT_EQ(0x110000, (int)c.get_version());

  Member_version d(0x989999);  // version: 98.99.99
  d.increment_major_version();
  ASSERT_EQ("99.99.99", d.get_version_string());
  ASSERT_EQ(0x999999, (int)d.get_version());
}

TEST_F(MemberVersionTest, DecrementMajor) {
  Member_version a(0x080400);  // version: 8.4.0
  a.decrement_major_version();
  ASSERT_EQ("7.4.0", a.get_version_string());
  ASSERT_EQ(0x070400, (int)a.get_version());

  Member_version b(0x100000);  // version: 10.0.0
  b.decrement_major_version();
  ASSERT_EQ("9.0.0", b.get_version_string());
  ASSERT_EQ(0x090000, (int)b.get_version());

  Member_version c(0x110000);  // version: 11.0.0
  c.decrement_major_version();
  ASSERT_EQ("10.0.0", c.get_version_string());
  ASSERT_EQ(0x100000, (int)c.get_version());
}

TEST_F(MemberVersionTest, IncrementMinor) {
  Member_version a(0x080400);  // version: 8.4.0
  a.increment_minor_version();
  ASSERT_EQ("8.5.0", a.get_version_string());
  ASSERT_EQ(0x080500, (int)a.get_version());

  Member_version b(0x089900);  // version: 8.99.0
  b.increment_minor_version();
  ASSERT_EQ("9.0.0", b.get_version_string());
  ASSERT_EQ(0x090000, (int)b.get_version());
}

TEST_F(MemberVersionTest, DecrementMinor) {
  Member_version a(0x080400);  // version: 8.4.0
  a.decrement_minor_version();
  ASSERT_EQ("8.3.0", a.get_version_string());
  ASSERT_EQ(0x080300, (int)a.get_version());

  Member_version b(0x090000);  // version: 8.99.0
  b.decrement_minor_version();
  ASSERT_EQ("8.99.0", b.get_version_string());
  ASSERT_EQ(0x089900, (int)b.get_version());

  Member_version c(0x010000);  // version: 1.0.0
  c.decrement_minor_version();
  ASSERT_EQ("0.99.0", c.get_version_string());
  ASSERT_EQ(0x009900, (int)c.get_version());
}

TEST_F(MemberVersionTest, IncrementPatch) {
  Member_version a(0x080400);  // version: 8.4.0
  a.increment_patch_version();
  ASSERT_EQ("8.4.1", a.get_version_string());
  ASSERT_EQ(0x080401, (int)a.get_version());

  Member_version b(0x080099);  // version: 8.0.99
  b.increment_patch_version();
  ASSERT_EQ("8.1.0", b.get_version_string());
  ASSERT_EQ(0x080100, (int)b.get_version());

  Member_version c(0x089999);  // version: 8.99.99
  c.increment_patch_version();
  ASSERT_EQ("9.0.0", c.get_version_string());
  ASSERT_EQ(0x090000, (int)c.get_version());

  Member_version d(0x099999);  // version: 9.99.99
  d.increment_patch_version();
  ASSERT_EQ("10.0.0", d.get_version_string());
  ASSERT_EQ(0x100000, (int)d.get_version());
}

TEST_F(MemberVersionTest, DecrementPatch) {
  Member_version a(0x080401);  // version: 8.4.1
  a.decrement_patch_version();
  ASSERT_EQ("8.4.0", a.get_version_string());
  ASSERT_EQ(0x080400, (int)a.get_version());

  Member_version b(0x080100);  // version: 8.1.0
  b.decrement_patch_version();
  ASSERT_EQ("8.0.99", b.get_version_string());
  ASSERT_EQ(0x080099, (int)b.get_version());

  Member_version c(0x090000);  // version: 9.0.0
  c.decrement_patch_version();
  ASSERT_EQ("8.99.99", c.get_version_string());
  ASSERT_EQ(0x089999, (int)c.get_version());

  Member_version d(0x100000);  // version: 10.0.0
  d.decrement_patch_version();
  ASSERT_EQ("9.99.99", d.get_version_string());
  ASSERT_EQ(0x099999, (int)d.get_version());
}
#endif /* NDEBUG */

}  // namespace member_version_unittest
