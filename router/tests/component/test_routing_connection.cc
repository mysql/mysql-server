/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include <fstream>
#include <stdexcept>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif
#include <rapidjson/document.h>

#include "config_builder.h"
#include "dim.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysqlrouter/cluster_metadata.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "random_generator.h"
#include "rest_metadata_client.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "router_test_helpers.h"
#include "tcp_port_pool.h"

#define ASSERT_NO_ERROR(expr) \
  ASSERT_THAT(expr, ::testing::Eq(std::error_code{}))

using mysqlrouter::ClusterType;
using mysqlrouter::MySQLSession;
using namespace std::chrono_literals;
static constexpr const char kMockServerConnectionsUri[] =
    "/api/v1/mock_server/connections/";

static const std::string kRestApiUsername("someuser");
static const std::string kRestApiPassword("somepass");

class ConfigGenerator {
  std::map<std::string, std::string> defaults_;
  std::string config_dir_;

  std::string metadata_cache_section_;
  std::string routing_primary_section_;
  std::string routing_secondary_section_;
  std::string monitoring_section_;

  std::vector<uint16_t> metadata_server_ports_;
  uint16_t router_rw_port_;
  uint16_t router_ro_port_;
  uint16_t monitoring_port_;
  std::string disconnect_on_metadata_unavailable_ =
      "&disconnect_on_metadata_unavailable=no";
  std::string disconnect_on_promoted_to_primary_ =
      "&disconnect_on_promoted_to_primary=no";

  std::chrono::milliseconds metadata_refresh_ttl_;

 public:
  ConfigGenerator(const std::map<std::string, std::string> &defaults,
                  const std::string &config_dir,
                  const std::vector<uint16_t> metadata_server_ports,
                  uint16_t router_rw_port, uint16_t router_ro_port,
                  uint16_t monitoring_port,
                  std::chrono::milliseconds metadata_refresh_ttl)
      : defaults_(defaults),
        config_dir_(config_dir),
        metadata_server_ports_(metadata_server_ports),
        router_rw_port_(router_rw_port),
        router_ro_port_(router_ro_port),
        monitoring_port_{monitoring_port},
        metadata_refresh_ttl_{metadata_refresh_ttl} {}

  void disconnect_on_metadata_unavailable(const std::string &value) {
    disconnect_on_metadata_unavailable_ = value;
  }

  void disconnect_on_promoted_to_primary(const std::string &value) {
    disconnect_on_promoted_to_primary_ = value;
  }

  // void metadata_refresh_ttl(unsigned ttl) { metadata_refresh_ttl_ = ttl; }

  void add_metadata_cache_section(
      std::chrono::milliseconds ttl,
      ClusterType cluster_type = ClusterType::GR_V2) {
    const std::string cluster_type_str =
        (cluster_type == ClusterType::RS_V2) ? "ar" : "gr";
    metadata_cache_section_ =
        "[logger]\n"
        "level = DEBUG\n\n"

        "[metadata_cache:test]\n"
        "cluster_type=" +
        cluster_type_str +
        "\n"
        "router_id=1\n"
        "user=mysql_router1_user\n"
        "metadata_cluster=test\n"
        "connect_timeout=1\n"
        "ttl=" +
        std::to_string(ttl.count() / 1000.0) + "\n\n";
  }

  std::string get_metadata_cache_routing_section(uint16_t router_port,
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

  void add_monitoring_section(const std::string &config_dir) {
    std::string passwd_filename =
        mysql_harness::Path(config_dir).join("users").str();

    monitoring_section_ =
        mysql_harness::ConfigBuilder::build_section("rest_api", {}) +
        mysql_harness::ConfigBuilder::build_section(
            "rest_metadata_cache", {{"require_realm", "somerealm"}}) +
        mysql_harness::ConfigBuilder::build_section("http_auth_realm:somerealm",
                                                    {{"backend", "somebackend"},
                                                     {"method", "basic"},
                                                     {"name", "somename"}}) +
        mysql_harness::ConfigBuilder::build_section(
            "http_auth_backend:somebackend",
            {{"backend", "file"}, {"filename", passwd_filename}}) +
        mysql_harness::ConfigBuilder::build_section(
            "http_server", {{"port", std::to_string(monitoring_port_)},
                            {"bind_address", "127.0.0.1"}});
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
           l("master_key_reader") + l("master_key_writer") +
           l("dynamic_state") + "\n";
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
               << routing_secondary_section_ << monitoring_section_
               << std::endl;
    ofs_config.close();

    return file_path.str();
  }

  std::string build_config_file(const std::string &temp_test_dir,
                                ClusterType cluster_type,
                                bool is_primary_and_secondary = false) {
    add_metadata_cache_section(metadata_refresh_ttl_, cluster_type);
    add_routing_primary_section();
    add_monitoring_section(temp_test_dir);

    if (is_primary_and_secondary) {
      add_routing_primary_and_secondary_section();
    } else {
      add_routing_secondary_section();
    }

    init_keyring(defaults_, temp_test_dir);

    const auto state_file = ProcessManager::create_state_file(
        temp_test_dir,
        create_state_file_content("uuid", "", metadata_server_ports_, 0));
    defaults_["dynamic_state"] = state_file;

    return create_config_file(&defaults_, config_dir_);
  }
};

class RouterRoutingConnectionCommonTest : public RouterComponentTest {
 public:
  void SetUp() override {
    RouterComponentTest::SetUp();

    mysql_harness::DIM &dim = mysql_harness::DIM::instance();
    // RandomGenerator
    dim.set_RandomGenerator(
        []() {
          static mysql_harness::RandomGenerator rg;
          return &rg;
        },
        [](mysql_harness::RandomGeneratorInterface *) {});
#if 1
    {
      ProcessWrapper::OutputResponder responder{
          [](const std::string &line) -> std::string {
            if (line == "Please enter password: ")
              return std::string(kRestApiPassword) + "\n";

            return "";
          }};

      auto &cmd = launch_command(
          ProcessManager::get_origin().join("mysqlrouter_passwd").str(),
          {"set",
           mysql_harness::Path(temp_test_dir_.name()).join("users").str(),
           kRestApiUsername},
          EXIT_SUCCESS, true,
          std::vector<std::pair<std::string, std::string>>{}, responder);
      EXPECT_EQ(cmd.wait_for_exit(), 0);
    }
#endif

    cluster_nodes_ports_ = {
        port_pool_.get_next_available(),  // first is PRIMARY
        port_pool_.get_next_available(), port_pool_.get_next_available(),
        port_pool_.get_next_available(), port_pool_.get_next_available()};

    cluster_nodes_http_ports_ = {
        port_pool_.get_next_available(), port_pool_.get_next_available(),
        port_pool_.get_next_available(), port_pool_.get_next_available(),
        port_pool_.get_next_available()};

    router_rw_port_ = port_pool_.get_next_available();
    router_ro_port_ = port_pool_.get_next_available();
    monitoring_port_ = port_pool_.get_next_available();

    config_generator_.reset(new ConfigGenerator(
        get_DEFAULT_defaults(), temp_conf_dir_.name(),
        {cluster_nodes_ports_[0]}, router_rw_port_, router_ro_port_,
        monitoring_port_, metadata_refresh_ttl_));

    mock_http_hostname_ = "127.0.0.1";
    mock_http_uri_ = kMockServerGlobalsRestUri;
  }

