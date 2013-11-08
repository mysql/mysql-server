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

"use strict";

/*jslint newcap: true */
/*global t_basic */

var ReadWrite = require('../lib/read_write').ReadWrite;

/** Data array */
var data = [
            {id: 10000, name: "Data 10000", age: 10000, magic: 10000},
            {id: 10001, name: "Data 10001", age: 10001, magic: 10001}  
  ]
;

/***** Read write adapter table name ***/
var t1 = new harness.SerialTest("write adapter read adapter table name");
t1.run = function() {
  var testCase = this;
  var tableNameOrConstructor = 't_basic';
  testCase.mappings = tableNameOrConstructor;
  fail_openSession(testCase, function(session) {
    var rw = new ReadWrite(testCase, tableNameOrConstructor, data, session);
    rw.writeAdapterReadAdapter();
  });
};

/***** Read write adapter constructor ***/
var t2 = new harness.SerialTest("write adapter read adapter constructor");
t2.run = function() {
  var testCase = this;
  var tableNameOrConstructor = global.t_basic;
  testCase.mappings = tableNameOrConstructor;
  fail_openSession(testCase, function(session) {
    var rw = new ReadWrite(testCase, tableNameOrConstructor, data, session);
    rw.writeAdapterReadAdapter();
  });
};

/***** Read write sql table name ***/
var t3 = new harness.SerialTest("write sql read sql table name");
t3.run = function() {
  var testCase = this;
  var tableNameOrConstructor = 't_basic';
  testCase.mappings = tableNameOrConstructor;
  fail_openSession(testCase, function(session) {
    var rw = new ReadWrite(testCase, tableNameOrConstructor, data, session);
    rw.writeSQLReadSQL();
  });
};

/***** Read write sql constructor ***/
var t4 = new harness.SerialTest("write sql read sql constructor");
t4.run = function() {
  var testCase = this;
  var tableNameOrConstructor = global.t_basic;
  testCase.mappings = tableNameOrConstructor;
  fail_openSession(testCase, function(session) {
    var rw = new ReadWrite(testCase, tableNameOrConstructor, data, session);
    rw.writeSQLReadSQL();
  });
};

/***** Write adapter read sql table name ***/
var t5 = new harness.SerialTest("write adapter read sql table name");
t5.run = function() {
  var testCase = this;
  var tableNameOrConstructor = 't_basic';
  testCase.mappings = tableNameOrConstructor;
  fail_openSession(testCase, function(session) {
    var rw = new ReadWrite(testCase, tableNameOrConstructor, data, session);
    rw.writeAdapterReadSQL();
  });
};

/***** Write adapter read sql constructor ***/
var t6 = new harness.SerialTest("write adapter read sql constructor");
t6.run = function() {
  var testCase = this;
  var tableNameOrConstructor = global.t_basic;
  testCase.mappings = tableNameOrConstructor;
  fail_openSession(testCase, function(session) {
    var rw = new ReadWrite(testCase, tableNameOrConstructor, data, session);
    rw.writeAdapterReadSQL();
  });
};

/***** Write sql read adapter table name ***/
var t7 = new harness.SerialTest("write sql read adapter table name");
t7.run = function() {
  var testCase = this;
  var tableNameOrConstructor = 't_basic';
  testCase.mappings = tableNameOrConstructor;
  fail_openSession(testCase, function(session) {
    var rw = new ReadWrite(testCase, tableNameOrConstructor, data, session);
    rw.writeSQLReadAdapter();
  });
};

/***** Write sql read adapter constructor ***/
var t8 = new harness.SerialTest("write sql read adapter constructor");
t8.run = function() {
  var testCase = this;
  var tableNameOrConstructor = global.t_basic;
  testCase.mappings = tableNameOrConstructor;
  fail_openSession(testCase, function(session) {
    var rw = new ReadWrite(testCase, tableNameOrConstructor, data, session);
    rw.writeSQLReadAdapter();
  });
};

module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8];

