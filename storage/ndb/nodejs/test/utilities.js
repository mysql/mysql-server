/*
 Copyright (c) 2012, 2013, 2014 Oracle and/or its affiliates. All rights
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

/*global adapter */

"use strict";

var path = require("path"),
    fs = require("fs"),
    dbServiceProvider = require(mynode.spi).getDBServiceProvider(adapter),
    metadataManager = dbServiceProvider.getDBMetadataManager();


/* Manage test_connection.js file:
   Read it if it exists.
   If it doesn't exist, copy it from the standard template in lib/
*/
function getTestConnectionProperties() {
  var props_file     = path.join(mynode.fs.suites_dir, "test_connection.js");
  var props_template = path.join(mynode.fs.suites_dir, "lib", "test_connection_js.dist");
  var existsSync     = fs.existsSync || path.existsSync;
  var properties     = null;
  var f1, f2; 
  
  if(! existsSync(props_file)) {
    try {
      f1 = fs.createReadStream(props_template);
      f2 = fs.createWriteStream(props_file);
      f1.pipe(f2);
      f1.on('end', function() {});
    }
    catch(e1) {
      console.log(e1);
    }
  }

  try {
    properties = require(props_file);
  }
  catch(e2) {
  }

  return properties;
} 

function getAdapterProperties(adapter) {
  var impl = adapter || global.adapter;
  var p = new mynode.ConnectionProperties(impl);
  return p;
}

function merge(target, m) {
  var p;
  for(p in m) {
    if(m.hasOwnProperty(p)) {
      target[p] = m[p];
    }
  }
}

function getConnectionProperties() {
  var adapterProps  = getAdapterProperties();
  var localConnectionProps = getTestConnectionProperties();
  merge(adapterProps, localConnectionProps);
  return adapterProps;
}

/** Set global test connection properties */
global.test_conn_properties = getConnectionProperties();

/** Metadata management */
global.sqlCreate = function(suite, callback) {
  metadataManager.createTestTables(global.test_conn_properties, suite.name, callback);
};

global.sqlDrop = function(suite, callback) {
  metadataManager.dropTestTables(global.test_conn_properties, suite.name, callback);
};


/** Open a session or fail the test case */
global.fail_openSession = function(testCase, callback) {
  var promise;
  if (arguments.length === 0) {
    throw new Error('Fatal internal exception: fail_openSession must have  1 or 2 parameters: testCase, callback');
  }
  var properties = global.test_conn_properties;
  var mappings = testCase.mappings;
  promise = mynode.openSession(properties, mappings, function(err, session) {
    if (callback && err) {
      testCase.fail(err);
      return;
    }
    testCase.session = session;
    if (typeof callback !== 'function') {
      return;
    }
    try {
      callback(session, testCase);
    }
    catch(e) {
      testCase.appendErrorMessage(e);
      testCase.stack = e.stack;
      testCase.failOnError();
    }
  });
  return promise;
};

/** Connect or fail the test case */
global.fail_connect = function(testCase, callback) {
  var promise;
  if (arguments.length === 0) {
    throw new Error('Fatal internal exception: fail_connect must have  1 or 2 parameters: testCase, callback');
  }
  var properties = global.test_conn_properties;
  var mappings = testCase.mappings;
  promise = mynode.connect(properties, mappings, function(err, sessionFactory) {
    if (callback && err) {
      testCase.fail(err);
      return;
    }
    testCase.sessionFactory = sessionFactory;
    if (typeof callback !== 'function') {
      return;
    }
    try {
      callback(sessionFactory, testCase);
    }
    catch(e) {
      testCase.appendErrorMessage(e);
      testCase.stack = e.stack;
      testCase.failOnError();
    }
  });
  return promise;
};
