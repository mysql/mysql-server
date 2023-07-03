/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef _ROUTER_COMPONENT_CLUSTERSET_H_
#define _ROUTER_COMPONENT_CLUSTERSET_H_

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <thread>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include "mysqlrouter/cluster_metadata.h"
#include "rest_api_testutils.h"
#include "router_component_test.h"

using mysqlrouter::ClusterType;

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

class RouterComponentClusterSetTest : public RestApiComponentTest {
 protected:
  struct ClusterNode {
    std::string uuid;

    std::string host;
    uint16_t classic_port;
    uint16_t x_port{0};
    uint16_t http_port;

    ProcessWrapper *process{nullptr};
  };

  struct ClusterData {
    unsigned id;

    std::string uuid;
    std::string name;
    std::string gr_uuid;
    std::string role;  // [PRIMARY, SECONDARY]
    // cluster is marked as invalid in the metadata
    bool invalid{false};

    std::vector<ClusterNode> nodes;
    unsigned primary_node_id;
  };

  struct ClusterSetData {
    std::string uuid{"clusterset-uuid"};
    std::vector<ClusterData> clusters;
    unsigned primary_cluster_id;

    auto get_all_nodes_classic_ports() const {
      std::vector<uint16_t> result;
      for (const auto &cluster : clusters) {
        for (const auto &node : cluster.nodes) {
          result.push_back(node.classic_port);
        }
      }
      return result;
    }

    void remove_node(const std::string &node_uuid) {
      for (auto &cluster : clusters) {
        unsigned i = 0;
        for (const auto &node : cluster.nodes) {
          if (node.uuid == node_uuid) {
            cluster.nodes.erase(cluster.nodes.begin() + i);
            return;
          }
          ++i;
        }
      }
    }

    void add_node(const unsigned cluster_id, const ClusterNode &node) {
      clusters[cluster_id].nodes.push_back(node);
    }
  };

  constexpr static unsigned kClustersNumber = 3;
  constexpr static unsigned kNodesPerClusterNumber = 3;

  void create_clusterset(uint64_t view_id, int target_cluster_id,
                         int primary_cluster_id, const std::string &tracefile,
                         const std::string &router_options = "",
                         const std::string &expected_target_cluster = ".*",
                         bool simulate_cluster_not_found = false,
                         bool use_gr_notifications = false);

  void change_clusterset_primary(ClusterSetData &clusterset_data,
                                 const unsigned new_primary_id);

  void add_json_str_field(JsonValue &json_doc, const std::string &field,
                          const std::string &value);

  void add_json_int_field(JsonValue &json_doc, const std::string &field,
                          const int value);

  void add_clusterset_data_field(JsonValue &json_doc, const std::string &field,
                                 const ClusterSetData &clusterset_data,
                                 const unsigned this_cluster_id);

  void set_mock_metadata(
      uint64_t view_id, unsigned this_cluster_id, unsigned target_cluster_id,
      uint16_t http_port, const ClusterSetData &clusterset_data,
      const std::string &router_options = "",
      const std::string &expected_target_cluster = ".*",
      const mysqlrouter::MetadataSchemaVersion &metadata_version = {2, 1, 0},
      bool simulate_cluster_not_found = false);

  ClusterSetData clusterset_data_;
  JsonAllocator json_allocator;
};

#endif  // _ROUTER_COMPONENT_CLUSTERSET_H_
