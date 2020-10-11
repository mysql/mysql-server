/*
  Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _ROUTER_COMPONENT_TEST_H_
#define _ROUTER_COMPONENT_TEST_H_

#include <chrono>

#include <gmock/gmock.h>

#include "mysqlrouter/cluster_metadata.h"
#include "process_manager.h"
#include "process_wrapper.h"
#include "tcp_port_pool.h"

/** @class RouterComponentTest
 *
 * Base class for the MySQLRouter component-like tests.
 * Enables creating processes, intercepting their output, writing to input, etc.
 *
 **/
class RouterComponentTest : public ProcessManager, public ::testing::Test {
 public:
  /** @brief Initializes the test
   */
  void SetUp() override;

  /** @brief Deinitializes the test
   */
  void TearDown() override;

  /** @brief Wait until the process' log contains a given pattern
   *
   * @param process process handle
   * @param pattern pattern in the log file to wait for
   * @param timeout maximum time to wait for a given pattern
   *
   * @return bool value indicating if the pattern was found in the log file or
   * not
   */
  bool wait_log_contains(const ProcessWrapper &process,
                         const std::string &pattern,
                         std::chrono::milliseconds timeout);

  /** @brief Sleep for a duration given as a parameter. The duration is
   * increased 10 times for the run with VALGRIND.
   */
  static void sleep_for(std::chrono::milliseconds duration);
};

/** @class CommonBootstrapTest
 *
 * Base class for the MySQLRouter component-like bootstrap tests.
 *
 **/
class RouterComponentBootstrapTest : public RouterComponentTest {
 public:
  static void SetUpTestCase() { my_hostname = "dont.query.dns"; }

 protected:
  TcpPortPool port_pool_;
  TempDirectory bootstrap_dir;
  static std::string my_hostname;
  std::string config_file;

  struct Config {
    std::string ip;
    unsigned int port;
    uint16_t http_port;
    std::string js_filename;
    bool unaccessible{false};
    std::string cluster_specific_id{"cluster-specific-id"};
  };

  void bootstrap_failover(
      const std::vector<Config> &mock_server_configs,
      const mysqlrouter::ClusterType cluster_type,
      const std::vector<std::string> &router_options = {},
      int expected_exitcode = 0,
      const std::vector<std::string> &expected_output_regex = {},
      std::chrono::milliseconds wait_for_exit_timeout =
          std::chrono::seconds(10),
      const mysqlrouter::MetadataSchemaVersion &metadata_version = {2, 0, 3},
      const std::vector<std::string> &extra_router_options = {});

  friend std::ostream &operator<<(
      std::ostream &os,
      const std::vector<std::tuple<ProcessWrapper &, unsigned int>> &T);

  ProcessWrapper &launch_router_for_bootstrap(
      std::vector<std::string> params, int expected_exit_code = EXIT_SUCCESS) {
    params.push_back("--disable-rest");
    return ProcessManager::launch_router(
        params, expected_exit_code, /*catch_stderr=*/true, /*with_sudo=*/false,
        /*wait_for_notify_ready=*/std::chrono::seconds(-1));
  }

  static constexpr const char kRootPassword[] = "fake-pass";
};

std::ostream &operator<<(
    std::ostream &os,
    const std::vector<std::tuple<ProcessWrapper &, unsigned int>> &T);

#endif  // _ROUTER_COMPONENT_TEST_H_
