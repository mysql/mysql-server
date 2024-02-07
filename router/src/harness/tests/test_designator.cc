/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "designator.h"

////////////////////////////////////////
// Standard include files
#include <iostream>
#include <stdexcept>

////////////////////////////////////////
// Third-party include files
#include <gtest/gtest.h>

////////////////////////////////////////
// Test system include files
#include "mysql/harness/plugin.h"
#include "test/helpers.h"
#include "utilities.h"

using mysql_harness::utility::make_range;

void check_desig(const std::string &input, const std::string &plugin) {
  Designator desig(input);
  EXPECT_EQ(plugin, desig.plugin);
}

void check_desig(const std::string &input, const std::string &plugin,
                 Designator::Relation relation, long major_version,
                 long minor_version, long patch_version) {
  Designator desig(input);
  EXPECT_EQ(plugin, desig.plugin);

  EXPECT_EQ(1, static_cast<int>(desig.constraint.size()));
  std::pair<Designator::Relation, Version> elem = desig.constraint.front();
  EXPECT_EQ(relation, elem.first);
  EXPECT_EQ(major_version, elem.second.ver_major);
  EXPECT_EQ(minor_version, elem.second.ver_minor);
  EXPECT_EQ(patch_version, elem.second.ver_patch);
}

void check_desig(const std::string &input, const std::string &plugin,
                 Designator::Relation relation1, long major_version1,
                 long minor_version1, long patch_version1,
                 Designator::Relation relation2, long major_version2,
                 long minor_version2, long patch_version2) {
  Designator desig(input);
  EXPECT_EQ(plugin, desig.plugin);

  EXPECT_EQ(2, static_cast<int>(desig.constraint.size()));
  std::pair<Designator::Relation, Version> elem1 = desig.constraint[0];
  EXPECT_EQ(relation1, elem1.first);
  EXPECT_EQ(major_version1, elem1.second.ver_major);
  EXPECT_EQ(minor_version1, elem1.second.ver_minor);
  EXPECT_EQ(patch_version1, elem1.second.ver_patch);

  std::pair<Designator::Relation, Version> elem2 = desig.constraint[1];
  EXPECT_EQ(relation2, elem2.first);
  EXPECT_EQ(major_version2, elem2.second.ver_major);
  EXPECT_EQ(minor_version2, elem2.second.ver_minor);
  EXPECT_EQ(patch_version2, elem2.second.ver_patch);
}

TEST(TestDesignator, TestGoodDesignators) {
  check_desig("foo", "foo");
  check_desig("foo(<<1)", "foo", Designator::LESS_THEN, 1, 0, 0);
  check_desig("foo (<=1.2)  ", "foo", Designator::LESS_EQUAL, 1, 2, 0);
  check_desig("foo  (  >>  1.2.3  ) \t", "foo", Designator::GREATER_THEN, 1, 2,
              3);
  check_desig("foo\t(!=1.2.55)\t", "foo", Designator::NOT_EQUAL, 1, 2, 55);
  check_desig("foo\t(==1.4711.001)\t", "foo", Designator::EQUAL, 1, 4711, 1);

  check_desig("foo (<=1.2, >>1.3)  ", "foo", Designator::LESS_EQUAL, 1, 2, 0,
              Designator::GREATER_THEN, 1, 3, 0);
  check_desig("foo (<=1.2 , >>1.3)  ", "foo", Designator::LESS_EQUAL, 1, 2, 0,
              Designator::GREATER_THEN, 1, 3, 0);
  check_desig("foo(<=1.2, >>1.3)", "foo", Designator::LESS_EQUAL, 1, 2, 0,
              Designator::GREATER_THEN, 1, 3, 0);
}

TEST(TestDesignator, TestBadDesignators) {
  const char *strings[] = {
      "foo(",           "foo\t(!1.2.55)", "foo\t(!1.2.55)", "foo\t(=1.2.55)",
      "foo\t(<1.2.55)", "foo\t(<<1.2.",   "foo\t(<<1.2",    "foo\t(<<.2.55)",
      "foo\t(<<1.2.55", "foo<<1.2.55",
  };

  for (auto input : make_range(strings, sizeof(strings) / sizeof(*strings))) {
    auto make_designator = [&]() { Designator desig{input}; };
    EXPECT_THROW(make_designator(), std::runtime_error);
  }
}

