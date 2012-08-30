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


function IntConverter() {
}

IntConverter.prototype.writeToBuffer = function(obj, buffer, offset, length) {
  buffer.writeInt32LE(obj, offset);
};

IntConverter.prototype.readFromBuffer = function(buffer, offset, length) {
  return buffer.readInt32LE(offset);
};

var intConverter = new IntConverter();

var defaultTypeConverters = {
  'TINYINT'       : intConverter,
  'SMALLINT'      : intConverter,               
  'MEDIUMINT'     : intConverter,
  'INT'           : intConverter,
  'BIGINT'        : intConverter,
  'FLOAT'         : intConverter,
  'DOUBLE'        : intConverter,
  'DECIMAL'       : intConverter,
  'CHAR'          : intConverter,
  'VARCHAR'       : intConverter,
  'BLOB'          : intConverter,
  'TEXT'          : intConverter,
  'DATE'          : intConverter,
  'TIME'          : intConverter,
  'DATETIME'      : intConverter,
  'YEAR'          : intConverter,
  'TIMESTAMP'     : intConverter,
  'BIT'           : intConverter,
  'BINARY'        : intConverter,
  'VARBINARY'     : intConverter
};

exports.defaultForType = defaultTypeConverters;