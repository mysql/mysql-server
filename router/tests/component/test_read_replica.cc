/*
Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
#include <fstream>
#include <stdexcept>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config_builder.h"
#include "keyring/keyring_manager.h"
#include "mock_server_rest_client.h"
#include "mock_server_testutils.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/rest_client.h"
#include "router_component_clusterset.h"
#include "router_component_test.h"
#include "router_component_testutils.h"
#include "stdx_expected_no_error.h"
#include "tcp_port_pool.h"

using mysqlrouter::InstanceType;
using mysqlrouter::MySQLSession;
using namespace std::chrono_literals;

Path g_origin_path;

namespace mysqlrouter {
std::ostream &operator<<(std::ostream &os, const MysqlError &e) {
  return os << e.sql_state() << " code: " << e.value() << ": " << e.message();
}
}  // namespace mysqlrouter

class ReadReplicaTest : public RouterComponentClusterSetTest {
 protected:
  std::pair<std::string, std::map<std::string, std::string>>
  metadata_cache_section(const std::string &cluster_type_str = "gr") {
    auto ttl_str = std::to_string(std::chrono::duration<double>(kTTL).count());

    std::map<std::string, std::string> options{
        {"cluster_type", cluster_type_str}, {"router_id", "1"},
        {"user", "mysql_router1_user"},     {"metadata_cluster", "test"},
        {"connect_timeout", "1"},           {"ttl", ttl_str}};

    return {"metadata_cache:test", options};
  }

  std::pair<std::string, std::map<std::string, std::string>> routing_section(
      uint16_t router_port, const std::string &role,
      const std::string &strategy) {
    std::map<std::string, std::string> options{
        {"bind_port", std::to_string(router_port)},
        {"destinations", "metadata-cache://test/default?role=" + role},
        {"protocol", "classic"}};

    if (!strategy.empty()) {
      options["routing_strategy"] = strategy;
    }

    return {"routing:test_default" + std::to_string(router_port), options};
  }

  std::pair<std::string, std::map<std::string, std::string>>
  routing_static_section(uint16_t router_port,
                         std::vector<uint16_t> dest_ports) {
    std::string destinations;
    for (size_t i = 0; i < dest_ports.size(); ++i) {
      destinations += "127.0.0.1:" + std::to_string(dest_ports[i]);
      if (i < dest_ports.size() - 1) {
        destinations += ",";
      }
    }

    std::map<std::string, std::string> options{
        {"bind_port", std::to_string(router_port)},
        {"mode", "read-only"},
        {"destinations", destinations},
        {"routing_strategy", "round-robin"},
        {"protocol", "classic"}};

    return {"routing:static", options};
  }

  std::pair<std::string, std::map<std::string, std::string>>
  destination_status_section(unsigned threshold) {
    std::map<std::string, std::string> options{
        {"error_quarantine_threshold", std::to_string(threshold)},
        {"error_quarantine_interval", "1"}};

    return {"destination_status", options};
  }

  std::string get_router_options_as_json_str(
      const std::string &target_cluster,
      const std::optional<std::string> &invalidated_cluster_policy,
      const std::optional<std::string> &read_only_targets) {
    JsonValue json_val(rapidjson::kObjectType);
    JsonAllocator allocator;
    json_val.AddMember(
        "target_cluster",
        JsonValue(target_cluster.c_str(), target_cluster.length(), allocator),
        allocator);

    if (invalidated_cluster_policy) {
      json_val.AddMember(
          "invalidated_cluster_policy",
          JsonValue((*invalidated_cluster_policy).c_str(),
                    (*invalidated_cluster_policy).length(), allocator),
          allocator);
    }

    if (read_only_targets) {
      json_val.AddMember("read_only_targets",
                         JsonValue((*read_only_targets).c_str(),
                                   (*read_only_targets).length(), allocator),
                         allocator);
    }

    return json_to_string(json_val);
  }

  struct NodeData {
    std::string gr_node_status{"ONLINE"};
    std::string gr_member_role;
    std::string uuid;
    uint16_t classic_port{};
    uint16_t x_port{};
    uint16_t http_port{};

    // attributes
    std::optional<std::string> instance_type;
    std::optional<bool> hidden;
    std::optional<bool> disconnect_existing_sessions_when_hidden;

    ProcessWrapper *process;

    std::string get_attributes_as_json_str() const {
      JsonValue json_val(rapidjson::kObjectType);
      JsonAllocator allocator;
      if (instance_type) {
        json_val.AddMember("instance_type",
                           JsonValue((*instance_type).c_str(),
                                     (*instance_type).length(), allocator),
                           allocator);
      }

      if (hidden || disconnect_existing_sessions_when_hidden) {
        JsonValue tags(rapidjson::kObjectType);

        if (hidden) {
          tags.AddMember("_hidden", *hidden, allocator);
        }

        if (disconnect_existing_sessions_when_hidden) {
          tags.AddMember("_disconnect_existing_sessions_when_hidden",
                         *disconnect_existing_sessions_when_hidden, allocator);
        }

        json_val.AddMember("tags", tags, allocator);
      }

      return json_to_string(json_val);
    }
  };

  JsonValue mock_metadata_as_json(const std::string &gr_id, unsigned gr_pos,
                                  const std::vector<NodeData> &cluster_nodes,
                                  const std::string &router_options,
                                  const std::string &node_host = "127.0.0.1") {
    JsonValue json_doc(rapidjson::kObjectType);
    JsonAllocator allocator;
    json_doc.AddMember("gr_id",
                       JsonValue(gr_id.c_str(), gr_id.length(), allocator),
                       allocator);

    JsonValue gr_nodes_json(rapidjson::kArrayType);

    for (auto &cluster_node : cluster_nodes) {
      if (cluster_node.instance_type == "read-replica") {
        continue;
      }
      JsonValue node(rapidjson::kArrayType);
      node.PushBack(JsonValue(cluster_node.uuid.c_str(),
                              cluster_node.uuid.length(), allocator),
                    allocator);
      node.PushBack(static_cast<int>(cluster_node.classic_port), allocator);
      node.PushBack(JsonValue(cluster_node.gr_node_status.c_str(),
                              cluster_node.gr_node_status.length(), allocator),
                    allocator);
      node.PushBack(JsonValue(cluster_node.gr_member_role.c_str(),
                              cluster_node.gr_member_role.length(), allocator),
                    allocator);
      gr_nodes_json.PushBack(node, allocator);
    }
    json_doc.AddMember("gr_nodes", gr_nodes_json, allocator);
    json_doc.AddMember("gr_pos", gr_pos, allocator);

    JsonValue cluster_nodes_json(rapidjson::kArrayType);
    for (auto &cluster_node : cluster_nodes) {
      JsonValue node(rapidjson::kArrayType);
      node.PushBack(JsonValue(cluster_node.uuid.c_str(),
                              cluster_node.uuid.length(), allocator),
                    allocator);
      node.PushBack(static_cast<int>(cluster_node.classic_port), allocator);
      node.PushBack(static_cast<int>(cluster_node.x_port), allocator);
      const std::string attributes = cluster_node.get_attributes_as_json_str();
      node.PushBack(
          JsonValue(attributes.c_str(), attributes.length(), allocator),
          allocator);

      cluster_nodes_json.PushBack(node, allocator);
    }
    json_doc.AddMember("cluster_nodes", cluster_nodes_json, allocator);

    json_doc.AddMember(
        "gr_node_host",
        JsonValue(node_host.c_str(), node_host.length(), allocator), allocator);

    json_doc.AddMember(
        "router_options",
        JsonValue(router_options.c_str(), router_options.length(), allocator),
        allocator);

    json_doc.AddMember("view_id", view_id_, allocator);

    return json_doc;
  }

  void set_mock_metadata(uint16_t http_port, const std::string &gr_id,
                         unsigned gr_pos,
                         const std::vector<NodeData> &cluster_nodes,
                         const std::string &router_options,
                         const std::string &node_host = "127.0.0.1") {
    const auto json_doc = mock_metadata_as_json(gr_id, gr_pos, cluster_nodes,
                                                router_options, node_host);

    const auto json_str = json_to_string(json_doc);

    ASSERT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
  }

  void launch_static_destinations(const size_t qty) {
    for (size_t i{0}; i < qty; ++i) {
      const auto classic_port = port_pool_.get_next_available();
      const auto http_port = port_pool_.get_next_available();
      router_static_dest_ports.push_back(classic_port);
      router_static_dest_http_ports.push_back(http_port);

      auto &mock_serv = mock_server_spawner().spawn(
          mock_server_cmdline("my_port.js")
              .port(router_static_dest_ports.back())
              .http_port(router_static_dest_http_ports.back())
              .args());

      ASSERT_NO_FATAL_FAILURE(check_port_ready(mock_serv, http_port));
      EXPECT_TRUE(
          MockServerRestClient(http_port).wait_for_rest_endpoint_ready());
    }
  }

  void create_gr_cluster(size_t gr_nodes_number, size_t rr_number,
                         const std::string &read_only_targets) {
    const std::string gr_trace_file = "metadata_rr_gr_nodes.js";
    const std::string no_gr_trace_file = "my_port.js";

    gr_nodes_count_ = gr_nodes_number;
    read_replica_nodes_count_ = rr_number;

    for (size_t i = 0; i < gr_nodes_number + rr_number; ++i) {
      NodeData node;
      node.uuid = "uuid-" + std::to_string(i + 1);
      node.instance_type =
          i < gr_nodes_number ? "group-member" : "read-replica";
      node.classic_port = port_pool_.get_next_available();
      node.http_port = port_pool_.get_next_available();
      node.gr_member_role = i == 0 ? "PRIMARY" : "SECONDARY";

      cluster_nodes_.emplace_back(node);
    }

    for (auto &cluster_node : cluster_nodes_) {
      const std::string trace_file =
          cluster_node.instance_type == "group-member" ? gr_trace_file
                                                       : no_gr_trace_file;

      cluster_node.process =
          &mock_server_spawner().spawn(mock_server_cmdline(trace_file)
                                           .port(cluster_node.classic_port)
                                           .http_port(cluster_node.http_port)
                                           .args());

      ASSERT_NO_FATAL_FAILURE(
          check_port_ready(*cluster_node.process, cluster_node.http_port));
      EXPECT_TRUE(MockServerRestClient(cluster_node.http_port)
                      .wait_for_rest_endpoint_ready());
    }

    router_options_ = get_router_options(read_only_targets);
    update_cluster_metadata();
  }

  void create_ar_cluster(size_t ar_nodes_number, size_t rr_number) {
    const std::string ar_trace_file = "metadata_dynamic_nodes_v2_ar.js";

    gr_nodes_count_ = ar_nodes_number;
    read_replica_nodes_count_ = rr_number;

    for (size_t i = 0; i < ar_nodes_number + rr_number; ++i) {
      NodeData node;
      node.uuid = "uuid-" + std::to_string(i + 1);
      node.classic_port = port_pool_.get_next_available();
      node.http_port = port_pool_.get_next_available();
      node.instance_type =
          i < ar_nodes_number ? "async-member" : "read-replica";

      cluster_nodes_.emplace_back(node);
    }

    for (auto &cluster_node : cluster_nodes_) {
      cluster_node.process =
          &mock_server_spawner().spawn(mock_server_cmdline(ar_trace_file)
                                           .port(cluster_node.classic_port)
                                           .http_port(cluster_node.http_port)
                                           .args());
      ASSERT_NO_FATAL_FAILURE(
          check_port_ready(*cluster_node.process, cluster_node.http_port));
      EXPECT_TRUE(MockServerRestClient(cluster_node.http_port)
                      .wait_for_rest_endpoint_ready());
    }

    update_cluster_metadata();
  }

  // returns classic port of the removed node
  uint16_t remove_node(size_t id, bool remove_from_md = true) {
    assert(id < cluster_nodes_.size());
    const auto result = cluster_nodes_[id].classic_port;
    const auto node = cluster_nodes_[id];
    node.process->kill();
    node.process->wait_for_exit();
    cluster_nodes_.erase(cluster_nodes_.begin() + id);
    if (remove_from_md) {
      update_cluster_metadata();
    }

    if (id < gr_nodes_count_) {
      gr_nodes_count_--;
    } else {
      read_replica_nodes_count_--;
    }

    return result;
  }

  /*
   * @param classic_port - use given port as a classic port of an added node, if
   * nullopt take new port from the pool
   * @param update_metadata - if true notify all the nodes in the cluster about
   * the change
   * @param position - put the newly created node at given position in the nodes
   * table, if std::nullopt put it at the end of the table
   */
  void add_read_replica_node(std::optional<size_t> classic_port = std::nullopt,
                             bool update_metadata = true,
                             std::optional<size_t> position = std::nullopt) {
    const std::string no_gr_trace_file = "my_port.js";

    NodeData node;
    node.instance_type = "read-replica";
    node.classic_port =
        classic_port ? *classic_port : port_pool_.get_next_available();
    node.uuid = "uuid-" + std::to_string(node.classic_port);
    node.http_port = port_pool_.get_next_available();
    node.process =
        &mock_server_spawner().spawn(mock_server_cmdline(no_gr_trace_file)
                                         .port(node.classic_port)
                                         .http_port(node.http_port)
                                         .args());

    ASSERT_NO_FATAL_FAILURE(check_port_ready(*node.process, node.http_port));
    EXPECT_TRUE(
        MockServerRestClient(node.http_port).wait_for_rest_endpoint_ready());

    if (!position) {
      cluster_nodes_.push_back(node);
    } else {
      assert(*position <= cluster_nodes_.size());
      cluster_nodes_.insert(cluster_nodes_.begin() + *position, node);
    }

    read_replica_nodes_count_++;
    if (update_metadata) update_cluster_metadata();
  }

  void change_read_only_targets(const std::optional<std::string> &value) {
    if (value)
      router_options_ = R"({"read_only_targets" : ")" + *value + R"("})";
    else
      router_options_ = "{}";
    update_cluster_metadata();
  }

  void set_instance_type(size_t node_id,
                         const std::optional<std::string> &type) {
    cluster_nodes_[node_id].instance_type = type;
    update_cluster_metadata();
  }

  void check_all_ports_used(
      const std::vector<uint16_t> &expected_ports,
      const std::vector<std::pair<uint16_t, std::unique_ptr<MySQLSession>>>
          &created_connections) {
    std::set<uint16_t> used_ports, expected_ports_set;
    for (const auto &con : created_connections) {
      used_ports.insert(con.first);
    }
    for (const auto &port : expected_ports) {
      expected_ports_set.insert(port);
    }

    EXPECT_THAT(expected_ports_set, ::testing::ContainerEq(used_ports));
  }

  std::vector<std::uint16_t> get_md_servers_classic_ports(
      const ClusterSetTopology &cs_topology = {}) const {
    if (is_target_clusterset_) {
      return cs_topology.get_md_servers_classic_ports();
    }

    std::vector<std::uint16_t> result;
    for (const auto &node : cluster_nodes_) {
      if (node.instance_type != "read-replica") {
        result.emplace_back(node.classic_port);
      }
    }

    return result;
  }

  std::vector<std::uint16_t> get_gr_rw_classic_ports() const {
    if (gr_nodes_count_ == 0) {
      return {};
    }
    return {cluster_nodes_[0].classic_port};
  }

  std::vector<std::uint16_t> get_gr_ro_classic_ports() const {
    std::vector<std::uint16_t> result;
    for (size_t i = 1; i < gr_nodes_count_; ++i) {
      result.push_back(cluster_nodes_[i].classic_port);
    }

    return result;
  }

  std::vector<std::uint16_t> get_read_replicas_classic_ports() const {
    std::vector<std::uint16_t> result;
    for (size_t i = gr_nodes_count_;
         i < gr_nodes_count_ + read_replica_nodes_count_; ++i) {
      result.push_back(cluster_nodes_[i].classic_port);
    }

    return result;
  }

  // both GR RO and read replicas
  std::vector<std::uint16_t> get_all_ro_classic_ports() const {
    std::vector<std::uint16_t> result;
    for (size_t i = 1; i < gr_nodes_count_ + read_replica_nodes_count_; ++i) {
      result.push_back(cluster_nodes_[i].classic_port);
    }

    return result;
  }

  std::vector<uint16_t> get_all_cs_ro_classic_ports(
      const ClusterSetTopology &cs_topology, const size_t cluster_id) {
    std::vector<std::uint16_t> result;
    const auto &cluster = cs_topology.clusters[cluster_id];
    for (const auto [i, node] : stdx::views::enumerate(cluster.nodes)) {
      if (i > 0 || cluster.role == "SECONDARY" || cluster.invalid) {
        result.push_back(node.classic_port);
      }
    }

    return result;
  }

  std::string get_uuid() const {
    return is_target_clusterset_ ? "clusterset-uuid" : "uuid";
  }

  auto &launch_router(std::vector<std::uint16_t> md_servers,
                      const unsigned quarantine_threshold = 1,
                      const std::string &configured_cluster_type = "gr",
                      const bool add_static_route = false,
                      const int expected_errorcode = EXIT_SUCCESS,
                      const std::chrono::milliseconds wait_for_notify_ready =
                          kReadyNotifyTimeout) {
    SCOPED_TRACE("// Prepare the dynamic state file for the Router");
    router_state_file = create_state_file(
        temp_test_dir_.name(),
        create_state_file_content("", get_uuid(), md_servers, view_id_));

    router_port_rw = port_pool_.get_next_available();
    router_port_ro = port_pool_.get_next_available();
    if (add_static_route) {
      router_port_static = port_pool_.get_next_available();
    }

    auto writer =
        config_writer(temp_test_dir_.name())
            .section(
                routing_section(router_port_rw, "PRIMARY", "first-available"))
            .section(
                routing_section(router_port_ro, "SECONDARY", "round-robin"))
            .section(metadata_cache_section(configured_cluster_type))
            .section(destination_status_section(quarantine_threshold));

    if (add_static_route) {
      writer.section(
          routing_static_section(router_port_static, router_static_dest_ports));
    }

    const std::string masterkey_file =
        Path(temp_test_dir_.name()).join("master.key").str();
    const std::string keyring_file =
        Path(temp_test_dir_.name()).join("keyring").str();
    mysql_harness::init_keyring(keyring_file, masterkey_file, true);
    mysql_harness::Keyring *keyring = mysql_harness::get_keyring();
    keyring->store("mysql_router1_user", "password", "root");
    mysql_harness::flush_keyring();
    mysql_harness::reset_keyring();

    // launch the router with metadata-cache configuration
    auto &default_section = writer.sections()["DEFAULT"];
    default_section["keyring_path"] = keyring_file;
    default_section["master_key_path"] = masterkey_file;
    default_section["dynamic_state"] = router_state_file;

    return router_spawner()
        .expected_exit_code(expected_errorcode)
        .wait_for_notify_ready(wait_for_notify_ready)
        .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::READY)
        .spawn({"-c", writer.write()});
  }

  void is_target_clusterset(bool v) { is_target_clusterset_ = v; }

  void update_cluster_metadata(std::optional<size_t> node_id = std::nullopt) {
    if (!node_id) {
      // update all nodes
      unsigned gr_pos{0};
      for (auto &cluster_node : cluster_nodes_) {
        if (cluster_node.instance_type == "read-replica") continue;
        set_mock_metadata(cluster_node.http_port, "", gr_pos, cluster_nodes_,
                          router_options_);
        gr_pos++;
      }
    } else {
      // update selected node only
      unsigned gr_pos{0};
      assert(*node_id < cluster_nodes_.size());
      for (unsigned i = 0; i < *node_id; i++)
        if (cluster_nodes_[*node_id].instance_type != "read-replica") gr_pos++;
      set_mock_metadata(cluster_nodes_[*node_id].http_port, "", gr_pos,
                        cluster_nodes_, router_options_);
    }
  }

  std::vector<NodeData> cluster_nodes_;
  size_t gr_nodes_count_;
  size_t read_replica_nodes_count_;
  std::string router_options_;
  unsigned view_id_{1};

  TempDirectory temp_test_dir_;

  std::string router_state_file;
  uint16_t router_port_rw;
  uint16_t router_port_ro;
  uint16_t router_port_static{0};

  std::vector<uint16_t> router_static_dest_ports;
  std::vector<uint16_t> router_static_dest_http_ports;

  static const std::chrono::milliseconds kTTL;
  static const std::chrono::seconds kReadyNotifyTimeout;

 private:
  bool is_target_clusterset_{false};

  std::string get_router_options(const std::string &read_only_targets) {
    return R"({"read_only_targets" : ")" + read_only_targets + R"("})";
  }
};

