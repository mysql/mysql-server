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
/*jslint newcap: true */
/*global t_basic, verify_t_basic, fail_verify_t_basic */

var path = require("path");
var ndbimpl;
try {
  ndbimpl = require(path.join(mynode.fs.build_dir, "ndb_adapter.node")).ndb.impl;
}
catch(e) {}

function assertVO(testCase, object, expected) {
  if(global.adapter == "ndb") {
    testCase.errorIfNotEqual("Expected NDB Value Object", expected,
                            ndbimpl.isValueObject(object));
  }
}

/*  Store a row and read it back
 *  Callback gets: (error, foundObject, session) 
*/ 
function persistAndFind(testCase, object, onFindCallback) {

  function onPersistThenFind(err, session) {
    session.find(t_basic, object.id, onFindCallback, session);
  }

  function onOpenThenPersist(session, testCase) { 
    testCase.session = session;
    assertVO(testCase, object, false);
    session.persist(t_basic, object, onPersistThenFind, testCase.session);
  }
    
  fail_openSession(testCase, onOpenThenPersist);
}

/* Generates a callback function to find a row,
   compare it to a known row, and pass or fail the test case.
*/
function makeFindAndCompare(testCase, session, refObject) {
  function onFindThenCompare(err, readObject) {
    var i, keys, k;
    testCase.errorIfError(err);
    testCase.errorIfNull("expected readObject on Find", readObject);
    assertVO(testCase, readObject, true);
    try {
      keys = Object.keys(refObject);
      for(i = 0 ; i < keys.length ; i++) {
        k = keys[i];
        testCase.errorIfNotEqual(k, refObject[k], readObject[k]);
      }
    }
    catch(e) {
      testCase.appendErrorMessage(e);
    }
    testCase.failOnError();  
  }

  return function findAndCompare(err) {
    testCase.errorIfError(err);
    session.find(t_basic, refObject.id, onFindThenCompare);
  };
}


// Find_Modify_Save 
var t1 = new harness.ConcurrentTest("Find_Modify_Save");
t1.run = function() {
  var row = new t_basic(23001, 'Snoopy', 23, 23001);
  
  /* Create a row with age = 23.
     Read it. 
     Update age to 29.
     Save it.
     Read it again.
  */

  function onFindThenSave(err, object, theSession) {
    assertVO(t1, object, true);
    object.age = 29; 
    t1.errorIfNotEqual("Age:Write_Read", 29, object.age);
    theSession.save(object, makeFindAndCompare(t1, theSession, object));
  }

  persistAndFind(this, row, onFindThenSave);
};


// Find_Modify_Update 
var t2 = new harness.ConcurrentTest("Find_Modify_Update");
t2.run = function() {
  var row = new t_basic(23002, 'Linus', 23, 23002); 
  
  function onFindThenUpdate(err, object, session) {
    assertVO(t2, object, true);
    object.age = object.age + 10;
    t2.errorIfNotEqual("Age:Read_Write_Read", row.age+10, object.age);
    session.update(object, makeFindAndCompare(t2, session, object));
  }
  
  persistAndFind(this, row, onFindThenUpdate);
};

// Find_Remove
var t3 = new harness.ConcurrentTest("Find_Remove");
t3.run = function() {
  var row = new t_basic(23003, 'Charlie Brown', 5, 23003);
  
  function onFindDeletedRow(err, obj) {
    t3.errorIfNotNull("found row should be null", obj);
    t3.failOnError();
  }
    
  function onDeleteThenFindAgain(err, session) {
    t3.errorIfError(err);
    session.find(t_basic, 20003, onFindDeletedRow);        
  }

  function onFindThenDelete(err, object, session) {
    t3.errorIfError(err);
    assertVO(t3, object, true);
    session.remove(object, onDeleteThenFindAgain, session);
  }
  
  persistAndFind(this, row, onFindThenDelete);
};


