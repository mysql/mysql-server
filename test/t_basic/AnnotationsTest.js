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

domainClass = function(id, name, age, magic) {
  this.id = id;
  this.name = name;
  this.age = age;
  this.magic = magic;
};

var t1 = new harness.ConcurrentTest("AnnotationsConstructor");
t1.run = function() {
  var annotations = new mynode.Annotations();
  return true;
};

var t2 = new harness.ConcurrentTest("strict");
t2.run = function() {
  var annotations = new mynode.Annotations();
  annotations.strict(true);
  return true;
};

var t3 = new harness.ConcurrentTest("mapAllTables");
t3.run = function() {
  var annotations = new mynode.Annotations();
  annotations.mapAllTables(true);
  return true;
};

var t4 = new harness.ConcurrentTest("mapClass");
t4.run = function() {
  var annotations = new mynode.Annotations();
  annotations.strict(true);
  annotations.mapClass(domainClass.prototype, {
    "table" : "t_basic",
    "schema" : "def",
    "database" : "test",
    "autoIncrementBatchSize" : 1,
    "mapAllColumns" : false,
    "fields" : {
      "name" : "id",
      "nullValue" : "NONE",
      "column" : "id",
      "notPersistent" : false
    }
  });
  return true; // test is complete
};

var group = new harness.Test("annotations").makeTestGroup(t1, t2, t3, t4);

module.exports = group;
