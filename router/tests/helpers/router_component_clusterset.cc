/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "router_component_clusterset.h"

#include "mock_server_testutils.h"
#include "mysqlrouter/mock_server_rest_client.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION

void RouterComponentClusterSetTest::create_clusterset(
    ClusterSetOptions &cs_options) {
  auto &cs_topology = cs_options.topology;

  cs_topology.primary_cluster_id = cs_options.primary_cluster_id;

  // first create a ClusterSet topology
  for (unsigned cluster_id = 0; cluster_id < cs_options.gr_nodes_number.size();
       ++cluster_id) {
    ClusterData cluster_data;
    cluster_data.id = cluster_id;
    cluster_data.primary_node_id = 0;
    // 0-based -> 1-based
    const std::string id = std::to_string(cluster_id + 1);

    cluster_data.uuid = "00000000-0000-0000-0000-0000000000c" + id;
    cluster_data.name = "cluster-name-" + id;
    cluster_data.gr_uuid = "00000000-0000-0000-0000-0000000000g" + id;

    const size_t gr_nodes_num = cs_options.gr_nodes_number[cluster_id];
    const size_t read_replicas_num =
        cluster_id < cs_options.read_replicas_number.size()
            ? cs_options.read_replicas_number[cluster_id]
            : 0;

    for (unsigned node_id = 0; node_id < gr_nodes_num + read_replicas_num;
         ++node_id) {
      ClusterNode cluster_node;
      cluster_node.is_read_replica = node_id >= gr_nodes_num;
      cluster_node.uuid = "00000000-0000-0000-0000-0000000000" +
                          std::to_string(cluster_id + 1) +
                          std::to_string(node_id + 1);
      cluster_node.host = "127.0.0.1";
      cluster_node.classic_port = port_pool_.get_next_available();
      cluster_node.http_port = port_pool_.get_next_available();
      if (cs_options.use_gr_notifications && !cluster_node.is_read_replica) {
        cluster_node.x_port = port_pool_.get_next_available();
      }

      cluster_data.nodes.push_back(cluster_node);
    }

    for (unsigned node_id = 0; node_id < gr_nodes_num; ++node_id) {
      const std::string role = node_id == 0 ? "PRIMARY" : "SECONDARY";
      GRNode gr_node{cluster_data.nodes[node_id].classic_port,
                     "00000000-0000-0000-0000-0000000000" +
                         std::to_string(cluster_id + 1) +
                         std::to_string(node_id + 1),
                     "ONLINE", role};

      cluster_data.gr_nodes.push_back(gr_node);
    }

    cs_topology.clusters.push_back(cluster_data);
  }

  change_clusterset_primary(cs_topology, cs_options.primary_cluster_id);

  // now launch the mock servers and set their metadata
  for (unsigned cluster_id = 0; cluster_id < cs_topology.clusters.size();
       ++cluster_id) {
    auto &cluster = cs_topology.clusters[cluster_id];
    for (unsigned node_id = 0; node_id < cluster.nodes.size(); ++node_id) {
      auto &node = cluster.nodes[node_id];
      node.process =
          &mock_server_spawner().spawn(mock_server_cmdline(cs_options.tracefile)
                                           .port(node.classic_port)
                                           .http_port(node.http_port)
                                           .x_port(node.x_port)
                                           .args());

      set_mock_clusterset_metadata(node.http_port, cluster_id, node_id,
                                   cs_options);
    }
  }
}

void RouterComponentClusterSetTest::change_clusterset_primary(
    ClusterSetTopology &cs_topology, const unsigned new_primary_id) {
  for (unsigned cluster_id = 0; cluster_id < cs_topology.clusters.size();
       ++cluster_id) {
    cs_topology.clusters[cluster_id].role =
        cluster_id == new_primary_id ? "PRIMARY" : "SECONDARY";
  }
}

void RouterComponentClusterSetTest::add_json_str_field(
    JsonValue &json_doc, const std::string &field, const std::string &value) {
  json_doc.AddMember(JsonValue(field.c_str(), field.length(), json_allocator),
                     JsonValue(value.c_str(), value.length(), json_allocator),
                     json_allocator);
}

void RouterComponentClusterSetTest::add_json_int_field(JsonValue &json_doc,
                                                       const std::string &field,
                                                       const int value) {
  json_doc.AddMember(JsonValue(field.c_str(), field.length(), json_allocator),
                     JsonValue(value), json_allocator);
}

