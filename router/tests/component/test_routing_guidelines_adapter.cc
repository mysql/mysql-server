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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

#include "config_builder.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/utility/string.h"  // string_format
#include "random_generator.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"

#include "mysqlxclient.h"

using namespace std::chrono_literals;
using namespace std::string_literals;

namespace mysqlrouter {
std::ostream &operator<<(std::ostream &os, const MysqlError &e) {
  return os << e.sql_state() << " code: " << e.value() << ": " << e.message();
}
}  // namespace mysqlrouter

class RoutingGuidelinesAdapterTest : public RouterComponentTest {
 public:
  RoutingGuidelinesAdapterTest() {}

 protected:
  auto &launch_router(const std::vector<uint16_t> &metadata_server_ports,
                      const std::string &routing_section,
                      const std::string &metadata_cache_section,
                      const unsigned exit_code = EXIT_SUCCESS) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(default_section, get_test_temp_dir_name(),
                 {KeyringEntry{user_, "password", "mysql_test_password"}});

    const auto state_file = create_state_file(
        get_test_temp_dir_name(),
        create_state_file_content("", "", metadata_server_ports, 0));
    default_section["dynamic_state"] = state_file;

    const std::string conf_file = create_config_file(
        get_test_temp_dir_name(), metadata_cache_section + routing_section,
        &default_section);

    const auto wait_for_notify_ready = exit_code == EXIT_SUCCESS ? 5s : -1s;
    return ProcessManager::launch_router({"-c", conf_file}, exit_code, true,
                                         false, wait_for_notify_ready);
  }

  std::string get_routing_section(const uint16_t port, const std::string &role,
                                  const std::string &protocol = "classic") {
    std::map<std::string, std::string> options{
        {"bind_port", std::to_string(port)},
        {"bind_address", "127.0.0.1"},
        {"destinations", "metadata-cache://test/default?role=" + role},
        {"protocol", protocol}};

    return mysql_harness::ConfigBuilder::build_section(
        "routing:" + role + "_" + protocol, options);
  }

  std::string get_metadata_cache_section(
      mysqlrouter::ClusterType cluster_type = mysqlrouter::ClusterType::GR_V2) {
    const std::string cluster_type_str =
        (cluster_type == mysqlrouter::ClusterType::RS_V2) ? "rs" : "gr";

    std::map<std::string, std::string> options{
        {"cluster_type", cluster_type_str},
        {"router_id", "1"},
        {"user", user_},
        {"connect_timeout", "1"},
        {"metadata_cluster", "test"},
        {"ttl", "0.1"}};

    return mysql_harness::ConfigBuilder::build_section(
        "metadata_cache:bootstrap", options);
  }

  void check_log_contains(const ProcessWrapper &process,
                          const std::string &needle) const {
    EXPECT_THAT(process.get_logfile_content(), ::testing::HasSubstr(needle));
  }

  void check_log_contains_regex(const ProcessWrapper &process,
                                const std::string &needle) const {
    EXPECT_THAT(process.get_logfile_content(),
                ::testing::ContainsRegex(needle));
  }

  const uint16_t router_port_rw{port_pool_.get_next_available()};
  const uint16_t router_port_ro{port_pool_.get_next_available()};
  const std::string user_{"mysql_test_user"};
  const std::string cluster_id = "3a0be5af-0022-11e8-9655-0800279e6a88";
};

