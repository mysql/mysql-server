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

var path           = require("path"),
    spi            = require(mynode.spi),
    adapter        = require(path.join(mynode.fs.build_dir, "ndb_adapter.node")).ndb,
    os             = require('os'),
    udebug         = unified_debug.getLogger('jscrund_null.js');

function implementation() {
  var b = new Buffer(4);
  this.inBatchMode   = false;
  this.batch         = null;
  this.buffers       = null;
  this.properties    = null;
  this.debug_msg     = "debug message ";
  this.bounds_helper = [ b,1,true,b,1,false,0 ]; // Index Bounds Helper Spec
};

implementation.prototype = {
};

implementation.prototype.getDefaultProperties = function() {
  return {
    mysql_user           : "root",  // For CREATE TABLE
    database             : "test",
    debug_messages       : 0,       // log_debug messages per operation
    debug_message_length : 20,      // length of debug messages
    ndbapi_calls         : 0,       // NDB API calls per operation
    system_calls         : 0,       // System calls per operation
    new_buffers          : 0,       // Allocate node Buffers per operation
    new_buffer_size      : 32,      // Size of allocation
  };
};

implementation.prototype.close = function(callback) {
  callback(null);
};

implementation.prototype.initialize = function(options, callback) {
  var n;
  spi.getDBServiceProvider("ndb");   // To call ndb_init()
  this.properties = options.properties;
  while(this.debug_msg.length < this.properties.debug_message_length) {
    this.debug_msg += "x";
  }
  callback(null);
};

implementation.prototype.execOneOperation = function(callback, value) {
  var b, n;

  /* Debug Messages */
  for(n = 0 ; n < this.properties.debug_messages ; n++) {
    udebug.log(this.debug_msg);
  }

  /* NDBAPI calls */
  /* Marshal/Unmarshal arguments and call into native code */
  for(n = 0 ; n < this.properties.ndbapi_calls ; n++) {
    adapter.impl.IndexBound.create(this.bounds_helper);
  }

  /* System calls */
  for(n = 0 ; n < this.properties.system_calls ; n++) {
    os.loadavg();          // calls sysctl() or sysinfo()
  }

  /* Buffers */
  for(n = 0 ; n < this.properties.new_buffers ; n++) {
    b = new Buffer(this.properties.new_buffer_size);
    if(this.inBatchMode) {
      this.buffers.push(b);
    }
  }

  /* Run operation or add it to batch */
  if(this.inBatchMode) {
    this.batch.push({ 'cb': callback, 'val': value });
  }
  else {
    setImmediate(callback, null, value);
  }
};

implementation.prototype.persist = function(parameters, callback) {
  this.execOneOperation(callback);
};

implementation.prototype.find = function(parameters, callback) {
  var object = {
    id :      parameters.key.id,
    cint:     parameters.key.id,
    clong:    parameters.key.id,
    cfloat:   parameters.key.id,
    cdouble:  parameters.key.id
  }
  this.execOneOperation(callback, object);
};

implementation.prototype.remove = function(parameters, callback) {
  this.execOneOperation(callback);
};

implementation.prototype.createBatch = function(callback) {
  this.batch = [];
  this.buffers = [];
  this.inBatchMode = true;
  callback(null);
};

implementation.prototype.executeBatch = function(callback) {
  var batch = this.batch;

  function runOperations() {
    var item;
    while((item = batch.pop())) {
      item.cb(null, item.val);
    }
    callback(null);
  }

  this.inBatchMode = false;
  setImmediate(runOperations);
};

implementation.prototype.begin = function(callback) {
  callback(null);
};

implementation.prototype.commit = function(callback) {
  callback(null);
};

exports.implementation = implementation;