// Find_ModifyPK_Save
var t4 = new harness.ConcurrentTest("Find_ModifyPK_Save");
t4.run = function() {
  var row4a = new t_basic(23004, 'Lucy', 80, 23004);
  var row4b = new t_basic(23104, 'Lucy', 80, 23104);

  function onFindThenChange(err, object, session) {
    t4.errorIfError(err);
    assertVO(t4, object, true);
    object.id = 23104;
    object.magic = 23104;  // unique index on magic
    session.save(object, makeFindAndCompare(t4, session, row4b));
  }
  
  persistAndFind(this, row4a, onFindThenChange);
};

// Find_ModifyPK_Persist
var t5 = new harness.ConcurrentTest("Find_ModifyPK_Persist");
t5.run = function() {
  var row5a = new t_basic(23005, 'Sally', 4, 23005);
  var row5b = new t_basic(23105, 'Sally', 4, 23105);

  function onFindThenChange(err, object, session) {
    t5.errorIfError(err);
    assertVO(t5, object, true);
    object.id = 23105;
    object.magic = 23105;  // unique index on magic
    session.persist(object, makeFindAndCompare(t5, session, row5b));
  }
  
  persistAndFind(this, row5a, onFindThenChange);
};


// Find_ModifyPK_Delete
var t6 = new harness.ConcurrentTest("Find_ModifyPK_Delete");
t6.run = function() {
  var row6 = new t_basic(23006, 'Schroeder', 12, 23006);

  function onDeleteExpectError(err) { 
    t6.errorIfNull("Expected Error", err);
    t6.errorIfNotEqual("SQLSTATE", "02000", err.sqlstate);
    t6.failOnError();
  }

  function onFindThenModify(err, object, theSession) {
    assertVO(t6, object, true);
    object.id = 23106;
    theSession.remove(object, onDeleteExpectError);
  }

  persistAndFind(this, row6, onFindThenModify);
};


function onLoadThenCompare(err, testCase, expectedObject, loadedObject) {
  var i, keys, k;
  testCase.errorIfError(err);
  assertVO(testCase, loadedObject, true);
  try {
    keys = Object.keys(expectedObject);
    for(i = 0 ; i < keys.length ; i++) {
      k = keys[i];
      testCase.errorIfNotEqual(k, expectedObject[k], loadedObject[k]);
    }
  }
  catch(e) {
    testCase.appendErrorMessage(e);
  }
  testCase.failOnError();  
}

/* load uses the object it has and copies database values into it 
    based on finding the object in the database. */
// Find_ModifyPK_Load
var t7 = new harness.ConcurrentTest("Find_ModifyPK_Load");
t7.run = function() {
  var specialTag = 1;
  var row7a = new t_basic(23007, 'Pigpen', 23, 23007);
  var row7b = new t_basic(7, 'Employee 7', 7, 7);
  row7b.specialTag = specialTag;
  
  function onFindThenLoad(err, object, theSession) {
    assertVO(t7, object, true);
    object.id = 7; 
    object.specialTag = specialTag;
    theSession.load(object, onLoadThenCompare, t7, row7b, object);
  }

  persistAndFind(this, row7a, onFindThenLoad);
};


// Find_ModifyUK_Load
var t8 = new harness.ConcurrentTest("Find_ModifyUK_Load");
t8.run = function() {
  var specialTag = 1;
  var row8a = new t_basic(23008, 'Franklin', 23, 23008);
  var row8b = new t_basic(8, 'Employee 8', 8, 8);
  row8b.specialTag = specialTag;
 
  function onFindThenLoad(err, object, theSession) {
    assertVO(t8, object, true);
    object.magic = 8; 
    object.specialTag = specialTag;
/// NB: 
    object.id = undefined;
    theSession.load(object, onLoadThenCompare, t8, row8b, object);
  }

  persistAndFind(this, row8a, onFindThenLoad);
};



// implement getColumnMaskForVO(obj) and test masked columns 
exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8];

