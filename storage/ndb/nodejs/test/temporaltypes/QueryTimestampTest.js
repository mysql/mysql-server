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

var udebug = unified_debug.getLogger("integraltypes/QueryTimestampTest.js");
var QueryTest = require('../lib/QueryTest.js');

/** equal query should use index scan */
var q1 = {name: 'q1', p1: new Date('2001-01-01 01:01:01'), expected: [1], queryType: 2, ordered: false, predicate: function(qdt) {
  return qdt.cTimestamp.eq(qdt.param('p1'));
}};

/** greater and less query should use index scan */
var q2 = {name: 'q2', p1: new Date('2001-01-01 01:01:01'), p2: new Date('2004-04-04 04:04:04'), expected: [2, 3], queryType: 2, ordered: false, predicate: function(qdt) {
  return qdt.cTimestamp.gt(qdt.param('p1')).and(qdt.cTimestamp.lt(qdt.param('p2')));
}};

/** greater equal query and less equal should use index scan */
var q3 = {name: 'q3', p1: new Date('2001-01-01 01:01:01'), p2: new Date('2004-04-04 04:04:04'), expected: [1, 2, 3, 4], queryType: 2, ordered: false, predicate: function(qdt) {
  return qdt.cTimestamp.ge(qdt.param('p1')).and(qdt.cTimestamp.le(qdt.param('p2')));
}};

/** between query should use index scan */
// between doesn't work yet
var q4 = {name: 'q4', p1: new Date('2001-01-01 01:01:01'), p2: new Date('2004-04-04 04:04:04'), expected: [1, 2, 3, 4], queryType: 2, ordered: false, predicate: function(qdt) {
  return qdt.cTimestamp.between(qdt.param('p1'), qdt.param('p2'));
}};

var queryTests = [q1, q2, q3, q4];

/** Set up domain type */
var temporaltypes = function temporaltypes() {};
new mynode.TableMapping('temporaltypes').applyToClass(temporaltypes);

/** Define test */
var testQueries = new QueryTest("testTimestampQueries", temporaltypes, queryTests);

module.exports.tests = [testQueries];

