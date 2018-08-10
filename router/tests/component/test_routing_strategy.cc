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
#include "keyring/keyring_manager.h"
#include "mysql_session.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

#include <chrono>
#include <thread>

Path g_origin_path;
using mysqlrouter::MySQLSession;

class RouterRoutingStrategyTest : public RouterComponentTest {
 protected:
  virtual void SetUp() {
    set_origin(g_origin_path);
    RouterComponentTest::SetUp();

    // Valgrind needs way more time
    if (getenv("WITH_VALGRIND")) {
      wait_for_cache_ready_timeout = 5000;
      wait_for_process_exit_timeout = 20000;
      wait_for_static_ready_timeout = 1000;
    }
  }

  std::string get_metadata_cache_section(unsigned metadata_server_port) {
    return "[metadata_cache:test]\n"
           "router_id=1\n"
           "bootstrap_server_addresses=mysql://localhost:" +
           std::to_string(metadata_server_port) +
           "\n"
           "user=mysql_router1_user\n"
           "metadata_cluster=test\n"
           "ttl=300\n\n";
  }

  std::string get_static_routing_section(
      unsigned router_port, const std::vector<unsigned> &destinations,
      const std::string &strategy, const std::string &mode = "") {
    std::string result =
        "[routing:test_default]\n"
        "bind_port=" +
        std::to_string(router_port) + "\n" + "protocol=classic\n";

    result += "destinations=";
    for (size_t i = 0; i < destinations.size(); ++i) {
      result += "127.0.0.1:" + std::to_string(destinations[i]);
      if (i != destinations.size() - 1) {
        result += ",";
      }
    }
    result += "\n";

    if (!strategy.empty())
      result += std::string("routing_strategy=" + strategy + "\n");
    if (!mode.empty()) result += std::string("mode=" + mode + "\n");

    return result;
  }

  // for error scenarios allow empty values
  std::string get_static_routing_section_error(
      unsigned router_port, const std::vector<unsigned> &destinations,
      const std::string &strategy, const std::string &mode) {
    std::string result =
        "[routing:test_default]\n"
        "bind_port=" +
        std::to_string(router_port) + "\n" + "protocol=classic\n";

    result += "destinations=";
    for (size_t i = 0; i < destinations.size(); ++i) {
      result += "localhost:" + std::to_string(destinations[i]);
      if (i != destinations.size() - 1) {
        result += ",";
      }
    }
    result += "\n";
    result += std::string("routing_strategy=" + strategy + "\n");
    result += std::string("mode=" + mode + "\n");

    return result;
  }

  std::string get_metadata_cache_routing_section(unsigned router_port,
                                                 const std::string &role,
                                                 const std::string &strategy,
                                                 const std::string &mode = "") {
    std::string result =
        "[routing:test_default]\n"
        "bind_port=" +
        std::to_string(router_port) + "\n" +
        "destinations=metadata-cache://test/default?role=" + role + "\n" +
        "protocol=classic\n";

    if (!strategy.empty())
      result += std::string("routing_strategy=" + strategy + "\n");
    if (!mode.empty()) result += std::string("mode=" + mode + "\n");

    return result;
  }

  // need to return void to be able to use ASSERT_ macros
  void connect_client_and_query_port(unsigned router_port,
                                     std::string &out_port,
                                     bool should_fail = false) {
    MySQLSession client;

    if (should_fail) {
      EXPECT_THROW_LIKE(client.connect("127.0.0.1", router_port, "username",
                                       "password", "", ""),
                        std::exception, "Error connecting to MySQL server");
      out_port = "";
      return;
    } else {
      ASSERT_NO_THROW(client.connect("127.0.0.1", router_port, "username",
                                     "password", "", ""));
    }

    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    ASSERT_NE(nullptr, result.get());
    ASSERT_EQ(1u, result->size());
    out_port = std::string((*result)[0]);
  }

