/*
 Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights
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

var util    = require("util");

var mindb = 1;
var maxdb = 8;
var numberOfDBs = maxdb - mindb + 1;

var tbl3 = function(i, j) {
  this.i = i;
  this.j = j;
};
new mynode.TableMapping('mysqljs_multidb_test3.tbl3').applyToClass(tbl3);

var tbl4 = function(i, j) {
  this.i = i;
  this.j = j;
};
new mynode.TableMapping('mysqljs_multidb_test4.tbl4').applyToClass(tbl4);

var tbl7 = function(i, j) {
  this.i = i;
  this.j = j;
};
new mynode.TableMapping('tbl7').applyToClass(tbl7);

var tbl8 = function(i, j) {
  this.i = i;
  this.j = j;
};
new mynode.TableMapping('tbl8').applyToClass(tbl8);

// badtbl8 is mapped to a table that only exists in database test8
var badtbl8 = function(i, j) {
  this.i = i;
  this.j = j;
};
new mynode.TableMapping('tbl8').applyToClass(badtbl8);

// badtesttbl8 is mapped to a non-existent table
var badtesttbl8 = function(i, j) {
  this.i = i;
  this.j = j;
};
new mynode.TableMapping('mysqljs_multidb_test.tbl8').applyToClass(badtesttbl8);

// badtbl9 is not mapped
var badtbl9 = function(i, j) {
  this.i = i;
  this.j = j;
};

// create all properties for connecting with different default databases
var properties = mynode.ConnectionProperties(global.adapter);
// make a local copy of the properties
var propertiesList = [];
var p, x, props = {};
for (p = mindb; p < mindb + numberOfDBs; ++p) {
  for (x in properties) if (properties.hasOwnProperty(x)) {
    props[x] = properties[x];
  }
  props.database = 'mysqljs_multidb_test' + p;
  propertiesList[p] = props;
}

var connectWithDefaultDb = function(testCase, db, callback) {
  console.log('ConnectTest openSession with', propertiesList[db].database);
  var tbl = 'tbl' + db;
  mynode.openSession(propertiesList[db], tbl, function(err, session) {
    if (err) {
      testCase.appendErrorMessage('error opening session ' + db);
    } else {
      console.log('ConnectTest openSession success for', propertiesList[db].database);
      // verify that the named table has an entry for cached metadata and chached table handler
//      session.find()
    }
      callback(db);
  });
};

var verifyTableMetadataCached = function(testCase, sessionFactory, qualifiedTableName) {
  // look in sessionFactory to see if there is a cached table metadata
  var split = qualifiedTableName.split('\.'); 
  var databaseName = split[0];
  var tableName = split[1];
  var tableMetadata = sessionFactory.tableMetadatas[qualifiedTableName];
  if (tableMetadata === undefined) {
    testCase.appendErrorMessage(tableName + 'was not cached in session factory.');
  } else {
    testCase.errorIfNotEqual('verifyTableMetadataCached mismatch database name', tableMetadata.database, databaseName);
    testCase.errorIfNotEqual('verifyTableMetadataCached mismatch table name', tableMetadata.name, tableName);
  }
};

var verifyConstructorMetadataCached = function(testCase, sessionFactory, qualifiedTableName, constructor) {
  verifyTableMetadataCached(testCase, sessionFactory, qualifiedTableName);
    // look in constructor to see if there is a cached table handler
  var split = qualifiedTableName.split('\.');
  var databaseName = split[0];
  var tableName = split[1];
  var tableHandler = constructor.prototype.mynode.tableHandler;
  if (tableHandler === undefined) {
    testCase.appendErrorMessage(tableName + 'table handler was not cached in constructor.');
  } else {
    testCase.errorIfNotEqual('verifyConstructorMetadataCached mismatch database name', tableHandler.dbTable.database, databaseName);
    testCase.errorIfNotEqual('verifyConstructorMetadataCached mismatch table name', tableHandler.dbTable.name, tableName);
  }
};

var t1 = new harness.ConcurrentTest('testOpenSessionExplicitTable');
t1.run = function() {
  var testCase = this;
  mynode.openSession(propertiesList[1], 'mysqljs_multidb_test1.tbl1', function(err, session) {
    if (err) {
      testCase.appendErrorMessage('t1 error on openSession with mysqljs_multidb_test1.tbl1');
    } else {
      testCase.session = session;
      verifyTableMetadataCached(testCase, session.sessionFactory, 'mysqljs_multidb_test1.tbl1');
    }
    testCase.failOnError();
  });
};

var t2 = new harness.ConcurrentTest('testConnectExplicitTable');
t2.run = function() {
  var testCase = this;
  mynode.connect(propertiesList[2], 'mysqljs_multidb_test2.tbl2', function(err, sessionFactory) {
    if (err) {
      testCase.appendErrorMessage('t2 error on connect with mysqljs_multidb_test2.tbl2');
    } else {
      verifyTableMetadataCached(testCase, sessionFactory, 'mysqljs_multidb_test2.tbl2');
    }
    testCase.failOnError();
  });
};

var t3 = new harness.ConcurrentTest('testOpenSessionExplicitConstructor');
t3.run = function() {
  var testCase = this;
  mynode.openSession(propertiesList[3], tbl3, function(err, session) {
    if (err) {
      testCase.appendErrorMessage('t3 error on openSession with mysqljs_multidb_test3.tbl3');
    } else {
      testCase.session = session;
      verifyConstructorMetadataCached(testCase, session.sessionFactory, 'mysqljs_multidb_test3.tbl3', tbl3);
    }
    testCase.failOnError();
  });
};

var t4 = new harness.ConcurrentTest('testConnectExplicitConstructor');
t4.run = function() {
  var testCase = this;
  mynode.connect(propertiesList[4], tbl4, function(err, sessionFactory) {
    if (err) {
      testCase.appendErrorMessage('t4 error on connect with mysqljs_multidb_test4.tbl4 ' + err);
    } else {
      verifyConstructorMetadataCached(testCase, sessionFactory, 'mysqljs_multidb_test4.tbl4', tbl4);
    }
    testCase.failOnError();
  });
};

var t5 = new harness.ConcurrentTest('testOpenSessionImplicitTable');
t5.run = function() {
  var testCase = this;
  mynode.openSession(propertiesList[5], 'tbl5', function(err, session) {
    if (err) {
      testCase.appendErrorMessage('t5 error on openSession with tbl5 with properties ' + 
          '\n' + util.inspect(propertiesList[5]) + '\n' + err);
    } else {
      testCase.session = session;
      verifyTableMetadataCached(testCase, session.sessionFactory, 'mysqljs_multidb_test5.tbl5');
    }
    testCase.failOnError();
  });
};

var t6 = new harness.ConcurrentTest('testConnectImplicitTable');
t6.run = function() {
  var testCase = this;
  mynode.connect(propertiesList[6], 'tbl6', function(err, sessionFactory) {
    if (err) {
      testCase.appendErrorMessage('t6 error on connect with tbl6 with properties ' + 
          '\n' + util.inspect(propertiesList[6]) + '\n' + err);
    } else {
      verifyTableMetadataCached(testCase, sessionFactory, 'mysqljs_multidb_test6.tbl6');
    }
    testCase.failOnError();
  });
};

var t7 = new harness.ConcurrentTest('testOpenSessionImplicitConstructor');
t7.run = function() {
  var testCase = this;
  mynode.openSession(propertiesList[7], tbl7, function(err, session) {
    if (err) {
      testCase.appendErrorMessage('t7 error on openSession with tbl7 with properties ' + 
          '\n' + util.inspect(propertiesList[7]) + '\n' + err);
    } else {
      testCase.session = session;
      verifyConstructorMetadataCached(testCase, session.sessionFactory, 'mysqljs_multidb_test7.tbl7', tbl7);
    }
    testCase.failOnError();
  });
};

var t8 = new harness.ConcurrentTest('testConnectImplicitConstructor');
t8.run = function() {
  var testCase = this;
  mynode.connect(propertiesList[8], tbl8, function(err, sessionFactory) {
    if (err) {
      testCase.appendErrorMessage('t8 error on connect with tbl8 ' + err);
    } else {
      verifyConstructorMetadataCached(testCase, sessionFactory, 'mysqljs_multidb_test8.tbl8', tbl8);
    }
    testCase.failOnError();
  });
};
// count the number of subtests in t9 and end the test case when all have reported
function reportResults(testCase) {
  if (++testCase.actualResultCount > testCase.expectedResultCount) {
    testCase.failOnError();
  }
}

var t9 = new harness.ConcurrentTest('testFailureCases');
t9.run = function() {
  var testCase = this;
  testCase.expectedResultCount = 1;
  testCase.actualResultCount = 0;
  mynode.connect(properties, 'tbl8', function(err) {
    if (!err) {
      testCase.appendErrorMessage('t9 failed to return error on tbl8');
    }
    reportResults(testCase);
  });
  ++testCase.expectedResultCount;
  mynode.connect(properties, 'mysqljs_multidb_test.tbl8', function(err) {
    if (!err) {
      testCase.appendErrorMessage('t9 failed to return error on mysqljs_multidb_test.tbl8');
    }
    reportResults(testCase);
  });
  ++testCase.expectedResultCount;
  mynode.connect(properties, badtbl8, function(err) {
    if (!err) {
      testCase.appendErrorMessage('t9 failed to return error on badtbl8');
    }
    reportResults(testCase);
  });
  ++testCase.expectedResultCount;
  mynode.connect(properties, badtesttbl8, function(err) {
    if (!err) {
      testCase.appendErrorMessage('t9 failed to return error on badtesttbl8');
    }
    reportResults(testCase);
  });
  ++testCase.expectedResultCount;
  mynode.connect(properties, badtbl9, function(err) {
    if (!err) {
      testCase.appendErrorMessage('t9 failed to return error on badtbl9');
    }
    reportResults(testCase);
  });
  reportResults(testCase);
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8, t9];
