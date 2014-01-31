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
/*jslint newcap: true */
/*global t_basic */


/***** Find with domain object and primitive primary key ***/
var t1 = new harness.ConcurrentTest("testFindDomainObjectPrimitive");
t1.run = function() {
  var testCase = this;
  // use the domain object and primitive to find an instance
  var from = global.t_basic;
  var id = 1;
  function beginTransaction(session) {
    testCase.session = session;
    return session.currentTransaction().begin();
  }
  function find() {
    return testCase.session.find(from, id);
  }
  function verify(instance) {
    global.verify_t_basic(null, instance, id, testCase, true);
  }
  function commit() {
    return testCase.session.currentTransaction().commit();
  }
  function commitOnFailure(err) {
    testCase.appendErrorMessage('error reported' + err.message);
    return testCase.session.currentTransaction().commit();
  }
  function reportSuccess() {
    testCase.errorIfTrue('t1 transaction should not be active after commit promise fulfilled', 
        testCase.session.currentTransaction().isActive());
    testCase.failOnError();
  }
  function reportFailure(err) {
    testCase.fail('t1 failed: ' + err.message);
  }
  fail_openSession(testCase).
    then(beginTransaction).
    then(find).
    then(verify).
    then(commit, commitOnFailure).
    then(reportSuccess, reportFailure);
};

module.exports.tests = [t1];