  RouterComponentTest::CommandHandle launch_cluster_node(
      unsigned cluster_port, const std::string &data_dir,
      const std::string &tmp_dir) {
    const std::string json_my_port_template =
        Path(data_dir).join("my_port.js").str();
    const std::string json_my_port =
        Path(tmp_dir)
            .join("my_port_" + std::to_string(cluster_port) + ".json")
            .str();
    std::map<std::string, std::string> env_vars = {
        {"MY_PORT", std::to_string(cluster_port)},
    };
    RouterComponentTest::rewrite_js_to_tracefile(json_my_port_template,
                                                 json_my_port, env_vars);
    auto cluster_node = RouterComponentTest::launch_mysql_server_mock(
        json_my_port, cluster_port, false);
    bool ready = wait_for_port_ready(cluster_port, 1000);
    EXPECT_TRUE(ready) << cluster_node.get_full_output();

    return cluster_node;
  }

  RouterComponentTest::CommandHandle launch_standalone_server(
      unsigned server_port, const std::string &data_dir,
      const std::string &tmp_dir) {
    // it' does the same thing, just an alias  for less confusion
    return launch_cluster_node(server_port, data_dir, tmp_dir);
  }

  RouterComponentTest::CommandHandle launch_router_static(
      unsigned router_port, const std::string &routing_section,
      bool expect_error = false, bool log_to_console = true) {
    auto def_section = get_DEFAULT_defaults();
    if (log_to_console) {
      def_section["logging_folder"] = "";
    }
    // launch the router with the static routing configuration
    const std::string conf_file =
        create_config_file(routing_section, &def_section);
    auto router = RouterComponentTest::launch_router("-c " + conf_file);
    if (!expect_error) {
      bool ready = wait_for_port_ready(router_port, 1000);
      EXPECT_TRUE(ready) << (log_to_console ? router.get_full_output()
                                            : get_router_log_output());
    }

    return router;
  }

  RouterComponentTest::CommandHandle launch_router(
      unsigned router_port, const std::string &temp_test_dir,
      const std::string &metadata_cache_section,
      const std::string &routing_section, bool catch_stderr = true,
      bool with_sudo = false, bool wait_ready = true,
      bool log_to_stdout = false) {
    const std::string masterkey_file =
        Path(temp_test_dir).join("master.key").str();
    const std::string keyring_file = Path(temp_test_dir).join("keyring").str();
    mysql_harness::init_keyring(keyring_file, masterkey_file, true);
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store("mysql_router1_user", "password", "root");
    mysql_harness::flush_keyring();
    mysql_harness::reset_keyring();

    // launch the router with metadata-cache configuration
    auto default_section = get_DEFAULT_defaults();
    default_section["keyring_path"] = keyring_file;
    default_section["master_key_path"] = masterkey_file;
    if (log_to_stdout) {
      default_section["logging_folder"] = "";
    }
    const std::string conf_file = create_config_file(
        metadata_cache_section + routing_section, &default_section);
    auto router = RouterComponentTest::launch_router("-c " + conf_file,
                                                     catch_stderr, with_sudo);
    if (wait_ready) {
      bool ready = wait_for_port_ready(router_port, 1000);
      EXPECT_TRUE(ready) << get_router_log_output();
    }

    return router;
  }

  void kill_server(RouterComponentTest::CommandHandle &server) {
    EXPECT_NO_THROW(server.kill()) << server.get_full_output();
  }

  TcpPortPool port_pool_;
  unsigned wait_for_cache_ready_timeout{1000};
  unsigned wait_for_static_ready_timeout{100};
  unsigned wait_for_process_exit_timeout{10000};
};

struct MetadataCacheTestParams {
  std::string role;
  std::string routing_strategy;
  std::string mode;

  // consecutive nodes ids that we expect to be connected to
  std::vector<unsigned> expected_node_connections;
  bool round_robin;

  MetadataCacheTestParams(const std::string &role_,
                          const std::string &routing_strategy_,
                          const std::string &mode_,
                          std::vector<unsigned> expected_node_connections_,
                          bool round_robin_ = false)
      : role(role_),
        routing_strategy(routing_strategy_),
        mode(mode_),
        expected_node_connections(expected_node_connections_),
        round_robin(round_robin_) {}
};

::std::ostream &operator<<(::std::ostream &os,
                           const MetadataCacheTestParams &mcp) {
  return os << "role=" << mcp.role
            << ", routing_strtegy=" << mcp.routing_strategy
            << ", mode=" << mcp.mode;
}

