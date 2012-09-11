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


/***** Insert ***/
var testInsert = function() {
  var testCase = this;
  var Employee = function() {
    this.id;
    this.name;
    this.age;
    this.magic;
  }
  var properties = mynode.ConnectionProperties(global.adapter);
  var annotations = mynode.Annotations();
  annotations.mapClass(Employee,
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
        testCase.fail(err);
      } else {
        testCase.pass();
      }
    });
  });
};

/*** t_basic.ndb.testInsert ***/
var t1 = new harness.SerialTest("testInsert");
t1.run = testInsert;


/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports = t1;
