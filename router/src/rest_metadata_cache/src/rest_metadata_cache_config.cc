/*
  Copyright (c) 2019, 2026, Oracle and/or its affiliates.

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

#include "rest_metadata_cache_config.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/rest_api_utils.h"

template <class AllocatorType>
static rapidjson::Value json_value_from_string(const std::string &s,
                                               AllocatorType &allocator) {
  return {s.data(), s.size(), allocator};
}

bool handle_params(http::base::Request &req) {
  if (!req.get_uri().get_query().empty()) {
    send_rfc7807_error(req, HttpStatusCode::BadRequest,
                       {
                           {"title", "validation error"},
                           {"detail", "unsupported parameter"},
                       });
  }

  return true;
}

bool RestMetadataCacheConfig::on_handle_request(
    http::base::Request &req, const std::string & /* base_path */,
    const std::vector<std::string> &path_matches) {
  if (!handle_params(req)) return true;

  if (path_matches[1] !=
      metadata_cache::MetadataCacheAPI::instance()->instance_name()) {
    send_rfc7807_not_found_error(req);
    return true;
  }

  auto &out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/json");

  rapidjson::Document json_doc;
  {
    rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();
    auto md_api = metadata_cache::MetadataCacheAPI::instance();

    rapidjson::Value members(rapidjson::kArrayType);
    auto clusterset_topology = md_api->get_cluster_topology();
    const auto &clusterset_name = clusterset_topology.name;
    for (const auto &cluster : clusterset_topology.clusters_data) {
      const std::string cluster_role =
          cluster.is_primary ? "PRIMARY" : "REPLICA";
      for (const auto &cluster_member : cluster.members) {
        std::string tags_str;
        for (const auto &tag : cluster_member.tags) {
          tags_str += "\n\t\t" + tag.first + ": " + tag.second;
        }

        std::string member_role;
        if (cluster_member.mode == metadata_cache::ServerMode::ReadWrite) {
          member_role = "PRIMARY";
        } else {
          member_role =
              cluster_member.type == mysqlrouter::InstanceType::ReadReplica
                  ? "READ_REPLICA"
                  : "SECONDARY";
        }
        const auto label_port = cluster_member.port == 0 ? cluster_member.xport
                                                         : cluster_member.port;
        const std::string &label =
            cluster_member.host + ":" + std::to_string(label_port);

        members.PushBack(
            rapidjson::Value(rapidjson::kObjectType)
                .AddMember(
                    "hostname",
                    json_value_from_string(cluster_member.host, allocator),
                    allocator)
                .AddMember("port", cluster_member.port, allocator)
                .AddMember("X_port", cluster_member.xport, allocator)
                .AddMember("UUID",
                           json_value_from_string(
                               cluster_member.mysql_server_uuid, allocator),
                           allocator)
                .AddMember("Cluster_name",
                           json_value_from_string(cluster.name, allocator),
                           allocator)
                .AddMember("member_role",
                           json_value_from_string(member_role, allocator),
                           allocator)
                .AddMember("ClusterSet_name",
                           json_value_from_string(clusterset_name, allocator),
                           allocator)
                .AddMember("Cluster_role",
                           json_value_from_string(cluster_role, allocator),
                           allocator)
                .AddMember("label", json_value_from_string(label, allocator),
                           allocator)
                .AddMember("tags", json_value_from_string(tags_str, allocator),
                           allocator)
            //
            ,
            allocator);
      }
    }

    const std::string cluster_name =
        md_api->target_cluster().target_type() ==
                mysqlrouter::TargetCluster::TargetType::ByName
            ? md_api->target_cluster().to_string()
            : "";

    const std::string uuid =
        md_api->target_cluster().target_type() ==
                mysqlrouter::TargetCluster::TargetType::ByUUID
            ? md_api->target_cluster().to_string()
            : "";

    json_doc.SetObject()
        .AddMember("clusterName",
                   json_value_from_string(cluster_name, allocator), allocator)
        .AddMember<uint64_t>("timeRefreshInMs",
                             static_cast<uint64_t>(md_api->ttl().count()),
                             allocator)
        .AddMember("groupReplicationId",
                   json_value_from_string(uuid, allocator), allocator)
        .AddMember("nodes", members, allocator)
        //
        ;
  }

  send_json_document(req, HttpStatusCode::Ok, json_doc);

  return true;
}
