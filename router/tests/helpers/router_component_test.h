/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef _ROUTER_COMPONENT_TEST_H_
#define _ROUTER_COMPONENT_TEST_H_

#include <chrono>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mysql/harness/stdx/attribute.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
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
  using MySQLSession = mysqlrouter::MySQLSession;

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
  [[nodiscard]] bool wait_log_contains(const ProcessWrapper &process,
                                       const std::string &pattern,
                                       std::chrono::milliseconds timeout);

  /** @brief Checks if the process' log contains specific number of occurences
   * of a given string
   *
   * @param process             process handle
   * @param expected_string     the string to look for in the logfile
   * @param expected_occurences number of string occurences expected in the
   *                            logfile
   *
   */
  void check_log_contains(const ProcessWrapper &process,
                          const std::string &expected_string,
                          size_t expected_occurences = 1);

  /** @brief Sleep for a duration given as a parameter. The duration is
   * increased 10 times for the run with VALGRIND.
   */
  static void sleep_for(std::chrono::milliseconds duration);

  static stdx::expected<std::unique_ptr<MySQLSession>, mysqlrouter::MysqlError>
  make_new_connection(uint16_t router_port) {
    auto session = std::make_unique<MySQLSession>();

    try {
      session->connect("127.0.0.1", router_port, "username", "password", "",
                       "");
      return session;
    } catch (const MySQLSession::Error &e) {
      return stdx::unexpected(
          mysqlrouter::MysqlError{e.code(), e.message(), "HY000"});
    }
  }

  static stdx::expected<std::unique_ptr<MySQLSession>, mysqlrouter::MysqlError>
  make_new_connection(const std::string &router_socket) {
    try {
      auto session = std::make_unique<MySQLSession>();
      session->connect("", 0, "username", "password", router_socket, "");
      return session;
    } catch (const MySQLSession::Error &e) {
      return stdx::unexpected(
          mysqlrouter::MysqlError{e.code(), e.message(), "HY000"});
    }
  }

  static stdx::expected<uint16_t, mysqlrouter::MysqlError> select_port(
      MySQLSession *session) {
    try {
      auto result = session->query_one("select @@port");
      return std::strtoul((*result)[0], nullptr, 10);
    } catch (const MySQLSession::Error &e) {
      return stdx::unexpected(
          mysqlrouter::MysqlError{e.code(), e.message(), "HY000"});
    }
  }

  static void verify_port(MySQLSession *session, uint16_t expected_port) {
    auto port_res = select_port(session);
    ASSERT_TRUE(port_res) << port_res.error().message();
    ASSERT_EQ(*port_res, expected_port);
  }

  /*
   * check if an existing connection allows to execute a query.
   *
   * must be wrapped in ASSERT_NO_FATAL_FAILURE
   */
  static void verify_existing_connection_ok(MySQLSession *session) {
    auto select_res = select_port(session);
    ASSERT_TRUE(select_res) << select_res.error().message();
  }

  static void verify_new_connection_fails(uint16_t router_port) {
    ASSERT_FALSE(make_new_connection(router_port));
  }

  static void verify_existing_connection_dropped(
      MySQLSession *session,
      std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    if (getenv("WITH_VALGRIND")) {
      timeout *= 10;
    }

    const auto MSEC_STEP = std::chrono::milliseconds(50);
    const auto started = std::chrono::steady_clock::now();
    do {
      auto select_res = select_port(session);
      // query failed, connection dropped, all good
      if (!select_res) return;

      auto step = std::min(timeout, MSEC_STEP);
      RouterComponentTest::sleep_for(step);
      timeout -= step;
    } while (timeout > std::chrono::steady_clock::now() - started);

    FAIL() << "Timed out waiting for the connection to drop";
  }

  static void prepare_config_dir_with_default_certs(
      const std::string &config_dir);

  static void copy_default_certs_to_datadir(const std::string &dst_dir);

  static std::string plugin_output_directory();

  /**
   * create a ID token for OpenID connect.
   *
   * @param subject                subject of the ID-token 'sub'
   * @param identity_provider_name 'name' of the identity provider
   * @param expiry                 expiry of the ID token in seconds
   * @param private_key_file       filename of a PEM file containing
   *                               the private key
   * @param outdir                 directory name to place the id-token file
   *                               into.
   */
  stdx::expected<std::string, int> create_openid_connect_id_token_file(
      const std::string &subject, const std::string &identity_provider_name,
      int expiry, const std::string &private_key_file,
      const std::string &outdir);

 protected:
  TcpPortPool port_pool_;
};

/** @class CommonBootstrapTest
 *
 * Base class for the MySQLRouter component-like bootstrap tests.
 *
 **/
class RouterComponentBootstrapTest : virtual public RouterComponentTest {
 public:
  using OutputResponder = ProcessWrapper::OutputResponder;

  static const OutputResponder kBootstrapOutputResponder;

 protected:
  TempDirectory bootstrap_dir;
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
          std::chrono::seconds(30),
      const mysqlrouter::MetadataSchemaVersion &metadata_version = {2, 0, 3},
      const std::vector<std::string> &extra_router_options = {});

  friend std::ostream &operator<<(
      std::ostream &os,
      const std::vector<std::tuple<ProcessWrapper &, unsigned int>> &T);

  ProcessWrapper &launch_router_for_bootstrap(
      std::vector<std::string> params, int expected_exit_code = EXIT_SUCCESS,
      const bool disable_rest = true, const bool add_report_host = true,
      const bool catch_stderr = true,
      ProcessWrapper::OutputResponder output_responder =
          RouterComponentBootstrapTest::kBootstrapOutputResponder) {
    if (disable_rest) params.push_back("--disable-rest");
    if (add_report_host) params.push_back("--report-host=dont.query.dns");
    params.push_back("--conf-set-option=DEFAULT.plugin_folder=" +
                     ProcessManager::get_plugin_dir().str());

    return ProcessManager::launch_router(
        params, expected_exit_code, catch_stderr, /*with_sudo=*/false,
        /*wait_for_notify_ready=*/std::chrono::seconds(-1), output_responder);
  }

  static constexpr const char kRootPassword[] = "fake-pass";
};

class RouterComponentBootstrapWithDefaultCertsTest
    : public RouterComponentBootstrapTest {
 public:
  void SetUp() override;
};

std::ostream &operator<<(
    std::ostream &os,
    const std::vector<std::tuple<ProcessWrapper &, unsigned int>> &T);

#endif  // _ROUTER_COMPONENT_TEST_H_
