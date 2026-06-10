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

#ifdef _WIN32
#include <Winsock2.h>  // gethostname()
#endif

#include <algorithm>  // std::replace
#include <chrono>

#include "config_builder.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "router/src/routing/tests/mysql_client.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "routing_guidelines_builder.h"
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

class RoutingGuidelinesTest : public RouterComponentTest {
 protected:
  RoutingGuidelinesTest() {
    for (int i = 0; i < kClusterSize; i++) {
      cluster_nodes_ports.push_back(port_pool_.get_next_available());
      cluster_nodes_http_ports.push_back(port_pool_.get_next_available());
    }
  }

  auto &launch_router(
      const std::string &routing_section,
      const std::string &metadata_cache_section,
      const std::optional<std::string> &sharing_section = std::nullopt) {
    auto default_section = get_DEFAULT_defaults();
    init_keyring(default_section, get_test_temp_dir_name(),
                 {KeyringEntry{user_, "password", "mysql_test_password"}});

    const auto state_file = create_state_file(
        get_test_temp_dir_name(),
        create_state_file_content("", "", cluster_nodes_ports, 0));
    default_section["dynamic_state"] = state_file;

    std::string config = metadata_cache_section + routing_section;
    if (sharing_section) config += sharing_section.value();

    const std::string conf_file =
        create_config_file(get_test_temp_dir_name(), config, &default_section);

    return ProcessManager::launch_router({"-c", conf_file});
  }

  std::string get_routing_section(
      const uint16_t port, const std::string &role,
      const std::string &protocol = "classic",
      const std::optional<std::string> &name = std::nullopt,
      const bool enable_ssl = false) {
    std::map<std::string, std::string> options{
        {"bind_port", std::to_string(port)},
        {"destinations", "metadata-cache://test/default?role=" + role},
        {"protocol", protocol}};
    if (enable_ssl) {
      options.emplace(std::make_pair("client_ssl_mode", "PREFERRED"));
      options.emplace(std::make_pair("server_ssl_mode", "PREFERRED"));
      options.emplace(std::make_pair(
          "client_ssl_key", SSL_TEST_DATA_DIR "/server-key-sha512.pem"));
      options.emplace(std::make_pair(
          "client_ssl_cert", SSL_TEST_DATA_DIR "/server-cert-sha512.pem"));
    }

    std::string plugin_header = "routing:";
    if (!name) {
      plugin_header += role + "_" + protocol + "_" + std::to_string(port);
    } else {
      plugin_header += name.value();
    }

    return mysql_harness::ConfigBuilder::build_section(plugin_header, options);
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
        {"metadata_cluster", cluster_name_},
        {"ttl", "0.1"}};

    return mysql_harness::ConfigBuilder::build_section(
        "metadata_cache:bootstrap", options);
  }

  void instrument_metadata_detailed(
      std::string_view guidelines, const std::vector<GRNode> &gr_nodes,
      const std::vector<ClusterNode> cluster_nodes, const uint16_t http_port,
      bool trigger_failover = false,
      const std::optional<std::string> &router_options = std::nullopt) {
    const std::string &r_options = router_options ? router_options.value() : "";
    auto globals = mock_GR_metadata_as_json(
        "", gr_nodes, 0, cluster_nodes, /*view_id*/ 0ULL,
        /*error_on_md_query*/ false, "127.0.0.1", r_options,
        mysqlrouter::MetadataSchemaVersion{(2), (3), (0)}, cluster_name_);
    JsonAllocator allocator;

    globals.AddMember("transaction_count", 0, allocator);
    globals.AddMember(
        "routing_guidelines",
        JsonValue(guidelines.data(), guidelines.size(), allocator), allocator);
    const std::string ro_port_str = std::to_string(router_port_ro);
    globals.AddMember(
        "router_ro_classic_port",
        JsonValue(ro_port_str.data(), ro_port_str.size(), allocator),
        allocator);
    const std::string rw_port_str = std::to_string(router_port_rw);
    globals.AddMember(
        "router_rw_classic_port",
        JsonValue(rw_port_str.data(), rw_port_str.size(), allocator),
        allocator);
    const std::string rw_split_port_str = std::to_string(router_port_rw_split);
    globals.AddMember("router_rw_split_classic_port",
                      JsonValue(rw_split_port_str.data(),
                                rw_split_port_str.size(), allocator),
                      allocator);
    const std::string rw_x_port_str = std::to_string(router_port_x_rw);
    globals.AddMember(
        "router_rw_x_port",
        JsonValue(rw_x_port_str.data(), rw_x_port_str.size(), allocator),
        allocator);
    const std::string ro_x_port_str = std::to_string(router_port_x_ro);
    globals.AddMember(
        "router_ro_x_port",
        JsonValue(ro_x_port_str.data(), ro_x_port_str.size(), allocator),
        allocator);
    if (trigger_failover) {
      globals.AddMember("primary_failover", true, allocator);
    }

    auto globals_str = json_to_string(globals);
    EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(globals_str));
    EXPECT_TRUE(wait_for_transaction_count_increase(http_port, 2));
  }

  void instrument_metadata(
      std::string_view guidelines, const std::vector<uint16_t> &nodes,
      const uint16_t http_port, bool trigger_failover = false,
      const std::optional<std::string> &router_options = std::nullopt) {
    instrument_metadata_detailed(guidelines, classic_ports_to_gr_nodes(nodes),
                                 classic_ports_to_cluster_nodes(nodes),
                                 http_port, trigger_failover, router_options);
  }

  void setup_cluster(const std::string &mock_file,
                     const bool enable_ssl = false) {
    const auto http_port = cluster_nodes_http_ports[0];
    auto mock_server_cmdline_args = mock_server_cmdline(mock_file)
                                        .port(cluster_nodes_ports[0])
                                        .http_port(http_port)
                                        .args();

    if (enable_ssl) {
      std::initializer_list<std::pair<std::string_view, std::string_view>>
          mock_opts = {{"--ssl-cert", SSL_TEST_DATA_DIR "server-cert.pem"},
                       {"--ssl-key", SSL_TEST_DATA_DIR "server-key.pem"},
                       {"--ssl-mode", "PREFERRED"}};

      for (const auto &[key, value] : mock_opts) {
        mock_server_cmdline_args.emplace_back(key);
        mock_server_cmdline_args.emplace_back(value);
      }
    }

    auto &primary_node = mock_server_spawner().spawn(mock_server_cmdline_args);

    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(primary_node, cluster_nodes_ports[0]));

    EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
    set_mock_metadata(
        http_port, "", classic_ports_to_gr_nodes(cluster_nodes_ports), 0,
        classic_ports_to_cluster_nodes(cluster_nodes_ports), /*view_id*/ 0,
        /*error_on_md_quert*/ false, "127.0.0.1", /*router_options*/ "",
        mysqlrouter::MetadataSchemaVersion{(2), (3), (0)}, cluster_name_);

    // launch the secondary cluster nodes
    for (unsigned port = 1; port < cluster_nodes_ports.size(); ++port) {
      auto &secondary_node = mock_server_spawner().spawn(
          mock_server_cmdline("my_port.js")
              .port(cluster_nodes_ports[port])
              .http_port(cluster_nodes_http_ports[port])
              .enable_ssl(enable_ssl)
              .args());
      ASSERT_NO_FATAL_FAILURE(
          check_port_ready(secondary_node, cluster_nodes_ports[port]));
    }
  }

  const uint16_t kClusterSize = 6;
  const uint16_t router_port_rw{port_pool_.get_next_available()};
  const uint16_t router_port_ro{port_pool_.get_next_available()};
  const uint16_t router_port_ro_2{port_pool_.get_next_available()};
  const uint16_t router_port_x_rw{port_pool_.get_next_available()};
  const uint16_t router_port_x_ro{port_pool_.get_next_available()};
  const uint16_t router_port_rw_split{port_pool_.get_next_available()};

  std::vector<uint16_t> cluster_nodes_ports;
  std::vector<uint16_t> cluster_nodes_http_ports;
  const std::string user_{"mysql_test_user"};
  const std::string cluster_name_{"clusterA"};
};

