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

/*global unified_debug, exports, api_dir, path */

"use strict";

/* Requires version 2.0 of Felix Geisendoerfer's MySQL client */

var mysql = require("mysql");
var mysqlConnection = require("./MySQLConnection.js");
var mysqlDictionary = require("./MySQLDictionary.js");
var udebug = unified_debug.getLogger("MySQLConnectionPool.js");
var stats_module = require(path.join(api_dir), "stats.js");
var stats = stats_module.getWriter("spi","mysql","DBConnectionPool");


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

  if(props.mysql_user) {
    driver.user = props.mysql_user;
  }
  if(props.mysql_password) {
    driver.password = props.mysql_password;
  }
  driver.database = props.database;
  driver.debug = props.mysql_debug;

  return driver;
}


/* Constructor saves properties but doesn't actually do anything with them.
*/
exports.DBConnectionPool = function(props) {
  this.driverproperties = getDriverProperties(props);
  udebug.log('MySQLConnectionPool constructor with driverproperties: ' + util.inspect(this.driverproperties));
  // connections not being used at the moment
  this.pooledConnections = [];
  // connections that are being used (wrapped by DBSession)
  this.openConnections = [];
  this.is_connected = false;
  stats.incr("created");
};


exports.DBConnectionPool.prototype.connectSync = function() {
  var pooledConnection;
  stats.incr("connect","sync");

  if (this.is_connected) {
    return;
  }
  pooledConnection = mysql.createConnection(this.driverproperties);
  if (typeof(pooledConnection) === 'undefined') {
    throw new Error('Fatal internal exception: got undefined pooledConnection for createConnection');
  }
  if (pooledConnection === null) {
    throw new Error('Fatal internal exception: got null pooledConnection for createConnection');
  }
  
  this.pooledConnections[0] = pooledConnection;
  this.is_connected = true;
};

exports.DBConnectionPool.prototype.connect = function(user_callback) {
  var callback = user_callback;
  var connectionPool = this;
  var pooledConnection;
  stats.incr("connect","async");
  
  if (this.is_connected) {
    udebug.log('MySQLConnectionPool.connect is already connected');
    callback(null, this);
  } else {
    pooledConnection = mysql.createConnection(this.driverproperties);
    pooledConnection.connect(function(err) {
    if (err) {
      stats.incr("connections","failed");
      callback(err);
    } else {
      stats.incr("connections","succesful");
      connectionPool.pooledConnections[0] = pooledConnection;
      connectionPool.is_connected = true;
      callback(null, connectionPool);
    }
  });
  }
};

exports.DBConnectionPool.prototype.closeSync = function() {
  udebug.log('closeSync');
  var i;
  for (i = 0; i < this.pooledConnections.length; ++i) {
    var pooledConnection = this.pooledConnections[i];
    udebug.log('closeSync ending pooled connection ' + i);
    if (pooledConnection._connectCalled) {
        pooledConnection.end();
    }
  }
  this.pooledConnections = [];
  for (i = 0; i < this.openConnections.length; ++i) {
    udebug.log('closeSync ending open connection ' + i);
    var openConnection = this.openConnections[i];
    if (openConnection._connectCalled) {
      this.openConnections[i].end();
    }
  }
  this.openConnections = [];
  this.is_connected = false;
};

exports.DBConnectionPool.prototype.destroy = function() { 
};

exports.DBConnectionPool.prototype.isConnected = function() {
  return this.is_connected;
};

var countOpenConnections = function(connectionPool) {
  var i, count = 0;
  for (i = 0; i < connectionPool.openConnections.length; ++i) {
    if (connectionPool.openConnections[i] !== null) {
      count++;
    }
  }
  return count;
};

exports.DBConnectionPool.prototype.getDBSession = function(index, callback) {
  // get a connection from the pool
  var pooledConnection = null;
  var connectionPool = this;
  var newDBSession = null;

  if (this.pooledConnections.length > 0) {
    udebug.log_detail('MySQLConnectionPool.getDBSession before found a pooledConnection for index ' + index + ' in connectionPool; ', 
        ' pooledConnections:', connectionPool.pooledConnections.length,
        ' openConnections: ', countOpenConnections(connectionPool));
    // pop a connection from the pool
    pooledConnection = connectionPool.pooledConnections.pop();
    newDBSession = new mysqlConnection.DBSession(pooledConnection, connectionPool);
    connectionPool.openConnections[index] = pooledConnection;
    udebug.log_detail('MySQLConnectionPool.getDBSession after found a pooledConnection for index ' + index + ' in connectionPool; ', 
        ' pooledConnections:', connectionPool.pooledConnections.length,
        ' openConnections: ', countOpenConnections(connectionPool));
    callback(null, newDBSession);
  } else {
    // create a new pooled connection
    var connected_callback = function(err) {
      newDBSession = new mysqlConnection.DBSession(pooledConnection, connectionPool);
      connectionPool.openConnections[index] = pooledConnection;
      udebug.log_detail('MySQLConnectionPool.getDBSession created a new pooledConnection for index ' + index + ' ; ', 
          ' pooledConnections:', connectionPool.pooledConnections.length,
          ' openConnections: ', countOpenConnections(connectionPool));
      
      callback(err, newDBSession);
    };
    // create a new connection
    pooledConnection = mysql.createConnection(this.driverproperties);
    pooledConnection.connect(connected_callback);
  }
};

