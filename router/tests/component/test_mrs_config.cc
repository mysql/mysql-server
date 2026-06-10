/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysqlrouter/mysql_session.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_test_helpers.h"
#include "tcp_port_pool.h"

using namespace std::chrono_literals;

class RouterMrsConfig : public RouterComponentTest {
 protected:
  void SetUp() override {
    for (int i = 0; i < kClusterSize; i++) {
      cluster_nodes_ports.push_back(port_pool_.get_next_available());
      cluster_nodes_http_ports.push_back(port_pool_.get_next_available());
    }
    RouterComponentTest::SetUp();
  }

  auto &launch_router(const std::string &config, const int expected_exit_code,
                      bool enable_logger_debug = true) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(
        default_section, get_test_temp_dir_name(),
        {KeyringEntry{user_, "password", "mysql_test_password"},
         KeyringEntry{"rest-user", "jwt_secret", "mysql_test_password"}});

    const auto state_file = create_state_file(
        get_test_temp_dir_name(),
        create_state_file_content("", "", cluster_nodes_ports, 0));
    default_section["dynamic_state"] = state_file;

    const std::string conf_file =
        create_config_file(get_test_temp_dir_name(), config, &default_section,
                           "mysqlrouter.conf", "", enable_logger_debug);

    const auto wait_for = expected_exit_code == EXIT_FAILURE ? -1s : 5s;
    return ProcessManager::launch_router({"-c", conf_file}, expected_exit_code,
                                         true, false, wait_for);
  }

  std::string get_routing_section(const uint16_t port, const std::string &role,
                                  const std::string &name) {
    std::map<std::string, std::string> options{
        {"bind_port", std::to_string(port)},
        {"destinations", "metadata-cache://test/default?role=" + role},
        {"protocol", "classic"}};

    return mysql_harness::ConfigBuilder::build_section("routing:" + name,
                                                       options);
  }

  std::string get_mrs_section(
      const std::optional<std::string> &mysql_read_only_route,
      const std::optional<std::string> &mysql_read_write_route,
      const std::optional<std::string> &mysql_user,
      const std::optional<std::string> &mysql_user_data_access,
      const std::optional<std::uint64_t> &router_id) const {
    std::vector<std::pair<std::string, std::string>> options{};

    if (mysql_read_only_route) {
      options.emplace_back("mysql_read_only_route",
                           mysql_read_only_route.value());
    }
    if (mysql_read_write_route) {
      options.emplace_back("mysql_read_write_route",
                           mysql_read_write_route.value());
    }
    if (mysql_user) {
      options.emplace_back("mysql_user", mysql_user.value());
    }
    if (mysql_user_data_access) {
      options.emplace_back("mysql_user_data_access",
                           mysql_user_data_access.value());
    }
    if (router_id) {
      options.emplace_back("router_id", std::to_string(router_id.value()));
    }

    return mysql_harness::ConfigBuilder::build_section("mysql_rest_service",
                                                       options);
  }

  std::string get_metadata_cache_section(
      mysqlrouter::ClusterType cluster_type = mysqlrouter::ClusterType::GR_V2) {
    const std::string cluster_type_str =
        (cluster_type == mysqlrouter::ClusterType::RS_V2) ? "rs" : "gr";

    std::map<std::string, std::string> options{
        {"cluster_type", cluster_type_str},
        {"router_id", std::to_string(router_id_)},
        {"user", user_},
        {"connect_timeout", "1"},
        {"metadata_cluster", cluster_name_},
        {"ttl", "0.1"}};

    return mysql_harness::ConfigBuilder::build_section(
        "metadata_cache:bootstrap", options);
  }

  void setup_cluster(const std::string &mock_file,
                     const uint64_t mrs_router_id = 1) {
    const auto http_port = cluster_nodes_http_ports[0];
    auto mock_server_cmdline_args = mock_server_cmdline(mock_file)
                                        .port(cluster_nodes_ports[0])
                                        .http_port(http_port)
                                        .args();

    auto &primary_node = mock_server_spawner().spawn(mock_server_cmdline_args);

    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(primary_node, cluster_nodes_ports[0]));

    EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
    auto json_doc = mock_GR_metadata_as_json(
        "", classic_ports_to_gr_nodes(cluster_nodes_ports), 0,
        classic_ports_to_cluster_nodes(cluster_nodes_ports), /*view_id*/ 0,
        /*error_on_md_quert*/ false, "127.0.0.1", /*router_options*/ "",
        mysqlrouter::MetadataSchemaVersion{(2), (3), (0)}, cluster_name_);

    JsonAllocator allocator;
    json_doc.AddMember("mrs_router_id", mrs_router_id, allocator);

    const auto json_str = json_to_string(json_doc);

    ASSERT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));

    // launch the secondary cluster nodes
    for (unsigned port = 1; port < cluster_nodes_ports.size(); ++port) {
      auto &secondary_node = mock_server_spawner().spawn(
          mock_server_cmdline("my_port.js")
              .port(cluster_nodes_ports[port])
              .http_port(cluster_nodes_http_ports[port])
              .args());
      ASSERT_NO_FATAL_FAILURE(
          check_port_ready(secondary_node, cluster_nodes_ports[port]));
    }
  }

  const uint16_t kClusterSize = 6;
  const uint16_t router_port_rw{port_pool_.get_next_available()};

  uint64_t router_id_{1};
  std::vector<uint16_t> cluster_nodes_ports;
  std::vector<uint16_t> cluster_nodes_http_ports;
  const std::string user_{"mysql_test_user"};
  const std::string cluster_name_{"clusterA"};
};

