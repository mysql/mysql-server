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
  fail_openSession(testCase, function(session) {
    // use the domain object and primitive to find an instance
    session.find(t_basic.prototype, 0, function(err, instance) {
      fail_verify_t_basic(err, testCase, 0, instance);
    });
  });
};

/***** Find with domain object and javascript literal ***/
t2 = new harness.ConcurrentTest("testFindDomainObjectLiteral");
t2.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    // use the domain object and literal to find an instance
    var key = {'id' : 0};
    session.find(t_basic.prototype, key, function(err, instance) {
      fail_verify_t_basic(err, testCase, 0, instance);
    });
  });
};

/***** Find with domain object and javascript object ***/
t3 = new harness.ConcurrentTest("testFindDomainObjectObject");
t3.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    // use the domain object and literal to find an instance
    var key = new t_basic_key(0);
    session.find(t_basic.prototype, key, function(err, instance) {
      fail_verify_t_basic(err, testCase, 0, instance);
    });
  });
};

/***** Find with table name and primitive key ***/
t4 = new harness.ConcurrentTest("testFindTableNamePrimitive");
t4.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    // use the table name and primitive to find an instance
    session.find('t_basic', 0, function(err, instance) {
      fail_verify_t_basic(err, testCase, 0, instance);
    });
  });
};

/***** Find with table name and javascript literal ***/
t5 = new harness.ConcurrentTest("testFindTableNameLiteral");
t5.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    // use the table name and literal to find an instance
    var key = {'id' : 0};
    session.find('t_basic', key, function(err, instance) {
      fail_verify_t_basic(err, testCase, 0, instance);
    });
  });
};

/***** Find with table name and javascript object ***/
t6 = new harness.ConcurrentTest("testFindTableNameObject");
t6.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    // use the domain object and literal to find an instance
    var key = new t_basic_key(0);
    session.find('t_basic', key, function(err, instance) {
      fail_verify_t_basic(err, testCase, 0, instance);
    });
  });
};

module.exports.tests = [t1, t2, t3, t4, t5, t6];
