/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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

