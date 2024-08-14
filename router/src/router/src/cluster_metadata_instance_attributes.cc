/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mysqlrouter/cluster_metadata_instance_attributes.h"

#include <stdexcept>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif
#include <rapidjson/document.h>

#include "common.h"  // get_from_map
#include "harness_assert.h"
#include "mysql/harness/event_state_tracker.h"
#include "mysql/harness/logging/logging.h"
#include "mysqld_error.h"
#include "mysqlrouter/utils.h"  // strtoui_checked
#include "mysqlrouter/utils_sqlstring.h"
#include "router_config.h"  // MYSQL_ROUTER_VERSION

using mysqlrouter::InstanceType;

namespace {

/**
 * @brief Returns value for the string field set in the attributes
 *
 * @param attributes    string containing JSON with the attributes
 * @param name          name of the field to be fetched
 *
 * @retval value of the attribute JSON field as string
 * @retval std::nullptr if the given field is missing
 * @retval error message if reading attribute from JSON failed.
 */
stdx::expected<std::optional<std::string>, std::string> get_string_attribute(
    const std::string_view &attributes, const std::string_view &name) {
  if (attributes.empty()) return std::nullopt;

  rapidjson::Document json_doc;
  json_doc.Parse(attributes.data(), attributes.size());

  if (!json_doc.IsObject()) {
    return stdx::unexpected("not a valid JSON object");
  }

  const auto it =
      json_doc.FindMember(rapidjson::Value{name.data(), name.size()});

  if (it == json_doc.MemberEnd()) {
    return std::nullopt;
  }

  if (!it->value.IsString()) {
    return stdx::unexpected("attributes." + std::string(name) +
                            " not a string");
  }

  return it->value.GetString();
}

/**
 * @brief Returns value for the boolean field set in the attributes
 *
 * @param attributes    string containing JSON with the attributes
 * @param name          name of the field to be fetched
 * @param default_value value to be returned in case the given attribute is not
 *                      present in the JSON
 *
 * @retval value of the attribute JSON field as boolean
 * @retval std::nullptr if the given field is missing
 * @retval error message if reading attribute from JSON failed.
 */
stdx::expected<bool, std::string> get_bool_tag(
    const std::string_view &attributes, const std::string_view &name,
    bool default_value) {
  if (attributes.empty()) return default_value;

  rapidjson::Document json_doc;
  json_doc.Parse(attributes.data(), attributes.size());

  if (!json_doc.IsObject()) {
    return stdx::unexpected("not a valid JSON object");
  }

  const auto tags_it = json_doc.FindMember("tags");
  if (tags_it == json_doc.MemberEnd()) {
    return default_value;
  }

  if (!tags_it->value.IsObject()) {
    return stdx::unexpected("tags - not a valid JSON object");
  }

  const auto tags = tags_it->value.GetObject();

  const auto it = tags.FindMember(rapidjson::Value{name.data(), name.size()});

  if (it == tags.MemberEnd()) {
    return default_value;
  }

  if (!it->value.IsBool()) {
    return stdx::unexpected("tags." + std::string(name) + " not a boolean");
  }

  return it->value.GetBool();
}

}  // namespace

namespace mysqlrouter {

stdx::expected<InstanceType, std::string> InstanceAttributes::get_instance_type(
    const std::string &attributes,
    const mysqlrouter::InstanceType default_instance_type) {
  const auto type_attr = get_string_attribute(attributes, "instance_type");
  if (!type_attr) {
    return stdx::unexpected(type_attr.error());
  }

  if (!type_attr.value()) {
    return default_instance_type;
  }

  auto result = mysqlrouter::str_to_instance_type(*type_attr.value());
  if (!result) {
    return stdx::unexpected("Unknown attributes.instance_type value: '" +
                            *type_attr.value() + "'");
  }

  return *result;
}

stdx::expected<bool, std::string> InstanceAttributes::get_hidden(
    const std::string &attributes, bool default_res) {
  return get_bool_tag(attributes, mysqlrouter::kNodeTagHidden, default_res);
}

stdx::expected<bool, std::string>
InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
    const std::string &attributes, bool default_res) {
  return get_bool_tag(attributes, mysqlrouter::kNodeTagDisconnectWhenHidden,
                      default_res);
}

}  // namespace mysqlrouter
