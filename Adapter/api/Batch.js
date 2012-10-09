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
  this.operations = [];
}


exports.Batch = Batch;

exports.Batch.prototype.getSession = function() {
  return this.session;
};

exports.Batch.prototype.find = function() {
  var context = new userContext.UserContext(arguments, 3, 2, this.session, this.session.sessionFactory, false);
  // delegate to context's find function for execution
  context.find();
  this.operations.push(context.operation);
};


exports.Batch.prototype.load = function() {
  var context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's load function for execution
  context.load();
  this.operations.push(context.operation);
};


exports.Batch.prototype.persist = function() {
  var context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's persist function for execution
  context.persist();
  this.operations.push(context.operation);
};


exports.Batch.prototype.remove = function() {
  var context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's remove function for execution
  context.remove();
  this.operations.push(context.operation);
};


exports.Batch.prototype.update = function() {
  var context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's update function for execution
  context.update();
  this.operations.push(context.operation);
};


exports.Batch.prototype.save = function() {
  var context = new userContext.UserContext(arguments, 2, 1, this.session, this.session.sessionFactory, false);
  // delegate to context's save function for execution
  context.save();
  this.operations.push(context.operation);
};


exports.Batch.prototype.execute = function() {
  var context = new userContext.UserContext(arguments, 1, 1, this.session, this.session.sessionFactory, true);
  context.executeBatch(this.operations);
};

exports.Batch.prototype.isBatch = function() {
  this.assertOpen();
  return true;
};

