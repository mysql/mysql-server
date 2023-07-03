/*
  Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include "router_component_clusterset.h"

#include "mock_server_testutils.h"
#include "mysqlrouter/mock_server_rest_client.h"

void RouterComponentClusterSetTest::create_clusterset(
    uint64_t view_id, int target_cluster_id, int primary_cluster_id,
    const std::string &tracefile, const std::string &router_options,
    const std::string &expected_target_cluster, bool simulate_cluster_not_found,
    bool use_gr_notifications) {
  const std::string tracefile_path = get_data_dir().str() + "/" + tracefile;

  ClusterSetData clusterset_data;

  clusterset_data.primary_cluster_id = primary_cluster_id;

  // first create a ClusterSet topology
  for (unsigned cluster_id = 0; cluster_id < kClustersNumber; ++cluster_id) {
    ClusterData cluster_data;
    cluster_data.id = cluster_id;
    cluster_data.primary_node_id = 0;
    // 0-based -> 1-based
    const std::string id = std::to_string(cluster_id + 1);

    cluster_data.uuid = "00000000-0000-0000-0000-0000000000c" + id;
    cluster_data.name = "cluster-name-" + id;
    cluster_data.gr_uuid = "00000000-0000-0000-0000-0000000000g" + id;
    for (unsigned node_id = 0; node_id < kNodesPerClusterNumber; ++node_id) {
      ClusterNode cluster_node;
      cluster_node.uuid = "00000000-0000-0000-0000-0000000000" +
                          std::to_string(cluster_id + 1) +
                          std::to_string(node_id + 1);
      cluster_node.host = "127.0.0.1";
      cluster_node.classic_port = port_pool_.get_next_available();
      cluster_node.http_port = port_pool_.get_next_available();
      if (use_gr_notifications) {
        cluster_node.x_port = port_pool_.get_next_available();
      }

      cluster_data.nodes.push_back(cluster_node);
    }
    clusterset_data.clusters.push_back(cluster_data);
  }

  change_clusterset_primary(clusterset_data, primary_cluster_id);

  // now launch the mock servers and set their metadata
  for (unsigned cluster_id = 0; cluster_id < clusterset_data.clusters.size();
       ++cluster_id) {
    auto &cluster = clusterset_data.clusters[cluster_id];
    for (unsigned node_id = 0; node_id < cluster.nodes.size(); ++node_id) {
      auto &node = cluster.nodes[node_id];
      node.process = &launch_mysql_server_mock(
          tracefile_path, node.classic_port, EXIT_SUCCESS, false,
          node.http_port, node.x_port);

      set_mock_metadata(view_id, cluster_id, target_cluster_id, node.http_port,
                        clusterset_data, router_options,
                        expected_target_cluster, {2, 1, 0},
                        simulate_cluster_not_found);
    }
  }

  clusterset_data_ = std::move(clusterset_data);
}

void RouterComponentClusterSetTest::change_clusterset_primary(
    ClusterSetData &clusterset_data, const unsigned new_primary_id) {
  for (unsigned cluster_id = 0; cluster_id < clusterset_data.clusters.size();
       ++cluster_id) {
    clusterset_data.clusters[cluster_id].role =
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
    const ClusterSetData &clusterset_data, const unsigned this_cluster_id) {
  JsonValue clusterset_obj(rapidjson::kObjectType);
  add_json_str_field(clusterset_obj, "clusterset_id", clusterset_data.uuid);
  add_json_str_field(clusterset_obj, "clusterset_name", "clusterset-name");
  add_json_int_field(clusterset_obj, "this_cluster_id", this_cluster_id);
  add_json_int_field(clusterset_obj, "primary_cluster_id",
                     clusterset_data.primary_cluster_id);

  JsonValue json_array_clusters(rapidjson::kArrayType);
  for (unsigned cluster_id = 0; cluster_id < clusterset_data.clusters.size();
       ++cluster_id) {
    JsonValue cluster_obj(rapidjson::kObjectType);
    const auto &cluster_data = clusterset_data.clusters[cluster_id];

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

      cluster_nodes_array.PushBack(node_obj, json_allocator);
    }

    cluster_obj.AddMember("nodes", cluster_nodes_array, json_allocator);
    add_json_int_field(cluster_obj, "primary_node_id",
                       cluster_data.primary_node_id);

    json_array_clusters.PushBack(cluster_obj, json_allocator);
  }

  clusterset_obj.AddMember("clusters", json_array_clusters, json_allocator);

  json_doc.AddMember(JsonValue(field.c_str(), field.length(), json_allocator),
                     clusterset_obj, json_allocator);
}

void RouterComponentClusterSetTest::set_mock_metadata(
    uint64_t view_id, unsigned this_cluster_id, unsigned target_cluster_id,
    uint16_t http_port, const ClusterSetData &clusterset_data,
    const std::string &router_options /*= ""*/,
    const std::string &expected_target_cluster /*= ""*/,
    const mysqlrouter::MetadataSchemaVersion &metadata_version /*= {2, 1, 0}*/,
    bool simulate_cluster_not_found /* = false*/) {
  JsonValue json_doc(rapidjson::kObjectType);

  add_clusterset_data_field(json_doc, "clusterset_data", clusterset_data,
                            this_cluster_id);

  JsonValue md_version(rapidjson::kArrayType);
  md_version.PushBack(static_cast<int>(metadata_version.major), json_allocator);
  md_version.PushBack(static_cast<int>(metadata_version.minor), json_allocator);
  md_version.PushBack(static_cast<int>(metadata_version.patch), json_allocator);
  json_doc.AddMember("metadata_version", md_version, json_allocator);
  add_json_int_field(json_doc, "view_id", view_id);
  add_json_int_field(json_doc, "target_cluster_id", target_cluster_id);
  add_json_str_field(json_doc, "router_options", router_options);
  add_json_str_field(json_doc, "router_expected_target_cluster",
                     expected_target_cluster);

  add_json_int_field(json_doc, "simulate_cluster_not_found",
                     simulate_cluster_not_found);

  const auto json_str = json_to_string(json_doc);

  //  if (clusterset_data.primary_cluster_id > 0) FAIL() << json_str;

  EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
}
