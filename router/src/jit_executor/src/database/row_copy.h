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

// Persistent copy of a Row object

#ifndef MYSQLSHDK_LIBS_DB_ROW_COPY_H_
#define MYSQLSHDK_LIBS_DB_ROW_COPY_H_

#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "database/column.h"
#include "database/row.h"
#include "mysqlrouter/jit_executor_db_interface.h"

namespace shcore {
namespace polyglot {
namespace database {
using jit_executor::db::IRow;

class Mem_row : public IRow {
 public:
  Mem_row() = default;
  Mem_row(const Mem_row &) = delete;
  Mem_row &operator=(const Mem_row &) = delete;
  Mem_row(Mem_row &&) = default;
  Mem_row &operator=(Mem_row &&) = default;
  ~Mem_row() override = default;

  uint32_t num_fields() const override;

  Type get_type(uint32_t index) const override;
  bool is_null(uint32_t index) const override;
  std::string get_as_string(uint32_t index) const override;

  std::string get_string(uint32_t index) const override;
  int64_t get_int(uint32_t index) const override;
  uint64_t get_uint(uint32_t index) const override;
  float get_float(uint32_t index) const override;
  double get_double(uint32_t index) const override;
  std::pair<const char *, size_t> get_string_data(
      uint32_t index) const override;
  void get_raw_data(uint32_t index, const char **out_data,
                    size_t *out_size) const override;
  std::tuple<uint64_t, int> get_bit(uint32_t index) const override;

  /** Inserts new field at specified offset. Field value is set to null.
   *
   * @param type field type.
   * @param offset offset at which to insert (default - last column).
   */
  void add_field(Type type, uint32_t offset);
  void add_field(Type type);

 protected:
  struct Field_data_ {
    virtual ~Field_data_() = default;
  };

  template <typename T>
  struct Field_data : public Field_data_ {
    explicit Field_data(const T &v) : value(v) {}
    explicit Field_data(T &&v) noexcept : value(std::move(v)) {}
    T value;
  };

  template <typename T>
  const T &get(size_t field) const {
    assert(field < _data->fields.size());
    if (field >= _data->fields.size())
      throw std::invalid_argument("Attempt to access invalid field");
    return static_cast<Field_data<T> *>(_data->fields[field].get())->value;
  }

  struct Data {
    std::vector<Type> types;
    std::vector<std::unique_ptr<Field_data_>> fields;

    Data() = default;
    explicit Data(std::vector<Type> types_)
        : types(std::move(types_)), fields(types.size()) {}

    explicit Data(const std::vector<Column> &columns) : fields(columns.size()) {
      for (const auto &c : columns) types.push_back(c.get_type());
    }
  };
  std::shared_ptr<Data> _data;
  mutable std::string m_raw_data_cache;
};

/**
 * A self-contained Row object that owns its own storage, as opposed to
 * mysql::Row or mysqlx::Row which are references to data owned by the
 * underlying client library.
 *
 * Can be created from the copy-constructor, from any instance of IRow.
 */
class Row_copy : public Mem_row {
 public:
  Row_copy() = default;
  virtual ~Row_copy() = default;

  Row_copy(const Row_copy &row) = delete;
  Row_copy &operator=(const Row_copy &row) = delete;
  Row_copy(Row_copy &&) = default;
  Row_copy &operator=(Row_copy &&) = default;

  explicit Row_copy(const IRow &row);
};

/**
 * A self-contained Row object like Row_copy, but can be created or modified
 * programmatically.
 */
class Mutable_row : public Mem_row {
 public:
  template <class T>
  typename std::enable_if<std::is_integral<T>::value>::type set_field(
      uint32_t index, T &&arg) {
    if (_data->types[index] == Type::Integer)
      _data->fields[index] = std::unique_ptr<Field_data_>(
          new Field_data<int64_t>(std::forward<T>(arg)));
    else if (_data->types[index] == Type::UInteger)
      _data->fields[index] = std::unique_ptr<Field_data_>(
          new Field_data<uint64_t>(std::forward<T>(arg)));
    else
      throw std::invalid_argument(
          "Attempt to write integer value to non integer field");
  }

  template <class T>
  typename std::enable_if<std::is_floating_point<T>::value>::type set_field(
      uint32_t index, T &&arg) {
    if (_data->types[index] != Type::Float &&
        _data->types[index] != Type::Double)
      throw std::invalid_argument(
          "Attempt to write floating point number to not neither float or "
          "double field.");
    _data->fields[index] =
        std::make_unique<Field_data<T>>(std::forward<T>(arg));
  }

  template <class T>
  typename std::enable_if<std::is_same<T, std::nullptr_t>::value>::type
  set_field(uint32_t index, T && /*arg*/) {
    _data->fields[index] = nullptr;
  }

  template <class T>
  typename std::enable_if<!std::is_arithmetic<T>::value &&
                          !std::is_same<T, std::nullptr_t>::value>::type
  set_field(uint32_t index, T &&arg) {
    if (_data->types[index] >= Type::Integer &&
        _data->types[index] <= Type::Double)
      throw std::invalid_argument(
          "Attempt to write arithmetic type to non arithmetic field");
    _data->fields[index] = std::unique_ptr<Field_data_>(
        new Field_data<std::string>(std::string(std::forward<T>(arg))));
  }

 private:
  template <class T, class... Args>
  void set_field(uint32_t start, T &&value, Args... args) {
    set_field(start, std::forward<T>(value));
    if (start < _data->fields.size())
      set_field(start + 1, std::forward<Args>(args)...);
  }

 public:
  template <class... Args>
  void set_row_values(Args... args) {
    constexpr int n = sizeof...(args);
    if (n != _data->fields.size())
      throw std::invalid_argument(
          "The number of arguments does not match the row size");
    set_field(0, std::forward<Args>(args)...);
  }

  explicit Mutable_row(const std::vector<Type> &types) {
    _data = std::make_shared<Data>(types);
  }

  explicit Mutable_row(const std::vector<Column> &columns) {
    _data = std::make_shared<Data>(columns);  // only types used
  }

  template <class... Args>
  Mutable_row(const std::vector<Type> &types, Args... args) {
    _data = std::make_shared<Data>(types);
    set_row_values(std::forward<Args>(args)...);
  }

  template <class... Args>
  Mutable_row(const std::vector<Column> &columns, Args... args) {
    _data = std::make_shared<Data>(columns);
    set_row_values(std::forward<Args>(args)...);
  }

  virtual ~Mutable_row() = default;
};

}  // namespace database
}  // namespace polyglot
}  // namespace shcore
#endif  // MYSQLSHDK_LIBS_DB_ROW_COPY_H_
