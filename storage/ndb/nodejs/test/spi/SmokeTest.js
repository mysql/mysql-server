/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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

/** This is the smoke test for the spi suite.
    We go just as far as getDBServiceProvider().
    This tests the loading of required compiled code in shared library files.
 */

"use strict";

try {
  require("./suite_config.js");
} catch (e) {}

var test = new harness.SmokeTest("LoadModule");

test.run = function() {
  var lib = require("./lib.js"),
      test = this;  

  function onCreate(err) {
    if(err) test.fail("create.sql failed");
    else test.pass();
  }

  function onConnected(err, connection) {
    if(err) {
      test.fail("Connection error " + err);
      return;
    }
    sqlCreate(test.suite, onCreate);  
  }

  lib.getConnectionPool(onConnected);
};


exports.tests = [test];
