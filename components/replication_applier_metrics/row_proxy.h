/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef ROW_PROXY_H
#define ROW_PROXY_H

#include <array>    // std::array
#include <cstring>  // std::strlen
#include "mysql/abi_helpers/abi_helpers.h"
#include "mysql/components/services/pfs_plugin_table_service.h"

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/pfs_plugin_table_service.h>

/// Framework that helps consuming data in a specific format, typically provided
/// by a service API, and publishes that data in the form of a
/// performance_schema table.
///
/// The format of a row is

/// Data types that this framework supports storing in a table.
enum Column_type {
  /// BIGINT, stored in the backend data as long long
  longlong_type,
  /// ENUM, stored in the backend data as long long
  enum_type,
  /// TIMESTAMP, stored in the backend data as microseconds in long long
  timestamp_type,
  /// CHAR(...), stored as a char *.
  string_type
};

/// Description of the mapping between a column and a field typecode.
///
/// @tparam Typecode_tp The enum type used for type codes.
/// @tparam typecode_end_tp Enum value whose value is greater than the largest
/// enum value that represents a field in a packet.
template <class Typecode_tp, Typecode_tp typecode_end_tp>
  requires requires { std::is_enum_v<Typecode_tp>; }
struct Field_view_definition {
  using Typecode_t = Typecode_tp;
  static constexpr Typecode_t typecode_end = typecode_end_tp;
  /// Enum value for the mysql::abi_helpers::Field object holding the value.
  Typecode_t m_field_typecode;
  /// Type of the field.
  Column_type m_type;
  /// Enum value for a mysql::abi_helpers::Field object that holds a bool
  /// indicating if the value is NULL.
  ///
  /// @note: this is only a remainder of a legacy protocol: new protocols should
  /// just omit m_field_typecode from the packet to indicate that the field is
  /// NULL, and their Field_view_definitions should leave this field with the
  /// default value.
  Typecode_t m_null_typecode{typecode_end_tp};
};

/// Description of the mapping between columns and field typecodes, for all
/// columns in a table.
template <class Typecode_tp, Typecode_tp typecode_end_tp,
          std::size_t column_count_tp>
using Row_view_definition =
    std::array<Field_view_definition<Typecode_tp, typecode_end_tp>,
               column_count_tp>;

/// Forward declaration.
template <class Typecode_tp, Typecode_tp typecode_end_tp,
          std::size_t column_count_tp>
  requires requires { std::is_enum_v<Typecode_tp>; }
class Row_proxy;

/// Aggregates type definitions and constants for a given typecode enum, end
/// element of that enum, and column count.
template <class Typecode_tp, Typecode_tp typecode_end_tp,
          std::size_t column_count_tp>
struct Row_proxy_type_info {
  using Typecode_t = Typecode_tp;
  static constexpr Typecode_t typecode_end = typecode_end_tp;
  static constexpr std::size_t column_count = column_count_tp;

  using Field_view_definition_t =
      Field_view_definition<Typecode_t, typecode_end>;
  using Row_view_definition_t =
      Row_view_definition<Typecode_t, typecode_end, column_count>;
  using Row_proxy_t = Row_proxy<Typecode_t, typecode_end, column_count>;

  using Field_t = mysql::abi_helpers::Field<Typecode_t>;
  using Row_t = mysql::abi_helpers::Packet<Typecode_t>;
  using Table_t = mysql::abi_helpers::Packet_array<Typecode_t>;
};

extern REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_string_v2);
extern REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_bigint_v1);
extern REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_timestamp_v2);
extern REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_enum_v1);

/// Fills values in fields in a table row, using an
/// Array_view<Field<Typecode_tp>> as data source (in namespace
/// mysql::abi_helpers), for a given enum type Typecode_tp.
///
/// This does not copy or allocate data; internally it only holds fixed-size
/// arrays that map column numbers to type codes in the enum type, and in turn
/// each type code to a field in the row. Using this information, when queried
/// with a column index and a PSI_field, it fills the PSI_field with the correct
/// value from the source data.
///
/// The object is meant to be reused for multiple queries to the same table. At
/// construction time, it creates the map associating column number with type
/// code, and for each row the user gives, it recreates the map from type codes
/// to field data.
///
/// @tparam Typecode_tp enum type that identifies each column, used in the
/// template argument of Field.
///
/// @tparam typecode_end_tp enum item whose numeric value is greater than the
/// largest enum value that represents a field in a packet.
///
/// @tparam column_count_tp The number of columns in the table.
template <class Typecode_tp, Typecode_tp typecode_end_tp,
          std::size_t column_count_tp>
  requires requires { std::is_enum_v<Typecode_tp>; }