  auto &launch_router(uint16_t /* router_port */,
                      const std::string &config_file,
                      std::chrono::milliseconds wait_for_ready = 5s) {
    return ProcessManager::launch_router({"-c", config_file}, EXIT_SUCCESS,
                                         true, false, wait_for_ready);
  }

  auto &launch_server(uint16_t cluster_port, const std::string &json_file,
                      uint16_t http_port, size_t number_of_servers = 5) {
    auto &cluster_node = ProcessManager::launch_mysql_server_mock(
        get_data_dir().join(json_file).str(), cluster_port, EXIT_SUCCESS, false,
        http_port);

    std::vector<uint16_t> nodes_ports;
    nodes_ports.resize(number_of_servers);
    std::copy_n(cluster_nodes_ports_.begin(), number_of_servers,
                nodes_ports.begin());

    EXPECT_TRUE(MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
    set_mock_metadata(http_port, "uuid", classic_ports_to_gr_nodes(nodes_ports),
                      0, classic_ports_to_cluster_nodes(nodes_ports));
    return cluster_node;
  }

  void setup_cluster(
      const std::string &js_for_primary, unsigned number_of_servers,
      const std::string &js_for_secondaries = "rest_server_mock_with_gr.js") {
    // launch cluster nodes
    for (unsigned port = 0; port < number_of_servers; ++port) {
      const std::string js_file =
          port == 0 ? js_for_primary : js_for_secondaries;
      cluster_nodes_.push_back(
          &launch_server(cluster_nodes_ports_[port], js_file,
                         cluster_nodes_http_ports_[port], number_of_servers));
      ASSERT_NO_FATAL_FAILURE(
          check_port_ready(*cluster_nodes_[port], cluster_nodes_ports_[port]));
    }
  }

  struct server_globals {
    bool primary_removed{false};
    bool primary_failover{false};
    bool secondary_failover{false};
    bool secondary_removed{false};
    bool cluster_partition{false};
    bool MD_failed{false};
    bool GR_primary_failed{false};
    bool GR_health_failed{false};

    server_globals &set_primary_removed() {
      primary_removed = true;
      return *this;
    }
    server_globals &set_primary_failover() {
      primary_failover = true;
      return *this;
    }
    server_globals &set_secondary_failover() {
      secondary_failover = true;
      return *this;
    }
    server_globals &set_secondary_removed() {
      secondary_removed = true;
      return *this;
    }
    server_globals &set_cluster_partition() {
      cluster_partition = true;
      return *this;
    }
    server_globals &set_MD_failed() {
      MD_failed = true;
      return *this;
    }
    server_globals &set_GR_primary_failed() {
      GR_primary_failed = true;
      return *this;
    }
    server_globals &set_GR_health_failed() {
      GR_health_failed = true;
      return *this;
    }
  };

  void set_additional_globals(uint16_t http_port,
                              const server_globals &globals) {
    auto json_doc = mock_GR_metadata_as_json(
        "uuid", classic_ports_to_gr_nodes(cluster_nodes_ports_), 0,
        classic_ports_to_cluster_nodes(cluster_nodes_ports_));
    JsonAllocator allocator;
    json_doc.AddMember("primary_removed", globals.primary_removed, allocator);
    json_doc.AddMember("primary_failover", globals.primary_failover, allocator);
    json_doc.AddMember("secondary_failover", globals.secondary_failover,
                       allocator);
    json_doc.AddMember("secondary_removed", globals.secondary_removed,
                       allocator);
    json_doc.AddMember("cluster_partition", globals.cluster_partition,
                       allocator);
    json_doc.AddMember("MD_failed", globals.MD_failed, allocator);
    json_doc.AddMember("GR_primary_failed", globals.GR_primary_failed,
                       allocator);
    json_doc.AddMember("GR_health_failed", globals.GR_health_failed, allocator);
    const auto json_str = json_to_string(json_doc);
    EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
  }

  std::chrono::milliseconds metadata_refresh_ttl_{100};
  std::chrono::milliseconds wait_for_cache_ready_timeout{
      metadata_refresh_ttl_ + std::chrono::milliseconds(5000)};
  std::chrono::milliseconds wait_for_cache_update_timeout{
      metadata_refresh_ttl_ * 20};
  std::unique_ptr<ConfigGenerator> config_generator_;
  TempDirectory temp_test_dir_;
  TempDirectory temp_conf_dir_;
  std::vector<uint16_t> cluster_nodes_ports_;
  std::vector<uint16_t> cluster_nodes_http_ports_;
  std::vector<ProcessWrapper *> cluster_nodes_;
  uint16_t router_rw_port_;
  uint16_t router_ro_port_;
  uint16_t monitoring_port_;

  // http properties
  std::string mock_http_hostname_;
  std::string mock_http_uri_;
};

class RouterRoutingConnectionTest : public RouterRoutingConnectionCommonTest {};

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
  RecordProperty("Worklog", "15868");
  RecordProperty("RequirementId", "FR1");
  RecordProperty("Requirement",
                 "When MySQLRouter connects to the Cluster with metadata "
                 "version 1.x, it MUST disable the routing and log an error");
  RecordProperty("Description",
                 "Testing that the Router disables the Routing and logs an "
                 "error for unsupported metadata version.");

