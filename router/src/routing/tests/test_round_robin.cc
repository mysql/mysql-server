/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#include "dest_round_robin.h"

#include <ostream>
#include <stdexcept>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/net_ts/io_context.h"
#include "tcp_address.h"
#include "test/helpers.h"  // init_test_logger

using ::testing::StrEq;

class RoundRobinDestinationTest : public ::testing::Test {
 protected:
  net::io_context io_ctx_;
};

TEST_F(RoundRobinDestinationTest, Constructor) {
  DestRoundRobin d(io_ctx_);
  size_t exp = 0;
  ASSERT_EQ(exp, d.size());
}

TEST_F(RoundRobinDestinationTest, Add) {
  size_t exp;
  DestRoundRobin d(io_ctx_);
  exp = 1;
  d.add("addr1", 1);
  ASSERT_EQ(exp, d.size());
  exp = 2;
  d.add("addr2", 2);
  ASSERT_EQ(exp, d.size());

  // Already added destination
  d.add("addr1", 1);
  exp = 2;
  ASSERT_EQ(exp, d.size());
}

TEST_F(RoundRobinDestinationTest, Remove) {
  size_t exp;
  DestRoundRobin d(io_ctx_);
  d.add("addr1", 1);
  d.add("addr99", 99);
  d.add("addr2", 2);
  exp = 3;
  ASSERT_EQ(exp, d.size());
  d.remove("addr99", 99);
  exp = 2;
  ASSERT_EQ(exp, d.size());
  d.remove("addr99", 99);
  exp = 2;
  ASSERT_EQ(exp, d.size());
}

TEST_F(RoundRobinDestinationTest, Get) {
  DestRoundRobin d(io_ctx_);
  ASSERT_THROW(d.get("addr1", 1), std::out_of_range);
  d.add("addr1", 1);
  ASSERT_NO_THROW(d.get("addr1", 1));

  mysql_harness::TCPAddress addr = d.get("addr1", 1);
  ASSERT_THAT(addr.address(), StrEq("addr1"));
  EXPECT_EQ(addr.port(), 1);

  d.remove("addr1", 1);
  ASSERT_THAT(addr.address(), StrEq("addr1"));
  EXPECT_EQ(addr.port(), 1);
}

TEST_F(RoundRobinDestinationTest, Size) {
  size_t exp;
  DestRoundRobin d(io_ctx_);
  exp = 0;
  ASSERT_EQ(exp, d.size());
  d.add("addr1", 1);
  exp = 1;
  ASSERT_EQ(exp, d.size());
  d.remove("addr1", 1);
  exp = 0;
  ASSERT_EQ(exp, d.size());
}

TEST_F(RoundRobinDestinationTest, RemoveAll) {
  size_t exp;
  DestRoundRobin d(io_ctx_);

  d.add("addr1", 1);
  d.add("addr2", 2);
  d.add("addr3", 3);
  exp = 3;
  ASSERT_EQ(exp, d.size());

  d.clear();
  exp = 0;
  ASSERT_EQ(exp, d.size());
}

/**
 * @test DestRoundRobin spawns the quarantine thread and
 *       joins it in the destructor. Make sure the destructor
 *       does not block/deadlock and forces the thread close (bug#27145261).
 */
TEST_F(RoundRobinDestinationTest, SpawnAndJoinQuarantineThread) {
  DestRoundRobin d(io_ctx_);
  d.start(nullptr);
}

bool operator==(const std::unique_ptr<Destination> &a, const Destination &b) {
  return a->hostname() == b.hostname() && a->port() == b.port();
}

std::ostream &operator<<(std::ostream &os, const Destination &v) {
  os << "{ address: " << v.hostname() << ":" << v.port()
     << ", good: " << v.good() << "}";
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const std::unique_ptr<Destination> &v) {
  os << *v;
  return os;
}

MATCHER(IsGoodEq, "") {
  return ::testing::ExplainMatchResult(
      ::testing::Property(&Destination::good, std::get<1>(arg)),
      std::get<0>(arg).get(), result_listener);
}

TEST_F(RoundRobinDestinationTest, RepeatedFetch) {
  DestRoundRobin dest(io_ctx_, Protocol::Type::kClassicProtocol);
  dest.add("41", 41);
  dest.add("42", 42);
  dest.add("43", 43);

  SCOPED_TRACE("// fetch 0, rotate 0");
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                               Destination("42", "42", 42),
                                               Destination("43", "43", 43)));
    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));
  }

  SCOPED_TRACE("// fetch 1, rotate 1");
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("42", "42", 42),
                                               Destination("43", "43", 43),
                                               Destination("41", "41", 41)));
    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));
  }

  SCOPED_TRACE("// fetch 2, rotate 2");
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("43", "43", 43),
                                               Destination("41", "41", 41),
                                               Destination("42", "42", 42)));
    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));
  }

  SCOPED_TRACE("// fetch 3, rotate 0");
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                               Destination("42", "42", 42),
                                               Destination("43", "43", 43)));
    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));
  }
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
