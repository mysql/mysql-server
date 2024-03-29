/*
  Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "dest_first_available.h"

#include <memory>  // unique_ptr

#include <gmock/gmock.h>

#include "mysql/harness/net_ts/io_context.h"
#include "test/helpers.h"  // init_test_logger

class FirstAvailableTest : public ::testing::Test {
 protected:
  net::io_context io_ctx_;
};

bool operator==(const std::unique_ptr<Destination> &a, const Destination &b) {
  return a->hostname() == b.hostname() && a->port() == b.port();
}

std::ostream &operator<<(std::ostream &os, const Destination &v) {
  os << "(host: " << v.hostname() << ", port: " << v.port() << ")";
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const std::unique_ptr<Destination> &v) {
  os << *(v.get());
  return os;
}

std::ostream &operator<<(std::ostream &os, const Destinations &v) {
  for (const auto &dest : v) {
    os << dest;
  }
  return os;
}

MATCHER(IsGoodEq, "") {
  return ::testing::ExplainMatchResult(
      ::testing::Property(&Destination::good, std::get<1>(arg)),
      std::get<0>(arg).get(), result_listener);
}

TEST_F(FirstAvailableTest, RepeatedFetch) {
  DestFirstAvailable dest(io_ctx_, Protocol::Type::kClassicProtocol);
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

TEST_F(FirstAvailableTest, FailOne) {
  DestFirstAvailable balancer(io_ctx_, Protocol::Type::kClassicProtocol);
  balancer.add("41", 41);
  balancer.add("42", 42);
  balancer.add("43", 43);

  {
    SCOPED_TRACE("// destination in order");
    auto actual = balancer.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                               Destination("42", "42", 42),
                                               Destination("43", "43", 43)));

    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));

    SCOPED_TRACE("// report a connection-error for the first node");
    size_t n{};
    for (auto const &d : actual) {
      d->connect_status(make_error_code(std::errc::connection_refused));

      if (++n >= 1) break;
    }
  }

  {
    SCOPED_TRACE("// fetching after first is failed");
    auto actual = balancer.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("42", "42", 42),
                                               Destination("43", "43", 43),
                                               Destination("41", "41", 41)));
  }

  {
    SCOPED_TRACE("// fetching it twice, no change");
    auto actual = balancer.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("42", "42", 42),
                                               Destination("43", "43", 43),
                                               Destination("41", "41", 41)));
  }
}

TEST_F(FirstAvailableTest, FailTwo) {
  DestFirstAvailable balancer(io_ctx_, Protocol::Type::kClassicProtocol);
  balancer.add("41", 41);
  balancer.add("42", 42);
  balancer.add("43", 43);

  {
    SCOPED_TRACE("// destination in order");
    auto actual = balancer.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                               Destination("42", "42", 42),
                                               Destination("43", "43", 43)));

    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));

    SCOPED_TRACE("// report a connection-error for the first node");
    size_t n{};
    for (auto const &d : actual) {
      d->connect_status(make_error_code(std::errc::connection_refused));

      if (++n >= 2) break;
    }
  }

  {
    SCOPED_TRACE("// fetching after some dead nodes");
    auto actual = balancer.destinations();

    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("43", "43", 43),
                                               Destination("41", "41", 41),
                                               Destination("42", "42", 42)));

    // 'good' state isn't permanent.
    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));
  }

  {
    SCOPED_TRACE("// fetching it twice, no change");
    auto actual = balancer.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("43", "43", 43),
                                               Destination("41", "41", 41),
                                               Destination("42", "42", 42)));

    // 'good' state isn't permanent.
    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));
  }
}

TEST_F(FirstAvailableTest, FailAll) {
  DestFirstAvailable balancer(io_ctx_, Protocol::Type::kClassicProtocol);
  balancer.add("41", 41);
  balancer.add("42", 42);
  balancer.add("43", 43);

  {
    SCOPED_TRACE("// destination in order");
    auto actual = balancer.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                               Destination("42", "42", 42),
                                               Destination("43", "43", 43)));

    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));

    SCOPED_TRACE("// report a connection-error for all nodes");
    for (auto const &d : actual) {
      d->connect_status(make_error_code(std::errc::connection_refused));
    }
  }

  {
    SCOPED_TRACE("// fetching after first is failed");
    auto actual = balancer.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                               Destination("42", "42", 42),
                                               Destination("43", "43", 43)));
    // 'good' state isn't permanent.
    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));
  }

  {
    SCOPED_TRACE("// fetching it twice, no change");
    auto actual = balancer.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(3));
    EXPECT_THAT(actual, ::testing::ElementsAre(Destination("41", "41", 41),
                                               Destination("42", "42", 42),
                                               Destination("43", "43", 43)));
    // 'good' state isn't permanent.
    EXPECT_THAT(actual, ::testing::Pointwise(IsGoodEq(), {true, true, true}));
  }
}

// should just return an empty set and not crash/fail.
TEST_F(FirstAvailableTest, Empty) {
  DestFirstAvailable balancer(io_ctx_, Protocol::Type::kClassicProtocol);

  {
    auto actual = balancer.destinations();
    EXPECT_THAT(actual, ::testing::SizeIs(0));
  }
}
int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