  // preparation
  //
  SCOPED_TRACE("// [prep] creating router config");
  TempDirectory tmp_dir;
  config_generator_.reset(new ConfigGenerator(
      get_DEFAULT_defaults(), tmp_dir.name(), {cluster_nodes_ports_[0]},
      router_rw_port_, router_ro_port_, monitoring_port_,
      metadata_refresh_ttl_));

  SCOPED_TRACE("// [prep] launch the primary node on port " +
               std::to_string(cluster_nodes_ports_.at(0)) +
               " working also as metadata server");

  cluster_nodes_.push_back(&launch_server(cluster_nodes_ports_.at(0),
                                          "metadata_old_schema.js",
                                          cluster_nodes_http_ports_[0]));

  SCOPED_TRACE("// [prep] launching router");
  auto &router = launch_router(router_rw_port_,
                               config_generator_->build_config_file(
                                   temp_test_dir_.name(), ClusterType::GR_V2),
                               -1s);
  EXPECT_TRUE(wait_for_port_ready(monitoring_port_));

  SCOPED_TRACE("// [prep] waiting " +
               std::to_string(wait_for_cache_ready_timeout.count()) +
               "ms until metadata is initialized (and failed)");
  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);

  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_fetched(
      wait_for_cache_ready_timeout, metadata_status,
      [](const RestMetadataClient::MetadataStatus &cur) {
        return cur.refresh_failed > 0;
      }));

  // testing
  //
  SCOPED_TRACE("// [test] expect connecting clients to fail");
  MySQLSession client;

  ASSERT_THROW(client.connect("127.0.0.1", router_rw_port_, "username",
                              "password", "", ""),
               MySQLSession::Error);

  SCOPED_TRACE("// [test] expect router log to contain error message");
  constexpr const char log_msg_re[]{
      "The target Cluster's Metadata version \\('1\\.0\\.1'\\) is not "
      "supported\\. Please use the latest MySQL Shell to upgrade it using "
      "'dba\\.upgradeMetadata\\(\\)'\\. Expected metadata version compatible "
      "with '2\\.0\\.0'"};

  ASSERT_THAT(vec_from_lines(router.get_logfile_content()),
              ::testing::Contains(::testing::ContainsRegex(log_msg_re)));
}

/**
 * @test
 *      Verify that router doesn't start when
 * disconnect_on_promoted_to_primary has invalid value.
 */
TEST_F(RouterRoutingConnectionTest,
       IsRouterFailToStartWhen_disconnect_on_promoted_to_primary_invalid) {
  config_generator_->disconnect_on_promoted_to_primary(
      "&disconnect_on_promoted_to_primary=bogus");
  auto &router = ProcessManager::launch_router(
      {"-c", config_generator_->build_config_file(temp_test_dir_.name(),
                                                  ClusterType::GR_V2)},
      EXIT_FAILURE, true, false, -1s);
  check_port_not_ready(router, router_ro_port_);
}

/**
 * @test
 *      Verify that router doesn't start when
 * disconnect_on_metadata_unavailable has invalid value.
 */
TEST_F(RouterRoutingConnectionTest,
       IsRouterFailToStartWhen_disconnect_on_metadata_unavailable_invalid) {
  config_generator_->disconnect_on_metadata_unavailable(
      "&disconnect_on_metadata_unavailable=bogus");
  auto &router = ProcessManager::launch_router(
      {"-c", config_generator_->build_config_file(temp_test_dir_.name(),
                                                  ClusterType::GR_V2)},
      EXIT_FAILURE, true, false, -1s);
  check_port_not_ready(router, router_ro_port_);
}

