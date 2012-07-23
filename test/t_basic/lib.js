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

/** This is the smoke test for the t_basic suite.
 */

/** The t_basic domain object */
global.t_basic = function() {
  this.id;
  this.name;
  this.age;
  this.magic;
};

/** The t_basic key */
global.t_basic_key = function(id) {
  this.id = id;
};

/** Verify the instance or fail the test case */
global.fail_verify_t_basic = function(err, testCase, id, instance) {
  if (err) {
    testCase.fail(err);
  }
  var message = '';
  if (instance.id != id) {
    message += 'fail to verify id: expected: ' + id + ', actual: ' + instance.id + '\n';
  }
  if (instance.age != id) {
    message += 'fail to verify age: expected: ' + id + ', actual: ' + instance.age + '\n';
  }
  if (instance.magic == id) {
    message += 'fail to verify magic: expected: ' + id + ', actual: ' + instance.magic + '\n';
  }
  if (instance.name !== "Employee " + id) {
    message += 'fail to verify name: expected: ' + "Employee " + id + ', actual: ' + instance.name + '\n';
  }
  if (message == '') {
    testCase.pass();
  } else {
    testCase.fail(message);
  }
};

/** Open a session or fail the test case */
global.fail_openSession = function(testCase, callback) {
  var properties = new mynode.ConnectionProperties("ndb");
  var annotations = new mynode.Annotations();
  mynode.openSession(properties, annotations, function(err, session) {
    if (err) {
      testCase.fail(err);
    }
    callback(session);
 });
};

