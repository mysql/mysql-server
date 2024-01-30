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

#include "mock_server_testutils.h"
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
    bool is_read_replica{false};
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
    std::vector<GRNode> gr_nodes;
    unsigned primary_node_id;
  };

  struct ClusterSetTopology {
    std::string uuid{"clusterset-uuid"};
    std::vector<ClusterData> clusters;
    unsigned primary_cluster_id;

    auto get_md_servers_classic_ports() const {
      std::vector<uint16_t> result;
      std::vector<uint16_t> secondary_clusters_nodes;
      for (const auto &cluster : clusters) {
        for (const auto &node : cluster.nodes) {
          if (node.is_read_replica) continue;
          // PRIMARY cluster nodes first to match the metadata-servers order
          // expectation
          if (cluster.role == "PRIMARY")
            result.push_back(node.classic_port);
          else
            secondary_clusters_nodes.push_back(node.classic_port);
        }
      }
      result.insert(result.end(), secondary_clusters_nodes.begin(),
                    secondary_clusters_nodes.end());

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
  constexpr static unsigned kGRNodesPerClusterNumber = 3;

  struct ClusterSetOptions {
    uint64_t view_id{0};
    int target_cluster_id{0};
    int primary_cluster_id{0};
    std::string tracefile;
    std::string router_options{""};
    std::string expected_target_cluster{".*"};
    bool simulate_cluster_not_found{false};
    bool simulate_config_defaults_stored_is_null{false};
    bool use_gr_notifications{false};
    std::vector<size_t> gr_nodes_number{3, 3, 3};
    std::vector<size_t> read_replicas_number{};
    mysqlrouter::MetadataSchemaVersion metadata_version{
        mysqlrouter::MetadataSchemaVersion{2, 2, 0}};
    ClusterSetTopology topology;
  };

  void create_clusterset(ClusterSetOptions &cs_options);

  void change_clusterset_primary(ClusterSetTopology &cs_topology,
                                 const unsigned new_primary_id);

  void add_json_str_field(JsonValue &json_doc, const std::string &field,
                          const std::string &value);

  void add_json_int_field(JsonValue &json_doc, const std::string &field,
                          const int value);

  void add_clusterset_data_field(JsonValue &json_doc, const std::string &field,
                                 const ClusterSetTopology &cs_topology,
                                 const unsigned this_cluster_id,
                                 const unsigned this_node_id);

  void set_mock_clusterset_metadata(uint16_t http_port,
                                    unsigned this_cluster_id,
                                    unsigned this_node_id,
                                    const ClusterSetOptions &cs_options);

  void set_mock_metadata_on_all_cs_nodes(const ClusterSetOptions &cs_options);

  JsonAllocator json_allocator;
};

#endif  // _ROUTER_COMPONENT_CLUSTERSET_H_
