/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#ifndef _PROTOCOL_CONST_H_
#define _PROTOCOL_CONST_H_

#define MYSQLX_COLUMN_BYTES_CONTENT_TYPE_GEOMETRY 0x0001 // GEOMETRY (WKB encoding)
#define MYSQLX_COLUMN_BYTES_CONTENT_TYPE_JSON     0x0002 // JSON (text encoding)
#define MYSQLX_COLUMN_BYTES_CONTENT_TYPE_XML      0x0003 // XML (text encoding)

#define MYSQLX_COLUMN_FLAGS_UINT_ZEROFILL         0x0001 // UINT zerofill
#define MYSQLX_COLUMN_FLAGS_DOUBLE_UNSIGNED       0x0001 // DOUBLE 0x0001 unsigned
#define MYSQLX_COLUMN_FLAGS_FLOAT_UNSIGNED        0x0001 // FLOAT  0x0001 unsigned
#define MYSQLX_COLUMN_FLAGS_DECIMAL_UNSIGNED      0x0001 // DECIMAL 0x0001 unsigned
#define MYSQLX_COLUMN_FLAGS_BYTES_RIGHTPAD        0x0001 // BYTES  0x0001 rightpad
#define MYSQLX_COLUMN_FLAGS_DATETIME_TIMESTAMP    0x0001 // DATETIME 0x0001 timestamp

#define MYSQLX_COLUMN_FLAGS_NOT_NULL              0x0010
#define MYSQLX_COLUMN_FLAGS_PRIMARY_KEY           0x0020
#define MYSQLX_COLUMN_FLAGS_UNIQUE_KEY            0x0040
#define MYSQLX_COLUMN_FLAGS_MULTIPLE_KEY          0x0080
#define MYSQLX_COLUMN_FLAGS_AUTO_INCREMENT        0x0100

#endif // _PROTOCOL_CONST_H_
