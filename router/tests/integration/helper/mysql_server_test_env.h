/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_TESTS_INTEGRATION_HELPER_MYSQL_SERVER_TEST_ENV_H_
#define ROUTER_TESTS_INTEGRATION_HELPER_MYSQL_SERVER_TEST_ENV_H_

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"

/* test environment.
 *
 * spawns servers for the tests.
 */
template <uint32_t noOfServers, typename Server>
class MySQLServerTestEnv : public ::testing::Environment {
 public:
  // More suiting is unique_ptr, still the
  // vector is shared outside this class thus it must be copyable
  // and the user must not delete holded instances by accident.
  using VectorOfServers = std::vector<std::shared_ptr<Server>>;

  void SetUp() override {
    shared_servers_.resize(noOfServers);
    for (auto &s : shared_servers_) {
      if (s == nullptr) {
        s.reset(new Server(port_pool_));
        s->prepare_datadir();
        s->spawn_server();

        if (s->mysqld_failed_to_start()) {
          GTEST_SKIP() << "mysql-server failed to start.";
        }
      }
    }

    run_slow_tests_ = std::getenv("RUN_SLOW_TESTS") != nullptr;
  }

  VectorOfServers servers() { return shared_servers_; }

  TcpPortPool &port_pool() { return port_pool_; }

  [[nodiscard]] bool run_slow_tests() const { return run_slow_tests_; }

  void TearDown() override {
    for (auto &s : shared_servers_) {
      if (s == nullptr || s->mysqld_failed_to_start()) continue;

      EXPECT_NO_ERROR(s->shutdown());
    }

    for (auto &s : shared_servers_) {
      if (s == nullptr || s->mysqld_failed_to_start()) continue;

      EXPECT_NO_ERROR(s->process_manager().wait_for_exit());
    }

    shared_servers_.clear();

    Server::destroy_statics();
  }

 protected:
  TcpPortPool port_pool_;
  VectorOfServers shared_servers_{};
  bool run_slow_tests_{false};
};

#endif  // ROUTER_TESTS_INTEGRATION_HELPER_MYSQL_SERVER_TEST_ENV_H_
