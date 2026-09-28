/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef COMPONENTS_SERVICES_BITS_MYSQL_USER_DEFINED_TYPE_BITS_H
#define COMPONENTS_SERVICES_BITS_MYSQL_USER_DEFINED_TYPE_BITS_H

#include <stdint.h>
#include <cstddef>

#include "mysql/components/services/bits/mysql_field_types_bits.h"

struct CHARSET_INFO;
struct UDT_value_in;
struct UDT_value_out;

struct mysql_type_ident_t {
  const char *schema;
  const char *object;
};

struct mysql_type_descriptor_t {
  mysql_field_type_t mysql_type{MYSQL_FIELD_TYPE_INVALID};
  uint32_t type_flags{0};
  size_t length{0};
  size_t decimals{0};
  const CHARSET_INFO *charset{nullptr};
  bool has_explicit_collation{false};
  mysql_type_ident_t *type_ident{nullptr};
  // FIXME: m_geo_type
  // FIXME: m_internal_list
};

struct mysql_function_descriptor_t {
  const char *name;
  mysql_type_descriptor_t *return_type{nullptr};
  size_t argument_count{0};
  mysql_type_descriptor_t **argument_type_array{nullptr};
};

typedef int (*register_type_t)(mysql_type_descriptor_t *td, void *impl);
typedef int (*unregister_type_t)(mysql_type_descriptor_t *td);

typedef int (*eval_function_t)(UDT_value_out *result, size_t argument_count,
                               UDT_value_in **argument_value_array);

typedef int (*register_function_t)(mysql_function_descriptor_t *fd,
                                   eval_function_t impl);
typedef int (*unregister_function_t)(mysql_function_descriptor_t *fd);

#endif /* COMPONENTS_SERVICES_BITS_MYSQL_USER_DEFINED_TYPE_BITS_H */
