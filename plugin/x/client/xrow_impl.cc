/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include "plugin/x/client/xrow_impl.h"

#include <cassert>
#include <cstdint>
#include <utility>

#include "my_compiler.h"
#include "mysql/strings/dtoa.h"

namespace xcl {
namespace details {

std::string as_string(const Column_metadata &m [[maybe_unused]],
                      const std::set<std::string> &values) {
  std::string result;
  bool first = true;

  for (const auto &value : values) {
    if (!first) {
      result += ",";
    }
    result += value;
    first = false;
  }

  return result;
}

std::string as_string(const Column_metadata &m [[maybe_unused]],
                      const std::string &value) {
  return value;
}

std::string as_string(const Column_metadata &m [[maybe_unused]],
                      const Time &value) {
  return value.to_string();
}

std::string as_string(const Column_metadata &m [[maybe_unused]],
                      const DateTime &value) {
  return value.to_string();
}

std::string as_string(const Column_metadata &m [[maybe_unused]],
                      const Decimal &value) {
  return value.to_string();
}

// 'my_gcvt()' function doesn't have overload for float,
// thus 'value' argument must be double (version of this
// method that takes float as 'value' is not needed)
std::string floating_point_as_string(const Column_metadata &m,
                                     const my_gcvt_arg_type arg_type,
                                     const double &value) {
  char buffer[100];

  if (m.fractional_digits < DECIMAL_NOT_SPECIFIED) {
    my_fcvt(value, m.fractional_digits, buffer, nullptr);

    return buffer;
  }

  my_gcvt(value, arg_type, sizeof(buffer) - 1, buffer, nullptr);

  return buffer;
}

std::string as_string(const Column_metadata &metadata, const double &value) {
  return floating_point_as_string(metadata, MY_GCVT_ARG_DOUBLE, value);
}

std::string as_string(const Column_metadata &metadata, const float &value) {
  return floating_point_as_string(metadata, MY_GCVT_ARG_FLOAT, value);
}

template <typename Value_type>
std::string as_string(const Column_metadata &m [[maybe_unused]],
                      const Value_type &value) {
  return std::to_string(value);
}

}  // namespace details

XRow_impl::XRow_impl(Metadata *metadata, Context *context)
    : m_metadata(metadata), m_context(context) {}

int32_t XRow_impl::get_number_of_fields() const {
  return static_cast<int32_t>(m_row->field_size());
}

bool XRow_impl::is_null(const int32_t field_index) const {
  return 0 == m_row->field(field_index).size();
}

bool XRow_impl::get_int64(const int32_t field_index, int64_t *out_data) const {
  if (m_metadata->empty()) return false;
  if (Column_type::SINT != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);

  return row_decoder::buffer_to_s64(field, out_data);
}

bool XRow_impl::get_uint64(const int32_t field_index,
                           uint64_t *out_data) const {
  if (m_metadata->empty()) return false;
  if (Column_type::UINT != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);

  return row_decoder::buffer_to_u64(field, out_data);
}

bool XRow_impl::get_double(const int32_t field_index, double *out_data) const {
  if (m_metadata->empty()) return false;
  if (Column_type::DOUBLE != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);

  return row_decoder::buffer_to_double(field, out_data);
}

bool XRow_impl::get_float(const int32_t field_index, float *out_data) const {
  if (m_metadata->empty()) return false;
  if (Column_type::FLOAT != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);

  return row_decoder::buffer_to_float(field, out_data);
}

bool XRow_impl::get_string(const int32_t field_index,
                           std::string *out_data) const {
  const char *string_data;
  size_t string_size;

  if (!get_string(field_index, &string_data, &string_size)) return false;

  *out_data = std::string(string_data, string_size);

  return true;
}

bool XRow_impl::get_enum(const int32_t field_index,
                         std::string *out_data) const {
  const char *string_data;
  size_t string_size;

  if (!get_enum(field_index, &string_data, &string_size)) return false;

  *out_data = std::string(string_data, string_size);

  return true;
}

bool XRow_impl::get_enum(const int32_t field_index, const char **out_data,
                         size_t *out_data_length) const {
  return get_string_based_field(Column_type::ENUM, field_index, out_data,
                                out_data_length);
}

bool XRow_impl::get_decimal(const int32_t field_index,
                            Decimal *out_data) const {
  if (m_metadata->empty()) return false;
  if (Column_type::DECIMAL != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);

  row_decoder::buffer_to_decimal(field, out_data);

  if (out_data) {
    return out_data->is_valid();
  }

  return true;
}

bool XRow_impl::get_string(const int32_t field_index, const char **out_data,
                           size_t *out_data_length) const {
  return get_string_based_field(Column_type::BYTES, field_index, out_data,
                                out_data_length);
}

bool XRow_impl::get_time(const int32_t field_index, Time *out_data) const {
  if (m_metadata->empty()) return false;
  if (Column_type::TIME != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);

  return row_decoder::buffer_to_time(field, out_data);
}

bool XRow_impl::get_datetime(const int32_t field_index,
                             DateTime *out_data) const {
  if (m_metadata->empty()) return false;
  if (Column_type::DATETIME != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);

  bool has_time = false;
  // Metadata does not contain content_type, the only way to determine
  // if there is a time part is to look at the fields length.
  if (!(*m_metadata)[field_index].has_content_type) {
    if ((*m_metadata)[field_index].length >
        m_context->m_datetime_length_discriminator)
      has_time = true;
  } else if ((*m_metadata)[field_index].content_type ==
             static_cast<uint32_t>(Mysqlx::Resultset::DATETIME)) {
    has_time = true;
  }

  return row_decoder::buffer_to_datetime(field, out_data, has_time);
}

bool XRow_impl::get_set(const int32_t field_index,
                        std::set<std::string> *out_data) const {
  if (m_metadata->empty()) return false;
  if (Column_type::SET != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);

  return row_decoder::buffer_to_set(field, out_data);
}

bool XRow_impl::get_bit(const int32_t field_index, bool *out_data) const {
  if (m_metadata->empty()) return false;
  if (Column_type::BIT != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);
  uint64_t value;

  if (!row_decoder::buffer_to_u64(field, &value)) return false;

  *out_data = 0 != value;

  return true;
}

bool XRow_impl::get_bit(const int32_t field_index, uint64_t *out_data) const {
  if (m_metadata->empty()) return false;
  if (Column_type::BIT != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);

  return row_decoder::buffer_to_u64(field, out_data);
}

bool XRow_impl::get_field_as_string(const int32_t field_index,
                                    std::string *out_data) const {
  if (m_metadata->empty()) return false;
  const auto &col = (*m_metadata)[field_index];

  if (is_null(field_index)) {
    if (out_data) *out_data = "null";

    return true;
  }

  switch (col.type) {
    case xcl::Column_type::SINT: {
      int64_t value;

      if (!get_int64(field_index, &value)) return false;

      if (out_data) *out_data = details::as_string(col, value);

      return true;
    }

    case xcl::Column_type::UINT: {
      uint64_t value;

      if (!get_uint64(field_index, &value)) return false;

      if (out_data) *out_data = details::as_string(col, value);

      return true;
    }

    case xcl::Column_type::DOUBLE: {
      double value;

      if (!get_double(field_index, &value)) return false;

      if (out_data) *out_data = details::as_string(col, value);

      return true;
    }

    case xcl::Column_type::FLOAT: {
      float value;

      if (!get_float(field_index, &value)) return false;

      if (out_data) *out_data = details::as_string(col, value);

      return true;
    }

    case xcl::Column_type::BYTES: {
      return get_string(field_index, out_data);
    }

    case xcl::Column_type::TIME: {
      Time value;

      if (!get_time(field_index, &value)) return false;

      if (out_data) *out_data = details::as_string(col, value);

      return true;
    }

    case xcl::Column_type::DATETIME: {
      DateTime value;

      if (!get_datetime(field_index, &value)) return false;

      if (out_data) *out_data = details::as_string(col, value);

      return true;
    }

    case xcl::Column_type::DECIMAL: {
      Decimal value;

      if (!get_decimal(field_index, &value)) return false;

      if (out_data) *out_data = details::as_string(col, value);

      return true;
    }

    case xcl::Column_type::SET: {
      String_set value;

      if (!get_set(field_index, &value)) return false;

      if (out_data) *out_data = details::as_string(col, value);

      return true;
    }

    case xcl::Column_type::ENUM: {
      return get_enum(field_index, out_data);
    }

    case xcl::Column_type::BIT: {
      uint64_t value;

      if (!get_bit(field_index, &value)) return false;

      if (out_data) *out_data = details::as_string(col, value);

      return true;
    }
  }

  return false;
}

bool XRow_impl::valid() const { return nullptr != m_row.get(); }

void XRow_impl::clean() { m_row.reset(); }

void XRow_impl::set_row(std::unique_ptr<Row> &&row) { m_row = std::move(row); }

bool XRow_impl::get_string_based_field(const Column_type expected_type,
                                       const int32_t field_index,
                                       const char **out_data,
                                       size_t *out_data_length) const {
  if (m_metadata->empty()) return false;
  if (expected_type != (*m_metadata)[field_index].type) return false;

  const std::string &field = m_row->field(field_index);

  return row_decoder::buffer_to_string(field, out_data, out_data_length);
}

}  // namespace xcl