struct TracefileTestParam {
  std::string tracefile;
  ClusterType cluster_type;
  std::string param{};

  TracefileTestParam(const std::string tracefile_,
                     const ClusterType cluster_type_,
                     const std::string param_ = "")
      : tracefile(tracefile_), cluster_type(cluster_type_), param(param_) {}
};

class IsConnectionsClosedWhenPrimaryRemovedFromClusterTest
    : public RouterRoutingConnectionTest,
      public ::testing::WithParamInterface<TracefileTestParam> {};

/**
 * @test
 *      Verify that all connections to Primary are closed when Primary is
 * removed from Cluster
 */
TEST_P(IsConnectionsClosedWhenPrimaryRemovedFromClusterTest,
       IsConnectionsClosedWhenPrimaryRemovedFromCluster) {
  TempDirectory tmp_dir("conf");
  const std::string tracefile = GetParam().tracefile;

  config_generator_.reset(new ConfigGenerator(
      get_DEFAULT_defaults(), tmp_dir.name(),
      {cluster_nodes_ports_[0], cluster_nodes_ports_[1]}, router_rw_port_,
      router_ro_port_, monitoring_port_, metadata_refresh_ttl_));

  SCOPED_TRACE("// launch the primary node on port " +
               std::to_string(cluster_nodes_ports_.at(0)) +
               " working also as metadata server");
  cluster_nodes_.push_back(&launch_server(cluster_nodes_ports_[0], tracefile,
                                          cluster_nodes_http_ports_[0], 4));

  SCOPED_TRACE("// launch the secondary node on port " +
               std::to_string(cluster_nodes_ports_.at(1)) +
               " working also as metadata server");
  cluster_nodes_.push_back(&launch_server(cluster_nodes_ports_[1], tracefile,
                                          cluster_nodes_http_ports_[1], 4));

  SCOPED_TRACE("// launch the rest of secondary cluster nodes");
  for (unsigned port = 2; port < 4; ++port) {
    cluster_nodes_.push_back(
        &launch_server(cluster_nodes_ports_[port], tracefile,
                       cluster_nodes_http_ports_[port], 4));
  }

  SCOPED_TRACE("// launching router");
  auto &router = launch_router(
      router_rw_port_, config_generator_->build_config_file(
                           temp_test_dir_.name(), GetParam().cluster_type));
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_rw_port_));

  SCOPED_TRACE("// waiting " +
               std::to_string(wait_for_cache_ready_timeout.count()) +
               "ms until metadata is initialized");
  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);

  SCOPED_TRACE("// connecting clients");
  std::vector<std::pair<MySQLSession, uint16_t>> clients(2);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_FATAL_FAILURE(client.connect("127.0.0.1", router_rw_port_,
                                           "username", "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  set_additional_globals(cluster_nodes_http_ports_[0],
                         server_globals().set_primary_removed());

  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_updated(
      wait_for_cache_update_timeout, metadata_status));

  // verify that connections to PRIMARY are broken
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    EXPECT_TRUE(wait_connection_dropped(client));
  }

  // check there info about closing invalid connections in the logfile
  const auto log_content = router.get_logfile_content();
  const std::string pattern = "INFO .* got request to disconnect " +
                              std::to_string(clients.size()) + " invalid";

  EXPECT_TRUE(pattern_found(log_content, pattern));
}

INSTANTIATE_TEST_SUITE_P(
    IsConnectionsClosedWhenPrimaryRemovedFromCluster,
    IsConnectionsClosedWhenPrimaryRemovedFromClusterTest,
    ::testing::Values(TracefileTestParam(
        "metadata_3_secondaries_server_removed_from_cluster_v2_gr.js",
        ClusterType::GR_V2)));

class IsConnectionsClosedWhenSecondaryRemovedFromClusterTest
    : public RouterRoutingConnectionTest,
      public ::testing::WithParamInterface<TracefileTestParam> {};

/**
 * @test
 *      Verify that all connections to Secondary are closed when Secondary is
 *      removed from Cluster.
 *
 */
