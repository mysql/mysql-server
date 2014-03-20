/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

// Table a has (id int not null primary key, cint int)

// Attempt to store a string into an int column
// Check for appropriate error handling

// Current behavior: 
//    ndb adapter hangs
//    mysql adapter stores a 0

var nosql = require('../..');
var udebug = unified_debug.getLogger("issues/2014-03-18Test.js");

var t1 = new harness.ConcurrentTest("WriteStringToIntCol");

t1.run = function() {
  fail_openSession(t1, function(session, testCase) {
    session.persist("a", { id: 2, cint: "foo"}, function(err) {
      if(err) {
        udebug.log(err);
        t1.pass();
      } else {
        t1.fail("should have error");
      }
    });
  });
}

module.exports.tests = [ t1 ];


