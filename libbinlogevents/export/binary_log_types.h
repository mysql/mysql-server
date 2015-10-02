/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file binary_log_types.h

  @brief This file contains the field type.


  @note This file can be imported both from C and C++ code, so the
  definitions have to be constructed to support this.
*/

#ifndef BINARY_LOG_TYPES_INCLUDED
#define BINARY_LOG_TYPES_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Constants exported from this package.
 */

typedef enum enum_field_types {
  MYSQL_TYPE_DECIMAL, MYSQL_TYPE_TINY,
  MYSQL_TYPE_SHORT,  MYSQL_TYPE_LONG,
  MYSQL_TYPE_FLOAT,  MYSQL_TYPE_DOUBLE,
  MYSQL_TYPE_NULL,   MYSQL_TYPE_TIMESTAMP,
  MYSQL_TYPE_LONGLONG,MYSQL_TYPE_INT24,
  MYSQL_TYPE_DATE,   MYSQL_TYPE_TIME,
  MYSQL_TYPE_DATETIME, MYSQL_TYPE_YEAR,
  MYSQL_TYPE_NEWDATE, MYSQL_TYPE_VARCHAR,
  MYSQL_TYPE_BIT,
  MYSQL_TYPE_TIMESTAMP2,
  MYSQL_TYPE_DATETIME2,
  MYSQL_TYPE_TIME2,
  MYSQL_TYPE_JSON=245,
  MYSQL_TYPE_NEWDECIMAL=246,
  MYSQL_TYPE_ENUM=247,
  MYSQL_TYPE_SET=248,
  MYSQL_TYPE_TINY_BLOB=249,
  MYSQL_TYPE_MEDIUM_BLOB=250,
  MYSQL_TYPE_LONG_BLOB=251,
  MYSQL_TYPE_BLOB=252,
  MYSQL_TYPE_VAR_STRING=253,
  MYSQL_TYPE_STRING=254,
  MYSQL_TYPE_GEOMETRY=255
} enum_field_types;

#define DATETIME_MAX_DECIMALS 6

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* BINARY_LOG_TYPES_INCLUDED */
