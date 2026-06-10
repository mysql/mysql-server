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

#include "database/column.h"

#include <cassert>
#include <sstream>
#include <stdexcept>

// #include "mysqlshdk/libs/db/charset.h"
#include "utils/utils_string.h"

namespace shcore {
namespace polyglot {
namespace database {

std::string to_string(Type type) {
  switch (type) {
    case Type::Null:
      return "Null";
    case Type::String:
      return "String";
    case Type::Integer:
      return "Integer";
    case Type::UInteger:
      return "UInteger";
    case Type::Float:
      return "Float";
    case Type::Double:
      return "Double";
    case Type::Decimal:
      return "Decimal";
    case Type::Bytes:
      return "Bytes";
    case Type::Geometry:
      return "Geometry";
    case Type::Json:
      return "Json";
    case Type::DateTime:
      return "DateTime";
    case Type::Date:
      return "Date";
    case Type::Time:
      return "Time";
    case Type::Bit:
      return "Bit";
    case Type::Enum:
      return "Enum";
    case Type::Set:
      return "Set";
    case Type::Vector:
      return "Vector";
  }
  throw std::logic_error("Unknown type");
}

Type string_to_type(const std::string &type) {
  if (type == "Null")
    return Type::Null;
  else if (type == "String")
    return Type::String;
  else if (type == "Integer")
    return Type::Integer;
  else if (type == "UInteger")
    return Type::UInteger;
  else if (type == "Float")
    return Type::Float;
  else if (type == "Double")
    return Type::Double;
  else if (type == "Decimal")
    return Type::Decimal;
  else if (type == "Bytes")
    return Type::Bytes;
  else if (type == "Geometry")
    return Type::Geometry;
  else if (type == "Json")
    return Type::Json;
  else if (type == "DateTime")
    return Type::DateTime;
  else if (type == "Date")
    return Type::Date;
  else if (type == "Time")
    return Type::Time;
  else if (type == "Bit")
    return Type::Bit;
  else if (type == "Enum")
    return Type::Enum;
  else if (type == "Set")
    return Type::Set;
  else if (type == "Vector")
    return Type::Vector;
  else
    throw std::logic_error("Unknown type " + type);
}

std::string type_to_dbstring(Type type, uint32_t length) {
  if (type == Type::Integer || type == Type::UInteger) {
    switch (length) {
      case 3:
      case 4:
        return "TinyInt";
      case 5:
      case 6:
        return "SmallInt";
      case 8:
      case 9:
        return "MediumInt";
      case 10:
      case 11:
        return "Int";
      case 20:
        return "BigInt";
    }
  }

  return to_string(type);
}

Type dbstring_to_type(const std::string &data_type,
                      const std::string &column_type) {
  if (shcore::str_iendswith(data_type, "geometry", "geomcollection",
                            "geometrycollection", "linestring", "point",
                            "polygon")) {
    return Type::Geometry;
  } else if (shcore::str_iendswith(data_type, "int")) {
    if (shcore::str_iendswith(column_type, " unsigned")) {
      return Type::UInteger;
    } else {
      return Type::Integer;
    }
  } else if (shcore::str_caseeq(data_type, "decimal")) {
    return Type::Decimal;
  } else if (shcore::str_caseeq(data_type, "double")) {
    return Type::Double;
  } else if (shcore::str_caseeq(data_type, "float")) {
    return Type::Float;
  } else if (shcore::str_caseeq(data_type, "date")) {
    return Type::Date;
  } else if (shcore::str_caseeq(data_type, "time")) {
    return Type::Time;
  } else if (shcore::str_caseeq(data_type, "timestamp", "datetime")) {
    return Type::DateTime;
  } else if (shcore::str_caseeq(data_type, "year")) {
    return Type::UInteger;
  } else if (shcore::str_iendswith(data_type, "blob",
                                   "binary")) {  // Includes varbinary data type
    return Type::Bytes;
  } else if (shcore::str_iendswith(data_type, "char",
                                   "text")) {  // Includes varchar data type
    return Type::String;
  } else if (shcore::str_caseeq(data_type, "bit")) {
    return Type::Bit;
  } else if (shcore::str_caseeq(data_type, "enum")) {
    return Type::Enum;
  } else if (shcore::str_caseeq(data_type, "set")) {
    return Type::Set;
  } else if (shcore::str_caseeq(data_type, "json")) {
    return Type::Json;
  } else if (shcore::str_caseeq(data_type, "vector")) {
    return Type::Vector;
  }

  throw std::logic_error("Unknown data_type: " + data_type +
                         " and column_type: " + column_type);
}

Column::Column(const std::string &catalog, const std::string &schema,
               const std::string &table_name, const std::string &table_label,
               const std::string &column_name, const std::string &column_label,
               uint32_t length, int fractional, Type type,
               uint32_t collation_id, bool unsigned_, bool zerofill,
               bool binary, const std::string &flags,
               const std::string &db_type)
    : _catalog(catalog),
      _schema(schema),
      _table_name(table_name),
      _table_label(table_label),
      _column_name(column_name),
      _column_label(column_label),
      _collation_id(collation_id),
      _length(length),
      _fractional(fractional),
      _type(type),
      _db_type(db_type),
      _unsigned(unsigned_),
      _zerofill(zerofill),
      _binary(binary),
      _flags(flags) {}

std::string Column::get_dbtype() const {
  if (_db_type.empty()) {
    switch (_type) {
      case Type::Bit:
        return "BIT";
      case Type::Integer:
      case Type::UInteger:
        switch (_length) {
          case 1:
          case 3:
          case 4:
            return "TINY";
          case 5:
          case 6:
            return "SHORT";
          case 8:
          case 9:
            return "INT24";
          case 10:
          case 11:
            return "LONG";
          case 20:
            return "LONGLONG";
          default:
            return "UNKNOWN INTEGER";
        }
        break;
      case Type::Float:
        return "FLOAT";
      case Type::Double:
        return "DOUBLE";
      case Type::Date:
        return "DATE";
      case Type::DateTime:
        if (_flags.find("TIMESTAMP") == std::string::npos) return "DATETIME";
        return "TIMESTAMP";
      case Type::Decimal:
        return "NEWDECIMAL";
      case Type::Geometry:
        return "GEOMETRY";
      case Type::Json:
        return "JSON";
      case Type::Set:
      case Type::Enum:
        return "STRING";
      case Type::Bytes:
      case Type::String:
        if (_flags.find("BLOB") != std::string::npos) return "BLOB";
        return "VAR_STRING";
      case Type::Null:
        return "NULL";
      case Type::Time:
        return "TIME";
      case Type::Vector:
        return "VECTOR";
      default:
        return "?-unknown-?";
    }
  }
  return _db_type;
}

// std::string Column::get_collation_name() const {
//   return charset::collation_name_from_collation_id(_collation_id);
// }

// std::string Column::get_charset_name() const {
//   return charset::charset_name_from_collation_id(_collation_id);
// }

bool Column::is_numeric() const {
  switch (_type) {
    case Type::Integer:
    case Type::UInteger:
    case Type::Float:
    case Type::Double:
    case Type::Decimal:
      return true;
    default:
      return false;
  }
}

std::string to_string(const Column &c) {
  std::stringstream ss;
  // uint32_t coll_id = c._collation_id == 0 ? 63 : c._collation_id;
  ss << "Name:      `" << c._column_label << "`\n";
  ss << "Org_name:  `" << c._column_name << "`\n";
  ss << "Catalog:   `" << c._catalog << "`\n";
  ss << "Database:  `" << c._schema << "`\n";
  ss << "Table:     `" << c._table_label << "`\n";
  ss << "Org_table: `" << c._table_name << "`\n";
  ss << "Type:      " << to_string(c._type) << "\n";
  ss << "DbType:    " << c.get_dbtype() << "\n";
  // ss << "Collation: " << charset::collation_name_from_collation_id(coll_id)
  // << " (" << coll_id << ")\n";
  ss << "Length:    " << c._length << "\n";
  ss << "Decimals:  " << c._fractional << "\n";
  ss << "Flags:     " << c._flags << "\n";

  return ss.str();
}

}  // namespace database
}  // namespace polyglot
}  // namespace shcore