TEST_F(RoutingGuidelinesAdapterTest, BootstrapGeneratedConfig) {
  const std::vector<uint16_t> cluster_nodes_ports{
      port_pool_.get_next_available(),  // first is PRIMARY
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};
  const std::vector<uint16_t> cluster_nodes_http_ports{
      port_pool_.get_next_available(),  // first is PRIMARY
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};

  std::vector<ProcessWrapper *> cluster_nodes;

  // launch the primary node working also as metadata server
  const auto http_port = cluster_nodes_http_ports[0];
  auto mock_server_cmdline_args = mock_server_cmdline("bootstrap_and_run_gr.js")
                                      .port(cluster_nodes_ports[0])
                                      .http_port(http_port)
                                      .args();

  auto &primary_node = mock_server_spawner().spawn(mock_server_cmdline_args);

  ASSERT_NO_FATAL_FAILURE(
      check_port_ready(primary_node, cluster_nodes_ports[0]));

  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
  set_mock_metadata(http_port, cluster_id,
                    classic_ports_to_gr_nodes(cluster_nodes_ports), 0,
                    classic_ports_to_cluster_nodes(cluster_nodes_ports));
  cluster_nodes.emplace_back(&primary_node);

  // launch the secondary cluster nodes
  for (auto i = 1u; i < cluster_nodes_ports.size(); ++i) {
    auto &secondary_node = mock_server_spawner().spawn(
        mock_server_cmdline("metadata_dynamic_nodes_v2_gr.js")
            .port(cluster_nodes_ports[i])
            .http_port(cluster_nodes_http_ports[i])
            .args());
    cluster_nodes.emplace_back(&secondary_node);
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(secondary_node, cluster_nodes_ports[i]));
    EXPECT_TRUE(MockServerRestClient(cluster_nodes_http_ports[i])
                    .wait_for_rest_endpoint_ready());

    set_mock_metadata(cluster_nodes_http_ports[i], cluster_id,
                      classic_ports_to_gr_nodes(cluster_nodes_ports), i,
                      classic_ports_to_cluster_nodes(cluster_nodes_ports));
  }
  cluster_nodes.emplace_back(&primary_node);

  TempDirectory temp_test_dir;
  const uint16_t router_port_rw = port_pool_.get_next_available();
  const uint16_t router_port_ro = port_pool_.get_next_available();
  const uint16_t router_port_rw_split = port_pool_.get_next_available();
  const uint16_t router_port_x_rw = port_pool_.get_next_available();
  const uint16_t router_port_x_ro = port_pool_.get_next_available();

  std::vector<std::string> bootstrap_params{
      "--bootstrap=127.0.0.1:" + std::to_string(cluster_nodes_ports[0]),
      "-d",
      temp_test_dir.name(),
      "--conf-set-option=routing:bootstrap_rw.bind_port=" +
          std::to_string(router_port_rw),
      "--conf-set-option=routing:bootstrap_ro.bind_port=" +
          std::to_string(router_port_ro),
      "--conf-set-option=routing:bootstrap_rw_split.bind_port=" +
          std::to_string(router_port_rw_split),
      "--conf-set-option=routing:bootstrap_x_rw.bind_port=" +
          std::to_string(router_port_x_rw),
      "--conf-set-option=routing:bootstrap_x_ro.bind_port=" +
          std::to_string(router_port_x_ro),
      "--conf-set-option=routing:bootstrap_rw.bind_address=127.0.0.1",
      "--conf-set-option=routing:bootstrap_ro.bind_address=127.0.0.1",
      "--conf-set-option=routing:bootstrap_rw_split.bind_address=127.0.0.1",
      "--conf-set-option=routing:bootstrap_x_rw.bind_address=127.0.0.1",
      "--conf-set-option=routing:bootstrap_x_ro.bind_address=127.0.0.1",
      "--conf-set-option=http_server.bind_address=127.0.0.1",
      "--conf-set-option=logger.level=DEBUG",
      "--conf-set-option=DEFAULT.logging_folder=" + get_logging_dir().str(),
      "--conf-set-option=DEFAULT.plugin_folder=" +
          ProcessManager::get_plugin_dir().str()};

  auto &router_bootstrap = ProcessManager::launch_router(
      bootstrap_params, EXIT_SUCCESS, true, false, -1s,
      RouterComponentBootstrapTest::kBootstrapOutputResponder);
  check_exit_code(router_bootstrap, EXIT_SUCCESS);

  EXPECT_TRUE(router_bootstrap.expect_output(
      "MySQL Router configured for the InnoDB Cluster 'test'"));

  const auto config_path = temp_test_dir.file("mysqlrouter.conf");
  auto &router =
      ProcessManager::launch_router({"-c", config_path}, EXIT_SUCCESS);

  // Destinations matches
  check_log_contains(router,
                     R"("name": "bootstrap_ro",
            "match": "$.server.memberRole = SECONDARY OR $.server.memberRole = READ_REPLICA")");
  check_log_contains(router,
                     R"("name": "bootstrap_rw",
            "match": "$.server.memberRole = PRIMARY")");
  check_log_contains(router,
                     R"("name": "bootstrap_x_ro",
            "match": "$.server.memberRole = SECONDARY OR $.server.memberRole = READ_REPLICA")");
  check_log_contains(router,
                     R"("name": "bootstrap_x_rw",
            "match": "$.server.memberRole = PRIMARY")");
  check_log_contains(router,
                     R"("name": "bootstrap_rw_split",
            "match": "$.server.memberRole = PRIMARY OR $.server.memberRole = SECONDARY OR $.server.memberRole = READ_REPLICA")");

  // Route entries
  check_log_contains(
      router, mysql_harness::utility::string_format(R"_("name": "bootstrap_ro",
            "match": "$.session.targetIP IN ('127.0.0.1') AND $.session.targetPort IN (%d)",
            "destinations": [
                {
                    "strategy": "round-robin",
                    "classes": [
                        "bootstrap_ro"
                    ],
                    "priority": 0
                },
                {
                    "strategy": "round-robin",
                    "classes": [
                        "bootstrap_rw"
                    ],
                    "priority": 1
                }
            ])_",
                                                    router_port_ro));
  check_log_contains(
      router, mysql_harness::utility::string_format(R"_("name": "bootstrap_rw",
            "match": "$.session.targetIP IN ('127.0.0.1') AND $.session.targetPort IN (%d)",
            "destinations": [
                {
                    "strategy": "first-available",
                    "classes": [
                        "bootstrap_rw"
                    ],
                    "priority": 0
                }
            ])_",
                                                    router_port_rw));
  check_log_contains(router, mysql_harness::utility::string_format(
                                 R"_("name": "bootstrap_x_ro",
            "match": "$.session.targetIP IN ('127.0.0.1') AND $.session.targetPort IN (%d)",
            "destinations": [
                {
                    "strategy": "round-robin",
                    "classes": [
                        "bootstrap_x_ro"
                    ],
                    "priority": 0
                },
                {
                    "strategy": "round-robin",
                    "classes": [
                        "bootstrap_x_rw"
                    ],
                    "priority": 1
                }
            ])_",
                                 router_port_x_ro));
  check_log_contains(router, mysql_harness::utility::string_format(
                                 R"_("name": "bootstrap_x_rw",
            "match": "$.session.targetIP IN ('127.0.0.1') AND $.session.targetPort IN (%d)",
            "destinations": [
                {
                    "strategy": "first-available",
                    "classes": [
                        "bootstrap_x_rw"
                    ],
                    "priority": 0
                }
            ])_",
                                 router_port_x_rw));
  check_log_contains(router, mysql_harness::utility::string_format(
                                 R"_("name": "bootstrap_rw_split",
            "match": "$.session.targetIP IN ('127.0.0.1') AND $.session.targetPort IN (%d)",
            "destinations": [
                {
                    "strategy": "round-robin",
                    "classes": [
                        "bootstrap_rw_split"
                    ],
                    "priority": 0
                }
            ])_",
                                 router_port_rw_split));
}

