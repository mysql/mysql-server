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

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>
#include "gmock/gmock.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "router_component_system_layout.h"
#include "router_component_test.h"
#include "tcp_port_pool.h"

#include <protobuf_lite/mysqlx_notice.pb.h>

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <thread>

using mysqlrouter::MySQLSession;
using ::testing::PrintToString;
using namespace std::chrono_literals;

namespace {
// default allocator for rapidJson (MemoryPoolAllocator) is broken for
// SparcSolaris
using JsonAllocator = rapidjson::CrtAllocator;
using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<>, JsonAllocator>;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;
using JsonStringBuffer =
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, rapidjson::CrtAllocator>;

constexpr auto kTTL = 60s;
}  // namespace

struct AsyncGRNotice {
  // how many milliseconds after the client connects this Notice
  // should be sent to the client
  std::chrono::milliseconds send_offset_ms;
  unsigned id;
  bool is_local;  // true = local, false = global
  // GR Notice specific payload:
  unsigned type;
  std::string view_id;
  // id of the node(s) on which given notice should get sent
  std::vector<int> nodes;
};

class GrNotificationsTest : public RouterComponentTest {
 protected:
  std::string get_metadata_cache_section(
      const std::string &use_gr_notifications,
      const std::chrono::milliseconds ttl = kTTL) {
    auto ttl_str = std::to_string(std::chrono::duration<double>(ttl).count());
    return "[metadata_cache:test]\n"
           "router_id=1\n"
           "user=mysql_router1_user\n"
           "metadata_cluster=test\n"
           "connect_timeout=1\n"
           "use_gr_notifications=" +
           use_gr_notifications + "\n" + "ttl=" + ttl_str + "\n\n";
  }

  std::string get_metadata_cache_routing_section(uint16_t router_port,
                                                 const std::string &role,
                                                 const std::string &strategy) {
    std::string result =
        "[routing:test_default]\n"
        "bind_port=" +
        std::to_string(router_port) + "\n" +
        "destinations=metadata-cache://test/default?role=" + role + "\n" +
        "protocol=classic\n";

    if (!strategy.empty())
      result += std::string("routing_strategy=" + strategy + "\n");

    return result;
  }

  auto &launch_router(const std::string &temp_test_dir,
                      const std::string &metadata_cache_section,
                      const std::string &routing_section,
                      const std::string &state_file_path,
                      const int expected_exit_code = 0) {
    const std::string masterkey_file =
        Path(temp_test_dir).join("master.key").str();
    const std::string keyring_file = Path(temp_test_dir).join("keyring").str();
    mysql_harness::init_keyring(keyring_file, masterkey_file, true);
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store("mysql_router1_user", "password", "root");
    mysql_harness::flush_keyring();
    mysql_harness::reset_keyring();

    // enable debug logs for better diagnostics in case of failure
    std::string logger_section = "[logger]\nlevel = DEBUG\n";

    // launch the router with metadata-cache configuration
    auto default_section = get_DEFAULT_defaults();
    default_section["keyring_path"] = keyring_file;
    default_section["master_key_path"] = masterkey_file;
    default_section["dynamic_state"] = state_file_path;
    const std::string conf_file = create_config_file(
        temp_test_dir,
        logger_section + metadata_cache_section + routing_section,
        &default_section);
    auto &router = ProcessManager::launch_router(
        {"-c", conf_file}, expected_exit_code, /*catch_stderr=*/true,
        /*with_sudo=*/false);
    return router;
  }

  void set_mock_metadata(const uint16_t http_port, const std::string &gr_id,
                         const std::vector<uint16_t> &gr_node_ports,
                         const std::vector<uint16_t> &gr_node_xports,
                         const bool send = false) {
    JsonAllocator allocator;
    gr_id_.reset(new JsonValue(gr_id.c_str(), gr_id.length(), allocator));

    gr_nodes_.reset(new JsonValue(rapidjson::kArrayType));
    size_t i{0};
    for (auto &gr_node : gr_node_ports) {
      JsonValue node(rapidjson::kArrayType);
      node.PushBack(JsonValue((int)gr_node), allocator);
      node.PushBack(JsonValue("ONLINE", strlen("ONLINE"), allocator),
                    allocator);
      node.PushBack(JsonValue((int)gr_node_xports[i++]), allocator);
      gr_nodes_->PushBack(node, allocator);
    }
    if (send) {
      send_globals(http_port);
    }
  }

