/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include "dim.h"
#include "keyring/keyring_manager.h"
#include "mysql_session.h"
#include "random_generator.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

#include "mysqlrouter/rest_client.h"
#include "rapidjson/document.h"

#include "gmock/gmock.h"

#include <fstream>
#include <thread>

Path g_origin_path;
using mysqlrouter::MySQLSession;

const std::string kMockServerGlobalsRestUri = "/api/v1/mock_server/globals/";

class ConfigGenerator {
  std::map<std::string, std::string> defaults_;
  std::string config_dir_;

  std::string metadata_cache_section_;
  std::string routing_primary_section_;
  std::string routing_secondary_section_;

  std::vector<unsigned> metadata_server_ports_;
  unsigned router_rw_port_;
  unsigned router_ro_port_;
  std::string disconnect_on_metadata_unavailable_ =
      "&disconnect_on_metadata_unavailable=no";
  std::string disconnect_on_promoted_to_primary_ =
      "&disconnect_on_promoted_to_primary=no";
  unsigned metadata_refresh_ttl_ = 1;

 public:
  ConfigGenerator(const std::map<std::string, std::string> &defaults,
                  const std::string &config_dir,
                  const std::vector<unsigned> metadata_server_ports,
                  unsigned router_rw_port, unsigned router_ro_port)
      : defaults_(defaults),
        config_dir_(config_dir),
        metadata_server_ports_(metadata_server_ports),
        router_rw_port_(router_rw_port),
        router_ro_port_(router_ro_port) {}

  void disconnect_on_metadata_unavailable(const std::string &value) {
    disconnect_on_metadata_unavailable_ = value;
  }

  void disconnect_on_promoted_to_primary(const std::string &value) {
    disconnect_on_promoted_to_primary_ = value;
  }

  void metadata_refresh_ttl(unsigned ttl) { metadata_refresh_ttl_ = ttl; }

  void add_metadata_cache_section(unsigned ttl = 1) {
    metadata_cache_section_ =
        "[logger]\n"
        "level = INFO\n\n"

        "[metadata_cache:test]\n"
        "router_id=1\n"
        "bootstrap_server_addresses=";
    unsigned i = 0;
    for (unsigned port : metadata_server_ports_) {
      metadata_cache_section_ += "mysql://127.0.0.1:" + std::to_string(port);
      if (i < metadata_server_ports_.size() - 1) metadata_cache_section_ += ",";
    }
    metadata_cache_section_ +=
        "\n"
        "user=mysql_router1_user\n"
        "metadata_cluster=test\n"
        "ttl=" +
        std::to_string(ttl) + "\n\n";
  }

  std::string get_metadata_cache_routing_section(unsigned router_port,
                                                 const std::string &role,
                                                 const std::string &strategy,
                                                 bool is_rw = false) {
    std::string result;

    if (is_rw) {
      result =
          "[routing:test_default_rw]\n"
          "bind_port=" +
          std::to_string(router_port) + "\n" +
          "destinations=metadata-cache://test/default?role=" + role +
          disconnect_on_metadata_unavailable_ + "\n" + "protocol=classic\n";
    } else {
      result =
          "[routing:test_default_ro]\n"
          "bind_port=" +
          std::to_string(router_port) + "\n" +
          "destinations=metadata-cache://test/default?role=" + role +
          disconnect_on_metadata_unavailable_ +
          disconnect_on_promoted_to_primary_ + "\n" + "protocol=classic\n";
    }

    if (!strategy.empty())
      result += std::string("routing_strategy=" + strategy + "\n");

    return result;
  }

  void add_routing_primary_section() {
    routing_primary_section_ = get_metadata_cache_routing_section(
        router_rw_port_, "PRIMARY", "round-robin", true);
  }

  void add_routing_secondary_section() {
    routing_secondary_section_ = get_metadata_cache_routing_section(
        router_ro_port_, "SECONDARY", "round-robin", false);
  }

  void add_routing_primary_and_secondary_section() {
    routing_secondary_section_ = get_metadata_cache_routing_section(
        router_ro_port_, "PRIMARY_AND_SECONDARY", "round-robin", false);
  }

  std::string make_DEFAULT_section(
      const std::map<std::string, std::string> *params) {
    auto l = [params](const char *key) -> std::string {
      return (params->count(key))
                 ? std::string(key) + " = " + params->at(key) + "\n"
                 : "";
    };

    return std::string("[DEFAULT]\n") + l("logging_folder") +
           l("plugin_folder") + l("runtime_folder") + l("config_folder") +
           l("data_folder") + l("keyring_path") + l("master_key_path") +
           l("master_key_reader") + l("master_key_writer") + "\n";
  }

  std::string create_config_file(
      const std::map<std::string, std::string> *params,
      const std::string &directory) {
    Path file_path = Path(directory).join("mysqlrouter.conf");
    std::ofstream ofs_config(file_path.str());

    if (!ofs_config.good()) {
      throw(std::runtime_error("Could not create config file " +
                               file_path.str()));
    }

    ofs_config << make_DEFAULT_section(params);
    ofs_config << metadata_cache_section_ << routing_primary_section_
               << routing_secondary_section_ << std::endl;
    ofs_config.close();

    return file_path.str();
  }

