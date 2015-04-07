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

"use strict";

function MySQLTime() {
}

/* In general we would declare all properties in the constructor. 
   Here, though, we put them in the prototype, so that only explicitly
   set ones reach the code in NdbTypeEncoders.cpp
*/
MySQLTime.prototype = {
  sign     : +1,
  year     : 0,
  month    : 0,
  day      : 0,
  hour     : 0,
  minute   : 0,
  second   : 0,
  microsec : 0,
  fsp      : 0,    /* Fraction seconds precision, 0 - 6 */
  valid    : true  /* If false, TypeEncoder will reject value */
};

MySQLTime.prototype.initializeFromTimeString = function(jsValue) {
  var numValue = "";
  var decimalPart = "";
  var decimalLength = 0;
  var pos = 0;
  var colons = 0;
  var c, cc;
  
  /* Initial + or - */
  c = jsValue.charAt(pos);
  if(c == '+' || c == '-') {
    numValue += c;
    pos++;
  }

  /* Copy numbers but skip separators */
  while(pos < jsValue.length) {
    c = jsValue.charAt(pos);
    cc = jsValue.charCodeAt(pos);
    if(cc > 47 && cc < 58) {  // i.e. isdigit()
      numValue += jsValue.charAt(pos);
    }
    else if(c === '.' && ! decimalPart) { 
      decimalPart = jsValue.slice(pos+1);
      pos = jsValue.length;
    }
    else if(c === ':') {
      colons++;
    } else if((cc > 64 && cc < 91) || (cc > 96 && cc < 123)) {
      /* MySQL truncates a time string where it sees a letter */
      pos = jsValue.length;
    }
    pos++;
  }

  /* Convert fixed and decimal parts to numbers */
  numValue = parseInt(numValue, 10);

  if(numValue < 0) {
    this.sign = -1;
    numValue = -numValue;  
  }

  this.hour = Math.floor(numValue / 10000); 
  this.minute = (Math.floor(numValue / 100)) % 100;
  this.second = numValue % 100;

  /* Expand decimal part out to microseconds */
  if(decimalPart > 0) {
    decimalLength = decimalPart.length;
    this.microsec = parseInt(decimalPart, 10);
    while(decimalLength > 6) {  
      this.microsec = Math.floor(this.microsec / 10);
      decimalLength--;
    }
    while(decimalLength < 6) {
      this.microsec *= 10;
      decimalLength++;
    }
  }

  /* Special case HH:MM */
  if(this.hour == 0 && colons == 1) {
    this.hour = this.minute;
    this.minute = this.second;
    this.second = 0; 
  }

  return this;
};

MySQLTime.prototype.initializeFromDateTimeString = function(jsValue) {
  // split date from time separated by blank
  var parts = jsValue.split(' ');
  this.initializeFromDateString(parts[0]);
  this.initializeFromTimeString(parts[1]);
  return this;
};

MySQLTime.prototype.initializeFromDateString = function(jsValue) {
  var parts = jsValue.split(/[\W_]/);   // split on a non-word or an underscore
  this.year = parts[0];
  this.month = parts[1];
  this.day = parts[2];
  return this;
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

MySQLTime.prototype.toDateTimeString = function() {
  return this.toDateString() + ' ' + this.toTimeString();
};

MySQLTime.prototype.toTimeString = function() {
  var strTime = "";
  var fsec = this.microsec;
  var fsp = this.fsp;
  if(this.sign === -1) strTime="-";

  if(this.hour < 10) strTime += "0";
  strTime += this.hour + ":";
  if(this.minute < 10) strTime += "0";
  strTime += this.minute + ":";
  if(this.second < 10) strTime += "0";
  strTime += this.second;
  if(fsp) {
    strTime += ".";
    if(fsec > 0) {
      while(fsp < 6) {
        fsec = Math.floor(fsec / 10);
        fsp++;
      }
      fsec = fsec.toString();
    }
    else {
      fsec = "";
    }
    while(fsec.length < this.fsp) {
      fsec = "0" + fsec;
    }
    strTime += fsec;
  }

  return strTime;
};

MySQLTime.prototype.toDateString = function() {
  var month = this.month;
  var day = this.day;
  if(month < 10) month = "0" + month;
  if(day < 10) day = "0" + day;
  return this.year + "-" + month + "-" + day;
};

MySQLTime.initializeFromNdb = function(dbTime) {
  dbTime.toJsDateUTC = MySQLTime.prototype.toJsDateUTC;
  dbTime.toJsDateLocal = MySQLTime.prototype.toJsDateLocal;
  dbTime.toTimeString = MySQLTime.prototype.toTimeString;
  dbTime.toDateString = MySQLTime.prototype.toDateString;
  return dbTime;
};

module.exports = MySQLTime;
