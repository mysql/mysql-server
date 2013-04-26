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

// Domain Object Constructor
var test_id = 1;
function TestData() {
  this.id = test_id++;
}

function ErrorVerifier(testCase, sqlState) {
  this.run = function onRead(err, rowRead) {
    var message = "Expected SQLState " + sqlState;
    if(testCase.errorIfUnset(message, err && err.cause && err.cause.sqlstate)) 
    {
      testCase.errorIfNotEqual("Expected sqlstate", sqlState, err.cause.sqlstate);
    }
    testCase.failOnError();
  };
}

function ValueVerifier(testCase, field, value) {
  this.run = function onRead(err, rowRead) {
    testCase.errorIfError(err);
    testCase.errorIfNull(rowRead);
    try {
      if(value !== rowRead[field]) {
        testCase.errorIfNotEqual("length", value.length, rowRead[field].length);
        testCase.errorIfNotEqual(field, value, rowRead[field]);
      }
    }
    catch(e) {
      testCase.appendErrorMessage(e);
    }
    testCase.failOnError();
  };
}


function ReadFunction(testCase, session) { 
  return function onPersist(err) {
    testCase.errorIfError(err);
    session.find(TestData, testCase.data.id, testCase.verifier.run);
  }
}

function InsertFunction(data) {
  return function onSession(session, testCase) {
    testCase.data = data;
    session.persist(data, ReadFunction(testCase, session));
  };
}

var mapping = new mynode.TableMapping("test.charset_test");
mapping.mapAllColumns = true;
mapping.applyToClass(TestData);


// 
var t1 = new harness.ConcurrentTest("str_fix_latin1_ASCII");
t1.run = function() {
  var data = new TestData();
  var value = "By early 1992 the";
  data.str_fix_latin1 = value;
  this.verifier = new ValueVerifier(this, "str_fix_latin1", value);
  fail_openSession(this, InsertFunction(data));
}

var t2 = new harness.ConcurrentTest("str_var_latin1_ASCII");
t2.run = function() {
  var data = new TestData();
  var value = "search was on for a";
  data.str_var_latin1 = value;
  this.verifier = new ValueVerifier(this, "str_var_latin1", value);
  fail_openSession(this, InsertFunction(data));
}

var t3 = new harness.ConcurrentTest("str_var_latin1");
t3.run = function() {
  var data = new TestData();
  var value = "Strü-å-øøp";
  data.str_var_latin1 = value;
  this.verifier = new ValueVerifier(this, "str_var_latin1", value);
  fail_openSession(this, InsertFunction(data));
}



/*
....+....+....+....+
 
good byte-stream 
encoding of multi-
byte character sets.
The draft ISO 10646 
standard contained 
a non-required annex
called UTF-1 that 
provided a byte-
stream encoding of 
its 32-bit code 
points. This encoding 
was not satisfactory
on performance 
grounds, but did
introduce the notion 
that bytes in the range of 0–127 continue representing the ASCII characters in UTF, thereby providing backward compatibility with ASCII.
In July 1992, the X/Open committee XoJIG was looking for a better encoding. Dave Prosser of Unix System Laboratories submitted a proposal for one that had faster implementation characteristics and introduced the improvement that 7-bit ASCII characters would only represent themselves; all multibyte sequences would include only bytes where the high bit was set. This original proposal, FSS-UTF (File System Safe UCS Transformation Format), was similar in concept to UTF-8, but lacked the crucial property of self-synchronization.[8][9]
In August 1992, this proposal was circulated by an IBM X/Open representative to interested parties. Ken Thompson of the Plan 9 operating system group at Bell Labs then made a small but crucial modification to the encoding, making it very slightly less bit-efficient than the previous proposal but allowing it to be self-synchronizing, meaning that it was no longer necessary to read from the beginning of the string to find code point boundaries. Thompson's design was outlined on September 2, 1992, on a placemat in a New Jersey diner with Rob Pike. The following days, Pike and Thompson implemented it and updated Plan 9 to use it throughout, and then communicated their success back to X/Open.[8]
UTF-8 was first officially presented at the USENIX conference in San Diego, from January 25–29, 1993.
In November 2003 UTF-8 was restricted by RFC 3629 to four bytes in order to match the constraints of the UTF-16 character encoding.
Google reported that in 2008 UTF-8 (misleadingly labelled "Unicode") became the most common encoding for HTML files.[10][11
*/

module.exports.tests = [t1 , t2 , t3];