  void set_keyring(const std::string &temp_test_dir) {
    const std::string masterkey_file =
        Path(temp_test_dir).join("master.key").str();
    const std::string keyring_file = Path(temp_test_dir).join("keyring").str();
    mysql_harness::init_keyring(keyring_file, masterkey_file, true);
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store("mysql_router1_user", "password", "root");
    mysql_harness::flush_keyring();
    mysql_harness::reset_keyring();

    // launch the router with metadata-cache configuration
    defaults_["keyring_path"] = keyring_file;
    defaults_["master_key_path"] = masterkey_file;
  }

  std::string build_config_file(const std::string &temp_test_dir,
                                bool is_primary_and_secondary = false) {
    add_metadata_cache_section(metadata_refresh_ttl_);
    add_routing_primary_section();

    if (is_primary_and_secondary) {
      add_routing_primary_and_secondary_section();
    } else {
      add_routing_secondary_section();
    }

    set_keyring(temp_test_dir);

    return create_config_file(&defaults_, config_dir_);
  }
};

class RouterRoutingConnectionCommonTest : public RouterComponentTest {
 public:
  void init() {
    set_origin(g_origin_path);
    RouterComponentTest::SetUp();

    mysql_harness::DIM &dim = mysql_harness::DIM::instance();
    // RandomGenerator
    dim.set_RandomGenerator(
        []() {
          static mysql_harness::RandomGenerator rg;
          return &rg;
        },
        [](mysql_harness::RandomGeneratorInterface *) {});
    temp_test_dir_ = get_tmp_dir();
    temp_conf_dir_ = get_tmp_dir("conf");

    cluster_nodes_ports_ = {
        port_pool_.get_next_available(),  // first is PRIMARY
        port_pool_.get_next_available(), port_pool_.get_next_available(),
        port_pool_.get_next_available(), port_pool_.get_next_available()};

    router_rw_port_ = port_pool_.get_next_available();
    router_ro_port_ = port_pool_.get_next_available();

    primary_json_env_vars_ = {
        {"PRIMARY_HOST",
         "127.0.0.1:" + std::to_string(cluster_nodes_ports_[0])},
        {"SECONDARY_1_HOST",
         "127.0.0.1:" + std::to_string(cluster_nodes_ports_[1])},
        {"SECONDARY_2_HOST",
         "127.0.0.1:" + std::to_string(cluster_nodes_ports_[2])},
        {"SECONDARY_3_HOST",
         "127.0.0.1:" + std::to_string(cluster_nodes_ports_[3])},
        {"SECONDARY_4_HOST",
         "127.0.0.1:" + std::to_string(cluster_nodes_ports_[4])},

        {"PRIMARY_PORT", std::to_string(cluster_nodes_ports_[0])},
        {"SECONDARY_1_PORT", std::to_string(cluster_nodes_ports_[1])},
        {"SECONDARY_2_PORT", std::to_string(cluster_nodes_ports_[2])},
        {"SECONDARY_3_PORT", std::to_string(cluster_nodes_ports_[3])},
        {"SECONDARY_4_PORT", std::to_string(cluster_nodes_ports_[4])},

    };

    config_generator_.reset(new ConfigGenerator(
        get_DEFAULT_defaults(), temp_conf_dir_, {cluster_nodes_ports_[0]},
        router_rw_port_, router_ro_port_));

    http_port_ = port_pool_.get_next_available();
    http_hostname_ = "127.0.0.1";
    http_uri_ = kMockServerGlobalsRestUri;
  }

  void clean() {
    purge_dir(temp_test_dir_);
    purge_dir(temp_conf_dir_);
  }

  RouterComponentTest::CommandHandle launch_router(
      unsigned /* router_port */, const std::string &config_file) {
    return RouterComponentTest::launch_router("-c " + config_file);
  }

  RouterComponentTest::CommandHandle launch_server(unsigned cluster_port,
                                                   const std::string &json_file,
                                                   unsigned http_port = 0) {
    auto cluster_node = RouterComponentTest::launch_mysql_server_mock(
        json_file, cluster_port, false, http_port);
    return cluster_node;
  }

  std::string get_json_for_secondary(unsigned /* cluster_port*/) {
    const std::string json_my_port_template =
        get_data_dir().join("rest_server_mock.js").str();
    return json_my_port_template;
  }

  std::string replace_env_variables(
      const std::string &js_file_name,
      const std::map<std::string, std::string> &primary_json_env_vars,
      unsigned /* my_port*/) {
    const std::string json_primary_node_template =
        get_data_dir().join(js_file_name).str();
    const std::string json_primary_node =
        Path(temp_test_dir_).join(js_file_name).str();
    std::map<std::string, std::string> env_vars = primary_json_env_vars;

    rewrite_js_to_tracefile(json_primary_node_template, json_primary_node,
                            env_vars);
    return json_primary_node;
  }

