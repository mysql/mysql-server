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

/**
 * BUG21873666 Correctly using configured values instead of defaults
 *
 */

#include "mysqlrouter/routing.h"
#include "plugin_config.h"

#include <fstream>
#include <memory>
#include <string>

// ignore GMock warnings
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#endif

#include "gmock/gmock.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "mysql/harness/config_parser.h"

#include "mysql_routing.h"

using ::testing::HasSubstr;
using mysqlrouter::to_string;

class Bug21771595 : public ::testing::Test {
 protected:
  virtual void SetUp() {}

  virtual void TearDown() {}
};

TEST_F(Bug21771595, ConstructorDefaults) {
  MySQLRouting r(routing::RoutingStrategy::kRoundRobin, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "test");
  ASSERT_EQ(r.get_destination_connect_timeout(),
            routing::kDefaultDestinationConnectionTimeout);
  ASSERT_EQ(r.get_max_connections(), routing::kDefaultMaxConnections);
}

TEST_F(Bug21771595, Constructor) {
  auto expect_max_connections = routing::kDefaultMaxConnections - 10;
  auto expect_connect_timeout =
      routing::kDefaultDestinationConnectionTimeout + std::chrono::seconds(10);

  MySQLRouting r(routing::RoutingStrategy::kRoundRobin, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "test", expect_max_connections,
                 expect_connect_timeout);
  ASSERT_EQ(r.get_destination_connect_timeout(), expect_connect_timeout);
  ASSERT_EQ(r.get_max_connections(), expect_max_connections);
}

TEST_F(Bug21771595, GetterSetterMaxConnections) {
  MySQLRouting r(routing::RoutingStrategy::kRoundRobin, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "test");
  ASSERT_EQ(r.get_max_connections(), routing::kDefaultMaxConnections);
  auto expected = routing::kDefaultMaxConnections + 1;
  ASSERT_EQ(r.set_max_connections(expected), expected);
  ASSERT_EQ(r.get_max_connections(), expected);
}

TEST_F(Bug21771595, InvalidDestinationConnectTimeout) {
  MySQLRouting r(routing::RoutingStrategy::kRoundRobin, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "test");
  ASSERT_THROW(r.validate_destination_connect_timeout(std::chrono::seconds(-1)),
               std::invalid_argument);
  // ASSERT_THROW(r.set_destination_connect_timeout(UINT16_MAX+1),
  // std::invalid_argument);
  try {
    r.validate_destination_connect_timeout(std::chrono::seconds(0));
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(),
                HasSubstr("tried to set destination_connect_timeout using "
                          "invalid value, was 0 ms"));
  }
  ASSERT_THROW(
      MySQLRouting(routing::RoutingStrategy::kRoundRobin, 7001,
                   Protocol::Type::kClassicProtocol,
                   routing::AccessMode::kReadWrite, "127.0.0.1",
                   mysql_harness::Path(), "test", 1, std::chrono::seconds(-1)),
      std::invalid_argument);
}

TEST_F(Bug21771595, InvalidMaxConnections) {
  MySQLRouting r(routing::RoutingStrategy::kRoundRobin, 7001,
                 Protocol::Type::kClassicProtocol,
                 routing::AccessMode::kReadWrite, "127.0.0.1",
                 mysql_harness::Path(), "test");
  ASSERT_THROW(r.set_max_connections(-1), std::invalid_argument);
  ASSERT_THROW(r.set_max_connections(UINT16_MAX + 1), std::invalid_argument);
  try {
    r.set_max_connections(0);
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(
        exc.what(),
        HasSubstr("tried to set max_connections using invalid value, was '0'"));
  }
  ASSERT_THROW(
      MySQLRouting(routing::RoutingStrategy::kRoundRobin, 7001,
                   Protocol::Type::kClassicProtocol,
                   routing::AccessMode::kReadWrite, "127.0.0.1",
                   mysql_harness::Path(), "test", 0, std::chrono::seconds(1)),
      std::invalid_argument);
}

TEST_F(Bug21771595, InvalidPort) {
  ASSERT_THROW(MySQLRouting(routing::RoutingStrategy::kRoundRobin, 0,
                            Protocol::Type::kClassicProtocol,
                            routing::AccessMode::kReadWrite, "127.0.0.1",
                            mysql_harness::Path(), "test"),
               std::invalid_argument);
  try {
    MySQLRouting r(routing::RoutingStrategy::kRoundRobin, (uint16_t)-1,
                   Protocol::Type::kClassicProtocol,
                   routing::AccessMode::kReadWrite, "127.0.0.1",
                   mysql_harness::Path(), "test");
  } catch (const std::invalid_argument &exc) {
    ASSERT_THAT(exc.what(),
                HasSubstr("Invalid bind address, was '127.0.0.1', port -1"));
  }
}