class BasicRoutingStandaloneCluster
    : public RoutingGuidelinesTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(BasicRoutingStandaloneCluster, BasicRoutingStandaloneClusterTest) {
  const std::map<std::string, std::pair<uint16_t, uint16_t>> port_map{
      {"rw", {router_port_rw, cluster_nodes_ports[0]}},
      {"ro", {router_port_ro, cluster_nodes_ports[1]}}};

  const auto [router_port, expected_node_port] = port_map.at(GetParam());

  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  const auto &guidelines_str = guidelines_builder::create(
      {{"rw", "$.server.port = " + std::to_string(cluster_nodes_ports[0])},
       {"ro", "$.server.port = " + std::to_string(cluster_nodes_ports[1])}},
      {{"r1",
        "$.session.targetPort = $.router.port." + GetParam(),
        {{"round-robin", {GetParam()}}}}});

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());
  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);

  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, expected_node_port);
  }

  for (const auto port : {router_port_rw, router_port_ro}) {
    if (port != router_port) verify_new_connection_fails(port);
  }
}

INSTANTIATE_TEST_SUITE_P(BasicRoutingStandaloneClusterTest,
                         BasicRoutingStandaloneCluster,
                         ::testing::Values("rw", "ro"));

TEST_F(RoutingGuidelinesTest, PrimaryFailover) {
  setup_cluster("metadata_3_secondaries_primary_failover_v2_gr.js");

  const auto &guidelines_str = guidelines_builder::create(
      {{"rw", "$.server.memberRole = PRIMARY"},
       {"ro", "$.server.memberRole = SECONDARY"}},
      {{"r1",
        "$.session.targetPort = " + std::to_string(router_port_rw),
        {{"round-robin", {"rw"}}, {"round-robin", {"ro"}}}}});

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection directed to PRIMARY");
  {
    auto client_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }

  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0],
                      /*trigger_failover*/ true);

  SCOPED_TRACE("Connection directed to SECONDARY");
  {
    auto client_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }

  SCOPED_TRACE("PRIMARY node is back, it should be used as RW");
  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0],
                      /*trigger_failover*/ false);
  {
    auto client_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

TEST_F(RoutingGuidelinesTest, MultipleRoutesFirstNotMatching) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  const auto &guidelines_str = guidelines_builder::create(
      {{"secondary1",
        "$.server.port = " + std::to_string(cluster_nodes_ports[1])},
       {"secondary2",
        "$.server.port = " + std::to_string(cluster_nodes_ports[2])},
       {"secondary3",
        "$.server.port = " + std::to_string(cluster_nodes_ports[3])}},
      {{"r1", "FALSE", {{"round-robin", {"secondary1"}}}},
       {"r2",
        "$.session.targetPort = " + std::to_string(router_port_ro),
        {{"round-robin", {"secondary2"}}}},
       {"r3",
        "$.session.targetPort = " + std::to_string(router_port_ro_2),
        {{"round-robin", {"secondary3"}}}}});

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY") +
                        get_routing_section(router_port_ro_2, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("RW connection fail as there is no route for it");
  verify_new_connection_fails(router_port_rw);

  // First route leads to cluster_nodes_ports[1], but it could not be matched
  SCOPED_TRACE("Connecting first RO plugin");
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }

  SCOPED_TRACE("Connecting second RO plugin");
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro_2);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }
}

