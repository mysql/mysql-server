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


exports.DBSession.prototype.createTransaction = function() {
  this.transactionHandler = new this.TransactionHandler();
  return this.transactionHandler;
};

function InsertOperation(sql, data) {
  this.sql = sql;
  this.data = data;
}

function ReadOperation(sql, keys) {
  this.sql = sql;
  this.keys = keys;
}

function getMetadata(dbTableHandler) {
  if (dbTableHandler.mysql) {
    return;
  }
  udebug.log_detail('MySQLConnection.getMetadata with dbTableHandler ' + JSON.stringify(dbTableHandler));
  dbTableHandler.mysql = {};
  dbTableHandler.mysql.indexes = {};
  dbTableHandler.mysql.deleteSQL = {};
  dbTableHandler.mysql.selectSQL = {};
  createInsertSQL(dbTableHandler);
  var i, indexes, index;
  // create a delete statement and select statement per index
  indexes = dbTableHandler.dbTable.indexes;
  for (i = 0; i < indexes.length; ++i) {
    index = dbTableHandler.dbTable.indexes[i];
    dbTableHandler.mysql.deleteSQL[index.name] = createDeleteSQL(dbTableHandler, index.name);
    dbTableHandler.mysql.selectSQL[index.name] = createSelectSQL(dbTableHandler, index.name);
  }
}

function createInsertSQL(dbTableHandler) {
  // create the insert SQL statement from the table metadata
  var insertSQL = 'INSERT INTO ' + dbTableHandler.mapping.database + '.' + dbTableHandler.mapping.name + ' (';
  var valuesSQL = ' VALUES (';
  var duplicateSQL = ' ON DUPLICATE KEY UPDATE ';
  var columns = dbTableHandler.dbTable.columns;
  udebug.log_detail('MySQLConnection.getMetadata with columns ' + JSON.stringify(columns));
  // loop over the columns and extract the column name
  var columnSeparator = '';
  var duplicateSeparator = '';
  var i, column, field;
  var fields = dbTableHandler.mapping.fields;
  for (i = 0; i < fields.length; ++i) {
    field = fields[i];
    if (!field.notPersistent) {
      insertSQL += columnSeparator + field.columnName;
      valuesSQL += columnSeparator + '?';
      columnSeparator = ', ';
      if (!columns[field.columnNumber].isInPrimaryKey) {
        duplicateSQL += duplicateSeparator + field.columnName + ' = VALUES (' + field.columnName + ') ';
        duplicateSeparator = ', ';
      }
    }
  }
  valuesSQL += ')';
  insertSQL += ')' + valuesSQL;
  dbTableHandler.mysql.insertSQL = insertSQL;
  dbTableHandler.mysql.duplicateSQL = insertSQL + duplicateSQL;
  udebug.log('MySQLConnection.getMetadata insertSQL: ' + dbTableHandler.mysql.insertSQL);
  udebug.log('MySQLConnection.getMetadata duplicateSQL: ' + dbTableHandler.mysql.duplicateSQL);
  return insertSQL;
}

function createDeleteSQL(dbTableHandler, index) {
  // create the delete SQL statement from the table metadata for the named index
  var deleteSQL = 'DELETE FROM ' + dbTableHandler.mapping.database + '.' + dbTableHandler.mapping.name + ' WHERE ';
  // find the index metadata from the dbTableHandler index section
  // loop over the columns in the index and extract the column name
  var indexMetadatas = dbTableHandler.dbTable.indexes;
  var columns = dbTableHandler.dbTable.columns;
  var separator = '';
  var i, j, indexMetadata;
  for (i = 0; i < indexMetadatas.length; ++i) {
    if (indexMetadatas[i].name === index) {
      indexMetadata = indexMetadatas[i];
      udebug.log('MySQLConnection.createDeleteSQL indexMetadata: ' + JSON.stringify(indexMetadata));
      for (j = 0; j < indexMetadata.columnNumbers.length; ++j) {
        deleteSQL += separator + columns[indexMetadata.columnNumbers[j]].name + ' = ?';
        separator = ' AND ';
      }
    }
  }
  udebug.log('MySQLConnection.getMetadata deleteSQL for ' + index + ': ' + deleteSQL);
  return deleteSQL;
}

function createSelectSQL(dbTableHandler, index) {
  // create the select SQL statement from the table metadata for the named index
  var selectSQL = 'SELECT ';
  var whereSQL =   ' FROM ' + dbTableHandler.mapping.database + '.' + dbTableHandler.mapping.name + ' WHERE ';
  // loop over the mapped column names in order
  var separator = '';
  var i, j, field;
  var fields = dbTableHandler.mapping.fields;
  for (i = 0; i < fields.length; ++i) {
    field = fields[i];
    if (!field.notPersistent) {
      selectSQL += separator + field.columnName;
      separator = ', ';
    }
  }

  // loop over the index columns
  // find the index metadata from the dbTableHandler index section
  // loop over the columns in the index and extract the column name
  var columns = dbTableHandler.dbTable.columns;
  var indexMetadatas = dbTableHandler.dbTable.indexes;
  separator = '';
  for (i = 0; i < indexMetadatas.length; ++i) {
    if (indexMetadatas[i].name === index) {
      var indexMetadata = indexMetadatas[i];
      for (j = 0; j < indexMetadata.columnNumbers.length; ++j) {
        whereSQL += separator + columns[indexMetadata.columnNumbers[j]].name + ' = ? ';
        separator = ' AND ';
      }
    }
  }
  selectSQL += whereSQL;
  udebug.log('MySQLConnection.getMetadata selectSQL for ' + index + ': ' + selectSQL);
  return selectSQL;
}

exports.DBSession.prototype.buildInsertOperation = function(dbTableHandler, object, transaction, callback) {
  getMetadata(dbTableHandler);
  var insertSQL = dbTableHandler.mysql.insertSQL;
//  var insertData = dbTableHandler.getInsertData(object);
  return new InsertOperation(insertSQL, object);
};


exports.DBSession.prototype.buildReadOperation = function(dbTableHandler, keys, transaction, callback) {
  getMetadata(dbTableHandler);
  var selectSQL = dbTableHandler.mysql.selectSQL;
//  var insertData = dbTableHandler.getInsertData(object);
  return new ReadOperation(selectSQL, keys);
};


exports.DBSession.prototype.closeSync = function() {
  if (this.pooledConnection) {
    this.pooledConnection.end();
    this.pooledConnection = null;
  }
};


exports.DBSession.prototype.close = function(callback) {
  if (this.pooledConnection) {
    this.pooledConnection.end();
    this.pooledConnection = null;
  }
  if (callback) {
    callback(null, null);
  }
};


exports.DBSession.prototype.getConnectionPool = function() {
  return this.connectionPool;
};
