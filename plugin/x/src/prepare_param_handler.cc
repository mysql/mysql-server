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

#include "plugin/x/src/prepare_param_handler.h"

#include <my_byteorder.h>

#include <string>

#include "plugin/x/src/ngs/mysqlx/getter_any.h"
#include "plugin/x/src/xpl_error.h"

namespace xpl {

namespace {
class Any_to_param_handler {
 public:
  Any_to_param_handler(Prepare_param_handler::Param_list *params,
                       Prepare_param_handler::Param_value_list *param_values)
      : m_params(params), m_param_values(param_values) {}

  void operator()() {
    m_params->push_back(
        {true, MYSQL_TYPE_NULL, false, nullptr, 0ul, nullptr, 0ul});
  }
  void operator()(const int64_t value) {
    m_params->push_back({false, MYSQL_TYPE_LONGLONG, false, store_value(value),
                         sizeof(value), nullptr, 0ul});
  }
  void operator()(const uint64_t value) {
    m_params->push_back({false, MYSQL_TYPE_LONGLONG, true, store_value(value),
                         sizeof(value), nullptr, 0ul});
  }
  void operator()(const std::string &value) {
    m_params->push_back({false, MYSQL_TYPE_STRING, false,
                         reinterpret_cast<const unsigned char *>(value.data()),
                         static_cast<unsigned long>(value.size()), nullptr,
                         0ul});  // NOLINT(runtime/int)
  }
  void operator()(const std::string &value, const uint32_t) {
    operator()(value);
  }
  void operator()(const double value) {
    m_params->push_back({false, MYSQL_TYPE_DOUBLE, false, store_value(value),
                         sizeof(value), nullptr, 0ul});
  }
  void operator()(const float value) {
    m_params->push_back({false, MYSQL_TYPE_FLOAT, false, store_value(value),
                         sizeof(value), nullptr, 0ul});
  }
  void operator()(const bool value) {
    m_params->push_back(
        {false, MYSQL_TYPE_TINY, false, store_value(value), 1ul, nullptr, 0ul});
  }

 protected:
  using Param_value = Prepare_param_handler::Param_value;
  template <typename T>
  const unsigned char *store_value(const T value) const {
    auto pos = m_param_values->emplace(m_param_values->end(), Param_value());
    store(pos->data(), value);
    return pos->data();
  }

  void store(unsigned char *buf, const uint64_t v) const { int8store(buf, v); }
  void store(unsigned char *buf, const int64_t v) const { int8store(buf, v); }
  void store(unsigned char *buf, const double v) const { float8store(buf, v); }
  void store(unsigned char *buf, const float v) const { float4store(buf, v); }
  void store(unsigned char *buf, const bool v) const { buf[0] = v ? 1 : 0; }

  Prepare_param_handler::Param_list *m_params;
  Prepare_param_handler::Param_value_list *m_param_values;
};

class Any_to_json_param_handler : protected Any_to_param_handler {
 public:
  Any_to_json_param_handler(
      Prepare_param_handler::Param_list *params,
      Prepare_param_handler::Param_value_list *param_values,
      Prepare_param_handler::Param_svalue_list *string_values)
      : Any_to_param_handler(params, param_values),
        m_string_values(string_values) {}
  using Any_to_param_handler::operator();
  void operator()() { Any_to_param_handler::operator()(store_svalue("null")); }
  void operator()(const std::string &value) {
    Any_to_param_handler::operator()(store_svalue("\"" + value + "\""));
  }
  void operator()(const std::string &value, const uint32_t type) {
    Any_to_param_handler::operator()(
        store_svalue(type == Mysqlx::Resultset::JSON ? std::string(value)
                                                     : "\"" + value + "\""));
  }
  void operator()(const bool value) {
    Any_to_param_handler::operator()(store_svalue(value ? "true" : "false"));
  }

 protected:
  const std::string &store_svalue(std::string &&value) {
    return *m_string_values->emplace(m_string_values->end(), value);
  }

  Prepare_param_handler::Param_svalue_list *m_string_values;
};

}  // namespace

ngs::Error_code Prepare_param_handler::prepare_parameters(
    const Arg_list &args) {
  m_params.reserve(m_placeholders.size());
  m_param_values.reserve(m_placeholders.size());
  m_string_values.reserve(m_placeholders.size());
  Any_to_param_handler handler(&m_params, &m_param_values);
  Any_to_json_param_handler handler_json(&m_params, &m_param_values,
                                         &m_string_values);
  for (const auto &ph : m_placeholders) {
    const auto &arg = args.Get(ph.m_id);
    try {
      if (ph.m_type == Placeholder_info::Type::k_json)
        ngs::Getter_any::put_scalar_value_to_functor(arg, handler_json);
      else
        ngs::Getter_any::put_scalar_value_to_functor(arg, handler);
    } catch (const ngs::Error_code &) {
      return ngs::Error(ER_X_PREPARED_EXECUTE_ARGUMENT_NOT_SUPPORTED,
                        "Argument at index '%" PRIu32
                        "' and of type '%s' is not supported for binding"
                        " to prepared statement",
                        ph.m_id,
                        arg.has_scalar() ? arg.scalar().GetTypeName().c_str()
                                         : arg.GetTypeName().c_str());
    }
  }
  return ngs::Success();
}

ngs::Error_code Prepare_param_handler::check_argument_placeholder_consistency(
    const Arg_list::size_type args_size, const uint32_t args_offset) const {
  for (const auto &ph : m_placeholders)
    if (ph.m_id >= static_cast<Placeholder_list::value_type::Id>(args_size))
      return ngs::Error(ER_X_PREPARED_EXECUTE_ARGUMENT_CONSISTENCY,
                        "There is no argument for statement placeholder "
                        "at position: %" PRIu32,
                        (ph.m_id + args_offset));
  return ngs::Success();
}

}  // namespace xpl
