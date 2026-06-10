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

#include <memory>  // unique_ptr
#include <string>
#include <system_error>
#include <vector>

#include <gmock/gmock.h>  // MATCHER_P

#include "context.h"
#include "dest_static.h"

class StaticRoutingTest : public ::testing::Test {
 public:
  std::unique_ptr<StaticDestinationsManager> get_destination_manager(
      const routing::RoutingStrategy strategy) {
    RoutingConfig conf;
    conf.protocol = Protocol::Type::kClassicProtocol;
    MySQLRoutingContext routing_ctx{conf, "static", nullptr, nullptr, nullptr};

    return std::make_unique<StaticDestinationsManager>(strategy, io_ctx_,
                                                       routing_ctx);
  }

  void simulate_previous_connection_failed(
      std::unique_ptr<StaticDestinationsManager> &dest_manager) {
    dest_manager->connect_status(
        make_error_code(std::errc::connection_refused));
  }

  void setup_destinations(
      std::unique_ptr<StaticDestinationsManager> &dest_manager) {
    dest_manager->add(mysql_harness::TcpDestination{"127.0.0.1", 41});
    dest_manager->add(mysql_harness::TcpDestination{"127.0.0.1", 42});
    dest_manager->add(mysql_harness::TcpDestination{"127.0.0.1", 43});

    EXPECT_EQ(dest_manager->get_destination_candidates().size(), 3);
  }

 protected:
  net::io_context io_ctx_;
};

MATCHER_P(PortMatcher, value, "") {
  return arg->destination().as_tcp().port() == value;
}

TEST_F(StaticRoutingTest, next_available) {
  auto dest = get_destination_manager(routing::RoutingStrategy::kNextAvailable);
  setup_destinations(dest);
  std::vector<std::unique_ptr<Destination>> nodes;

  for (int i = 0; i < 3; i++) {
    nodes.push_back(dest->get_next_destination({}));
  }

  SCOPED_TRACE("First node is used");
  EXPECT_THAT(nodes, ::testing::ElementsAre(PortMatcher(41), PortMatcher(41),
                                            PortMatcher(41)));

  nodes.clear();
  SCOPED_TRACE("Move one position");
  simulate_previous_connection_failed(dest);

  SCOPED_TRACE("Next connections are successful");
  for (int i = 0; i < 3; i++) {
    nodes.push_back(dest->get_next_destination({}));
    dest->connect_status({});
  }

  SCOPED_TRACE("Second node is selected");
  EXPECT_THAT(nodes, ::testing::ElementsAre(PortMatcher(42), PortMatcher(42),
                                            PortMatcher(42)));

  SCOPED_TRACE("Move one position, to the third node");
  simulate_previous_connection_failed(dest);
  EXPECT_THAT(dest->get_next_destination({}), PortMatcher(43));

  SCOPED_TRACE("All nodes down");
  simulate_previous_connection_failed(dest);
  EXPECT_EQ(dest->get_next_destination({}), nullptr);

  SCOPED_TRACE("Not going back, still failing");
  dest->connect_status({});
  EXPECT_EQ(dest->get_next_destination({}), nullptr);
}

TEST_F(StaticRoutingTest, first_available) {
  auto dest =
      get_destination_manager(routing::RoutingStrategy::kFirstAvailable);
  setup_destinations(dest);
  std::vector<std::unique_ptr<Destination>> nodes;

  for (int i = 0; i < 3; i++) {
    nodes.push_back(dest->get_next_destination({}));
  }

  SCOPED_TRACE("First node is used");
  EXPECT_THAT(nodes, ::testing::ElementsAre(PortMatcher(41), PortMatcher(41),
                                            PortMatcher(41)));

  SCOPED_TRACE("Move one position");
  simulate_previous_connection_failed(dest);
  EXPECT_THAT(dest->get_next_destination({}), PortMatcher(42));

  SCOPED_TRACE("Connection ok, we should try from the beginning again");
  nodes.clear();
  dest->connect_status({});
  for (int i = 0; i < 3; i++) {
    nodes.push_back(dest->get_next_destination({}));
  }
  EXPECT_THAT(nodes, ::testing::ElementsAre(PortMatcher(41), PortMatcher(41),
                                            PortMatcher(41)));

  SCOPED_TRACE("Two connections failed, we should end up on the third node");
  simulate_previous_connection_failed(dest);
  dest->get_next_destination({});  // move to second node
  simulate_previous_connection_failed(
      dest);  // second node failed, move one position again
  EXPECT_THAT(dest->get_next_destination({}), PortMatcher(43));

  SCOPED_TRACE("Third connection failed, no nodes are available");
  simulate_previous_connection_failed(dest);
  EXPECT_EQ(dest->get_next_destination({}), nullptr);

  SCOPED_TRACE("Connection ok, we should try from the beginning again");
  nodes.clear();
  dest->connect_status({});
  for (int i = 0; i < 3; i++) {
    nodes.push_back(dest->get_next_destination({}));
  }
  EXPECT_THAT(nodes, ::testing::ElementsAre(PortMatcher(41), PortMatcher(41),
                                            PortMatcher(41)));
}

TEST_F(StaticRoutingTest, round_robin) {
  auto dest = get_destination_manager(routing::RoutingStrategy::kRoundRobin);
  setup_destinations(dest);
  std::vector<std::unique_ptr<Destination>> nodes;

  for (int i = 0; i < 5; i++) {
    nodes.push_back(dest->get_next_destination({}));
  }

  SCOPED_TRACE("All nodes are used in round-robin fashion");
  EXPECT_THAT(nodes, ::testing::ElementsAre(PortMatcher(41), PortMatcher(42),
                                            PortMatcher(43), PortMatcher(41),
                                            PortMatcher(42)));

  SCOPED_TRACE(
      "Previous connection failed, but we go to the next node either way");
  simulate_previous_connection_failed(dest);
  EXPECT_THAT(dest->get_next_destination({}), PortMatcher(43));
  simulate_previous_connection_failed(dest);
  EXPECT_THAT(dest->get_next_destination({}), PortMatcher(41));

  SCOPED_TRACE("All nodes are down");
  simulate_previous_connection_failed(dest);
  EXPECT_EQ(dest->get_next_destination({}), nullptr);

  SCOPED_TRACE("Connection ok, we should pick up the last position");
  nodes.clear();
  dest->connect_status({});
  for (int i = 0; i < 5; i++) {
    nodes.push_back(dest->get_next_destination({}));
  }
  EXPECT_THAT(nodes, ::testing::ElementsAre(PortMatcher(43), PortMatcher(41),
                                            PortMatcher(42), PortMatcher(43),
                                            PortMatcher(41)));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
