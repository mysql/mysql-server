/*
 * Copyright (c) 2017, 2026, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "database/row_copy.h"

#include <bitset>
#include <cassert>
#include <climits>  // C limit constants
#include <cmath>    // HUGE_VAL
#include <limits>   // std::numeric_limits
#include <memory>
#include <stdexcept>
#include <utility>

#include "utils/utils_string.h"

namespace shcore {
namespace polyglot {
namespace database {

#define FIELD_ERROR(index, msg) \
  std::invalid_argument(        \
      shcore::str_format("%s(%u): " msg, __FUNCTION__, index).c_str())

#define FIELD_ERROR1(index, msg, arg) \
  std::invalid_argument(              \
      shcore::str_format("%s(%u): " msg, __FUNCTION__, index, arg).c_str())

#define VALIDATE_INDEX(index)                          \
  do {                                                 \
    if (index >= num_fields())                         \
      throw FIELD_ERROR(index, "index out of bounds"); \
  } while (0)

#define GET_VALIDATE_TYPE(index, TYPE_CHECK)                                  \
  if (index >= num_fields()) throw FIELD_ERROR(index, "index out of bounds"); \
  if (_data->fields[index] == nullptr)                                        \
    throw FIELD_ERROR(index, "field is NULL");                                \
  ftype = get_type(index);                                                    \
  if (!(TYPE_CHECK))                                                          \
    throw FIELD_ERROR1(index, "field type is %s", to_string(ftype).c_str());

Row_copy::Row_copy(const IRow &row) {
  _data = std::make_shared<Data>();
  _data->types.reserve(row.num_fields());
  _data->fields.reserve(row.num_fields());

  for (uint32_t c = row.num_fields(), i = 0; i < c; i++) {
    _data->types.emplace_back(row.get_type(i));

    if (row.is_null(i)) {
      _data->fields.emplace_back(nullptr);
      continue;
    }

    switch (row.get_type(i)) {
      case Type::Null:
        _data->fields.emplace_back(nullptr);
        break;

      case Type::Decimal:
        _data->fields.emplace_back(
            std::make_unique<Field_data<std::string>>(row.get_as_string(i)));
        break;

      case Type::Date:
      case Type::DateTime:
      case Type::Time:
      case Type::Geometry:
      case Type::Json:
      case Type::Vector:
      case Type::Enum:
      case Type::Set:
        _data->fields.emplace_back(
            std::make_unique<Field_data<std::string>>(row.get_string(i)));
        break;

      case Type::String:
        _data->fields.emplace_back(
            std::make_unique<Field_data<std::string>>(row.get_string(i)));
        break;

      case Type::Bytes:
        _data->fields.emplace_back(
            std::make_unique<Field_data<std::string>>(row.get_string(i)));
        break;

      case Type::Integer:
        _data->fields.emplace_back(
            std::make_unique<Field_data<int64_t>>(row.get_int(i)));
        break;

      case Type::UInteger:
        _data->fields.emplace_back(
            std::make_unique<Field_data<uint64_t>>(row.get_uint(i)));
        break;

      case Type::Float:
        _data->fields.emplace_back(
            std::make_unique<Field_data<float>>(row.get_float(i)));
        break;

      case Type::Double:
        _data->fields.emplace_back(
            std::make_unique<Field_data<double>>(row.get_double(i)));
        break;

      case Type::Bit:
        _data->fields.emplace_back(
            std::make_unique<Field_data<std::string>>(row.get_as_string(i)));
        break;
    }
  }
}

Type Mem_row::get_type(uint32_t index) const {
  VALIDATE_INDEX(index);
  return _data->types[index];
}

uint32_t Mem_row::num_fields() const {
  return static_cast<uint32_t>(_data->types.size());
}

std::string Mem_row::get_as_string(uint32_t index) const {
  VALIDATE_INDEX(index);

  if (is_null(index)) return "NULL";

  switch (get_type(index)) {
    case Type::Null:
      return "NULL";

    case Type::String:
    case Type::Bytes:
      return get<std::string>(index);

    case Type::Decimal:
    case Type::Date:
    case Type::DateTime:
    case Type::Time:
    case Type::Geometry:
    case Type::Vector:
    case Type::Json:
    case Type::Enum:
    case Type::Set:
      return get<std::string>(index);

    case Type::Integer:
      return std::to_string(get<int64_t>(index));

    case Type::UInteger:
      return std::to_string(get<uint64_t>(index));

    case Type::Float:
      return std::to_string(get<float>(index));

    case Type::Double:
      return std::to_string(get<double>(index));

    case Type::Bit:
      return get<std::string>(index);
  }
  throw std::invalid_argument("Unknown type in field");
}

int64_t Mem_row::get_int(uint32_t index) const {
  Type ftype;
  std::string dec;
  GET_VALIDATE_TYPE(index, (ftype == Type::Integer || ftype == Type::UInteger ||
                            (ftype == Type::Decimal &&
                             (dec = get<std::string>(index)).find('.') ==
                                 std::string::npos)));

  if (ftype == Type::UInteger) {
    uint64_t u = get<uint64_t>(index);
    if (u > LLONG_MAX) {
      throw FIELD_ERROR(index, "field value out of the allowed range");
    }
    return static_cast<int64_t>(u);
  } else if (ftype == Type::Decimal) {
    return std::stoll(dec);
  }
  return get<int64_t>(index);
}

uint64_t Mem_row::get_uint(uint32_t index) const {
  Type ftype;
  std::string dec;
  GET_VALIDATE_TYPE(index, (ftype == Type::Integer || ftype == Type::UInteger ||
                            (ftype == Type::Decimal &&
                             (dec = get<std::string>(index)).find('.') ==
                                 std::string::npos)));

  if (ftype == Type::Integer) {
    int64_t i = get<int64_t>(index);
    if (i < 0) {
      throw FIELD_ERROR(index, "field value out of the allowed range");
    }
    return static_cast<uint64_t>(i);
  } else if (ftype == Type::Decimal) {
    if (!dec.empty() && dec[0] == '-') {
      throw FIELD_ERROR(index, "field value out of the allowed range");
    }
    return std::stoull(dec);
  }
  return get<uint64_t>(index);
}

std::string Mem_row::get_string(uint32_t index) const {
  Type ftype;
  GET_VALIDATE_TYPE(index, (is_string_type(ftype)));
  return get<std::string>(index);
}

std::pair<const char *, size_t> Mem_row::get_string_data(uint32_t index) const {
  Type ftype;
  GET_VALIDATE_TYPE(index, (is_string_type(ftype)));
  const auto &s =
      static_cast<Field_data<std::string> *>(_data->fields[index].get())->value;
  return {s.data(), s.size()};
}

void Mem_row::get_raw_data(uint32_t index, const char **out_data,
                           size_t *out_size) const {
  if (is_null(index)) {
    *out_data = nullptr;
    *out_size = 0;
  } else {
    m_raw_data_cache = get_as_string(index);
    *out_data = m_raw_data_cache.c_str();
    *out_size = m_raw_data_cache.length();
  }
}

float Mem_row::get_float(uint32_t index) const {
  Type ftype;
  GET_VALIDATE_TYPE(index, (ftype == Type::Float || ftype == Type::Decimal ||
                            ftype == Type::Double));
  switch (ftype) {
    case Type::Decimal:
      try {
        return std::stof(get<std::string>(index));
      } catch (...) {
        throw FIELD_ERROR(index, "float value out of the allowed range");
      }
    case Type::Double:
      return static_cast<float>(get<double>(index));
    case Type::Float:
      return get<float>(index);
    default:
      throw std::logic_error("internal error");
  }
}

double Mem_row::get_double(uint32_t index) const {
  Type ftype;
  GET_VALIDATE_TYPE(index, (ftype == Type::Double || ftype == Type::Float ||
                            ftype == Type::Decimal));
  switch (ftype) {
    case Type::Decimal:
      try {
        return std::stod(get<std::string>(index));
      } catch (const std::exception &) {
        throw FIELD_ERROR(index, "double value out of the allowed range");
      }
    case Type::Float:
      return static_cast<double>(get<float>(index));
    case Type::Double:
      return get<double>(index);
    default:
      throw std::logic_error("internal error");
  }
}

std::tuple<uint64_t, int> Mem_row::get_bit(uint32_t index) const {
  Type ftype;
  GET_VALIDATE_TYPE(index, (ftype == Type::Bit));
  return shcore::string_to_bits(get<std::string>(index));
}

bool Mem_row::is_null(uint32_t index) const {
  VALIDATE_INDEX(index);
  return _data->fields[index] == nullptr;
}

void Mem_row::add_field(Type type, uint32_t offset) {
  if (offset > _data->types.size())
    throw std::invalid_argument("Attempt to insert column past row size");

  _data->types.insert(_data->types.begin() + offset, type);
  _data->fields.insert(_data->fields.begin() + offset, nullptr);
}

void Mem_row::add_field(Type type) {
  _data->types.push_back(type);
  _data->fields.push_back(nullptr);
}

}  // namespace database
}  // namespace polyglot
}  // namespace shcore
