/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

var tbl1A = function(i, j) {
  if (i) {this.i = i;}
  if (j) {this.j = j;}
};

var t1 = new harness.ConcurrentTest("MissingColumnMapping");
t1.run = function() {
  var testCase = this;
  var tablemapping = new mynode.TableMapping('tbl1');
  tablemapping.mapAllColumns = false;
  tablemapping.mapField('i');
  tablemapping.mapField('bad_field', 'missing_column');
  tablemapping.applyToClass(tbl1A);
  global.fail_openSession(testCase, function(session) {
    session.persist(tbl1A, {i:0, j:0}, function(err) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail('Bad mapping should not succeed.');
      }
    });
  });
};

module.exports.tests = [t1];
