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

#ifndef MYSQL_TRANSACTION_DELEGATE_CONTROL
#define MYSQL_TRANSACTION_DELEGATE_CONTROL

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/mysql_transaction_delegate_control.h>

/**
  @class mysql_new_transaction_control_imp
  This class is the implementation of service mysql_new_transaction_control.

  @sa service mysql_new_transaction_control
*/
class mysql_new_transaction_control_imp {
 public:
  /**
    Method to stop new incoming transactions allowing some management queries
    to run. New incoming transactions are rolled back.
  */
  static DEFINE_METHOD(void, stop, ());

  /**
    Method that allows the transactions which were earlier stopped by
    stop method.
  */
  static DEFINE_METHOD(void, allow, ());
};

/**
  @class mysql_before_commit_transaction_control_imp
  This class is the implementation of service
  mysql_before_commit_transaction_control.

  @sa service mysql_before_commit_transaction_control
*/
class mysql_before_commit_transaction_control_imp {
 public:
  /**
    Method rollback any transaction that reaches the commit stage.
  */
  static DEFINE_METHOD(void, stop, ());

  /**
    Method re-allows the commit, earlier stopped in stop function.
  */
  static DEFINE_METHOD(void, allow, ());
};

/* clang-format off */
/**
  @class mysql_close_connection_of_binloggable_transaction_not_reached_commit_imp
  This class is the implementation of service
  mysql_close_connection_of_binloggable_transaction_not_reached_commit.

  @sa service
  mysql_close_connection_of_binloggable_transaction_not_reached_commit
*/
/* clang-format on */
class mysql_close_connection_of_binloggable_transaction_not_reached_commit_imp {
 public:
  /**
    Method that gracefully closes the client connection which are running a
    binloggable transactions that did not reach the commit stage.
  */
  static DEFINE_METHOD(void, close, ());
};

#endif
