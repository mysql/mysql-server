/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/xa/sql_xa_prepare.h"                   // Sql_cmd_xa_prepare
#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysql/psi/mysql_transaction.h"  // MYSQL_SET_TRANSACTION_XA_STATE
#include "mysqld_error.h"                 // Error codes
#include "scope_guard.h"                  // Scope_guard
#include "sql/binlog.h"                   // is_transaction_empty
#include "sql/clone_handler.h"            // Clone_handler::XA_Operation
#include "sql/debug_sync.h"               // DEBUG_SYNC
#include "sql/handler.h"                  // ha_rollback_trans
#include "sql/mdl_context_backup.h"       // MDL_context_backup_manager
#include "sql/rpl_replica_commit_order_manager.h"  // Commit_order_manager
#include "sql/sql_class.h"                         // THD
#include "sql/transaction.h"  // trans_reset_one_shot_chistics, trans_track_end_trx
#include "sql/transaction_info.h"      // Transaction_ctx
#include "sql/xa/transaction_cache.h"  // xa::Transaction_cache

namespace {
/**
  The main processing function for `XA PREPARE`:
  1. Iterates over all storage engines that participate in the
     transaction that are not the binary log and commands each to
     prepare.
  2. Prepares the transaction in the binary log, writing the `XA PREPARE`
     associated event to the current binlog.

  @param thd The `THD` session object within which the command is being
             executed.

  @return 0 if the transaction was successfully prepared, > 0 otherwise.
 */
int process_xa_prepare(THD *thd);
/**
  Detaches the active XA transaction from the current THD session.

  Executes the following actions:
  1. Creates an MDL context backup.
  2. Adds the transaction to the XA transaction cache.
  3. Detaches the underlying native SE transaction.

  @param thd The THD session object holding the transaction to be detached.

  @return false if the transaction has been successfully detached, true
          otherwise.
 */
bool detach_xa_transaction(THD *thd);
/**
  Detaches the transaction held by the THD session object from the storage
  engine represented by `hton`.

  This the same action which is performed by SE when disconnecting a
  connection which has a prepared XA transaction, when xa_detach_on_prepare
  is OFF.

  @param thd The THD session object holding the transaction to be detached.
  @param hton The handlerton object representing the SE for which the
              transaction must be detached.

  @return false if the transaction has been successfully detached, true
          otherwise.
*/
bool detach_native_trx_one_ht(THD *thd, handlerton *hton);
/**
  Reset the THD session object after XA transaction is detached, in order
  to make it usable for "new" work, normal statements or a new XA
  transaction.

  @param thd The THD session object to be reset and cleaned up.
 */
void reset_xa_connection(THD *thd);
}  // namespace

Sql_cmd_xa_prepare::Sql_cmd_xa_prepare(xid_t *xid_arg) : m_xid(xid_arg) {}

enum_sql_command Sql_cmd_xa_prepare::sql_command_code() const {
  return SQLCOM_XA_PREPARE;
}

bool Sql_cmd_xa_prepare::execute(THD *thd) {
  bool st = trans_xa_prepare(thd);

  if (!st) {
    if (!thd->is_engine_ha_data_detached() ||
        !(st = applier_reset_xa_trans(thd)))
      my_ok(thd);
  }

  return st;
}

