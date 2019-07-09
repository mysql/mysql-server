/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
"use strict";

var UnmappedDomainObject = function() {
  // nothing here
};

/***** Persist with unmapped domain object ***/
var t1 = new harness.ConcurrentTest("testPersistUnmappedDomainObject");
t1.run = function() {
  var testCase = this;
  var object = new UnmappedDomainObject();
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err, session2) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail();
      }
    }, session);
  });
};

/***** Save with unmapped domain object ***/
var t2 = new harness.ConcurrentTest("testSaveUnmappedDomainObject");
t2.run = function() {
  var testCase = this;
  var object = new UnmappedDomainObject();
  fail_openSession(testCase, function(session) {
    session.save(object, function(err, session2) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail();
      }
    }, session);
  });
};

/***** Update with unmapped domain object ***/
var t3 = new harness.ConcurrentTest("testUpdateUnmappedDomainObject");
t3.run = function() {
  var testCase = this;
  var object = new UnmappedDomainObject();
  fail_openSession(testCase, function(session) {
    session.update(object, function(err, session2) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail();
      }
    }, session);
  });
};

/***** Remove with unmapped domain object ***/
var t4 = new harness.ConcurrentTest("testRemoveUnmappedDomainObject");
t4.run = function() {
  var testCase = this;
  var object = new UnmappedDomainObject();
  fail_openSession(testCase, function(session) {
    session.remove(object, function(err, session2) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail();
      }
    }, session);
  });
};

/***** Load with unmapped domain object ***/
var t5 = new harness.ConcurrentTest("testLoadUnmappedDomainObject");
t5.run = function() {
  var testCase = this;
  var object = new UnmappedDomainObject();
  fail_openSession(testCase, function(session) {
    session.load(object, function(err, session2) {
      if (err) {
        testCase.pass();
      } else {
        testCase.fail();
      }
    }, session);
  });
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5];
