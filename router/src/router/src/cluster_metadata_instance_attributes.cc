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

#include "mysqlrouter/cluster_metadata_instance_attributes.h"

#include <cstring>
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
 * @brief Returns value fo the bool tag set in the attributes
 *
 * @param attributes    string containing JSON with the attributes
 * @param name          name of the tag to be fetched
 * @param default_value value to be returned if the given tag is missing or
 * invalid or if the JSON string is invalid
 * @param[out] out_warning  output parameter where the function sets the
 * descriptive warning in case there was a JSON parsing error
 *
 * @note the function always sets out_warning to "" at the beginning
 *
 * @return value of the bool tag
 */
bool get_bool_tag(const std::string_view &attributes,
                  const std::string_view &name, bool default_value,
                  std::string &out_warning) {
  out_warning = "";
  if (attributes.empty()) return default_value;

  rapidjson::Document json_doc;
  json_doc.Parse(attributes.data(), attributes.size());

  if (!json_doc.IsObject()) {
    out_warning = "not a valid JSON object";
    return default_value;
  }

  const auto tags_it = json_doc.FindMember("tags");
  if (tags_it == json_doc.MemberEnd()) {
    return default_value;
  }

  if (!tags_it->value.IsObject()) {
    out_warning = "tags - not a valid JSON object";
    return default_value;
  }

  const auto tags = tags_it->value.GetObject();

  const auto it = tags.FindMember(rapidjson::Value{name.data(), name.size()});

  if (it == tags.MemberEnd()) {
    return default_value;
  }

  if (!it->value.IsBool()) {
    out_warning = "tags." + std::string(name) + " not a boolean";
    return default_value;
  }

  return it->value.GetBool();
}

}  // namespace

namespace mysqlrouter {

bool InstanceAttributes::get_hidden(const std::string &attributes,
                                    std::string &out_warning) {
  return get_bool_tag(attributes, mysqlrouter::kNodeTagHidden,
                      mysqlrouter::kNodeTagHiddenDefault, out_warning);
}

bool InstanceAttributes::get_disconnect_existing_sessions_when_hidden(
    const std::string &attributes, std::string &out_warning) {
  return get_bool_tag(attributes, mysqlrouter::kNodeTagDisconnectWhenHidden,
                      mysqlrouter::kNodeTagDisconnectWhenHiddenDefault,
                      out_warning);
}

}  // namespace mysqlrouter