const std::chrono::milliseconds ReadReplicaTest::kTTL = 50ms;
const std::chrono::seconds ReadReplicaTest::kReadyNotifyTimeout = 30s;

/**
 * @test Check that changes to read_only_targets while the Router is running
 * are handled properly.
 */
TEST_F(ReadReplicaTest, ReadOnlyTargetsChanges) {
  const unsigned initial_gr_nodes_count = 3;
  const unsigned initial_replica_nodes_count = 1;

  create_gr_cluster(initial_gr_nodes_count, initial_replica_nodes_count, "all");
  const auto &router = launch_router(get_md_servers_classic_ports());

  // all
  for (size_t i = 0; i <= gr_nodes_count_ + 1; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));
  }

  change_read_only_targets("read_replicas");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  // read_replicas
  for (size_t i = 0; i <= 2 * read_replica_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_read_replicas_classic_ports(),
                testing::Contains(*port_res));
  }

  change_read_only_targets("secondaries");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  // secondaries
  for (size_t i = 0; i <= gr_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_gr_ro_classic_ports(), testing::Contains(*port_res));
  }

  change_read_only_targets(std::nullopt);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  // unset defaults to "secondaries"
  for (size_t i = 0; i <= gr_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_gr_ro_classic_ports(), testing::Contains(*port_res));
  }

  change_read_only_targets("");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  // empty defaults to "secondaries"
  for (size_t i = 0; i <= gr_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_gr_ro_classic_ports(), testing::Contains(*port_res));
  }
  check_log_contains(
      router,
      "Error parsing read_only_targets from options JSON string: "
      "Unknown read_only_targets read from the metadata: ''. "
      "Using default value. ({\"read_only_targets\" : \"\"})",
      1);

  change_read_only_targets("foo");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  // unrecognised defaults to "secondaries"
  for (size_t i = 0; i <= gr_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_gr_ro_classic_ports(), testing::Contains(*port_res));
  }
  check_log_contains(
      router,
      "Error parsing read_only_targets from options JSON string: "
      "Unknown read_only_targets read from the metadata: 'foo'. "
      "Using default value. ({\"read_only_targets\" : \"foo\"})",
      1);

  // set back valid read_only_targets option
  change_read_only_targets("all");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  check_log_contains(router, "Using read_only_targets='all'", 2);

  // make sure Read Replicas were NOT added to the state file as a metadata
  // servers
  const std::string state_file_path = temp_test_dir_.name() + "/state.json";
  check_state_file(state_file_path, ClusterType::GR_CS, get_uuid(),
                   get_md_servers_classic_ports());
}

