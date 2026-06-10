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

#include "rest_host_cache_spec.h"

#include <array>
#include <string>
#include <string_view>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>

#include "mysqlrouter/rest_api_component.h"

using JsonPointer = RestApiComponent::JsonPointer;
using JsonValue = RestApiComponent::JsonValue;

static JsonPointer::Token make_json_pointer_token(std::string_view token) {
  return {token.data(), token.size(), rapidjson::kPointerInvalidIndex};
}

static const std::array host_cache_entries_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("HostCacheEntries"),
};

static const std::array host_cache_config_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("HostCacheConfig"),
};

static const std::array host_cache_status_def_tokens{
    make_json_pointer_token("definitions"),
    make_json_pointer_token("HostCacheStatus"),
};

static const std::array host_cache_status_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/host_cache/status"),
};

static const std::array host_cache_config_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/host_cache/config"),
};

static const std::array host_cache_entries_path_tokens{
    make_json_pointer_token("paths"),
    make_json_pointer_token("/host_cache/entries"),
};

static const std::array tags_append_tokens{
    make_json_pointer_token("tags"),
    make_json_pointer_token("-"),
};

static std::string json_pointer_stringfy(const JsonPointer &ptr) {
  rapidjson::StringBuffer sb;
  ptr.StringifyUriFragment(sb);
  return {sb.GetString(), sb.GetSize()};
}

void append_specification(RestApiComponent::JsonDocument &spec_doc) {
  auto &allocator = spec_doc.GetAllocator();

  {
    JsonPointer ptr(tags_append_tokens.data(), tags_append_tokens.size());

    ptr.Set(spec_doc,
            JsonValue(rapidjson::kObjectType)
                .AddMember("name", "hostcache", allocator)
                .AddMember("description", "Host Cache", allocator),
            allocator);
  }

  // /definitions/HostCacheStatus
  const RestApiComponent::JsonPointer host_cache_status_def_ptr(
      host_cache_status_def_tokens.data(), host_cache_status_def_tokens.size());

  JsonValue host_cache_status_cache_object(rapidjson::kObjectType);
  host_cache_status_cache_object.AddMember("type", "object", allocator)
      .AddMember("properties",
                 JsonValue(rapidjson::kObjectType)
                     .AddMember("hits",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator)
                     .AddMember("misses",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator)
                     .AddMember("inserts",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator)
                     .AddMember("evictions",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator)
                     .AddMember("expiredPurges",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator),
                 allocator);
  host_cache_status_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("cache", host_cache_status_cache_object.Move(),
                             allocator)
                  .AddMember("numberOfEntries",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("numberOfTemporaryEntries",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator),
              allocator),
      allocator);

  std::string host_cache_status_def_ptr_str =
      json_pointer_stringfy(host_cache_status_def_ptr);

  // /definitions/HostCacheConfig
  const RestApiComponent::JsonPointer host_cache_config_def_ptr(
      host_cache_config_def_tokens.data(), host_cache_config_def_tokens.size());

  host_cache_config_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("enabled",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "boolean", allocator),
                             allocator)
                  .AddMember("ttlSuccessSeconds",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("ttlNegativeSeconds",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("maxEntries",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "integer", allocator),
                             allocator)
                  .AddMember("jitterRatio",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "number", allocator),
                             allocator),
              allocator),
      allocator);

  std::string host_cache_config_def_ptr_str =
      json_pointer_stringfy(host_cache_config_def_ptr);

  // /definitions/HostCacheEntries
  const RestApiComponent::JsonPointer host_cache_entries_def_ptr(
      host_cache_entries_def_tokens.data(),
      host_cache_entries_def_tokens.size());

  JsonValue host_cache_entries_object(rapidjson::kObjectType);
  host_cache_entries_object.AddMember("type", "object", allocator)
      .AddMember("properties",
                 JsonValue(rapidjson::kObjectType)
                     .AddMember("secondsRemainingTtl",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator)
                     .AddMember("ttl",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator)
                     .AddMember("cacheHits",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator)
                     .AddMember("singleFlight",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator),
                 allocator);

  JsonValue host_cache_in_progress_object(rapidjson::kObjectType);
  host_cache_in_progress_object.AddMember("type", "object", allocator)
      .AddMember("properties",
                 JsonValue(rapidjson::kObjectType)
                     .AddMember("age",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator)
                     .AddMember("consumers",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("type", "integer", allocator),
                                allocator),
                 allocator);

  host_cache_entries_def_ptr.Set(
      spec_doc,
      JsonValue(rapidjson::kObjectType)
          .AddMember("type", "object", allocator)
          .AddMember(
              "properties",
              JsonValue(rapidjson::kObjectType)
                  .AddMember("entries",
                             JsonValue(rapidjson::kObjectType)
                                 .AddMember("type", "object", allocator)
                                 .AddMember("additionalProperties",
                                            host_cache_entries_object.Move(),
                                            allocator),
                             allocator)
                  .AddMember(
                      "inProgress",
                      JsonValue(rapidjson::kObjectType)
                          .AddMember("type", "object", allocator)
                          .AddMember("additionalProperties",
                                     host_cache_in_progress_object.Move(),
                                     allocator),
                      allocator),
              allocator),
      allocator);

  std::string host_cache_entries_def_ptr_str =
      json_pointer_stringfy(host_cache_entries_def_ptr);

  // /paths/hostCacheStatus
  {
    JsonPointer ptr(host_cache_status_path_tokens.data(),
                    host_cache_status_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("hostcache", allocator),
                               allocator)
                    .AddMember("description",
                               "Get status of the host_cache plugin", allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember(
                                        "description",
                                        "Get status of the host_cache plugin",
                                        allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    host_cache_status_def_ptr_str
                                                        .data(),
                                                    host_cache_status_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator),
                        allocator),
                allocator),
        allocator);
  }

  // /paths/hostCacheConfig
  {
    JsonPointer ptr(host_cache_config_path_tokens.data(),
                    host_cache_config_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("hostcache", allocator),
                               allocator)
                    .AddMember("description", "Get config of host_cache plugin",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember("description",
                                               "config of host_cache plugin",
                                               allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    host_cache_config_def_ptr_str
                                                        .data(),
                                                    host_cache_config_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator),
                        allocator),
                allocator));
  }

  // /paths/hostCacheEntrie
  {
    JsonPointer ptr(host_cache_entries_path_tokens.data(),
                    host_cache_entries_path_tokens.size());

    ptr.Set(
        spec_doc,
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "get",
                JsonValue(rapidjson::kObjectType)
                    .AddMember("tags",
                               JsonValue(rapidjson::kArrayType)
                                   .PushBack("hostcache", allocator),
                               allocator)
                    .AddMember("description",
                               "Get entries of the host_cache plugin",
                               allocator)
                    .AddMember(
                        "responses",
                        JsonValue(rapidjson::kObjectType)
                            .AddMember(
                                "200",
                                JsonValue(rapidjson::kObjectType)
                                    .AddMember(
                                        "description",
                                        "entries of the host_cache plugin",
                                        allocator)
                                    .AddMember(
                                        "schema",
                                        JsonValue(rapidjson::kObjectType)
                                            .AddMember(
                                                "$ref",
                                                JsonValue(
                                                    host_cache_entries_def_ptr_str
                                                        .data(),
                                                    host_cache_entries_def_ptr_str
                                                        .size(),
                                                    allocator),
                                                allocator),
                                        allocator),
                                allocator),
                        allocator),
                allocator),
        allocator);
  }
}
