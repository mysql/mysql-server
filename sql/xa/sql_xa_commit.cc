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

#include "sql/xa/sql_xa_commit.h"         // Sql_cmd_xa_commit
#include "mysql/psi/mysql_transaction.h"  // MYSQL_SET_TRANSACTION_XA_STATE
#include "mysqld_error.h"                 // Error codes
#include "sql/clone_handler.h"            // Clone_handler::XA_Operation
#include "sql/debug_sync.h"               // DEBUG_SYNC
#include "sql/handler.h"                  // commit_owned_gtids
#include "sql/raii/sentry.h"              // raii::Sentry<>
#include "sql/rpl_gtid.h"                 // gtid_state_commit_or_rollback
#include "sql/sql_class.h"                // THD
#include "sql/sql_lex.h"                  // struct LEX
#include "sql/tc_log.h"                   // tc_log
#include "sql/transaction.h"  // trans_reset_one_shot_chistics, trans_track_end_trx
#include "sql/transaction_info.h"  // Transaction_ctx

namespace {
/**
  Forces the transaction to be rolled back upon error in the commit process.

  It, temporarily, changes `THD::lex::sql_command` to
  `SQLCOM_XA_ROLLBACK` so that the executed rollback stack may behave as
  an actual `XA ROLLBACK` was issued (log an `XA ROLLBACK` to the binary
  log, for instance).

  @param thd The `THD` session object within which the command is being
             executed.
 */
void force_rollback(THD *thd);
}  // namespace

Sql_cmd_xa_commit::Sql_cmd_xa_commit(xid_t *xid_arg,
                                     enum xa_option_words xa_option)
    : Sql_cmd_xa_second_phase{xid_arg}, m_xa_opt{xa_option} {}

enum_sql_command Sql_cmd_xa_commit::sql_command_code() const {
  return SQLCOM_XA_COMMIT;
}

enum xa_option_words Sql_cmd_xa_commit::get_xa_opt() const { return m_xa_opt; }

bool Sql_cmd_xa_commit::execute(THD *thd) {
  bool st = trans_xa_commit(thd);

  if (!st) {
    thd->mdl_context.release_transactional_locks();
    /*
        We've just done a commit, reset transaction
        isolation level and access mode to the session default.
    */
    trans_reset_one_shot_chistics(thd);

    my_ok(thd);
  }
  return st;
}

bool Sql_cmd_xa_commit::trans_xa_commit(THD *thd) {
  auto xid_state = thd->get_transaction()->xid_state();

  assert(!thd->slave_thread || xid_state->get_xid()->is_null() ||
         m_xa_opt == XA_ONE_PHASE);

  /* Inform clone handler of XA operation. */
  Clone_handler::XA_Operation xa_guard(thd);

  if (!xid_state->has_same_xid(this->m_xid)) {
    return this->process_detached_xa_commit(thd);
  }
  return this->process_attached_xa_commit(thd);
}