/**
 * @test Check that changes to read_only_targets while the Router is running
 * are handled properly when there is only single GR node (RW).
 */
TEST_F(ReadReplicaTest, ReadReplicaModeChangesGRWithOnlyRWNode) {
  const unsigned initial_gr_nodes_count = 1;
  const unsigned initial_read_replica_nodes_count = 0;

  create_gr_cluster(initial_gr_nodes_count, initial_read_replica_nodes_count,
                    "all");
  launch_router(get_md_servers_classic_ports());

  auto conn_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(conn_res);
  auto port_res = select_port(conn_res->get());
  ASSERT_NO_ERROR(port_res);
  EXPECT_EQ(*port_res, cluster_nodes_[0].classic_port);
  verify_new_connection_fails(router_port_ro);

  // add Read Replica node to the Cluster
  add_read_replica_node();
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  // check it is used for RO connections
  {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
  }

  for (size_t i = 0; i < read_replica_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_[1].classic_port);
  }

  change_read_only_targets("secondaries");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    EXPECT_EQ(*port_res, cluster_nodes_[0].classic_port);
  }
  // no RO connection should be possible now
  verify_new_connection_fails(router_port_ro);

  change_read_only_targets("read_replicas");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_[0].classic_port);
  }
  // RO connections should be possible again
  for (size_t i = 0; i < 2 * read_replica_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_[1].classic_port);
  }
}

