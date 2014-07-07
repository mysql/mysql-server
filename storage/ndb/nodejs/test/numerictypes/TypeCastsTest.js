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

"use strict";

/* You could also test that writing a negative value to a "float unsigned"
   or "double unsigned" column returns an error, but those tests are not 
   included here because NDBAPI has no way to distinguish between signed 
   and unsigned versions of floating point columns.
*/

/*  
  id int NOT NULL,
  tfloat float NOT NULL,
  tdouble double,
  tnumber decimal(11,3),
  tposint int unsigned,
  tposnumber decimal(11,3) unsigned,
  tposbigint bigint unsigned
*/
function makePass(testCase) {
  return function() { testCase.pass(); };
}

function makeFail(testCase, message) {
  return function(err) { testCase.fail(message + err.message); };
}

function shouldSucceed(test, promise) {
  promise.then(makePass(test), makeFail(test, "Should succeed"));
}

function shouldGetError(test, sqlstate, promise) {
  var message = "should get error " + sqlstate;
  promise.then(makeFail(test, message), function(error) {
    test.errorIfNotEqual("SQLState", sqlstate, error.sqlstate);
    test.failOnError();
  });
}

function Numerictypes() {}
var mapping = new mynode.TableMapping("test.numerictypes");
mapping.applyToClass(Numerictypes);


// Write numbers to all columns
var t1 = new harness.ConcurrentTest("writeNumbers");
t1.run = function() {
  fail_openSession(t1, function(session) {
    shouldSucceed(t1, 
      session.persist("numerictypes",
                      { id: 301, tfloat: -301.1, tdouble: 301.10000000, 
                        tnumber: -301.101, tposint: 301, 
                        tposnumber: 301.101, tposbigint: 301 }));
  });
};

// Write strings to all columns 
var t2 = new harness.ConcurrentTest("writeStrings");
t2.run = function() {
  fail_openSession(t2, function(session) {
    shouldSucceed(t2,
      session.persist("numerictypes",
                      { id: "302", tfloat: "-302.2", tdouble: "302.2000000",
                        tnumber: "-302.202", tposint: "302", 
                        tposnumber: "302.202", tposbigint: "302"}));
  });
};

// Write negative value to unsigned decimal column: 22003
var t3 = new harness.ConcurrentTest("writeNegativeToUnsignedDecimal");
t3.run = function() {
  fail_openSession(t3, function(session) {
    shouldGetError(t3, "22003",
      session.persist("numerictypes", { id: 307, tfloat: 307, tposnumber: -307 }));
  });
};

// Write string containing negative value to unsigned decimal:  22003 
var t4 = new harness.ConcurrentTest("writeStringNegativeToUnsignedDecimal");
t4.run = function() {
  fail_openSession(t4, function(session) {
    shouldGetError(t4, "22003", 
      session.persist("numerictypes", { id: 309, tfloat: 309, tposnumber: "-309"}));
  });
};

// Write decimal value that will get rounded due to precision.
// In SQL strict mode, this succeeds but generates a warning.
var t5 = new harness.ConcurrentTest("writeTruncatedDecimal");
t5.run = function() {
  fail_openSession(t5, function(session) {
    shouldSucceed(t5,
      session.persist("numerictypes", { id: 310, tfloat:310, tnumber: 310.0001}));
  });
};

// Write negative value to unsigned bigint:  22003
var t6 = new harness.ConcurrentTest("writeNegativeToUnsignedBigint");
t6.run = function() {
  fail_openSession(t6, function(session) {
    shouldGetError(t6, "22003",
      session.persist("numerictypes", { id: 306, tfloat: 306, tposbigint: -306 }));
  });
};

// Write string containing negative value to unsigned bigint:  22003
var t7 = new harness.ConcurrentTest("writeStringNegativeToUnsignedBigint");
t7.run = function() {
  fail_openSession(t7, function(session) {
    shouldGetError(t7, "22003",
      session.persist("numerictypes", { id: 307, tfloat: 307, tposbigint: "-307" }));
  });
};

// READ/MODIFY/SAVE as string


var t8 = new harness.ConcurrentTest("FindModifySave_Strings");
t8.run = function() {
  fail_openSession(t8, function(session) {
    session.find(Numerictypes, 108).then(function(r) {
      r.tfloat = "108.8";
      r.tdouble = "108.8";
      r.tnumber = "-108.8";
      r.tposint = "118";
      r.tposnumber = "108.80";
      r.tposbigint = "118";
      shouldSucceed(t8, session.save(r));
    });
  });
};

var t9 = new harness.ConcurrentTest("FindModifySave_22003");
t9.run = function() {
  fail_openSession(t9, function(session) {
    session.find(Numerictypes, 109).then(function(r) {
      r.tposnumber = -109;  // store a negative value in an unsigned column
      shouldGetError(t9, "22003", session.save(r));
    });
  });
};

var t10 = new harness.ConcurrentTest("FindModifySave_22003_batch");
t10.run = function() {
  fail_openSession(t10, function(session) {
    session.find(Numerictypes, 110).then(function(findResult) {
      findResult.tposnumber = -110;  // store a negative value in an unsigned column
      var batch = session.createBatch();
      batch.find(Numerictypes, 109);
      batch.save(findResult).
        then(function()  { t10.appendErrorMessage("expected error state 22003"); },
             function(e) { t10.errorIfNotEqual("sqlstate", "22003", e.sqlstate); }
        );
      batch.execute(function() { t10.failOnError(); });
    });
  });
};

module.exports.tests = [ t1,t2,t3,t4,t5,t6,t7,t8,t9,t10];
