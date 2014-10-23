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

var     udebug     = unified_debug.getLogger("Transaction.js");
var userContext    = require("./UserContext.js");

/** Transaction is implemented as a state machine. 
 * States are:
 * idle: user has not called begin
 * active user has called begin
 * rollback_only user has called begin and setRollbackOnly
 * 
 * Functions that cause state transitions are:
 * begin: begin a transaction if not already begun
 * rollback: rollback the current transaction
 * setRollbackOnly: allow rollback but not commit
 * commit: commit the current transaction
 * rollback: roll back the current transaction
 * 
 * Functions that return status are:
 * getRollbackOnly returns whether the state is rollback only
 * isActive returns whether the state is active
 * 
 * There are only three valid transitions:
 * 
 * (Idle) begin -> (Active) commit -> (Idle)
 * (Idle) begin -> (Active) rollback -> (Idle)
 * (Idle) begin -> (Active) setRollbackOnly -> (RollbackOnly) rollback -> (Idle)
 */

function Idle() {
  this.name = 'Idle';
}
var idle = new Idle();

function Active() {
  this.name = 'Active';
}
var active = new Active();

function RollbackOnly() {
  this.name = 'RollbackOnly';
}
var rollbackOnly = new RollbackOnly();

/** An error may have occurred. If there is a callback defined, signal the error via the callback,
 * and return the new transaction state. The state should remain the same (the current state
 * is passed in the function).
 * If no callback is defined with an error, throw the error (and remain in the current state).
 */
var callbackErrOrThrow = function(err, user_arguments) {
  var promise = new userContext.Promise();
  // signal the error via the promise
  if (err) {
    promise.reject(err);
  } else {
    promise.fulfill();
  }
   if (typeof(user_arguments[0]) === 'function') {
    var return_arguments = [];
    var i;
    for (i = 1; i < user_arguments.length; ++i) {
      return_arguments[i] = user_arguments[i];
    }
    return_arguments[0] = err;
    user_arguments[0].apply(null, return_arguments);
  }
 return promise;
};

Idle.prototype.begin = function(session, user_arguments) {
  udebug.log('Idle begin');
  // notify dbSession if they are interested
  if (typeof(session.dbSession.begin) === 'function') {
    session.dbSession.begin();
  }
  session.tx.setState(active);
  // no error
  return callbackErrOrThrow(null, user_arguments);
};

Idle.prototype.commit = function(session, user_arguments) {
  udebug.log('Idle commit');
  var err = new Error('Illegal state: Idle cannot commit.');
  err.sqlstate = '25000';
  return callbackErrOrThrow(err, user_arguments);
};

Idle.prototype.rollback = function(session, user_arguments) {
  udebug.log('Idle rollback');
  var err = new Error('Illegal state: Idle cannot rollback.');
  err.sqlstate = '25000';
  return callbackErrOrThrow(err, user_arguments);
};

Idle.prototype.isActive = function() {
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('Idle isActive');
  return false;
};

Idle.prototype.setRollbackOnly = function(session, user_arguments) {
  udebug.log('Idle setRollbackOnly');
};

Idle.prototype.getRollbackOnly = function() {
  udebug.log('Idle getRollbackOnly');
  return false;
};

Active.prototype.begin = function(session, user_arguments) {
  udebug.log('Active begin');
  var err = new Error('Illegal state: Active cannot begin.');
  err.sqlstate = '25000';
  return callbackErrOrThrow(err, user_arguments);
};

Active.prototype.commit = function(session, user_arguments) {
  udebug.log('Active commit');
  var context = new userContext.UserContext(user_arguments, 1, 1, session, session.sessionFactory);
  // delegate to context's commit for execution which will change the state to idle
  return context.commit();
};

Active.prototype.rollback = function(session, user_arguments) {
  udebug.log('Active rollback');
  var context = new userContext.UserContext(user_arguments, 1, 1, session, session.sessionFactory);
  // delegate to context's rollback for execution which will change the state to idle
  return context.rollback();
};

Active.prototype.isActive = function() {
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('Active isActive');
  return true;
};

Active.prototype.setRollbackOnly = function(session) {
  udebug.log('Active setRollbackOnly');
  session.tx.setState(rollbackOnly);
};

Active.prototype.getRollbackOnly = function() {
  udebug.log('Active getRollbackOnly');
  return false;
};

RollbackOnly.prototype.begin = function(session, user_arguments) {
  udebug.log('RollbackOnly begin');
  var err = new Error('Illegal state: RollbackOnly cannot begin.');
  err.sqlstate = '25000';
  return callbackErrOrThrow(err, user_arguments);
};

RollbackOnly.prototype.commit = function(session, user_arguments) {
  udebug.log('RollbackOnly commit');
  var err = new Error('Illegal state: RollbackOnly cannot commit.');
  err.sqlstate = '25000';
  return callbackErrOrThrow(err, user_arguments);
};

RollbackOnly.prototype.rollback = function(session, user_arguments) {
  udebug.log('RollbackOnly rollback');
  var context = new userContext.UserContext(user_arguments, 1, 1, session, session.sessionFactory);
  // delegate to context's rollback for execution which will set the state to idle
  return context.rollback();
};

RollbackOnly.prototype.isActive = function() {
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('RollbackOnly isActive');
  return true;
};

RollbackOnly.prototype.setRollbackOnly = function() {
  udebug.log('RollbackOnly setRollbackOnly');
};

RollbackOnly.prototype.getRollbackOnly = function() {
  udebug.log('RollbackOnly getRollbackOnly');
  return true;
};

function Transaction(session) {
  this.session = session;
  this.state = idle;
}

// States can be addressed by name in the Transaction object for setState(newState)
Transaction.prototype.idle = idle;
Transaction.prototype.active = active;
Transaction.prototype.rollbackOnly = rollbackOnly;

Transaction.prototype.begin = function() {
  udebug.log('Transaction.begin');
  return this.state.begin(this.session, arguments);
};

Transaction.prototype.commit = function() {
  udebug.log('Transaction.commit');
  return this.state.commit(this.session, arguments);
};

Transaction.prototype.rollback = function() {
  udebug.log('Transaction.rollback');
  return this.state.rollback(this.session, arguments);
};

Transaction.prototype.isActive = function() {
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('Transaction.isActive');
  return this.state.isActive();
};

Transaction.prototype.setRollbackOnly = function() {
  udebug.log('Transaction.setRollbackOnly');
  this.state.setRollbackOnly(this.session, arguments);
};

Transaction.prototype.getRollbackOnly = function() {
  udebug.log('Transaction.getRollbackOnly');
  return this.state.getRollbackOnly();
};

/** This function is used by each state to change the state of the transaction.
 * @param newState one of: Idle, Active, RollbackOnly
 */
Transaction.prototype.setState = function(newState) {
  udebug.log('Transaction.setState to', newState.name);
  this.state = newState;
};

exports.Transaction = Transaction;

