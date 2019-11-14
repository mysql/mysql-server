/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "mysqlrouter/utils.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

class RouterPidfileTest : public RouterComponentTest {};

#define MY_WAIT_US(cond, usec)          \
  {                                     \
    int n = 0;                          \
    int max = usec / 1000;              \
    do {                                \
      std::this_thread::sleep_for(1ms); \
      n++;                              \
    } while ((n < max) && (cond));      \
    ASSERT_FALSE((cond));               \
  }

// Bug #29441087 ROUTER SHOULD REMOVE PIDFILE ON CLEAN EXIT
TEST_F(RouterPidfileTest, PidFileRemovedAtExit) {
  // create tmp dir where we will log
  TempDirectory logging_folder;

  // create config with logging_folder set to that directory
  std::map<std::string, std::string> params = get_DEFAULT_defaults();
  params.at("logging_folder") = logging_folder.name();

  // load keepalive with 10sec duration
  TempDirectory conf_dir("conf");
  const std::string conf_file = create_config_file(
      conf_dir.name(), "[keepalive]\ninterval = 10\n", &params);

  // Use the temporary ROUTER_PID envvar to set pidfile
  mysql_harness::Path pidfile =
      mysql_harness::Path(conf_dir.name()).join("mysqlrouter.pid");
  int err_code;
#ifdef _WIN32
  err_code = _putenv_s("ROUTER_PID", pidfile.c_str());
#else
  err_code = ::setenv("ROUTER_PID", pidfile.c_str(), 1);
#endif
  if (err_code) throw std::runtime_error("Failed to add ROUTER_PID");

  // start router
  std::vector<std::string> router_cmdline{"-c", conf_file};
  auto &router = ProcessManager::launch_router(router_cmdline);

  // wait for logfile to appear, otherwise framework throws exceptions
  mysql_harness::Path logfile =
      mysql_harness::Path(logging_folder.name()).join("mysqlrouter.log");
  MY_WAIT_US(!mysql_harness::Path(logfile).exists(), 200 * 1000);

  // wait for pidfile to appear
  MY_WAIT_US(!mysql_harness::Path(pidfile).exists(), 200 * 1000);

  // check pidfile exists
  ASSERT_TRUE(mysql_harness::Path(pidfile).exists());

  // verify clean shutdown exitcode
  EXPECT_FALSE(router.send_clean_shutdown_event());
  check_exit_code(router, EXIT_SUCCESS, 3s);

  // check pidfile removed
  ASSERT_FALSE(mysql_harness::Path(pidfile).exists());
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