  void set_mock_notices(const int node_id, const uint16_t http_port,
                        const std::vector<AsyncGRNotice> &async_notices,
                        const bool send = false) {
    JsonAllocator allocator;

    notices_.reset(new JsonValue(rapidjson::kArrayType));
    for (const auto &async_notice : async_notices) {
      // check if given notice is for this node
      const auto &nodes = async_notice.nodes;
      if (std::find(nodes.begin(), nodes.end(), node_id) == nodes.end())
        continue;

      JsonValue json_notice(rapidjson::kObjectType);
      json_notice.AddMember(
          "send_offset",
          JsonValue((unsigned)async_notice.send_offset_ms.count()), allocator);
      json_notice.AddMember("type", JsonValue(async_notice.id), allocator);
      const std::string scope = async_notice.is_local ? "LOCAL" : "GLOBAL";
      json_notice.AddMember("scope",
                            JsonValue(scope.c_str(), scope.length(), allocator),
                            allocator);
      // gr notice specific payload
      JsonValue json_notice_gr(rapidjson::kObjectType);
      json_notice_gr.AddMember("type", JsonValue(async_notice.type), allocator);
      const std::string view_id = async_notice.view_id;
      json_notice_gr.AddMember(
          "view_id", JsonValue(view_id.c_str(), view_id.length(), allocator),
          allocator);
      json_notice.AddMember("payload", json_notice_gr, allocator);
      notices_->PushBack(json_notice, allocator);
    }
    if (send) {
      send_globals(http_port);
    }
  }

  void send_globals(const uint16_t http_port) {
    JsonAllocator allocator;
    JsonValue json_doc(rapidjson::kObjectType);
    if (gr_id_) {
      json_doc.AddMember("gr_id", *gr_id_, allocator);
    }
    if (gr_nodes_) {
      json_doc.AddMember("gr_nodes", *gr_nodes_, allocator);
    }
    if (notices_) {
      json_doc.AddMember("notices", *notices_, allocator);
    }
    const auto json_str = json_to_string(json_doc);
    EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
  }

  int get_ttl_queries_count(const std::string &json_string) {
    rapidjson::Document json_doc;
    json_doc.Parse(json_string.c_str());
    if (json_doc.HasMember("md_query_count")) {
      EXPECT_TRUE(json_doc["md_query_count"].IsInt());
      return json_doc["md_query_count"].GetInt();
    }
    return 0;
  }

  std::string create_state_file(const std::string &dir,
                                const std::string &group_id,
                                const uint16_t node1_port,
                                const uint16_t node2_port) {
    return RouterComponentTest::create_state_file(
        dir,
        "{"
        "\"version\": \"1.0.0\","
        "\"metadata-cache\": {"
        "\"group-replication-id\": "
        "\"" +
            group_id +
            "\","
            "\"cluster-metadata-servers\": ["
            "\"mysql://127.0.0.1:" +
            std::to_string(node1_port) +
            "\", "
            "\"mysql://127.0.0.1:" +
            std::to_string(node2_port) +
            "\""
            "]"
            "}"
            "}");
  }

  int wait_for_md_queries(int expected_md_queries_count, uint16_t http_port) {
    int md_queries_count, retries{0};
    do {
      std::this_thread::sleep_for(100ms);
      const std::string server_globals =
          MockServerRestClient(http_port).get_globals_as_json_string();
      md_queries_count = get_ttl_queries_count(server_globals);
    } while (md_queries_count != expected_md_queries_count && retries++ < 50);

    return md_queries_count;
  }

  TcpPortPool port_pool_;
  std::unique_ptr<JsonValue> notices_;
  std::unique_ptr<JsonValue> gr_id_;
  std::unique_ptr<JsonValue> gr_nodes_;
};

