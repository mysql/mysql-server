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

#include "rest_metadata_cache_status.h"

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

bool RestMetadataCacheStatus::on_handle_request(
    HttpRequest &req, const std::string & /* base_path */,
    const std::vector<std::string> &path_matches) {
  if (!ensure_no_params(req)) return true;

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
    // the metadata-plugin may not be initialized when we try to ask for its
    // status
    metadata_cache::MetadataCacheAPI::RefreshStatus refresh_status;

    refresh_status =
        metadata_cache::MetadataCacheAPI::instance()->get_refresh_status();

    json_doc.SetObject()
        .AddMember("refreshFailed",
                   rapidjson::Value(refresh_status.refresh_failed), allocator)
        .AddMember("refreshSucceeded",
                   rapidjson::Value(refresh_status.refresh_succeeded),
                   allocator);

    if (std::chrono::system_clock::to_time_t(
            refresh_status.last_refresh_succeeded) > 0) {
      json_doc
          .AddMember("timeLastRefreshSucceeded",
                     json_value_from_timepoint<rapidjson::Value::EncodingType>(
                         refresh_status.last_refresh_succeeded, allocator),
                     allocator)
          .AddMember("lastRefreshHostname",
                     json_value_from_string(
                         refresh_status.last_metadata_server_host, allocator),
                     allocator)
          .AddMember("lastRefreshPort",
                     refresh_status.last_metadata_server_port, allocator)
          //
          ;
    }
    if (std::chrono::system_clock::to_time_t(
            refresh_status.last_refresh_failed) > 0) {
      json_doc.AddMember(
          "timeLastRefreshFailed",
          json_value_from_timepoint<rapidjson::Value::EncodingType>(
              refresh_status.last_refresh_failed, allocator),
          allocator)
          //
          ;
    }
  }
  send_json_document(req, HttpStatusCode::Ok, json_doc);

  return true;
}
