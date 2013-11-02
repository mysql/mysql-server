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

var unified_debug  = require('../Adapter/api/unified_debug.js'),
    udebug         = unified_debug.getLogger('jscrund_null.js');

function implementation() {
};

implementation.prototype = {
  inBatchMode : false,
  batch       : null,
  properties  : null,
  debug_msg   : "debug message "
};

implementation.prototype.getDefaultProperties = function() {
  return {
    mysql_user           : "root",  // For CREATE TABLE
    database             : "test",
    debug_messages       : 0,       // log_debug messages per operation
    debug_message_length : 20,      // length of debug messages
  };
};

implementation.prototype.close = function(callback) {
  callback(null);
};

implementation.prototype.initialize = function(options, callback) {
  var n;
  this.properties = options.properties;
  while(this.debug_msg.length < this.properties.debug_message_length) {
    this.debug_msg += "x";
  }
  callback(null);
};

implementation.prototype.execOneOperation = function(callback, value) {
  for(var n = 0 ; n < this.properties.debug_messages ; n++) {
    udebug.log(this.debug_msg);
  }

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