struct GrNotificationsTestParams {
  // how long do we wait for the router to operate before checking the
  // metadata queries count
  std::chrono::milliseconds router_uptime;
  // how many metadata queries we expect over this period
  int expected_md_queries_count;
  // what Notices should be sent by the given cluster nodes at what
  // time offsets
  std::vector<AsyncGRNotice> notices;

  GrNotificationsTestParams(std::chrono::milliseconds router_uptime_,
                            int expected_md_queries_count_,
                            std::vector<AsyncGRNotice> notices_)
      : router_uptime(router_uptime_),
        expected_md_queries_count(expected_md_queries_count_),
        notices(notices_) {}
};

class GrNotificationsParamTest
    : public GrNotificationsTest,
      public ::testing::WithParamInterface<GrNotificationsTestParams> {
 protected:
  virtual void SetUp() { GrNotificationsTest::SetUp(); }
};

/**
 * @test
 *      Verify that Router gets proper GR Notifications according to the cluster
 *      and Router configuration.
 */
TEST_P(GrNotificationsParamTest, GrNotification) {
  const auto test_params = GetParam();
  const auto async_notices = test_params.notices;
  const std::string kGroupId = "3a0be5af-0022-11e8-9655-0800279e6a88";

  TempDirectory temp_test_dir;

  const unsigned kClusterNodesCount = 2;
  std::vector<ProcessWrapper *> cluster_nodes;
  std::vector<uint16_t> cluster_nodes_ports;
  std::vector<uint16_t> cluster_nodes_xports;
  std::vector<uint16_t> cluster_http_ports;
  for (unsigned i = 0; i < kClusterNodesCount; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_nodes_xports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE(
      "// Launch 2 server mocks that will act as our metadata servers");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes.js").str();
  std::vector<uint16_t> classic_ports, x_ports;
  for (unsigned i = 0; i < kClusterNodesCount; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i], cluster_nodes_xports[i]));
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(*cluster_nodes[i], cluster_nodes_ports[i], 5000ms));
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(*cluster_nodes[i], cluster_nodes_xports[i], 5000ms));
    ASSERT_TRUE(MockServerRestClient(cluster_http_ports[i])
                    .wait_for_rest_endpoint_ready())
        << cluster_nodes[i]->get_full_output();

    SCOPED_TRACE("// Make our metadata server return 2 metadata servers");
    classic_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1]};
    x_ports = {cluster_nodes_xports[0], cluster_nodes_xports[1]};
    set_mock_metadata(cluster_http_ports[i], kGroupId, classic_ports, x_ports);

    SCOPED_TRACE(
        "// Make our metadata server to send GR notices at requested "
        "time offsets");
    set_mock_notices(i, cluster_http_ports[i], async_notices, /*send=*/true);
  }

  SCOPED_TRACE("// Create a router state file");
  const std::string state_file =
      create_state_file(temp_test_dir.name(), kGroupId, cluster_nodes_ports[0],
                        cluster_nodes_ports[1]);

  SCOPED_TRACE(
      "// Create a configuration file sections with high ttl so that "
      "metadata updates were triggered by the GR notifications");
  const std::string metadata_cache_section =
      get_metadata_cache_section(/*use_gr_notifications=*/"1", kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file);

  std::this_thread::sleep_for(test_params.router_uptime);

  // +1 is for expected initial metadata read that the router does at the
  // beginning
  const int expected_md_queries_count =
      test_params.expected_md_queries_count + 1;

  // we only expect initial ttl read (hence 1), because x-port is not valid
  // there are not metadata refresh triggered by the notifications
  int md_queries_count =
      wait_for_md_queries(expected_md_queries_count, cluster_http_ports[0]);
  ASSERT_EQ(expected_md_queries_count, md_queries_count)
      << "mock[0]: " << cluster_nodes[0]->get_full_output() << "\n"
      << "mock[1]: " << cluster_nodes[1]->get_full_output() << "\n"
      << "router: " << router.get_full_logfile();
}

