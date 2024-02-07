/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef COMPONENTS_SERVICES_STORED_PROGRAM_BITS_H
#define COMPONENTS_SERVICES_STORED_PROGRAM_BITS_H

// 1-to-1 mapping with include/field_types.h
#define MYSQL_SP_ARG_TYPE_DECIMAL (1ULL << 0)
#define MYSQL_SP_ARG_TYPE_TINY (1ULL << 1)
#define MYSQL_SP_ARG_TYPE_SHORT (1ULL << 2)
#define MYSQL_SP_ARG_TYPE_LONG (1ULL << 3)
#define MYSQL_SP_ARG_TYPE_FLOAT (1ULL << 4)
#define MYSQL_SP_ARG_TYPE_DOUBLE (1ULL << 5)
#define MYSQL_SP_ARG_TYPE_NULL (1ULL << 6)
#define MYSQL_SP_ARG_TYPE_TIMESTAMP (1ULL << 7)
#define MYSQL_SP_ARG_TYPE_LONGLONG (1ULL << 8)
#define MYSQL_SP_ARG_TYPE_INT24 (1ULL << 9)
#define MYSQL_SP_ARG_TYPE_DATE (1ULL << 10)
#define MYSQL_SP_ARG_TYPE_TIME (1ULL << 11)
#define MYSQL_SP_ARG_TYPE_DATETIME (1ULL << 12)
#define MYSQL_SP_ARG_TYPE_YEAR (1ULL << 13)
#define MYSQL_SP_ARG_TYPE_NEWDATE (1ULL << 14)
#define MYSQL_SP_ARG_TYPE_VARCHAR (1ULL << 15)
#define MYSQL_SP_ARG_TYPE_BIT (1ULL << 16)
#define MYSQL_SP_ARG_TYPE_TIMESTAMP2 (1ULL << 17)
#define MYSQL_SP_ARG_TYPE_DATETIME2 (1ULL << 18)
#define MYSQL_SP_ARG_TYPE_TIME2 (1ULL << 19)
#define MYSQL_SP_ARG_TYPE_TYPED_ARRAY (1ULL << 20)
#define MYSQL_SP_ARG_TYPE_INVALID (1ULL << 21)
#define MYSQL_SP_ARG_TYPE_BOOL (1ULL << 22)
#define MYSQL_SP_ARG_TYPE_JSON (1ULL << 23)
#define MYSQL_SP_ARG_TYPE_NEWDECIMAL (1ULL << 24)
#define MYSQL_SP_ARG_TYPE_ENUM (1ULL << 25)
#define MYSQL_SP_ARG_TYPE_SET (1ULL << 26)
#define MYSQL_SP_ARG_TYPE_TINY_BLOB (1ULL << 27)
#define MYSQL_SP_ARG_TYPE_MEDIUM_BLOB (1ULL << 28)
#define MYSQL_SP_ARG_TYPE_LONG_BLOB (1ULL << 29)
#define MYSQL_SP_ARG_TYPE_BLOB (1ULL << 30)
#define MYSQL_SP_ARG_TYPE_VAR_STRING (1ULL << 31)
#define MYSQL_SP_ARG_TYPE_STRING (1ULL << 32)
#define MYSQL_SP_ARG_TYPE_GEOMETRY (1ULL << 33)

#endif
