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

exports.DBSession.prototype.TransactionHandler = function(dbSession) {
  this.isOpen = true;
  this.dbSession = dbSession;
  this.execute = function(type, callback) {
    var err = new Error('not implemented: MySQLConnection.TransactionHandler.execute');
    callback(err, this);
  };

  this.close = function() {
  };

  this.executeNoCommit = function(operationsList, transactionExecuteCallback) {
    var transactionHandler = this;
    operationsList.forEach(function(operation) {
      operation.execute(transactionHandler.dbSession.pooledConnection, function() {
        console.log('MySQLConnection.transactionHandler.executeNoCommit callback');
      });
    });
  };

  this.executeCommit = function(operationsList, transactionExecuteCallback) {
    var transactionHandler = this;
    operationsList.forEach(function(operation) {
      operation.execute(transactionHandler.dbSession.pooledConnection, function() {
        console.log('MySQLConnection.transactionHandler.executeCommit callback');
      });
    });
  };
};


exports.DBSession.prototype.createTransaction = function() {
  var transactionHandler = new this.TransactionHandler(this);
  return transactionHandler;
};

function InsertOperation(sql, data, callback) {
  udebug.log('MySQLConnection.dbSession.InsertOperation with ' + util.inspect(sql) + ' ' + util.inspect(data));

  var op = this;
  this.sql = sql;
  this.data = data;
  this.callback = callback;
  this.result = {};
  this.result.error = 0;

  function onInsertError(err) {
    udebug.log(err);
    op.callback(err, null);
  }

  function onInsert(err) {
    if (err) {
      udebug.log('MySQLConnection.dbSession.InsertOperation err callback: ' + err);
      op.callback(err, null);
    } else {
      udebug.log('MySQLConnection.dbSession.InsertOperation NO ERROR callback.');
      op.callback(null, op);
    }
  }
  this.execute = function(connection) {
    connection.on('error', onInsertError);
    connection.query(this.sql, this.data, this.onInsert);
  };
}

function ReadOperation(sql, keys, callback) {
  console.log('MySQLConnection.dbSession.ReadOperation with ' + util.inspect(sql) + ' ' + util.inspect(keys));
  var op = this;

  function onRead(err, rows) {
    if (err) {
      udebug.log('MySQLConnection.dbSession.ReadOperation err callback: ' + err);
      op.callback(err, op);
    } else {
      if (rows.length > 1) {
        err = new Error('Too many results from read: ' + rows.length);
        op.callback(err, op);
      } else {
        udebug.log('MySQLConnection.dbSession.ReadOperation NO ERROR callback: ' + util.inspect(rows[0]));
        op.result.value = rows[0];
        op.callback(null, op);
      }
    }
  }
  this.sql = sql;
  this.keys = keys;
  this.callback = callback;
  this.result = {};
  this.result.error = 0;

  this.execute = function(connection, callback) {
    connection.query(this.sql, this.keys, onRead);
  };
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
  var insertSQL = 'INSERT INTO ' + dbTableHandler.dbTable.database + '.' + dbTableHandler.dbTable.name + ' (';
  var valuesSQL = ' VALUES (';
  var duplicateSQL = ' ON DUPLICATE KEY UPDATE ';
  var columns = dbTableHandler.getColumnMetadata();
  udebug.log_detail('MySQLConnection.getMetadata with columns', JSON.stringify(columns));
  // loop over the columns and extract the column name
  var columnSeparator = '';
  var duplicateSeparator = '';
  var i, column;  
  for (i = 0; i < columns.length; ++i) {
    column = columns[i];
    insertSQL += columnSeparator + column.name;
    valuesSQL += columnSeparator + '?';
    columnSeparator = ', ';
    if (!column.isInPrimaryKey) {
      duplicateSQL += duplicateSeparator + column.name + ' = VALUES (' + column.name + ') ';
      duplicateSeparator = ', ';
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
  var deleteSQL = 'DELETE FROM ' + dbTableHandler.dbTable.database + '.' + dbTableHandler.dbTable.name + ' WHERE ';
  // find the index metadata from the dbTableHandler index section
  // loop over the columns in the index and extract the column name
  var indexMetadatas = dbTableHandler.dbTable.indexes;
  var columns = dbTableHandler.getColumnMetadata();
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
  var whereSQL =   ' FROM ' + dbTableHandler.dbTable.database + '.' + dbTableHandler.dbTable.name + ' WHERE ';
  // loop over the mapped column names in order
  var separator = '';
  var i, j, column;
  var columns = dbTableHandler.getColumnMetadata();
  for (i = 0; i < columns.length; ++i) {
    column = columns[i];
    selectSQL += separator + column.name;
    separator = ', ';
  }

  // loop over the index columns
  // find the index metadata from the dbTableHandler index section
  // loop over the columns in the index and extract the column name
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
  udebug.log_detail('MySQLConnection.dbSession.buildInsertOperation with tableHandler: ' + util.inspect(dbTableHandler) +
      ' object: ' + util.inspect(object));
  getMetadata(dbTableHandler);
  var fields = dbTableHandler.getFields(object);
  var insertSQL = dbTableHandler.mysql.insertSQL;
  return new InsertOperation(insertSQL, fields, callback);
};


exports.DBSession.prototype.buildReadOperation = function(dbIndexHandler, keys, transaction, callback) {
  udebug.log_detail('MySQLConnection.dbSession.buildReadOperation with indexHandler: ' + util.inspect(dbIndexHandler) +
      util.inspect(keys));
  var dbTableHandler = dbIndexHandler.tableHandler;
  var fields = dbIndexHandler.getFields(keys);
  getMetadata(dbTableHandler);
  var selectSQL = dbTableHandler.mysql.selectSQL[dbIndexHandler.dbIndex.name];
  return new ReadOperation(selectSQL, fields, callback);
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