  void setup_cluster(const std::string &js_for_primary,
                     unsigned number_of_servers, unsigned my_port = 0) {
    // launch the primary node working also as metadata server
    const std::string json_for_primary =
        replace_env_variables(js_for_primary, primary_json_env_vars_, my_port);

    cluster_nodes_.push_back(
        launch_server(cluster_nodes_ports_[0], json_for_primary, http_port_));

    // launch the secondary cluster nodes
    for (unsigned port = 1; port < number_of_servers; ++port) {
      std::string secondary_json_file =
          get_json_for_secondary(cluster_nodes_ports_[port]);
      cluster_nodes_.push_back(
          launch_server(cluster_nodes_ports_[port], secondary_json_file));
    }

    for (unsigned ndx = 0; ndx < number_of_servers; ++ndx) {
      ASSERT_TRUE(wait_for_port_ready(cluster_nodes_ports_[ndx], 1000))
          << cluster_nodes_.at(ndx).get_full_output();
    }
  }

  void set_global(const std::string &global) { set_global(global, http_port_); }

  void set_global(const std::string &global, unsigned http_port) {
    IOContext io_ctx;
    RestClient rest_client(io_ctx, http_hostname_, http_port);
    auto put_req = rest_client.request_sync(HttpMethod::Put, http_uri_, global);

    ASSERT_TRUE(put_req) << "HTTP Request to " << http_hostname_ << ":"
                         << std::to_string(http_port)
                         << " failed (early): " << put_req.error_msg()
                         << std::endl;

    ASSERT_GT(put_req.get_response_code(), 0u)
        << "HTTP Request to " << http_hostname_ << ":"
        << std::to_string(http_port) << " failed: " << put_req.error_msg()
        << std::endl;

    EXPECT_EQ(put_req.get_response_code(), 204u);

    auto put_resp_body = put_req.get_input_buffer();
    EXPECT_EQ(put_resp_body.length(), 0u);
  }

  void send_delete(unsigned http_port, int node_id) {
    IOContext io_ctx;
    RestClient rest_client(io_ctx, http_hostname_, http_port);
    auto kill_req = rest_client.request_sync(
        HttpMethod::Delete, "/api/v1/mock_server/connections/");

    ASSERT_TRUE(kill_req) << "HTTP Request to " << http_hostname_ << ":"
                          << std::to_string(http_port)
                          << " failed (early): " << kill_req.error_msg()
                          << std::endl
                          << cluster_nodes_[node_id].get_full_output()
                          << std::endl;

    ASSERT_GT(kill_req.get_response_code(), 0u)
        << "HTTP Request to " << http_hostname_ << ":"
        << std::to_string(http_port) << " failed: " << kill_req.error_msg()
        << std::endl
        << cluster_nodes_[node_id].get_full_output() << std::endl;

    EXPECT_EQ(kill_req.get_response_code(), 200u);

    auto put_resp_body = kill_req.get_input_buffer();
    EXPECT_EQ(put_resp_body.length(), 0u);
  }

  rapidjson::Document get_global_json(const std::string &global_name) {
    IOContext io_ctx;
    auto req_get = RestClient(io_ctx, http_hostname_, http_port_)
                       .request_sync(HttpMethod::Get, http_uri_);
    EXPECT_TRUE(req_get);
    EXPECT_EQ(req_get.get_response_code(), 200u);
    EXPECT_THAT(req_get.get_input_headers().get("Content-Type"),
                ::testing::StrEq("application/json"));
    auto resp_body = req_get.get_input_buffer();
    EXPECT_GT(resp_body.length(), 0u);
    auto resp_body_content = resp_body.pop_front(resp_body.length());

    // parse json
    std::string json_payload(resp_body_content.begin(),
                             resp_body_content.end());

    rapidjson::Document json_doc;
    json_doc.Parse(json_payload.c_str());
    EXPECT_TRUE(json_doc.HasMember(global_name.c_str()));
    EXPECT_TRUE(json_doc[global_name.c_str()].IsInt());
    return json_doc;
  }

  int get_int_global(const std::string &global_name) {
    return get_global_json(global_name)[global_name.c_str()].GetInt();
  }

  bool get_bool_global(const std::string &global_name) {
    return get_global_json(global_name)[global_name.c_str()].GetBool();
  }

  TcpPortPool port_pool_;
  unsigned wait_for_cache_ready_timeout{1200};
  unsigned wait_for_cache_update_timeout{2000};
  std::unique_ptr<ConfigGenerator> config_generator_;
  std::string temp_test_dir_;
  std::string temp_conf_dir_;
  std::vector<unsigned> cluster_nodes_ports_;
  std::map<std::string, std::string> primary_json_env_vars_;
  std::vector<RouterComponentTest::CommandHandle> cluster_nodes_;
  unsigned router_rw_port_;
  unsigned router_ro_port_;

  // http properties
  unsigned http_port_;
  std::string http_hostname_;
  std::string http_uri_;
};

