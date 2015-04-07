/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

// TODO:  default values?  undefined?

var path = require("path"),
    MySQLTime = require(path.join(mynode.fs.spi_dir,"common","MySQLTime.js")),
    udebug = unified_debug.getLogger("NdbTimeConverter.js");

exports.toDB = function(jsValue) {
  var dbtime = null;
  if(typeof jsValue === 'string') {
    dbtime = new MySQLTime().initializeFromTimeString(jsValue);
  }
  return dbtime;
};

exports.fromDB = function(dbTime) {
  return MySQLTime.initializeFromNdb(dbTime).toTimeString();
};

