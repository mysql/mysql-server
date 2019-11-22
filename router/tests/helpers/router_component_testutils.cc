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

#include "router_component_testutils.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif
#include <gmock/gmock.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>

#include "router_test_helpers.h"

namespace {
// default allocator for rapidJson (MemoryPoolAllocator) is broken for
// SparcSolaris
using JsonAllocator = rapidjson::CrtAllocator;
using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<>, JsonAllocator>;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;
using JsonStringBuffer =
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
}  // namespace

std::string create_state_file_content(
    const std::string &replication_goup_id,
    const std::vector<uint16_t> &metadata_servers_ports,
    const unsigned view_id /*= 0*/) {
  std::string metadata_servers;
  for (std::size_t i = 0; i < metadata_servers_ports.size(); i++) {
    metadata_servers +=
        "\"mysql://127.0.0.1:" + std::to_string(metadata_servers_ports[i]) +
        "\"";
    if (i < metadata_servers_ports.size() - 1) metadata_servers += ",";
  }
  std::string view_id_str;
  if (view_id > 0) view_id_str = R"(, "view-id":)" + std::to_string(view_id);
  // clang-format off
  const std::string result =
    "{"
       R"("version": "1.0.0",)"
       R"("metadata-cache": {)"
         R"("group-replication-id": ")" + replication_goup_id + R"(",)"
         R"("cluster-metadata-servers": [)" + metadata_servers + "]"
         + view_id_str +
        "}"
      "}";
  // clang-format on

  return result;
}

void check_state_file(const std::string &state_file,
                      const std::string &expected_group_replication_id,
                      const std::vector<uint16_t> expected_cluster_nodes,
                      const unsigned expected_view_id /*= 0*/,
                      const std::string node_address /*= "127.0.0.1"*/) {
  const std::string state_file_content = get_file_output(state_file);
  JsonDocument json_doc;
  json_doc.Parse(state_file_content.c_str());
  constexpr const char *kExpectedVersion = "1.0.0";

  EXPECT_TRUE(json_doc.HasMember("version")) << state_file_content;
  EXPECT_TRUE(json_doc["version"].IsString()) << state_file_content;
  EXPECT_STREQ(kExpectedVersion, json_doc["version"].GetString())
      << state_file_content;

  EXPECT_TRUE(json_doc.HasMember("metadata-cache")) << state_file_content;
  EXPECT_TRUE(json_doc["metadata-cache"].IsObject()) << state_file_content;

  auto metadata_cache_section = json_doc["metadata-cache"].GetObject();

  EXPECT_TRUE(metadata_cache_section.HasMember("group-replication-id"))
      << state_file_content;
  EXPECT_TRUE(metadata_cache_section["group-replication-id"].IsString())
      << state_file_content;
  EXPECT_STREQ(expected_group_replication_id.c_str(),
               metadata_cache_section["group-replication-id"].GetString())
      << state_file_content;

  if (expected_view_id > 0) {
    EXPECT_TRUE(metadata_cache_section.HasMember("view-id"))
        << state_file_content;
    EXPECT_TRUE(metadata_cache_section["view-id"].IsInt())
        << state_file_content;
    EXPECT_EQ(expected_view_id,
              (unsigned)metadata_cache_section["view-id"].GetInt())
        << state_file_content;
  }

  EXPECT_TRUE(metadata_cache_section.HasMember("cluster-metadata-servers"))
      << state_file_content;
  EXPECT_TRUE(metadata_cache_section["cluster-metadata-servers"].IsArray())
      << state_file_content;
  auto cluster_nodes =
      metadata_cache_section["cluster-metadata-servers"].GetArray();
  EXPECT_EQ(expected_cluster_nodes.size(), cluster_nodes.Size())
      << state_file_content;
  for (unsigned i = 0; i < cluster_nodes.Size(); ++i) {
    EXPECT_TRUE(cluster_nodes[i].IsString()) << state_file_content;
    const std::string expected_cluster_node =
        "mysql://" + node_address + ":" +
        std::to_string(expected_cluster_nodes[i]);
    EXPECT_STREQ(expected_cluster_node.c_str(), cluster_nodes[i].GetString())
        << state_file_content;
  }

  // check that we have write access to the file
  // just append it with an empty line, that will not break it
  EXPECT_NO_THROW({
    std::ofstream ofs(state_file, std::ios::app);
    ofs << "\n";
  });
}
