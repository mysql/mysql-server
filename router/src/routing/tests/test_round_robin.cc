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

#include <functional>
#include <iostream>
#include <stdexcept>

#include "mysql/harness/logging/logging.h"
#include "test/helpers.h"
// TODO: what is needed ?
//#include "router_test_helpers.h"
//
#include "dest_round_robin.h"

#include "routing_mocks.h"
#include "tcp_address.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::StrEq;
using mysql_harness::TCPAddress;

class RoundRobinDestinationTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}

  MockRoutingSockOps mock_routing_sock_ops_;
};

TEST_F(RoundRobinDestinationTest, Constructor) {
  DestRoundRobin d;
  size_t exp = 0;
  ASSERT_EQ(exp, d.size());
}

TEST_F(RoundRobinDestinationTest, Add) {
  size_t exp;
  DestRoundRobin d;
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
  DestRoundRobin d;
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
  DestRoundRobin d;
  ASSERT_THROW(d.get("addr1", 1), std::out_of_range);
  d.add("addr1", 1);
  ASSERT_NO_THROW(d.get("addr1", 1));

  TCPAddress addr = d.get("addr1", 1);
  ASSERT_THAT(addr.addr, StrEq("addr1"));
  EXPECT_EQ(addr.port, 1);

  d.remove("addr1", 1);
  ASSERT_THAT(addr.addr, StrEq("addr1"));
  EXPECT_EQ(addr.port, 1);
}

TEST_F(RoundRobinDestinationTest, Size) {
  size_t exp;
  DestRoundRobin d;
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
  DestRoundRobin d;

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
 *       does not block/dealock and forces the thread close (bug#27145261).
 */
TEST_F(RoundRobinDestinationTest, SpawnAndJoinQuarantineThread) {
  DestRoundRobin d;
  d.start(nullptr);
}

TEST_F(RoundRobinDestinationTest, get_server_socket) {
  int error;

  // create round-robin (read-only) destination and add a few servers
  DestRoundRobin dest(Protocol::get_default(), &mock_routing_sock_ops_,
                      mysql_harness::kDefaultStackSizeInKiloBytes);
  std::vector<int> dest_servers_addresses{11, 12, 13};
  for (const auto &server_address : dest_servers_addresses) {
    dest.add(std::to_string(server_address), 1 /*port - doesn't matter here*/);
  }

  // NOTE: this test exploits the fact that
  // MockSocketOperations::get_mysql_socket() returns the value based on the IP
  // address it is given (it uses the number the address starts with)

  using ThrPtr = std::unique_ptr<std::thread>;
  std::vector<ThrPtr> client_threads;
  std::map<int, size_t>
      connections;  // number of connections per each destination address
  std::mutex connections_mutex;

  // spawn number of threads each trying to get the server socket at the same
  // time
  const size_t kNumClientThreads = dest_servers_addresses.size() * 10;
  for (size_t i = 0; i < kNumClientThreads; ++i) {
    client_threads.emplace_back(new std::thread([&]() {
      int addr =
          dest.get_server_socket(std::chrono::milliseconds::zero(), &error);
      {
        std::unique_lock<std::mutex> lock(connections_mutex);
        // increment the counter for returned address
        ++connections[addr];
      }
    }));
  }

  // wait for each thread to finish
  for (auto &thr : client_threads) {
    thr->join();
  }

  // we didn't simulate any connection errors so there should be no quarantine
  // so the number of connections to the the destination addresses should be
  // evenly distributed
  for (const auto &server_address : dest_servers_addresses) {
    EXPECT_EQ(connections[server_address],
              kNumClientThreads / dest_servers_addresses.size());
  }
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