TEST_F(RoutingGuidelinesTest, RouteEnabledOption) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  bool r1_enabled{true};
  bool r2_enabled{true};
  bool r3_enabled{true};

  const auto &guidelines_creator = [&]() {
    return guidelines_builder::create(
        {{"secondary1",
          "$.server.port = " + std::to_string(cluster_nodes_ports[1])},
         {"secondary2",
          "$.server.port = " + std::to_string(cluster_nodes_ports[2])},
         {"secondary3",
          "$.server.port = " + std::to_string(cluster_nodes_ports[3])}},
        {{"r1",
          "TRUE",
          {{"round-robin", {"secondary1"}, /*priority*/ 5}},
          /*enabled*/ r1_enabled},
         {"r2",
          "$.session.targetPort = " + std::to_string(router_port_ro),
          {{"round-robin", {"secondary2"}, /*priority*/ 10}},
          /*enabled*/ r2_enabled},
         {"r3",
          "$.session.targetPort = " + std::to_string(router_port_ro),
          {{"round-robin", {"secondary3"}, /*priority*/ 15}},
          /*enabled*/ r3_enabled}});
  };

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_creator(), cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("All routes are enabled, first one is used");
  auto ro_con_route_1 = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con_route_1);
  EXPECT_EQ(select_port(ro_con_route_1->get()), cluster_nodes_ports[1]);

  SCOPED_TRACE("First route is disabled, r2 should be used");
  r1_enabled = false;
  instrument_metadata(guidelines_creator(), cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  auto ro_con_route_2 = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con_route_2);
  EXPECT_EQ(select_port(ro_con_route_2->get()), cluster_nodes_ports[2]);
  verify_existing_connection_dropped(ro_con_route_1->get());

  SCOPED_TRACE("Second route is disabled, r3 should be used");
  r2_enabled = false;
  instrument_metadata(guidelines_creator(), cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  auto ro_con_route_3 = make_new_connection(router_port_ro);
  ASSERT_NO_ERROR(ro_con_route_3);
  EXPECT_EQ(select_port(ro_con_route_3->get()), cluster_nodes_ports[3]);
  verify_existing_connection_dropped(ro_con_route_2->get());

  SCOPED_TRACE("First route is enabled again");
  r1_enabled = true;
  instrument_metadata(guidelines_creator(), cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  verify_existing_connection_ok(ro_con_route_3->get());
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
}

TEST_F(RoutingGuidelinesTest, NoDestinationsMatched) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  const auto &guidelines_creator = [&]() {
    return guidelines_builder::create(
        {{"empty_group", "FALSE"}},
        {{"r1", "TRUE", {{"round-robin", {"empty_group"}}}}});
  };

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_creator(), cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("There is only one destination group, but it is empty");
  verify_new_connection_fails(router_port_ro);
  verify_new_connection_fails(router_port_rw);
}

TEST_F(RoutingGuidelinesTest, FirstDestinationGroupEmptyRoundRobin) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  const auto &guidelines_creator = [&]() {
    return guidelines_builder::create(
        {{"empty_group", "FALSE"},
         {"working_group", "$.server.memberRole = SECONDARY"}},
        {{"r1", "TRUE", {{"round-robin", {"empty_group", "working_group"}}}}});
  };

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_creator(), cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE(
      "First group is empty, switch to the second one and go round-robin");
  std::vector<std::uint16_t> ports_used;
  for (std::size_t i = 0; i < cluster_nodes_ports.size() - 1; i++) {
    auto ro_con_route = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(ro_con_route);
    auto port_res = select_port(ro_con_route->get());
    ASSERT_NO_ERROR(port_res);
    ports_used.push_back(port_res.value());
  }

  // All secondary nodes are used, next connection should start at the beginning
  EXPECT_THAT(ports_used,
              ::testing::ElementsAreArray(std::begin(cluster_nodes_ports) + 1,
                                          std::end(cluster_nodes_ports)));

  // round robin wraps around to the first position in a dest group
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
}

TEST_F(RoutingGuidelinesTest, RoundRobinGroupSwitch) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  // Create route with groups [[Node1, Node2],[Node3,Node4],[Node5]]
  const auto &guidelines_creator = [&]() {
    return guidelines_builder::create(
        {{"g1", "$.server.port IN (" + std::to_string(cluster_nodes_ports[1]) +
                    "," + std::to_string(cluster_nodes_ports[2]) + ")"},
         {"g2", "$.server.port IN (" + std::to_string(cluster_nodes_ports[3]) +
                    "," + std::to_string(cluster_nodes_ports[4]) + ")"},
         {"g3",
          "$.server.port IN (" + std::to_string(cluster_nodes_ports[5]) + ")"}},
        {{"r1",
          "TRUE",
          {{"round-robin", {"g1"}},
           {"round-robin", {"g2"}},
           {"round-robin", {"g3"}}}}});
  };
  const auto guidelines_str = guidelines_creator();

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("All nodes are up, round robin in first destination group");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }

  SCOPED_TRACE("One node from the first group goes away");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[2],
                      cluster_nodes_ports[3], cluster_nodes_ports[4],
                      cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }

  SCOPED_TRACE("First group is down, round robin in second group");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[3],
                      cluster_nodes_ports[4], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[4]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }

  SCOPED_TRACE("Remove one node from the second group");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[4],
                      cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[4]);
  }

  SCOPED_TRACE("Bring back node in the first group");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[2],
                      cluster_nodes_ports[4], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }

  SCOPED_TRACE("Only the last group is alive");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[5]);
  }

  SCOPED_TRACE("Bring back all nodes");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1],
                      cluster_nodes_ports[2], cluster_nodes_ports[3],
                      cluster_nodes_ports[4], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
}

TEST_F(RoutingGuidelinesTest, FirstAvailableGroupSwitch) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  // Create route with groups [[Node1, Node2],[Node3,Node4],[Node5]]
  const auto &guidelines_creator = [&]() {
    return guidelines_builder::create(
        {{"g1", "$.server.port IN (" + std::to_string(cluster_nodes_ports[1]) +
                    "," + std::to_string(cluster_nodes_ports[2]) + ")"},
         {"g2", "$.server.port IN (" + std::to_string(cluster_nodes_ports[3]) +
                    "," + std::to_string(cluster_nodes_ports[4]) + ")"},
         {"g3",
          "$.server.port IN (" + std::to_string(cluster_nodes_ports[5]) + ")"}},
        {{"r1",
          "TRUE",
          {{"first-available", {"g1"}},
           {"first-available", {"g2"}},
           {"first-available", {"g3"}}}}});
  };
  const auto guidelines_str = guidelines_creator();

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("All nodes are up, always go to 1st node in 1st group");
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }

  SCOPED_TRACE("One node from the first group goes away");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[2],
                      cluster_nodes_ports[3], cluster_nodes_ports[4],
                      cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }

  SCOPED_TRACE("First group is down, pick up 1st node from 2nd group");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[3],
                      cluster_nodes_ports[4], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }

  SCOPED_TRACE("Remove one node from the second group");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[4],
                      cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[4]);
  }

  SCOPED_TRACE("Bring back node in the first group");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[2],
                      cluster_nodes_ports[4], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }

  SCOPED_TRACE("Only the last group is alive");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[5]);
  }

  SCOPED_TRACE("Bring back all nodes");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1],
                      cluster_nodes_ports[2], cluster_nodes_ports[3],
                      cluster_nodes_ports[4], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
}

