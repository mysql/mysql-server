/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <memory>
#include <utility>

#include "mysql/components/library_mysys/my_hex_tools.h"

#include "json_reader.h"

namespace keyring_common {
namespace json_data {

static std::string schema_version_1_0 =
    "{"
    "  \"title\": \"Key store validator version 1.0\","
    "  \"description\": \"Expected schema for version 1.0\","
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"version\": {"
    "      \"description\": \"The file format version\","
    "      \"type\": \"string\""
    "    },"
    "    \"elements\": {"
    "      \"description\": \"Array of stored data\","
    "      \"type\": \"array\","
    "      \"items\": {"
    "        \"type\": \"object\","
    "        \"properties\": {"
    "          \"user\": { \"type\": \"string\" },"
    "          \"data_id\": { \"type\": \"string\" },"
    "          \"data_type\": { \"type\": \"string\" },"
    "          \"data\": { \"type\": \"string\" },"
    "          \"extension\" : { \"type\": \"array\" }"
    "        },"
    "        \"required\": ["
    "          \"user\","
    "          \"data_id\","
    "          \"data_type\","
    "          \"data\","
    "          \"extension\""
    "        ]"
    "      }"
    "    }"
    "  },"
    "  \"required\": ["
    "    \"version\","
    "    \"elements\""
    "  ]"
    "}";

/**
  Constructor

  @param [in] schema      JSON schema in string format
  @param [in] data        JSON data in string format
  @param [in] version_key JSON schema version information
  @param [in] array_key   Key for array of elements
*/
Json_reader::Json_reader(const std::string schema, const std::string data,
                         const std::string version_key,
                         const std::string array_key)
    : document_(),
      version_key_(version_key),
      array_key_(array_key),
      valid_(false) {
  rapidjson::Document schema_json;
  if (schema_json.Parse(schema).HasParseError()) return;
  if (document_.Parse(data).HasParseError()) return;

  rapidjson::SchemaDocument sd(schema_json);
  rapidjson::SchemaValidator validator(sd);
  if (!document_.Accept(validator)) return;

  valid_ = true;
}

/**
  Constructor

  @param [in] data JSON Data in string format

  Initializes JSON document with given data
  and sets validity state.
*/
Json_reader::Json_reader(const std::string data)
    : Json_reader(schema_version_1_0, data) {}

/* Default constructor - creates empty document */
Json_reader::Json_reader() : Json_reader(schema_version_1_0, "") {}

/**
  Get version info

  @returns version string in case same is
           present, empty string otherwise.
*/
std::string Json_reader::version() const {
  if (!valid_) return {};
  return document_[version_key_.c_str()].Get<std::string>();
}

/**
  Get number of elements in the document

  @returns number elements in the document
*/
size_t Json_reader::num_elements() const {
  if (!valid_) return 0;
  return static_cast<size_t>(document_[array_key_.c_str()].Size());
}

bool Json_reader::get_element(
    size_t index, meta::Metadata &metadata, data::Data &data,
    std::unique_ptr<Json_data_extension> &json_data_extension) const {
  if (!valid_ || index >= num_elements()) return true;
  const rapidjson::Value &elements = document_[array_key_.c_str()];
  if (!elements.IsArray()) return true;
  metadata = {elements[index]["data_id"].Get<std::string>(),
              elements[index]["user"].Get<std::string>()};
  std::string hex_data(elements[index]["data"].Get<std::string>());
  std::string unhex_data(hex_data.length() * 2, '\0');
  unsigned long length = unhex_string(
      hex_data.data(), hex_data.data() + hex_data.size(), &unhex_data[0]);
  unhex_data.resize(length);
  data = {unhex_data, elements[index]["data_type"].Get<std::string>()};
  json_data_extension = std::make_unique<Json_data_extension>();
  return false;
}

bool Json_reader::get_elements(output_vector &output) const {
  if (!valid_) return true;
  const rapidjson::Value &elements = document_[array_key_.c_str()];
  if (!elements.IsArray()) return true;
  for (rapidjson::SizeType t = 0; t < elements.Size(); ++t) {
    meta::Metadata metadata;
    data::Data secret_data;
    std::unique_ptr<Json_data_extension> ext;
    if (get_element(t, metadata, secret_data, ext) == true) {
      output.clear();
      return true;
    }
    output.push_back(
        std::make_pair(std::make_pair(metadata, secret_data), std::move(ext)));
  }
  return false;
}

}  // namespace json_data
}  // namespace keyring_common