class RouterRoutingConnectionTest : public RouterRoutingConnectionCommonTest,
                                    public testing::Test {
 public:
  void SetUp() override { init(); }

  void TearDown() override { clean(); }
};

static std::vector<std::string> vec_from_lines(const std::string &s) {
  std::vector<std::string> lines;
  std::istringstream lines_stream{s};

  for (std::string line; std::getline(lines_stream, line);) {
    lines.push_back(line);
  }

  return lines;
}

/**
 * @test
 *      Verify connections through router fail if metadata's schema-version is
 * too old.
 */
TEST_F(RouterRoutingConnectionTest, OldSchemaVersion) {
  // preparation
  //
  SCOPED_TRACE("// [prep] creating router config");
  config_generator_.reset(new ConfigGenerator(
      get_DEFAULT_defaults(), get_tmp_dir("conf"), {cluster_nodes_ports_[0]},
      router_rw_port_, router_ro_port_));

  unsigned http_port_primary = port_pool_.get_next_available();

  SCOPED_TRACE("// [prep] launch the primary node on port " +
               std::to_string(cluster_nodes_ports_.at(0)) +
               " working also as metadata server");
  const std::string json_for_primary =
      replace_env_variables("metadata_old_schema.js", primary_json_env_vars_,
                            cluster_nodes_ports_[0]);
  cluster_nodes_.push_back(launch_server(cluster_nodes_ports_.at(0),
                                         json_for_primary, http_port_primary));

  SCOPED_TRACE("// [prep] wait until mock-servers are started");
  ASSERT_TRUE(wait_for_port_ready(cluster_nodes_ports_.at(0), 1000))
      << cluster_nodes_.at(0).get_full_output();

  SCOPED_TRACE("// [prep] launching router");
  auto router = launch_router(
      router_rw_port_, config_generator_->build_config_file(temp_test_dir_));
  ASSERT_TRUE(wait_for_port_ready(router_rw_port_, 1000))
      << router.get_full_output();

  SCOPED_TRACE("// [prep] waiting " +
               std::to_string(wait_for_cache_ready_timeout) +
               "ms until metadata is initialized");
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  // testing
  //
  SCOPED_TRACE("// [test] expect connecting clients to fail");
  MySQLSession client;

  ASSERT_THROW(client.connect("127.0.0.1", router_rw_port_, "username",
                              "password", "", ""),
               MySQLSession::Error);

  SCOPED_TRACE("// [test] expect router log to contain error message");

  // posix RE has [0-9], but no \\d
  // simple RE has \\d, but no [0-9]
  constexpr const char log_msg_re[]{
#ifdef GTEST_USES_POSIX_RE
      "Unsupported metadata schema on .*\\. Expected Metadata Schema version "
      "compatible to [0-9]\\.[0-9]\\.[0-9], got 0\\.0\\.0"
#else
      "Unsupported metadata schema on .*\\. Expected Metadata Schema version "
      "compatible to \\d\\.\\d\\.\\d, got 0\\.0\\.0"
#endif
  };

  ASSERT_THAT(vec_from_lines(get_router_log_output()),
              ::testing::Contains(::testing::ContainsRegex(log_msg_re)));
}

/**
 * @test
 *      Verify that router doesn't start when disconnect_on_promoted_to_primary
 *      has invalid value.
 */
TEST_F(RouterRoutingConnectionTest,
       IsRouterFailToStartWhen_disconnect_on_promoted_to_primary_invalid) {
  ASSERT_NO_FATAL_FAILURE(setup_cluster(
      "metadata_3_secondaries_server_removed_from_cluster.js", 4));
  config_generator_->disconnect_on_promoted_to_primary(
      "&disconnect_on_promoted_to_primary=bogus");
  auto router = RouterComponentTest::launch_router(
      "-c " + config_generator_->build_config_file(temp_test_dir_));
  ASSERT_FALSE(wait_for_port_ready(router_ro_port_, 1000))
      << router.get_full_output();
}

/**
 * @test
 *      Verify that router doesn't start when disconnect_on_metadata_unavailable
 *      has invalid value.
 */
TEST_F(RouterRoutingConnectionTest,
       IsRouterFailToStartWhen_disconnect_on_metadata_unavailable_invalid) {
  ASSERT_NO_FATAL_FAILURE(setup_cluster(
      "metadata_3_secondaries_server_removed_from_cluster.js", 4));
  config_generator_->disconnect_on_metadata_unavailable(
      "&disconnect_on_metadata_unavailable=bogus");
  auto router = RouterComponentTest::launch_router(
      "-c " + config_generator_->build_config_file(temp_test_dir_));
  ASSERT_FALSE(wait_for_port_ready(router_ro_port_, 1000))
      << router.get_full_output();
}

/**
 * @test
 *      Verify that all connections to Primary are closed when Primary is
 * removed from GR;
 */