TEST_F(RoutingGuidelinesTest, MixedStrategyGroupSwitch) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  // Create route with groups [[Node1, Node2],[Node3,Node4],[Node5]]
  const auto &guidelines_creator = [&]() {
    return guidelines_builder::create(
        {{"g1", "$.server.port IN (" + std::to_string(cluster_nodes_ports[1]) +
                    "," + std::to_string(cluster_nodes_ports[2]) + ")"},
         {"g2", "$.server.port IN (" + std::to_string(cluster_nodes_ports[3]) +
                    "," + std::to_string(cluster_nodes_ports[4]) + ")"},
         {"g3",
          "$.server.port IN (" + std::to_string(cluster_nodes_ports[5]) + ")"}},
        {{"r1",
          "TRUE",
          {{"first-available", {"g1"}},
           {"round-robin", {"g2"}},
           {"first-available", {"g3"}}}}});
  };
  const auto guidelines_str = guidelines_creator();

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("All nodes are up, always go to 1st node in 1st group");
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }

  SCOPED_TRACE("One node from the first group goes away");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[2],
                      cluster_nodes_ports[3], cluster_nodes_ports[4],
                      cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }

  SCOPED_TRACE("First group is down, second group uses round-robin");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[3],
                      cluster_nodes_ports[4], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[4]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }

  SCOPED_TRACE("Remove one node from the second group");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[4],
                      cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[4]);
  }

  SCOPED_TRACE("Bring back node in the first group");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[2],
                      cluster_nodes_ports[4], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }

  SCOPED_TRACE("Only the last group is alive");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[5]);
  }

  SCOPED_TRACE("Bring back all nodes");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1],
                      cluster_nodes_ports[2], cluster_nodes_ports[3],
                      cluster_nodes_ports[4], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
}

TEST_F(RoutingGuidelinesTest, NoDestinationsOneGroup) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  const auto &guidelines_creator = [&]() {
    return guidelines_builder::create(
        {{"empty", "FALSE"}},
        {{"r1", "TRUE", {{"first-available", {"empty"}}}}});
  };
  const auto guidelines_str = guidelines_creator();

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  verify_new_connection_fails(router_port_ro);
}

TEST_F(RoutingGuidelinesTest, NoDestinationsMultipleGroups) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  const auto &guidelines_creator = [&]() {
    return guidelines_builder::create(
        {{"empty1", "FALSE"}, {"empty2", "FALSE"}, {"empty3", "FALSE"}},
        {{"r1",
          "TRUE",
          {{"first-available", {"empty1"}},
           {"first-available", {"empty2"}},
           {"first-available", {"empty3"}}}}});
  };
  const auto guidelines_str = guidelines_creator();

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  verify_new_connection_fails(router_port_ro);
}

TEST_F(RoutingGuidelinesTest, RoutePriorities) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  // Create route with groups [[Node1, Node2],[Node3,Node4],[Node5]]
  const auto &guidelines_creator = [&]() {
    return guidelines_builder::create(
        {{"g1", "$.server.port IN (" + std::to_string(cluster_nodes_ports[1]) +
                    "," + std::to_string(cluster_nodes_ports[2]) + ")"},
         {"g2", "$.server.port IN (" + std::to_string(cluster_nodes_ports[3]) +
                    "," + std::to_string(cluster_nodes_ports[4]) + ")"},
         {"g3",
          "$.server.port IN (" + std::to_string(cluster_nodes_ports[5]) + ")"}},
        {{"r1",
          "TRUE",
          {{"round-robin", {"g1"}, /*priority*/ 5},
           {"round-robin", {"g2"}, /*priority*/ 1},
           {"round-robin", {"g3"}, /*priority*/ 2}}}});
  };
  const auto guidelines_str = guidelines_creator();

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(guidelines_str, cluster_nodes_ports,
                      cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE(
      "All nodes are up, round robin in g2 destination group (it has "
      "highest priority)");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[4]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }

  SCOPED_TRACE("One node from the g2 group goes away");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1],
                      cluster_nodes_ports[2], cluster_nodes_ports[4],
                      cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[4]);
  }

  SCOPED_TRACE(
      "g2 group is down, round robin in g3 group (second highest priority)");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1],
                      cluster_nodes_ports[2], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[5]);
  }

  SCOPED_TRACE("g3 group is also down, switch to g1");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1],
                      cluster_nodes_ports[2]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }

  SCOPED_TRACE("Bring back node in the g2 group");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1],
                      cluster_nodes_ports[2], cluster_nodes_ports[3]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  for (int i = 0; i < 3; i++) {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }

  SCOPED_TRACE("Bring back all nodes");
  {
    auto new_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1],
                      cluster_nodes_ports[2], cluster_nodes_ports[3],
                      cluster_nodes_ports[4], cluster_nodes_ports[5]};
    instrument_metadata(guidelines_str, new_ports, cluster_nodes_http_ports[0],
                        /*trigger_failover*/ false);
    EXPECT_TRUE(
        wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[4]);
  }
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[3]);
  }
}

TEST_F(RoutingGuidelinesTest, UpdateDropUnsupportedConnection) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1", "TRUE", {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Make a valid connection");
  std::unique_ptr<MySQLSession> con;
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
    con = std::move(client_res.value());
  }

  SCOPED_TRACE(
      "Update guidelines so that existing connection is no longer valid");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1", "TRUE", {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));

  verify_existing_connection_dropped(con.get());

  SCOPED_TRACE("New connection according to the updated guidelines");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
}

TEST_F(RoutingGuidelinesTest, UpdateKeepConnectionDespiteDestMatchChanged) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  SCOPED_TRACE("Match only one specific node");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1", "TRUE", {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Make a valid connection");
  std::unique_ptr<MySQLSession> con;
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
    con = std::move(client_res.value());
  }

  SCOPED_TRACE(
      "Update guidelines with new destination group which allows the existing "
      "connection to be kept");
  instrument_metadata(guidelines_builder::create(
                          {{"new_d", "$.server.memberRole = PRIMARY"}},
                          {{"r1", "TRUE", {{"first-available", {"new_d"}}}}}),
                      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));

  verify_existing_connection_ok(con.get());

  SCOPED_TRACE("New connection should work as well");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

