/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "rest_host_cache_status.h"

#include <string>
#include <vector>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "mysqlrouter/host_cache_component.h"
#include "mysqlrouter/rest_api_utils.h"

bool RestHostCacheStatus::on_handle_request(
    http::base::Request &req, const std::string & /* base_path */,
    const std::vector<std::string> & /*path_matches*/) {
  if (!ensure_no_params(req)) return true;

  auto &inst = HostCacheComponent::get_instance();

  auto &out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/json");

  rapidjson::Document json_doc;
  {
    rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();
    const auto temporary_size = inst.get_temporary_size();
    const auto entry_size = inst.get_cache_size();

    {
      auto hits = inst.get_cache_hits();
      auto uses = inst.get_cache_used();
      rapidjson::Value obj_cache(rapidjson::kObjectType);
      obj_cache.AddMember("hits", inst.get_cache_hits(), allocator);
      obj_cache.AddMember("misses", (uses - hits), allocator);
      obj_cache.AddMember("inserts", inst.get_cache_inserts(), allocator);
      obj_cache.AddMember("evictions", inst.get_cache_drops(), allocator);
      obj_cache.AddMember("expiredPurges", inst.get_cache_expired(), allocator);

      json_doc.SetObject()
          .AddMember<uint64_t>("numberOfEntries", entry_size, allocator)
          .AddMember<uint64_t>("numberOfTemporaryEntries", temporary_size,
                               allocator)
          .AddMember("cache", obj_cache.Move(), allocator);
    }
  }
  send_json_document(req, HttpStatusCode::Ok, json_doc);

  return true;
}