TEST_F(RouterRoutingConnectionTest,
       IsConnectionsClosedWhenPrimaryRemovedFromGR) {
  config_generator_.reset(
      new ConfigGenerator(get_DEFAULT_defaults(), get_tmp_dir("conf"),
                          {cluster_nodes_ports_[0], cluster_nodes_ports_[1]},
                          router_rw_port_, router_ro_port_));

  unsigned http_port_primary = port_pool_.get_next_available();

  SCOPED_TRACE("// launch the primary node on port " +
               std::to_string(cluster_nodes_ports_.at(0)) +
               " working also as metadata server");
  const std::string json_for_primary = replace_env_variables(
      "metadata_3_secondaries_server_removed_from_cluster.js",
      primary_json_env_vars_, cluster_nodes_ports_[0]);
  cluster_nodes_.push_back(launch_server(cluster_nodes_ports_[0],
                                         json_for_primary, http_port_primary));

  SCOPED_TRACE("// launch the secondary node on port " +
               std::to_string(cluster_nodes_ports_.at(1)) +
               " working also as metadata server");
  const std::string json_for_secondary = replace_env_variables(
      "metadata_3_secondaries_server_removed_from_cluster.js",
      primary_json_env_vars_, cluster_nodes_ports_[1]);
  cluster_nodes_.push_back(
      launch_server(cluster_nodes_ports_[1], json_for_secondary, http_port_));

  SCOPED_TRACE("// launch the rest of secondary cluster nodes");
  for (unsigned port = 2; port < 4; ++port) {
    std::string secondary_json_file =
        get_json_for_secondary(cluster_nodes_ports_[port]);
    cluster_nodes_.push_back(
        launch_server(cluster_nodes_ports_[port], secondary_json_file));
  }

  SCOPED_TRACE("// wait until mock-servers are started");
  for (unsigned ndx = 0; ndx < 4; ndx++) {
    ASSERT_TRUE(wait_for_port_ready(cluster_nodes_ports_[ndx], 1000))
        << cluster_nodes_.at(ndx).get_full_output();
  }

  SCOPED_TRACE("// launching router");
  auto router = launch_router(
      router_rw_port_, config_generator_->build_config_file(temp_test_dir_));
  ASSERT_TRUE(wait_for_port_ready(router_rw_port_, 1000))
      << router.get_full_output();

  SCOPED_TRACE("// waiting " + std::to_string(wait_for_cache_ready_timeout) +
               "ms until metadata is initialized");
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  SCOPED_TRACE("// connecting clients");
  std::vector<std::pair<MySQLSession, unsigned>> clients(2);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    try {
      client.connect("127.0.0.1", router_rw_port_, "username", "password", "",
                     "");
    } catch (const std::exception &e) {
      FAIL() << e.what() << "\n"
             << "router: " << router.get_full_output() << "\n"
             << "cluster[0]: " << cluster_nodes_.at(0).get_full_output() << "\n"
             << "cluster[1]: " << cluster_nodes_.at(1).get_full_output() << "\n"
             << "cluster[2]: " << cluster_nodes_.at(2).get_full_output() << "\n"
             << "cluster[3]: " << cluster_nodes_.at(3).get_full_output()
             << "\n";
    }
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second = std::stoul(std::string((*result)[0]));
  }

  // set primary_removed variable in javascript for cluster_nodes_[0]
  // which acts as primary node in cluster
  set_global("{\"primary_removed\": true}", http_port_primary);
  // set primary_removed variable in javascript for cluster_nodes_[1]
  // which uses default port http_port_
  set_global("{\"primary_removed\": true}", http_port_);

  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_update_timeout));

  // verify that connections to PRIMARY are broken
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_ANY_THROW(client.query_one("select @@port"))
        << router.get_full_output();
  }
}

/**
 * @test
 *      Verify that all connections to Secondary are closed when Secondary is
 *      removed from GR.
 *
 */
TEST_F(RouterRoutingConnectionTest,
       IsConnectionsClosedWhenSecondaryRemovedFromGR) {
  ASSERT_NO_FATAL_FAILURE(setup_cluster(
      "metadata_3_secondaries_server_removed_from_cluster.js", 4));

  auto router = launch_router(
      router_rw_port_, config_generator_->build_config_file(temp_test_dir_));
  ASSERT_TRUE(wait_for_port_ready(router_rw_port_, 1000))
      << router.get_full_output();

  /*
   * wait until metadata is initialized
   */
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  // connect clients
  std::vector<std::pair<MySQLSession, unsigned>> clients(6);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second = std::stoul(std::string((*result)[0]));
  }

  set_global("{\"secondary_removed\": true}");

  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_update_timeout));

  // verify that connections to SECONDARY_1 are broken
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    unsigned port = client_and_port.second;

    if (port == cluster_nodes_ports_[1])
      ASSERT_ANY_THROW(client.query_one("select @@port"))
          << router.get_full_output();
    else
      ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
          client.query_one("select @@port")})
          << router.get_full_output();
  }
}

/**
 * @test
 *       Verify that when Primary is demoted, then all RW connections
 *       to that server are closed.
 */
