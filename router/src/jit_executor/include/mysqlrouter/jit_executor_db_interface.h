/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#ifndef ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_MYSQL_SESSION_H_
#define ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_MYSQL_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "mysqlrouter/jit_executor_plugin_export.h"
namespace jit_executor {
namespace db {

enum class Type {
  Null,
  String,
  Integer,
  UInteger,
  Float,
  Double,
  Decimal,
  Bytes,
  Geometry,
  Json,
  Date,
  Time,
  DateTime,
  Bit,
  Enum,
  Set,
  Vector
};

class JIT_EXECUTOR_PLUGIN_EXPORT IColumn {
 public:
  virtual const std::string &get_catalog() const = 0;
  virtual const std::string &get_schema() const = 0;
  virtual const std::string &get_table_name() const = 0;
  virtual const std::string &get_table_label() const = 0;
  virtual const std::string &get_column_name() const = 0;
  virtual const std::string &get_column_label() const = 0;
  virtual uint32_t get_length() const = 0;
  virtual int get_fractional() const = 0;
  virtual Type get_type() const = 0;
  virtual std::string get_dbtype() const = 0;
  // virtual std::string get_collation_name() const = 0;
  // virtual std::string get_charset_name() const = 0;
  virtual uint32_t get_collation() const = 0;
  virtual const std::string &get_flags() const = 0;

  virtual bool is_unsigned() const = 0;
  virtual bool is_zerofill() const = 0;
  virtual bool is_binary() const = 0;
  virtual bool is_numeric() const = 0;

  virtual ~IColumn() = default;
};

class JIT_EXECUTOR_PLUGIN_EXPORT IRow {
 public:
  IRow() = default;
  IRow(const IRow &other) = delete;
  IRow &operator=(const IRow &other) = delete;
  IRow(IRow &&other) = default;
  IRow &operator=(IRow &&other) = default;

  virtual uint32_t num_fields() const = 0;

  virtual Type get_type(uint32_t index) const = 0;
  virtual bool is_null(uint32_t index) const = 0;
  virtual std::string get_as_string(uint32_t index) const = 0;

  virtual std::string get_string(uint32_t index) const = 0;
  virtual std::wstring get_wstring(uint32_t index) const;
  virtual int64_t get_int(uint32_t index) const = 0;
  virtual uint64_t get_uint(uint32_t index) const = 0;
  virtual float get_float(uint32_t index) const = 0;
  virtual double get_double(uint32_t index) const = 0;
  virtual std::pair<const char *, size_t> get_string_data(
      uint32_t index) const = 0;
  virtual void get_raw_data(uint32_t index, const char **out_data,
                            size_t *out_size) const = 0;
  virtual std::tuple<uint64_t, int> get_bit(uint32_t index) const = 0;

  inline std::string get_as_string(uint32_t index,
                                   const std::string &default_if_null) const {
    if (is_null(index)) return default_if_null;
    return get_as_string(index);
  }

  inline std::string get_string(uint32_t index,
                                const std::string &default_if_null) const {
    if (is_null(index)) return default_if_null;
    return get_string(index);
  }

  inline std::wstring get_wstring(uint32_t index,
                                  const std::wstring &default_if_null) const {
    if (is_null(index)) return default_if_null;
    return get_wstring(index);
  }

  inline int64_t get_int(uint32_t index, int64_t default_if_null) const {
    if (is_null(index)) return default_if_null;
    return get_int(index);
  }

  inline uint64_t get_uint(uint32_t index, uint64_t default_if_null) const {
    if (is_null(index)) return default_if_null;
    return get_uint(index);
  }

  inline double get_double(uint32_t index, double default_if_null) const {
    if (is_null(index)) return default_if_null;
    return get_double(index);
  }

  virtual ~IRow() = default;
};

struct JIT_EXECUTOR_PLUGIN_EXPORT Warning {
  enum class Level { Note, Warn, Error };
  Level level;
  std::string msg;
  uint32_t code;
};

class JIT_EXECUTOR_PLUGIN_EXPORT IResult {
 public:
  IResult() = default;
  double get_execution_time() const { return m_execution_time; }
  void set_execution_time(double time) { m_execution_time = time; }

  virtual const IRow *fetch_one() = 0;
  virtual bool next_resultset() = 0;
  virtual std::unique_ptr<Warning> fetch_one_warning() = 0;

  // Metadata retrieval
  virtual int64_t get_auto_increment_value() const = 0;
  virtual bool has_resultset() = 0;

  virtual uint64_t get_affected_row_count() const = 0;
  virtual uint64_t get_fetched_row_count() const = 0;

  // In case of classic result this will only return real value after fetching
  // the result data
  virtual uint64_t get_warning_count() const = 0;
  virtual std::string get_info() const = 0;
  virtual const std::vector<std::string> &get_gtids() const = 0;

  virtual const std::vector<std::shared_ptr<IColumn>> &get_metadata() const = 0;
  virtual std::string get_statement_id() const { return ""; }

  virtual void buffer() = 0;
  virtual void rewind() = 0;

  virtual ~IResult() = default;

 protected:
  double m_execution_time = 0.0;
};

class JIT_EXECUTOR_PLUGIN_EXPORT ISession {
 public:
  virtual std::shared_ptr<IResult> run_sql(const std::string &sql) = 0;

  virtual void reset() = 0;

  virtual ~ISession() = default;
};

}  // namespace db
}  // namespace jit_executor

#endif  // ROUTER_SRC_JIT_EXECUTOR_INCLUDE_MYSQLROUTER_JIT_EXECUTOR_MYSQL_SESSION_H_