TEST_P(IsConnectionsClosedWhenSecondaryRemovedFromClusterTest,
       IsConnectionsClosedWhenSecondaryRemovedFromCluster) {
  const std::string tracefile = GetParam().tracefile;
  ASSERT_NO_FATAL_FAILURE(setup_cluster(tracefile, 4));

  launch_router(router_rw_port_,
                config_generator_->build_config_file(temp_test_dir_.name(),
                                                     GetParam().cluster_type));
  EXPECT_TRUE(wait_for_port_used(router_ro_port_));

  SCOPED_TRACE("// connect clients");
  std::vector<std::pair<MySQLSession, uint16_t>> clients(6);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_FATAL_FAILURE(client.connect("127.0.0.1", router_ro_port_,
                                           "username", "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  SCOPED_TRACE("// removed secondary from mock-server on port " +
               std::to_string(cluster_nodes_http_ports_[0]));
  set_additional_globals(cluster_nodes_http_ports_[0],
                         server_globals().set_secondary_removed());

  // wait for metadata refresh
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);
  RestMetadataClient::MetadataStatus metadata_status;

  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_updated(
      wait_for_cache_update_timeout, metadata_status));

  SCOPED_TRACE("// verify that connections to SECONDARY_1 are broken");
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    uint16_t port = client_and_port.second;

    if (port == cluster_nodes_ports_[1]) {
      SCOPED_TRACE("// connections to aborted server fails");
      EXPECT_TRUE(wait_connection_dropped(client));
    } else {
      SCOPED_TRACE("// connection to server on port " + std::to_string(port) +
                   " still succeeds");
      try {
        std::unique_ptr<MySQLSession::ResultRow> result{
            client.query_one("select @@port")};
      } catch (const std::exception &e) {
        FAIL() << e.what();
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    IsConnectionsClosedWhenSecondaryRemovedFromCluster,
    IsConnectionsClosedWhenSecondaryRemovedFromClusterTest,
    ::testing::Values(TracefileTestParam(
        "metadata_3_secondaries_server_removed_from_cluster_v2_gr.js",
        ClusterType::GR_V2)));

class IsRWConnectionsClosedWhenPrimaryFailoverTest
    : public RouterRoutingConnectionTest,
      public ::testing::WithParamInterface<TracefileTestParam> {};

/**
 * @test
 *       Verify that when Primary is demoted, then all RW connections
 *       to that server are closed.
 */
TEST_P(IsRWConnectionsClosedWhenPrimaryFailoverTest,
       IsRWConnectionsClosedWhenPrimaryFailover) {
  const std::string tracefile = GetParam().tracefile;

  ASSERT_NO_FATAL_FAILURE(setup_cluster(tracefile, 4));
  launch_router(router_ro_port_,
                config_generator_->build_config_file(temp_test_dir_.name(),
                                                     GetParam().cluster_type));
  EXPECT_TRUE(wait_for_port_used(router_rw_port_));

  // connect clients
  std::vector<std::pair<MySQLSession, uint16_t>> clients(2);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_rw_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  set_additional_globals(cluster_nodes_http_ports_[0],
                         server_globals().set_primary_failover());

  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_updated(
      wait_for_cache_update_timeout, metadata_status));

  // verify if RW connections to PRIMARY are closed
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    EXPECT_TRUE(wait_connection_dropped(client));
  }
}

INSTANTIATE_TEST_SUITE_P(IsRWConnectionsClosedWhenPrimaryFailover,
                         IsRWConnectionsClosedWhenPrimaryFailoverTest,
                         ::testing::Values(TracefileTestParam(
                             "metadata_3_secondaries_primary_failover_v2_gr.js",
                             ClusterType::GR_V2)));

class RWDestinationUnreachableTest
    : public RouterRoutingConnectionTest,
      public ::testing::WithParamInterface<TracefileTestParam> {};

TEST_P(RWDestinationUnreachableTest, RWDestinationUnreachable) {
  TempDirectory tmp_dir("conf");
  const std::string tracefile = GetParam().tracefile;

  // TTL is large enough so that router will not notice that RW destination is
  // gone when we kill it
  const auto ttl = 3600s;
  config_generator_.reset(new ConfigGenerator(
      get_DEFAULT_defaults(), tmp_dir.name(), {cluster_nodes_ports_[0]},
      router_rw_port_, router_ro_port_, monitoring_port_, ttl));

  SCOPED_TRACE("// launch the primary node on port " +
               std::to_string(cluster_nodes_ports_.at(0)) +
               " working also as metadata server");
  cluster_nodes_.push_back(&launch_server(cluster_nodes_ports_[0], tracefile,
                                          cluster_nodes_http_ports_[0], 1));

  SCOPED_TRACE("// launching router");
  auto &router = launch_router(
      router_rw_port_, config_generator_->build_config_file(
                           temp_test_dir_.name(), GetParam().cluster_type));
  ASSERT_NO_FATAL_FAILURE(check_port_ready(router, router_rw_port_));

  SCOPED_TRACE("// make a connection to primary node");
  {
    MySQLSession client;
    ASSERT_NO_FATAL_FAILURE(client.connect("127.0.0.1", router_rw_port_,
                                           "username", "password", "", ""));
  }

  SCOPED_TRACE("// kill the primary node");
  EXPECT_NO_THROW(cluster_nodes_[0]->send_clean_shutdown_event());
  EXPECT_NO_THROW(cluster_nodes_[0]->wait_for_exit());

  SCOPED_TRACE("// RW connection should fail");
  {
    MySQLSession client;
    ASSERT_THROW(client.connect("127.0.0.1", router_rw_port_, "username",
                                "password", "", "", /*connect_timeout*/ 1),
                 MySQLSession::Error);
  }
}

INSTANTIATE_TEST_SUITE_P(
    RWDestinationUnreachable, RWDestinationUnreachableTest,
    ::testing::Values(TracefileTestParam("metadata_1_node_repeat_v2_gr.js",
                                         ClusterType::GR_V2)));

class IsROConnectionsKeptWhenPrimaryFailoverTest
    : public RouterRoutingConnectionTest,
      public ::testing::WithParamInterface<TracefileTestParam> {};

/**
 * @test
 *       Verify that when Primary is demoted, then RO connections
 *       to that server are kept.
 */
