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

#include "sql/xa/sql_xa_second_phase.h"  // Sql_cmd_xa_second_phase
#include "mysqld_error.h"                // Error codes
#include "sql/debug_sync.h"              // DEBUG_SYNC
#include "sql/mdl_context_backup.h"      // MDL_context_backup_manager
#include "sql/raii/sentry.h"             // raii::Sentry<>
#include "sql/rpl_gtid.h"                // gtid_state_commit_or_rollback
#include "sql/rpl_replica_commit_order_manager.h"  // Commit_order_manager
#include "sql/sql_class.h"                         // THD
#include "sql/transaction_info.h"                  // Transaction_ctx
#include "sql/xa/transaction_cache.h"              // xa::Transaction_cache

Sql_cmd_xa_second_phase::Sql_cmd_xa_second_phase(xid_t *xid_arg)
    : m_xid(xid_arg) {}

bool Sql_cmd_xa_second_phase::find_and_initialize_xa_context(THD *thd) {
  // XID_STATE object used in the TC_LOG/handler execution stack
  auto thd_xs = thd->get_transaction()->xid_state();
  this->m_detached_trx_context =
      find_trn_for_recover_and_check_its_state(thd, this->m_xid, thd_xs);

  if (this->m_detached_trx_context == nullptr) return (this->m_result = true);

  assert(*this->m_detached_trx_context->xid_state()->get_xid() == *this->m_xid);

  return false;
}

bool Sql_cmd_xa_second_phase::acquire_locks(THD *thd) {
  assert(this->m_detached_trx_context != nullptr);
  auto thd_xs = thd->get_transaction()->xid_state();
  auto detached_xs = this->m_detached_trx_context->xid_state();

  /*
    Metadata locks taken during XA COMMIT should be released when
    there is error in commit order execution, so we take a savepoint
    and rollback to it in case of error. The error during commit order
    execution can be temporary like commit order deadlock (check header
    comment for check_and_report_deadlock()) and can be recovered after
    retrying unlike other commit errors. And to do so we need to restore
    status of metadata locks i.e. rollback to savepoint, before the retry
    attempt to ensure order for applier threads.
  */
  this->m_mdl_savepoint = thd->mdl_context.mdl_savepoint();

  DEBUG_SYNC(thd, "detached_xa_commit_before_acquire_xa_lock");
  /*
    Acquire XID_STATE::m_xa_lock to prevent concurrent running of two
    XA COMMIT/XA ROLLBACK statements. Without acquiring this lock an attempt
    to run two XA COMMIT/XA ROLLBACK statement for the same xid value may lead
    to writing two events for the same xid into the binlog (e.g. twice
    XA COMMIT event, that is an event for XA COMMIT some_xid_value
    followed by an another event XA COMMIT with the same xid value).
    As a consequences, presence of two XA COMMIT/XA ROLLACK statements for
    the same xid value in binlog would break replication.
  */
  detached_xs->get_xa_lock().lock();

  raii::Sentry<> mutex_guard_on_error{[detached_xs, this]() -> void {
    if (this->m_result) detached_xs->get_xa_lock().unlock();
  }};

  /*
    Double check that the XA transaction still does exist since the
    transaction could be removed from the cache by another XA COMMIT/XA
    ROLLBACK statement being executed concurrently from parallel session with
    the same xid value.
  */
  if (find_trn_for_recover_and_check_its_state(thd, this->m_xid, thd_xs) ==
      nullptr)
    return (this->m_result = true);

  if (acquire_mandatory_metadata_locks(thd, this->m_xid)) {
    /*
      We can't rollback an XA transaction on lock failure due to
      Innodb redo log and bin log update is involved in rollback.
      Return error to user for a retry.
    */
    my_error(ER_XA_RETRY, MYF(0));
    return (this->m_result = true);
  }

  DEBUG_SYNC(thd, "detached_xa_commit_after_acquire_commit_lock");

  return false;
}

void Sql_cmd_xa_second_phase::release_locks() const {
  assert(this->m_detached_trx_context != nullptr);
  auto detached_xs = this->m_detached_trx_context->xid_state();
  detached_xs->get_xa_lock().unlock();
}

void Sql_cmd_xa_second_phase::setup_thd_context(THD *thd) {
  assert(this->m_detached_trx_context != nullptr);
  auto thd_xs = thd->get_transaction()->xid_state();
  auto detached_xs = this->m_detached_trx_context->xid_state();

  std::tie(this->m_gtid_error, this->m_need_clear_owned_gtid) =
      commit_owned_gtids(thd, true);
  if (this->m_gtid_error) my_error(ER_XA_RBROLLBACK, MYF(0));
  this->m_result = detached_xs->xa_trans_rolled_back() || this->m_gtid_error;

  assert(thd_xs->is_binlogged() == false);
  /*
    Detached transaction XID_STATE::is_binlogged() is passed through THD
    XID_STATE class member to low-level logging routines for deciding how
    to log.
  */
  if (detached_xs->is_binlogged())
    thd_xs->set_binlogged();
  else
    thd_xs->unset_binlogged();
}

bool Sql_cmd_xa_second_phase::enter_commit_order(THD *thd) {
  auto thd_xs = thd->get_transaction()->xid_state();

  if (Commit_order_manager::wait(
          thd)) {  // Ensure externalization order for applier threads
                   // (no-op for non-applier threads).

    Commit_order_manager::wait_and_finish(
        thd, true);  // Remove from commit order queue and allow for a retry

    gtid_state_commit_or_rollback(thd, /* needs_to */ true,
                                  /* do_commit */ false);
    thd->mdl_context.rollback_to_savepoint(this->m_mdl_savepoint);
    thd_xs->unset_binlogged();
    return (this->m_result = true);
  }
  return false;
}

void Sql_cmd_xa_second_phase::assign_xid_to_thd(THD *thd) const {
  assert(this->m_detached_trx_context != nullptr);
  auto thd_xs = thd->get_transaction()->xid_state();
  thd_xs->start_detached_xa(this->m_xid, thd_xs->is_binlogged());
}

void Sql_cmd_xa_second_phase::exit_commit_order(THD *thd) const {
  Commit_order_manager::wait_and_finish(
      thd, this->m_result);  // Remove from the commit order queue (no-op for
                             // non-applier threads).
}

void Sql_cmd_xa_second_phase::cleanup_context(THD *thd) const {
  assert(this->m_detached_trx_context != nullptr);
  auto thd_xs = thd->get_transaction()->xid_state();

  // Restoring the binlogged status of the thd_xid_state after borrowing it
  // to pass the binlogged flag to binlog_xa_commit().
  thd_xs->unset_binlogged();

  MDL_context_backup_manager::instance().delete_backup(
      this->m_xid->key(), this->m_xid->key_length());

  xa::Transaction_cache::remove(this->m_detached_trx_context.get());
  gtid_state_commit_or_rollback(thd, this->m_need_clear_owned_gtid,
                                !this->m_result);
}

void Sql_cmd_xa_second_phase::dispose() {
  this->m_detached_trx_context.reset();
}