TEST_F(RoutingGuidelinesTest, UpdateKeepConnectionDespiteRoutesChanged) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  std::string plugin_name = "plugin1";
  auto &router = launch_router(
      get_routing_section(router_port_ro, "SECONDARY", "classic", plugin_name),
      get_metadata_cache_section());

  SCOPED_TRACE("Match only one specific node");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.session.targetPort=" + std::to_string(router_port_ro),
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Make a valid connection");
  std::unique_ptr<MySQLSession> con;
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
    con = std::move(client_res.value());
  }

  SCOPED_TRACE(
      "Update guidelines with new route which allows the existing connection "
      "to be kept");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"new_route",
            "$.router.routeName=" + plugin_name,
            {{"round-robin", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));

  verify_existing_connection_ok(con.get());

  SCOPED_TRACE("New connection should work as well");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

TEST_F(RoutingGuidelinesTest, UpdateDropConnectionMatchingOtherPlugin) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  std::string plugin_1 = "routing1";
  std::string plugin_2 = "routing2";
  auto &router = launch_router(
      get_routing_section(router_port_rw, "PRIMARY", "classic", plugin_1) +
          get_routing_section(router_port_ro, "SECONDARY", "classic", plugin_2),
      get_metadata_cache_section());

  SCOPED_TRACE("Match only one specific node");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.routeName=" + plugin_1,
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Make a valid connection to first plugin");
  std::unique_ptr<MySQLSession> con;
  {
    auto client_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
    con = std::move(client_res.value());
  }

  SCOPED_TRACE(
      "New guidelines allows the same node to be used, but on a different "
      "plugin");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"new_route",
            "$.router.routeName=" + plugin_2,
            {{"round-robin", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));

  verify_existing_connection_dropped(con.get());

  SCOPED_TRACE("New connection should go to the other plugin");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

class GuidelinesFailedUpdate
    : public RoutingGuidelinesTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(GuidelinesFailedUpdate, UpdateWithUnsupportedVersion) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  SCOPED_TRACE("Match only one specific node");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.session.targetPort=" + std::to_string(router_port_ro),
            {{"first-available", {"d1"}}}}},
          "guidelines_1", "1.0"),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Make a connection");
  std::unique_ptr<MySQLSession> con;
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
    con = std::move(client_res.value());
  }

  SCOPED_TRACE("New guidelines have unsupported version, should not be used");
  instrument_metadata(
      guidelines_builder::create(
          {{"d2", "$.server.port=" + std::to_string(cluster_nodes_ports[2])}},
          {{"new_route",
            "$.session.targetPort=" + std::to_string(router_port_ro),
            {{"round-robin", {"d2"}}}}},
          "guidelines_2", GetParam()),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));

  EXPECT_TRUE(wait_log_contains(
      router,
      "Update guidelines failed - routing guidelines version not supported. "
      "Router supported version is 1.1 but got " +
          GetParam(),
      5s));
  SCOPED_TRACE("Guidelines are not updated");
  verify_existing_connection_ok(con.get());
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
}

INSTANTIATE_TEST_SUITE_P(GuidelinesFailedUpdateTest, GuidelinesFailedUpdate,
                         ::testing::Values("1.2", "1.9", "2.5"));

class GuidelinesUpdate : public RoutingGuidelinesTest,
                         public ::testing::WithParamInterface<std::string> {};

TEST_P(GuidelinesUpdate, UpdateWithSupportedVersion) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  SCOPED_TRACE("Match only one specific node");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.session.targetPort=" + std::to_string(router_port_ro),
            {{"first-available", {"d1"}}}}},
          "guidelines_1", "1.0"),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Make a connection");
  std::unique_ptr<MySQLSession> con;
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
    con = std::move(client_res.value());
  }

  SCOPED_TRACE("New guideline version should allow to use this guideline");
  instrument_metadata(
      guidelines_builder::create(
          {{"d2", "$.server.port=" + std::to_string(cluster_nodes_ports[2])}},
          {{"new_route",
            "$.session.targetPort=" + std::to_string(router_port_ro),
            {{"round-robin", {"d2"}}}}},
          "guidelines_2", GetParam()),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));

  SCOPED_TRACE("Guidelines are updated");
  verify_existing_connection_dropped(con.get());
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }
}

INSTANTIATE_TEST_SUITE_P(GuidelinesUpdateTest, GuidelinesUpdate,
                         ::testing::Values("0.0", "0.5", "1.0", "1.1"));

TEST_F(RoutingGuidelinesTest, UpdateSetToDefault) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  SCOPED_TRACE("Match only one specific node");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.session.targetPort=$.router.port.ro",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Make a connection");
  std::unique_ptr<MySQLSession> con;
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
    con = std::move(client_res.value());
  }
  SCOPED_TRACE("RW port is not allowed in this guideline");
  verify_new_connection_fails(router_port_rw);

  SCOPED_TRACE("Restore default config based guideline");
  instrument_metadata("{}", cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(wait_log_contains(
      router, "Restore initial routing guidelines autogenerated from config",
      5s));

  SCOPED_TRACE("Guidelines are set to default");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
  {
    auto client_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

class MatchExtendedInfoSSLdisabledTest
    : public RoutingGuidelinesTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(MatchExtendedInfoSSLdisabledTest, MatchExtendedInfoSSLdisabled) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  const std::string failing_match = "$.session." + GetParam() + "='foobar'";
  SCOPED_TRACE(
      "First route match depends on SSL being enabled, which is not the case");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1", failing_match, {{"first-available", {"d1"}}}},
           {"r2",
            "$.session.targetPort=$.router.port.rw",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);

  EXPECT_TRUE(wait_log_contains(
      router,
      escape_regexp(
          "S.session.user, $.session.schema and $.session.connectAttrs are "
          "supported only when ssl_server_mode is set to PREFERRED"),
      5s));

  SCOPED_TRACE("First route is not matched and could not be used");
  verify_new_connection_fails(router_port_ro);
  {
    auto client_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(client_res);
  }
}

INSTANTIATE_TEST_SUITE_P(
    MatchExtendedInfoSSLdisabled, MatchExtendedInfoSSLdisabledTest,
    ::testing::Values("user", "connectAttrs._client_name",
                      "connectAttrs._client_version", "connectAttrs._os",
                      "connectAttrs._pid", "connectAttrs._platform", "schema"));

TEST_F(RoutingGuidelinesTest, MatchDefaultSchema) {
  SCOPED_TRACE(
      "Enable secure transport so that the Router could inspect connection "
      "details");
  setup_cluster("metadata_dynamic_nodes_v2_gr.js", /*enable_ssl*/ true);

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY",
                                                   "classic", "test_routing",
                                                   /*enable_ssl*/ true),
                               get_metadata_cache_section());

  const std::string matching_schema = "foobar";
  SCOPED_TRACE("Route match based on connection's default schema");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.session.schema='" + matching_schema + "'",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection matching our route's default schema");
  {
    MySQLSession session;
    EXPECT_NO_THROW(session.connect("127.0.0.1", router_port_ro, "username",
                                    "password", "", matching_schema));
    auto result{session.query_one("select @@port")};
    EXPECT_EQ(cluster_nodes_ports[1], std::stoul((*result)[0]));
  }

  SCOPED_TRACE("Connection using different schema");
  {
    MySQLSession session;
    EXPECT_ANY_THROW(session.connect("127.0.0.1", router_port_ro, "username",
                                     "password", "", "mysql"));
    EXPECT_TRUE(wait_log_contains(router, "Could not match any route", 5s));
  }
}

