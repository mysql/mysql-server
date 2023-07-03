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

#include "dest_next_available.h"

#include <memory>  // unique_ptr
#include <ostream>
#include <system_error>

#include <gmock/gmock.h>  // MATCHER

#include "test/helpers.h"  // init_test_logger

//   A -> B -> C -> sorry, no more servers
//   (regardless of whether A and B go back up or not)
//
class NextAvailableTest : public ::testing::Test {
 protected:
  net::io_context io_ctx_;
};

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

TEST_F(NextAvailableTest, RepeatedFetch) {
  DestNextAvailable dest(io_ctx_, Protocol::Type::kClassicProtocol);
  dest.add("41", 41);
  dest.add("42", 42);
  dest.add("43", 43);

  SCOPED_TRACE("// destination in order");
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                               Destination("42", "42", 42),
                                               Destination("43", "43", 43)));
  }

  SCOPED_TRACE("// fetching it twice, no change");
  {
    auto actual = dest.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                               Destination("42", "42", 42),
                                               Destination("43", "43", 43)));
  }
}

TEST_F(NextAvailableTest, FailOne) {
  DestNextAvailable balancer(io_ctx_, Protocol::Type::kClassicProtocol);
  balancer.add("41", 41);
  balancer.add("42", 42);
  balancer.add("43", 43);

  SCOPED_TRACE("// destination in order");
  auto actual = balancer.destinations();
  EXPECT_THAT(actual, ::testing::SizeIs(3));
  EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                             Destination("42", "42", 42),
                                             Destination("43", "43", 43)));

  SCOPED_TRACE("// fetching it twice, no change");
  auto actual2 = balancer.destinations();
  EXPECT_THAT(actual, ::testing::SizeIs(3));
  EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                             Destination("42", "42", 42),
                                             Destination("43", "43", 43)));

  ASSERT_EQ(balancer.valid_ndx(), 0);
  EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));

  SCOPED_TRACE("// report a connection-error for the first node");
  size_t n{};
  for (auto const &d : actual) {
    d->connect_status(make_error_code(std::errc::connection_refused));

    if (++n >= 1) break;
  }

  SCOPED_TRACE("// it should result in valid-ndx moving to the 2nd node");
  ASSERT_EQ(balancer.valid_ndx(), n);

  SCOPED_TRACE("// ... but first node isn't good on 1st anymore");
  EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {false, true, true}));

  SCOPED_TRACE("// ... but first node isn't good on 2nd either");
  EXPECT_THAT(actual2, ::testing::Pointwise(IsGoodEq(), {false, true, true}));
}

TEST_F(NextAvailableTest, FailTwo) {
  DestNextAvailable balancer(io_ctx_, Protocol::Type::kClassicProtocol);
  balancer.add("41", 41);
  balancer.add("42", 42);
  balancer.add("43", 43);

  SCOPED_TRACE("// destination in order");
  auto actual = balancer.destinations();
  EXPECT_EQ(balancer.valid_ndx(), 0);
  EXPECT_THAT(actual, ::testing::SizeIs(3));
  EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                             Destination("42", "42", 42),
                                             Destination("43", "43", 43)));
  SCOPED_TRACE("// report a connection-error for the first node");
  size_t n{};
  for (auto const &d : actual) {
    d->connect_status(make_error_code(std::errc::connection_refused));

    if (++n >= 2) break;
  }

  SCOPED_TRACE("// it should result in valid-ndx moving to the 2nd node");
  ASSERT_EQ(balancer.valid_ndx(), n);

  SCOPED_TRACE("// fetching it twice, no change");
  actual = balancer.destinations();
  EXPECT_THAT(actual, ::testing::SizeIs(3));
  EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                             Destination("42", "42", 42),
                                             Destination("43", "43", 43)));

  SCOPED_TRACE("// ... but first node isn't good anymore");
  EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {false, false, true}));
}

TEST_F(NextAvailableTest, FailAll) {
  DestNextAvailable balancer(io_ctx_, Protocol::Type::kClassicProtocol);
  balancer.add("41", 41);
  balancer.add("42", 42);
  balancer.add("43", 43);

  SCOPED_TRACE("// destination in order");
  auto actual = balancer.destinations();
  EXPECT_EQ(balancer.valid_ndx(), 0);
  EXPECT_THAT(actual, ::testing::SizeIs(3));
  EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                             Destination("42", "42", 42),
                                             Destination("43", "43", 43)));

  SCOPED_TRACE("// fetching it twice, no change");
  auto actual2 = balancer.destinations();
  EXPECT_THAT(actual2, ::testing::SizeIs(3));
  EXPECT_THAT(actual2, ::testing::ElementsAre(Destination("41", "41", 41),
                                              Destination("42", "42", 42),
                                              Destination("43", "43", 43)));

  SCOPED_TRACE("// report a connection-error for all nodes");
  size_t n{};
  for (const auto &d : actual) {
    d->connect_status(make_error_code(std::errc::connection_refused));
    ++n;
  }

  SCOPED_TRACE("// it should result in valid-ndx moving to the 2nd node");
  ASSERT_EQ(balancer.valid_ndx(), n);

  SCOPED_TRACE("// ... all nodes are dead on the first");
  EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {false, false, false}));

  SCOPED_TRACE("// ... all nodes are dead on the second");
  EXPECT_THAT(actual2, ::testing::Pointwise(IsGoodEq(), {false, false, false}));
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
