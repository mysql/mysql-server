/*
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_PREPARE_PARAM_HANDLER_H_
#define PLUGIN_X_SRC_PREPARE_PARAM_HANDLER_H_

#include <array>
#include <cinttypes>
#include <string>
#include <vector>

#include "mysql/com_data.h"
#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"

namespace xpl {

struct Placeholder_info {
  enum class Type { k_raw = 0, k_json };
  using Id = uint32_t;
  Placeholder_info(const Id id,
                   const Type type = Type::k_raw)  // NOLINT(runtime/explicit)
      : m_id(id), m_type(type) {}
  bool operator==(const Placeholder_info &other) const {
    return m_type == other.m_type && m_id == other.m_id;
  }
  Id m_id{0};
  Type m_type{Type::k_raw};
};

class Prepare_param_handler {
 public:
  using Arg_list = google::protobuf::RepeatedPtrField<Mysqlx::Datatypes::Any>;
  using Placeholder_list = std::vector<Placeholder_info>;
  using Param_list = std::vector<PS_PARAM>;
  using Param_value = std::array<unsigned char, 18>;
  using Param_value_list = std::vector<Param_value>;
  using Param_svalue_list = std::vector<std::string>;

  explicit Prepare_param_handler(const Placeholder_list &phs)
      : m_placeholders(phs) {}

  ngs::Error_code prepare_parameters(const Arg_list &args);
  ngs::Error_code check_argument_placeholder_consistency(
      const Arg_list::size_type args_size, const uint32_t args_offset) const;

  const Param_list &get_params() const { return m_params; }
  const Param_value_list &get_values() const { return m_param_values; }
  const Param_svalue_list &get_string_values() const { return m_string_values; }

 private:
  Prepare_param_handler(const Prepare_param_handler &) = delete;
  Prepare_param_handler &operator=(const Prepare_param_handler &) = delete;

  const Placeholder_list &m_placeholders;
  Param_list m_params;
  Param_value_list m_param_values;
  Param_svalue_list m_string_values;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_PREPARE_PARAM_HANDLER_H_