TEST_F(RouterRoutingConnectionTest, IsRWConnectionsClosedWhenPrimaryFailover) {
  ASSERT_NO_FATAL_FAILURE(
      setup_cluster("metadata_3_secondaries_primary_failover.js", 4));
  auto router = launch_router(
      router_ro_port_, config_generator_->build_config_file(temp_test_dir_));
  ASSERT_TRUE(wait_for_port_ready(router_rw_port_, 1000))
      << router.get_full_output();

  /*
   * wait until metadata is initialized
   */
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  // connect clients
  std::vector<std::pair<MySQLSession, unsigned>> clients(2);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_rw_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second = std::stoul(std::string((*result)[0]));
  }

  set_global("{\"primary_failover\": true}");

  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_update_timeout));

  // verify if RW connections to PRIMARY are closed
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_ANY_THROW(client.query_one("select @@port"))
        << router.get_full_output();
  }
}

/**
 * @test
 *       Verify that when Primary is demoted, then RO connections
 *       to that server are kept.
 */
TEST_F(RouterRoutingConnectionTest, IsROConnectionsKeptWhenPrimaryFailover) {
  ASSERT_NO_FATAL_FAILURE(
      setup_cluster("metadata_3_secondaries_primary_failover.js", 4));

  config_generator_->disconnect_on_promoted_to_primary("");

  auto router =
      launch_router(router_ro_port_,
                    config_generator_->build_config_file(temp_test_dir_, true));
  ASSERT_TRUE(wait_for_port_ready(router_rw_port_, 1000))
      << router.get_full_output();

  /*
   * wait until metadata is initialized
   */
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  // connect clients
  std::vector<std::pair<MySQLSession, unsigned>> clients(4);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second = std::stoul(std::string((*result)[0]));
  }

  set_global("{\"primary_failover\": true}");
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_update_timeout));

  // verify that RO connections to PRIMARY are kept
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")})
        << router.get_full_output();
  }
}

class RouterRoutingConnectionPromotedTest
    : public RouterRoutingConnectionCommonTest,
      public testing::TestWithParam<std::string> {
 public:
  void SetUp() override { init(); }

  void TearDown() override { clean(); }
};

/**
 * @test
 *       Verify that when server is promoted from Secondary to Primary and
 *       disconnect_on_promoted_to_primary is set to 'no' (default value) then
 *       connections to that server are not closed.
 */
TEST_P(RouterRoutingConnectionPromotedTest,
       IsConnectionsToSecondaryKeptWhenPromotedToPrimary) {
  ASSERT_NO_FATAL_FAILURE(
      setup_cluster("metadata_3_secondaries_primary_failover.js", 4));

  config_generator_->disconnect_on_promoted_to_primary(GetParam());

  auto router = launch_router(
      router_ro_port_, config_generator_->build_config_file(temp_test_dir_));
  ASSERT_TRUE(wait_for_port_ready(router_rw_port_, 1000))
      << router.get_full_output();

  /*
   * wait until metadata is initialized
   */
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  // connect clients
  std::vector<std::pair<MySQLSession, unsigned>> clients(6);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second = std::stoul(std::string((*result)[0]));
  }

  set_global("{\"primary_failover\": true}");
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_update_timeout));

  // verify that connections to SECONDARY_1 are kept (all RO connections should
  // NOT be closed)
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")})
        << router.get_full_output();
  }
}

std::string promoted_flags[] = {
    "&disconnect_on_promoted_to_primary=no",
    "",
};

INSTANTIATE_TEST_CASE_P(RouterRoutingIsConnectionNotClosedWhenPromoted,
                        RouterRoutingConnectionPromotedTest,
                        testing::ValuesIn(promoted_flags));

/**
 * @test
 *       Verify that when server is promoted from Secondary to Primary and
 *       disconnect_on_promoted_to_primary is set to 'yes' then connections
 *       to that server are closed.
 */
TEST_F(RouterRoutingConnectionTest,
       IsConnectionToSecondaryClosedWhenPromotedToPrimary) {
  ASSERT_NO_FATAL_FAILURE(
      setup_cluster("metadata_3_secondaries_primary_failover.js", 4));

  config_generator_->disconnect_on_promoted_to_primary(
      "&disconnect_on_promoted_to_primary=yes");

  auto router = launch_router(
      router_ro_port_, config_generator_->build_config_file(temp_test_dir_));
  ASSERT_TRUE(wait_for_port_ready(router_ro_port_, 1000))
      << router.get_full_output();

  /*
   * wait until metadata is initialized
   */
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  // connect clients
  std::vector<std::pair<MySQLSession, unsigned>> clients(6);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second = std::stoul(std::string((*result)[0]));
  }

  set_global("{\"primary_failover\": true}");
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_update_timeout));

  // verify that connections to SECONDARY_1 are closed
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    unsigned port = client_and_port.second;

    if (port == cluster_nodes_ports_[1])
      ASSERT_ANY_THROW(client.query_one("select @@port"))
          << router.get_full_output();
    else
      ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
          client.query_one("select @@port")})
          << router.get_full_output();
  }
}

/**
 * @test
 *       Verify that when GR is partitioned, then connections to servers that
 *       are not in majority are closed.
 */
