/*
 Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights
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

var userContext = require("./UserContext.js"),
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


exports.Batch.prototype.clearError = new Error('Batch was cleared.');

exports.Batch.prototype.clear = function() {
  var batch = this;
  // clear all operations from the batch
  // if any pending operations, synchronously call each callback with an error
  // the transaction state is unchanged
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('Batch.clear with operationContexts.length ' + this.operationContexts.length);
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
  var context, promise;
  context = new userContext.UserContext(arguments, 3, 2, this.session, this.session.sessionFactory, false);
  // delegate to context's find function for execution
  promise = context.find();
  this.operationContexts.push(context);
  return promise;
};


exports.Batch.prototype.load = function() {
  var context, promise;
  context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's load function for execution
  promise = context.load();
  this.operationContexts.push(context);
  return promise;
};


exports.Batch.prototype.persist = function(tableIndicator) {
  var context, promise;
  if (typeof(tableIndicator) === 'object') {
    // persist(domainObject, callback)
    context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  } else {
    // persist(tableNameOrConstructor, values, callback)
    context = new userContext.UserContext(arguments, 3, 1, this.session, this.session.sessionFactory, false);
  }
  // delegate to context's persist function for execution
  promise = context.persist();
  this.operationContexts.push(context);
  return promise;
};


exports.Batch.prototype.remove = function(tableIndicator) {
  var context, promise;
  if (typeof(tableIndicator) === 'object') {
    // remove(domainObject, callback)
    context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  } else {
    // remove(tableNameOrConstructor, keys, callback)
    context = new userContext.UserContext(arguments, 3, 1, this.session, this.session.sessionFactory, false);
  }
  // delegate to context's remove function for execution
  promise = context.remove();
  this.operationContexts.push(context);
  return promise;
};


exports.Batch.prototype.update = function(tableIndicator) {
  var context, promise;
  if (typeof(tableIndicator) === 'object') {
    // update(domainObject, callback)
    context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  } else {
    // update(tableNameOrConstructor, keys, values, callback)
    context = new userContext.UserContext(arguments, 4, 1, this.session, this.session.sessionFactory, false);
  }
  // delegate to context's update function for execution
  promise = context.update();
  this.operationContexts.push(context);
  return promise;
};


exports.Batch.prototype.save = function(tableIndicator) {
  var context, promise;
  if (typeof(tableIndicator) === 'object') {
    // save(domainObject, callback)
    context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  } else {
    // save(tableNameOrConstructor, values, callback)
    context = new userContext.UserContext(arguments, 3, 1, this.session, this.session.sessionFactory, false);
  }
  // delegate to context's save function for execution
  promise = context.save();
  this.operationContexts.push(context);
  return promise;
};


exports.Batch.prototype.execute = function() {
  var context = new userContext.UserContext(arguments, 1, 1, this.session, this.session.sessionFactory, true);
  var promise = context.executeBatch(this.operationContexts);
  // clear the operations that have just been executed
  this.operationContexts = [];
  return promise;
};

exports.Batch.prototype.isBatch = function() {
  this.assertOpen();
  return true;
};

exports.Batch.prototype.getOperationCount = function() {
  return this.operationContexts.length;
};