void RouterComponentClusterSetTest::add_clusterset_data_field(
    JsonValue &json_doc, const std::string &field,
    const ClusterSetTopology &cs_topology, const unsigned this_cluster_id,
    const unsigned this_node_id) {
  JsonValue clusterset_obj(rapidjson::kObjectType);
  add_json_str_field(clusterset_obj, "clusterset_id", cs_topology.uuid);
  add_json_str_field(clusterset_obj, "clusterset_name", "clusterset-name");
  add_json_int_field(clusterset_obj, "this_cluster_id", this_cluster_id);
  add_json_int_field(clusterset_obj, "this_node_id", this_node_id);
  add_json_int_field(clusterset_obj, "primary_cluster_id",
                     cs_topology.primary_cluster_id);

  JsonValue json_array_clusters(rapidjson::kArrayType);
  for (unsigned cluster_id = 0; cluster_id < cs_topology.clusters.size();
       ++cluster_id) {
    JsonValue cluster_obj(rapidjson::kObjectType);
    const auto &cluster_data = cs_topology.clusters[cluster_id];

    add_json_int_field(cluster_obj, "primary_node_id",
                       cluster_data.primary_node_id);
    add_json_str_field(cluster_obj, "uuid", cluster_data.uuid);
    add_json_str_field(cluster_obj, "name", cluster_data.name);
    add_json_str_field(cluster_obj, "role", cluster_data.role);
    add_json_str_field(cluster_obj, "gr_uuid", cluster_data.gr_uuid);
    add_json_int_field(cluster_obj, "invalid", cluster_data.invalid ? 1 : 0);

    JsonValue cluster_nodes_array(rapidjson::kArrayType);
    for (auto &node_data : cluster_data.nodes) {
      JsonValue node_obj(rapidjson::kObjectType);

      add_json_str_field(node_obj, "uuid", node_data.uuid);
      add_json_str_field(node_obj, "host", node_data.host);
      add_json_int_field(node_obj, "classic_port", node_data.classic_port);
      add_json_int_field(node_obj, "http_port", node_data.http_port);
      if (node_data.x_port > 0) {
        add_json_int_field(node_obj, "x_port", node_data.x_port);
      }
      const std::string attributes =
          node_data.is_read_replica ? R"({"instance_type" : "read-replica" })"
                                    : "{}";
      add_json_str_field(node_obj, "attributes", attributes);

      cluster_nodes_array.PushBack(node_obj, json_allocator);
    }
    cluster_obj.AddMember("nodes", cluster_nodes_array, json_allocator);

    JsonValue gr_nodes_array(rapidjson::kArrayType);
    for (auto &node_data : cluster_data.gr_nodes) {
      JsonValue node_obj(rapidjson::kObjectType);

      add_json_str_field(node_obj, "uuid", node_data.server_uuid);
      add_json_int_field(node_obj, "classic_port", node_data.classic_port);
      add_json_str_field(node_obj, "status", node_data.member_status);
      add_json_str_field(node_obj, "role", node_data.member_role);

      gr_nodes_array.PushBack(node_obj, json_allocator);
    }
    cluster_obj.AddMember("gr_nodes", gr_nodes_array, json_allocator);

    add_json_int_field(cluster_obj, "primary_node_id",
                       cluster_data.primary_node_id);

    json_array_clusters.PushBack(cluster_obj, json_allocator);
  }

  clusterset_obj.AddMember("clusters", json_array_clusters, json_allocator);

  json_doc.AddMember(JsonValue(field.c_str(), field.length(), json_allocator),
                     clusterset_obj, json_allocator);
}

void RouterComponentClusterSetTest::set_mock_metadata_on_all_cs_nodes(
    const ClusterSetOptions &cs_options) {
  for (const auto &cluster : cs_options.topology.clusters) {
    for (size_t node_id = 0; node_id < cluster.nodes.size(); ++node_id) {
      set_mock_clusterset_metadata(cluster.nodes[node_id].http_port,
                                   /*this_cluster_id*/ cluster.id,
                                   /*this_node_id*/ node_id, cs_options);
    }
  }
}

void RouterComponentClusterSetTest::set_mock_clusterset_metadata(
    uint16_t http_port, unsigned this_cluster_id, unsigned this_node_id,
    const ClusterSetOptions &cs_options) {
  JsonValue json_doc(rapidjson::kObjectType);

  add_clusterset_data_field(json_doc, "clusterset_data", cs_options.topology,
                            this_cluster_id, this_node_id);

  JsonValue md_version(rapidjson::kArrayType);
  md_version.PushBack(static_cast<int>(cs_options.metadata_version.major),
                      json_allocator);
  md_version.PushBack(static_cast<int>(cs_options.metadata_version.minor),
                      json_allocator);
  md_version.PushBack(static_cast<int>(cs_options.metadata_version.patch),
                      json_allocator);
  json_doc.AddMember("metadata_schema_version", md_version, json_allocator);
  add_json_int_field(json_doc, "view_id", cs_options.view_id);
  add_json_int_field(json_doc, "target_cluster_id",
                     cs_options.target_cluster_id);
  add_json_str_field(json_doc, "router_options", cs_options.router_options);
  add_json_str_field(json_doc, "router_expected_target_cluster",
                     cs_options.expected_target_cluster);
  add_json_int_field(json_doc, "simulate_cluster_not_found",
                     cs_options.simulate_cluster_not_found);
  add_json_int_field(json_doc, "config_defaults_stored_is_null",
                     cs_options.simulate_config_defaults_stored_is_null);
  add_json_str_field(json_doc, "router_version", MYSQL_ROUTER_VERSION);

  const auto json_str = json_to_string(json_doc);

  EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
}