TEST_F(RoutingGuidelinesAdapterTest, CustomBindPort) {
  const auto router_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> options{
      {"bind_port", std::to_string(router_port)},
      {"destinations", "metadata-cache://test/default?role=PRIMARY"},
      {"protocol", "classic"}};
  const auto &routing_section =
      mysql_harness::ConfigBuilder::build_section("routing:test", options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  const std::string js_file =
      Path(get_data_dir()).join("metadata_1_node_repeat_v2_gr.js").str();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router =
      launch_router({node_port}, routing_section, get_metadata_cache_section());
  check_log_contains(
      router, mysql_harness::utility::string_format(
                  R"_("match": "$.session.targetPort IN (%d)")_", router_port));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port);
    ASSERT_NO_ERROR(client_res);
  }
}

TEST_F(RoutingGuidelinesAdapterTest, BindAddressHostname) {
  const auto router_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> options{
      {"bind_address", "localhost"},
      {"bind_port", std::to_string(router_port)},
      {"destinations", "metadata-cache://test/default?role=PRIMARY"},
      {"protocol", "classic"}};
  const auto &routing_section =
      mysql_harness::ConfigBuilder::build_section("routing:test", options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router =
      launch_router({node_port}, routing_section, get_metadata_cache_section());
  check_log_contains_regex(
      router, mysql_harness::utility::string_format(
                  "\"match\": \"\\$.session.targetIP IN \\('.*', '.*'\\) AND "
                  "\\$.session.targetPort IN \\(%d\\)\"",
                  router_port));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port, "localhost");
    ASSERT_NO_ERROR(client_res);
  }
}

