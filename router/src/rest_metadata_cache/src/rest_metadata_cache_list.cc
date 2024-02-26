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

#include "rest_metadata_cache_list.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/rest_api_utils.h"

template <class AllocatorType>
rapidjson::Value json_value_from_string(const std::string &s,
                                        AllocatorType &allocator) {
  return {s.data(), s.size(), allocator};
}

bool RestMetadataCacheList::on_handle_request(
    HttpRequest &req, const std::string & /* base_path */,
    const std::vector<std::string> & /* path_matches */) {
  if (!ensure_no_params(req)) return true;

  auto out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/json");

  rapidjson::Document json_doc;
  {
    rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();

    json_doc.SetObject();

    {
      rapidjson::Value json_items(rapidjson::kArrayType);
      std::string inst_name =
          metadata_cache::MetadataCacheAPI::instance()->instance_name();
      json_items.PushBack(
          rapidjson::Value(rapidjson::kObjectType)
              .AddMember("name", json_value_from_string(inst_name, allocator),
                         allocator),
          allocator);

      json_doc.AddMember("items", json_items, allocator);
    }
  }
  send_json_document(req, HttpStatusCode::Ok, json_doc);

  return true;
}
