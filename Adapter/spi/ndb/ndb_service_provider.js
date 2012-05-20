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

var ndbconnection = require("./NdbConnection.js");

var NdbDefaultConnectionProperties = {  
  "implementation" : "ndb",
  "database" : "test",
  
  "ndb_connectstring" : "localhost:1186",
  "ndb_connect_retries" : 4, 
  "ndb_connect_delay" : 5,
  "ndb_connect_verbose" : 0,
  "ndb_connect_timeout_before" : 30,
  "ndb_connect_timeout_after" : 20
};


exports.getDefaultConnectionProperties = function() {
  return NdbDefaultConnectionProperties;
}


exports.connectSync = function(properties) {
  var dbconn = new ndbconnection.DBConnection(properties);
  dbconn.connectSync();
  return dbconn;
}

