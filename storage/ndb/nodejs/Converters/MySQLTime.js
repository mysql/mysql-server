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


/* MySQLTime is a lossless interchange format 
   between JavaScript and MySQL TIME and DATETIME columns.
   
   For a Converter that uses MySQLTime, see NdbDatetimeConverter.js    
*/

function MySQLTime() {
};

MySQLTime.prototype = {
  sign     : +1,
  year     : 0,
  month    : 0,
  day      : 0,
  hour     : 0,
  minute   : 0,
  second   : 0,
  microsec : 0,
};

MySQLTime.prototype.initializeFromJsDateUTC = function(jsdate) {
  this.year     = jsdate.getUTCFullYear();
  this.month    = jsdate.getUTCMonth() + 1;
  this.day      = jsdate.getUTCDate();
  this.hour     = jsdate.getUTCHours();
  this.minute   = jsdate.getUTCMinutes();
  this.second   = jsdate.getUTCSeconds();
  this.microsec = jsdate.getUTCMilliseconds() * 1000;
  return this;
};

MySQLTime.prototype.initializeFromJsDateLocal = function(jsdate) {
  this.year     = jsdate.getFullYear();
  this.month    = jsdate.getMonth() + 1;
  this.day      = jsdate.getDate();
  this.hour     = jsdate.getHours();
  this.minute   = jsdate.getMinutes();
  this.second   = jsdate.getSeconds();
  this.microsec = jsdate.getMilliseconds() * 1000;
  return this;
};

MySQLTime.prototype.toJsDateUTC = function() {
  var utcdate = Date.UTC(this.year, this.month - 1, this.day,
                         this.hour, this.minute, this.second,
                         this.microsec / 1000);
  return new Date(utcdate);
};

MySQLTime.prototype.toJsDateLocal = function() {
  return new Date(this.year, this.month - 1, this.day,
                  this.hour, this.minute, this.second,
                  this.microsec / 1000);
};

MySQLTime.initializeFromNdb = function(dbTime) {
  dbTime.toJsDateUTC = MySQLTime.prototype.toJsDateUTC;
  dbTime.toJsDateLocal = MySQLTime.prototype.toJsDateLocal;
  return dbTime;
}

module.exports = MySQLTime;


