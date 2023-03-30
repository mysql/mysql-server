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

#include "rest_clusters_nodes.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/rest_api_utils.h"

static const char *server_mode_to_string(metadata_cache::ServerMode mode) {
  switch (mode) {
    case metadata_cache::ServerMode::ReadOnly:
      return "read_only";
    case metadata_cache::ServerMode::ReadWrite:
      return "writable";
    case metadata_cache::ServerMode::Unavailable:
    default:
      return "unknown";
  }
}

bool RestClustersNodes::on_handle_request(
    HttpRequest &req, const std::string & /* base_path */,
    const std::vector<std::string> & /*path_matches*/) {
  if (!ensure_no_params(req)) return true;

  auto out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/json");

  rapidjson::Document json_doc;
  {
    rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();

    const auto &res =
        metadata_cache::MetadataCacheAPI::instance()->get_cluster_nodes();

    rapidjson::Value items(rapidjson::kArrayType);

    for (auto &inst : res) {
      rapidjson::Value o(rapidjson::kObjectType);

      o.AddMember("replicasetName",
                  rapidjson::Value("default", allocator).Move(), allocator);
      o.AddMember(
          "mysqlServerUuid",
          rapidjson::Value(inst.mysql_server_uuid.c_str(), allocator).Move(),
          allocator);

      // read-only, writable
      o.AddMember(
          "mode",
          rapidjson::Value(server_mode_to_string(inst.mode), allocator).Move(),
          allocator);

      // ONLINE, OFFLINE, RECOVERING, UNREACHABLE, ERROR
      // o.AddMember("state", "", allocator);

      // backend_state: can the router reach the backend?
      // o.AddMember("backend_state", "", allocator);
      o.AddMember("hostname",
                  rapidjson::Value(inst.host.c_str(), allocator).Move(),
                  allocator);
      o.AddMember("tcpPortClassic", inst.port, allocator);
      o.AddMember("tcpPortX", inst.xport, allocator);

      items.PushBack(o, allocator);
    }

    json_doc.SetObject().AddMember("items", items, allocator);
  }

  send_json_document(req, HttpStatusCode::Ok, json_doc);

  return true;
}
