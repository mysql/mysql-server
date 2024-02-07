/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_TRANSACTION_DELEGATE_CONTROL_H
#define MYSQL_TRANSACTION_DELEGATE_CONTROL_H
#include <mysql/components/service.h>

/**
  A service to manage transactions execution.
  The service will stop new incoming transactions to execute.

  @note Some management related queries are allowed.

  @sa @ref mysql_new_transaction_control_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_new_transaction_control)

/**
  Method to stop new incoming transactions allowing some management queries
  to run. New incoming transactions are rolled back.

  @sa mysql_new_transaction_control_imp
*/
DECLARE_METHOD(void, stop, ());

/**
  Method that allows the transactions which were earlier stopped by
  stop method.

  @sa mysql_new_transaction_control_imp
*/
DECLARE_METHOD(void, allow, ());

END_SERVICE_DEFINITION(mysql_new_transaction_control)

/**
  A service to manage transactions execution.
  This service rollbacks the transactions that reach the before_commit state.
  This service does not impact transactions that are already in the commit
  stage.

  @sa @ref mysql_before_commit_transaction_control_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_before_commit_transaction_control)

/**
  This method rollback any transaction that reaches the commit stage.

  @sa mysql_before_commit_transaction_control_imp
*/
DECLARE_METHOD(void, stop, ());

/**
  Method re-allows the commit, earlier stopped in stop function.

  @note Flag set in stop function to rollback the transactions in commit is
        unset.

  @sa mysql_before_commit_transaction_control_imp
*/
DECLARE_METHOD(void, allow, ());

END_SERVICE_DEFINITION(mysql_before_commit_transaction_control)

/**
  This service will gracefully close all the client connections which are
  running a binloggable transaction that did not reach the commit stage.
  The term `bingloggable transactions` is used to identify transactions that
  will be written to the binary log once they do commit.
  At present when binlog cache is initialized by a transaction, the transaction
  is marked as `bingloggable`.

  @sa @ref
  mysql_close_connection_of_binloggable_transaction_not_reached_commit_imp
*/
BEGIN_SERVICE_DEFINITION(
    mysql_close_connection_of_binloggable_transaction_not_reached_commit)

/**
  Method that gracefully closes all the client connections which are running a
  binloggable transaction that did not reach the commit stage.

  @note method sets the killed flag with value THD::KILL_CONNECTION in the THD
  to gracefully KILL the transaction and client connection.

  @sa mysql_close_connection_of_binloggable_transaction_not_reached_commit_imp
*/
DECLARE_METHOD(void, close, ());

END_SERVICE_DEFINITION(
    mysql_close_connection_of_binloggable_transaction_not_reached_commit)

#endif /* MYSQL_TRANSACTION_DELEGATE_CONTROL_H */
