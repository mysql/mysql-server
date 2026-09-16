#pragma once

/* Copyright (c) 2018, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation. The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include <cstdint>
#include <optional>
#include <type_traits>

#include <mysql/components/services/pfs_plugin_table_service.h>
#include <mysql/service_plugin_registry.h>

extern SERVICE_TYPE(pfs_plugin_table_v1) * pt_srv;
extern SERVICE_TYPE(pfs_plugin_column_integer_v1) * pc_integer_srv;
extern SERVICE_TYPE(pfs_plugin_column_bigint_v1) * pc_bigint_srv;
extern SERVICE_TYPE(pfs_plugin_column_string_v2) * pc_string_srv;

extern int init_plugin_table_service(SERVICE_TYPE(registry) * reg_srv);
extern void deinit_plugin_table_service(SERVICE_TYPE(registry) * reg_srv);

mysql_service_status_t tp_tables_init();
mysql_service_status_t tp_tables_deinit();

// Helper functions use the column services
namespace srv_utils {
// Note that the types used in the row tuple must match the table definition,
// not the actual type used in the code.
// Otherwise the wrong overload of may be chosen and a runtime column type
// assertion triggered.
// Note that even though pc_integer_srv->set_unsigned() takes an unsigned
// long value, you cannot use unsigned long for the type in the row tuple
// for an INT UNSIGNED column. This is becuase on most platforms unsigned
// long is 8 bytes and so overload resolution will choose a version of
// srv_utils::overloaded_set() which uses pc_bigint. And this will give assert
// when used with an INT UNSIGNED columnn

using PSI_int_NOT_NULL = std::int32_t;
using PSI_integer = std::optional<PSI_int_NOT_NULL>;
using PSI_int_unsigned_NOT_NULL = std::uint32_t;
using PSI_int_unsigned = std::optional<PSI_int_unsigned_NOT_NULL>;

using PSI_bigint_NOT_NULL = std::int64_t;
using PSI_bigint = std::optional<PSI_bigint_NOT_NULL>;
using PSI_bigint_unsigned_NOT_NULL = std::uint64_t;
using PSI_bigint_unsigned = std::optional<PSI_bigint_unsigned_NOT_NULL>;

/** Sets a string value in a field, using the appropriate service pointer,
    member function and PSI value.  */
inline void overloaded_set(PSI_field *f, const char *v) {
  pc_string_srv->set_varchar_utf8mb4(f, v);
}

/** Sets a integer value in a field, using the appropriate service pointer,
    member function and PSI value.  */
template <typename T>
inline void overloaded_set(PSI_field *f, std::optional<T> v) {
  static_assert(std::is_integral_v<T>);
  if constexpr (sizeof(T) == 8) {
    if constexpr (std::is_signed_v<T>) {
      pc_bigint_srv->set(f, {v.has_value() ? v.value() : 0, !v.has_value()});
    } else {
      pc_bigint_srv->set_unsigned(
          f, {v.has_value() ? v.value() : 0, !v.has_value()});
    }
  } else {
    if constexpr (std::is_signed_v<T>) {
      pc_integer_srv->set(f, {v.has_value() ? v.value() : 0, !v.has_value()});
    } else {
      pc_integer_srv->set_unsigned(
          f, {v.has_value() ? v.value() : 0, !v.has_value()});
    }
  }
}

/** Wraps non-optional integer values in an optional and forwards
    to the version taking an optional. */
template <typename T>
inline void overloaded_set(PSI_field *f, T t) {
  overloaded_set(f, std::make_optional(std::forward<T>(t)));
}
}  // namespace srv_utils
