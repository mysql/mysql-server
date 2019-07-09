/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
 reserved.
 
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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
*/

/**********************
  This is the standard TypeConverter class used with JSON columns.
  A JavaScript object is converted to and from a string in the database.

  Writing from JavaScript to DB, this converter takes a JavaScript object and
  returns a string using JSON.stringify. 

  Reading from DB to JavaScript, this converter takes a string formatted as 
  JSON.stringify and parses it into a JavaScript object to store in a field.
  
************************/

var udebug = unified_debug.getLogger("JSONConverter.js");

exports.toDB = function(jsValue) {
  return JSON.stringify(jsValue);
};

exports.fromDB = function(dbValue) {
  return JSON.parse(dbValue);
};