class RouterRoutingStrategyMetadataCache
    : public RouterRoutingStrategyTest,
      public ::testing::TestWithParam<MetadataCacheTestParams> {
 protected:
  virtual void SetUp() { RouterRoutingStrategyTest::SetUp(); }
};

////////////////////////////////////////
/// MATADATA-CACHE ROUTING TESTS
////////////////////////////////////////

TEST_P(RouterRoutingStrategyMetadataCache, MetadataCacheRoutingStrategy) {
  auto test_params = GetParam();

  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  const std::vector<unsigned> cluster_nodes_ports{
      port_pool_.get_next_available(),  // first is PRIMARY
      port_pool_.get_next_available(), port_pool_.get_next_available(),
      port_pool_.get_next_available()};

  std::vector<RouterComponentTest::CommandHandle> cluster_nodes;

  std::map<std::string, std::string> primary_json_env_vars = {
      {"PRIMARY_HOST", "127.0.0.1:" + std::to_string(cluster_nodes_ports[0])},
      {"SECONDARY_1_HOST",
       "127.0.0.1:" + std::to_string(cluster_nodes_ports[1])},
      {"SECONDARY_2_HOST",
       "127.0.0.1:" + std::to_string(cluster_nodes_ports[2])},
      {"SECONDARY_3_HOST",
       "127.0.0.1:" + std::to_string(cluster_nodes_ports[3])},

      {"PRIMARY_PORT", std::to_string(cluster_nodes_ports[0])},
      {"SECONDARY_1_PORT", std::to_string(cluster_nodes_ports[1])},
      {"SECONDARY_2_PORT", std::to_string(cluster_nodes_ports[2])},
      {"SECONDARY_3_PORT", std::to_string(cluster_nodes_ports[3])},

      {"MY_PORT", std::to_string(cluster_nodes_ports[0])},
  };

  // launch the primary node working also as metadata server
  const std::string json_primary_node_template =
      get_data_dir().join("metadata_3_secondaries.js").str();
  const std::string json_primary_node =
      Path(temp_test_dir).join("metadata_3_secondaries.js").str();
  rewrite_js_to_tracefile(json_primary_node_template, json_primary_node,
                          primary_json_env_vars);
  auto primary_node = launch_mysql_server_mock(json_primary_node,
                                               cluster_nodes_ports[0], false);
  bool ready = wait_for_port_ready(cluster_nodes_ports[0], 1000);
  EXPECT_TRUE(ready) << primary_node.get_full_output();
  cluster_nodes.emplace_back(std::move(primary_node));

  // launch the router with metadata-cache configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string metadata_cache_section =
      get_metadata_cache_section(cluster_nodes_ports[0]);
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, test_params.role, test_params.routing_strategy,
      test_params.mode);
  auto router = launch_router(router_port, temp_test_dir,
                              metadata_cache_section, routing_section);

  // launch the secondary cluster nodes
  for (unsigned port = 1; port < cluster_nodes_ports.size(); ++port) {
    auto secondary_node = launch_cluster_node(
        cluster_nodes_ports[port], get_data_dir().str(), temp_test_dir);
    cluster_nodes.emplace_back(std::move(secondary_node));
  }

  // give the router a chance to initialise metadata-cache module
  // there is currently now easy way to check that
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  if (!test_params.round_robin) {
    // check if the server nodes are being used in the expected order
    std::string node_port;
    for (auto expected_node_id : test_params.expected_node_connections) {
      ASSERT_NO_FATAL_FAILURE(
          connect_client_and_query_port(router_port, node_port));
      EXPECT_EQ(std::to_string(cluster_nodes_ports[expected_node_id]),
                node_port);
    }
  } else {
    // for round-robin we can't be sure which server will be the starting one
    // on Solaris wait_for_port_ready() causes the router to switch to the next
    // server while on other OSes it does not. We check it the round robin is
    // done on provided set od ids.
    const auto &expected_nodes = test_params.expected_node_connections;
    std::string node_port;
    size_t first_port_id{0};
    for (size_t i = 0; i < expected_nodes.size() + 1;
         ++i) {  // + 1 to check that after
                 // full round it strats from beginning
      ASSERT_NO_FATAL_FAILURE(
          connect_client_and_query_port(router_port, node_port));
      if (i == 0) {  // first-connection
        const auto &real_port_iter =
            std::find(cluster_nodes_ports.begin(), cluster_nodes_ports.end(),
                      (unsigned)std::atoi(node_port.c_str()));
        ASSERT_NE(real_port_iter, cluster_nodes_ports.end());
        auto port_id_ = real_port_iter - std::begin(cluster_nodes_ports);

        EXPECT_TRUE(std::find(expected_nodes.begin(), expected_nodes.end(),
                              port_id_) != expected_nodes.end());
        first_port_id =
            std::find(expected_nodes.begin(), expected_nodes.end(), port_id_) -
            expected_nodes.begin();
      } else {
        const auto current_idx = (first_port_id + i) % expected_nodes.size();
        const auto expected_node_id = expected_nodes[current_idx];
        EXPECT_EQ(std::to_string(cluster_nodes_ports[expected_node_id]),
                  node_port);
      }
    }
  }

  ASSERT_THAT(router.kill(), testing::Eq(0));
}