TEST(TestDesignator, TestVersion) {
  EXPECT_EQ(Version(1, 0, 0), Version(1, 0, 0));
  EXPECT_FALSE(Version(1, 0, 0) < Version(1, 0, 0));
  EXPECT_LE(Version(1, 0, 0), Version(1, 0, 0));
  EXPECT_FALSE(Version(1, 0, 0) > Version(1, 0, 0));
  EXPECT_GE(Version(1, 0, 0), Version(1, 0, 0));

  EXPECT_NE(Version(1, 0, 0), Version(1, 0, 1));
  EXPECT_LT(Version(1, 0, 0), Version(1, 0, 1));
  EXPECT_LE(Version(1, 0, 0), Version(1, 0, 1));
  EXPECT_FALSE(Version(1, 0, 0) > Version(1, 0, 1));
  EXPECT_FALSE(Version(1, 0, 0) >= Version(1, 0, 1));

  EXPECT_FALSE(Version(1, 0, 0) == Version(1, 1, 0));
  EXPECT_LT(Version(1, 0, 0), Version(1, 1, 0));
  EXPECT_LE(Version(1, 0, 0), Version(1, 1, 0));
  EXPECT_FALSE(Version(1, 0, 0) > Version(1, 1, 0));
  EXPECT_FALSE(Version(1, 0, 0) >= Version(1, 1, 0));

  EXPECT_FALSE(Version(1, 0, 0) == Version(1, 1, 5));
  EXPECT_LT(Version(1, 0, 0), Version(1, 1, 5));
  EXPECT_LE(Version(1, 0, 0), Version(1, 1, 5));
  EXPECT_FALSE(Version(1, 0, 0) > Version(1, 1, 5));
  EXPECT_FALSE(Version(1, 0, 0) >= Version(1, 1, 5));

  EXPECT_FALSE(Version(1, 0, 0) == Version(2, 1, 5));
  EXPECT_LT(Version(1, 0, 0), Version(2, 1, 5));
  EXPECT_LE(Version(1, 0, 0), Version(2, 1, 5));
  EXPECT_FALSE(Version(1, 0, 0) > Version(2, 1, 5));
  EXPECT_FALSE(Version(1, 0, 0) >= Version(2, 1, 5));

  EXPECT_EQ(Version(VERSION_NUMBER(1, 0, 0)), Version(1, 0, 0));
  EXPECT_EQ(Version(VERSION_NUMBER(1, 1, 0)), Version(1, 1, 0));
  EXPECT_EQ(Version(VERSION_NUMBER(1, 2, 0)), Version(1, 2, 0));
  EXPECT_EQ(Version(VERSION_NUMBER(1, 0, 2)), Version(1, 0, 2));
  EXPECT_EQ(Version(VERSION_NUMBER(1, 2, 3)), Version(1, 2, 3));
}

TEST(TestDesignator, TestConstraints) {
  EXPECT_TRUE(Designator("foo(<< 1.2)").version_good(Version(1, 1)));
  EXPECT_FALSE(Designator("foo(<< 1.2)").version_good(Version(1, 2)));
  EXPECT_TRUE(Designator("foo(<= 1.2)").version_good(Version(1, 2)));
  EXPECT_FALSE(Designator("foo(<= 1.2)").version_good(Version(1, 2, 1)));
  EXPECT_TRUE(Designator("foo(>= 1.2)").version_good(Version(1, 2, 2)));
  EXPECT_TRUE(Designator("foo(>>1.2)").version_good(Version(1, 2, 2)));
  EXPECT_FALSE(
      Designator("foo(>= 1.2, !=1.2.2)").version_good(Version(1, 2, 2)));
  EXPECT_FALSE(
      Designator("foo(>> 1.2, !=1.2.2)").version_good(Version(1, 2, 2)));
  EXPECT_TRUE(
      Designator("foo(>> 1.2, !=1.2.2)").version_good(Version(1, 2, 3)));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
