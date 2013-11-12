/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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

/*global spi_module, unified_debug */
"use strict";

var spi        = require(spi_module),
    service    = spi.getDBServiceProvider(global.adapter),
    properties = global.test_conn_properties,
    udebug     = unified_debug.getLogger("spi/lib.js");
    
var spi_test_connection = null;
var sessionSlot = 0;

exports.getConnectionPool = function(userCallback) {

  function onConnect(err, conn) {
    udebug.log("getConnectionPool onConnect err:", err);    
    spi_test_connection = conn;
    userCallback(err, conn);
  }

  if(spi_test_connection) {
    udebug.log("getConnectionPool returning established connection");
    userCallback(null, spi_test_connection);
  }
  else {
    udebug.log("getConnectionPool opening new connection with properties: ", properties);
    service.connect(properties, onConnect);
  }
};


exports.closeConnectionPool = function(callback) {
  if(spi_test_connection) {
    spi_test_connection.close(callback);
  }
};

exports.allocateSessionSlot = function() {
  return sessionSlot++;
};