TEST_P(IsROConnectionsKeptWhenPrimaryFailoverTest,
       IsROConnectionsKeptWhenPrimaryFailover) {
  const std::string tracefile = GetParam().tracefile;

  ASSERT_NO_FATAL_FAILURE(setup_cluster(tracefile, 4));

  config_generator_->disconnect_on_promoted_to_primary("");

  launch_router(router_ro_port_,
                config_generator_->build_config_file(
                    temp_test_dir_.name(), GetParam().cluster_type, true));
  EXPECT_TRUE(wait_for_port_used(router_ro_port_));

  // connect clients
  std::vector<std::pair<MySQLSession, uint16_t>> clients(4);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  set_additional_globals(cluster_nodes_http_ports_[0],
                         server_globals().set_primary_failover());

  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_updated(
      wait_for_cache_update_timeout, metadata_status));

  // verify that RO connections to PRIMARY are kept
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")});
  }
}

INSTANTIATE_TEST_SUITE_P(IsROConnectionsKeptWhenPrimaryFailover,
                         IsROConnectionsKeptWhenPrimaryFailoverTest,
                         ::testing::Values(TracefileTestParam(
                             "metadata_3_secondaries_primary_failover_v2_gr.js",
                             ClusterType::GR_V2)));

class RouterRoutingConnectionPromotedTest
    : public RouterRoutingConnectionCommonTest,
      public testing::WithParamInterface<TracefileTestParam> {};

/**
 * @test
 *       Verify that when server is promoted from Secondary to Primary and
 *       disconnect_on_promoted_to_primary is set to 'no' (default value) then
 *       connections to that server are not closed.
 */
TEST_P(RouterRoutingConnectionPromotedTest,
       IsConnectionsToSecondaryKeptWhenPromotedToPrimary) {
  const std::string tracefile = GetParam().tracefile;
  const std::string param = GetParam().param;

  ASSERT_NO_FATAL_FAILURE(setup_cluster(tracefile, 4));

  config_generator_->disconnect_on_promoted_to_primary(param);

  launch_router(router_ro_port_,
                config_generator_->build_config_file(temp_test_dir_.name(),
                                                     GetParam().cluster_type));
  EXPECT_TRUE(wait_for_port_used(router_ro_port_));

  // connect clients
  std::vector<std::pair<MySQLSession, uint16_t>> clients(6);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  server_globals globals;
  globals.primary_failover = true;
  set_additional_globals(cluster_nodes_http_ports_[0], globals);
  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_updated(
      wait_for_cache_update_timeout, metadata_status));

  // verify that connections to SECONDARY_1 are kept (all RO connections
  // should NOT be closed)
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")});
  }
}

INSTANTIATE_TEST_SUITE_P(
    RouterRoutingIsConnectionNotClosedWhenPromoted,
    RouterRoutingConnectionPromotedTest,
    ::testing::Values(
        TracefileTestParam("metadata_3_secondaries_primary_failover_v2_gr.js",
                           ClusterType::GR_V2,
                           "&disconnect_on_promoted_to_primary=no"),
        TracefileTestParam("metadata_3_secondaries_primary_failover_v2_gr.js",
                           ClusterType::GR_V2, "")));

class IsConnectionToSecondaryClosedWhenPromotedToPrimaryTest
    : public RouterRoutingConnectionTest,
      public ::testing::WithParamInterface<TracefileTestParam> {};

/**
 * @test
 *       Verify that when server is promoted from Secondary to Primary and
 *       disconnect_on_promoted_to_primary is set to 'yes' then connections
 *       to that server are closed.
 */
TEST_P(IsConnectionToSecondaryClosedWhenPromotedToPrimaryTest,
       IsConnectionToSecondaryClosedWhenPromotedToPrimary) {
  const std::string tracefile = GetParam().tracefile;

  ASSERT_NO_FATAL_FAILURE(setup_cluster(tracefile, 4));

  config_generator_->disconnect_on_promoted_to_primary(
      "&disconnect_on_promoted_to_primary=yes");

  launch_router(router_ro_port_,
                config_generator_->build_config_file(temp_test_dir_.name(),
                                                     GetParam().cluster_type));
  EXPECT_TRUE(wait_for_port_used(router_ro_port_));

  // connect clients
  std::vector<std::pair<MySQLSession, uint16_t>> clients(6);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  set_additional_globals(cluster_nodes_http_ports_[0],
                         server_globals().set_primary_failover());
  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_updated(
      wait_for_cache_update_timeout, metadata_status));

  // verify that connections to SECONDARY_1 are closed
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    uint16_t port = client_and_port.second;

    if (port == cluster_nodes_ports_[1])
      EXPECT_TRUE(wait_connection_dropped(client));
    else
      ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
          client.query_one("select @@port")});
  }
}

INSTANTIATE_TEST_SUITE_P(IsConnectionToSecondaryClosedWhenPromotedToPrimary,
                         IsConnectionToSecondaryClosedWhenPromotedToPrimaryTest,
                         ::testing::Values(TracefileTestParam(
                             "metadata_3_secondaries_primary_failover_v2_gr.js",
                             ClusterType::GR_V2)));

class IsConnectionToMinorityClosedWhenClusterPartitionTest
    : public RouterRoutingConnectionTest,
      public ::testing::WithParamInterface<TracefileTestParam> {};

/**
 * @test
 *       Verify that when GR is partitioned, then connections to servers that
 *       are not in majority are closed.
 */
