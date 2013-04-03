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

var mynode = require('..');

var implementation = function() {

  // the session is initialized in function initialize
  var session = null;
  // the batch is initialized in createBatch
  var batch = null;
  // the context is initialized to session and changed to batch in createBatch
  var context = null;

};

implementation.prototype.close = function(callback) {
  this.session.close(callback);
};

implementation.prototype.initialize = function(options, callback) {
  JSCRUND.udebug.log_detail('jscrund_mysqljs.initialize', this);
  var impl = this;
  // set up the session
  mynode.openSession(options.properties, options.annotations, function(err, session) {
    impl.session = session;
    impl.context = session;
    session.getMapping(options.annotations, function(a,b) { callback(a,b); });
  });
};

implementation.prototype.persist = function(parameters, callback) {
  JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.insert object:', parameters.object);
  this.context.persist(parameters.object, function(err) {
    JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.insert callback err:', err);
    callback(err);
  });
};

implementation.prototype.find = function(parameters, callback) {
  JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.find key:', parameters.key);
  this.context.find(parameters.object.constructor, parameters.key, function(err, found) {
    JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.find callback err:', err);
    callback(err);
  });
};

implementation.prototype.remove = function(parameters, callback) {
  JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.find key:', parameters.key);
  this.context.remove(parameters.object.constructor, parameters.key, function(err) {
    JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.remove callback err:', err);
    callback(err);
  });
};

implementation.prototype.createBatch = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.createBatch');
  this.batch = this.session.createBatch();
  this.context = this.batch;
  callback(null);
};

implementation.prototype.executeBatch = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.begin');
  this.context.execute(callback);
  this.context = this.session;
};

implementation.prototype.begin = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.begin');
  this.session.currentTransaction().begin();
  callback(null);
};

implementation.prototype.commit = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.commit');
  this.session.currentTransaction().commit(callback);
};

exports.implementation = implementation;
