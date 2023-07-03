/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

bool handle_params(HttpRequest &req) {
  auto md_api = metadata_cache::MetadataCacheAPI::instance();

  if (!req.get_uri().get_query().empty()) {
    const auto q = req.get_uri().get_query();
    if (q == "fetchWholeTopology=1") {
      md_api->fetch_whole_topology(true);
    } else if (q == "fetchWholeTopology=0") {
      md_api->fetch_whole_topology(false);
    } else {
      send_rfc7807_error(req, HttpStatusCode::BadRequest,
                         {
                             {"title", "validation error"},
                             {"detail", "unsupported parameter"},
                         });
    }
    return true;
  }

  return true;
}

bool RestMetadataCacheConfig::on_handle_request(
    HttpRequest &req, const std::string & /* base_path */,
    const std::vector<std::string> &path_matches) {
  if (!handle_params(req)) return true;

  if (path_matches[1] !=
      metadata_cache::MetadataCacheAPI::instance()->instance_name()) {
    send_rfc7807_not_found_error(req);
    return true;
  }

  auto out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/json");

  rapidjson::Document json_doc;
  {
    rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();

    auto md_api = metadata_cache::MetadataCacheAPI::instance();
    auto group_members = md_api->get_cluster_nodes();

    rapidjson::Value members(rapidjson::kArrayType);

    for (auto &member : group_members) {
      members.PushBack(
          rapidjson::Value(rapidjson::kObjectType)
              .AddMember("hostname",
                         json_value_from_string(member.host, allocator),
                         allocator)
              .AddMember("port", member.port, allocator)
          //
          ,
          allocator);
    }

    json_doc.SetObject()
        .AddMember("clusterName",
                   json_value_from_string(md_api->target_cluster().to_string(),
                                          allocator),
                   allocator)
        .AddMember<uint64_t>("timeRefreshInMs",
                             static_cast<uint64_t>(md_api->ttl().count()),
                             allocator)
        .AddMember("groupReplicationId",
                   json_value_from_string(md_api->cluster_type_specific_id(),
                                          allocator),
                   allocator)
        .AddMember("nodes", members, allocator)
        //
        ;
  }

  send_json_document(req, HttpStatusCode::Ok, json_doc);

  return true;
}
