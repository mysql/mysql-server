/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysqlrouter/mysql_session.h"
#include "rest_metadata_client.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_test_helpers.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;

class DestinationStatusTest : public RouterComponentTest {
 protected:
  std::string get_destination_status_section(
      const std::string &quarantine_interval,
      const std::string &quarantine_threshold) {
    return mysql_harness::ConfigBuilder::build_section(
        "destination_status",
        {
            {"error_quarantine_interval", quarantine_interval},
            {"error_quarantine_threshold", quarantine_threshold},
        });
  }

  auto &launch_router(const std::string &sections, const int expected_exitcode,
                      std::chrono::milliseconds wait_for_notify_ready = 30s) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(default_section, get_test_temp_dir_name());

    const std::string conf_file = create_config_file(
        get_test_temp_dir_name(), sections, &default_section);

    auto &router =
        ProcessManager::launch_router({"-c", conf_file}, expected_exitcode,
                                      true, false, wait_for_notify_ready);

    return router;
  }
};

class QuarantineThresholdInvalidValueTest
    : public DestinationStatusTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(QuarantineThresholdInvalidValueTest, InvalidValues) {
  const std::string kCorrectInterval{"1"};
  const std::string destination_status_section =
      get_destination_status_section(kCorrectInterval, GetParam());
  auto &router = launch_router(destination_status_section, EXIT_FAILURE, -1s);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(wait_log_contains(
      router,
      "Configuration error: option error_quarantine_threshold in "
      "\\[destination_status\\] needs value between 1 and 65535 inclusive, "
      "was '" +
          GetParam() + "'",
      500ms));
}

INSTANTIATE_TEST_SUITE_P(InvalidValues, QuarantineThresholdInvalidValueTest,
                         ::testing::Values("''", "0", "1.2", "-1", "65536",
                                           "foo"));

class QuarantineIntervalInvalidValueTest
    : public DestinationStatusTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(QuarantineIntervalInvalidValueTest, InvalidValues) {
  const std::string kCorrectThreshold{"1"};
  const std::string destination_status_section =
      get_destination_status_section(GetParam(), kCorrectThreshold);
  auto &router = launch_router(destination_status_section, EXIT_FAILURE, -1s);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(wait_log_contains(
      router,
      "Configuration error: option error_quarantine_interval in "
      "\\[destination_status\\] needs value between 1 and 3600 inclusive, "
      "was '" +
          GetParam() + "'",
      500ms));
}

INSTANTIATE_TEST_SUITE_P(InvalidValues, QuarantineIntervalInvalidValueTest,
                         ::testing::Values("''", "0", "1.2", "-1", "3601",
                                           "foo"));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