TEST_P(IsConnectionToMinorityClosedWhenClusterPartitionTest,
       IsConnectionToMinorityClosedWhenClusterPartition) {
  const std::string tracefile = GetParam().tracefile;

  /*
   * create cluster with 5 servers
   */
  ASSERT_NO_FATAL_FAILURE(setup_cluster(tracefile, 5, tracefile));

  launch_router(router_ro_port_,
                config_generator_->build_config_file(temp_test_dir_.name(),
                                                     GetParam().cluster_type));
  EXPECT_TRUE(wait_for_port_used(router_rw_port_));

  // connect clients
  std::vector<std::pair<MySQLSession, uint16_t>> clients(10);

  // connect clients to Primary
  for (size_t i = 0; i < 2; ++i) {
    auto &client = clients[i].first;
    ASSERT_NO_FATAL_FAILURE(client.connect("127.0.0.1", router_rw_port_,
                                           "username", "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    clients[i].second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  // connect clients to Secondaries
  for (size_t i = 2; i < 10; ++i) {
    auto &client = clients[i].first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    clients[i].second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  /*
   * Partition the cluster:
   * - 2 servers are ONLINE: Primary and Secondary_1
   * - 3 servers are OFFLINE: Secondary_2, Secondary_3, Secondary_4
   *
   * Connections to OFFLINE servers should be closed.
   * Since only 2 servers are ONLINE (minority) connections to them should be
   * closed as well.
   */
  for (size_t i = 0; i < cluster_nodes_http_ports_.size(); ++i) {
    set_additional_globals(cluster_nodes_http_ports_[i],
                           server_globals().set_cluster_partition());
  }

  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_changed(
      wait_for_cache_update_timeout, metadata_status));

  /*
   * Connections to servers that are offline should be closed
   * Connections to servers in minority should be closed as well
   */
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    EXPECT_TRUE(wait_connection_dropped(client));
  }
}

INSTANTIATE_TEST_SUITE_P(IsConnectionToMinorityClosedWhenClusterPartition,
                         IsConnectionToMinorityClosedWhenClusterPartitionTest,
                         ::testing::Values(TracefileTestParam(
                             "metadata_4_secondaries_partitioning_v2_gr.js",
                             ClusterType::GR_V2)));

class IsConnectionClosedWhenClusterOverloadedTest
    : public RouterRoutingConnectionCommonTest,
      public testing::WithParamInterface<TracefileTestParam> {};

/**
 * @test
 *       Verity that when GR is overloaded and
 * disconnect_on_metadata_unavailable is set to 'yes' then all connection to
 * GR are closed
 */
TEST_P(IsConnectionClosedWhenClusterOverloadedTest,
       IsConnectionClosedWhenClusterOverloaded) {
  const std::string tracefile = GetParam().tracefile;

  ASSERT_NO_FATAL_FAILURE(setup_cluster(tracefile, 4));

  config_generator_->disconnect_on_metadata_unavailable(
      "&disconnect_on_metadata_unavailable=yes");

  launch_router(router_ro_port_,
                config_generator_->build_config_file(temp_test_dir_.name(),
                                                     GetParam().cluster_type));
  EXPECT_TRUE(wait_for_port_used(router_ro_port_));

  // connect clients
  std::vector<std::pair<MySQLSession, uint16_t>> clients(6);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_FATAL_FAILURE(client.connect("127.0.0.1", router_ro_port_,
                                           "username", "password", "", ""));

    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  /*
   * There is only 1 metadata server, so then primary
   * goes away, metadata is unavailable.
   */
  MockServerRestClient(cluster_nodes_http_ports_[0])
      .send_delete(kMockServerConnectionsUri);
  cluster_nodes_[0]->kill();
  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_changed(
      wait_for_cache_update_timeout, metadata_status));

  // verify that all connections are closed
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    EXPECT_TRUE(wait_connection_dropped(client));
  }
}

INSTANTIATE_TEST_SUITE_P(
    IsConnectionClosedWhenClusterOverloaded,
    IsConnectionClosedWhenClusterOverloadedTest,
    ::testing::Values(TracefileTestParam("metadata_3_secondaries_pass_v2_gr.js",
                                         ClusterType::GR_V2)));

class RouterRoutingConnectionMDUnavailableTest
    : public RouterRoutingConnectionCommonTest,
      public testing::WithParamInterface<TracefileTestParam> {};

/**
 * @test
 *       Verify that when GR is overloaded and
 * disconnect_on_metadata_unavailable is set to 'no' (default value) then
 * connections to GR are NOT closed.
 */
TEST_P(RouterRoutingConnectionMDUnavailableTest,
       IsConnectionKeptWhenClusterOverloaded) {
  const std::string tracefile = GetParam().tracefile;
  const std::string param = GetParam().param;

  ASSERT_NO_FATAL_FAILURE(setup_cluster(tracefile, 4));

  config_generator_->disconnect_on_promoted_to_primary(param);
  launch_router(router_ro_port_,
                config_generator_->build_config_file(temp_test_dir_.name(),
                                                     GetParam().cluster_type));
  EXPECT_TRUE(wait_for_port_used(router_ro_port_));

  // connect clients
  std::vector<std::pair<MySQLSession, uint16_t>> clients(6);

  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    client_and_port.second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  /*
   * There is only 1 metadata server, so then primary
   * goes away, metadata is unavailable.
   */
  MockServerRestClient(cluster_nodes_http_ports_[0])
      .send_delete(kMockServerConnectionsUri);
  cluster_nodes_[0]->kill();
  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_changed(
      wait_for_cache_update_timeout, metadata_status));

  // verify if all connections are NOT closed
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")});
  }
}

