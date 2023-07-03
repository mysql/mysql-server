
/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
#include "mysqlrouter/mock_server_rest_client.h"
#include "mysqlrouter/rest_client.h"
#include "rest_api_testutils.h"

std::string json_to_string(const JsonValue &json_doc) {
  JsonStringBuffer out_buffer;

  rapidjson::Writer<JsonStringBuffer> out_writer{out_buffer};
  json_doc.Accept(out_writer);
  return out_buffer.GetString();
}

std::vector<GRNode> classic_ports_to_gr_nodes(
    const std::vector<uint16_t> &classic_ports) {
  std::vector<GRNode> result;
  for (const auto &port : classic_ports) {
    result.emplace_back(port);
  }

  return result;
}

std::vector<ClusterNode> classic_ports_to_cluster_nodes(
    const std::vector<uint16_t> &classic_ports) {
  std::vector<ClusterNode> result;
  for (const auto &port : classic_ports) {
    result.emplace_back(port);
  }

  return result;
}

JsonValue mock_GR_metadata_as_json(
    const std::string &gr_id, const std::vector<GRNode> &gr_nodes,
    unsigned gr_pos, const std::vector<ClusterNode> &cluster_nodes,
    unsigned primary_id, uint64_t view_id, bool error_on_md_query,
    const std::string &gr_node_host) {
  JsonValue json_doc(rapidjson::kObjectType);
  JsonAllocator allocator;
  json_doc.AddMember(
      "gr_id", JsonValue(gr_id.c_str(), gr_id.length(), allocator), allocator);

  JsonValue gr_nodes_json(rapidjson::kArrayType);
  JsonValue cluster_nodes_json(rapidjson::kArrayType);
  for (auto &gr_node : gr_nodes) {
    JsonValue node(rapidjson::kArrayType);
    node.PushBack(static_cast<int>(gr_node.classic_port), allocator);
    node.PushBack(JsonValue(gr_node.member_status.c_str(),
                            gr_node.member_status.length(), allocator),
                  allocator);

    gr_nodes_json.PushBack(node, allocator);
  }

  for (auto &cluster_node : cluster_nodes) {
    JsonValue node(rapidjson::kArrayType);
    node.PushBack(static_cast<int>(cluster_node.classic_port), allocator);
    node.PushBack(static_cast<int>(cluster_node.x_port), allocator);
    node.PushBack(JsonValue(cluster_node.attributes.c_str(),
                            cluster_node.attributes.length(), allocator),
                  allocator);

    cluster_nodes_json.PushBack(node, allocator);
  }

  json_doc.AddMember("gr_nodes", gr_nodes_json, allocator);
  json_doc.AddMember("gr_pos", gr_pos, allocator);
  json_doc.AddMember("cluster_nodes", cluster_nodes_json, allocator);
  json_doc.AddMember("primary_id", static_cast<int>(primary_id), allocator);
  if (view_id > 0) {
    json_doc.AddMember("view_id", view_id, allocator);
  }
  json_doc.AddMember("error_on_md_query", error_on_md_query ? 1 : 0, allocator);
  json_doc.AddMember(
      "gr_node_host",
      JsonValue(gr_node_host.c_str(), gr_node_host.length(), allocator),
      allocator);

  return json_doc;
}

void set_mock_metadata(uint16_t http_port, const std::string &gr_id,
                       const std::vector<GRNode> &gr_nodes, unsigned gr_pos,
                       const std::vector<ClusterNode> &cluster_nodes,
                       unsigned primary_id, uint64_t view_id,
                       bool error_on_md_query,
                       const std::string &gr_node_host) {
  const auto json_doc = mock_GR_metadata_as_json(
      gr_id, gr_nodes, gr_pos, cluster_nodes, primary_id, view_id,
      error_on_md_query, gr_node_host);

  const auto json_str = json_to_string(json_doc);

  ASSERT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
}

void set_mock_bootstrap_data(
    uint16_t http_port, const std::string &cluster_name,
    const std::vector<std::pair<std::string, unsigned>> &gr_members_ports,
    const mysqlrouter::MetadataSchemaVersion &metadata_version,
    const std::string &cluster_specific_id) {
  JsonValue json_doc(rapidjson::kObjectType);
  JsonAllocator allocator;
  json_doc.AddMember(
      "cluster_name",
      JsonValue(cluster_name.c_str(), cluster_name.length(), allocator),
      allocator);

  JsonValue gr_members_json(rapidjson::kArrayType);
  size_t i{1};
  for (auto &gr_member : gr_members_ports) {
    JsonValue member(rapidjson::kArrayType);
    const std::string id = "uuid-" + std::to_string(i);
    member.PushBack(JsonValue(id.c_str(), id.length(), allocator), allocator);
    member.PushBack(
        JsonValue(gr_member.first.c_str(), gr_member.first.length(), allocator),
        allocator);
    member.PushBack(static_cast<int>(gr_member.second), allocator);
    gr_members_json.PushBack(member, allocator);
    i++;
  }
  JsonValue cluster_instances_json{gr_members_json, allocator};
  json_doc.AddMember("gr_members", gr_members_json, allocator);
  json_doc.AddMember("innodb_cluster_instances", cluster_instances_json,
                     allocator);

  JsonValue md_version(rapidjson::kArrayType);
  md_version.PushBack(static_cast<int>(metadata_version.major), allocator);
  md_version.PushBack(static_cast<int>(metadata_version.minor), allocator);
  md_version.PushBack(static_cast<int>(metadata_version.patch), allocator);
  json_doc.AddMember("metadata_version", md_version, allocator);

  json_doc.AddMember("gr_id",
                     JsonValue(cluster_specific_id.c_str(),
                               cluster_specific_id.length(), allocator),
                     allocator);

  const auto json_str = json_to_string(json_doc);

  EXPECT_NO_THROW(MockServerRestClient(http_port).set_globals(json_str));
}
