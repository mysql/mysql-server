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
    udebug         = unified_debug.getLogger('jscrund_dbspi.js');

function implementation() {
};

implementation.prototype = {
  dbServiceProvider :  null,
  dbConnPool        :  null,
  dbSession         :  null,
  inBatchMode       :  false,  
  operations        :  null
};

implementation.prototype.getDefaultProperties = function(adapter) {
  this.dbServiceProvider = spi.getDBServiceProvider(adapter);
  return this.dbServiceProvider.getDefaultConnectionProperties();
};

implementation.prototype.close = function(callback) {
  var impl = this;
  impl.dbSession.close(function() { impl.dbConnPool.close(callback); });
};

implementation.prototype.initialize = function(options, callback) {
  udebug.log("initialize");
  var impl = this;
  var mappings = options.annotations;
  var nmappings = mappings.length;

  function getMapping(n) {
    function gotMapping(err, tableMetadata) {
      udebug.log("gotMapping", n);
      nmappings--;
      var dbt = new DBTableHandler(tableMetadata, mappings[n].prototype.mynode.mapping, 
                                   mappings[n]);
      udebug.log("Got DBTableHandler", dbt);
      mappings[n].dbt = dbt;
      if(nmappings == 0) {
        callback(null);  /* All done */
      }
    }

    impl.dbConnPool.getTableMetadata(options.properties.database, 
                                     mappings[n].prototype.mynode.mapping.table,
                                     impl.dbSession, gotMapping);
  }

  function onDbSession(err, dbSession) {
    var n;
    if(err) { callback(err, null); } 
    else {
      impl.dbSession = dbSession;
      if(mappings.length) {
        for(n = 0 ; n < mappings.length ; n++) { getMapping(n); }
      }
      else {
        callback(null);
      }
    }
  }
    
  function onConnect(err, dbConnectionPool) {
    impl.dbConnPool = dbConnectionPool;
    if(err) { callback(err, null); }
    else { 
      dbConnectionPool.getDBSession(1, onDbSession);
    }
  }
  
  impl.dbServiceProvider.connect(options.properties, onConnect);
};

implementation.prototype.execOneOperation = function(op, tx, callback, row) {
  if(this.inBatchMode) {
    this.operations.push(op);
  }
  else {
    tx.execute([op], function(err) { if(err) console.log("TX EXECUTE ERR:", err, row); });
  }
};

implementation.prototype.persist = function(parameters, callback) {
  udebug.log_detail('persist object:', parameters.object);
  var dbt = parameters.object.constructor.dbt;
  var tx = this.dbSession.getTransactionHandler();
  var op = this.dbSession.buildInsertOperation(dbt, parameters.object, tx, callback);
  this.execOneOperation(op, tx, callback, parameters.object);
};

implementation.prototype.find = function(parameters, callback) {
  udebug.log_detail('find key:', parameters.key);
  var dbt = parameters.object.constructor.dbt;
  var tx = this.dbSession.getTransactionHandler();
  var index = dbt.getIndexHandler(parameters.key, true);
  var op = this.dbSession.buildReadOperation(index, parameters.key, tx, callback);
  this.execOneOperation(op, tx, callback);
};

implementation.prototype.remove = function(parameters, callback) {
  udebug.log_detail('remove key:', parameters.key);
  var dbt = parameters.object.constructor.dbt;
  var tx = this.dbSession.getTransactionHandler();
  var index = dbt.getIndexHandler(parameters.key, true);
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
