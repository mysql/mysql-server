/*
 * Copyright (c) 2018, 2026, Oracle and/or its affiliates.
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

#ifndef MYSQLSHDK_LIBS_DB_ROW_BY_NAME_H_
#define MYSQLSHDK_LIBS_DB_ROW_BY_NAME_H_

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "database/row.h"
#include "database/row_copy.h"
#include "mysqlrouter/jit_executor_db_interface.h"
#include "utils/utils_string.h"

namespace shcore {
namespace polyglot {
namespace database {

class Field_names {
 public:
  explicit Field_names(const std::vector<Column> &metadata) {
    for (const auto &col : metadata) add(col.get_column_label());
  }

  Field_names() = default;

  Field_names(const Field_names &) = delete;
  Field_names &operator=(const Field_names &) = delete;
  Field_names(Field_names &&) noexcept = default;
  Field_names &operator=(Field_names &&) noexcept = default;

  void add(const std::string &name) {
    if (has_field(name)) throw std::invalid_argument("duplicate field " + name);

    uint32_t idx = static_cast<uint32_t>(m_fields.size());
    m_fields[name] = idx;
  }

  uint32_t field_index(const std::string &name) {
    auto it = m_fields.find(name);
    if (it == m_fields.end())
      throw std::invalid_argument("invalid field name " + name);
    return it->second;
  }

  const std::string &field_name(uint32_t index) {
    for (const auto &f : m_fields) {
      if (f.second == index) return f.first;
    }
    throw std::invalid_argument("invalid field index " + std::to_string(index));
  }

  bool has_field(const std::string &field) const {
    return m_fields.find(field) != m_fields.end();
  }

 private:
  std::map<std::string, uint32_t, shcore::Case_insensitive_comparator> m_fields;
};

class Row_ref_by_name {
 public:
  Row_ref_by_name() = default;
  virtual ~Row_ref_by_name() = default;

  Row_ref_by_name(std::shared_ptr<Field_names> field_names,
                  const IRow *row) noexcept
      : _field_names(std::move(field_names)), _row_ref(row) {}

  Row_ref_by_name(const Row_ref_by_name &) = default;
  Row_ref_by_name &operator=(const Row_ref_by_name &) = default;
  Row_ref_by_name(Row_ref_by_name &&t) noexcept : Row_ref_by_name() {
    *this = std::move(t);
  }
  Row_ref_by_name &operator=(Row_ref_by_name &&t) noexcept {
    std::swap(_field_names, t._field_names);
    std::swap(_row_ref, t._row_ref);
    return *this;
  }

  const IRow &operator*() const { return *ref(); }

  explicit operator bool() const {
    return _field_names && (_row_ref != nullptr);
  }

  uint32_t num_fields() const { return ref()->num_fields(); }

  Type get_type(const std::string &field) const {
    try {
      return ref()->get_type(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  bool is_null(const std::string &field) const {
    try {
      return ref()->is_null(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  std::string get_as_string(const std::string &field) const {
    try {
      return ref()->get_as_string(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  std::string get_string(const std::string &field) const {
    try {
      return ref()->get_string(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  std::string get_string(const std::string &field,
                         const std::string &default_if_null) const {
    if (is_null(field)) return default_if_null;
    try {
      return ref()->get_string(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  std::wstring get_wstring(const std::string &field) const {
    try {
      return ref()->get_wstring(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  std::wstring get_wstring(const std::string &field,
                           const std::wstring &default_if_null) const {
    if (is_null(field)) return default_if_null;
    try {
      return ref()->get_wstring(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  int64_t get_int(const std::string &field) const {
    try {
      return ref()->get_int(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  int64_t get_int(const std::string &field, int64_t default_if_null) const {
    if (is_null(field)) return default_if_null;
    try {
      return ref()->get_int(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  uint64_t get_uint(const std::string &field) const {
    try {
      return ref()->get_uint(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  uint64_t get_uint(const std::string &field, uint64_t default_if_null) const {
    if (is_null(field)) return default_if_null;
    try {
      return ref()->get_uint(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  float get_float(const std::string &field) const {
    try {
      return ref()->get_float(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  double get_double(const std::string &field) const {
    try {
      return ref()->get_double(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  double get_double(const std::string &field, double default_if_null) const {
    if (is_null(field)) return default_if_null;
    try {
      return ref()->get_double(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  std::pair<const char *, size_t> get_string_data(
      const std::string &field) const {
    try {
      return ref()->get_string_data(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  std::tuple<uint64_t, int> get_bit(const std::string &field) const {
    try {
      return ref()->get_bit(field_index(field));
    } catch (const bad_field &e) {
      throw decorate_bad_field(field, e);
    }
  }

  bool has_field(const std::string &field) const {
    assert(_field_names);
    return _field_names->has_field(field);
  }

  uint32_t field_index(const std::string &field) const {
    assert(_field_names);
    return _field_names->field_index(field);
  }

  const std::string &field_name(uint32_t i) const {
    assert(_field_names);
    return _field_names->field_name(i);
  }

  std::shared_ptr<Field_names> field_names() const { return _field_names; }

  const IRow *ref() const {
    if (!_row_ref) throw std::invalid_argument("invalid row reference");
    return _row_ref;
  }

 protected:
  std::shared_ptr<Field_names> _field_names;
  const IRow *_row_ref = nullptr;

  static bad_field decorate_bad_field(const std::string &field,
                                      const bad_field &e) {
    std::string msg = e.what();
    msg.append(" (").append(field).append(")");
    return bad_field(msg.c_str(), e.field);
  }
};

class Row_by_name : public Row_ref_by_name {
 public:
  Row_by_name() : Row_ref_by_name(nullptr, &_row_copy) {}

  Row_by_name(const std::shared_ptr<Field_names> &field_names, const IRow &row)
      : Row_ref_by_name(field_names, &_row_copy), _row_copy(row) {}

  Row_by_name(const std::shared_ptr<Field_names> &field_names,
              Row_copy &&row_copy) noexcept
      : Row_ref_by_name(field_names, &_row_copy),
        _row_copy(std::move(row_copy)) {}

  Row_by_name(const Row_by_name &) = delete;
  Row_by_name &operator=(const Row_by_name &) = delete;

  Row_by_name(Row_by_name &&o) noexcept : Row_by_name() {
    *this = std::move(o);
  }
  Row_by_name &operator=(Row_by_name &&o) noexcept {
    if (this == &o) return *this;

    std::swap(static_cast<Row_ref_by_name &>(*this),
              static_cast<Row_ref_by_name &>(o));
    std::swap(this->_row_copy, o._row_copy);
    this->_row_ref = &_row_copy;
    o._row_ref = &o._row_copy;
    return *this;
  }

  explicit Row_by_name(const Row_ref_by_name &rrbn)
      : Row_ref_by_name(rrbn.field_names(), &_row_copy),
        _row_copy(rrbn.ref() ? Row_copy(*rrbn.ref()) : Row_copy()) {}

 private:
  Row_copy _row_copy;
};

}  // namespace database
}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_LIBS_DB_ROW_BY_NAME_H_
