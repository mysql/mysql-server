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

/* Write MySQL, Read NDB Datetime
   Write NDB, Read MySQL Datetime
   Write MySQL, Read NDB Timestamp
   Write NDB, Read MySQL Timestamp
   
   TODO: These tests should use the current adapter rather than "Ndb" 
         and should use a direct mysql connection rather than our "Mysql"
   
   Assure that the two backends have identical behavior,
   especially with regard to local time vs. UTC.
*/

// http://dev.mysql.com/doc/refman/5.6/en/time-zone-support.html

var propsNdb = new mynode.ConnectionProperties(global.test_conn_properties);
var propsMysql = new mynode.ConnectionProperties(global.test_conn_properties);
propsNdb.implementation = global.adapter;
propsMysql.implementation = "mysql";

function openSessions(testCase, callback) {
  function onOpen2(err, session) {
    if(err) {
      testCase.fail("mysql:", err);
    }
    else {
      testCase.mysqlSession = session;
      callback(testCase);
    }
  }
  
  function onOpen1(err, session) {
    if(err) {
      testCase.fail("ndb:", err);
    }
    else {
      testCase.ndbSession = session;
      mynode.openSession(propsMysql, testCase.mappings, onOpen2);
    }
  }
  
  mynode.openSession(propsNdb, testCase.mappings, onOpen1);
}


function ValueVerifier(testCase, field, value) {
  this.run = function onRead(err, rowRead) {
    testCase.errorIfError(err);
    testCase.errorIfNull(rowRead);
    /* Date objects can only be compared using Date.valueOf(), 
       so compare using x.valueOf(), but report using just x
    */
    try {
      if(value.valueOf() !== rowRead[field].valueOf()) {
        testCase.errorIfNotEqual(field, value, rowRead[field]);
      }
    }
    catch(e) { testCase.appendErrorMessage(e); }
    if (testCase.ndbSession) {
      testCase.ndbSession.close();
    }
    if (testCase.mysqlSession) {
      testCase.mysqlSession.close();
    }
    testCase.failOnError();
  };
}

function ReadNdbFunction(testCase) { 
  return function onMysqlPersist(err) {
    testCase.errorIfError(err);
    testCase.ndbSession.find(testCase.tableName, testCase.data.id, testCase.verifier.run);
  }
}

function ReadMysqlFunction(testCase) {
  return function onNdbPersist(err) {
    testCase.errorIfError(err);
    testCase.mysqlSession.find(testCase.tableName, testCase.data.id, testCase.verifier.run);
  }
}

function InsertMysqlFunction(tableName, data) {
  return function onSessions(testCase) {
    testCase.tableName = tableName;
    testCase.data = data;
    testCase.mysqlSession.persist(tableName, data, ReadNdbFunction(testCase));
  };
}

function InsertNdbFunction(tableName, data) {
  return function onSessions(testCase) {
    testCase.tableName = tableName;
    testCase.data = data;
    testCase.ndbSession.persist(tableName, data, ReadMysqlFunction(testCase));
  }
}

// Domain Object Constructor
function TestData(id) {
  if(id) this.id = id;
  this.cTimestamp = new Date();
}

// Timestamp NDB->MySQL
var t1 = new harness.ConcurrentTest("Timestamp-NDB-MySQL");
t1.run = function() {
  var data = new TestData(101);
  var date1970 = new Date(Date.UTC(1970, 0, 1, 3, 34, 30));
  data.cNullableTimestamp = date1970;
  this.verifier = new ValueVerifier(this, "cNullableTimestamp", date1970);
  openSessions(this, InsertNdbFunction("temporaltypes", data));
}

// Timestamp MySQL->NDB
var t2 = new harness.ConcurrentTest("Timestamp-MySQL-NDB");
t2.run = function() {
  var data = new TestData(102);
  var date1970 = new Date(Date.UTC(1970, 1, 2, 4, 34, 30));
  data.cNullableTimestamp = date1970;
  this.verifier = new ValueVerifier(this, "cNullableTimestamp", date1970);
  openSessions(this, InsertMysqlFunction("temporaltypes", data));
}

// Datetime NDB->MySQL
var t3 = new harness.ConcurrentTest("Datetime-NDB-MySQL");
t3.run = function() {
  var data = new TestData(103);
  var date1970 = new Date(Date.UTC(1970, 2, 3, 4, 34, 30));
  data.cDatetime = date1970;
  this.verifier = new ValueVerifier(this, "cDatetime", date1970);
  openSessions(this, InsertNdbFunction("temporaltypes", data));
}

// Datetime MySQL->NDB
var t4 = new harness.ConcurrentTest("Datetime-MySQL-NDB");
t4.run = function() {
  var data = new TestData(104);
  var date1970 = new Date(Date.UTC(1970, 3, 4, 4, 34, 30));
  data.cDatetime = date1970;
  this.verifier = new ValueVerifier(this, "cDatetime", date1970);
  openSessions(this, InsertMysqlFunction("temporaltypes", data));
}


exports.tests = [ t1, t2, t3, t4 ];
