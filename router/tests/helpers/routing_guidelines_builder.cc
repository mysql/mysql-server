/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "routing_guidelines_builder.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

namespace guidelines_builder {

using JsonValue =
    rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator>;

std::string create(const std::vector<Destination> &destinations,
                   const std::vector<Route> &routes, const std::string &name,
                   const std::string &version) {
  JsonDocument json_guidelines_doc;
  rapidjson::CrtAllocator allocator;
  json_guidelines_doc.SetObject();
  allocator = json_guidelines_doc.GetAllocator();

  json_guidelines_doc.AddMember(
      "name", JsonValue(name.data(), name.size(), allocator), allocator);

  json_guidelines_doc.AddMember(
      "version", JsonValue(version.data(), version.size(), allocator),
      allocator);

  JsonValue destinations_array{rapidjson::kArrayType};
  for (const auto &dest : destinations) {
    destinations_array.PushBack(
        JsonValue(rapidjson::kObjectType)
            .AddMember("name",
                       JsonValue(dest.name.data(), dest.name.size(), allocator),
                       allocator)
            .AddMember(
                "match",
                JsonValue(dest.match.data(), dest.match.size(), allocator),
                allocator),
        allocator);
  }
  json_guidelines_doc.AddMember("destinations", destinations_array, allocator);

  JsonValue routes_array{rapidjson::kArrayType};
  for (const auto &route : routes) {
    JsonValue route_destinations{rapidjson::kArrayType};
    for (const auto &sink : route.route_sinks) {
      JsonValue destination_classes(rapidjson::kArrayType);
      JsonValue dest_class_str(rapidjson::kStringType);

      for (const auto &dest_class : sink.destination_names) {
        dest_class_str.SetString(dest_class.data(), dest_class.size(),
                                 allocator);
        destination_classes.PushBack(dest_class_str, allocator);
      }

      route_destinations.PushBack(
          JsonValue(rapidjson::kObjectType)
              .AddMember("strategy",
                         JsonValue(sink.strategy.data(), sink.strategy.size(),
                                   allocator),
                         allocator)
              .AddMember("priority", sink.priority, allocator)
              .AddMember("classes", destination_classes, allocator),
          allocator);
    }

    routes_array.PushBack(
        JsonValue(rapidjson::kObjectType)
            .AddMember(
                "name",
                JsonValue(route.name.data(), route.name.size(), allocator),
                allocator)
            .AddMember("enabled", route.enabled, allocator)
            .AddMember("connectionSharingAllowed", route.sharing_allowed,
                       allocator)
            .AddMember(
                "match",
                JsonValue(route.match.data(), route.match.size(), allocator),
                allocator)
            .AddMember("destinations", route_destinations, allocator),
        allocator);
  }
  json_guidelines_doc.AddMember("routes", routes_array, allocator);

  using JsonStringBuffer =
      rapidjson::GenericStringBuffer<rapidjson::UTF8<>,
                                     rapidjson::CrtAllocator>;
  JsonStringBuffer out_buffer;
  rapidjson::PrettyWriter<JsonStringBuffer> out_writer{out_buffer};
  json_guidelines_doc.Accept(out_writer);
  return out_buffer.GetString();
}

}  // namespace guidelines_builder
