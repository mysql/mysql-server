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

#include "rest_routing_routes_status.h"

#include <ctime>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "mysqlrouter/rest_api_utils.h"
#include "mysqlrouter/routing_component.h"

bool RestRoutingRoutesStatus::on_handle_request(
    HttpRequest &req, const std::string & /* base_path */,
    const std::vector<std::string> &path_matches) {
  if (!ensure_no_params(req)) return true;

  MySQLRoutingAPI inst =
      MySQLRoutingComponent::get_instance().api(path_matches[1]);

  if (!inst) {
    send_rfc7807_not_found_error(req);
    return true;
  }

  auto out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/json");

#if 0
  // handle If-Modified-Since

  last_modified_ = ::time(nullptr);

  if (!ensure_modified_since(req, last_modified_)) return true;
#endif

  rapidjson::Document json_doc;
  {
    rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();

    json_doc.SetObject()
        .AddMember("activeConnections", inst.get_active_connections(),
                   allocator)
        .AddMember("totalConnections", inst.get_total_connections(), allocator)
        .AddMember<uint64_t>("blockedHosts",
                             inst.get_blocked_client_hosts().size(), allocator);
  }
  send_json_document(req, HttpStatusCode::Ok, json_doc);

  return true;
}