/**
 * @test Check that unexpected instances types in the metadata are handled
 * properly.
 */
TEST_F(ReadReplicaTest, ReadReplicaInstanceType) {
  const unsigned gr_nodes_count = 3;
  const unsigned replica_nodes_count = 1;
  const auto read_replica_node_id = gr_nodes_count;

  create_gr_cluster(gr_nodes_count, replica_nodes_count, "read_replicas");
  const auto &router = launch_router(get_md_servers_classic_ports());

  // the read_only_targets=read_replicas so the Router should only use RR node
  // for RO connections
  for (size_t i = 0; i < 2 * replica_nodes_count; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_[read_replica_node_id].classic_port);
  }

  set_instance_type(read_replica_node_id, "group-member");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  // no read-replica and the read_only_targets is "read_replicas" so the RO
  // connection should not be possible
  verify_new_connection_fails(router_port_ro);

  set_instance_type(read_replica_node_id, std::nullopt);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  // no read-replica and read_only_targets is "read_replicas" so the RO
  // connection should not be possible
  verify_new_connection_fails(router_port_ro);

  set_instance_type(read_replica_node_id, "");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  // no read-replica and read_only_targets is "read_replicas" so the RO
  // connection should not be possible
  verify_new_connection_fails(router_port_ro);

  check_log_contains(router,
                     "Error parsing instance_type from attributes JSON string: "
                     "Unknown attributes.instance_type value: ''",
                     1);

  set_instance_type(read_replica_node_id, "foo");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  // no read-replica and the read_only_targets is "read_replicas" so the RO
  // connection should not be possible
  verify_new_connection_fails(router_port_ro);

  check_log_contains(router,
                     "Error parsing instance_type from attributes JSON string: "
                     "Unknown attributes.instance_type value: 'foo'",
                     1);
}

/**
 * @test Check that Read Replicas are handled properly when added and removed
 * once the Router is running.
 */
TEST_F(ReadReplicaTest, ReadReplicaAddRemove) {
  const unsigned initial_gr_nodes_count = 3;
  const unsigned initial_read_replica_nodes_count = 1;

  create_gr_cluster(initial_gr_nodes_count, initial_read_replica_nodes_count,
                    "all");
  launch_router(get_md_servers_classic_ports());

  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  // the read_only_targets is all so the Router should use both GR
  // secondaries and RR nodes for RO connections
  for (size_t i = 0; i < gr_nodes_count_ + read_replica_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));
  }

  // add new RR to the Cluster, check that it is used for RO connections
  add_read_replica_node();
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  for (size_t i = 0; i < gr_nodes_count_ + read_replica_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));
  }

  for (size_t i = 0; i < 2; i++) {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_gr_rw_classic_ports(), testing::Contains(*port_res));
  }

  change_read_only_targets("secondaries");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  for (size_t i = 0; i < 2 * gr_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_gr_ro_classic_ports(), testing::Contains(*port_res));
  }

  change_read_only_targets("read_replicas");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  for (size_t i = 0; i < 2 * read_replica_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_read_replicas_classic_ports(),
                testing::Contains(*port_res));
  }

  change_read_only_targets("secondaries");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  for (size_t i = 0; i < gr_nodes_count_ + read_replica_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));
  }

  // remove first RR
  remove_node(/*id=*/gr_nodes_count_);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  for (size_t i = 0; i < 2 * (gr_nodes_count_ + read_replica_nodes_count_);
       i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));
  }

  change_read_only_targets("read_replicas");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  for (size_t i = 0; i < 2 * read_replica_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_read_replicas_classic_ports(),
                testing::Contains(*port_res));
  }

  // remove last remaining RR
  remove_node(/*id=*/gr_nodes_count_);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  verify_new_connection_fails(router_port_ro);

  change_read_only_targets("secondaries");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));
  for (size_t i = 0; i < 2 * gr_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_gr_ro_classic_ports(), testing::Contains(*port_res));
  }
}

class ReadReplicaQuarantinedTest
    : public ReadReplicaTest,
      public ::testing::WithParamInterface<unsigned> {};

/**
 * @test Check that Read Replicas are quarantined properly when cannot be
 * accessed for user connections ("read_replicas" read_only_targets).
 */
