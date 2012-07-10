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

var mysqlconnection = require("./MysqlConnection.js");

var MysqlDefaultConnectionProperties = {  
  "implementation" : "mysql",
  "database"       : "test",
  
  "mysql_host"     : "localhost",
  "mysql_port"     : 3306,
  "mysql_user"     : null,
  "mysql_password" : null,
  "mysql_socket"   : null,
  "mysql_debug"    : false
};


exports.getDefaultConnectionProperties = function() {
  return MysqlDefaultConnectionProperties;
};


exports.connectSync = function(properties) {
  var conn = new mysqlconnection.DBConnection(properties);
  conn.connectSync();
  return conn;
};

