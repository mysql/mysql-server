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

"use strict";

var path            = require("path"),
    adapter         = require(path.join(mynode.fs.build_dir, "ndb_adapter.node")).ndb,
    stats_module    = require(mynode.api.stats),
    QueuedAsyncCall = require(mynode.common.QueuedAsyncCall).QueuedAsyncCall,
    udebug          = unified_debug.getLogger("NdbAutoIncrement.js");


/** NdbAutoIncrementCache 
    Wraps an Ndb object which manages (and caches) auto-inc values for a table.
    The API is that first you call prefetch(n) indicating how many values you
    want. Then you call getValue() once for each desired value.
*/

function NdbAutoIncrementCache(table) {
  udebug.log("New cache for table", table.name);
  this.table = table;
  this.impl = table.per_table_ndb;
  this.execQueue = [];
}

NdbAutoIncrementCache.prototype = {
  table         : null,
  impl          : null,
  execQueue     : null,
  batch_size    : 1
};

NdbAutoIncrementCache.prototype.prefetch = function(n) {
  this.batch_size += n;
};

NdbAutoIncrementCache.prototype.getValue = function(callback) {
  var cache = this;
  var apiCall = new QueuedAsyncCall(this.execQueue, callback);
  udebug.log("NdbAutoIncrementCache getValue table:", this.table.name, 
             "queue:", this.execQueue.length, "batch:", this.batch_size);
  apiCall.description = "AutoIncrementCache getValue";
  apiCall.batch_size = this.batch_size;
  apiCall.run = function() {
    adapter.impl.getAutoIncrementValue(cache.impl, cache.table, this.batch_size,
                                       this.callback);
    cache.batch_size--;
  };
  apiCall.enqueue();
};


/* TODO: This now creates an autoIncrementCache even for tables with no
   auto-inc columns; maybe don't do that.
*/
function getAutoIncCacheForTable(table) {
  if(table.per_table_ndb) {
    table.autoIncrementCache = new NdbAutoIncrementCache(table);
  }
}


/** NdbAutoIncrementHandler 
    This provides a service to NdbTransactionHandler.execute() 
    which, given a list of operations, may need to populate them with 
    auto-inc values before executing.
*/

function NdbAutoIncrementHandler(operations) {
  var i, op;
  this.autoinc_op_list = [];
  for(i = 0 ; i < operations.length ; i++) { 
    op = operations[i];
    if(typeof op.tableHandler.autoIncColumnNumber === 'number' && op.needAutoInc) {
      this.values_needed++;    
      this.autoinc_op_list.push(op);
      op.tableHandler.dbTable.autoIncrementCache.prefetch(1);
    }
  }
  udebug.log("New Handler",this.autoinc_op_list.length,"/",operations.length);
}

NdbAutoIncrementHandler.prototype = {
  values_needed   : 0,
  autoinc_op_list : null,
  final_callback  : null,
  errors          : 0
};


function makeOperationCallback(handler, op) {
  return function(err, value) {
    udebug.log("getValue operation callback value:", value, "qlen:",
               op.tableHandler.dbTable.autoIncrementCache.execQueue.length);
    handler.values_needed--;
    if(value > 0) {
      op.result.insert_id = value;
      op.tableHandler.setAutoincrement(op.values, value);
    }
    else {
      handler.erorrs++;
    }
    if(handler.values_needed === 0) {
      handler.dispatchFinalCallback();
    }
  };
}


NdbAutoIncrementHandler.prototype.dispatchFinalCallback = function() {
  this.final_callback(this.errors, this);
};


NdbAutoIncrementHandler.prototype.getAllValues = function(callback) {
  var i, op, cache;
  this.final_callback = callback;
  for(i = 0 ; i < this.autoinc_op_list.length ; i++) {
    op = this.autoinc_op_list[i];
    cache = op.tableHandler.dbTable.autoIncrementCache;
    cache.getValue(makeOperationCallback(this, op));
  }
};
  

exports.getCacheForTable = getAutoIncCacheForTable;
exports.AutoIncHandler = NdbAutoIncrementHandler;

