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

#include "common.h"
#include "gmock/gmock.h"
#include "router_component_system_layout.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"
#include "utils.h"

#ifndef _WIN32
#include <unistd.h>
#endif

Path g_origin_path;

/*
 * These tests are executed only for STANDALONE layout and are not executed for
 * Windows. Bootstrap for layouts different than STANDALONE use directories to
 * which tests don't have access (see install_layout.cmake).
 */
#ifndef SKIP_BOOTSTRAP_SYSTEM_DEPLOYMENT_TESTS

class RouterBootstrapSystemDeploymentTest : public RouterComponentTest,
                                            public RouterSystemLayout,
                                            public ::testing::Test {
 protected:
  void SetUp() override {
    set_origin(g_origin_path);
    RouterComponentTest::init();
    init_system_layout_dir(get_mysqlrouter_exec(), g_origin_path);

    set_mysqlrouter_exec(Path(exec_file_));
  }

  void TearDown() override { cleanup_system_layout(); }

  RouterComponentTest::CommandHandle run_server_mock() {
    const std::string json_stmts = get_data_dir().join("bootstrap.js").str();
    server_port_ = port_pool_.get_next_available();

    // launch mock server and wait for it to start accepting connections
    auto server_mock = launch_mysql_server_mock(json_stmts, server_port_);
    EXPECT_TRUE(wait_for_port_ready(server_port_, 1000))
        << "Timed out waiting for mock server port ready\n"
        << server_mock.get_full_output();
    return server_mock;
  }

  TcpPortPool port_pool_;
  uint16_t server_port_;
};

/*
 * This test is executed only for STANDALONE layout are is not executed for
 * Windows. Bootstrap for other layouts uses directories to which tests don't
 * have access (see install_layout.cmake).
 */
TEST_F(RouterBootstrapSystemDeploymentTest, BootstrapPass) {
  auto server_mock = run_server_mock();

  // launch the router in bootstrap mode
  auto router =
      launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port_) +
                    " --report-host dont.query.dns");

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  // check if the bootstraping was successful
  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 0))
      << router.get_full_output();

  EXPECT_TRUE(
      router.expect_output("MySQL Router configured for the "
                           "InnoDB cluster 'mycluster'"))
      << "router: " << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();
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
  auto server_mock = run_server_mock();

  // launch the router in bootstrap mode
  auto router =
      launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port_) +
                    " --report-host dont.query.dns");

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 1))
      << router.get_full_output();

  EXPECT_TRUE(router.expect_output(
      "Error: Could not save configuration file to final location", false))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

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
  auto server_mock = run_server_mock();

  // launch the router in bootstrap mode
  auto router =
      launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port_) +
                    " --report-host dont.query.dns");

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 1))
      << router.get_full_output();

  EXPECT_TRUE(router.expect_output(
      "Error: Could not save configuration file to final location", false))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

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
  static const char kMasterKeyFileSignature[] = "MRKF";

  {
    std::ofstream keyring_file(
        tmp_dir_ + "/stage/mysqlrouter.key",
        std::ios_base::binary | std::ios_base::trunc | std::ios_base::out);

    mysql_harness::make_file_private(tmp_dir_ + "/stage/mysqlrouter.key");
    keyring_file.write(kMasterKeyFileSignature,
                       strlen(kMasterKeyFileSignature));
  }

  /*
   * Create directory with the same name as mysql router's config file to force
   * bootstrap to fail.
   */
  mysql_harness::mkdir(config_file_, 0700);
  auto server_mock = run_server_mock();

  // launch the router in bootstrap mode
  auto router =
      launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port_) +
                    " --report-host dont.query.dns");

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 1))
      << router.get_full_output();

  EXPECT_TRUE(router.expect_output(
      "Error: Could not save configuration file to final location", false))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

  mysql_harness::Path mysqlrouter_key_path(tmp_dir_ + "/stage/mysqlrouter.key");
  EXPECT_TRUE(mysqlrouter_key_path.exists());

  std::ifstream keyring_file(tmp_dir_ + "/stage/mysqlrouter.key",
                             std::ios_base::binary | std::ios_base::in);

  char buf[10] = {0};
  keyring_file.read(buf, sizeof(buf));
  EXPECT_THAT(keyring_file.gcount(), 4);
  EXPECT_THAT(std::strncmp(buf, kMasterKeyFileSignature, 4), testing::Eq(0));
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
  auto server_mock = run_server_mock();

  // launch the router in bootstrap mode
  auto router =
      launch_router("--bootstrap=127.0.0.1:" + std::to_string(server_port_) +
                    " --report-host dont.query.dns");

  // add login hook
  router.register_response("Please enter MySQL password for root: ",
                           "fake-pass\n");

  EXPECT_NO_THROW(EXPECT_EQ(router.wait_for_exit(), 1))
      << router.get_full_output();

  EXPECT_TRUE(router.expect_output(
      "Error: Could not save configuration file to final location", false))
      << router.get_full_output() << std::endl
      << "server: " << server_mock.get_full_output();

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