bool Sql_cmd_xa_prepare::trans_xa_prepare(THD *thd) {
  DBUG_TRACE;

  auto tran = thd->get_transaction();
  auto xid_state = tran->xid_state();

  DBUG_PRINT("xa", ("trans_xa_prepare: formatID:%ld",
                    xid_state->get_xid()->get_format_id()));

  if (!xid_state->has_state(XID_STATE::XA_IDLE)) {
    my_error(ER_XAER_RMFAIL, MYF(0), xid_state->state_name());
    return true;
  }

  if (!xid_state->has_same_xid(m_xid)) {
    my_error(ER_XAER_NOTA, MYF(0));
    return true;
  }

  if (thd->slave_thread && is_transaction_empty(thd)) {
    my_error(ER_XA_REPLICATION_FILTERS,
             MYF(0));  // Empty XA transactions not allowed
    return true;
  }

  auto rollback_xa_tran = create_scope_guard([&]() {
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
    assert(thd->m_transaction_psi == nullptr);
#endif

    // Reset rm_error in case ha_xa_prepare() returned error,
    // so thd->transaction.xid structure gets reset
    // by THD::transaction::cleanup().
    xid_state->reset_error();
    cleanup_trans_state(thd);
    xid_state->reset();
    tran->cleanup();
    my_error(ER_XA_RBROLLBACK, MYF(0));
  });

  /*
    Acquire metadata lock which will ensure that XA PREPARE is blocked
    by active FLUSH TABLES WITH READ LOCK (and vice versa PREPARE in
    progress blocks FTWRL). This is to avoid binlog and redo entries
    while a backup is in progress.
  */
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request, MDL_key::COMMIT, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_STATEMENT);
  if (DBUG_EVALUATE_IF("xaprep_mdl_fail", true, false) ||
      thd->mdl_context.acquire_lock(&mdl_request,
                                    thd->variables.lock_wait_timeout)) {
    // Rollback the transaction if lock failed.
    ha_rollback_trans(thd, true);
    return true;
  }

  // For ha_xa_prepare() failure scenarios, transaction is already
  // rolled back by ha_xa_prepare().
  if (DBUG_EVALUATE_IF("xaprep_ha_xa_prepare_fail",
                       (ha_rollback_trans(thd, true), true), false) ||
      ::process_xa_prepare(thd))
    return true;

  xid_state->set_state(XID_STATE::XA_PREPARED);
  MYSQL_SET_TRANSACTION_XA_STATE(thd->m_transaction_psi,
                                 (int)xid_state->get_state());
  if (thd->rpl_thd_ctx.session_gtids_ctx().notify_after_xa_prepare(thd))
    LogErr(WARNING_LEVEL, ER_TRX_GTID_COLLECT_REJECT);

  // Use old style prepare unless xa_detach_on_prepare==true
  if (!is_xa_tran_detached_on_prepare(thd)) {
    rollback_xa_tran.commit();
    return thd->is_error();
  }
  // If xa_detach_on_prepare==true, detach the transaction and clean-up session
  if (::detach_xa_transaction(thd)) return true;
  rollback_xa_tran.commit();
  ::reset_xa_connection(thd);

  return false;
}