TEST_F(RoutingGuidelinesTest, MatchUsername) {
  SCOPED_TRACE(
      "Enable secure transport so that the Router could inspect connection "
      "details");
  setup_cluster("metadata_dynamic_nodes_v2_gr.js", /*enable_ssl*/ true);

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY",
                                                   "classic", "test_routing",
                                                   /*enable_ssl*/ true),
                               get_metadata_cache_section());

  const std::string matching_user = "username";
  SCOPED_TRACE("Route match based on connection's default schema");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.session.user='" + matching_user + "'",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection matching defined route's user");
  {
    MySQLSession session;
    EXPECT_NO_THROW(session.connect("127.0.0.1", router_port_ro, matching_user,
                                    "password", "", ""));
    auto result{session.query_one("select @@port")};
    EXPECT_EQ(cluster_nodes_ports[1], std::stoul((*result)[0]));
  }

  SCOPED_TRACE("Connection using different user");
  {
    MySQLSession session;
    EXPECT_ANY_THROW(session.connect("127.0.0.1", router_port_ro,
                                     "not_matching_user", "password", "", ""));
    EXPECT_TRUE(wait_log_contains(router, "Could not match any route", 5s));
  }
}

class MatchConnectionAttributesTest
    : public RoutingGuidelinesTest,
      public ::testing::WithParamInterface<std::string> {};

TEST_P(MatchConnectionAttributesTest, MatchConnectionAttributes) {
  SCOPED_TRACE(
      "Enable secure transport so that the Router could inspect connection "
      "details");
  setup_cluster("metadata_dynamic_nodes_v2_gr.js", /*enable_ssl*/ true);

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY",
                                                   "classic", "test_routing",
                                                   /*enable_ssl*/ true),
                               get_metadata_cache_section());

  const std::string &match = GetParam();
  SCOPED_TRACE("Route match based on connection attributes");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1", match, {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Route is matched, connection attempt is successful");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
  }
}

INSTANTIATE_TEST_SUITE_P(
    MatchConnectionAttributes, MatchConnectionAttributesTest,
    ::testing::Values(
        "$.session.connectAttrs._client_name = 'libmysql'",
        "NUMBER($.session.connectAttrs._pid) > 1",  // 0 is not used, 1 is init
        "$.session.connectAttrs._client_version <> ''",
        "$.session.connectAttrs._os <> ''",
        "$.session.connectAttrs._platform <> ''"),
    [](const auto &arg) {
      // Extract connection attribute name from matching condition
      const std::string connect_attrs = "connectAttrs.";
      size_t start_pos = arg.param.find(connect_attrs);
      start_pos += connect_attrs.length();
      size_t end_pos = start_pos;
      while (end_pos < arg.param.length() &&
             (isalnum(arg.param[end_pos]) || arg.param[end_pos] == '_')) {
        ++end_pos;
      }
      return arg.param.substr(start_pos, end_pos - start_pos);
    });

TEST_F(RoutingGuidelinesTest, MatchSessionRandomValue) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  SCOPED_TRACE("Match based on random variable");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.session.randomValue >= 0.1",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Make some connection attempts");
  std::vector<std::unique_ptr<MySQLSession>> connections;
  for (int i = 0; i < 20; i++) {
    try {
      std::unique_ptr<MySQLSession> session = std::make_unique<MySQLSession>();
      session->connect("127.0.0.1", router_port_ro, "username", "password", "",
                       "mysql");
      connections.emplace_back(std::move(session));
    } catch (...) {
      // Some connections will fail if generated random value is less than 0.1
    }
  }

  // With 20 consecutive connection it would be extremely unlikely to not have
  // any random result > 0.1
  EXPECT_TRUE(connections.size() > 0);

  SCOPED_TRACE("Update the guidelines");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.session.randomValue >= 0.09",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));

  // Random value is generated once per session, and it initially had value >
  // 0.1 so it is not possible that existing connections were dropped
  for (const auto &con : connections) {
    verify_existing_connection_ok(con.get());
  }
}

TEST_F(RoutingGuidelinesTest, MatchSessionSourceIP) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  SCOPED_TRACE("Match any connection from localhost");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.session.sourceIP = RESOLVE_V4(localhost)",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
  }
}

TEST_F(RoutingGuidelinesTest, MatchSessionTargetIP) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  SCOPED_TRACE("Match any connection to localhost");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.session.targetIP = RESOLVE_V4(localhost)",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
  }
}

TEST_F(RoutingGuidelinesTest, MatchRouterBindAddress) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  SCOPED_TRACE(
      "Match connections from applications running on the same machine as the "
      "Router");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.router.bindAddress = '127.0.0.1'",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
  }
}

