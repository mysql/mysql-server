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
var DEBUG, DETAIL;

var implementation = function() {
};

implementation.prototype.getDefaultProperties = function(adapter) {
  return new mynode.ConnectionProperties(adapter);
};

implementation.prototype.close = function(callback) {
  if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.close', this);
  this.session.close(callback);
};

implementation.prototype.initialize = function(options, callback) {
  var impl = this;

  // Deferred initialization of file-level DEBUG and DETAIL
  DEBUG = JSCRUND.udebug.is_debug();
  DETAIL = JSCRUND.udebug.is_detail();

  // Set options
  this.options = options;

  if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.initialize', this);

  // set up the session
  mynode.openSession(options.properties, options.annotations, function(err, session) {
    if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.initialize callback err:', err);
    if (err) {
      console.log(err);
      process.exit(1);
    }
    impl.session = session;   // Initialize session
    impl.context = session;   // This will be changed to batch in createBatch
    impl.batch   = null;      // This will be set in createBatch
    callback(err);
  });
};

implementation.prototype.persist = function(parameters, callback) {
  if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.persist object:', parameters.object);
  // account for object construction
  var o = new parameters.object.constructor();
  o.init(parameters.object.id);
  this.context.persist(o, function(err) {
    callback(err);
  });
};

implementation.prototype.find = function(parameters, callback) {
  if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.find key:', parameters.key);
  this.context.find(parameters.object.constructor, parameters.key, function(err, found) {
    parameters.object.verify(found);
    callback(err);
  });
};

implementation.prototype.remove = function(parameters, callback) {
  if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.remove key:', parameters.key);
  this.context.remove(parameters.object.constructor, parameters.key, function(err) {
    callback(err);
  });
};

implementation.prototype.setVarchar = function(parameters, callback) {
  if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.setVarchar key:', parameters.key);
   parameters.object.cvarchar_def = this.options.B_varchar_value;
   this.context.update(parameters.object, function(err) {
    callback(err);
  });
};

implementation.prototype.clearVarchar = function(parameters, callback) {
   if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.clearVarchar key:', parameters.key);
  parameters.object.cvarchar_def = null;
  this.context.update(parameters.object, function(err) {
    callback(err);
  });
};

implementation.prototype.createBatch = function(callback) {
  if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.createBatch');
  this.batch = this.session.createBatch();
  this.context = this.batch;
  callback(null);
};

implementation.prototype.executeBatch = function(callback) {
  JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.executeBatch');
  this.context.execute(function(err) {
    callback(err);
  });
  this.context = this.session;
};

implementation.prototype.begin = function(callback) {
  if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.begin');
  // begin() signals programming error by exception if no callback provided
  this.session.currentTransaction().begin(function(err) {
    callback(err);
  });
};

implementation.prototype.commit = function(callback) {
  if(DETAIL) JSCRUND.udebug.log_detail('jscrund_mysqljs implementation.commit');
  this.session.currentTransaction().commit(function(err) {
    callback(err);
  });
};

exports.implementation = implementation;
