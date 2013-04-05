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

/***** Find with domain object and primitive primary key ***/
var t1 = new harness.ConcurrentTest("PersistFindModifySaveFind");
t1.run = function() {
  
  /* Create a row with age = 23.
     Read it. 
     Update age to 29.
     Save it.
     Read it again.
  */

  function onReadAgain(err, readObject, writtenObject) {
    t1.errorIfError(err);
    t1.errorIfNotEqual("id", writtenObject.id, readObject.id);
    t1.errorIfNotEqual("name", writtenObject.name, readObject.name);
    t1.errorIfNotEqual("age", writtenObject.age, readObject.age);
    t1.failOnError();  
  }
  
  function onSave(err, sess, thePreviousObject) { 
    t1.errorIfError(err);
    sess.find(t_basic, 23001, onReadAgain, thePreviousObject);
  }
  
  function onFind(err, object, theSession) {
    object.age = 29; 
    t1.errorIfNotEqual("ReadAfterWrite", 29, object.age);
    theSession.save(object, onSave, theSession, object);
  }

  function onInsert(err, aSession) {
    aSession.find(t_basic, 23001, onFind, aSession);
  }

  function onOpen(session, testCase) { 
    testCase.session = session;
    var row = new t_basic(23001, 'Snoopy', 23, 23001);
    session.persist(t_basic, row, onInsert, testCase.session);
  }
  
  fail_openSession(this, onOpen);
};


exports.tests = [ t1];