INSTANTIATE_TEST_CASE_P(
    MetadataCacheRoutingStrategy, RouterRoutingStrategyMetadataCache,
    // node_id=0 is PRIARY, node_id=1..3 are SECONDARY
    ::testing::Values(
        // test round-robin on SECONDARY servers
        // we expect 1->2->3->1 for 4 consecutive connections
        MetadataCacheTestParams("SECONDARY", "round-robin", "", {1, 2, 3},
                                /*round-robin=*/true),

        // test first-available on SECONDARY servers
        // we expect 1->1->1 for 3 consecutive connections
        MetadataCacheTestParams("SECONDARY", "first-available", "", {1, 1, 1}),

        // *basic* test round-robin-with-fallback
        // we expect 1->2->3->1 for 4 consecutive connections
        // as there are SECONDARY servers available (PRIMARY id=0 should not be
        // used)
        MetadataCacheTestParams("SECONDARY", "round-robin-with-fallback", "",
                                {1, 2, 3},
                                /*round-robin=*/true),

        // test round-robin on PRIMARY_AND_SECONDARY
        // we expect the primary to participate in the round-robin from the
        // beginning we expect 0->1->2->3->0 for 5 consecutive connections
        MetadataCacheTestParams("PRIMARY_AND_SECONDARY", "round-robin", "",
                                {0, 1, 2, 3},
                                /*round-robin=*/true),

        // test round-robin with allow-primary-reads=yes
        // this should work similar to PRIMARY_AND_SECONDARY
        // we expect 0->1->2->3->0 for 5 consecutive connections
        MetadataCacheTestParams("SECONDARY&allow_primary_reads=yes", "",
                                "read-only", {0, 1, 2, 3},
                                /*round-robin=*/true),

        // test first-available on PRIMARY
        // we expect 0->0->0 for 2 consecutive connections
        MetadataCacheTestParams("PRIMARY", "first-available", "", {0, 0}),

        // test round-robin on PRIMARY
        // there is single primary so we expect 0->0->0 for 2 consecutive
        // connections
        MetadataCacheTestParams("PRIMARY", "round-robin", "", {0, 0})));

////////////////////////////////////////
/// STATIC ROUTING TESTS
////////////////////////////////////////

class RouterRoutingStrategyTestRoundRobin
    : public RouterRoutingStrategyTest,
      // r. strategy, mode
      public ::testing::TestWithParam<std::pair<std::string, std::string>> {
 protected:
  virtual void SetUp() { RouterRoutingStrategyTest::SetUp(); }
};

TEST_P(RouterRoutingStrategyTestRoundRobin, StaticRoutingStrategyRoundRobin) {
  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  const std::vector<unsigned> server_ports{port_pool_.get_next_available(),
                                           port_pool_.get_next_available(),
                                           port_pool_.get_next_available()};

  // launch the standalone servers
  std::vector<RouterComponentTest::CommandHandle> server_instances;
  for (auto &server_port : server_ports) {
    auto secondary_node = launch_standalone_server(
        server_port, get_data_dir().str(), temp_test_dir);
    server_instances.emplace_back(std::move(secondary_node));
  }

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();

  const auto routing_strategy = GetParam().first;
  const auto mode = GetParam().second;
  const std::string routing_section = get_static_routing_section(
      router_port, server_ports, routing_strategy, mode);
  auto router = launch_router_static(router_port, routing_section);

  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_static_ready_timeout));

  // expect consecutive connections to be done in round-robin fashion
  // will start with the second because wait_for_port_ready on the router will
  // cause it to switch
  std::string node_port;
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[1]), node_port);
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[2]), node_port);
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[0]), node_port);
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[1]), node_port);
}

