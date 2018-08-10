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

#include "dest_next_available.h"
#include "routing_mocks.h"
#include "test/helpers.h"

class NextAvailableTest : public ::testing::Test {
 public:
  NextAvailableTest()
      : routing_sock_ops_(new MockRoutingSockOps()),
        dest_(Protocol::Type::kClassicProtocol, routing_sock_ops_.get()) {
    dest_.add("41", 1);
    dest_.add("42", 2);
    dest_.add("43", 3);
  }

  DestNextAvailable &dest() { return dest_; }

 protected:
  std::unique_ptr<MockRoutingSockOps> routing_sock_ops_;

 private:
  DestNextAvailable dest_;  // this is the class we're testing
};

// The idea behind these tests is to test DestNextAvailable::get_server_socket()
// server selection strategy. That method is responsible for returning the new
// connection to the active server. The active server should be switched in such
// fashion:
//
//   A -> B -> C -> sorry, no more servers (regardless of whether A and B go
//   back up or not)
//
// The switch should occur only when the current active server becomes
// unavailable. DestNextAvailable::get_server_socket() relies on
// RoutingSockOpsInterface::get_mysql_socket() to return the actual file
// descriptor, which we mock in this test to simulate connection success or
// failure.

TEST_F(NextAvailableTest, TypicalFailoverSequence) {
  int dummy;

  // talk to 1st server
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 41);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 41);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 41);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 41);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 41);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            5);  // 5 good connections

  // fail 1st server -> failover to 2nd
  routing_sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 42);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            2);  // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 42);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            4);  // 4 more good conns

  // fail 2nd server -> failover to 3rd
  routing_sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            2);  // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            4);  // 4 more good conns

  // fail 3rd server -> no more servers
  routing_sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            1);  // 1 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            0);  // no more servers
}

TEST_F(NextAvailableTest, StartWith1stDown) {
  int dummy;

  // fail 1st server -> failover to 2nd
  routing_sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 42);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            2);  // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 42);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 42);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            4);  // 4 more good conns

  // fail 2nd server -> failover to 3rd
  routing_sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            2);  // 1 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            4);  // 4 more good conns

  // fail 3rd server -> no more servers
  routing_sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            1);  // 1 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            0);  // no more servers
}

TEST_F(NextAvailableTest, StartWith2ndDown) {
  int dummy;

  // fail 1st and 2nd server -> failover to 3rd
  routing_sock_ops_->get_mysql_socket_fail(2);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            3);  // 2 failed + 1 good conn
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), 43);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            4);  // 4 more good conns

  // fail 3rd server -> no more servers
  routing_sock_ops_->get_mysql_socket_fail(1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            1);  // 1 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            0);  // no more servers
}

TEST_F(NextAvailableTest, StartWithAllDown) {
  int dummy;

  // fail 1st, 2nd and 3rd server -> no more servers
  routing_sock_ops_->get_mysql_socket_fail(3);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            3);  // 3 failed, no more servers
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(dest().get_server_socket(std::chrono::milliseconds(0), &dummy), -1);
  ASSERT_EQ(routing_sock_ops_->get_mysql_socket_call_cnt(),
            0);  // no more servers
}

int main(int argc, char *argv[]) {
  init_test_logger();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
