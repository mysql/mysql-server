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

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef _CORELIBS_DB_COLUMN_H_
#define _CORELIBS_DB_COLUMN_H_
#include <cstdint>
#include <string>

#include "mysqlrouter/jit_executor_db_interface.h"

namespace shcore {
namespace polyglot {
namespace database {
/*
 * We require a common representation of the MySQL data MySQL data types
 * So it can be used no matter the source of the metadata information
 * (MySQL protocol or XProtocol)
 */
using jit_executor::db::Type;

std::string to_string(Type type);
Type string_to_type(const std::string &type);

inline bool is_string_type(Type type) {
  return (type == Type::Bytes || type == Type::Geometry || type == Type::Json ||
          type == Type::Date || type == Type::Time || type == Type::DateTime ||
          type == Type::Enum || type == Type::Set || type == Type::String ||
          type == Type::Vector);
}

inline bool is_binary_type(Type type) {
  return (type == Type::Bytes || type == Type::Geometry ||
          type == Type::Vector);
}

std::string type_to_dbstring(Type type, uint32_t length = 0);

Type dbstring_to_type(const std::string &data_type,
                      const std::string &column_type);

/**
 * These class represents a protocol independent Column
 * The Resultset implementation for each protocol should
 * make sure the metadata is represented on instances of this
 * class.
 *
 * This class aligns with the Column API as specified
 * on the DevAPI.
 */
class Column : public jit_executor::db::IColumn {
 public:
  Column(const std::string &catalog, const std::string &schema,
         const std::string &table_name, const std::string &table_label,
         const std::string &column_name, const std::string &column_label,
         uint32_t length, int frac_digits, Type type, uint32_t collation_id,
         bool unsigned_, bool zerofill, bool binary,
         const std::string &flags = "", const std::string &db_type = "");

  bool operator==(const Column &o) const {
    return _schema == o._schema && _table_name == o._table_name &&
           _table_label == o._table_label && _column_name == o._column_name &&
           _column_label == o._column_label &&
           _collation_id == o._collation_id && _length == o._length &&
           _fractional == o._fractional && _type == o._type &&
           _unsigned == o._unsigned && _zerofill == o._zerofill &&
           _binary == o._binary;
  }

  const std::string &get_catalog() const override { return _catalog; }
  const std::string &get_schema() const override { return _schema; }
  const std::string &get_table_name() const override { return _table_name; }
  const std::string &get_table_label() const override { return _table_label; }
  const std::string &get_column_name() const override { return _column_name; }
  const std::string &get_column_label() const override { return _column_label; }
  uint32_t get_length() const override { return _length; }
  int get_fractional() const override { return _fractional; }
  Type get_type() const override { return _type; }
  std::string get_dbtype() const override;
  // std::string get_collation_name() const;
  // std::string get_charset_name() const;
  uint32_t get_collation() const override { return _collation_id; }
  const std::string &get_flags() const override { return _flags; }

  bool is_unsigned() const override { return _unsigned; }
  bool is_zerofill() const override { return _zerofill; }
  bool is_binary() const override { return _binary; }
  bool is_numeric() const override;

  friend std::string to_string(const Column &c);

 private:
  std::string _catalog;
  std::string _schema;
  std::string _table_name;
  std::string _table_label;
  std::string _column_name;
  std::string _column_label;
  uint32_t _collation_id;
  uint32_t _length;
  int _fractional;
  Type _type;
  std::string _db_type;

  // Flags
  bool _unsigned;
  bool _zerofill;
  bool _binary;
  std::string _flags;
};
}  // namespace database
}  // namespace polyglot
}  // namespace shcore
#endif