// We expect round robin for routing-strategy=round-robin and as default for
// read-only
INSTANTIATE_TEST_CASE_P(
    StaticRoutingStrategyRoundRobin, RouterRoutingStrategyTestRoundRobin,
    ::testing::Values(
        std::make_pair(std::string("round-robin"), std::string("")),
        std::make_pair(std::string("round-robin"), std::string("read-only")),
        std::make_pair(std::string("round-robin"), std::string("read-write")),
        std::make_pair(std::string(""), std::string("read-only"))));

class RouterRoutingStrategyTestFirstAvailable
    : public RouterRoutingStrategyTest,
      // r. strategy, mode
      public ::testing::TestWithParam<std::pair<std::string, std::string>> {
 protected:
  virtual void SetUp() { RouterRoutingStrategyTest::SetUp(); }
};

TEST_P(RouterRoutingStrategyTestFirstAvailable,
       StaticRoutingStrategyFirstAvailable) {
  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  const std::vector<unsigned> server_ports{port_pool_.get_next_available(),
                                           port_pool_.get_next_available(),
                                           port_pool_.get_next_available()};

  // launch the standalone servers
  std::vector<RouterComponentTest::CommandHandle> server_instances;
  for (auto &server_port : server_ports) {
    auto secondary_node = launch_standalone_server(
        server_port, get_data_dir().str(), temp_test_dir);
    server_instances.emplace_back(std::move(secondary_node));
  }

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();

  const auto routing_strategy = GetParam().first;
  const auto mode = GetParam().second;
  const std::string routing_section = get_static_routing_section(
      router_port, server_ports, routing_strategy, mode);
  auto router = launch_router_static(router_port, routing_section);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // expect consecutive connections to be done in first-available fashion
  std::string node_port;
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[0]), node_port);
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[0]), node_port);

  // "kill" server 1 and 2, expect moving to server 3
  kill_server(server_instances[0]);
  kill_server(server_instances[1]);
  // now we should connect to 3rd server
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[2]), node_port);

  // kill also 3rd server
  kill_server(server_instances[2]);
  // expect connection failure
  connect_client_and_query_port(router_port, node_port, /*should_fail=*/true);
  EXPECT_EQ("", node_port);

  // bring back 1st server
  server_instances.emplace_back(launch_standalone_server(
      server_ports[0], get_data_dir().str(), temp_test_dir));
  // we should now succesfully connect to this server
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[0]), node_port);
}

// We expect first-available for routing-strategy=first-available and as default
// for read-write
INSTANTIATE_TEST_CASE_P(
    StaticRoutingStrategyFirstAvailable,
    RouterRoutingStrategyTestFirstAvailable,
    ::testing::Values(
        std::make_pair(std::string("first-available"), std::string("")),
        std::make_pair(std::string("first-available"),
                       std::string("read-write")),
        std::make_pair(std::string("first-available"),
                       std::string("read-only")),
        std::make_pair(std::string(""), std::string("read-write"))));

// for non-param tests
class RouterRoutingStrategyStatic : public RouterRoutingStrategyTest,
                                    public ::testing::Test {
 protected:
  virtual void SetUp() { RouterRoutingStrategyTest::SetUp(); }
};