TEST_F(RoutingGuidelinesTest, MatchRouterHostname) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router =
      launch_router(get_routing_section(router_port_rw, "PRIMARY") +
                        get_routing_section(router_port_ro, "SECONDARY"),
                    get_metadata_cache_section());

  SCOPED_TRACE(
      "Match connections reaching Router running on a specific hostname");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[1])}},
          {{"r1",
            "$.router.hostname = 'router-host'",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
  }
}

TEST_F(RoutingGuidelinesTest, MatchRoutingRouteName) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  std::string plugin_name = "routing1";
  auto &router = launch_router(
      get_routing_section(router_port_ro, "SECONDARY", "classic", plugin_name),
      get_metadata_cache_section());

  SCOPED_TRACE("Match routing plugin name");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.routeName=" + plugin_name,
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

TEST_F(RoutingGuidelinesTest, MatchRouterName) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  std::string router_name = "test_router";
  auto &router = launch_router(
      get_routing_section(router_port_ro, "SECONDARY", "classic", router_name),
      get_metadata_cache_section());

  SCOPED_TRACE("Match routing plugin name");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.name=" + router_name,
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

struct SupportedMatch {
  std::string version;
  std::string match;
};

class RoutingGuidelinesTagsTest
    : public RoutingGuidelinesTest,
      public ::testing::WithParamInterface<SupportedMatch> {};

class RoutingGuidelinesStringTagsTest : public RoutingGuidelinesTagsTest {};

TEST_P(RoutingGuidelinesStringTagsTest, string) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");
  const auto match = GetParam().match;

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match router tags");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar='" + match + "'",
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\": \"baz\"}}");
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }

  SCOPED_TRACE("Match router tags using \"\"");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar=\"" + match + "\"",
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\": \"baz\"}}");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }

  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar='" + match + "'",
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\": \"miss\"}}");

  SCOPED_TRACE("Tags has changed, route could not be matched");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  verify_new_connection_fails(router_port_ro);
}

INSTANTIATE_TEST_SUITE_P(RoutingGuidelinesStringTags,
                         RoutingGuidelinesStringTagsTest,
                         ::testing::Values(SupportedMatch{"1.0", "\"baz\""},
                                           SupportedMatch{"1.1", "baz"}),
                         [](auto info) {
                           std::replace(info.param.version.begin(),
                                        info.param.version.end(), '.', '_');
                           return std::string("v") + info.param.version;
                         });

class RoutingGuidelinesBoolTagsTest : public RoutingGuidelinesTagsTest {};

TEST_P(RoutingGuidelinesBoolTagsTest, boolean) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match router tags");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar=" + GetParam().match,
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\": true}}");
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }

  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar=" + GetParam().match,
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\": false}}");

  SCOPED_TRACE("Tags has changed, route could not be matched");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  verify_new_connection_fails(router_port_ro);
}

INSTANTIATE_TEST_SUITE_P(RoutingGuidelinesBoolTags,
                         RoutingGuidelinesBoolTagsTest,
                         ::testing::Values(SupportedMatch{"1.0", "'true'"},
                                           SupportedMatch{"1.1", "true"}),
                         [](auto info) {
                           std::replace(info.param.version.begin(),
                                        info.param.version.end(), '.', '_');
                           return std::string("v") + info.param.version;
                         });

class RoutingGuidelinesIntTagsTest : public RoutingGuidelinesTagsTest {};

TEST_P(RoutingGuidelinesIntTagsTest, integer) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match router tags");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar=" + GetParam().match,
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\": 41}}");
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }

  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar=" + GetParam().match,
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\": 9}}");

  SCOPED_TRACE("Tags has changed, route could not be matched");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  verify_new_connection_fails(router_port_ro);
}

INSTANTIATE_TEST_SUITE_P(RoutingGuidelinesIntTags, RoutingGuidelinesIntTagsTest,
                         ::testing::Values(SupportedMatch{"1.0", "'41'"},
                                           SupportedMatch{"1.1", "41"}),
                         [](auto info) {
                           std::replace(info.param.version.begin(),
                                        info.param.version.end(), '.', '_');
                           return std::string("v") + info.param.version;
                         });

class RoutingGuidelinesNullTagsTest : public RoutingGuidelinesTagsTest {};

TEST_P(RoutingGuidelinesNullTagsTest, null_value) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match router tags");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar=" + GetParam().match,
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\":null}}");
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }

  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar=" + GetParam().match,
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\": \"not null\"}}");

  SCOPED_TRACE("Tags has changed, route could not be matched");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  verify_new_connection_fails(router_port_ro);
}

INSTANTIATE_TEST_SUITE_P(RoutingGuidelinesNullTags,
                         RoutingGuidelinesNullTagsTest,
                         ::testing::Values(SupportedMatch{"1.0", "'null'"},
                                           SupportedMatch{"1.1", "null"}),
                         [](auto info) {
                           std::replace(info.param.version.begin(),
                                        info.param.version.end(), '.', '_');
                           return std::string("v") + info.param.version;
                         });

class RoutingGuidelinesObjTagsTest : public RoutingGuidelinesTagsTest {};

TEST_P(RoutingGuidelinesObjTagsTest, object) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match router tags");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            R"($.router.tags.foobar=)" + GetParam().match,
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, R"({"tags": {"foobar": {"bar":1}}})");
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }

  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            R"($.router.tags.foobar=)" + GetParam().match,
            {{"first-available", {"d1"}}}}},
          "rg", GetParam().version),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, R"({"tags": {"foobar": {"bar":2}}})");

  SCOPED_TRACE("Tags has changed, route could not be matched");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_http_ports[0], 2));
  verify_new_connection_fails(router_port_ro);
}

INSTANTIATE_TEST_SUITE_P(
    RoutingGuidelinesObjTags, RoutingGuidelinesObjTagsTest,
    ::testing::Values(SupportedMatch{"1.0", "'{\"bar\":1}'"},
                      SupportedMatch{"1.1", "{\"bar\":1}"}),
    [](auto info) {
      std::replace(info.param.version.begin(), info.param.version.end(), '.',
                   '_');
      return std::string("v") + info.param.version;
    });

