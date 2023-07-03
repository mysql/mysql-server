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

#include <thread>

#include "router_component_test.h"

#include "dim.h"
#include "filesystem_utils.h"
#include "mock_server_testutils.h"
#include "random_generator.h"

using namespace std::chrono_literals;
using namespace std::string_literals;

void RouterComponentTest::SetUp() {
  mysql_harness::DIM &dim = mysql_harness::DIM::instance();
  // RandomGenerator
  dim.set_RandomGenerator(
      []() {
        static mysql_harness::RandomGenerator rg;
        return &rg;
      },
      [](mysql_harness::RandomGeneratorInterface *) {});
}

void RouterComponentTest::TearDown() {
  shutdown_all();  // shutdown all that are still running.
  wait_for_exit();

  terminate_all_still_alive();  // terminate hanging processes.
  ensure_clean_exit();

  if (::testing::Test::HasFailure()) {
    dump_all();
  }
}

void RouterComponentTest::sleep_for(std::chrono::milliseconds duration) {
  if (getenv("WITH_VALGRIND")) {
    duration *= 10;
  }
  std::this_thread::sleep_for(duration);
}

bool RouterComponentTest::wait_log_contains(const ProcessWrapper &router,
                                            const std::string &pattern,
                                            std::chrono::milliseconds timeout) {
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
  }

  const auto MSEC_STEP = 50ms;
  bool found = false;
  using clock_type = std::chrono::steady_clock;
  const auto end = clock_type::now() + timeout;
  do {
    const std::string log_content = router.get_logfile_content();
    found = pattern_found(log_content, pattern);
    if (!found) {
      auto step = std::min(timeout, MSEC_STEP);
      RouterComponentTest::sleep_for(step);
    }
  } while (!found && clock_type::now() < end);

  return found;
}

std::string RouterComponentBootstrapTest::my_hostname;
constexpr const char RouterComponentBootstrapTest::kRootPassword[];

const RouterComponentBootstrapTest::OutputResponder
    RouterComponentBootstrapTest::kBootstrapOutputResponder{
        [](const std::string &line) -> std::string {
          if (line == "Please enter MySQL password for root: ")
            return kRootPassword + "\n"s;

          return "";
        }};

/**
 * the tiny power function that does all the work.
 *
 * - build environment
 * - start mock servers based on Config[]
 * - pass router_options to the launched router
 * - check the router exits as expected
 * - check output of router contains the expected lines
 */
void RouterComponentBootstrapTest::bootstrap_failover(
    const std::vector<Config> &mock_server_configs,
    const mysqlrouter::ClusterType cluster_type,
    const std::vector<std::string> &router_options, int expected_exitcode,
    const std::vector<std::string> &expected_output_regex,
    std::chrono::milliseconds wait_for_exit_timeout,
    const mysqlrouter::MetadataSchemaVersion &metadata_version,
    const std::vector<std::string> &extra_router_options) {
  std::string cluster_name("mycluster");

  std::vector<std::pair<std::string, unsigned>> gr_members;
  for (const auto &mock_server_config : mock_server_configs) {
    gr_members.emplace_back(mock_server_config.ip, mock_server_config.port);
  }

  std::vector<std::tuple<ProcessWrapper &, unsigned int>> mock_servers;

  // start the mocks
  for (const auto &mock_server_config : mock_server_configs) {
    if (mock_server_config.js_filename.empty()) continue;

    // 0x10000 & 0xffff = 0 (port 0), but we bypass
    // libmysqlclient's default-port assignment
    const auto port =
        mock_server_config.unaccessible ? 0x10000 : mock_server_config.port;
    const auto http_port = mock_server_config.http_port;
    mock_servers.emplace_back(
        launch_mysql_server_mock(mock_server_config.js_filename, port,
                                 EXIT_SUCCESS, false, http_port),
        port);
    set_mock_bootstrap_data(http_port, cluster_name, gr_members,
                            metadata_version,
                            mock_server_config.cluster_specific_id);
  }

  std::vector<std::string> router_cmdline;

  if (router_options.size()) {
    router_cmdline = router_options;
  } else {
    router_cmdline.emplace_back("--bootstrap=" + gr_members[0].first + ":" +
                                std::to_string(gr_members[0].second));

    router_cmdline.emplace_back("--report-host");
    router_cmdline.emplace_back(my_hostname);
    router_cmdline.emplace_back("--connect-timeout");
    router_cmdline.emplace_back("1");
    router_cmdline.emplace_back("-d");
    router_cmdline.emplace_back(bootstrap_dir.name());
  }

  if (getenv("WITH_VALGRIND")) {
    // for the bootstrap tests that are using this method the "--disable-rest"
    // is not relevant so we use it for VALGRIND testing as it saves huge amount
    // of time that generating the certificates takes
    router_cmdline.emplace_back("--disable-rest");
  }

  for (const auto &opt : extra_router_options) {
    router_cmdline.push_back(opt);
  }

  // launch the router
  auto &router =
      launch_router_for_bootstrap(router_cmdline, expected_exitcode, true);

  ASSERT_NO_FATAL_FAILURE(
      check_exit_code(router, expected_exitcode, wait_for_exit_timeout));

  // split the output into lines
  std::vector<std::string> lines;
  {
    std::istringstream ss{router.get_full_output()};

    for (std::string line; std::getline(ss, line);) {
      lines.emplace_back(line);
    }
  }

  for (auto const &re_str : expected_output_regex) {
    EXPECT_THAT(lines, ::testing::Contains(::testing::ContainsRegex(re_str)))
        << mock_servers;
  }

  if (EXIT_SUCCESS == expected_exitcode) {
    const std::string cluster_type_name =
        cluster_type == mysqlrouter::ClusterType::RS_V2 ? "InnoDB ReplicaSet"
                                                        : "InnoDB Cluster";
    EXPECT_THAT(lines, ::testing::Contains(
                           "# MySQL Router configured for the " +
                           cluster_type_name + " '" + cluster_name + "'"));

    config_file = bootstrap_dir.name() + "/mysqlrouter.conf";

    // check that the config files (static and dynamic) have the proper
    // access rights
    ASSERT_NO_FATAL_FAILURE(
        check_config_file_access_rights(config_file, /*read_only=*/true));
    const std::string state_file = bootstrap_dir.name() + "/data/state.json";
    ASSERT_NO_FATAL_FAILURE(
        check_config_file_access_rights(state_file, /*read_only=*/false));
  }
}

std::ostream &operator<<(
    std::ostream &os,
    const std::vector<std::tuple<ProcessWrapper &, unsigned int>> &T) {
  for (auto &t : T) {
    auto &proc = std::get<0>(t);

    os << "member@" << std::to_string(std::get<1>(t)) << ": "
       << proc.get_current_output() << std::endl;
  }
  return os;
}
