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

#ifndef COMPONENTS_SERVICES_BITS_MYSQL_FIELD_TYPES_BITS_H
#define COMPONENTS_SERVICES_BITS_MYSQL_FIELD_TYPES_BITS_H

#include <stdint.h>

typedef uint32_t mysql_field_type_t;

/**
  Mirror enum_field_types values from field_types.h to keep this header
  independent from the client C API.
*/
#define MYSQL_FIELD_TYPE_DECIMAL 0
#define MYSQL_FIELD_TYPE_TINY 1
#define MYSQL_FIELD_TYPE_SHORT 2
#define MYSQL_FIELD_TYPE_LONG 3
#define MYSQL_FIELD_TYPE_FLOAT 4
#define MYSQL_FIELD_TYPE_DOUBLE 5
#define MYSQL_FIELD_TYPE_NULL 6
#define MYSQL_FIELD_TYPE_TIMESTAMP 7
#define MYSQL_FIELD_TYPE_LONGLONG 8
#define MYSQL_FIELD_TYPE_INT24 9
#define MYSQL_FIELD_TYPE_DATE 10
#define MYSQL_FIELD_TYPE_TIME 11
#define MYSQL_FIELD_TYPE_DATETIME 12
#define MYSQL_FIELD_TYPE_YEAR 13
#define MYSQL_FIELD_TYPE_NEWDATE 14
#define MYSQL_FIELD_TYPE_VARCHAR 15
#define MYSQL_FIELD_TYPE_BIT 16
#define MYSQL_FIELD_TYPE_TIMESTAMP2 17
#define MYSQL_FIELD_TYPE_DATETIME2 18
#define MYSQL_FIELD_TYPE_TIME2 19
#define MYSQL_FIELD_TYPE_TYPED_ARRAY 20
#define MYSQL_FIELD_TYPE_VECTOR 242
#define MYSQL_FIELD_TYPE_INVALID 243
#define MYSQL_FIELD_TYPE_BOOL 244
#define MYSQL_FIELD_TYPE_JSON 245
#define MYSQL_FIELD_TYPE_NEWDECIMAL 246
#define MYSQL_FIELD_TYPE_ENUM 247
#define MYSQL_FIELD_TYPE_SET 248
#define MYSQL_FIELD_TYPE_TINY_BLOB 249
#define MYSQL_FIELD_TYPE_MEDIUM_BLOB 250
#define MYSQL_FIELD_TYPE_LONG_BLOB 251
#define MYSQL_FIELD_TYPE_BLOB 252
#define MYSQL_FIELD_TYPE_VAR_STRING 253
#define MYSQL_FIELD_TYPE_STRING 254
#define MYSQL_FIELD_TYPE_GEOMETRY 255

#endif /* COMPONENTS_SERVICES_BITS_MYSQL_FIELD_TYPES_BITS_H */
