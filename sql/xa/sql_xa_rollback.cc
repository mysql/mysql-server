/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "sql/xa/sql_xa_rollback.h"  // Sql_cmd_xa_rollback
#include "mysqld_error.h"            // Error codes
#include "sql/clone_handler.h"       // Clone_handler::XA_Operation
#include "sql/debug_sync.h"          // DEBUG_SYNC
#include "sql/handler.h"             // commit_owned_gtids
#include "sql/raii/sentry.h"         // raii::Sentry<>
#include "sql/rpl_gtid.h"            // gtid_state_commit_or_rollback
#include "sql/sql_class.h"           // THD
#include "sql/tc_log.h"              // tc_log
#include "sql/transaction.h"  // trans_reset_one_shot_chistics, trans_track_end_trx
#include "sql/transaction_info.h"  // Transaction_ctx

Sql_cmd_xa_rollback::Sql_cmd_xa_rollback(xid_t *xid_arg)
    : Sql_cmd_xa_second_phase{xid_arg} {}

enum_sql_command Sql_cmd_xa_rollback::sql_command_code() const {
  return SQLCOM_XA_ROLLBACK;
}

bool Sql_cmd_xa_rollback::execute(THD *thd) {
  bool st = trans_xa_rollback(thd);

  if (!st) {
    thd->mdl_context.release_transactional_locks();
    /*
      We've just done a rollback, reset transaction
      isolation level and access mode to the session default.
    */
    trans_reset_one_shot_chistics(thd);
    my_ok(thd);
  }

  DBUG_EXECUTE_IF("crash_after_xa_rollback", DBUG_SUICIDE(););

  return st;
}

bool Sql_cmd_xa_rollback::trans_xa_rollback(THD *thd) {
  auto xid_state = thd->get_transaction()->xid_state();

  /* Inform clone handler of XA operation. */
  Clone_handler::XA_Operation xa_guard(thd);
  if (!xid_state->has_same_xid(this->m_xid)) {
    return this->process_detached_xa_rollback(thd);
  }
  return this->process_attached_xa_rollback(thd);
}

bool Sql_cmd_xa_rollback::process_attached_xa_rollback(THD *thd) const {
  DBUG_TRACE;
  auto xid_state = thd->get_transaction()->xid_state();

  if (xid_state->has_state(XID_STATE::XA_NOTR) ||
      xid_state->has_state(XID_STATE::XA_ACTIVE)) {
    my_error(ER_XAER_RMFAIL, MYF(0), xid_state->state_name());
    return true;
  }

  /*
    Acquire metadata lock which will ensure that XA ROLLBACK is blocked
    by active FLUSH TABLES WITH READ LOCK (and vice versa ROLLBACK in
    progress blocks FTWRL). This is to avoid binlog and redo entries
    while a backup is in progress.
  */
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request, MDL_key::COMMIT, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_STATEMENT);
  if (thd->mdl_context.acquire_lock(&mdl_request,
                                    thd->variables.lock_wait_timeout)) {
    /*
      We can't rollback an XA transaction on lock failure due to
      Innodb redo log and bin log update is involved in rollback.
      Return error to user for a retry.
    */
    my_error(ER_XAER_RMERR, MYF(0));
    return true;
  }

  bool gtid_error = false;
  bool need_clear_owned_gtid = false;
  std::tie(gtid_error, need_clear_owned_gtid) = commit_owned_gtids(thd, true);
  CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_rollback_xa_trx");
  bool res = xa_trans_force_rollback(thd) || gtid_error;
  gtid_state_commit_or_rollback(thd, need_clear_owned_gtid, !gtid_error);
  // todo: report a bug in that the raised rm_error in this branch
  //       is masked unlike the detached rollback branch above.
  DBUG_EXECUTE_IF("simulate_xa_rm_error", {
    my_error(ER_XA_RBROLLBACK, MYF(0));
    res = true;
  });

  cleanup_trans_state(thd);

  xid_state->set_state(XID_STATE::XA_NOTR);
  xid_state->unset_binlogged();
  trans_track_end_trx(thd);
  /* The transaction should be marked as complete in P_S. */
  assert(thd->m_transaction_psi == nullptr);
  return res;
}

bool Sql_cmd_xa_rollback::process_detached_xa_rollback(THD *thd) {
  DBUG_TRACE;

  raii::Sentry<> dispose_guard{[this]() -> void { this->dispose(); }};
  if (this->find_and_initialize_xa_context(thd)) return true;
  if (this->acquire_locks(thd)) return true;
  raii::Sentry<> acquired_locks_guard{
      [this]() -> void { this->release_locks(); }};
  this->setup_thd_context(thd);
  if (this->enter_commit_order(thd)) return true;

  CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_rollback_xa_trx");
  this->assign_xid_to_thd(thd);
  if (tc_log == nullptr) {
    this->m_result =
        trx_coordinator::rollback_detached_by_xid(thd) || this->m_result;
  } else {
    this->m_result = tc_log->rollback(thd, /* all */ true) || this->m_result;
  }

  this->exit_commit_order(thd);
  // This is normally done in ha_rollback_trans, but since we do not call this
  // for an external rollback we need to do it explicitly here.
  this->m_detached_trx_context->push_unsafe_rollback_warnings(thd);
  this->cleanup_context(thd);

  return this->m_result;
}
