/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

'use strict';

var spi            = require('../Adapter/impl/SPI.js'),
    dbt_module     = require('../Adapter/impl/common/DBTableHandler.js'),
    DBTableHandler = dbt_module.DBTableHandler,
    unified_debug  = require('../Adapter/api/unified_debug.js'),
    udebug         = unified_debug.getLogger('jscrund_dbspi.js')

function blah() {
  console.log("BLAH");
  console.log.apply(null, arguments);
  process.exit();
}

function implementation() {
};

implementation.prototype = {
  dbService      :    null,   // DBServiceProvider
  dbConnPool     :    null,
  dbSession      :    null,
  dbTableHandler :    null,
  operations     :    [],
  inBatchMode    :    false,  
};

implementation.prototype.close = function(callback) {
  var impl = this;
  impl.dbSession.close(function() { impl.dbConnPool.close(callback); });
};

implementation.prototype.initialize = function(options, callback) {
  var impl = this;
  var mapping = options.annotations.prototype.mynode.mapping;
  udebug.log("initalize() with mapping", mapping);

  impl.dbService = spi.getDBServiceProvider(options.adapter);

  function onMetadata(err, tableMetadata) {
    if(err) { callback(err); } 
    else {  
      var dbt = new DBTableHandler(tableMetadata, mapping, options.annotations);
      udebug.log("Got DBTableHandler", dbt);
      impl.dbTableHandler = dbt;
      callback(null);
    }
  }
    
  function onDbSession(err, dbSession) {
    if(err) { callback(err, null); } 
    else {
      impl.dbSession = dbSession;      
      impl.dbConnPool.getTableMetadata(options.properties.database, 
                                       mapping.table, dbSession, onMetadata);                                       
    }
  }
    
  function onConnect(err, dbConnectionPool) {
    impl.dbConnPool = dbConnectionPool;
    if(err) { callback(err, null); }
    else { 
      dbConnectionPool.getDBSession(1, onDbSession);
    }
  }
  
  impl.dbService.connect(options.properties, onConnect);
};

//
//  QUESTION:  When does jscrund expect to get the callback?
//  as an operation callback?  a transaction callback?  both? 
//
//
//


implementation.prototype.execOneOperation = function(op, tx, callback) {
  if(this.inBatchMode) {
    this.operations.push(op);
  }
  else {
    tx.execute([op], function(err) { if(err) console.log("TX EXECUTE ERR:", err); });
  }
}

implementation.prototype.persist = function(parameters, callback) {  
  udebug.log_detail('persist object:', parameters.object);
  var tx = this.dbSession.getTransactionHandler();
  var op = this.dbSession.buildInsertOperation(this.dbTableHandler, 
                                               parameters.object, tx, callback);
  this.execOneOperation(op, tx, callback);
};

implementation.prototype.find = function(parameters, callback) {
  udebug.log_detail('find key:', parameters.key);
  var tx = this.dbSession.getTransactionHandler();
  var index = this.dbTableHandler.getIndexHandler(parameters.key, true);
  var op = this.dbSession.buildReadOperation(index, parameters.key, tx, callback);
  this.execOneOperation(op, tx, callback);
};

implementation.prototype.remove = function(parameters, callback) {
  udebug.log_detail('remove key:', parameters.key);
  var tx = this.dbSession.getTransactionHandler();
  var index = this.dbTableHandler.getIndexHandler(parameters.key, true);
  var op = this.dbSession.buildDeleteOperation(index, parameters.key, tx, callback);
  this.execOneOperation(op, tx, callback);
};

implementation.prototype.createBatch = function(callback) {
  udebug.log_detail('createBatch');
  this.operations = [];
  this.inBatchMode = true;
  callback(null);
};

implementation.prototype.executeBatch = function(callback) {
  udebug.log_detail('executeBatch');
  this.inBatchMode = false;
  this.dbSession.getTransactionHandler().execute(this.operations, callback);  
};

implementation.prototype.begin = function(callback) {
  udebug.log_detail('begin');
  this.dbSession.begin();
  callback(null);
};

implementation.prototype.commit = function(callback) {
  udebug.log_detail('commit');
  this.dbSession.commit(callback);
};

exports.implementation = implementation;
