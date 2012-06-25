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


/* Requires version 2.0 of Felix Geisendörfer's MySQL client */
var mysql = require("mysql");
var dbconn;
var driverproperties;
var is_connected = 0;

/* Translate our properties to the driver's */
function getDriverProperties(props) {
  var driver = {};

  if(props.mysql_socket) {
    driver.SocketPath = props.mysql_socket;
  }
  else {
    driver.host = props.mysql_host;
    driver.port = props.mysql_port;
  }

  if(props.mysql_user)     driver.user = props.mysql_user;
  if(props.mysql_password) driver.password = props.mysql_password;
  driver.database = props.database;
  driver.debug = props.mysql_debug;

  return driver;
}


/* Constructor 
*/
exports.DBConnection = function(props) {
  driverproperties = getDriverProperties(props);
  dbconn = mysql.createConnection(driverproperties);
};


exports.DBConnection.prototype.connectSync = function() {
  dbconn.connect();
  is_connected = 1;  // but what if connection failed? 
};

exports.DBConnection.prototype.closeSync = function() {
  dbconn.end();
};

exports.DBConnection.prototype.destroy = function() { };

exports.DBConnection.prototype.isConnected = function() {
  return is_connected;
};
