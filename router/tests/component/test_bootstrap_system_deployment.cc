/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include <array>
#include <fstream>

#include <gmock/gmock.h>

#include "router_component_system_layout.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

#ifndef _WIN32
#include <unistd.h>
#endif

Path g_origin_path;

using namespace std::chrono_literals;

/*
 * These tests are executed only for STANDALONE layout and are not executed for
 * Windows. Bootstrap for layouts different than STANDALONE use directories to
 * which tests don't have access (see install_layout.cmake).
 */
#ifndef SKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS

class RouterBootstrapSystemDeploymentTest : public RouterComponentBootstrapTest,
                                            public RouterSystemLayout {
 protected:
  void SetUp() override {
    RouterComponentTest::SetUp();
    // this test modifies the origin path so we need to restore it
    ProcessManager::set_origin(g_origin_path);
    init_system_layout_dir(get_mysqlrouter_exec(),
                           ProcessManager::get_origin());

    set_mysqlrouter_exec(Path(exec_file_));
  }

  void TearDown() override {
    RouterComponentTest::TearDown();
    cleanup_system_layout();
  }

  auto &run_server_mock() {
    const std::string json_stmts = get_data_dir().join("bootstrap_gr.js").str();
    server_port_ = port_pool_.get_next_available();

    // launch mock server and wait for it to start accepting connections
    auto &server_mock = launch_mysql_server_mock(json_stmts, server_port_);
    return server_mock;
  }

  ProcessWrapper &launch_router_for_bootstrap(
      const std::vector<std::string> &params,
      int expected_exit_code = EXIT_SUCCESS,
      ProcessWrapper::OutputResponder output_responder =
          RouterComponentBootstrapTest::kBootstrapOutputResponder) {
    return ProcessManager::launch_router(
        params, expected_exit_code, /*catch_stderr=*/true, /*with_sudo=*/false,
        /*wait_for_notify_ready=*/-1s, output_responder);
  }

  uint16_t server_port_;
};

/*
 * This test is executed only for STANDALONE layout are is not executed for
 * Windows. Bootstrap for other layouts uses directories to which tests don't
 * have access (see install_layout.cmake).
 */
TEST_F(RouterBootstrapSystemDeploymentTest, BootstrapPass) {
  run_server_mock();

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap({
      "--bootstrap=127.0.0.1:" + std::to_string(server_port_),
      "--connect-timeout=1",
      "--report-host",
      "dont.query.dns",
  });

  // check if the bootstrapping was successful
  check_exit_code(router, EXIT_SUCCESS);

  EXPECT_TRUE(
      router.expect_output("MySQL Router configured for the "
                           "InnoDB Cluster 'mycluster'"));
}

/*
 * This test is executed only for STANDALONE layout and is not executed for
 * Windows. Bootstrap for other layouts uses directories to which tests don't
 * have access (see install_layout.cmake).
 */
TEST_F(RouterBootstrapSystemDeploymentTest,
       No_mysqlrouter_conf_tmp_WhenBootstrapFailed) {
  /*
   * Create directory with the same name as mysql router's config file to force
   * bootstrap to fail.
   */
  mysql_harness::mkdir(config_file_, 0700);
  run_server_mock();

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port_),
          "--connect-timeout=1",
          "--report-host",
          "dont.query.dns",
      },
      EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(router.expect_output(
      "Error: Could not save configuration file to final location", false));

  mysql_harness::Path mysqlrouter_conf_tmp_path(tmp_dir_ +
                                                "/stage/mysqlrouter.conf.tmp");
  EXPECT_FALSE(mysqlrouter_conf_tmp_path.exists());
}

/*
 * This test is executed only for STANDALONE layout and is not executed for
 * Windows. Bootstrap for other layouts uses directories to which tests don't
 * have access (see install_layout.cmake).
 */
TEST_F(RouterBootstrapSystemDeploymentTest,
       No_mysqlrouter_key_WhenBootstrapFailed) {
  /*
   * Create directory with the same name as mysql router's config file to force
   * bootstrap to fail.
   */
  mysql_harness::mkdir(config_file_, 0700);
  run_server_mock();

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port_),
          "--connect-timeout=1",
          "--report-host",
          "dont.query.dns",
      },
      EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(router.expect_output(
      "Error: Could not save configuration file to final location", false));

  mysql_harness::Path mysqlrouter_key_path(tmp_dir_ + "/stage/mysqlrouter.key");
  EXPECT_FALSE(mysqlrouter_key_path.exists());
}

/*
 * This test is executed only for STANDALONE layout and is not executed for
 * Windows. Bootstrap for other layouts uses directories to which tests don't
 * have access (see install_layout.cmake).
 */
TEST_F(RouterBootstrapSystemDeploymentTest,
       IsKeyringRevertedWhenBootstrapFail) {
  const std::array<char, 5> kMasterKeyFileSignature = {'M', 'R', 'K', 'F',
                                                       '\0'};

  {
    std::ofstream keyring_file(
        tmp_dir_ + "/stage/mysqlrouter.key",
        std::ios_base::binary | std::ios_base::trunc | std::ios_base::out);

    mysql_harness::make_file_private(tmp_dir_ + "/stage/mysqlrouter.key");
    keyring_file.write(kMasterKeyFileSignature.data(),
                       kMasterKeyFileSignature.size());
  }

  /*
   * Create directory with the same name as mysql router's config file to force
   * bootstrap to fail.
   */
  mysql_harness::mkdir(config_file_, 0700);
  run_server_mock();

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port_),
          "--connect-timeout=1",
          "--report-host",
          "dont.query.dns",
      },
      EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(router.expect_output(
      "Error: Could not save configuration file to final location", false));

  mysql_harness::Path mysqlrouter_key_path(tmp_dir_ + "/stage/mysqlrouter.key");
  EXPECT_TRUE(mysqlrouter_key_path.exists());

  std::ifstream keyring_file(tmp_dir_ + "/stage/mysqlrouter.key",
                             std::ios_base::binary | std::ios_base::in);

  std::array<char, 5> buf;
  keyring_file.read(buf.data(), buf.size());
  EXPECT_THAT(keyring_file.gcount(), 5);
  EXPECT_EQ(buf, kMasterKeyFileSignature);
}

/*
 * This test is executed only for STANDALONE layout and is not executed for
 * Windows. Bootstrap for other layouts uses directories to which tests don't
 * have access (see install_layout.cmake).
 */
TEST_F(RouterBootstrapSystemDeploymentTest,
       Keep_mysqlrouter_log_WhenBootstrapFailed) {
  /*
   * Create directory with the same name as mysql router's config file to force
   * bootstrap to fail.
   */
  mysql_harness::mkdir(config_file_, 0700);
  run_server_mock();

  // launch the router in bootstrap mode
  auto &router = launch_router_for_bootstrap(
      {
          "--bootstrap=127.0.0.1:" + std::to_string(server_port_),
          "--connect-timeout=1",
          "--report-host",
          "dont.query.dns",
      },
      EXIT_FAILURE);

  check_exit_code(router, EXIT_FAILURE);

  EXPECT_TRUE(router.expect_output(
      "Error: Could not save configuration file to final location", false));

  mysql_harness::Path mysqlrouter_log_path(tmp_dir_ + "/stage/mysqlrouter.log");
  EXPECT_TRUE(mysqlrouter_log_path.exists());
}

#endif

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
