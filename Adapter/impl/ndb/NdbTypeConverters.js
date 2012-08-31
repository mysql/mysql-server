/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

"use strict";

/* jslint --node --white --vars --plusplus */
/*global udebug, exports */


function makeBufferConverter(type) {
  var rd_func = "read" + type + "LE";
  var wr_func = "write" + type + "LE";
  
  function rd_c(buffer, offset)       { return buffer[rd_func](offset);       }
  function wr_c(obj, buffer, offset)  { return buffer[wr_func](obj, offset);  }
  
  var converter = { writeToBuffer: wr_c, readFromBuffer: rd_c };
  return converter;
}


var defaultTypeConverters = [
  null,                                   // 0 
  makeBufferConverter("Int8"),            // 1  TINY INT
  makeBufferConverter("Uint8"),           // 2  TINY UNSIGNED
  makeBufferConverter("Int16"),           // 3  SMALL INT
  makeBufferConverter("Uint16"),          // 4  SMALL UNSIGNED
  null,                                   // 5  MEDIIUM INT
  null,                                   // 6  MEDIUM UNSIGNED
  makeBufferConverter("Int32"),           // 7  INT
  makeBufferConverter("Uint32"),          // 8  UNSIGNED
  null,                                   // 9  BIGINT
  null,                                   // 10 BIG UNSIGNED
  null,                                   // 11
  null,                                   // 12
  null,                                   // OLDDECIMAL
  null,                                   // 14
  null,                                   // 15
  null,                                   // 16
  null,                                   // 17
  null,                                   // 18
  null,                                   // 19
  null,                                   // 20
  null,                                   // 21 TEXT
  null,                                   // 22 
  null,                                   // 23 LONGVARCHAR
  null,                                   // 24 LONGVARBINARY
  null,                                   // 25
  null,                                   // 26
  null,                                   // 27
  null,                                   // 28 OLDDECIMAL UNSIGNED
  null,                                   // 29 DECIMAL
  null,                                   // 30 DECIMAL UNSIGNED
];

exports.defaultForType = defaultTypeConverters;