TEST_P(ReadReplicaQuarantinedTest, ReadReplicaQuarantined) {
  // [ A ] - GR RW node
  // [ B, C ] - GR RO nodes
  // [ D, E ] - read replicas
  const unsigned gr_rw_nodes_count = 1;
  const unsigned gr_ro_nodes_count = 2;
  const unsigned gr_nodes_count = gr_rw_nodes_count + gr_ro_nodes_count;
  unsigned read_replica_nodes_count = 2;

  create_gr_cluster(gr_nodes_count, read_replica_nodes_count, "read_replicas");
  const unsigned quarantine_threshold = GetParam();
  const auto &router =
      launch_router(get_md_servers_classic_ports(), quarantine_threshold);

  SCOPED_TRACE("// remove first read replica [D]");
  const auto classic_port_E = cluster_nodes_[4].classic_port;
  const auto classic_port_D = remove_node(gr_nodes_count, false);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  for (size_t i = 0; i < 2 * quarantine_threshold + quarantine_threshold % 2;
       i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_[gr_nodes_count].classic_port);
  }

  ASSERT_NO_FATAL_FAILURE(check_log_contains(
      router,
      "add destination '127.0.0.1:" + std::to_string(classic_port_D) +
          "' to quarantine",
      1));

  SCOPED_TRACE("// bring back read replica [D]");
  add_read_replica_node(classic_port_D, false, /*position=*/gr_nodes_count);

  EXPECT_TRUE(wait_log_contains(
      router,
      "Destination candidate '127.0.0.1:" + std::to_string(classic_port_D) +
          "' is available, remove it from quarantine",
      10s));

  SCOPED_TRACE("// check RR [D] is back in the rotation");
  const auto expected_ports =
      std::vector<uint16_t>{classic_port_D, classic_port_E};
  std::vector<std::pair<uint16_t, std::unique_ptr<MySQLSession>>> ro_cons;
  for (size_t i = 0; i < 2 * quarantine_threshold + 1; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(expected_ports, testing::Contains(*port_res));

    ro_cons.emplace_back(*port_res, std::move(*conn_res));
  }
  check_all_ports_used(expected_ports, ro_cons);

  SCOPED_TRACE("// remove second read replica [E] now");
  remove_node(gr_nodes_count + 1, false);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  for (size_t i = 0; i < 2 * quarantine_threshold + 1; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, classic_port_D);
  }

  ASSERT_NO_FATAL_FAILURE(check_log_contains(
      router,
      "add destination '127.0.0.1:" + std::to_string(classic_port_E) +
          "' to quarantine",
      1));

  SCOPED_TRACE("// remove the first RR [D] again");
  remove_node(gr_nodes_count, false);

  SCOPED_TRACE("// triggering quarantine: threshold=" +
               std::to_string(quarantine_threshold));
  for (size_t i = 0; i < quarantine_threshold; i++) {
    ASSERT_NO_FATAL_FAILURE(verify_new_connection_fails(router_port_ro));
  }

  ASSERT_NO_FATAL_FAILURE(check_log_contains(
      router,
      "add destination '127.0.0.1:" + std::to_string(classic_port_D) +
          "' to quarantine",
      2));

  SCOPED_TRACE("// bring back second RR [E]");
  add_read_replica_node(classic_port_E, false, /*position=*/gr_nodes_count);

  EXPECT_TRUE(wait_log_contains(
      router,
      "Destination candidate '127.0.0.1:" + std::to_string(classic_port_E) +
          "' is available, remove it from quarantine",
      10s));

  for (size_t i = 0; i < 2 * quarantine_threshold; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, classic_port_E);
  }
}

INSTANTIATE_TEST_SUITE_P(ReadReplicaQuarantined, ReadReplicaQuarantinedTest,
                         ::testing::Values(1, 2));

/**
 * @test Check that Read Replicas quarantined properly when cannot be accessed
 * for user connections (read_only_targets = "all").
 */
TEST_F(ReadReplicaTest, ReadReplicaQuarantinedReadOnlyTargetsAll) {
  // [ A ] - GR RW node
  // [ B, C ] - GR RO nodes
  // [ D, E ] - read replicas
  const unsigned initial_gr_nodes_count = 3;
  const unsigned initial_read_replica_nodes_count = 2;

  create_gr_cluster(initial_gr_nodes_count, initial_read_replica_nodes_count,
                    "all");
  const unsigned quarantine_threshold = 1;
  const auto &router =
      launch_router(get_md_servers_classic_ports(), quarantine_threshold);

  // remove first read replica [D]
  const auto classic_port_D = remove_node(gr_nodes_count_, false);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  for (size_t i = 0; i < gr_nodes_count_ + read_replica_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));
  }

  check_log_contains(router,
                     "add destination '127.0.0.1:" +
                         std::to_string(classic_port_D) + "' to quarantine",
                     1);

  // bring back read replica [D]
  add_read_replica_node(classic_port_D, false, /*position=*/gr_nodes_count_);

  EXPECT_TRUE(wait_log_contains(
      router,
      "Destination candidate '127.0.0.1:" + std::to_string(classic_port_D) +
          "' is available, remove it from quarantine",
      10s));

  // check RR [D] is back in the rotation
  for (size_t i = 0; i < gr_nodes_count_ + read_replica_nodes_count_; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));
  }

  // remove second read replica [E] now
  const auto classic_port_E = remove_node(gr_nodes_count_ + 1, false);
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  for (size_t i = 0; i <= gr_nodes_count_ + read_replica_nodes_count_ + 1;
       i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));
  }

  check_log_contains(router,
                     "add destination '127.0.0.1:" +
                         std::to_string(classic_port_E) + "' to quarantine",
                     1);

  // remove the first RR [D] again
  remove_node(gr_nodes_count_, false);
  const auto classic_port_B = cluster_nodes_[1].classic_port;
  const auto classic_port_C = cluster_nodes_[2].classic_port;
  for (size_t i = 0; i < 2 * (gr_nodes_count_ + read_replica_nodes_count_);
       i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);

    std::array<uint16_t, 2> classic_ports{classic_port_B, classic_port_C};
    EXPECT_THAT(classic_ports, ::testing::Contains(*port_res));
  }

  check_log_contains(router,
                     "add destination '127.0.0.1:" +
                         std::to_string(classic_port_D) + "' to quarantine",
                     2);

  // bring back second RR [E]
  add_read_replica_node(classic_port_E, false, /*position=*/gr_nodes_count_);

  EXPECT_TRUE(wait_log_contains(
      router,
      "Destination candidate '127.0.0.1:" + std::to_string(classic_port_E) +
          "' is available, remove it from quarantine",
      10s));

  for (size_t i = 2; i < 2 * quarantine_threshold + 2; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));
  }
}

/**
 * @test Check that Read Replicas are handled as expected when used with a
 * ReplicaSet(ignored and warning logged).
 */
TEST_F(ReadReplicaTest, ReadReplicaInAsyncReplicaCluster) {
  // [ A ] - ReplicaSet Primary
  // [ B, C ] - ReplicaSet Secondaries
  // [ D, E ] - Read Replica
  const unsigned ar_nodes_count = 3;
  const unsigned replica_nodes_count = 2;

  const unsigned ar_rw_nodes_count = 1;
  const unsigned ar_ro_nodes_count = ar_nodes_count - ar_rw_nodes_count;

  create_ar_cluster(ar_nodes_count, replica_nodes_count);
  const auto &router = launch_router(get_md_servers_classic_ports(), 1, "rs");

  // We sleep to verify that the warning that we check is only logged once
  // despite several metadata_cache refresh cycles
  std::this_thread::sleep_for(200ms);

  // only ReplicaSet secondaries should be used for RO connections
  for (size_t i = 0; i < 2 * ar_ro_nodes_count; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_gr_ro_classic_ports(), ::testing::Contains(*port_res));
  }

  const auto classic_port_D = cluster_nodes_[3].classic_port;
  const auto classic_port_E = cluster_nodes_[4].classic_port;
  // Read Replicas should be ignored
  for (const auto port : {classic_port_D, classic_port_E}) {
    check_log_contains(router,
                       "Ignoring unsupported instance 127.0.0.1:" +
                           std::to_string(port) + ", type: 'read-replica'",
                       1);
  }
}