INSTANTIATE_TEST_CASE_P(
    CheckNoticesHandlingIsOk, GrNotificationsParamTest,
    ::testing::Values(
        // 0) single notification received from single (first) node
        // we expect 1 metadata cache update
        GrNotificationsTestParams(
            500ms, 1,
            {{100ms,
              Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
              true,
              Mysqlx::Notice::
                  GroupReplicationStateChanged_Type_MEMBERSHIP_VIEW_CHANGE,
              "abcdefg",
              {0}}}),
        // 1) 3 notifications with the same view id, again only 1
        // mdc update expected
        GrNotificationsTestParams(
            500ms, 1,
            {{100ms,
              Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
              true,
              Mysqlx::Notice::
                  GroupReplicationStateChanged_Type_MEMBERSHIP_VIEW_CHANGE,
              "abcdefg",
              {0}},
             {200ms,
              Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
              true,
              Mysqlx::Notice::
                  GroupReplicationStateChanged_Type_MEMBER_STATE_CHANGE,
              "abcdefg",
              {0}},
             {300ms,
              Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
              true,
              Mysqlx::Notice::
                  GroupReplicationStateChanged_Type_MEMBERSHIP_QUORUM_LOSS,
              "abcdefg",
              {0}}}),
        // 2) 3 notifications; 2 have different view id this time so the
        // refresh should be triggered twice
        GrNotificationsTestParams(
            1000ms, 2,
            {{100ms,
              Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
              true,
              Mysqlx::Notice::
                  GroupReplicationStateChanged_Type_MEMBERSHIP_VIEW_CHANGE,
              "abcdefg",
              {0}},
             {400ms,
              Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
              true,
              Mysqlx::Notice::
                  GroupReplicationStateChanged_Type_MEMBER_STATE_CHANGE,
              "abcdefg",
              {0}},
             {700ms,
              Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
              true,
              Mysqlx::Notice::
                  GroupReplicationStateChanged_Type_MEMBERSHIP_QUORUM_LOSS,
              "hijklmn",
              {0}}}),
        // 3) 2 notifications on both nodes with the same view id
        GrNotificationsTestParams(
            1500ms, 1,
            {{100ms,
              Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
              true,
              Mysqlx::Notice::
                  GroupReplicationStateChanged_Type_MEMBERSHIP_VIEW_CHANGE,
              "abcdefg",
              {0, 1}}}),
        // 4) 2 notifications on both nodes with different view ids
        GrNotificationsTestParams(
            700ms, 2,
            {{100ms,
              Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
              true,
              Mysqlx::Notice::
                  GroupReplicationStateChanged_Type_MEMBERSHIP_VIEW_CHANGE,
              "abcdefg",
              {0}},
             {500ms,
              Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
              true,
              Mysqlx::Notice::
                  GroupReplicationStateChanged_Type_MEMBER_ROLE_CHANGE,
              "hijklmn",
              {0}}})));

class GrNotificationsTestNoParam : public GrNotificationsTest {
 public:
  virtual void SetUp() { GrNotificationsTest::SetUp(); }
};

/**
 * @test
 *      Verify that Router operates properly when it can't connect to the
 * x-port.
 */
