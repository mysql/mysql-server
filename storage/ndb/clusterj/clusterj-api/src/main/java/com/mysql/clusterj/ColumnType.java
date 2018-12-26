/*
   Copyright (c) 2009, 2013, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj;

/** This class enumerates the column types for columns in ndb.
 *
 */
public enum ColumnType {

    Bigint,          ///< 64 bit. 8 byte signed integer, can be used in array
    Bigunsigned,     ///< 64 Bit. 8 byte signed integer, can be used in array
    Binary,          ///< Length is fixed. A fixed array of 1-byte values
    Bit,             ///< Bit, length specifies no of bits
    Blob,            ///< Binary large object (see NdbBlob)
    Char,            ///< Length is fixed. A fixed array of 1-byte chars
    Date,            ///< Precision down to 1 day(sizeof(Date) == 4 bytes )
    Datetime,        ///< Precision down to 1 sec (sizeof(Datetime) == 8 bytes )
    Double,          ///< 64-bit float. 8 byte float, can be used in array
    Decimal,         ///< MySQL >= 5.0 signed decimal,  Precision, Scale
    Decimalunsigned,
    Float,           ///< 32-bit float. 4 bytes float, can be used in array
    Int,             ///< 32 bit. 4 byte signed integer, can be used in array
    Longvarchar,     ///< Length bytes: 2, little-endian
    Longvarbinary,   ///< Length bytes: 2, little-endian
    Mediumint,       ///< 24 bit. 3 byte signed integer, can be used in array
    Mediumunsigned,  ///< 24 bit. 3 byte unsigned integer, can be used in array
    Olddecimal,
    Olddecimalunsigned,
    Smallint,        ///< 16 bit. 2 byte signed integer, can be used in array
    Smallunsigned,   ///< 16 bit. 2 byte unsigned integer, can be used in array
    Text,            ///< Text blob
    Time,            ///< Time without date
    Timestamp,       ///< Unix time
    Tinyint,         ///< 8 bit. 1 byte signed integer, can be used in array
    Tinyunsigned,    ///< 8 bit. 1 byte unsigned integer, can be used in array
    Undefined,
    Unsigned,        ///< 32 bit. 4 byte unsigned integer, can be used in array
    Varbinary,       ///< Length bytes: 1, Max: 255
    Varchar,         ///< Length bytes: 1, Max: 255
    Year,            ///< Year 1901-2155 (1 byte)
    Time2,           ///< MySQL 5.6 time2
    Datetime2,       ///< MySQL 5.6 datetime2
    Timestamp2       ///< MySQL 5.6 timestamp2
}