bool Sql_cmd_xa_commit::process_attached_xa_commit(THD *thd) const {
  DBUG_TRACE;
  bool res = false;
  bool gtid_error = false, need_clear_owned_gtid = false;
  auto xid_state = thd->get_transaction()->xid_state();

  if (xid_state->xa_trans_rolled_back()) {
    xa_trans_force_rollback(thd);
    res = thd->is_error();
  } else if (xid_state->has_state(XID_STATE::XA_IDLE) &&
             m_xa_opt == XA_ONE_PHASE) {
    int r = ha_commit_trans(thd, true);
    if ((res = r)) my_error(r == 1 ? ER_XA_RBROLLBACK : ER_XAER_RMERR, MYF(0));
  } else if (xid_state->has_state(XID_STATE::XA_PREPARED) &&
             m_xa_opt == XA_NONE) {
    MDL_request mdl_request;

    /*
      Acquire metadata lock which will ensure that COMMIT is blocked
      by active FLUSH TABLES WITH READ LOCK (and vice versa COMMIT in
      progress blocks FTWRL).

      We allow FLUSHer to COMMIT; we assume FLUSHer knows what it does.
    */
    MDL_REQUEST_INIT(&mdl_request, MDL_key::COMMIT, "", "",
                     MDL_INTENTION_EXCLUSIVE, MDL_STATEMENT);
    if (thd->mdl_context.acquire_lock(&mdl_request,
                                      thd->variables.lock_wait_timeout)) {
      /*
        We can't rollback an XA transaction on lock failure due to
        Innodb redo log and bin log update are involved in rollback.
        Return error to user for a retry.
      */
      my_error(ER_XA_RETRY, MYF(0));
      return true;
    }

    std::tie(gtid_error, need_clear_owned_gtid) = commit_owned_gtids(thd, true);
    if (gtid_error) {
      res = true;
      /*
        Failure to store gtid is regarded as a unilateral one of the
        resource manager therefore the transaction is to be rolled back.
        The specified error is the same as @c xa_trans_force_rollback.
        The prepared XA will be rolled back along and so will do Gtid state,
        see ha_rollback_trans().

        Todo/fixme: fix binlogging, "XA rollback" event could be missed out.
        Todo/fixme: as to XAER_RMERR, should not it be XA_RBROLLBACK?
                    Rationale: there's no consistency concern after rollback,
                    unlike what XAER_RMERR suggests.
      */
      ha_rollback_trans(thd, true);
      my_error(ER_XAER_RMERR, MYF(0));
    } else {
      CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_commit_xa_trx");
      DEBUG_SYNC(thd, "trans_xa_commit_after_acquire_commit_lock");

      if (tc_log != nullptr)
        res = tc_log->commit(thd, /* all */ true);
      else
        res = ha_commit_low(thd, /* all */ true);

      DBUG_EXECUTE_IF("simulate_xa_commit_log_failure", { res = true; });

      if (res)
        my_error(ER_XAER_RMERR, MYF(0));  // todo/fixme: consider to rollback it
#ifdef HAVE_PSI_TRANSACTION_INTERFACE
      else {
        /*
          Since we don't call ha_commit_trans() for prepared transactions,
          we need to explicitly mark the transaction as committed.
        */
        MYSQL_COMMIT_TRANSACTION(thd->m_transaction_psi);
      }

      thd->m_transaction_psi = nullptr;
#endif
    }
  } else {
    my_error(ER_XAER_RMFAIL, MYF(0), xid_state->state_name());
    return true;
  }

  gtid_state_commit_or_rollback(thd, need_clear_owned_gtid, !gtid_error);
  cleanup_trans_state(thd);

  xid_state->set_state(XID_STATE::XA_NOTR);
  xid_state->unset_binlogged();
  trans_track_end_trx(thd);
  /* The transaction should be marked as complete in P_S. */
  assert(thd->m_transaction_psi == nullptr || res);
  return res;
}

bool Sql_cmd_xa_commit::process_detached_xa_commit(THD *thd) {
  DBUG_TRACE;

  raii::Sentry<> dispose_guard{[this]() -> void { this->dispose(); }};
  if (this->find_and_initialize_xa_context(thd)) return true;
  if (this->acquire_locks(thd)) return true;
  raii::Sentry<> acquired_locks_guard{
      [this]() -> void { this->release_locks(); }};
  this->setup_thd_context(thd);
  if (this->enter_commit_order(thd)) return true;

  CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("before_commit_xa_trx");
  this->assign_xid_to_thd(thd);
  if (!this->m_result) {
    if (tc_log == nullptr)
      this->m_result = trx_coordinator::commit_detached_by_xid(thd);
    else
      this->m_result = tc_log->commit(thd, /* all */ true);
  } else
    ::force_rollback(thd);

  this->exit_commit_order(thd);
  this->cleanup_context(thd);

  return this->m_result;
}

namespace {
void force_rollback(THD *thd) {
  auto sql_command = thd->lex->sql_command;
  thd->lex->sql_command = SQLCOM_XA_ROLLBACK;
  raii::Sentry<> _lex_guard{
      [&]() -> void { thd->lex->sql_command = sql_command; }};
  if (tc_log == nullptr)
    trx_coordinator::rollback_detached_by_xid(thd);
  else
    tc_log->rollback(thd, /* all */ true);
}
}  // namespace
