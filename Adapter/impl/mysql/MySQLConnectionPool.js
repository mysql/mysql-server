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


/* Requires version 2.0 of Felix Geisendoerfer's MySQL client */
var mysql = require("mysql");
var connection = require("./MySQLConnection.js");
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
exports.DBConnectionPool = function(props) {
  this.driverproperties = getDriverProperties(props);
  this.dbconn = mysql.createConnection(this.driverproperties);
  this.is_connected = false;
};


exports.DBConnectionPool.prototype.connectSync = function() {
  this.dbconn.connect();
  this.is_connected = true;
};

exports.DBConnectionPool.prototype.connect = function(user_callback) {
  connectionPool = this;
  this.dbconn.connect(function(err) {
    if (!err) {
      connectionPool.is_connected = true;
    }
    user_callback(err);
  });
};

exports.DBConnectionPool.prototype.closeSync = function() {
  this.dbconn.end();
};

exports.DBConnectionPool.prototype.destroy = function() { 
};

exports.DBConnectionPool.prototype.isConnected = function() {
  return this.is_connected;
};

exports.DBConnectionPool.prototype.getDBSession = function() {
  var newDBSession = new connection.DBSession();
  return newDBSession;
};

