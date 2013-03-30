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

  On the JavaScript side, this converter takes a number of milliseconds.
  
  On the database side, TIME columns are read and written using a MySQLTime 
  structure.  MySQL TIME is a signed type, and the sign is preserved.  
  In MySQL 5.6, MySQL TIME can also support up to microsecond precision. 

  An application can override this converter and use MySQLTime directly:
    sessionFactory.registerTypeConverter("TIME", null);
  
  Or replace this converter with a custom one:
    sessionFactory.registerTypeConverter("TIME", myConverterClass);
      
************************/

var MySQLTime = require("./MySQLTime.js"),
    unified_debug = require(path.join(api_dir, "unified_debug")),
    udebug = unified_debug.getLogger("NdbTimeConverter.js");


exports.toDB = function(msec) { 
  var tm = new MySQLTime();
  if(msec < 0) {
    tm.sign = -1;
    msec = -msec;
  }
  tm.hour = Math.floor(msec / 3600000);    msec %= 3600000;
  tm.minute = Math.floor(msec / 60000);    msec %= 60000;
  tm.second = Math.floor(msec / 1000);     msec %= 1000;
  tm.microsec = msec * 1000;
  return tm;
};

exports.fromDB = function(mysqlTime) {
  var msec = mysqlTime.hour * 3600000
           + mysqlTime.minute * 60000
           + mysqlTime.second * 1000
           + mysqlTime.microsec / 1000;
  msec *= mysqlTime.sign;
  return msec;
};