exports.DBConnectionPool.prototype.returnPooledConnection = function(index) {
throw new Error('Fatal internal exception: returnPooledConnection is not supported.');
var pooledConnection = this.openConnections[index];
  this.openConnections[index] = null;
  this.pooledConnections.push(pooledConnection);
  udebug.log('MySQLConnectionPool.returnPooledConnection; ', 
      ' pooledConnections:', this.pooledConnections.length,
      ' openConnections: ', countOpenConnections(this));
};

exports.DBConnectionPool.prototype.getTableMetadata = function(databaseName, tableName, dbSession, user_callback) {

  var TableMetadataHandler = function() {
    this.databaseName = arguments[0];
    this.tableName = arguments[1];
    this.dbConnectionPool = arguments[2];
    this.user_callback = arguments[3];
    
    this.getTableMetadata = function() {
      var metadataHandler = this;
      var pooledConnection;
      var dictionary;

      var metadataHandlerOnTableMetadata = function(err, tableMetadata) {
        udebug.log_detail('getTableMetadataHandler.metadataHandlerOnTableMetadata');
        // put back the pooledConnection
        if (typeof(pooledConnection) === 'undefined' || pooledConnection === null) {
          throw new Error('Fatal internal exception: got null for pooledConnection');
        }
        metadataHandler.dbConnectionPool.pooledConnections.push(pooledConnection);
        metadataHandler.user_callback(err, tableMetadata);
      };

      var metadataHandlerOnConnect = function() {
        udebug.log_detail('getTableMetadataHandler.metadataHandlerOnConnect');

        dictionary = new mysqlDictionary.DataDictionary(pooledConnection);
        dictionary.getTableMetadata(metadataHandler.databaseName, metadataHandler.tableName, 
            metadataHandlerOnTableMetadata);
      };
      udebug.log_detail('getTableMetadataHandler.getTableMetadata');
      if (this.dbConnectionPool.pooledConnections.length > 0) {
        // pop a connection from the pool
        pooledConnection = this.dbConnectionPool.pooledConnections.pop();
        dictionary = new mysqlDictionary.DataDictionary(pooledConnection);
        dictionary.getTableMetadata(databaseName, metadataHandler.tableName, user_callback);
      } else {
        pooledConnection = mysql.createConnection(this.dbConnectionPool.driverproperties);
        pooledConnection.connect(metadataHandlerOnConnect);
      }
    };

  };

  // getTableMetadata starts here
  // getTableMetadata = function(databaseName, tableName, dbSession, user_callback)
  var pooledConnection, dictionary;
  stats.incr("getTableMetadata");

  if (dbSession) {
    // dbSession exists; call the dictionary directly
    pooledConnection = dbSession.pooledConnection;
    dictionary = new mysqlDictionary.DataDictionary(pooledConnection);
    dictionary.getTableMetadata(databaseName, tableName, user_callback);
  } else {
    // dbSession does not exist; create a closure to handle it
    var handler = new TableMetadataHandler(databaseName, tableName, this, user_callback);
    handler.getTableMetadata();
  }
  
};

exports.DBConnectionPool.prototype.listTables = function(databaseName, dbSession, user_callback) {
  var ListTablesHandler = function() {
    this.databaseName = arguments[0];
    this.dbConnectionPool = arguments[1];
    this.user_callback = arguments[2];
    this.pooledConnection = null;
    
    this.listTables = function() {
      var listTablesHandler = this;

      var listTablesHandlerOnTableList = function(err, tableList) {
        udebug.log_detail('listTablesHandler.listTablesHandlerOnTableList');
        // put back the pooledConnection
        if (typeof(listTablesHandler.pooledConnection) === 'undefined') {
          throw new Error('Fatal internal exception: got undefined pooledConnection for createConnection');
        }
        if (listTablesHandler.pooledConnection === null) {
          throw new Error('Fatal internal exception: got null pooledConnection for createConnection');
        }
        listTablesHandler.dbConnectionPool.pooledConnections.push(listTablesHandler.pooledConnection);
        listTablesHandler.user_callback(err, tableList);
      };

      var listTablesHandlerOnConnect = function() {
        var dictionary;
        udebug.log_detail('listTablesHandler.listTablesHandlerOnConnect');

        dictionary = new mysqlDictionary.DataDictionary(listTablesHandler.pooledConnection);
        dictionary.listTables(listTablesHandler.databaseName, listTablesHandlerOnTableList);
      };

      udebug.log_detail('listTablesHandler.listTables');
      if (listTablesHandler.dbConnectionPool.pooledConnections.length > 0) {
        var dictionary;
        // pop a connection from the pool
        listTablesHandler.pooledConnection = listTablesHandler.connectionPool.pooledConnections.pop();
        dictionary = new mysqlDictionary.DataDictionary(listTablesHandler.pooledConnection);
        dictionary.listTables(listTablesHandler.databaseName, listTablesHandler.tableName, listTablesHandlerOnTableList);
      } else {
        listTablesHandler.pooledConnection = mysql.createConnection(listTablesHandler.dbConnectionPool.driverproperties);
        listTablesHandler.pooledConnection.connect(listTablesHandlerOnConnect);
      }
    };

  };

  // listTables starts here
  // listTables = function(databaseName, dbSession, user_callback)
  var pooledConnection, dictionary;
  stats.incr("listTables");
  
  if (dbSession) {
    // dbSession exists; call the dictionary directly
    pooledConnection = dbSession.pooledConnection;
    dictionary = new mysqlDictionary.DataDictionary(pooledConnection);
    dictionary.listTables(databaseName, user_callback);
  } else {
    // no dbSession; create a list table handler
    var handler = new ListTablesHandler(databaseName, this, user_callback);
    handler.listTables();
  }
  
};