TEST_F(RoutingGuidelinesAdapterTest, BindAddressInvalidHostname) {
  const auto &hostname =
      mysql_harness::RandomGenerator().generate_identifier(30);
  const auto router_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> options{
      {"bind_address", hostname},
      {"bind_port", std::to_string(router_port)},
      {"destinations", "metadata-cache://test/default?role=PRIMARY"},
      {"protocol", "classic"}};
  const auto &routing_section =
      mysql_harness::ConfigBuilder::build_section("routing:test", options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router = launch_router({node_port}, routing_section,
                               get_metadata_cache_section(), EXIT_FAILURE);
  check_exit_code(router, EXIT_FAILURE);
}

TEST_F(RoutingGuidelinesAdapterTest, BindAddressIP) {
  const auto router_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> options{
      {"bind_address", "127.0.0.1"},
      {"bind_port", std::to_string(router_port)},
      {"destinations", "metadata-cache://test/default?role=PRIMARY"},
      {"protocol", "classic"}};
  const auto &routing_section =
      mysql_harness::ConfigBuilder::build_section("routing:test", options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router =
      launch_router({node_port}, routing_section, get_metadata_cache_section());
  check_log_contains(
      router,
      mysql_harness::utility::string_format(
          R"_("match": "$.session.targetIP IN ('127.0.0.1') AND $.session.targetPort IN (%d)")_",
          router_port));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port, "127.0.0.1");
    ASSERT_NO_ERROR(client_res);
  }
}

TEST_F(RoutingGuidelinesAdapterTest, PrimaryMode) {
  const auto router_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> options{
      {"bind_address", "127.0.0.1"},
      {"bind_port", std::to_string(router_port)},
      {"destinations", "metadata-cache://test/default?role=PRIMARY"},
      {"protocol", "classic"}};
  const auto &routing_section =
      mysql_harness::ConfigBuilder::build_section("routing:test", options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  const std::string js_file =
      Path(get_data_dir()).join("metadata_1_node_repeat_v2_gr.js").str();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router =
      launch_router({node_port}, routing_section, get_metadata_cache_section());
  check_log_contains(router, R"_("match": "$.server.memberRole = PRIMARY")_");
  check_log_contains(router, R"_("destinations": [
                {
                    "strategy": "first-available",
                    "classes": [
                        "test"
                    ],
                    "priority": 0
                }
            ])_");

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port);
    ASSERT_NO_ERROR(client_res);
  }
}

TEST_F(RoutingGuidelinesAdapterTest, SecondaryModeNoFallback) {
  const auto router_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> options{
      {"bind_address", "127.0.0.1"},
      {"bind_port", std::to_string(router_port)},
      {"destinations", "metadata-cache://test/default?role=SECONDARY"},
      {"protocol", "classic"}};
  const auto &routing_section =
      mysql_harness::ConfigBuilder::build_section("routing:ro", options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router =
      launch_router({node_port}, routing_section, get_metadata_cache_section());
  check_log_contains(router,
                     "\"match\": \"$.server.memberRole = SECONDARY OR "
                     "$.server.memberRole = READ_REPLICA\"");
  check_log_contains(router, R"_("destinations": [
                {
                    "strategy": "round-robin",
                    "classes": [
                        "ro"
                    ],
                    "priority": 0
                }
            ])_");

  SCOPED_TRACE("There is no fallback, connection should fail");
  ASSERT_FALSE(make_new_connection(router_port));
}

class RoutingGuidelinesAdapterTestFallback
    : public RoutingGuidelinesAdapterTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(RoutingGuidelinesAdapterTestFallback, SecondaryModeWithFallback) {
  const auto router_ro_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> ro_options{
      {"bind_address", "127.0.0.1"},
      {"bind_port", std::to_string(router_ro_port)},
      {"destinations", "metadata-cache://test/default?role=SECONDARY"},
      {"protocol", "classic"}};
  const auto &ro_routing_section = mysql_harness::ConfigBuilder::build_section(
      "routing:test_ro", ro_options);

  bool is_explicit = GetParam();
  if (is_explicit) ro_options.push_back({"strategy", "round_robin"});

  const auto router_rw_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> rw_options{
      {"bind_address", "127.0.0.1"},
      {"bind_port", std::to_string(router_rw_port)},
      {"destinations", "metadata-cache://test/default?role=PRIMARY"},
      {"protocol", "classic"}};
  const auto &rw_routing_section = mysql_harness::ConfigBuilder::build_section(
      "routing:test_rw", rw_options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router =
      launch_router({node_port}, ro_routing_section + rw_routing_section,
                    get_metadata_cache_section());
  check_log_contains(router,
                     "\"match\": \"$.server.memberRole = SECONDARY OR "
                     "$.server.memberRole = READ_REPLICA\"");
  check_log_contains(router, R"_("destinations": [
                {
                    "strategy": "round-robin",
                    "classes": [
                        "test_ro"
                    ],
                    "priority": 0
                },
                {
                    "strategy": "round-robin",
                    "classes": [
                        "test_rw"
                    ],
                    "priority": 1
                }
            ])_");

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_rw_port);
    ASSERT_NO_ERROR(client_res);
  }
}