/**
 * @test Check that having GR Cluster with some Read Replicas and static route
 * configured works as expected (the RRs do not "leak" into static route).
 */
TEST_F(ReadReplicaTest, ReadReplicaGRPlusStaticRouting) {
  // [ A ] - GR RW node
  // [ B, C ] - Read Replicas nodes
  // [ D, E ] - non-cluster nodes for static routing
  const unsigned gr_rw_nodes_count = 1;
  const unsigned read_replica_nodes_count = 2;
  const unsigned static_dest_count = 2;

  create_gr_cluster(gr_rw_nodes_count, read_replica_nodes_count, "all");
  launch_static_destinations(static_dest_count);
  launch_router(get_md_servers_classic_ports(), /*quarantine_threshold*/ 1,
                "gr",
                /*add_static_route*/ true);

  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  for (size_t i = 0; i < 2; i++) {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_[0].classic_port);
  }

  for (size_t i = 0; i < read_replica_nodes_count + 1; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_read_replicas_classic_ports(),
                testing::Contains(*port_res));
  }

  for (size_t i = 0; i < static_dest_count + 1; i++) {
    auto conn_res = make_new_connection(router_port_static);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    std::array dest_ports{router_static_dest_ports[0],
                          router_static_dest_ports[1]};
    EXPECT_THAT(dest_ports, testing::Contains(*port_res));
  }
}

/**
 * @test Check that having ReplicaSet Cluster with some Read Replicas
 * (unexpected) and static route configured works as expected: the RRs do not
 * "leak" into static route and are not used for ReplicaSet either.
 */
TEST_F(ReadReplicaTest, ReadReplicaReplicaSetPlusStaticRouting) {
  // [ A ] - ReplicaSet RW node
  // [ B, C ] - RR nodes
  // [ D, E ] - non-cluster nodes for static routing
  const unsigned ar_rw_nodes_count = 1;
  const unsigned read_replica_nodes_count = 2;
  const unsigned static_dest_count = 2;

  create_ar_cluster(ar_rw_nodes_count, read_replica_nodes_count);
  launch_static_destinations(static_dest_count);
  launch_router(get_md_servers_classic_ports(), /*quarantine_threshold*/ 1,
                "rs",
                /*add_static_route*/ true);

  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  for (size_t i = 0; i < ar_rw_nodes_count + 1; i++) {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_[0].classic_port);
  }

  verify_new_connection_fails(router_port_ro);

  for (size_t i = 0; i < static_dest_count + 1; i++) {
    auto conn_res = make_new_connection(router_port_static);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(router_static_dest_ports, testing::Contains(*port_res));
  }
}

/**
 * @test Check that Read Replicas are handled as expected when used with a
 * ClusterSet.
 */
TEST_F(ReadReplicaTest, ReadReplicaClusterSet) {
  const unsigned primary_gr_ro_nodes_count = 2;
  const unsigned primary_read_replicas_nodes_count = 2;
  const unsigned replica1_gr_nodes_count = 3;
  const unsigned replica1_read_replicas_nodes_count = 1;
  std::string router_options =
      get_router_options_as_json_str("primary", std::nullopt, "all");

  ClusterSetOptions cs_options;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  cs_options.gr_nodes_number = std::vector<size_t>{3, 3, 3};
  cs_options.read_replicas_number = std::vector<size_t>{
      primary_read_replicas_nodes_count, replica1_read_replicas_nodes_count, 0};
  create_clusterset(cs_options);

  is_target_clusterset(true);

  launch_router(get_md_servers_classic_ports(cs_options.topology));

  // read_only_targets is 'all' so both 2 RO nodes of the Primary Cluster
  // and 2 RRs should be used
  const auto expected_ports =
      get_all_cs_ro_classic_ports(cs_options.topology, 0);
  std::vector<std::pair<uint16_t, std::unique_ptr<MySQLSession>>> ro_cons;
  for (size_t i = 0;
       i < 2 * (primary_gr_ro_nodes_count + primary_read_replicas_nodes_count);
       i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(expected_ports, ::testing::Contains(*port_res));

    ro_cons.emplace_back(*port_res, std::move(*conn_res));
  }
  check_all_ports_used(expected_ports, ro_cons);

  // change the target cluster to secondary cluster
  cs_options.view_id = ++view_id_;
  cs_options.target_cluster_id = 1;
  cs_options.router_options = get_router_options_as_json_str(
      "00000000-0000-0000-0000-0000000000g2", std::nullopt, "all");

  set_mock_metadata_on_all_cs_nodes(cs_options);
  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  ro_cons.clear();
  // read_only_targets is 'all' so all 3 RO nodes of the second Cluster and
  // 1 RR should be used
  const auto expected_ports_2 =
      get_all_cs_ro_classic_ports(cs_options.topology, 1);
  for (size_t i = 0;
       i < 2 * (replica1_gr_nodes_count + replica1_read_replicas_nodes_count);
       i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(expected_ports_2, ::testing::Contains(*port_res));

    ro_cons.emplace_back(*port_res, std::move(*conn_res));
  }
  check_all_ports_used(expected_ports_2, ro_cons);

  // change the target cluster back to primary cluster
  cs_options.view_id = ++view_id_;
  cs_options.target_cluster_id = 0;
  cs_options.router_options =
      get_router_options_as_json_str("primary", std::nullopt, "all");
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  ro_cons.clear();
  // read_only_targets is 'all' so both 2 RO nodes of the Primary Cluster
  // and 2 RRs should be used
  for (size_t i = 0;
       i < 2 * (primary_gr_ro_nodes_count + primary_read_replicas_nodes_count);
       i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(expected_ports, testing::Contains(*port_res));

    ro_cons.emplace_back(*port_res, std::move(*conn_res));
  }
  check_all_ports_used(expected_ports, ro_cons);
}

struct ReadReplicaInvalidatedClusterTestParam {
  std::optional<std::string> invalidated_cluster_policy;
  bool expect_RO_connections_allowed;
};

class ReadReplicaInvalidatedClusterTest
    : public ReadReplicaTest,
      public ::testing::WithParamInterface<
          ReadReplicaInvalidatedClusterTestParam> {};

/**
 * @test Check that Read Replicas are handled as expected when used with a
 * ClusterSet and the target Cluster is marked as invalid in the metadata
 * (invalidated_cluster_policy is honored).
 */
