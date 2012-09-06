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

/*global udebug, build_dir, path */

"use strict";

var util = require(path.join(build_dir, "ndb_adapter.node")).ndb.util,
    charsetMap = null;

function init() {
  charsetMap = new util.CharsetMap();
}

function NdbEncoder() {}
NdbEncoder.prototype = {
  read           : function(col, buffer, offset) {},
  write          : function(col, value, buffer, offset) {}
};


function NdbStringEncoder() {}
NdbStringEncoder.prototype = {
  lengthEncoder  : new NdbEncoder(),
  lengthBytes    : 0,
  read           : function(col, buffer, offset) {},
  write          : function(col, value, buffer, offset) {}
};


/* Encoder Factory for TINYINT, SMALLINT, INT  -- signed and unsigned */
function makeIntEncoder(type) {
  var rd_func = "read" + type + "LE";   // FIXME: BE on a big endian machine
  var wr_func = "write" + type + "LE";

  var enc = new NdbEncoder();
  
  enc.read = function read (col, buffer, offset) {
    return buffer[rd_func](offset);
  };
  enc.write = function write (col, value, buffer, offset) { 
    udebug.log("NdbTypeEncoders.js write", wr_func, value);
    return buffer[wr_func](value, offset);
  };

  return enc;
}

// pad string with spaces for CHAR column
var spaces = String('                                             ');
function pad(str, finalLen) {
  var result = String(str);

  while(result.length - finalLen > spaces.length) {
    result = result.concat(spaces);
  }
  if(result.length < finalLen) {
    result = result.concat(spaces.slice(0, finalLen - result.length));
  }
  return result;
}

// write string to buffer through Charset recoder
function strWrite(col, value, buffer, offset, length) {
  if(! charsetMap) { init(); }
  charsetMap.recodeIn(value, col.charsetNumber, buffer, offset, length);  
}

// read string from buffer through Charset recoder
function strRead(col, buffer, offset, length) {
  var status, r;
  if(! charsetMap) { init(); }
  r = charsetMap.recodeOut(buffer, offset, length, col.charsetNumber, status);
  return r;
}


/* CHAR */
var CharEncoder = new NdbEncoder();

CharEncoder.write = function(col, obj, buffer, offset) {
  var value = pad(obj, col.length);   /* pad with spaces */
  strWrite(col, value, buffer, offset, col.length);
};

CharEncoder.read = function(col, buffer, offset) {
  return strRead(col, buffer, offset, col.length);
};


/* VARCHAR */
var VarcharEncoder = new NdbStringEncoder();
VarcharEncoder.lengthEncoder = makeIntEncoder("Uint8");
VarcharEncoder.lengthBytes = 1;

VarcharEncoder.write = function(col, value, buffer, offset) {
  var len = value.length;
  this.lengthEncoder.write(col, len, buffer, offset);
  strWrite(col, value, buffer, offset + this.lengthBytes, len);
};

VarcharEncoder.read = function(col, buffer, offset) {
  var len = this.lengthEncoder.read(col, buffer, offset);
  return strRead(col, buffer, offset + this.lengthBytes, len);
};


/* LONGVARCHAR */
var LongVarcharEncoder = new NdbStringEncoder();
LongVarcharEncoder.lengthEncoder = makeIntEncoder("Uint16");
LongVarcharEncoder.lengthBytes = 2;
LongVarcharEncoder.write = VarcharEncoder.write;
LongVarcharEncoder.read = VarcharEncoder.read;


var defaultTypeEncoders = [
  null,                                   // 0 
  makeIntEncoder("Int8"),                 // 1  TINY INT
  makeIntEncoder("Uint8"),                // 2  TINY UNSIGNED
  makeIntEncoder("Int16"),                // 3  SMALL INT
  makeIntEncoder("Uint16"),               // 4  SMALL UNSIGNED
  null,                                   // 5  MEDIUM INT
  null,                                   // 6  MEDIUM UNSIGNED
  makeIntEncoder("Int32"),                // 7  INT
  makeIntEncoder("Uint32"),               // 8  UNSIGNED
  null,                                   // 9  BIGINT
  null,                                   // 10 BIG UNSIGNED
  null,                                   // 11
  null,                                   // 12
  null,                                   // OLDDECIMAL
  CharEncoder,                            // 14 CHAR
  VarcharEncoder,                         // 15 VARCHAR
  null,                                   // 16
  null,                                   // 17
  null,                                   // 18
  null,                                   // 19
  null,                                   // 20
  null,                                   // 21 TEXT
  null,                                   // 22 
  LongVarcharEncoder,                     // 23 LONGVARCHAR
  null,                                   // 24 LONGVARBINARY
  null,                                   // 25
  null,                                   // 26
  null,                                   // 27
  null,                                   // 28 OLDDECIMAL UNSIGNED
  null,                                   // 29 DECIMAL
  null,                                   // 30 DECIMAL UNSIGNED
];

exports.defaultForType = defaultTypeEncoders;

