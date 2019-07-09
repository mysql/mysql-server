/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

var path     = require("path");
var udebug   = unified_debug.getLogger("spi/lib.js");
var exec     = require("child_process").exec;

var SQL = {};
    
var spi_test_connection = null;

/* SQL DDL Utilities
*/
var runSQL = function(sqlPath, source, properties, callback) {

  function childProcess(error, stdout, stderr) {
    udebug.log('harness runSQL process completed.');
    udebug.log(source + ' stdout: ' + stdout);
    udebug.log(source + ' stderr: ' + stderr);
    if (error !== null) {
      console.log(source + 'exec error: ' + error);
    } else {
      udebug.log(source + ' exec OK');
    }
    if(callback) {
      callback(error);  
    }
  }

  // runSQL starts here
  var cmd = 'mysql';
  if(properties) {
    if(properties.mysql_socket)     { cmd += " --socket=" + properties.mysql_socket; }
    else if(properties.mysql_port)  { cmd += " --port=" + properties.mysql_port; }
    if(properties.mysql_host)     { cmd += " -h " + properties.mysql_host; }
    if(properties.mysql_user)     { cmd += " -u " + properties.mysql_user; }
    if(properties.mysql_password) { cmd += " --password=" + properties.mysql_password; }
  }
  cmd += ' <' + sqlPath; 
  udebug.log('harness runSQL forking process...' + cmd);
  var child = exec(cmd, childProcess);
};

SQL.create =  function(dir, properties, callback) {
  var sqlPath = path.join(dir, 'create.sql');
  udebug.log_detail("createSQL path: " + sqlPath);
  runSQL(sqlPath, 'createSQL', properties, callback);
};

SQL.drop = function(dir, properties, callback) {
  var sqlPath = path.join(dir, 'drop.sql');
  udebug.log_detail("dropSQL path: " + sqlPath);
  runSQL(sqlPath, 'dropSQL', properties, callback);
};

/** Timer functions specifically for crund. Multiple timers can be used simultaneously,
 * as long as each timer is created via new Timer().
 * start() starts the timer.
 * stop() stops the timer and writes results to the file.
 * mode is the mode of operation (indy, each, or bulk)
 * operation is the operation (e.g. persist, find, delete)
 * numberOfIterations is the number of iterations of each operation
 */
var Timer = function() {
  
};

Timer.prototype.start = function(mode, operation, numberOfIterations) {
  //console.log('lib.Timer.start', mode, operation, 'iterations:', numberOfIterations);
  this.mode = mode;
  this.operation = operation;
  this.numberOfIterations = numberOfIterations;
  this.current = Date.now();
};

Timer.prototype.stop = function() {
  function pad(str, count, onRight) {
    while (str.length < count)
      str = (onRight ? str + ' ' : ' ' + str);
    return str;
  }
  function rpad(num, str) {
    return pad(str, num, true);
  }
  function lpad(num, str) {
    return pad(str, num, false);
  }
  this.interval = Date.now() - this.current;
  this.average = this.interval / this.numberOfIterations;
  var ops = Math.round(this.numberOfIterations * 1000 / this.interval);
  console.log(rpad(18, this.mode + ' ' + this.operation),
              '    time: ' + lpad(4, this.interval.toString()) + 'ms',
              '    avg latency: ' + lpad(4, this.average.toFixed(3)) + 'ms',
              '    ops/s: ' + lpad(4, ops.toString()));
};

exports.SQL = SQL;
exports.Timer = Timer;
