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

/*global unified_debug */

"use strict";

var userContext = require('../impl/common/UserContext.js'),
    udebug      = unified_debug.getLogger("Batch.js"),
    transaction = require('./Transaction.js');

function Batch(session) {
  this.session = session;
  this.operationContexts = [];
}


exports.Batch = Batch;

exports.Batch.prototype.getSession = function() {
  return this.session;
};


exports.Batch.prototype.getMapping = function(tableNameOrConstructor) {
  return this.session.getMapping(tableNameOrConstructor);
};

exports.Batch.prototype.clearError = new Error('Batch was cleared.');

exports.Batch.prototype.clear = function() {
  var batch = this;
  // clear all operations from the batch
  // if any pending operations, synchronously call each callback with an error
  // the transaction state is unchanged
  udebug.log_detail('Batch.clear with operationContexts.length ' + this.operationContexts.length);
  // for each context, extract the operation and clear it
  this.operationContexts.forEach(function(context) {
    // first, mark the context as cleared so if getTableHandler returns during a user callback,
    // we won't continue to create the operation
    context.clear = true;
    // now call the user callback with an error
    switch (context.returned_parameter_count) {
    case 1:
      context.applyCallback(batch.clearError);
      break;
    case 2:
      context.applyCallback(batch.clearError, null);
      break;
    default:
      throw new Error(
          'Fatal internal exception: wrong parameter count ' + context.returned_parameter_count +
          ' for Batch.clearBatch');
    }
  });
  // now clear the operations in this batch
  batch.operationContexts = [];
};


exports.Batch.prototype.find = function() {
  var context = new userContext.UserContext(arguments, 3, 2, this.session, this.session.sessionFactory, false);
  // delegate to context's find function for execution
  context.find();
  this.operationContexts.push(context);
};


exports.Batch.prototype.load = function() {
  var context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's load function for execution
  context.load();
  this.operationContexts.push(context);
};


exports.Batch.prototype.persist = function() {
  var context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's persist function for execution
  context.persist();
  this.operationContexts.push(context);
};


exports.Batch.prototype.remove = function() {
  var context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's remove function for execution
  context.remove();
  this.operationContexts.push(context);
};


exports.Batch.prototype.update = function() {
  var context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's update function for execution
  context.update();
  this.operationContexts.push(context);
};


exports.Batch.prototype.save = function() {
  var context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's save function for execution
  context.save();
  this.operationContexts.push(context);
};


exports.Batch.prototype.execute = function() {
  var context = new userContext.UserContext(arguments, 1, 1, this.session, this.session.sessionFactory, true);
  context.executeBatch(this.operationContexts);
  // clear the operations that have just been executed
  this.operationContexts = [];
};

exports.Batch.prototype.isBatch = function() {
  this.assertOpen();
  return true;
};

