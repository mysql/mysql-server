/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "mysql/harness/destination.h"

#include <gtest/gtest.h>

// TcpDestination, init and assign

TEST(TestTcpDestination, init_default) {
  mysql_harness::TcpDestination dst{};

  EXPECT_TRUE(dst.hostname().empty());
  EXPECT_EQ(dst.port(), 0);
  EXPECT_EQ(dst.str(), ":0");
}

TEST(TestTcpDestination, init) {
  mysql_harness::TcpDestination dst{"abc", 25};

  EXPECT_EQ(dst.hostname(), "abc");
  EXPECT_EQ(dst.port(), 25);
  EXPECT_EQ(dst.str(), "abc:25");
}

// TcpDestination, compare

TEST(TestTcpDestination, cmp_same) {
  mysql_harness::TcpDestination lhs("abc", 25);
  mysql_harness::TcpDestination rhs("abc", 25);

  EXPECT_EQ(lhs, rhs);
}

TEST(TestTcpDestination, cmp_diff_host) {
  mysql_harness::TcpDestination lhs("abc", 25);
  mysql_harness::TcpDestination rhs("def", 25);

  EXPECT_LT(lhs, rhs);
}

TEST(TestTcpDestination, cmp_diff_port) {
  mysql_harness::TcpDestination lhs("abc", 25);
  mysql_harness::TcpDestination rhs("abc", 26);

  EXPECT_LT(lhs, rhs);
}

// LocalDestination, init

TEST(TestLocalDestination, init) {
  mysql_harness::LocalDestination dst("/tmp/abc");

  EXPECT_EQ(dst.path(), "/tmp/abc");
  EXPECT_EQ(dst.str(), "/tmp/abc");
}

// LocalDestination, compare

TEST(TestLocalDestination, cmp_same) {
  mysql_harness::LocalDestination lhs("/foo");
  mysql_harness::LocalDestination rhs("/bar");

  EXPECT_NE(lhs, rhs);
  EXPECT_GT(lhs, rhs);
  EXPECT_LT(rhs, lhs);
}

// Destination, init and assign

TEST(TestDestination, init_tcp) {
  mysql_harness::Destination dst{mysql_harness::TcpDestination()};

  EXPECT_TRUE(dst.is_tcp());
  EXPECT_FALSE(dst.is_local());
  EXPECT_EQ(dst.str(), mysql_harness::TcpDestination().str());
}

TEST(TestDestination, init_local) {
  mysql_harness::Destination dst{mysql_harness::LocalDestination()};

  EXPECT_FALSE(dst.is_tcp());
  EXPECT_TRUE(dst.is_local());
  EXPECT_EQ(dst.str(), mysql_harness::LocalDestination().str());
}

TEST(TestDestination, as_tcp) {
  mysql_harness::Destination dst{mysql_harness::TcpDestination("abc", 25)};

  ASSERT_TRUE(dst.is_tcp());
  EXPECT_FALSE(dst.is_local());
  EXPECT_EQ(dst.str(), "abc:25");

  EXPECT_EQ(dst.as_tcp().hostname(), "abc");
  EXPECT_EQ(dst.as_tcp().port(), 25);
}

TEST(TestDestination, assign_tcp) {
  mysql_harness::Destination from{mysql_harness::TcpDestination("abc", 25)};
  mysql_harness::Destination to = from;

  EXPECT_TRUE(to.is_tcp());
  EXPECT_FALSE(to.is_local());
  EXPECT_EQ(to.str(), from.str());
}

TEST(TestDestination, assign_local) {
  mysql_harness::Destination from{mysql_harness::LocalDestination("/tmp/abc")};
  mysql_harness::Destination to = from;

  EXPECT_FALSE(to.is_tcp());
  EXPECT_TRUE(to.is_local());
  EXPECT_EQ(to.str(), from.str());
}

TEST(TestDestination, assign_local_overwrite) {
  mysql_harness::Destination from{mysql_harness::LocalDestination("/tmp/abc")};
  mysql_harness::Destination to{mysql_harness::TcpDestination("abc", 123)};

  EXPECT_TRUE(to.is_tcp());
  EXPECT_FALSE(to.is_local());

  to = from;

  EXPECT_FALSE(to.is_tcp());
  EXPECT_TRUE(to.is_local());
  EXPECT_EQ(to.str(), from.str());
}

// Destination, compare

TEST(TestDestination, cmp_local_different) {
  mysql_harness::Destination lhs{mysql_harness::LocalDestination("/foo")};
  mysql_harness::Destination rhs{mysql_harness::LocalDestination("/bar")};

  EXPECT_EQ(lhs, lhs);
  EXPECT_EQ(rhs, rhs);
  EXPECT_NE(lhs, rhs);
  EXPECT_GT(lhs, rhs);
  EXPECT_LT(rhs, lhs);
}

TEST(TestDestination, cmp_local_same) {
  mysql_harness::Destination lhs{mysql_harness::LocalDestination("/foo")};
  mysql_harness::Destination rhs{mysql_harness::LocalDestination("/foo")};

  EXPECT_EQ(lhs, rhs);
  EXPECT_EQ(rhs, lhs);
}

TEST(TestDestination, cmp_tcp_same) {
  mysql_harness::Destination lhs{mysql_harness::TcpDestination("abc", 25)};
  mysql_harness::Destination rhs{mysql_harness::TcpDestination("abc", 25)};

  EXPECT_EQ(lhs, rhs);
}

TEST(TestDestination, cmp_tcp_differ) {
  mysql_harness::Destination lhs{mysql_harness::TcpDestination("abc", 25)};
  mysql_harness::Destination rhs{mysql_harness::TcpDestination("def", 25)};

  EXPECT_LT(lhs, rhs);
}

TEST(TestDestination, cmp_diff_types) {
  mysql_harness::Destination lhs{mysql_harness::LocalDestination()};
  mysql_harness::Destination rhs{mysql_harness::TcpDestination()};

  EXPECT_NE(lhs, rhs);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
