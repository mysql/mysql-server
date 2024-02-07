/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/tests/driver/connector/result_fetcher.h"

#include <set>
#include <stdexcept>
#include <vector>

#include "plugin/x/client/mysqlxclient/xrow.h"
#include "plugin/x/src/ngs/protocol/protocol_const.h"

namespace {

inline std::string get_typename(const xcl::Column_type &field) {
  switch (field) {
    case xcl::Column_type::SINT:
      return "SINT";
    case xcl::Column_type::UINT:
      return "UINT";
    case xcl::Column_type::DOUBLE:
      return "DOUBLE";
    case xcl::Column_type::FLOAT:
      return "FLOAT";
    case xcl::Column_type::BYTES:
      return "BYTES";
    case xcl::Column_type::TIME:
      return "TIME";
    case xcl::Column_type::DATETIME:
      return "DATETIME";
    case xcl::Column_type::SET:
      return "SET";
    case xcl::Column_type::ENUM:
      return "ENUM";
    case xcl::Column_type::BIT:
      return "BIT";
    case xcl::Column_type::DECIMAL:
      return "DECIMAL";
  }
  return "UNKNOWN";
}

inline std::string get_flags(const xcl::Column_type &field, uint32_t flags) {
  std::string r;

  if (flags & MYSQLX_COLUMN_FLAGS_UINT_ZEROFILL) {  // and other equal 1
    switch (field) {
      case xcl::Column_type::SINT:
      case xcl::Column_type::UINT:
        r += " ZEROFILL";
        break;

      case xcl::Column_type::DOUBLE:
      case xcl::Column_type::FLOAT:
      case xcl::Column_type::DECIMAL:
        r += " UNSIGNED";
        break;

      case xcl::Column_type::BYTES:
        r += " RIGHTPAD";
        break;

      case xcl::Column_type::DATETIME:
        r += " TIMESTAMP";
        break;

      default: {
      }
    }
  }
  if (flags & MYSQLX_COLUMN_FLAGS_NOT_NULL) r += " NOT_NULL";

  if (flags & MYSQLX_COLUMN_FLAGS_PRIMARY_KEY) r += " PRIMARY_KEY";

  if (flags & MYSQLX_COLUMN_FLAGS_UNIQUE_KEY) r += " UNIQUE_KEY";

  if (flags & MYSQLX_COLUMN_FLAGS_MULTIPLE_KEY) r += " MULTIPLE_KEY";

  if (flags & MYSQLX_COLUMN_FLAGS_AUTO_INCREMENT) r += " AUTO_INCREMENT";

  return r;
}

}  // namespace

std::ostream &operator<<(std::ostream &os, const xcl::Column_metadata &meta) {
  os << meta.name << ":" << get_typename(meta.type) << ':'
     << get_flags(meta.type, meta.flags);
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const std::vector<xcl::Column_metadata> &meta) {
  for (const xcl::Column_metadata &col : meta) {
    os << col.name << ":" << get_typename(col.type) << ':'
       << get_flags(col.type, col.flags) << '\n';
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, Result_fetcher *result) {
  std::vector<xcl::Column_metadata> meta(result->column_metadata());

  if (result->get_last_error()) return os;

  for (std::size_t col = 0; col < meta.size(); ++col) {
    if (col != 0) os << "\t";
    os << meta[col].name;
  }

  os << "\n";

  std::string out_data;

  while (const xcl::XRow *row = result->next()) {
    for (int field = 0; field < row->get_number_of_fields(); ++field) {
      if (field != 0) os << "\t";

      if (!row->get_field_as_string(field, &out_data))
        throw std::runtime_error("Data decoder failed");

      os << out_data;
    }

    os << "\n";
  }
  return os;
}
