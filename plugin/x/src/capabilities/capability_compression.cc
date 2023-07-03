/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
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
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "my_dbug.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/capabilities/set_variable_adaptor.h"
#include "plugin/x/src/helper/string_case.h"
#include "plugin/x/src/interface/client.h"
#include "plugin/x/src/ngs/mysqlx/getter_any.h"
#include "plugin/x/src/ngs/mysqlx/setter_any.h"

namespace xpl {

namespace {

const char *const k_algorithm_key = "algorithm";
const char *const k_server_max_combine_messages = "server_max_combine_messages";
const char *const k_server_combine_mixed_messages =
    "server_combine_mixed_messages";
const char *const k_level_key = "level";

enum class Compression_field {
  k_unknown,
  k_algorithm,
  k_server_max_messages,
  k_server_combine_messages,
  k_level
};

Compression_field get_compression_field(const std::string &name) {
  static const std::map<std::string, Compression_field> fields{
      {k_algorithm_key, Compression_field::k_algorithm},
      {k_server_max_combine_messages, Compression_field::k_server_max_messages},
      {k_server_combine_mixed_messages,
       Compression_field::k_server_combine_messages},
      {k_level_key, Compression_field::k_level},
  };

  const std::string lowercase_name = to_lower(name);
  if (0 == fields.count(lowercase_name)) return Compression_field::k_unknown;

  return fields.at(lowercase_name);
}

template <typename Variable, typename Result>
bool set_capability_value(const Variable &variable, const std::string &value,
                          Result *result) {
  if (!variable.is_allowed_value(value)) return false;

  *result = variable.get_value(value);
  return true;
}

}  // namespace

void Capability_compression::get_impl(Mysqlx::Datatypes::Any *any) {
  std::vector<std::string> values;
  auto obj = ngs::Setter_any::set_object(any);
  m_algorithms_variable.get_allowed_values(&values);
  ngs::Setter_any::set_object_field(obj, k_algorithm_key, values);
}

ngs::Error_code Capability_compression::set_impl(
    const Mysqlx::Datatypes::Any &any) {
  if (!any.has_obj())
    return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                      "Capability prepare failed for '%s'", name().c_str());

  ngs::Error_code error;
  bool is_algorithm_set = false;
  m_max_messages = -1;
  m_combine_messages = true;
  m_level.reset();

  for (const auto &f : any.obj().fld()) {
    switch (get_compression_field(f.key())) {
      case Compression_field::k_algorithm: {
        const auto value = ngs::Getter_any::get_string_value(f.value(), &error);

        if (error) {
          // Overwrite the error with generic capability-get error
          return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                            "Capability prepare failed for '%s'",
                            name().c_str());
        }

        is_algorithm_set =
            set_capability_value(m_algorithms_variable, value, &m_algorithm);
        if (!is_algorithm_set)
          return ngs::Error(ER_X_CAPABILITY_COMPRESSION_INVALID_ALGORITHM,
                            "Invalid or unsupported value for '%s.%s'",
                            name().c_str(), k_algorithm_key);
      } break;

      case Compression_field::k_server_max_messages: {
        const auto value =
            ngs::Getter_any::get_numeric_value<int32_t>(f.value(), &error);

        if (error) {
          // Overwrite the error with generic capability-get error
          return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                            "Capability prepare failed for '%s'",
                            name().c_str());
        }
        m_max_messages = value;
      } break;

      case Compression_field::k_server_combine_messages: {
        const auto value =
            ngs::Getter_any::get_numeric_value<bool>(f.value(), &error);

        if (error) {
          // Overwrite the error with generic capability-get error
          return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                            "Capability prepare failed for '%s'",
                            name().c_str());
        }

        m_combine_messages = value;
      } break;

      case Compression_field::k_level: {
        const auto value =
            ngs::Getter_any::get_numeric_value<int64_t>(f.value(), &error);
        if (error) {
          // Overwrite the error with generic capability-get error
          return ngs::Error(ER_X_CAPABILITIES_PREPARE_FAILED,
                            "Capability prepare failed for '%s'",
                            name().c_str());
        }
        m_level = value;
      } break;

      case Compression_field::k_unknown:
        return ngs::Error(ER_X_CAPABILITY_COMPRESSION_INVALID_OPTION,
                          "Invalid or unsupported option '%s.%s'",
                          name().c_str(), f.key().c_str());
    }
  }

  if (!is_algorithm_set) {
    return ngs::Error(ER_X_CAPABILITY_COMPRESSION_MISSING_REQUIRED_FIELDS,
                      "The algorithm is required for '%s'", name().c_str());
  }

  return ngs::Success();
}

void Capability_compression::commit() {
  m_client->configure_compression_opts(m_algorithm, m_max_messages,
                                       m_combine_messages, m_level);
}

}  // namespace xpl
