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

#include "gmock/gmock.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

Path g_origin_path;
using testing::StartsWith;

class RouterConfigTest : public RouterComponentTest, public ::testing::Test {
 protected:
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::init();
  }

  TcpPortPool port_pool_;
};

// Bug #25800863 WRONG ERRORMSG IF DIRECTORY IS PROVIDED AS CONFIGFILE
TEST_F(RouterConfigTest, RoutingDirAsMainConfigDirectory) {
  const std::string config_dir = get_tmp_dir();

  // launch the router giving directory instead of config_name
  auto router = launch_router("-c " + config_dir);

  EXPECT_EQ(router.wait_for_exit(), 1);

  EXPECT_TRUE(router.expect_output(
      "Expected configuration file, got directory name: " + config_dir))
      << "router output: " << router.get_full_output() << std::endl;
}

// Bug #25800863 WRONG ERRORMSG IF DIRECTORY IS PROVIDED AS CONFIGFILE
TEST_F(RouterConfigTest, RoutingDirAsExtendedConfigDirectory) {
  const auto router_port = port_pool_.get_next_available();
  const auto server_port = port_pool_.get_next_available();

  const std::string routing_section =
      "[routing:basic]\n"
      "bind_port = " +
      std::to_string(router_port) +
      "\n"
      "mode = read-write\n"
      "destinations = 127.0.0.1:" +
      std::to_string(server_port) + "\n";

  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard1(nullptr,
                                    [&](void *) { purge_dir(conf_dir); });
  const std::string extra_conf_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard2(nullptr,
                                    [&](void *) { purge_dir(extra_conf_dir); });

  std::string conf_file = create_config_file(conf_dir, routing_section);

  // launch the router giving directory instead of an extra config name
  auto router = launch_router("-c " + conf_file + " -a " + extra_conf_dir);

  EXPECT_EQ(router.wait_for_exit(), 1);

  EXPECT_TRUE(router.expect_output(
      "Expected configuration file, got directory name: " + extra_conf_dir))
      << "router output: " << router.get_full_output() << std::endl;
}

TEST_F(RouterConfigTest,
       IsExceptionThrownWhenAddTwiceTheSameSectionWithoutKey) {
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  const std::string conf_file =
      create_config_file(conf_dir, "[section1]\n[section1]\n");

  // run the router and wait for it to exit
  auto router = launch_router("-c " + conf_file);
  EXPECT_EQ(router.wait_for_exit(), 1);

  EXPECT_THAT(
      router.get_full_output(),
      StartsWith(
          "Error: Configuration error: Section 'section1' already exists"));
}

TEST_F(RouterConfigTest, IsExceptionThrownWhenAddTwiceTheSameSectionWithKey) {
  const std::string conf_dir = get_tmp_dir("conf");
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(conf_dir); });
  const std::string conf_file =
      create_config_file(conf_dir, "[section1:key1]\n[section1:key1]\n");

  // run the router and wait for it to exit
  auto router = launch_router("-c " + conf_file);
  EXPECT_EQ(router.wait_for_exit(), 1);

  EXPECT_THAT(router.get_full_output(),
              StartsWith("Error: Configuration error: Section 'section1:key1' "
                         "already exists"));
}

#ifdef _WIN32
static bool isRouterServiceInstalled() {
  SC_HANDLE service, scm;
  bool result = false;

  if ((scm = OpenSCManager(0, 0, SC_MANAGER_ENUMERATE_SERVICE))) {
    if ((service = OpenService(scm, "MySQLRouter", SERVICE_QUERY_STATUS))) {
      CloseServiceHandle(service);
      result = true;
    }
    CloseServiceHandle(scm);
  }
  return result;
}

/**
 * ensure that the router exits with proper error when launched with --service
 * and the service is not installed
 */
TEST_F(RouterConfigTest, IsErrorReturnedWhenServiceDoesNotExist) {
  // first we need to make sure the service really is not installed on the
  // system that the test is running on. If it is we can't do much about it and
  // we just skip testing.
  if (!isRouterServiceInstalled()) {
    const std::string conf_dir = get_tmp_dir("conf");
    std::shared_ptr<void> exit_guard(nullptr,
                                     [&](void *) { purge_dir(conf_dir); });
    const std::string conf_file =
        create_config_file(conf_dir, "[keepalive]\ninterval = 60\n");

    // run the router and wait for it to exit
    auto router = launch_router("-c " + conf_file + " --service");
    EXPECT_EQ(router.wait_for_exit(), 1);

    EXPECT_THAT(router.get_full_output(),
                StartsWith("Could not find service 'MySQLRouter'!\n"
                           "Use --install-service or --install-service-manual "
                           "option to install the service first.\n"));
  }
}
#endif  // _WIN32

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