INSTANTIATE_TEST_SUITE_P(
    RouterRoutingIsConnectionNotClosedWhenMDUnavailable,
    RouterRoutingConnectionMDUnavailableTest,
    ::testing::Values(
        TracefileTestParam("metadata_3_secondaries_pass_v2_gr.js",
                           ClusterType::GR_V2,
                           "&disconnect_on_metadata_unavailable=no"),
        TracefileTestParam("metadata_3_secondaries_pass_v2_gr.js",
                           ClusterType::GR_V2, "")));

using server_globals = RouterRoutingConnectionCommonTest::server_globals;

struct MDRefreshTestParam {
  ClusterType cluster_type;
  std::string tracefile1;
  std::string tracefile2;
  server_globals globals;

  MDRefreshTestParam(ClusterType cluster_type_, std::string tracefile1_,
                     std::string tracefile2_, server_globals globals_)
      : cluster_type(cluster_type_),
        tracefile1(tracefile1_),
        tracefile2(tracefile2_),
        globals(globals_) {}
};

class RouterRoutingConnectionMDRefreshTest
    : public RouterRoutingConnectionCommonTest,
      public testing::WithParamInterface<MDRefreshTestParam> {};

/**
 * @test
 *      Verify if connections are not closed when fetching metadata from
 * current metadata server fails, but fetching from subsequent metadata server
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
  TempDirectory temp_dir("conf");
  config_generator_.reset(new ConfigGenerator(
      get_DEFAULT_defaults(), temp_dir.name(),
      {cluster_nodes_ports_[0], cluster_nodes_ports_[1]}, router_rw_port_,
      router_ro_port_, monitoring_port_, metadata_refresh_ttl_));

  // launch the primary node working also as metadata server
  cluster_nodes_.push_back(&launch_server(cluster_nodes_ports_[0],
                                          GetParam().tracefile1,
                                          cluster_nodes_http_ports_[0], 4));

  // launch the secondary node working also as metadata server
  cluster_nodes_.push_back(&launch_server(cluster_nodes_ports_[1],
                                          GetParam().tracefile2,
                                          cluster_nodes_http_ports_[1], 4));

  // launch the rest of secondary cluster nodes
  for (unsigned port = 2; port < 4; ++port) {
    cluster_nodes_.push_back(&launch_server(
        cluster_nodes_ports_[port], "rest_server_mock_with_gr.js",
        cluster_nodes_http_ports_[port], 4));
  }

  config_generator_->disconnect_on_metadata_unavailable(
      "&disconnect_on_metadata_unavailable=yes");
  auto &router = launch_router(
      router_ro_port_, config_generator_->build_config_file(
                           temp_test_dir_.name(), GetParam().cluster_type));
  EXPECT_TRUE(wait_for_port_used(router_rw_port_));

  // connect clients
  std::vector<std::pair<MySQLSession, uint16_t>> clients(10);

  for (size_t i = 0; i < 2; ++i) {
    auto &client = clients[i].first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_rw_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    clients[i].second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  for (size_t i = 2; i < 10; ++i) {
    auto &client = clients[i].first;
    ASSERT_NO_THROW(client.connect("127.0.0.1", router_ro_port_, "username",
                                   "password", "", ""));
    std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")};
    clients[i].second =
        static_cast<uint16_t>(std::stoul(std::string((*result)[0])));
  }

  ASSERT_TRUE(MockServerRestClient(cluster_nodes_http_ports_[0])
                  .wait_for_rest_endpoint_ready());
  set_additional_globals(cluster_nodes_http_ports_[0], GetParam().globals);
  RestMetadataClient::MetadataStatus metadata_status;
  RestMetadataClient rest_metadata_client(mock_http_hostname_, monitoring_port_,
                                          kRestApiUsername, kRestApiPassword);
  ASSERT_NO_ERROR(rest_metadata_client.wait_for_cache_updated(
      wait_for_cache_update_timeout, metadata_status));

  // verify if all connections are NOT closed
  for (auto &client_and_port : clients) {
    auto &client = client_and_port.first;
    ASSERT_NO_THROW(std::unique_ptr<MySQLSession::ResultRow> result{
        client.query_one("select @@port")});
  }

  // check there is NO info about closing invalid connections in the logfile
  const auto log_content = router.get_logfile_content();
  const std::string pattern = ".* got request to disconnect .* invalid";

  EXPECT_FALSE(pattern_found(log_content, pattern));
}

MDRefreshTestParam steps[] = {
    MDRefreshTestParam(ClusterType::GR_V2,
                       "metadata_3_secondaries_failed_to_update_v2_gr.js",
                       "metadata_3_secondaries_pass_v2_gr.js",
                       server_globals().set_MD_failed()),

    MDRefreshTestParam(ClusterType::GR_V2,
                       "metadata_3_secondaries_failed_to_update_v2_gr.js",
                       "metadata_3_secondaries_pass_v2_gr.js",
                       server_globals().set_GR_primary_failed()),

    MDRefreshTestParam(ClusterType::GR_V2,
                       "metadata_3_secondaries_failed_to_update_v2_gr.js",
                       "metadata_3_secondaries_pass_v2_gr.js",
                       server_globals().set_GR_health_failed())};

INSTANTIATE_TEST_SUITE_P(RouterRoutingIsConnectionNotDisabledWhenMDRefresh,
                         RouterRoutingConnectionMDRefreshTest,
                         testing::ValuesIn(steps));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