namespace {
int process_xa_prepare(THD *thd) {
  int error{0};
  auto trn_ctx = thd->get_transaction();
  DBUG_TRACE;

  if (trn_ctx->is_active(Transaction_ctx::SESSION)) {
    bool gtid_error = false, need_clear_owned_gtid = false;
    std::tie(gtid_error, need_clear_owned_gtid) = commit_owned_gtids(thd, true);
    auto clean_up_guard = create_scope_guard([&]() {
      if (error != 0) ha_rollback_trans(thd, true);

      /*
        After ensuring externalization order for applier thread, remove it
        from waiting (Commit Order Queue) and allow next applier thread to
        be ordered.
        NOTE: the calls to Commit_order_manager::wait/wait_and_finish() will
        be no-op for threads other than replication applier threads.
      */
      Commit_order_manager::wait_and_finish(thd, error);
      gtid_state_commit_or_rollback(thd, need_clear_owned_gtid, !error);
    });

    if (gtid_error) {
      assert(need_clear_owned_gtid);
      return error = 1;
    }

    if (Commit_order_manager::wait(thd)) {  // Ensure externalization order
                                            // for applier threads.
      thd->commit_error = THD::CE_NONE;
      return error = 1;
    }

    Clone_handler::XA_Operation xa_guard(
        thd);  // Allow GTID to be read by SE for XA prepare.

    DBUG_EXECUTE_IF("simulate_xa_failure_prepare", { return error = 1; });

    if (tc_log) {
      if ((error = tc_log->prepare(thd, /* all */ true)) != 0) return error;
    } else if ((error = trx_coordinator::set_prepared_in_tc_in_engines(
                    thd, /* all */ true)) != 0)
      return error;

    assert(thd->get_transaction()->xid_state()->has_state(XID_STATE::XA_IDLE));
  }

  return error;
}

bool detach_xa_transaction(THD *thd) {
  auto trn_ctx = thd->get_transaction();
  auto xid_state = trn_ctx->xid_state();

  if (MDL_context_backup_manager::instance().create_backup(
          &thd->mdl_context, xid_state->get_xid()->key(),
          xid_state->get_xid()->key_length())) {
    ha_rollback_trans(thd, true);
    return true;
  }

  if (DBUG_EVALUATE_IF("xaprep_trans_detach_fail", true, false) ||
      xa::Transaction_cache::detach(trn_ctx)) {
    MDL_context_backup_manager::instance().delete_backup(
        xid_state->get_xid()->key(), xid_state->get_xid()->key_length());
    ha_rollback_trans(thd, true);
    return true;
  }

  // Detach transaction in SE explicitly (when disconnecting this is done
  // by SE itself)
  auto ha_list = trn_ctx->ha_trx_info(Transaction_ctx::SESSION);
  for (auto const &ha_info : ha_list) {
    auto ht = ha_info.ht();
    if (::detach_native_trx_one_ht(thd, ht)) return true;
  }

  return false;
}

bool detach_native_trx_one_ht(THD *thd, handlerton *hton) {
  assert(hton != nullptr);

  if (hton->state != SHOW_OPTION_YES) {
    assert(hton->replace_native_transaction_in_thd == nullptr);
    return false;
  }
  assert(hton->slot != HA_SLOT_UNDEF);

  if (hton->replace_native_transaction_in_thd != nullptr) {
    // Force call to trx_disconnect_prepared in Innodb when calling with
    // nullptr,nullptr
    hton->replace_native_transaction_in_thd(thd, nullptr, nullptr);
  }

  // Reset session Ha_trx_info so it is not marked as started.
  // Otherwise, we will not be able to start a new XA transaction on
  // this connection.
  thd->get_ha_data(hton->slot)
      ->ha_info[Transaction_ctx::SESSION]
      .reset();  // Mark as not started

  thd->get_ha_data(hton->slot)
      ->ha_info[Transaction_ctx::STMT]
      .reset();  // Mark as not started

  return false;
}

void reset_xa_connection(THD *thd) {
  auto trn_ctx = thd->get_transaction();
  auto xid_state = trn_ctx->xid_state();

  xid_state->reset();

  // Can't call cleanup_trans_state here since it will delete transaction
  // from cache. So reset manually.
  thd->variables.option_bits &= ~OPTION_BEGIN;
  thd->server_status &=
      ~(SERVER_STATUS_IN_TRANS | SERVER_STATUS_IN_TRANS_READONLY);

  trn_ctx->reset_unsafe_rollback_flags(Transaction_ctx::SESSION);
  trn_ctx->reset_unsafe_rollback_flags(Transaction_ctx::STMT);

  trn_ctx->reset_scope(
      Transaction_ctx::SESSION);  // Make non-active, so we don't get
                                  // problems in new transactions on this
                                  // connection.
  // For completeness.
  trn_ctx->reset_scope(Transaction_ctx::STMT);

  thd->mdl_context.release_transactional_locks();
  trans_reset_one_shot_chistics(thd);
  trans_track_end_trx(thd);
  // Needed to clear out savepoints, and to clear transaction context memory
  // root.
  trn_ctx->cleanup();

  assert(xid_state->has_state(XID_STATE::XA_NOTR));

#ifdef HAVE_PSI_TRANSACTION_INTERFACE
  thd->m_transaction_psi = nullptr;  // avoid assert
#endif                               /* HAVE_PSI_TRANSACTION_INTERFACE */
}
}  // namespace