TEST_P(ReadReplicaInvalidatedClusterTest,
       ReadReplicaClusterSetInvalidatedCluster) {
  const unsigned primary_gr_ro_nodes_count = 2;
  const unsigned primary_read_replicas_nodes_count = 2;
  const unsigned replica1_read_replicas_nodes_count = 1;
  const std::string router_options = get_router_options_as_json_str(
      "primary", GetParam().invalidated_cluster_policy, "all");

  ClusterSetOptions cs_options;
  cs_options.view_id = view_id_;
  cs_options.tracefile = "metadata_clusterset.js";
  cs_options.router_options = router_options;
  cs_options.gr_nodes_number = std::vector<size_t>{3, 3, 3};
  cs_options.read_replicas_number = std::vector<size_t>{
      primary_read_replicas_nodes_count, replica1_read_replicas_nodes_count, 0};
  create_clusterset(cs_options);
  is_target_clusterset(true);

  launch_router(get_md_servers_classic_ports(cs_options.topology));

  auto rw_con_res = make_new_connection(router_port_rw);
  ASSERT_NO_ERROR(rw_con_res);
  auto rw_port_res = select_port(rw_con_res->get());
  ASSERT_NO_ERROR(rw_port_res);
  EXPECT_EQ(*rw_port_res,
            cs_options.topology.clusters[0].nodes[0].classic_port);
  auto rw_con = std::move(*rw_con_res);

  const auto expected_ports =
      get_all_cs_ro_classic_ports(cs_options.topology, 0);
  std::vector<std::pair<uint16_t, std::unique_ptr<MySQLSession>>> ro_cons;
  // read_only_targets is 'all' so both 2 RO nodes of the Primary Cluster
  // and 2 RRs should be used
  for (size_t i = 0;
       i < 2 * (primary_gr_ro_nodes_count + primary_read_replicas_nodes_count);
       i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(expected_ports, testing::Contains(*port_res));

    ro_cons.emplace_back(*port_res, std::move(*conn_res));
  }
  check_all_ports_used(expected_ports, ro_cons);

  // mark the target_cluster as invalid
  cs_options.topology.clusters[0].invalid = true;
  cs_options.view_id = ++view_id_;
  set_mock_metadata_on_all_cs_nodes(cs_options);

  EXPECT_TRUE(wait_for_transaction_count_increase(
      cs_options.topology.clusters[0].nodes[0].http_port, 2));

  verify_existing_connection_dropped(rw_con.get());
  verify_new_connection_fails(router_port_rw);

  if (GetParam().expect_RO_connections_allowed) {
    // cluster is invalidated in the metadata but the invalidated_cluster_policy
    // allows RO connections so the RO connections should still be possible and
    // RRs should also be used for them
    for (const auto &ro_con : ro_cons) {
      verify_existing_connection_ok(ro_con.second.get());
    }

    ro_cons.clear();
    const auto expected_ports_2 =
        get_all_cs_ro_classic_ports(cs_options.topology, 0);
    for (size_t i = 0; i < 2 * (primary_gr_ro_nodes_count +
                                primary_read_replicas_nodes_count);
         i++) {
      auto conn_res = make_new_connection(router_port_ro);
      ASSERT_NO_ERROR(conn_res);
      auto port_res = select_port(conn_res->get());
      ASSERT_NO_ERROR(port_res);
      EXPECT_THAT(expected_ports_2, testing::Contains(*port_res));

      ro_cons.emplace_back(*port_res, std::move(*conn_res));
    }
    check_all_ports_used(expected_ports_2, ro_cons);
  } else {
    // cluster is invalidated in the metadata and the invalidated_cluster_policy
    // does not allow RO connections so no new RO connections should be
    // possible and old ones should be dropped
    for (const auto &ro_con : ro_cons) {
      verify_existing_connection_dropped(ro_con.second.get());
    }

    verify_new_connection_fails(router_port_ro);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ReadReplicaClusterSetInvalidatedCluster, ReadReplicaInvalidatedClusterTest,
    ::testing::Values(ReadReplicaInvalidatedClusterTestParam{"drop_all", false},
                      ReadReplicaInvalidatedClusterTestParam{"accept_ro", true},
                      ReadReplicaInvalidatedClusterTestParam{std::nullopt,
                                                             false},
                      ReadReplicaInvalidatedClusterTestParam{"", false},
                      ReadReplicaInvalidatedClusterTestParam{"foo", false}));

class ReadReplicaNoQuorumTest
    : public ReadReplicaTest,
      public ::testing::WithParamInterface<std::string> {};

/**
 * @test Check that Read Replicas are handled as expected when GR has no quorum
 * (Router does not accept any connections).
 */
TEST_P(ReadReplicaNoQuorumTest, ReadReplicaGRNoQuorum) {
  const unsigned initial_gr_nodes_count = 3;
  const unsigned initial_replica_nodes_count = 2;

  create_gr_cluster(initial_gr_nodes_count, initial_replica_nodes_count, "all");
  /*const auto &router =*/launch_router(get_md_servers_classic_ports());

  // all
  for (size_t i = 0; i <= initial_gr_nodes_count + initial_replica_nodes_count;
       i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));
  }

  // set secondary GR nodes of the cluster to the selected state
  cluster_nodes_[1].gr_node_status = GetParam();
  cluster_nodes_[2].gr_node_status = GetParam();
  update_cluster_metadata();
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  verify_new_connection_fails(router_port_rw);
  verify_new_connection_fails(router_port_ro);

  change_read_only_targets("read_replicas");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  verify_new_connection_fails(router_port_rw);
  verify_new_connection_fails(router_port_ro);

  change_read_only_targets("secondaries");
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  verify_new_connection_fails(router_port_rw);
  verify_new_connection_fails(router_port_ro);
}

INSTANTIATE_TEST_SUITE_P(ReadReplicaGRNoQuorum, ReadReplicaNoQuorumTest,
                         ::testing::Values("OFFLINE", "UNREACHABLE"));

/**
 * @test Check that hiding Read Replica nodes works as expected when
 * read_only_targets="all" option is used
 */
TEST_F(ReadReplicaTest, HidingNodesReadOnlyTargetsAll) {
  // [ A ] - GR RW node
  // [ B, C ] - GR RO nodes
  // [ D, E ] - RR nodes
  const unsigned gr_nodes_count = 3;
  const unsigned read_replica_nodes_count = 2;
  create_gr_cluster(gr_nodes_count, read_replica_nodes_count, "all");
  launch_router(get_md_servers_classic_ports());

  std::vector<std::pair<uint16_t, std::unique_ptr<MySQLSession>>> ro_cons;

  for (size_t i = 0; i < gr_nodes_count + read_replica_nodes_count; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_all_ro_classic_ports(), testing::Contains(*port_res));

    ro_cons.emplace_back(*port_res, std::move(*conn_res));
  }

  SCOPED_TRACE("// hide first RR (D)");
  cluster_nodes_[3].hidden = true;
  update_cluster_metadata();
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  SCOPED_TRACE(
      "// existing connection to D should be dropped, all the rest should be "
      "ok");
  for (const auto &con : ro_cons) {
    if (con.first == cluster_nodes_[3].classic_port) {
      ASSERT_NO_FATAL_FAILURE(
          verify_existing_connection_dropped(con.second.get()));
    } else {
      ASSERT_NO_FATAL_FAILURE(verify_existing_connection_ok(con.second.get()));
    }
  }

  SCOPED_TRACE("// new connections should not reach D");
  {
    std::vector<uint16_t> ro_dest_nodes{cluster_nodes_[1].classic_port,
                                        cluster_nodes_[2].classic_port,
                                        cluster_nodes_[4].classic_port};
    for (size_t i = 0; i < gr_nodes_count + read_replica_nodes_count; i++) {
      auto conn_res = make_new_connection(router_port_ro);
      ASSERT_NO_ERROR(conn_res);
      auto port_res = select_port(conn_res->get());
      ASSERT_NO_ERROR(port_res);
      EXPECT_THAT(ro_dest_nodes, testing::Contains(*port_res));
    }
  }

  SCOPED_TRACE("// hide second RR (E) but make it keep existing connections");
  cluster_nodes_[4].hidden = true;
  cluster_nodes_[4].disconnect_existing_sessions_when_hidden = false;
  update_cluster_metadata();
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  SCOPED_TRACE("// connection to D should be dropped but to E should be kept");
  for (const auto &con : ro_cons) {
    if (con.first == cluster_nodes_[3].classic_port) {
      ASSERT_NO_FATAL_FAILURE(
          verify_existing_connection_dropped(con.second.get()));
    } else {
      ASSERT_NO_FATAL_FAILURE(verify_existing_connection_ok(con.second.get()));
    }
  }

  SCOPED_TRACE("// new connections should not reach D nor E");
  {
    for (size_t i = 0; i < gr_nodes_count + read_replica_nodes_count; i++) {
      auto conn_res = make_new_connection(router_port_ro);
      ASSERT_NO_ERROR(conn_res);
      auto port_res = select_port(conn_res->get());
      ASSERT_NO_ERROR(port_res);
      EXPECT_THAT(get_gr_ro_classic_ports(), testing::Contains(*port_res));
    }
  }

  SCOPED_TRACE("// un-hide node D");
  cluster_nodes_[3].hidden = false;
  update_cluster_metadata();
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  SCOPED_TRACE("// it should be in the rotation for new connections again");
  {
    std::vector<uint16_t> ro_dest_nodes{cluster_nodes_[1].classic_port,
                                        cluster_nodes_[2].classic_port,
                                        cluster_nodes_[3].classic_port};
    for (size_t i = 0; i < gr_nodes_count + read_replica_nodes_count; i++) {
      auto conn_res = make_new_connection(router_port_ro);
      ASSERT_NO_ERROR(conn_res);
      auto port_res = select_port(conn_res->get());
      ASSERT_NO_ERROR(port_res);
      EXPECT_THAT(ro_dest_nodes, testing::Contains(*port_res));
    }
  }
}

