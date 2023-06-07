/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include <my_rapidjson_size_t.h>

#include <rapidjson/document.h>
#include <memory>
#include "mrs/database/entry/object.h"
#include "mrs/database/helper/object_checksum.h"
#include "router/src/http/src/digest.h"

namespace mrs {
namespace database {

namespace {
bool compute_object_checksum(Digest *digest,
                             std::shared_ptr<entry::Object> object,
                             const rapidjson::Value &doc) {
  for (const auto &m : doc.GetObject()) {
    std::string data;

    auto field = object->get_field(m.name.GetString());
    if (field && field->enabled && !field->no_check) {
      auto ref_field = std::dynamic_pointer_cast<entry::ReferenceField>(field);
      if (ref_field && !m.value.IsNull()) {
        if (ref_field->is_array) {
          if (!m.value.IsArray()) return false;
          for (const auto &v : m.value.GetArray()) {
            if (!v.IsObject()) return false;
            compute_object_checksum(digest, ref_field->nested_object, v);
          }
        } else {
          if (!m.value.IsObject()) return false;
          compute_object_checksum(digest, ref_field->nested_object, m.value);
        }
      } else {
        switch (m.value.GetType()) {
          case rapidjson::kStringType:
            digest->update(m.name.GetString(), m.name.GetStringLength());
            digest->update(":\"");
            digest->update(m.value.GetString(), m.value.GetStringLength());
            break;

          case rapidjson::kNullType:
            digest->update(m.name.GetString(), m.name.GetStringLength());
            digest->update(":null");
            break;

          case rapidjson::kTrueType:
            digest->update(m.name.GetString(), m.name.GetStringLength());
            digest->update(":true");
            break;

          case rapidjson::kFalseType:
            digest->update(m.name.GetString(), m.name.GetStringLength());
            digest->update(":false");
            break;

          case rapidjson::kNumberType:
            digest->update(m.name.GetString(), m.name.GetStringLength());
            digest->update(":");
            if (m.value.IsDouble()) {
              auto d = m.value.GetDouble();
              digest->update(reinterpret_cast<const char *>(&d), sizeof(d));
            } else {
              auto i = m.value.GetInt();
              digest->update(reinterpret_cast<const char *>(&i), sizeof(i));
            }
            break;

          case rapidjson::kArrayType:
          case rapidjson::kObjectType:
            assert(0);
            return false;
        }
      }
    }
  }
  return true;
}

std::string string_to_hex(std::string_view s) {
  constexpr char hexmap[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  std::string encoded;

  encoded.reserve(s.size() * 2);

  for (const auto cur_char : s) {
    encoded.push_back(hexmap[(cur_char & 0xF0) >> 4]);
    encoded.push_back(hexmap[cur_char & 0x0F]);
  }

  return encoded;
}

}  // namespace

std::string compute_checksum(std::shared_ptr<entry::Object> object,
                             const rapidjson::Document &doc) {
  Digest digest(Digest::Type::Sha256);

  if (!compute_object_checksum(&digest, object, doc)) return {};

  std::string result;
  result.resize(digest.digest_size(Digest::Type::Sha256));
  digest.finalize(result);

  return string_to_hex(result);
}

rapidjson::Document compute_and_embed_etag(
    std::shared_ptr<entry::Object> object, std::string_view doc) {
  rapidjson::Document json_doc;
  json_doc.Parse((const char *)doc.data(), doc.size());
  if (json_doc.HasParseError() || !json_doc.IsObject()) return {};

  std::string etag = compute_checksum(object, json_doc);
  if (etag.empty()) return {};

  rapidjson::Value metadata(rapidjson::kObjectType);

  rapidjson::Value json_etag;
  json_etag.SetString(etag.data(), etag.length(), json_doc.GetAllocator());
  metadata.AddMember("etag", json_etag, json_doc.GetAllocator());

  json_doc.AddMember("_metadata", metadata, json_doc.GetAllocator());

  return json_doc;
}

}  // namespace database
}  // namespace mrs