INSTANTIATE_TEST_SUITE_P(SecondaryModeWithFallback,
                         RoutingGuidelinesAdapterTestFallback,
                         ::testing::Values(true, false));

TEST_F(RoutingGuidelinesAdapterTest, PrimaryAndSecondaryMode) {
  const auto router_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> options{
      {"bind_address", "127.0.0.1"},
      {"bind_port", std::to_string(router_port)},
      {"destinations",
       "metadata-cache://test/default?role=PRIMARY_AND_SECONDARY"},
      {"protocol", "classic"}};
  const auto &routing_section =
      mysql_harness::ConfigBuilder::build_section("routing:rorw", options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router =
      launch_router({node_port}, routing_section, get_metadata_cache_section());
  check_log_contains(router,
                     "\"match\": \"$.server.memberRole = PRIMARY OR "
                     "$.server.memberRole = SECONDARY OR "
                     "$.server.memberRole = READ_REPLICA\"");
  check_log_contains(router, R"_("destinations": [
                {
                    "strategy": "round-robin",
                    "classes": [
                        "rorw"
                    ],
                    "priority": 0
                }
            ])_");

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port);
    ASSERT_NO_ERROR(client_res);
  }
}

class RoutingGuidelinesAdapterTestStrategy
    : public RoutingGuidelinesAdapterTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(RoutingGuidelinesAdapterTestStrategy, ExplicitStrategy) {
  const auto router_port = port_pool_.get_next_available();
  const auto &strategy = GetParam();
  std::vector<std::pair<std::string, std::string>> options{
      {"bind_address", "127.0.0.1"},
      {"bind_port", std::to_string(router_port)},
      {"routing_strategy", strategy},
      {"destinations", "metadata-cache://test/default?role=PRIMARY"},
      {"protocol", "classic"}};
  const auto &routing_section =
      mysql_harness::ConfigBuilder::build_section("routing:test", options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router =
      launch_router({node_port}, routing_section, get_metadata_cache_section());
  check_log_contains(router, R"("strategy": ")" + strategy + "\"");

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port);
    ASSERT_NO_ERROR(client_res);
  }
}

INSTANTIATE_TEST_SUITE_P(ExplicitStrategy, RoutingGuidelinesAdapterTestStrategy,
                         ::testing::Values("round-robin", "first-available"));

TEST_F(RoutingGuidelinesAdapterTest, ExplicitName) {
  const auto router_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> options{
      {"bind_address", "127.0.0.1"},
      {"bind_port", std::to_string(router_port)},
      {"destinations", "metadata-cache://test/default?role=PRIMARY"},
      {"protocol", "classic"}};
  const auto &routing_section =
      mysql_harness::ConfigBuilder::build_section("routing:foobar", options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router =
      launch_router({node_port}, routing_section, get_metadata_cache_section());
  check_log_contains(router, R"_("destinations": [
        {
            "name": "foobar")_");
  check_log_contains(router, R"_("routes": [
        {
            "name": "foobar")_");

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port);
    ASSERT_NO_ERROR(client_res);
  }
}

TEST_F(RoutingGuidelinesAdapterTest, AutoGeneratedName) {
  const auto router_port = port_pool_.get_next_available();
  std::vector<std::pair<std::string, std::string>> options{
      {"bind_address", "127.0.0.1"},
      {"bind_port", std::to_string(router_port)},
      {"destinations", "metadata-cache://test/default?role=PRIMARY"},
      {"protocol", "classic"}};
  const auto &routing_section =
      mysql_harness::ConfigBuilder::build_section("routing", options);

  const auto node_port = port_pool_.get_next_available();
  const auto http_port = port_pool_.get_next_available();
  mock_server_spawner().spawn(
      mock_server_cmdline("metadata_1_node_repeat_v2_gr.js")
          .port(node_port)
          .http_port(http_port)
          .args());
  EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());

  set_mock_metadata(http_port, "", classic_ports_to_gr_nodes({node_port}), 0,
                    {node_port});

  auto &router =
      launch_router({node_port}, routing_section, get_metadata_cache_section());
  check_log_contains(router, R"^("destinations": [
        {
            "name": "__section_)^");
  check_log_contains(router, R"^("routes": [
        {
            "name": "__section_)^");

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port);
    ASSERT_NO_ERROR(client_res);
  }
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();  // WSAStartup

  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