TEST_F(RouterRoutingConnectionTest,
       IsConnectionToMinorityClosedWhenClusterPartition) {
  /*
   * create cluster with 5 servers
   */
  ASSERT_NO_FATAL_FAILURE(
      setup_cluster("metadata_4_secondaries_partitioning.js", 5));

  auto router = launch_router(
      router_ro_port_, config_generator_->build_config_file(temp_test_dir_));
  ASSERT_TRUE(wait_for_port_ready(router_ro_port_, 1000))
      << router.get_full_output();

  /*
   * wait until metadata is initialized
   */
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  // connect clients
  std::vector<std::pair<MySQLSession, unsigned>> clients(10);

  // connect clients to Primary
  for (int i = 0; i < 2; ++i) {
    auto &client = clients[i].first;
    try {
      client.connect("127.0.0.1", router_rw_port_, "username", "password", "",
                     "");
    } catch (const std::exception &e) {
      FAIL() << e.what() << "\n"
             << "router: " << get_router_log_output() << "\n"
             << "cluster[0]: " << cluster_nodes_.at(0).get_full_output() << "\n"
             << "cluster[1]: " << cluster_nodes_.at(1).get_full_output() << "\n"
             << "cluster[2]: " << cluster_nodes_.at(2).get_full_output() << "\n"
             << "cluster[3]: " << cluster_nodes_.at(3).get_full_output() << "\n"
             << "cluster[4]: " << cluster_nodes_.at(4).get_full_output()
             << "\n";
    }
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    clients[i].second = std::stoul(std::string((*result)[0]));
  }

  // connect clients to Secondaries
  for (int i = 2; i < 10; ++i) {
    auto &client = clients[i].first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    clients[i].second = std::stoul(std::string((*result)[0]));
  }

  /*
   * Partition the cluster:
   * - 2 servers are ONLINE: Primary and Secondary_1
   * - 3 servers are OFFLINE: Secondary_2, Secondary_3, Secondary_4
   *
   * Connetions to OFFLINE servers should be closed.
   * Since only 2 servers are ONLINE (minority) connections to them should be
   * closed as well.
   */
  set_global("{\"cluster_partition\": true}");
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_update_timeout));

  /*
   * Connections to servers that are offline should be closed
   * Connections to servers in minority should be closed as well
   */
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_ANY_THROW(client.query_one("select @@port"))
        << router.get_full_output();
  }
}

class RouterRoutingConnectionClusterOverloadTest
    : public RouterRoutingConnectionCommonTest,
      public testing::TestWithParam<std::string> {
 public:
  void SetUp() override { init(); }

  void TearDown() override { clean(); }
};

/**
 * @test
 *       Verity that when GR is overloaded and
 * disconnect_on_metadata_unavailable is set to 'yes' then all connection to GR
 * are closed
 */
TEST_F(RouterRoutingConnectionTest, IsConnectionClosedWhenClusterOverloaded) {
  ASSERT_NO_FATAL_FAILURE(setup_cluster("metadata_3_secondaries_pass.js", 4));

  config_generator_->disconnect_on_metadata_unavailable(
      "&disconnect_on_metadata_unavailable=yes");

  auto router = launch_router(
      router_ro_port_, config_generator_->build_config_file(temp_test_dir_));
  ASSERT_TRUE(wait_for_port_ready(router_ro_port_, 1000))
      << router.get_full_output();

  /*
   * wait until metadata is initialized
   */
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  // connect clients
  std::vector<std::pair<MySQLSession, unsigned>> clients(6);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    try {
      client.connect("127.0.0.1", router_ro_port_, "username", "password", "",
                     "");
    } catch (const std::exception &e) {
      FAIL() << e.what() << "\n"
             << "router: " << get_router_log_output() << "\n"
             << "cluster[0]: " << cluster_nodes_.at(0).get_full_output() << "\n"
             << "cluster[1]: " << cluster_nodes_.at(1).get_full_output() << "\n"
             << "cluster[2]: " << cluster_nodes_.at(2).get_full_output() << "\n"
             << "cluster[3]: " << cluster_nodes_.at(3).get_full_output()
             << "\n";
    }
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second = std::stoul(std::string((*result)[0]));
  }

  /*
   * There is only 1 metadata server, so then primary
   * goes away, metadata is unavailable.
   */
  send_delete(http_port_, 0);
  cluster_nodes_[0].kill();
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_update_timeout));

  // verify that all connections are closed
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_ANY_THROW(client.query_one("select @@port"))
        << router.get_full_output();
  }
}

class RouterRoutingConnectionMDUnavailableTest
    : public RouterRoutingConnectionCommonTest,
      public testing::TestWithParam<std::string> {
 public:
  void SetUp() override { init(); }

  void TearDown() override { clean(); }
};

/**
 * @test
 *       Verify that when GR is overloaded and
 * disconnect_on_metadata_unavailable is set to 'no' (default value) then
 * connections to GR are NOT closed.
 */
