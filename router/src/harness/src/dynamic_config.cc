/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

// enable using Rapidjson library with std::string
#define RAPIDJSON_HAS_STDSTRING 1

#include "mysql/harness/dynamic_config.h"

#include <cassert>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

using JsonAllocator = rapidjson::CrtAllocator;
using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<>, JsonAllocator>;
using JsonStringBuffer =
    rapidjson::GenericStringBuffer<rapidjson::UTF8<>, JsonAllocator>;
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, JsonAllocator>;

namespace mysql_harness {

JsonDocument DynamicConfig::get_json(const ValueType value_type) const {
  JsonDocument json_doc;
  json_doc.SetObject();
  auto &allocator = json_doc.GetAllocator();

  auto &config = get_config(value_type);

  for (const auto &section : config) {
    const std::string &section_name = section.first.first;
    const std::string &subsection_name = section.first.second;
    if (!json_doc.HasMember(section_name)) {
      JsonValue section_object;
      section_object.SetObject();

      JsonValue section_name_json;
      section_name_json.SetString(section_name.c_str(), section_name.size(),
                                  allocator);

      json_doc.AddMember(section_name_json, section_object, allocator);
    }

    auto &section_object = json_doc[section_name];
    assert(section_object.IsObject());

    if (!subsection_name.empty()) {
      JsonValue subsection_object;
      subsection_object.SetObject();

      JsonValue subsection_name_json;
      subsection_name_json.SetString(subsection_name.c_str(),
                                     subsection_name.size(), allocator);

      section_object.AddMember(subsection_name_json, subsection_object,
                               allocator);
    }

    auto &parent_object = subsection_name.empty()
                              ? section_object
                              : section_object[subsection_name.c_str()];

    for (const auto &option : section.second.options) {
      const std::string &option_name = option.first;
      JsonValue opion_name_json;
      opion_name_json.SetString(option_name.c_str(), option_name.size(),
                                allocator);

      OptionValue value = option.second;
      if (std::holds_alternative<int64_t>(value)) {
        parent_object.AddMember(opion_name_json, std::get<int64_t>(value),
                                allocator);
      } else if (std::holds_alternative<std::string>(value)) {
        const std::string &value_str = std::get<std::string>(value);
        JsonValue value_str_json;
        value_str_json.SetString(value_str.c_str(), value_str.size(),
                                 allocator);
        parent_object.AddMember(opion_name_json, value_str_json, allocator);
      } else if (std::holds_alternative<double>(value)) {
        parent_object.AddMember(opion_name_json, std::get<double>(value),
                                allocator);
      } else if (std::holds_alternative<bool>(value)) {
        parent_object.AddMember(opion_name_json, std::get<bool>(value),
                                allocator);
      } else {
        assert(std::holds_alternative<std::monostate>(value));
        // skip
      }
    }
  }

  return json_doc;
}

std::string DynamicConfig::get_json_as_string(
    const ValueType value_type) const {
  auto json_doc = get_json(value_type);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  json_doc.Accept(writer);

  return buffer.GetString();
}

void DynamicConfig::set_option(const ValueType value_type,
                               const SectionId &section_id,
                               const OptionName &option_name,
                               const OptionValue &value) {
  auto &config = get_config(value_type);

  if (config.count(section_id) == 0) {
    SectionOptions section_options;
    section_options[option_name] = value;
    config[section_id].options = std::move(section_options);
  } else {
    auto &section_options = config[section_id].options;
    section_options[option_name] = value;
  }
}

void DynamicConfig::set_option_configured(const SectionId &section_id,
                                          const OptionName &option_name,
                                          const OptionValue &value) {
  set_option(ValueType::ConfiguredValue, section_id, option_name, value);
}

void DynamicConfig::set_option_default(
    const SectionId &section_id, const OptionName &option_name,
    const OptionValue &default_value_cluster,
    const OptionValue &default_value_clusterset) {
  set_option(ValueType::DefaultForCluster, section_id, option_name,
             default_value_cluster);
  set_option(ValueType::DefaultForClusterSet, section_id, option_name,
             default_value_clusterset);
}

void DynamicConfig::set_option_default(const SectionId &section_id,
                                       const OptionName &option_name,
                                       const OptionValue &default_value) {
  set_option_default(section_id, option_name, default_value, default_value);
}

/* static */ DynamicConfig &DynamicConfig::instance() {
  static DynamicConfig instance_;
  return instance_;
}

void DynamicConfig::clear() {
  configured_.clear();
  defaults_cluster_.clear();
  defaults_clusterset_.clear();
}

DynamicConfig::Config &DynamicConfig::get_config(
    const DynamicConfig::ValueType value_type) {
  switch (value_type) {
    case ValueType::ConfiguredValue:
      return configured_;
    case ValueType::DefaultForCluster:
      return defaults_cluster_;
      break;
    default:
      assert(value_type == ValueType::DefaultForClusterSet);
      // fallback
  }

  return defaults_clusterset_;
}

DynamicConfig::Config const &DynamicConfig::get_config(
    const DynamicConfig::ValueType value_type) const {
  switch (value_type) {
    case ValueType::ConfiguredValue:
      return configured_;
    case ValueType::DefaultForCluster:
      return defaults_cluster_;
      break;
    default:
      assert(value_type == ValueType::DefaultForClusterSet);
      // fallback
  }

  return defaults_clusterset_;
}

}  // namespace mysql_harness