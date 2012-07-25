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

/***** Find with domain object and primitive key ***/
t1 = new harness.ConcurrentTest("testFindDomainObjectPrimitive");
t1.run = function() {
  var testCase = this;
  // use the domain object and primitive to find an instance
  var from = t_basic.prototype
  var key = 0;
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, key, testCase);
  });
};

/***** Find with domain object and javascript literal ***/
t2 = new harness.ConcurrentTest("testFindDomainObjectLiteral");
t2.run = function() {
  var testCase = this;
  // use the domain object and literal to find an instance
  var from = t_basic.prototype
  var key = {'id' : 0};
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, key, testCase);
  });
};

/***** Find with domain object and javascript object ***/
t3 = new harness.ConcurrentTest("testFindDomainObjectObject");
t3.run = function() {
  var testCase = this;
  // use the domain object and key object to find an instance
  var from = t_basic.prototype
  var key = new t_basic_key(0);
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, key, testCase);
  });
};

/***** Find with table name and primitive key ***/
t4 = new harness.ConcurrentTest("testFindTableNamePrimitive");
t4.run = function() {
  var testCase = this;
  var from = 't_basic';
  var key = 0;
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, key, testCase);
  });
};

/***** Find with table name and javascript literal ***/
t5 = new harness.ConcurrentTest("testFindTableNameLiteral");
t5.run = function() {
  var testCase = this;
  // use table name and literal to find an instance
  var from = 't_basic';
  var key = {'id' : 0};
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, key, testCase);
  });
};

/***** Find with table name and javascript object ***/
t6 = new harness.ConcurrentTest("testFindTableNameObject");
t6.run = function() {
  var testCase = this;
  var from = 't_basic';
  var key = new t_basic_key(0);
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, key, testCase);
  });
};

module.exports.tests = [t1, t2, t3, t4, t5, t6];
