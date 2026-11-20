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

#ifndef BUILTIN_TYPE_ENUM_H
#define BUILTIN_TYPE_ENUM_H

#define MYSQL_BUILTIN_TYPE_ENUM_STRING    \
  "ENUM(\n"                               \
  "  'MYSQL_BUILTIN_TYPE_DECIMAL',\n"     \
  "  'MYSQL_BUILTIN_TYPE_TINY',\n"        \
  "  'MYSQL_BUILTIN_TYPE_SHORT',\n"       \
  "  'MYSQL_BUILTIN_TYPE_LONG',\n"        \
  "  'MYSQL_BUILTIN_TYPE_FLOAT',\n"       \
  "  'MYSQL_BUILTIN_TYPE_DOUBLE',\n"      \
  "  'MYSQL_BUILTIN_TYPE_TIMESTAMP',\n"   \
  "  'MYSQL_BUILTIN_TYPE_LONGLONG',\n"    \
  "  'MYSQL_BUILTIN_TYPE_INT24',\n"       \
  "  'MYSQL_BUILTIN_TYPE_DATE',\n"        \
  "  'MYSQL_BUILTIN_TYPE_TIME',\n"        \
  "  'MYSQL_BUILTIN_TYPE_DATETIME',\n"    \
  "  'MYSQL_BUILTIN_TYPE_YEAR',\n"        \
  "  'MYSQL_BUILTIN_TYPE_NEWDATE',\n"     \
  "  'MYSQL_BUILTIN_TYPE_VARCHAR',\n"     \
  "  'MYSQL_BUILTIN_TYPE_BIT',\n"         \
  "  'MYSQL_BUILTIN_TYPE_TIMESTAMP2',\n"  \
  "  'MYSQL_BUILTIN_TYPE_DATETIME2',\n"   \
  "  'MYSQL_BUILTIN_TYPE_TIME2',\n"       \
  "  'MYSQL_BUILTIN_TYPE_NEWDECIMAL',\n"  \
  "  'MYSQL_BUILTIN_TYPE_TINY_BLOB',\n"   \
  "  'MYSQL_BUILTIN_TYPE_MEDIUM_BLOB',\n" \
  "  'MYSQL_BUILTIN_TYPE_LONG_BLOB',\n"   \
  "  'MYSQL_BUILTIN_TYPE_BLOB',\n"        \
  "  'MYSQL_BUILTIN_TYPE_VAR_STRING',\n"  \
  "  'MYSQL_BUILTIN_TYPE_STRING'\n"       \
  ")"

#define MYSQL_TYPE_ENUM_STRING    \
  "ENUM(\n"                       \
  "  'MYSQL_TYPE_DECIMAL',\n"     \
  "  'MYSQL_TYPE_TINY',\n"        \
  "  'MYSQL_TYPE_SHORT',\n"       \
  "  'MYSQL_TYPE_LONG',\n"        \
  "  'MYSQL_TYPE_FLOAT',\n"       \
  "  'MYSQL_TYPE_DOUBLE',\n"      \
  "  'MYSQL_TYPE_TIMESTAMP',\n"   \
  "  'MYSQL_TYPE_LONGLONG',\n"    \
  "  'MYSQL_TYPE_INT24',\n"       \
  "  'MYSQL_TYPE_DATE',\n"        \
  "  'MYSQL_TYPE_TIME',\n"        \
  "  'MYSQL_TYPE_DATETIME',\n"    \
  "  'MYSQL_TYPE_YEAR',\n"        \
  "  'MYSQL_TYPE_NEWDATE',\n"     \
  "  'MYSQL_TYPE_VARCHAR',\n"     \
  "  'MYSQL_TYPE_BIT',\n"         \
  "  'MYSQL_TYPE_TIMESTAMP2',\n"  \
  "  'MYSQL_TYPE_DATETIME2',\n"   \
  "  'MYSQL_TYPE_TIME2',\n"       \
  "  'MYSQL_TYPE_NEWDECIMAL',\n"  \
  "  'MYSQL_TYPE_ENUM',\n"        \
  "  'MYSQL_TYPE_SET',\n"         \
  "  'MYSQL_TYPE_TINY_BLOB',\n"   \
  "  'MYSQL_TYPE_MEDIUM_BLOB',\n" \
  "  'MYSQL_TYPE_LONG_BLOB',\n"   \
  "  'MYSQL_TYPE_BLOB',\n"        \
  "  'MYSQL_TYPE_VAR_STRING',\n"  \
  "  'MYSQL_TYPE_GEOMETRY,'\n"    \
  "  'MYSQL_TYPE_JSON,'\n"        \
  "  'MYSQL_TYPE_VECTOR'\n"       \
  ")"

#endif  // BUILTIN_TYPE_ENUM_H
