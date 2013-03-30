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

MySQLTime.prototype.initializeFromJsDate = function(jsdate) {
  this.year     = jsdate.getUTCFullYear();
  this.month    = jsdate.getUTCMonth() + 1;
  this.day      = jsdate.getUTCDate();
  this.hour     = jsdate.getUTCHours();
  this.minute   = jsdate.getUTCMinutes();
  this.second   = jsdate.getUTCSeconds();
  this.microsec = jsdate.getUTCMilliseconds() * 1000;
  return this;
};

module.exports = MySQLTime;


