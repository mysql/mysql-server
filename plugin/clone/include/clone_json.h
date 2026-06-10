/* Copyright (c) 2026 Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#pragma once

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace myclone {
/**
  Insert Key-Value to JSON document (Object type)
  @param[in]      key   Key string
  @param[in]      value Value string
  @param[in,out]  doc   JSON Document
*/
inline void insert_key_value(const std::string &key, const std::string &value,
                             rapidjson::Document &doc) {
  assert(doc.IsObject());
  auto &alloc = doc.GetAllocator();
  const auto key_length = static_cast<rapidjson::SizeType>(key.length());
  const auto value_length = static_cast<rapidjson::SizeType>(value.length());

  rapidjson::Value object_key(key.c_str(), key_length, alloc);
  rapidjson::Value object_value(value.c_str(), value_length, alloc);

  doc.AddMember(object_key, object_value, alloc);
}

/**
  Insert list of Key-Value pairs to JSON document (Object type)
  @param[in]  key_values  Vector of Key-Value pairs
  @param[out] doc         JSON Document
*/
inline void insert_key_value(const Key_Values &key_values,
                             rapidjson::Document &doc) {
  for (const auto &key_value : key_values) {
    insert_key_value(key_value.first, key_value.second, doc);
  }
}

/**
  Parse a JSON string into a Document.
  @param[in]      json  String representation of a JSON object
  @param[in,out]  doc   JSON Document where the parsed JSON is returned
  @return true on success, false if parsing failed
*/
inline bool parse_json_object(const char *json, rapidjson::Document &doc) {
  if (json == nullptr) {
    return false;
  }

  doc.Parse(json);

  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }
  return true;
}

/**
  Convert the JSON value into a string
  @param[in] value JSON Value or Document of Object Type
*/
inline std::string to_json_string(const rapidjson::Value &value) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  value.Accept(writer);
  return std::string(buffer.GetString(), buffer.GetSize());
}

/**
  Convert the Key_Values vector into a string
  @param[in]  key_values  Vector of key_value pairs
*/
inline std::string to_json_string(const Key_Values &key_values) {
  rapidjson::Document doc;
  doc.SetObject();

  insert_key_value(key_values, doc);
  return to_json_string(doc);
}
}  // namespace myclone