class Row_proxy {
 public:
  using Type_info_t =
      Row_proxy_type_info<Typecode_tp, typecode_end_tp, column_count_tp>;

  /// Construct a new Row_proxy for rows in a table whose schema is described
  /// by the given Type_info_t::Row_view_definition_t.
  ///
  /// @param row_proxy_definition Description of table definition and packet
  /// layout. This is an std::array with @c column_count_tp elements, each
  /// representing a table column. Each element has three members: the typecode
  /// of the packet where the field value is found; the typecode of the packet
  /// holding a boolean that indicates whether the value is NULL; and the SQL
  /// data type. @see Field_view_definition .
  Row_proxy(
      const typename Type_info_t::Row_view_definition_t &row_proxy_definition)
      : m_row_proxy_definition(row_proxy_definition) {
    clear();
  }

  /// Make the proxy be a view over an "empty" row, which has NULL in all
  /// nullable fields and 0/"" in non-nullable fields.
  void clear() { memset(m_field_by_typecode, 0, sizeof(m_field_by_typecode)); }

  /// Make the proxy be a view over the given row.
  /// @param row Object holding values for all fields in this row.
  void set_row(const typename Type_info_t::Row_t &row) {
    clear();
    for (auto &field : row) {
      if (field.m_type >= 0 && field.m_type < Type_info_t::typecode_end) {
        m_field_by_typecode[field.m_type] = &field;
      }
    }
  }

  /// Copy the value of the given field of the current row to the target.
  /// @param index The column index.
  /// @param target Opaque pointer (PSI_field *) to the target field.
  void copy_field(int index, PSI_field *target) const {
    const auto &def = m_row_proxy_definition[index];
    bool is_null = false;
    bool is_empty = false;
    auto *field = m_field_by_typecode[def.m_field_typecode];
    // If there is a dedicated "null-ness" field, read that.
    // (This is a legacy protocol: new protocols should just leave out the
    // field to indicate that is it is NULL).
    if (def.m_null_typecode != Type_info_t::typecode_end) {
      auto *null_field = m_field_by_typecode[def.m_null_typecode];
      // If null-ness field is missing, don't force it to null.
      if (null_field != nullptr) {
        is_empty = is_null = null_field->m_data.m_bool;
      }
    }
    // If the field is missing, make it empty, and in case it is nullable, also
    // make it null.
    if (field == nullptr) {
      is_empty = true;
      if (def.m_null_typecode != Type_info_t::typecode_end) {
        is_null = true;
      }
    }
    switch (def.m_type) {
      /// If more types are needed, see e.g.
      /// plugin/replication_observers_example/src/binlog/service/iterator/tests/pfs.cc
      case string_type: {
        auto *value = is_empty ? "" : field->m_data.m_string;
        SERVICE_PLACEHOLDER(pfs_plugin_column_string_v2)
            ->set_char_utf8mb4(target, value, std::strlen(value));
        break;
      }
      case longlong_type: {
        auto value = is_empty ? 0 : field->m_data.m_int;
        SERVICE_PLACEHOLDER(pfs_plugin_column_bigint_v1)
            ->set(target, PSI_longlong{value, is_null});
        break;
      }
      case timestamp_type: {
        auto value = is_empty ? 0 : field->m_data.m_int;
        SERVICE_PLACEHOLDER(pfs_plugin_column_timestamp_v2)
            ->set2(target, value);
        break;
      }
      case enum_type: {
        unsigned long long value = is_empty ? 0 : field->m_data.m_int;
        SERVICE_PLACEHOLDER(pfs_plugin_column_enum_v1)
            ->set(target, PSI_ulonglong{value, is_null});
        break;
      }
    }
  }

 private:
  /// Array where the Nth entry is the definition of the Nth column.
  const typename Type_info_t::Row_view_definition_t &m_row_proxy_definition;
  /// Current map from m_type code to row value
  const typename Type_info_t::Field_t
      *m_field_by_typecode[Type_info_t::typecode_end];
};

#endif  // ifndef ROW_PROXY_H
