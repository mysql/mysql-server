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

var session = require("./Session.js");

var SessionFactory = function(key) {
  this.key = key;
  this.dbconnection = {};
  this.annotations = {};
  this.properties = {};
};


//openSession(Annotations annotations, Function(Object error, Session session, ...) callback, ...);
// Open new session or get one from a pool
SessionFactory.prototype.openSession = function(annotations, user_callback) {
  var newSession = new session.Session();
  newSession.connection = this.dbconnection;
  user_callback(null, newSession);  // todo: extras
};

SessionFactory.prototype.close = function() {
  // TODO: close all sessions first
  if (this.dbconnection != null) {
    this.dbconnection.closeSync();
    this.dbconnection = null;
  }
  if (debug) console.log('SessionFactory.close ' + this.key);
  this.delete_callback(this.key);
};


exports.SessionFactory = SessionFactory;