TEST_F(GrNotificationsTestNoParam, GrNotificationNoXPort) {
  const std::string kGroupId = "3a0be5af-0022-11e8-9655-0800279e6a88";

  TempDirectory temp_test_dir;

  const unsigned CLUSTER_NODES = 2;
  std::vector<ProcessWrapper *> cluster_nodes;
  std::vector<uint16_t> cluster_nodes_ports;
  std::vector<uint16_t> reserved_nodes_xports;
  std::vector<uint16_t> cluster_http_ports;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    reserved_nodes_xports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE(
      "// Launch 2 server mocks that will act as our metadata servers");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes.js").str();
  std::vector<uint16_t> classic_ports, x_ports;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i]));
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(*cluster_nodes[i], cluster_nodes_ports[i], 5000ms));
    ASSERT_TRUE(MockServerRestClient(cluster_http_ports[i])
                    .wait_for_rest_endpoint_ready())
        << cluster_nodes[i]->get_full_output();

    SCOPED_TRACE("// Make our metadata server return 2 metadata servers");
    classic_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1]};
    x_ports = {reserved_nodes_xports[0], reserved_nodes_xports[1]};
    set_mock_metadata(cluster_http_ports[i], kGroupId, classic_ports, x_ports);

    set_mock_notices(
        i, cluster_http_ports[i],
        {{100ms,
          Mysqlx::Notice::Frame::GROUP_REPLICATION_STATE_CHANGED,
          true,
          Mysqlx::Notice::
              GroupReplicationStateChanged_Type_MEMBERSHIP_VIEW_CHANGE,
          "abcdefg",
          {0}}},
        true);
  }

  SCOPED_TRACE("// Create a router state file");
  const std::string state_file =
      create_state_file(temp_test_dir.name(), kGroupId, cluster_nodes_ports[0],
                        cluster_nodes_ports[1]);

  SCOPED_TRACE("// Create a configuration file sections with high ttl");
  const std::string metadata_cache_section =
      get_metadata_cache_section(/*use_gr_notifications=*/"1", kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch the router");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file);

  SCOPED_TRACE("// Let the router run for a while");
  std::this_thread::sleep_for(500ms);

  // we only expect initial ttl read (hence 1), because x-port is not valid
  // there are not metadata refresh triggered by the notifications
  int md_queries_count = wait_for_md_queries(1, cluster_http_ports[0]);
  ASSERT_EQ(1, md_queries_count)
      << "mock[0]: " << cluster_nodes[0]->get_full_output() << "\n"
      << "mock[1]: " << cluster_nodes[1]->get_full_output() << "\n"
      << "router: " << router.get_full_logfile();

  // we expect that the router will not be able to connect to both nodes on the
  // x-port. that can take up to 2 * 10s as 10 seconds is a timeout we use for
  // the x connect. If the port that we try to connect to is not used/blocked by
  // anyone that should error out right away but that is not a case sometimes on
  // Solaris so in the worst case we will wait 20 seconds here for the router to
  // exit
  ASSERT_FALSE(router.send_shutdown_event());
  check_exit_code(router, EXIT_SUCCESS, 22000ms);
}

/**
 * @test Verify that killing one of the nodes (hence disconnecting the
 * notification listener is triggering the metadata refresh.
 */
TEST_F(GrNotificationsTestNoParam, GrNotificationXPortConnectionFailure) {
  const std::string kGroupId = "3a0be5af-0022-11e8-9655-0800279e6a88";

  TempDirectory temp_test_dir;

  const unsigned CLUSTER_NODES = 2;
  std::vector<ProcessWrapper *> cluster_nodes;
  std::vector<uint16_t> cluster_nodes_ports, cluster_nodes_xports,
      cluster_http_ports;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes_ports.push_back(port_pool_.get_next_available());
    cluster_nodes_xports.push_back(port_pool_.get_next_available());
    cluster_http_ports.push_back(port_pool_.get_next_available());
  }

  SCOPED_TRACE(
      "// Launch 2 server mocks that will act as our metadata servers");
  const auto trace_file =
      get_data_dir().join("metadata_dynamic_nodes.js").str();
  std::vector<uint16_t> classic_ports, x_ports;
  for (unsigned i = 0; i < CLUSTER_NODES; ++i) {
    cluster_nodes.push_back(&ProcessManager::launch_mysql_server_mock(
        trace_file, cluster_nodes_ports[i], EXIT_SUCCESS, false,
        cluster_http_ports[i], cluster_nodes_xports[i]));
    ASSERT_NO_FATAL_FAILURE(
        check_port_ready(*cluster_nodes[i], cluster_nodes_ports[i], 5000ms));
    ASSERT_TRUE(MockServerRestClient(cluster_http_ports[i])
                    .wait_for_rest_endpoint_ready())
        << cluster_nodes[i]->get_full_output();

    SCOPED_TRACE("// Make our metadata server return 2 metadata servers");
    classic_ports = {cluster_nodes_ports[0], cluster_nodes_ports[1]};
    x_ports = {cluster_nodes_xports[0], cluster_nodes_xports[1]};
    set_mock_metadata(cluster_http_ports[i], kGroupId, classic_ports, x_ports,
                      true);
  }

  SCOPED_TRACE("// Create a router state file");
  const std::string state_file =
      create_state_file(temp_test_dir.name(), kGroupId, cluster_nodes_ports[0],
                        cluster_nodes_ports[1]);

  SCOPED_TRACE("// Create a configuration file sections with high ttl");
  const std::string metadata_cache_section =
      get_metadata_cache_section(/*use_gr_notifications=*/"1", kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router");
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file);

  std::this_thread::sleep_for(1s);
  EXPECT_TRUE(cluster_nodes[1]->kill() == 0)
      << cluster_nodes[1]->get_full_output();
  std::this_thread::sleep_for(1s);

  // we only xpect initial ttl read plus the one caused by the x-protocol
  // notifier connection to the node we killed
  int md_queries_count = wait_for_md_queries(2, cluster_http_ports[0]);
  ASSERT_EQ(2, md_queries_count)
      << "mock[0]: " << cluster_nodes[0]->get_full_output() << "\n"
      << "mock[1]: " << cluster_nodes[1]->get_full_output() << "\n"
      << "router: " << router.get_full_logfile();
}

