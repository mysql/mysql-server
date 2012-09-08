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

"use strict";

var mysql = require("mysql");

/** MySQLConnection wraps a mysql connection and implements the DBSession contract */
exports.DBSession = function(pooledConnection, connectionPool) {
  if (arguments.length !== 2) {
    throw new Error('Fatal internal exception: expected 2 arguments; got ' + arguments.length);
  } else {
    this.pooledConnection = pooledConnection; 
    this.connectionPool = connectionPool;
  }
};

exports.DBSession.prototype.TransactionHandler = function() {
  this.isOpen = true;
  this.execute = function(type, callback) {
    var err = new Error('not implemented: MySQLConnection.TransactionHandler.execute');
    callback(err, this);
  };
  this.close = function() {
  };
};


exports.DBSession.prototype.openTransaction = function() {
  this.transactionHandler = new this.TransactionHandler();
  return this.transactionHandler;
};

function InsertOperation(sql, data) {
  this.sql = sql;
  this.data = data;
}

function getMetadata(dbTableHandler) {
  if (dbTableHandler.mysql) {
    return;
  }
  udebug.log_detail('MySQLConnection.getMetadata with dbTableHandler ' + JSON.stringify(dbTableHandler));
  // create the insert SQL statement from the table metadata
  var insertSQL = 'INSERT INTO ' + dbTableHandler.mapping.database + '.' + dbTableHandler.mapping.name + '(';
  var valuesSQL = 'VALUES (';
  // get the fields
  var fields = dbTableHandler.mapping.fields;
  udebug.log_detail('MySQLConnection.getMetadata with fields ' + typeof(fields) + ' ' + JSON.stringify(fields));
  // loop over the fields and extract the column name
  var separator = '';
  var i;
  for (i = 0; i < fields.length; ++i) {
    var field = fields[i];
    insertSQL += separator + field.columnName;
    valuesSQL += separator + '?';
    separator = ', ';
  }
  valuesSQL += ')';
  insertSQL += ')' + valuesSQL;
  udebug.log('MySQLConnection.getMetadata insertSQL: ' + insertSQL);
  dbTableHandler.mysql = {};
  dbTableHandler.mysql.insertSQL = insertSQL;
}

exports.DBSession.prototype.buildInsertOperation = function(dbTableHandler, object) {
  getMetadata(dbTableHandler);
  var insertSQL = dbTableHandler.mysql.insertSQL;
//  var insertData = dbTableHandler.getInsertData(object);
  return new InsertOperation(insertSQL, null);
};


exports.DBSession.prototype.closeSync = function() {
  if (this.pooledConnection) {
    this.pooledConnection.end();
    this.pooledConnection = null;
  }
};


exports.DBSession.prototype.getConnectionPool = function() {
  return this.connectionPool;
};