TEST_F(RouterRoutingStrategyStatic, StaticRoutingStrategyNextAvailable) {
  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  const std::vector<unsigned> server_ports{port_pool_.get_next_available(),
                                           port_pool_.get_next_available(),
                                           port_pool_.get_next_available()};

  // launch the standalone servers
  std::vector<RouterComponentTest::CommandHandle> server_instances;
  for (auto &server_port : server_ports) {
    auto secondary_node = launch_standalone_server(
        server_port, get_data_dir().str(), temp_test_dir);
    server_instances.emplace_back(std::move(secondary_node));
  }

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section =
      get_static_routing_section(router_port, server_ports, "next-available");
  auto router = launch_router_static(router_port, routing_section);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // expect consecutive connections to be done in first-available fashion
  std::string node_port;
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[0]), node_port);
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[0]), node_port);

  // "kill" server 1 and 2, expect connection to server 3 after that
  kill_server(server_instances[0]);
  kill_server(server_instances[1]);
  // now we should connect to 3rd server
  connect_client_and_query_port(router_port, node_port);
  EXPECT_EQ(std::to_string(server_ports[2]), node_port);

  // kill also 3rd server
  kill_server(server_instances[2]);
  // expect connection failure
  connect_client_and_query_port(router_port, node_port, /*should_fail=*/true);
  EXPECT_EQ("", node_port);

  // bring back 1st server
  server_instances.emplace_back(launch_standalone_server(
      server_ports[0], get_data_dir().str(), temp_test_dir));
  // we should NOT connect to this server (in next-available we NEVER go back)
  connect_client_and_query_port(router_port, node_port, /*should_fail=*/true);
  EXPECT_EQ("", node_port);
}

// configuration error scenarios

TEST_F(RouterRoutingStrategyStatic, InvalidStrategyName) {
  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section = get_static_routing_section_error(
      router_port, {1, 2}, "round-robin-with-fallback", "read-only");
  auto router =
      launch_router_static(router_port, routing_section, /*expect_error=*/true);

  EXPECT_EQ(router.wait_for_exit(wait_for_process_exit_timeout), 1);
  EXPECT_TRUE(
      router.expect_output("Configuration error: option routing_strategy in "
                           "[routing:test_default] is invalid; "
                           "valid are first-available, next-available, and "
                           "round-robin (was 'round-robin-with-fallback'"))
      << get_router_log_output();
}

TEST_F(RouterRoutingStrategyStatic, InvalidMode) {
  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section = get_static_routing_section_error(
      router_port, {1, 2}, "invalid", "read-only");
  auto router =
      launch_router_static(router_port, routing_section, /*expect_error=*/true);

  EXPECT_EQ(router.wait_for_exit(wait_for_process_exit_timeout), 1);
  EXPECT_TRUE(router.expect_output(
      "option routing_strategy in [routing:test_default] is invalid; valid are "
      "first-available, next-available, and round-robin (was 'invalid')"))
      << get_router_log_output();
}

TEST_F(RouterRoutingStrategyStatic, BothStrategyAndModeMissing) {
  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section =
      get_static_routing_section(router_port, {1, 2}, "");
  auto router =
      launch_router_static(router_port, routing_section, /*expect_error=*/true);

  EXPECT_EQ(router.wait_for_exit(wait_for_process_exit_timeout), 1);
  EXPECT_TRUE(
      router.expect_output("Configuration error: option routing_strategy in "
                           "[routing:test_default] is required"))
      << get_router_log_output();
}

TEST_F(RouterRoutingStrategyStatic, RoutingSrtategyEmptyValue) {
  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section =
      get_static_routing_section_error(router_port, {1, 2}, "", "read-only");
  auto router =
      launch_router_static(router_port, routing_section, /*expect_error=*/true);

  EXPECT_EQ(router.wait_for_exit(wait_for_process_exit_timeout), 1);
  EXPECT_TRUE(
      router.expect_output("Configuration error: option routing_strategy in "
                           "[routing:test_default] needs a value"))
      << get_router_log_output();
}

TEST_F(RouterRoutingStrategyStatic, ModeEmptyValue) {
  const std::string temp_test_dir = get_tmp_dir();
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { purge_dir(temp_test_dir); });

  // launch the router with the static configuration
  const auto router_port = port_pool_.get_next_available();
  const std::string routing_section = get_static_routing_section_error(
      router_port, {1, 2}, "first-available", "");
  auto router =
      launch_router_static(router_port, routing_section, /*expect_error=*/true);

  EXPECT_EQ(router.wait_for_exit(wait_for_process_exit_timeout), 1);
  EXPECT_TRUE(
      router.expect_output("Configuration error: option mode in "
                           "[routing:test_default] needs a value"))
      << get_router_log_output();
}

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
