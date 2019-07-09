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

var spi        = require(mynode.spi),
    service    = spi.getDBServiceProvider(global.adapter),
    properties = global.test_conn_properties;
    
var spi_test_connection = null,
    sessionSlot = 0;


function getConnectionPool(userCallback) {
  function onConnect(err, conn) {
    spi_test_connection = conn;
    userCallback(err, conn);
  }

  if(spi_test_connection) {
    userCallback(null, spi_test_connection);
  }
  else {
    service.connect(properties, onConnect);
  }
}


function closeConnectionPool(callback) {
  if(spi_test_connection) {
    spi_test_connection.close(callback);
  }
}


function allocateSessionSlot() {
  return sessionSlot++;
}


/** Open a DBSession or fail the test case */
function fail_openDBSession(testCase, callback) {
  getConnectionPool(function(error, dbConnectionPool) {
    if(dbConnectionPool && ! error) {
      dbConnectionPool.getDBSession(allocateSessionSlot(), callback);
    } else {
      testCase.fail(error);
    }
  });
}


module.exports = {
  "allocateSessionSlot" : allocateSessionSlot,
  "fail_openDBSession"  : fail_openDBSession,
  "closeConnectionPool" : closeConnectionPool,
  "getConnectionPool"   : getConnectionPool
};
