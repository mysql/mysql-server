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

#include "mysql_transaction_delegate_control_imp.h"  // THIS FILE

#include "mutex_lock.h"              // MUTEX_LOCK
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/rpl_handler.h"         // RUN_HOOK
#include "sql/sql_class.h"           // THD_CHECK

DEFINE_METHOD(void, mysql_new_transaction_control_imp::stop, ()) {
  (void)RUN_HOOK(transaction, set_transactions_at_begin_must_fail, ());
}

DEFINE_METHOD(void, mysql_new_transaction_control_imp::allow, ()) {
  (void)RUN_HOOK(transaction, set_no_restrictions_at_transaction_begin, ());
}

class Close_connection_all_transactions_that_begin : public Do_THD_Impl {
 public:
  void operator()(THD *thd_to_close) override {
    MUTEX_LOCK(lock, &thd_to_close->LOCK_thd_data);
    THD *thd = thd_to_close;  // TX_TRACKER_GET has a embedded thd
    TX_TRACKER_GET(is_transaction_explicit);
    /**
      Super user connection is also disconnected.
      1. If THD killed flag is already set, do not over-ride it because query
         was supposed to be rolled back, we will end up overriding the decision
         resulting in closing the connection.
      2. If THD life cycle has finished do not kill the transaction.
      3. If THD has error, do not kill the transaction, it will be rolled back.
      4. REPLICA thread should not be running but yet check it. Do not kill
         REPLICA transactions.
      5. Transaction should not be binloggable, so check we are >
         TX_RPL_STAGE_CACHE_CREATED
      6. Do not close connection of committing transaction, so check <
         TX_RPL_STAGE_BEFORE_COMMIT
      7. TX_RPL_STAGE_BEFORE_ROLLBACK is not required, transaction is being
         rolledback, no need to close connection
      8. Kill all explicit transactions which are not committing because change
         primary UDF blocks on explicit transactions
    */
    if (thd_to_close->killed == THD::NOT_KILLED &&
        !thd_to_close->slave_thread && !thd->is_being_disposed() &&
        !thd->is_error() &&
        ((thd_to_close->rpl_thd_ctx.get_tx_rpl_delegate_stage_status() >=
              Rpl_thd_context::TX_RPL_STAGE_CACHE_CREATED &&
          thd_to_close->rpl_thd_ctx.get_tx_rpl_delegate_stage_status() <
              Rpl_thd_context::TX_RPL_STAGE_BEFORE_COMMIT) ||
         (((is_transaction_explicit->get_trx_state() & TX_EXPLICIT) > 0) &&
          thd_to_close->rpl_thd_ctx.get_tx_rpl_delegate_stage_status() <
              Rpl_thd_context::TX_RPL_STAGE_BEFORE_COMMIT))) {
      thd_to_close->awake(THD::KILL_CONNECTION);
    }
  }
};

DEFINE_METHOD(
    void,
    mysql_close_connection_of_binloggable_transaction_not_reached_commit_imp::
        close,
    ()) {
  Close_connection_all_transactions_that_begin close;
  Global_THD_manager *thd_manager = Global_THD_manager::get_instance();
  /**
    Close all the client connections which is running a binloggable transaction
    that have yet not reached the before_commit stage.
  */
  thd_manager->do_for_all_thd(&close);
}

DEFINE_METHOD(void, mysql_before_commit_transaction_control_imp::stop, ()) {
  /**
    Rollback the transactions that have passed the begin hook but have yet not
    reached the before_commit stage. There can be some transactions that are yet
    not known as binloggable, so the THD::KILL_CONNECTION is not set.
    This function sets a flag to rollback the transactions.
  */
  RUN_HOOK(transaction, set_transactions_not_reached_before_commit_must_fail,
           ());
}

DEFINE_METHOD(void, mysql_before_commit_transaction_control_imp::allow, ()) {
  RUN_HOOK(transaction, set_no_restrictions_at_transactions_before_commit, ());
}