struct ConfErrorTestParams {
  std::string use_gr_notifications_option_value;
  std::string expected_error_message;
};

class GrNotificationsConfErrorTest
    : public GrNotificationsTest,
      public ::testing::WithParamInterface<ConfErrorTestParams> {
 protected:
  virtual void SetUp() { GrNotificationsTest::SetUp(); }
};

/**
 * @test
 *      Verify that Router returns with a proper error message when
 *      invalid GR notification options is configured.
 */
TEST_P(GrNotificationsConfErrorTest, GrNotificationConfError) {
  const auto test_params = GetParam();
  const std::string kGroupId = "3a0be5af-0022-11e8-9655-0800279e6a88";

  TempDirectory temp_test_dir;

  SCOPED_TRACE("// Create a router configuration file");
  const std::string metadata_cache_section = get_metadata_cache_section(
      /*use_gr_notifications=*/test_params.use_gr_notifications_option_value,
      kTTL);
  const uint16_t router_port = port_pool_.get_next_available();
  const std::string routing_section = get_metadata_cache_routing_section(
      router_port, "PRIMARY", "first-available");

  SCOPED_TRACE("// Launch ther router");
  // clang-format off
  const std::string state_file =
              RouterComponentTest::create_state_file(temp_test_dir.name(),
                                "{"
                                  "\"version\": \"1.0.0\","
                                  "\"metadata-cache\": {"
                                    "\"group-replication-id\": " "\"" + kGroupId + "\","
                                    "\"cluster-metadata-servers\": []"
                                  "}"
                                "}");
  // clang-format on
  auto &router = launch_router(temp_test_dir.name(), metadata_cache_section,
                               routing_section, state_file, EXIT_FAILURE);

  const auto wait_for_process_exit_timeout{10000ms};
  check_exit_code(router, EXIT_FAILURE, wait_for_process_exit_timeout);

  const std::string log_content = router.get_full_logfile();
  EXPECT_NE(log_content.find(test_params.expected_error_message),
            log_content.npos)
      << log_content;
}

INSTANTIATE_TEST_CASE_P(
    CheckNoticesConfError, GrNotificationsConfErrorTest,
    ::testing::Values(
        ConfErrorTestParams{"2",
                            "Configuration error: option use_gr_notifications "
                            "in [metadata_cache:test] needs value between 0 "
                            "and 1 inclusive, was '2'"},
        ConfErrorTestParams{"-1",
                            "Configuration error: option use_gr_notifications "
                            "in [metadata_cache:test] needs value between 0 "
                            "and 1 inclusive, was '-1'"},
        ConfErrorTestParams{"invalid",
                            "Configuration error: option use_gr_notifications "
                            "in [metadata_cache:test] needs value between 0 "
                            "and 1 inclusive, was 'invalid'"}));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
