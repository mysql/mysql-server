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

"use strict";

var userContext = require('../impl/common/UserContext.js');

function Session(index, sessionFactory, dbSession) {
  this.index = index;
  this.sessionFactory = sessionFactory;
  this.dbSession = dbSession;
  this.closed = false;
}


exports.Session = Session;

exports.Session.prototype.getTableMetadata = function() {
  var context = new userContext.UserContext(arguments, 4, 2, this, this.sessionFactory);
  // delegate to context's getTableMetadata for execution
  context.getTableMetadata();
};


exports.Session.prototype.listTables = function() {
  var context = new userContext.UserContext(arguments, 2, 2, this, this.sessionFactory);
  // delegate to context's getTableMetadata for execution
  context.listTables();
};


exports.Session.prototype.find = function() {
  var context = new userContext.UserContext(arguments, 3, 2, this, this.sessionFactory);
  // delegate to context's find function for execution
  context.find();
};


exports.Session.prototype.load = function() {
  var context = new userContext.UserContext(arguments, 2, 2, this, this.sessionFactory);
  // delegate to context's load function for execution
  context.load();
};


exports.Session.prototype.persist = function() {
  var context = new userContext.UserContext(arguments, 2, 2, this, this.sessionFactory);
  // delegate to context's persist function for execution
  context.persist();
};


exports.Session.prototype.remove = function() {
  var context = new userContext.UserContext(arguments, 2, 2, this, this.sessionFactory);
  // delegate to context's remove function for execution
  context.remove();
};


exports.Session.prototype.update = function() {
  var context = new userContext.UserContext(arguments, 2, 2, this, this.sessionFactory);
  // delegate to context's update function for execution
  context.update();
};


exports.Session.prototype.save = function() {
  var context = new userContext.UserContext(arguments, 2, 2, this, this.sessionFactory);
  // delegate to context's save function for execution
  context.save();
};


exports.Session.prototype.close = function() {
  // delegate to db session close to clean up (actually close or return to pool)
  this.dbSession.close(this.index);
  // remove this session from session factory
  this.sessionFactory.closeSession(this.index);
  this.closed = true;
};


exports.Session.prototype.isBatch = function() {
  this.assertOpen();
  return false;
};


exports.Session.prototype.isClosed = function() {
  return this.closed;
};