TEST_P(RouterRoutingConnectionMDUnavailableTest,
       IsConnectionKeptWhenClusterOverloaded) {
  ASSERT_NO_FATAL_FAILURE(setup_cluster("metadata_3_secondaries_pass.js", 4));

  config_generator_->disconnect_on_promoted_to_primary(GetParam());
  auto router = launch_router(
      router_ro_port_, config_generator_->build_config_file(temp_test_dir_));
  ASSERT_TRUE(wait_for_port_ready(router_rw_port_, 1000))
      << router.get_full_output();

  /*
   * wait until metadata is initialized
   */
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  // connect clients
  std::vector<std::pair<MySQLSession, unsigned>> clients(6);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second = std::stoul(std::string((*result)[0]));
  }

  /*
   * There is only 1 metadata server, so then primary
   * goes away, metadata is unavailable.
   */
  send_delete(http_port_, 0);
  cluster_nodes_[0].kill();
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_update_timeout));

  // verify if all connections are NOT closed
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")})
        << router.get_full_output();
  }
}

std::string metadata_unavailable_flags[] = {
    "&disconnect_on_metadata_unavailable=no",
    "",
};

INSTANTIATE_TEST_CASE_P(RouterRoutingIsConnectionNotClosedWhenMDUnavailable,
                        RouterRoutingConnectionMDUnavailableTest,
                        testing::ValuesIn(metadata_unavailable_flags));

class RouterRoutingConnectionMDRefreshTest
    : public RouterRoutingConnectionCommonTest,
      public testing::TestWithParam<std::string> {
 public:
  void SetUp() override { init(); }

  void TearDown() override { clean(); }
};

/**
 * @test
 *      Verify if connections are not closed when fetching metadata from current
 *      metadata server fails, but fetching from subsequent metadata server
 * passes.
 *
 *      1. Start cluster with 1 Primary and 4 Secondary
 *      2. Establish 2 RW connections and 8 RO connections
 *      3. Fetching MD from Primary fails
 *      4. Fetching MD from Secondary passes
 *      5. Check if connections are still open.
 */
TEST_P(RouterRoutingConnectionMDRefreshTest,
       IsConnectionNotClosedWhenRrefreshFailedForParticularMDServer) {
  /*
   * Primary and first secondary are metadata-cache servers
   */
  config_generator_.reset(
      new ConfigGenerator(get_DEFAULT_defaults(), get_tmp_dir("conf"),
                          {cluster_nodes_ports_[0], cluster_nodes_ports_[1]},
                          router_rw_port_, router_ro_port_));

  unsigned http_port_primary = port_pool_.get_next_available();

  // launch the primary node working also as metadata server
  const std::string json_for_primary =
      replace_env_variables("metadata_3_secondaries_failed_to_update.js",
                            primary_json_env_vars_, cluster_nodes_ports_[0]);
  cluster_nodes_.push_back(launch_server(cluster_nodes_ports_[0],
                                         json_for_primary, http_port_primary));

  // launch the secondary node working also as metadata server
  const std::string json_for_secondary =
      replace_env_variables("metadata_3_secondaries_pass.js",
                            primary_json_env_vars_, cluster_nodes_ports_[1]);
  cluster_nodes_.push_back(
      launch_server(cluster_nodes_ports_[1], json_for_secondary, http_port_));

  // launch the rest of secondary cluster nodes
  for (unsigned port = 2; port < 4; ++port) {
    std::string secondary_json_file =
        get_json_for_secondary(cluster_nodes_ports_[port]);
    cluster_nodes_.push_back(
        launch_server(cluster_nodes_ports_[port], secondary_json_file));
  }

  config_generator_->disconnect_on_metadata_unavailable(
      "&disconnect_on_metadata_unavailable=yes");
  auto router = launch_router(
      router_ro_port_, config_generator_->build_config_file(temp_test_dir_));
  ASSERT_TRUE(wait_for_port_ready(router_rw_port_, 1000))
      << router.get_full_output();

  /*
   * wait until metadata is initialized
   */
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_ready_timeout));

  // connect clients
  std::vector<std::pair<MySQLSession, unsigned>> clients(10);

  for (int i = 0; i < 2; ++i) {
    auto &client = clients[i].first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_rw_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    clients[i].second = std::stoul(std::string((*result)[0]));
  }

  for (int i = 2; i < 10; ++i) {
    auto &client = clients[i].first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    clients[i].second = std::stoul(std::string((*result)[0]));
  }

  set_global(GetParam(), http_port_primary);
  std::this_thread::sleep_for(
      std::chrono::milliseconds(wait_for_cache_update_timeout));

  // verify if all connections are NOT closed
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")})
        << router.get_full_output();
  }
}

std::string steps[] = {"{\"MD_failed\": true}", "{\"GR_primary_failed\": true}",
                       "{\"GR_health_failed\": true}"};

INSTANTIATE_TEST_CASE_P(RouterRoutingIsConnectionNotDisabledWhenMDRefresh,
                        RouterRoutingConnectionMDRefreshTest,
                        testing::ValuesIn(steps));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
