/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#ifndef PLUGIN_X_SRC_NGS_PROTOCOL_PROTOCOL_CONST_H_
#define PLUGIN_X_SRC_NGS_PROTOCOL_PROTOCOL_CONST_H_

#define MYSQLX_COLUMN_FLAGS_UINT_ZEROFILL 0x0001     // UINT zerofill
#define MYSQLX_COLUMN_FLAGS_DOUBLE_UNSIGNED 0x0001   // DOUBLE 0x0001 unsigned
#define MYSQLX_COLUMN_FLAGS_FLOAT_UNSIGNED 0x0001    // FLOAT  0x0001 unsigned
#define MYSQLX_COLUMN_FLAGS_DECIMAL_UNSIGNED 0x0001  // DECIMAL 0x0001 unsigned
#define MYSQLX_COLUMN_FLAGS_BYTES_RIGHTPAD 0x0001    // BYTES  0x0001 rightpad
#define MYSQLX_COLUMN_FLAGS_DATETIME_TIMESTAMP \
  0x0001  // DATETIME 0x0001 timestamp

#define MYSQLX_COLUMN_FLAGS_NOT_NULL 0x0010
#define MYSQLX_COLUMN_FLAGS_PRIMARY_KEY 0x0020
#define MYSQLX_COLUMN_FLAGS_UNIQUE_KEY 0x0040
#define MYSQLX_COLUMN_FLAGS_MULTIPLE_KEY 0x0080
#define MYSQLX_COLUMN_FLAGS_AUTO_INCREMENT 0x0100

#endif  // PLUGIN_X_SRC_NGS_PROTOCOL_PROTOCOL_CONST_H_