/**
 * @test Checks that the Router refuses to start if
 * [mysql_rest_service].router_id is not configured
 */
TEST_F(RouterMrsConfig, RouterComponentBootstrapWithDefaultCertsTest) {
  const std::string mrs_section =
      get_mrs_section("bootstrap_ro", "bootstrap_rw",
                      "mysql_router_mrs1_ie9u74n75rsb", "", std::nullopt);

  auto &router = launch_router(mrs_section,
                               /*expect_error=*/true);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_TRUE(wait_log_contains(router,
                                "Configuration error: option router_id in "
                                "\\[mysql_rest_service\\] is required",
                                500ms));
}

/**
 * @test Check that if Router detects that there is the same Router registered
 * but with different router_id it fails to start.
 */
TEST_F(RouterMrsConfig, id_mismatch) {
  router_id_ = 1;
  // Use different id
  setup_cluster("metadata_dynamic_nodes_v2_gr_mrs.js",
                /*mrs_router_id*/ router_id_ + 1);

  const auto rw_route_name = "rw_route";
  const auto mrs_section = get_mrs_section(std::nullopt, rw_route_name, user_,
                                           std::nullopt, router_id_);
  auto &router = launch_router(
      get_routing_section(router_port_rw, "PRIMARY", rw_route_name) +
          get_metadata_cache_section() + mrs_section,
      EXIT_FAILURE);

  EXPECT_TRUE(wait_log_contains(router,
                                "Metadata already contains Router registered "
                                "as '.*' at '.*' with id: \\d+, new id: \\d+",
                                5s));
}

/**
 * @test Checks that the Router refuses to start if [logger].sink
 * 'mysql_rest_service' is configred but [mysql_rest_service] is not
 */
TEST_F(RouterMrsConfig, mrs_md_sink_with_no_mrs) {
  const std::string logger_section = "[logger]\nsinks=mysql_rest_service\n\n";
  auto &router = launch_router(
      logger_section + get_routing_section(router_port_rw, "PRIMARY", "rw"),
      /*expect_error=*/true, /*enable_logger_debug=*/false);

  check_exit_code(router, EXIT_FAILURE);
  EXPECT_THAT(router.get_full_output(),
              ::testing::HasSubstr(
                  "External logging sink 'mysql_rest_service' configured, but "
                  "section with that name missing in the config file."));
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
