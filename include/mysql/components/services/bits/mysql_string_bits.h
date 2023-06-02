/* Copyright (c) 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef COMPONENTS_SERVICES_BITS_MYSQL_STRING_BITS_H
#define COMPONENTS_SERVICES_BITS_MYSQL_STRING_BITS_H

/**
  @file mysql/components/services/bits/mysql_string_bits.h
  Type information related to strings
*/

#define CHAR_TYPE_UPPER_CASE (1 << 0)
#define CHAR_TYPE_LOWER_CASE (1 << 1)
#define CHAR_TYPE_NUMERICAL (1 << 2)
#define CHAR_TYPE_SPACING (1 << 3)
#define CHAR_TYPE_PUNCTUATION (1 << 4)
#define CHAR_TYPE_CONTROL (1 << 5)
#define CHAR_TYPE_BLANK (1 << 6)
#define CHAR_TYPE_HEXADECIMAL (1 << 7)

#endif /* COMPONENTS_SERVICES_BITS_MYSQL_STRING_BITS_H */
