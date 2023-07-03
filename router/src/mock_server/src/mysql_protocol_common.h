/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLD_MOCK_MYSQL_PROTOCOL_COMMON_INCLUDED
#define MYSQLD_MOCK_MYSQL_PROTOCOL_COMMON_INCLUDED

#include <string>

namespace server_mock {

/** @enum MySQLColumnType
 *
 * Supported MySQL Coumn types.
 *
 **/
enum class MySQLColumnType {
  DECIMAL = 0x00,
  TINY = 0x01,
  SHORT = 0x02,
  LONG = 0x03,
  FLOAT = 0x04,
  DOUBLE = 0x05,
  NULL_ = 0x06,
  TIMESTAMP = 0x07,
  LONGLONG = 0x08,
  INT24 = 0x09,
  DATE = 0x0a,
  TIME = 0x0b,
  DATETIME = 0x0c,
  YEAR = 0x0d,
  NEWDATE = 0x0e,
  VARCHAR = 0x0f,
  BIT = 0x10,
  TIMESTAMP2 = 0x11,
  JSON = 0xf5,
  NEWDECIMAL = 0xf6,
  ENUM = 0xf7,
  SET = 0xf8,
  TINY_BLOB = 0xf9,
  MEDIUM_BLOB = 0xfa,
  LONG_BLOB = 0xfb,
  BLOB = 0xfc,
  VAR_STRING = 0xfd,
  STRING = 0xfe,
  GEOMETRY = 0xff
};

}  // namespace server_mock

#endif  // MYSQLD_MOCK_MYSQL_PROTOCOL_COMMON_INCLUDED
