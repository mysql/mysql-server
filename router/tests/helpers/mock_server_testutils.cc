
/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include "mock_server_testutils.h"

#include <array>
#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/error/en.h>

#include "config_builder.h"
#include "mysql/harness/stdx/ranges.h"  // enumerate
#include "mysqlrouter/mock_server_rest_client.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION

std::string json_to_string(const JsonValue &json_doc) {
  JsonStringBuffer out_buffer;

  rapidjson::Writer<JsonStringBuffer> out_writer{out_buffer};
  json_doc.Accept(out_writer);
  return out_buffer.GetString();
}

std::vector<GRNode> classic_ports_to_gr_nodes(
    const std::vector<uint16_t> &classic_ports) {
  std::vector<GRNode> result;

  for (const auto [id, port] : stdx::views::enumerate(classic_ports)) {
    const std::string role = id == 0 ? "PRIMARY" : "SECONDARY";
    result.emplace_back(port, "uuid-" + std::to_string(id + 1), "ONLINE", role);
  }

  return result;
}

std::vector<ClusterNode> classic_ports_to_cluster_nodes(
    const std::vector<uint16_t> &classic_ports) {
  std::vector<ClusterNode> result;
  for (const auto [id, port] : stdx::views::enumerate(classic_ports)) {
    const std::string role = id == 0 ? "PRIMARY" : "SECONDARY";
    result.emplace_back(port, "uuid-" + std::to_string(id + 1), 0, "", role);
  }

  return result;
}

JsonValue mock_GR_metadata_as_json(
    const std::string &gr_id, const std::vector<GRNode> &gr_nodes,
    unsigned gr_pos, const std::vector<ClusterNode> &cluster_nodes,
    uint64_t view_id, bool error_on_md_query, const std::string &gr_node_host,
    const std::string &router_options,
    const mysqlrouter::MetadataSchemaVersion &metadata_version,
    const std::string &cluster_name) {
  JsonValue json_doc(rapidjson::kObjectType);
  JsonAllocator allocator;
  json_doc.AddMember(
      "gr_id", JsonValue(gr_id.c_str(), gr_id.length(), allocator), allocator);
  json_doc.AddMember(
      "cluster_name",
      JsonValue(cluster_name.c_str(), cluster_name.length(), allocator),
      allocator);

  JsonValue gr_nodes_json(rapidjson::kArrayType);
  JsonValue cluster_nodes_json(rapidjson::kArrayType);
  for (auto &gr_node : gr_nodes) {
    JsonValue node(rapidjson::kArrayType);
    node.PushBack(JsonValue(gr_node.server_uuid.c_str(),
                            gr_node.server_uuid.length(), allocator),
                  allocator);
    node.PushBack(static_cast<int>(gr_node.classic_port), allocator);
    node.PushBack(JsonValue(gr_node.member_status.c_str(),
                            gr_node.member_status.length(), allocator),
                  allocator);
    node.PushBack(JsonValue(gr_node.member_role.c_str(),
                            gr_node.member_role.length(), allocator),
                  allocator);

    gr_nodes_json.PushBack(node, allocator);
  }

  for (auto &cluster_node : cluster_nodes) {
    JsonValue node(rapidjson::kArrayType);
    node.PushBack(JsonValue(cluster_node.server_uuid.c_str(),
                            cluster_node.server_uuid.length(), allocator),
                  allocator);
    node.PushBack(static_cast<int>(cluster_node.classic_port), allocator);
    node.PushBack(static_cast<int>(cluster_node.x_port), allocator);
    node.PushBack(JsonValue(cluster_node.attributes.c_str(),
                            cluster_node.attributes.length(), allocator),
                  allocator);
    // The role (PRIMARY, SECONDARY) for ReplicaSet is in the static metadata as
    // there is no GR there
    node.PushBack(JsonValue(cluster_node.role.c_str(),
                            cluster_node.role.length(), allocator),
                  allocator);

    cluster_nodes_json.PushBack(node, allocator);
  }

  json_doc.AddMember("gr_nodes", gr_nodes_json, allocator);
  json_doc.AddMember("gr_pos", gr_pos, allocator);
  json_doc.AddMember("cluster_nodes", cluster_nodes_json, allocator);
  if (view_id > 0) {
    json_doc.AddMember("view_id", view_id, allocator);
  }
  json_doc.AddMember("error_on_md_query", error_on_md_query ? 1 : 0, allocator);
  json_doc.AddMember(
      "gr_node_host",
      JsonValue(gr_node_host.c_str(), gr_node_host.length(), allocator),
      allocator);
  json_doc.AddMember(
      "router_options",
      JsonValue(router_options.c_str(), router_options.length(), allocator),
      allocator);

  JsonValue md_version(rapidjson::kArrayType);
  md_version.PushBack(static_cast<int>(metadata_version.major), allocator);
  md_version.PushBack(static_cast<int>(metadata_version.minor), allocator);
  md_version.PushBack(static_cast<int>(metadata_version.patch), allocator);
  json_doc.AddMember("metadata_schema_version", md_version, allocator);
  json_doc.AddMember("router_version", MYSQL_ROUTER_VERSION, allocator);

  return json_doc;
}

void set_mock_metadata(
    uint16_t http_port, const std::string &gr_id,
    const std::vector<GRNode> &gr_nodes, unsigned gr_pos,
    const std::vector<ClusterNode> &cluster_nodes, uint64_t view_id,
    bool error_on_md_query, const std::string &gr_node_host,
    const std::string &router_options,
    const mysqlrouter::MetadataSchemaVersion &metadata_version,
    const std::string &cluster_name) {
  const auto json_doc = mock_GR_metadata_as_json(
      gr_id, gr_nodes, gr_pos, cluster_nodes, view_id, error_on_md_query,
      gr_node_host, router_options, metadata_version, cluster_name);

  const auto json_str = json_to_string(json_doc);

  ASSERT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
}
