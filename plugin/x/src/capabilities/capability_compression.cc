/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/capabilities/capability_compression.h"

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <vector>

#include "my_dbug.h"

#include "plugin/x/ngs/include/ngs/interface/client_interface.h"
#include "plugin/x/ngs/include/ngs/mysqlx/getter_any.h"
#include "plugin/x/ngs/include/ngs/mysqlx/setter_any.h"
#include "plugin/x/src/capabilities/set_variable_adaptor.h"
#include "plugin/x/src/helper/string_case.h"

namespace xpl {

namespace {

const char *const k_algorithm_key = "algorithm";
const char *const k_server_style_key = "server_style";
const char *const k_client_style_key = "client_style";

enum class Compression_field {
  k_unknown,
  k_algorithm,
  k_client_style,
  k_server_style
};

Compression_field get_compression_field(const std::string &name) {
  static const std::map<std::string, Compression_field> fields{
      {k_algorithm_key, Compression_field::k_algorithm},
      {k_client_style_key, Compression_field::k_client_style},
      {k_server_style_key, Compression_field::k_server_style}};

  const std::string lowercase_name = to_lower(name);
  if (0 == fields.count(lowercase_name)) return Compression_field::k_unknown;

  return fields.at(lowercase_name);
}

}  // namespace

void Capability_compression::get_impl(Mysqlx::Datatypes::Any *any) {
  std::vector<std::string> values;
  auto obj = ngs::Setter_any::set_object(any);
  m_algorithms_variable.get_allowed_values(&values);
  ngs::Setter_any::set_object_field(obj, k_algorithm_key, values);
  m_server_style_variable.get_allowed_values(&values);
  ngs::Setter_any::set_object_field(obj, k_server_style_key, values);
  m_client_style_variable.get_allowed_values(&values);
  ngs::Setter_any::set_object_field(obj, k_client_style_key, values);
}

template <typename Variable, typename Result>
bool set_capability_value(const Variable &variable, const std::string &value,
                          Result *result) {
  if (!variable.is_allowed_value(value)) return false;

  *result = variable.get_value(value);
  return true;
}

ngs::Error_code Capability_compression::set_impl(
    const Mysqlx::Datatypes::Any &any) {
  if (!any.has_obj())
    return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                      "Capability prepare failed for '%s'", name().c_str());

  ngs::Error_code error;
  bool is_algorithm_set = false;
  bool is_server_style_set = false;
  bool is_client_style_set = false;
  m_server_style = ngs::Compression_style::k_none;
  m_client_style = ngs::Compression_style::k_none;

  for (const auto &f : any.obj().fld()) {
    const auto value = ngs::Getter_any::get_string_value(f.value(), &error);

    if (error) {
      // Overwrite the error with generic capability-get error
      return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                        "Capability prepare failed for '%s'", name().c_str());
    }

    switch (get_compression_field(f.key())) {
      case Compression_field::k_algorithm:
        is_algorithm_set =
            set_capability_value(m_algorithms_variable, value, &m_algorithm);
        if (!is_algorithm_set)
          return ngs::Error(ER_X_CAPABILITY_COMPRESSION_INVALID_ALGORITHM,
                            "Invalid or unsupported value for '%s.%s'",
                            name().c_str(), k_algorithm_key);
        break;

      case Compression_field::k_server_style:
        is_server_style_set = set_capability_value(m_server_style_variable,
                                                   value, &m_server_style);
        if (!is_server_style_set)
          return ngs::Error(ER_X_CAPABILITY_COMPRESSION_INVALID_SERVER_STYLE,
                            "Invalid or unsupported value for '%s.%s'",
                            name().c_str(), k_server_style_key);
        break;

      case Compression_field::k_client_style:
        is_client_style_set = set_capability_value(m_client_style_variable,
                                                   value, &m_client_style);
        if (!is_client_style_set)
          return ngs::Error(ER_X_CAPABILITY_COMPRESSION_INVALID_CLIENT_STYLE,
                            "Invalid or unsupported value for '%s.%s'",
                            name().c_str(), k_client_style_key);
        break;

      case Compression_field::k_unknown:
        return ngs::Error(ER_X_CAPABILITY_COMPRESSION_INVALID_OPTION,
                          "Invalid or unsupported option for '%s'",
                          name().c_str());
    }
  }

  if (!is_algorithm_set || (!is_server_style_set && !is_client_style_set)) {
    return ngs::Error(
        ER_X_CAPABILITY_COMPRESSION_MISSING_REQUIRED_FIELDS,
        "The algorithm and at least one style is required for '%s'",
        name().c_str());
  }

  return ngs::Success();
}

void Capability_compression::commit() {
  m_client->configure_compression_style(m_server_style);
  m_client->configure_compression_client_style(m_client_style);
  m_client->enable_compression_algo(m_algorithm);
}

}  // namespace xpl