/**
 * @test Check that hiding Read Replica nodes works as expected when
 * read_only_targets="read_replicas" option is used
 */
TEST_F(ReadReplicaTest, HidingNodesReadReplicas) {
  // [ A ] - GR RW node
  // [ B, C ] - GR RO nodes
  // [ D, E ] - RR nodes
  const unsigned gr_nodes_count = 3;
  const unsigned replica_nodes_count = 2;

  create_gr_cluster(gr_nodes_count, replica_nodes_count, "read_replicas");
  launch_router(get_md_servers_classic_ports());

  std::vector<std::pair<uint16_t, std::unique_ptr<MySQLSession>>> ro_cons;

  for (size_t i = 0; i < replica_nodes_count; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_read_replicas_classic_ports(),
                testing::Contains(*port_res));

    ro_cons.emplace_back(*port_res, std::move(*conn_res));
  }

  SCOPED_TRACE("// hide first RR (D)");
  cluster_nodes_[3].hidden = true;
  update_cluster_metadata();
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  SCOPED_TRACE(
      "// existing connection to D should be dropped, the one to E should be "
      "kept");
  for (const auto &con : ro_cons) {
    if (con.first == cluster_nodes_[3].classic_port) {
      ASSERT_NO_FATAL_FAILURE(
          verify_existing_connection_dropped(con.second.get()));
    } else {
      ASSERT_NO_FATAL_FAILURE(verify_existing_connection_ok(con.second.get()));
    }
  }

  SCOPED_TRACE("// new connections should not reach D, all should go to E");
  for (size_t i = 0; i < replica_nodes_count; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_[4].classic_port);
  }

  SCOPED_TRACE("// hide second RR (E) but make it keep existing connections");
  cluster_nodes_[4].hidden = true;
  cluster_nodes_[4].disconnect_existing_sessions_when_hidden = false;
  update_cluster_metadata();
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  SCOPED_TRACE("// connection to D should be dropped but to E should be kept");
  for (const auto &con : ro_cons) {
    if (con.first == cluster_nodes_[3].classic_port) {
      ASSERT_NO_FATAL_FAILURE(
          verify_existing_connection_dropped(con.second.get()));
    } else {
      ASSERT_NO_FATAL_FAILURE(verify_existing_connection_ok(con.second.get()));
    }
  }

  SCOPED_TRACE(
      "// there is no valid RO destination so the port should be closed");
  verify_new_connection_fails(router_port_ro);

  SCOPED_TRACE("// un-hide both read replicas");
  cluster_nodes_[3].hidden = false;
  cluster_nodes_[4].hidden = false;
  update_cluster_metadata();
  EXPECT_TRUE(
      wait_for_transaction_count_increase(cluster_nodes_[0].http_port, 3));

  SCOPED_TRACE(
      "// both D and E should be in the rotation for new connections again");
  for (size_t i = 0; i < 4; i++) {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(get_read_replicas_classic_ports(),
                testing::Contains(*port_res));
  }
}

class MetadataUnavailableTest
    : public ReadReplicaTest,
      public ::testing::WithParamInterface<std::string> {};

/**
 * @test Check that when the GR nodes are not available the Router does not try
 * to reach read replicas for the metadata regardless of read_only_targets
 */
TEST_P(MetadataUnavailableTest, MetadataUnavailable) {
  // [ A ] - GR RW node
  // [ B, C ] - GR RO nodes
  // [ D, E ] - RR nodes
  const unsigned gr_nodes_count = 3;
  const unsigned replica_nodes_count = 2;

  create_gr_cluster(gr_nodes_count, replica_nodes_count, GetParam());
  auto &router = launch_router(get_md_servers_classic_ports());

  // check that RW/RO ports are open
  {
    auto conn_res = make_new_connection(router_port_rw);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_EQ(*port_res, cluster_nodes_[0].classic_port);
  }

  std::vector<uint16_t> ro_nodes_ports;
  for (size_t i = 1; i < cluster_nodes_.size(); ++i) {
    ro_nodes_ports.push_back(cluster_nodes_[i].classic_port);
  }

  {
    auto conn_res = make_new_connection(router_port_ro);
    ASSERT_NO_ERROR(conn_res);
    auto port_res = select_port(conn_res->get());
    ASSERT_NO_ERROR(port_res);
    EXPECT_THAT(ro_nodes_ports, testing::Contains(*port_res));
  }

  // kill all 3 GR nodes making them unavailable to the Router when querying
  // metadata
  for (auto &node : cluster_nodes_) {
    if (node.instance_type != "read-replica") {
      node.process->kill();
      check_exit_code(*node.process, EXIT_SUCCESS, 5s);
    }
  }

  // the Router should complain about no metadata server available and shut
  // down the accepting ports
  const std::string rw = std::to_string(router_port_rw);
  const std::string ro = std::to_string(router_port_ro);
  const std::vector<std::string> expected_log_lines{
      "ERROR .* Failed fetching metadata from any of the 3 metadata servers",
      "INFO .* Stop accepting connections for routing routing:test_default" +
          rw + " listening on 127.0.0.1:" + rw,
      "INFO .* Stop accepting connections for routing routing:test_default" +
          ro + " listening on 127.0.0.1:" + ro};
  for (const auto &expected_line : expected_log_lines) {
    EXPECT_TRUE(wait_log_contains(router, expected_line, 5s));
  }

  verify_new_connection_fails(router_port_rw);
  verify_new_connection_fails(router_port_ro);

  // the state file should still only contain GR nodes
  const std::string state_file_path = temp_test_dir_.name() + "/state.json";
  check_state_file(state_file_path, ClusterType::GR_CS, get_uuid(),
                   get_md_servers_classic_ports());
}

INSTANTIATE_TEST_SUITE_P(MetadataUnavailable, MetadataUnavailableTest,
                         ::testing::Values("all", "read_replicas",
                                           "secondaries"));

int main(int argc, char *argv[]) {
  init_windows_sockets();
  g_origin_path = Path(argv[0]).dirname();
  ProcessManager::set_origin(g_origin_path);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
