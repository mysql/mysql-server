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

#ifndef MYSQLROUTER_MOCK_SERVER_TESTUTILS_H_INCLUDED
#define MYSQLROUTER_MOCK_SERVER_TESTUTILS_H_INCLUDED

#include <chrono>
#include <optional>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>

#include "mysqlrouter/cluster_metadata.h"

// AddressSanitizer gets confused by the default, MemoryPoolAllocator
// Solaris sparc also gets crashes
using JsonAllocator = rapidjson::CrtAllocator;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;
using JsonValue =
    rapidjson::GenericValue<rapidjson::UTF8<>, JsonDocument::AllocatorType>;
using JsonStringBuffer =
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, rapidjson::CrtAllocator>;

struct GRNode {
  GRNode(uint32_t p_classic_port, const std::string &p_server_uuid,
         const std::string &p_member_status, const std::string &p_member_role)
      : server_uuid(p_server_uuid.empty() ? std::to_string(p_classic_port)
                                          : p_server_uuid),
        classic_port(p_classic_port),
        member_status(p_member_status),
        member_role(p_member_role) {}

  std::string server_uuid;
  uint32_t classic_port;
  std::string member_status;  // ONLINE, ...
  std::string member_role;    // PRIMARY, ...
};

struct ClusterNode {
  ClusterNode(uint32_t p_classic_port, const std::string &p_server_uuid = "",
              uint32_t p_x_port = 0, const std::string &p_attributes = "{}",
              const std::string &p_role = "")
      : server_uuid(p_server_uuid.empty() ? std::to_string(p_classic_port)
                                          : p_server_uuid),
        classic_port(p_classic_port),
        x_port(p_x_port),
        attributes(p_attributes),
        role(p_role) {}

  std::string server_uuid;
  uint32_t classic_port;
  uint32_t x_port;
  std::string attributes;

  // only relevant for ReplicaSet nodes, for GR-based Clusters the role is
  // determined dynamically based on the GR status
  std::string role;  // PRIMARY, SECONDARY
};

/**
 * Converts a vector of classic port numbers to the vector of GRNode objects.
 */
std::vector<GRNode> classic_ports_to_gr_nodes(
    const std::vector<uint16_t> &classic_ports);

/**
 * Converts a vector of classic port numbers to the vector of Cluster Node
 * objects.
 */
std::vector<ClusterNode> classic_ports_to_cluster_nodes(
    const std::vector<uint16_t> &classic_ports);

class MockGrMetadata {
 public:
  MockGrMetadata &gr_id(const std::string &gr_id);

  MockGrMetadata &cluster_name(const std::string &cluster_name);

  MockGrMetadata &gr_node_host(const std::string &gr_node_host);

  MockGrMetadata &router_options(const std::string &router_options);

  MockGrMetadata &router_version(const std::string &router_version);

  MockGrMetadata &gr_nodes(const std::vector<GRNode> &gr_nodes);

  MockGrMetadata &cluster_nodes(const std::vector<ClusterNode> &cluster_nodes);

  MockGrMetadata &gr_pos(unsigned gr_pos);

  MockGrMetadata &view_id(uint64_t id);

  MockGrMetadata &metadata_version(
      const mysqlrouter::MetadataSchemaVersion &metadata_version);

  MockGrMetadata &error_on_md_query(bool error_on_md_query);

  JsonValue as_json() const;

 private:
  std::optional<std::string> gr_id_;
  std::optional<std::string> cluster_name_;
  std::optional<std::string> gr_node_host_;
  std::optional<std::string> router_options_;
  std::optional<std::string> router_version_;
  std::optional<std::vector<ClusterNode>> cluster_nodes_;
  std::optional<std::vector<GRNode>> gr_nodes_;
  std::optional<unsigned> gr_pos_;
  std::optional<uint64_t> view_id_;
  std::optional<mysqlrouter::MetadataSchemaVersion> metadata_version_;
  std::optional<bool> error_on_md_query_{};
};

/**
 * Converts the GR mock data to the JSON object.
 *
 * @param gr_id replication group id to set
 * @param gr_nodes vector with the GR nodes
 * @param gr_pos this node's position in GR nodes table
 * @param cluster_nodes vector with cluster nodes as defined in the metadata
 * @param view_id metadata view id (for AR cluster)
 * @param error_on_md_query if true the mock should return an error when
 * handling the metadata query
 * @param gr_node_host address of the host with the nodes
 * @param router_options JSON with router options in metadata
 * @param metadata_version metadata schema version
 * @param cluster_name name of the InnoDB Cluster in the metadata
 *
 * @return JSON object with the GR mock data.
 */
JsonValue mock_GR_metadata_as_json(
    const std::string &gr_id, const std::vector<GRNode> &gr_nodes,
    unsigned gr_pos, const std::vector<ClusterNode> &cluster_nodes,
    uint64_t view_id = 0, bool error_on_md_query = false,
    const std::string &gr_node_host = "127.0.0.1",
    const std::string &router_options = "",
    const mysqlrouter::MetadataSchemaVersion &metadata_version =
        mysqlrouter::MetadataSchemaVersion{2, 2, 0},
    const std::string &cluster_name = "test");

/**
 * Sets the metadata returned by the mock server.
 *
 * @param http_port mock server's http port where it services the http requests
 * @param gr_id replication group id to set
 * @param gr_nodes vector with the GR nodes
 * @param gr_pos this node's position in GR nodes table
 * @param cluster_nodes vector with cluster nodes as defined in the metadata
 * @param view_id metadata view id (for AR cluster)
 * @param error_on_md_query if true the mock should return an error when
 * @param gr_node_host address of the host with the nodes handling the metadata
 * @param router_options JSON with router options in metadata
 * @param metadata_version metadata schema version
 * @param cluster_name name of the InnoDB Cluster in the metadata
 * query
 */
void set_mock_metadata(
    uint16_t http_port, const std::string &gr_id,
    const std::vector<GRNode> &gr_nodes, unsigned gr_pos,
    const std::vector<ClusterNode> &cluster_nodes, uint64_t view_id = 0,
    bool error_on_md_query = false,
    const std::string &gr_node_host = "127.0.0.1",
    const std::string &router_options = "",
    const mysqlrouter::MetadataSchemaVersion &metadata_version =
        mysqlrouter::MetadataSchemaVersion{2, 2, 0},
    const std::string &cluster_name = "test");

/**
 * Converts JSON object to string representation.
 */
std::string json_to_string(const JsonValue &json_doc);

#endif  // MYSQLROUTER_MOCK_SERVER_TESTUTILS_H_INCLUDED
