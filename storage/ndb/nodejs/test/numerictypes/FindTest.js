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

"use strict";
/*global fail_verify_integraltypes*/

/***** Find with id ***/
var t1 = new harness.ConcurrentTest("testFindById");
t1.run = function() {
  var testCase = this;
  // use the id to find an instance
  var from = global.integraltypes;
  var key = 2;
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_integraltypes, key, testCase, true);
  });
};

module.exports.tests = [t1];

