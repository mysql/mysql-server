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

var harness = require(global.test_harness_module);

/***** Insert ***/
var testInsert = function() {
  var testCase = this;
  var Employee = function() {
    this.id;
    this.name;
    this.age;
    this.magic;
  }
  var properties = mynode.ConnectionProperties('ndb');
  var annotations = mynode.Annotations();
  annotations.mapClass(Employee.prototype,
      {'table' : 't_basic'});
  var session;
  mynode.openSession(properties, annotations, function(err, session) {
    // use the session to insert an instance
    var employee = new Employee();
    employee.id = 999;
    employee.name = 'Employee 999';
    employee.age = 999;
    employee.magic = 999;
    session.persist(employee, function(err, instance) {
      if (err) {
        testCase.appendMessage('testInsert.persist failed.');
        testCase.fail(err);
      } else {
        testCase.pass();
      }
    });
  });
};

/*** t_basic.ndb.testInsert ***/
var t1 = new harness.SerialTest("testInsert");
t1.impl= "ndb";
t1.run = testInsert;

/*** t_basic.mysql.testInsert ***/
var t2 = new harness.SerialTest("testInsert");
t2.impl= "mysql";
t2.run = testInsert;


/******************* TEST GROUPS ********/

var ndb_group = new harness.Test("ndb").makeTestGroup(t1);

var mysql_group = new harness.Test("mysql").makeTestGroup(t2);

var group = new harness.Test("spi").makeTestGroup(ndb_group, mysql_group);


/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports = group;