TEST_F(RoutingGuidelinesTest, MatchRouterTagsInvalid) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match router tags");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar='TRUE'",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\": TRUE}}");
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection cannot be matched");
  EXPECT_TRUE(wait_log_contains(
      router, "Error parsing router tags JSON string: not a valid JSON object",
      5s));

  verify_new_connection_fails(router_port_ro);
}

TEST_F(RoutingGuidelinesTest, MatchRouterComplexExpr) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match complex expression");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.router.tags.foobar=true AND ($.router.port.ro > 65535 "
            "OR $.router.hostname = 'router-host')",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0],
      /*trigger_failover*/ false, "{\"tags\": {\"foobar\": true}}");
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

TEST_F(RoutingGuidelinesTest, MatchRouterRWSplitPort) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js", /*enable_ssl*/ true);

  auto routing_section =
      get_routing_section(router_port_rw_split, "PRIMARY_AND_SECONDARY",
                          "classic", "plugin1", /*enable_ssl*/ true);
  routing_section += "connection_sharing=1 \n access_mode=auto";
  const std::string sharing_section =
      "\n[connection_pool]\nmax_idle_server_connections=1";
  auto &router = launch_router(routing_section, get_metadata_cache_section(),
                               sharing_section);

  SCOPED_TRACE("Match router RW splitting port");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[0])}},
          {{"r1",
            "$.session.targetPort=$.router.port.rw_split",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_rw_split);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

TEST_F(RoutingGuidelinesTest, MatchServerLabel) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match server label (in format <address>:<ip>)");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.label=CONCAT(RESOLVE_V4(localhost), ':'," +
                      std::to_string(cluster_nodes_ports[0]) + ")"}},
          {{"r1", "TRUE", {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

TEST_F(RoutingGuidelinesTest, MatchServerUUID) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  // uuid-1 is the uuid of the first server, as in classic_ports_to_gr_nodes()
  SCOPED_TRACE("Match server UUID");
  instrument_metadata(guidelines_builder::create(
                          {{"d1", "$.server.uuid='uuid-1'"}},
                          {{"r1", "TRUE", {{"first-available", {"d1"}}}}}),
                      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

TEST_F(RoutingGuidelinesTest, MatchServerVersion) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match server version");
  instrument_metadata(guidelines_builder::create(
                          {{"d1", "$.server.version < 90090"}},
                          {{"r1", "TRUE", {{"first-available", {"d1"}}}}}),
                      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[0]);
  }
}

struct ServerSupportedMatch {
  std::string version;
  std::string old_match;
  std::string new_match;
};

class RoutingGuidelinesServerTagsTest
    : public RoutingGuidelinesTest,
      public ::testing::WithParamInterface<ServerSupportedMatch> {};

TEST_P(RoutingGuidelinesServerTagsTest, server_tags) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  // Prepare custom tags for one of the servers
  const auto &gr_nodes = classic_ports_to_gr_nodes(
      {cluster_nodes_ports[0], cluster_nodes_ports[1], cluster_nodes_ports[2]});
  auto cluster_nodes = classic_ports_to_cluster_nodes(
      {cluster_nodes_ports[0], cluster_nodes_ports[1], cluster_nodes_ports[2]});
  JsonValue tags(rapidjson::kObjectType);
  JsonAllocator alloc;
  tags.AddMember("my_tag", "foobar", alloc);
  JsonValue attributes(rapidjson::kObjectType);
  attributes.AddMember("tags", tags, alloc);
  cluster_nodes[1].attributes = json_to_string(attributes);

  SCOPED_TRACE("Match server tags");
  instrument_metadata_detailed(
      guidelines_builder::create(
          {{"d1", "$.server.tags.my_tag='" + GetParam().old_match + "'"}},
          {{"r1", "TRUE", {{"first-available", {"d1"}}}}}, "rg",
          GetParam().version),
      gr_nodes, cluster_nodes, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }

  // Update the tag without changing the topology
  JsonValue new_tags(rapidjson::kObjectType);
  new_tags.AddMember("my_tag", "baz", alloc);
  JsonValue new_attributes(rapidjson::kObjectType);
  new_attributes.AddMember("tags", new_tags, alloc);
  cluster_nodes[1].attributes = json_to_string(new_attributes);

  SCOPED_TRACE("Match updated tags");
  instrument_metadata_detailed(
      guidelines_builder::create(
          {{"d1", "$.server.tags.my_tag='" + GetParam().new_match + "'"}},
          {{"r1", "TRUE", {{"first-available", {"d1"}}}}}, "rg",
          GetParam().version),
      gr_nodes, cluster_nodes, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched after tags update");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[1]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    RoutingGuidelinesServerTags, RoutingGuidelinesServerTagsTest,
    ::testing::Values(ServerSupportedMatch{"1.0", "\"foobar\"", "\"baz\""},
                      ServerSupportedMatch{"1.1", "foobar", "baz"}),
    [](auto info) {
      std::replace(info.param.version.begin(), info.param.version.end(), '.',
                   '_');
      return std::string("v") + info.param.version;
    });

TEST_F(RoutingGuidelinesTest, MatchRouterLocalCluster) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match router local cluster");
  instrument_metadata(
      guidelines_builder::create(
          {{"d1", "$.server.port=" + std::to_string(cluster_nodes_ports[2])}},
          {{"r1",
            "$.router.localCluster='" + cluster_name_ + "'",
            {{"first-available", {"d1"}}}}}),
      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
    auto port_res = select_port(client_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_ports[2]);
  }
}

TEST_F(RoutingGuidelinesTest, MatchClusterRole) {
  setup_cluster("metadata_dynamic_nodes_v2_gr.js");

  auto &router = launch_router(get_routing_section(router_port_ro, "SECONDARY"),
                               get_metadata_cache_section());

  SCOPED_TRACE("Match standalone cluster role");
  instrument_metadata(guidelines_builder::create(
                          {{"d1", "$.server.clusterRole = UNDEFINED"}},
                          {{"r1", "TRUE", {{"first-available", {"d1"}}}}}),
                      cluster_nodes_ports, cluster_nodes_http_ports[0]);
  EXPECT_TRUE(
      wait_log_contains(router, "Routing guidelines document updated", 5s));

  SCOPED_TRACE("Connection is matched");
  {
    auto client_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(client_res);
  }
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();  // WSAStartup

  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
