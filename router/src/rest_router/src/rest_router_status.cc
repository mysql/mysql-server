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

#ifdef _WIN32
#include <Winsock2.h>  // gethostname()
#endif

#include "rest_router_status.h"

#include <ctime>

#ifdef _WIN32
#include <process.h>  // getpid()
#else
#include <unistd.h>  // getpid(), gethostname()
#endif

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "mysqlrouter/rest_api_utils.h"
#include "mysqlrouter/utils.h"  // string_format

#include "router_config.h"  // MYSQL_ROUTER_VERSION

constexpr const char RestRouterStatus::path_regex[];

bool RestRouterStatus::on_handle_request(HttpRequest &req,
                                         const std::string & /* base_path */,
                                         const std::vector<std::string> &) {
  if (!ensure_no_params(req)) return true;

  auto out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/json");

  if (!ensure_modified_since(req, last_modified_)) return true;

  rapidjson::Document json_doc;
  {
    rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();

    json_doc.SetObject()
        .AddMember("processId", rapidjson::Value(getpid()), allocator)
        .AddMember("productEdition",
                   rapidjson::Value(MYSQL_ROUTER_VERSION_EDITION), allocator)
        .AddMember("timeStarted",
                   json_value_from_timepoint<rapidjson::Value::EncodingType>(
                       running_since_, allocator),
                   allocator)
        .AddMember("version", rapidjson::Value(MYSQL_ROUTER_VERSION),
                   allocator);

    char hname[256];  // enough for windows and unix
    if (0 == gethostname(hname, sizeof(hname))) {
      json_doc.AddMember("hostname", rapidjson::Value(hname, allocator),
                         allocator);
    }
  }
  send_json_document(req, HttpStatusCode::Ok, json_doc);

  return true;
}
