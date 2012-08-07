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

function Session(index, sessionFactory, dbSession) {
  this.index = index;
  this.sessionFactory = sessionFactory;
  this.dbSession = dbSession;
  this.closed = false;
}


exports.Session = Session;

exports.Session.prototype.find = function() {
  this.assertOpen();
  // delegate to db session's method with the same arguments we were passed
  this.dbSession.find.apply(this.dbSession, arguments);
};


exports.Session.prototype.load = function() {
  this.assertOpen();
  // delegate to db session's method with the same arguments we were passed
  this.dbSession.load.apply(this.dbSession, arguments);
};


exports.Session.prototype.persist = function() {
  this.assertOpen();
  // delegate to db session's method with the same arguments we were passed
  this.dbSession.persist.apply(this.dbSession, arguments);
};


exports.Session.prototype.remove = function() {
  this.assertOpen();
  // delegate to db session's method with the same arguments we were passed
  this.dbSession.remove.apply(this.dbSession, arguments);
};


exports.Session.prototype.update = function() {
  this.assertOpen();
  // delegate to db session's method with the same arguments we were passed
  this.dbSession.update.apply(this.dbSession, arguments);
};


exports.Session.prototype.save = function() {
  this.assertOpen();
  // delegate to db session's method with the same arguments we were passed
  this.dbSession.save.apply(this.dbSession, arguments);
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


Session.prototype.assertOpen = function() {
  if (this.closed) throw new Error('Error: Session is closed');
};

