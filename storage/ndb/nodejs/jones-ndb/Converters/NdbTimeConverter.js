/*
 Copyright (c) 2013, 2022, Oracle and/or its affiliates.
 
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

"use strict";

/**********************
  This is the standard TypeConverter class used with TIME columns 
  in the Ndb Adapter.

  On the JavaScript side, this converter takes a string formatted as 
  "HH:MM:SS.sss" with optional sign +/-, and precision after the decimal
  point up to six digits.
  
  On the database side, TIME columns are read and written using a MySQLTime 
  structure.  MySQL TIME is a signed type, and the sign is preserved.  
  In MySQL 5.6, MySQL TIME can also support up to microsecond precision. 

  An application can override this converter and use MySQLTime directly:
    sessionFactory.registerTypeConverter("TIME", null);
  
  Or replace this converter with a custom one:
    sessionFactory.registerTypeConverter("TIME", myConverterClass);
      
************************/

var jones = require("database-jones"),
    MySQLTime = require(jones.common.MySQLTime),
    udebug = unified_debug.getLogger("NdbTimeConverter.js");

exports.toDB = function(jsValue) {
  var dbtime;
  if(typeof jsValue === 'string') {
    dbtime = new MySQLTime().initializeFromTimeString(jsValue);
  } else if(jsValue === null) {
    dbtime = null;
  }
  return dbtime;
};

exports.fromDB = function(dbTime) {
  if(dbTime === null) {
    return null;
  }
  if(typeof dbTime === 'object') {
    return MySQLTime.initializeFromNdb(dbTime).toTimeString();
  }
};

