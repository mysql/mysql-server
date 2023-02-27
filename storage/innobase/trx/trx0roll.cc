/*****************************************************************************

Copyright (c) 1996, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file trx/trx0roll.cc
 Transaction rollback

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#include <sys/types.h>

#include "clone0clone.h"
#include "dict0dd.h"
#include "fsp0fsp.h"
#include "ha_prototypes.h"
#include "lock0lock.h"
#include "mach0data.h"
#include "os0thread-create.h"
#include "pars0pars.h"
#include "que0que.h"
#include "read0read.h"
#include "row0mysql.h"
#include "row0undo.h"
#include "sql_thd_internal_api.h"
#include "srv0mon.h"
#include "srv0start.h"
#include "trx0rec.h"
#include "trx0roll.h"
#include "trx0rseg.h"
#include "trx0sys.h"
#include "trx0trx.h"
#include "trx0undo.h"
#include "usr0sess.h"

#include "current_thd.h"

/** This many pages must be undone before a truncate is tried within
rollback */
static const ulint TRX_ROLL_TRUNC_THRESHOLD = 1;

/** In crash recovery, the current trx to be rolled back; NULL otherwise */
static const trx_t *trx_roll_crash_recv_trx = nullptr;

/** In crash recovery we set this to the undo n:o of the current trx to be
rolled back. Then we can print how many % the rollback has progressed. */
static undo_no_t trx_roll_max_undo_no;

/** Auxiliary variable which tells the previous progress % we printed */
static ulint trx_roll_progress_printed_pct;

/** Finishes a transaction rollback. */
static void trx_rollback_finish(trx_t *trx); /*!< in: transaction */

/** Rollback a transaction used in MySQL. */
static void trx_rollback_to_savepoint_low(
    trx_t *trx,           /*!< in: transaction handle */
    trx_savept_t *savept) /*!< in: pointer to savepoint undo number, if
                          partial rollback requested, or NULL for
                          complete rollback */
{
  que_thr_t *thr;
  mem_heap_t *heap;
  roll_node_t *roll_node;

  heap = mem_heap_create(512, UT_LOCATION_HERE);

  roll_node = roll_node_create(heap);

  if (savept != nullptr) {
    roll_node->partial = true;
    roll_node->savept = *savept;
    check_trx_state(trx);
  } else {
    assert_trx_nonlocking_or_in_list(trx);
  }

  trx->error_state = DB_SUCCESS;

  if (trx_is_rseg_updated(trx)) {
    ut_ad(trx->rsegs.m_redo.rseg != nullptr ||
          trx->rsegs.m_noredo.rseg != nullptr);

    thr = pars_complete_graph_for_exec(roll_node, trx, heap, nullptr);

    ut_a(thr == que_fork_start_command(
                    static_cast<que_fork_t *>(que_node_get_parent(thr))));

    que_run_threads(thr);

    ut_a(roll_node->undo_thr != nullptr);
    que_run_threads(roll_node->undo_thr);

    /* Free the memory reserved by the undo graph. */
    que_graph_free(static_cast<que_t *>(roll_node->undo_thr->common.parent));
  }

  if (savept == nullptr) {
    trx_rollback_finish(trx);
    MONITOR_INC(MONITOR_TRX_ROLLBACK);
  } else {
    trx->lock.que_state = TRX_QUE_RUNNING;
    MONITOR_INC(MONITOR_TRX_ROLLBACK_SAVEPOINT);
  }

  ut_a(trx->error_state == DB_SUCCESS);
  ut_a(trx->lock.que_state == TRX_QUE_RUNNING);

  mem_heap_free(heap);

  /* There might be work for utility threads.*/
  srv_active_wake_master_thread();

  MONITOR_DEC(MONITOR_TRX_ACTIVE);
}

/** Rollback a transaction to a given savepoint or do a complete rollback.
 @return error code or DB_SUCCESS */
dberr_t trx_rollback_to_savepoint(
    trx_t *trx,           /*!< in: transaction handle */
    trx_savept_t *savept) /*!< in: pointer to savepoint undo number, if
                          partial rollback requested, or NULL for
                          complete rollback */
{
  ut_ad(!trx_mutex_own(trx));

  trx_start_if_not_started_xa(trx, true, UT_LOCATION_HERE);

  trx_rollback_to_savepoint_low(trx, savept);

  return (trx->error_state);
}

/** Rollback a transaction used in MySQL.
 @return error code or DB_SUCCESS */
static dberr_t trx_rollback_for_mysql_low(
    trx_t *trx) /*!< in/out: transaction */
{
  trx->op_info = "rollback";

  /* If we are doing the XA recovery of prepared transactions,
  then the transaction object does not have an InnoDB session
  object, and we set a dummy session that we use for all MySQL
  transactions. */

  trx_rollback_to_savepoint_low(trx, nullptr);

  trx->op_info = "";

  ut_a(trx->error_state == DB_SUCCESS);

  return (trx->error_state);
}

/** Rollback a transaction used in MySQL
@param[in, out] trx     transaction
@return error code or DB_SUCCESS */
static dberr_t trx_rollback_low(trx_t *trx) {
  /* We are reading trx->state without mutex protection here,
  because the rollback should either be invoked for:
    - a running active MySQL transaction associated
      with the current thread,
    - or a recovered prepared transaction,
    - or a transaction which is a victim being killed by HP transaction
      run by the current thread, in which case it is guaranteed that
      thread owning the transaction, which is being killed, is not
      inside InnoDB (thanks to TRX_FORCE_ROLLBACK and TrxInInnoDB::wait()). */
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  switch (trx->state.load(std::memory_order_relaxed)) {
    case TRX_STATE_FORCED_ROLLBACK:
    case TRX_STATE_NOT_STARTED:
      trx->will_lock = 0;
      ut_ad(trx->in_mysql_trx_list);
      return (DB_SUCCESS);

    case TRX_STATE_ACTIVE:
      ut_ad(trx->in_mysql_trx_list);
      assert_trx_nonlocking_or_in_list(trx);
      /* Check an validate that undo is available for GTID. */
      trx_undo_gtid_add_update_undo(trx, false, true);
      return (trx_rollback_for_mysql_low(trx));

    case TRX_STATE_PREPARED:
      /* Check an validate that undo is available for GTID. */
      trx_undo_gtid_add_update_undo(trx, false, true);
      ut_ad(!trx_is_autocommit_non_locking(trx));
      if (trx->rsegs.m_redo.rseg != nullptr && trx_is_redo_rseg_updated(trx)) {
        /* Change the undo log state back from
        TRX_UNDO_PREPARED to TRX_UNDO_ACTIVE
        so that if the system gets killed,
        recovery will perform the rollback. */
        trx_undo_ptr_t *undo_ptr = &trx->rsegs.m_redo;

        mtr_t mtr;

        mtr.start();

        trx->rsegs.m_redo.rseg->latch();

        if (undo_ptr->insert_undo != nullptr) {
          trx_undo_set_state_at_prepare(trx, undo_ptr->insert_undo, true, &mtr);
        }

        if (undo_ptr->update_undo != nullptr) {
          trx_undo_gtid_set(trx, undo_ptr->update_undo, false);
          trx_undo_set_state_at_prepare(trx, undo_ptr->update_undo, true, &mtr);
        }
        trx->rsegs.m_redo.rseg->unlatch();

        /* Persist the XA ROLLBACK, so that crash
        recovery will replay the rollback in case
        the redo log gets applied past this point. */
        mtr.commit();
        ut_ad(mtr.commit_lsn() > 0 || !mtr_t::s_logging.is_enabled());
      }
#ifdef ENABLED_DEBUG_SYNC
      if (trx->mysql_thd == nullptr) {
        /* We could be executing XA ROLLBACK after
        XA PREPARE and a server restart. */
      } else if (!trx_is_redo_rseg_updated(trx)) {
        /* innobase_close_connection() may roll back a
        transaction that did not generate any
        persistent undo log. The DEBUG_SYNC
        would cause an assertion failure for a
        disconnected thread.

        NOTE: InnoDB will not know about the XID
        if no persistent undo log was generated. */
      } else {
        DEBUG_SYNC_C("trx_xa_rollback");
      }
#endif /* ENABLED_DEBUG_SYNC */
      return (trx_rollback_for_mysql_low(trx));

    case TRX_STATE_COMMITTED_IN_MEMORY:
      check_trx_state(trx);
      break;
  }

  ut_error;
}

/** Rollback a transaction used in MySQL.
 @return error code or DB_SUCCESS */
dberr_t trx_rollback_for_mysql(trx_t *trx) /*!< in/out: transaction */
{
  /* Avoid the tracking of async rollback killer
  thread to enter into InnoDB. */
  if (TrxInInnoDB::is_async_rollback(trx)) {
    return (trx_rollback_low(trx));

  } else {
    TrxInInnoDB trx_in_innodb(trx, true);
    return (trx_rollback_low(trx));
  }
}

/** Rollback the latest SQL statement for MySQL.
 @return error code or DB_SUCCESS */
dberr_t trx_rollback_last_sql_stat_for_mysql(
    trx_t *trx) /*!< in/out: transaction */
{
  dberr_t err;

  ut_ad(trx->in_mysql_trx_list);

  /* We are reading trx->state without mutex protection here,
  because the rollback should either be invoked for:
    - a running active MySQL transaction associated
      with the current thread,
    - or a recovered prepared transaction,
    - or a transaction which is a victim being killed by HP transaction
      run by the current thread, in which case it is guaranteed that
      thread owning the transaction, which is being killed, is not
      inside InnoDB (thanks to TRX_FORCE_ROLLBACK and TrxInInnoDB::wait()). */

  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  switch (trx->state.load(std::memory_order_relaxed)) {
    case TRX_STATE_FORCED_ROLLBACK:
    case TRX_STATE_NOT_STARTED:
      return (DB_SUCCESS);

    case TRX_STATE_ACTIVE:
      assert_trx_nonlocking_or_in_list(trx);

      trx->op_info = "rollback of SQL statement";

      err = trx_rollback_to_savepoint(trx, &trx->last_sql_stat_start);

      if (trx->fts_trx != nullptr) {
        fts_savepoint_rollback_last_stmt(trx);
      }

      /* The following call should not be needed,
      but we play it safe: */
      trx_mark_sql_stat_end(trx);

      trx->op_info = "";

      return (err);

    case TRX_STATE_PREPARED:
    case TRX_STATE_COMMITTED_IN_MEMORY:
      /* The statement rollback is only allowed on an ACTIVE
      transaction, not a PREPARED or COMMITTED one. */
      break;
  }

  ut_error;
}

/** Search for a savepoint using name.
 @return savepoint if found else NULL */
static trx_named_savept_t *trx_savepoint_find(
    trx_t *trx,       /*!< in: transaction */
    const char *name) /*!< in: savepoint name */
{
  for (auto savep : trx->trx_savepoints) {
    if (0 == ut_strcmp(savep->name, name)) {
      return (savep);
    }
  }

  return (nullptr);
}

/** Frees a single savepoint struct. */
static void trx_roll_savepoint_free(
    trx_t *trx,                /*!< in: transaction handle */
    trx_named_savept_t *savep) /*!< in: savepoint to free */
{
  UT_LIST_REMOVE(trx->trx_savepoints, savep);

  ut::free(savep->name);
  ut::free(savep);
}

/** Frees savepoint structs starting from savep.
@param[in] trx Transaction handle
@param[in] savep Free all savepoints starting with this savepoint i, if savep is
nullptr free all save points */
void trx_roll_savepoints_free(trx_t *trx, trx_named_savept_t *savep) {
  while (savep != nullptr) {
    trx_named_savept_t *next_savep;

    next_savep = UT_LIST_GET_NEXT(trx_savepoints, savep);

    trx_roll_savepoint_free(trx, savep);

    savep = next_savep;
  }
}

/** Rolls back a transaction back to a named savepoint. Modifications after the
 savepoint are undone but InnoDB does NOT release the corresponding locks
 which are stored in memory. If a lock is 'implicit', that is, a new inserted
 row holds a lock where the lock information is carried by the trx id stored in
 the row, these locks are naturally released in the rollback. Savepoints which
 were set after this savepoint are deleted.
 @return if no savepoint of the name found then DB_NO_SAVEPOINT,
 otherwise DB_SUCCESS */
[[nodiscard]] static dberr_t trx_rollback_to_savepoint_for_mysql_low(
    trx_t *trx,                /*!< in/out: transaction */
    trx_named_savept_t *savep, /*!< in/out: savepoint */
    int64_t *mysql_binlog_cache_pos)
/*!< out: the MySQL binlog
cache position corresponding
to this savepoint; MySQL needs
this information to remove the
binlog entries of the queries
executed after the savepoint */
{
  dberr_t err;

  ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));
  ut_ad(trx->in_mysql_trx_list);

  /* Free all savepoints strictly later than savep. */

  trx_roll_savepoints_free(trx, UT_LIST_GET_NEXT(trx_savepoints, savep));

  *mysql_binlog_cache_pos = savep->mysql_binlog_cache_pos;

  trx->op_info = "rollback to a savepoint";

  err = trx_rollback_to_savepoint(trx, &savep->savept);

  /* Store the current undo_no of the transaction so that
  we know where to roll back if we have to roll back the
  next SQL statement: */

  trx_mark_sql_stat_end(trx);

  trx->op_info = "";

  return (err);
}

/** Rolls back a transaction back to a named savepoint. Modifications after the
 savepoint are undone but InnoDB does NOT release the corresponding locks
 which are stored in memory. If a lock is 'implicit', that is, a new inserted
 row holds a lock where the lock information is carried by the trx id stored in
 the row, these locks are naturally released in the rollback. Savepoints which
 were set after this savepoint are deleted.
 @return if no savepoint of the name found then DB_NO_SAVEPOINT,
 otherwise DB_SUCCESS */
dberr_t trx_rollback_to_savepoint_for_mysql(
    trx_t *trx,                      /*!< in: transaction handle */
    const char *savepoint_name,      /*!< in: savepoint name */
    int64_t *mysql_binlog_cache_pos) /*!< out: the MySQL binlog cache
                                     position corresponding to this
                                     savepoint; MySQL needs this
                                     information to remove the
                                     binlog entries of the queries
                                     executed after the savepoint */
{
  trx_named_savept_t *savep;

  ut_ad(trx->in_mysql_trx_list);

  savep = trx_savepoint_find(trx, savepoint_name);

  if (savep == nullptr) {
    return (DB_NO_SAVEPOINT);
  }

  /* We are reading trx->state without mutex protection here,
  because the rollback should either be invoked for:
    - a running active MySQL transaction associated
      with the current thread,
    - or a recovered prepared transaction,
    - or a transaction which is a victim being killed by HP transaction
      run by the current thread, in which case it is guaranteed that
      thread owning the transaction, which is being killed, is not
      inside InnoDB (thanks to TRX_FORCE_ROLLBACK and TrxInInnoDB::wait()). */

  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  switch (trx->state.load(std::memory_order_relaxed)) {
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:

      ib::error(ER_IB_MSG_1185) << "Transaction has a savepoint " << savep->name
                                << " though it is not started";

      return (DB_ERROR);

    case TRX_STATE_ACTIVE:

      return (trx_rollback_to_savepoint_for_mysql_low(trx, savep,
                                                      mysql_binlog_cache_pos));

    case TRX_STATE_PREPARED:
    case TRX_STATE_COMMITTED_IN_MEMORY:
      /* The savepoint rollback is only allowed on an ACTIVE
      transaction, not a PREPARED or COMMITTED one. */
      break;
  }

  ut_error;
}

/** Creates a named savepoint. If the transaction is not yet started, starts it.
 If there is already a savepoint of the same name, this call erases that old
 savepoint and replaces it with a new. Savepoints are deleted in a transaction
 commit or rollback.
 @return always DB_SUCCESS */
dberr_t trx_savepoint_for_mysql(
    trx_t *trx,                 /*!< in: transaction handle */
    const char *savepoint_name, /*!< in: savepoint name */
    int64_t binlog_cache_pos)   /*!< in: MySQL binlog cache
                                position corresponding to this
                                connection at the time of the
                                savepoint */
{
  trx_named_savept_t *savep;

  trx_start_if_not_started_xa(trx, false, UT_LOCATION_HERE);

  savep = trx_savepoint_find(trx, savepoint_name);

  if (savep) {
    /* There is a savepoint with the same name: free that */

    UT_LIST_REMOVE(trx->trx_savepoints, savep);

    ut::free(savep->name);
    ut::free(savep);
  }

  /* Create a new savepoint and add it as the last in the list */

  savep = static_cast<trx_named_savept_t *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*savep)));

  savep->name = mem_strdup(savepoint_name);

  savep->savept = trx_savept_take(trx);

  savep->mysql_binlog_cache_pos = binlog_cache_pos;

  UT_LIST_ADD_LAST(trx->trx_savepoints, savep);

  return (DB_SUCCESS);
}

/** Releases only the named savepoint. Savepoints which were set after this
 savepoint are left as is.
 @return if no savepoint of the name found then DB_NO_SAVEPOINT,
 otherwise DB_SUCCESS */
dberr_t trx_release_savepoint_for_mysql(
    trx_t *trx,                 /*!< in: transaction handle */
    const char *savepoint_name) /*!< in: savepoint name */
{
  trx_named_savept_t *savep;

  ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));
  ut_ad(trx->in_mysql_trx_list);

  savep = trx_savepoint_find(trx, savepoint_name);

  if (savep != nullptr) {
    trx_roll_savepoint_free(trx, savep);
  }

  return (savep != nullptr ? DB_SUCCESS : DB_NO_SAVEPOINT);
}

/** Determines if this transaction is rolling back an incomplete transaction
 in crash recovery.
 @return true if trx is an incomplete transaction that is being rolled
 back in crash recovery */
bool trx_is_recv(const trx_t *trx) /*!< in: transaction */
{
  return (trx == trx_roll_crash_recv_trx);
}

/** Returns a transaction savepoint taken at this point in time.
 @return savepoint */
trx_savept_t trx_savept_take(trx_t *trx) /*!< in: transaction */
{
  trx_savept_t savept;

  savept.least_undo_no = trx->undo_no;

  return (savept);
}

/** Roll back an active transaction. */
static void trx_rollback_active(trx_t *trx) /*!< in/out: transaction */
{
  mem_heap_t *heap;
  que_fork_t *fork;
  que_thr_t *thr;
  roll_node_t *roll_node;
  int64_t rows_to_undo;
  const char *unit = "";

  heap = mem_heap_create(512, UT_LOCATION_HERE);

  fork = que_fork_create(nullptr, nullptr, QUE_FORK_RECOVERY, heap);
  fork->trx = trx;

  thr = que_thr_create(fork, heap, nullptr);

  roll_node = roll_node_create(heap);

  thr->child = roll_node;
  roll_node->common.parent = thr;

  trx->graph = fork;

  ut_a(thr == que_fork_start_command(fork));

  trx_sys_mutex_enter();

  trx_roll_crash_recv_trx = trx;

  trx_roll_max_undo_no = trx->undo_no;

  trx_roll_progress_printed_pct = 0;

  rows_to_undo = trx_roll_max_undo_no;

  trx_sys_mutex_exit();

  if (rows_to_undo > 1000000000) {
    rows_to_undo = rows_to_undo / 1000000;
    unit = "M";
  }

  const trx_id_t trx_id = trx_get_id_for_print(trx);

  ib::info(ER_IB_MSG_1186) << "Rolling back trx with id " << trx_id << ", "
                           << rows_to_undo << unit << " rows to undo";

  que_run_threads(thr);
  ut_a(roll_node->undo_thr != nullptr);

  que_run_threads(roll_node->undo_thr);

  trx_rollback_finish(thr_get_trx(roll_node->undo_thr));

  /* Free the memory reserved by the undo graph */
  que_graph_free(static_cast<que_t *>(roll_node->undo_thr->common.parent));

  ut_a(trx->lock.que_state == TRX_QUE_RUNNING);

  ib::info(ER_IB_MSG_1187) << "Rollback of trx with id " << trx_id
                           << " completed";

  mem_heap_free(heap);

  trx_roll_crash_recv_trx = nullptr;
}

/** Rollback or clean up any resurrected incomplete transactions. It assumes
 that the caller holds the trx_sys_t::mutex and it will release the
 lock if it does a clean up or rollback.
 @return true if the transaction was cleaned up or rolled back
 and trx_sys->mutex was released. */
static bool trx_rollback_or_clean_resurrected(
    trx_t *trx, /*!< in: transaction to rollback or clean */
    bool all)   /*!< in: false=roll back dictionary transactions;
                 true=roll back all non-PREPARED transactions */
{
  ut_ad(trx_sys_mutex_own());
  ut_ad(trx->in_rw_trx_list);

  /* Generally, an HA transaction with is_recovered && state==TRX_STATE_PREPARED
  can be committed or rolled back by a client who knows its XID at any time.
  To prove that no such state transition is possible while our thread operates,
  observe that we hold trx_sys->mutex which is required by both commit and
  rollback to deregister the trx from trx_sys->rw_trx_list during
  trx_release_impl_and_expl_locks() and we see the trx is still in this list.
  Thus, if we see is_recovered==true, then the state can not change until we
  release the trx_sys->mutex. Moreover for TRX_STATE_PREPARED we do nothing, so
  we will not interfere with an HA COMMIT or ROLLBACK. So, if XA ROLLBACK or
  COMMIT latches trx_sys->mutex before us, then we will not see the trx in the
  rw_trx_list (so trx_rollback_or_clean_resurrected() would not be called for
  this transaction in the first place), and if we latch first, then we will
  leave the trx intact. */

  trx_mutex_enter(trx);
  const bool is_recovered = trx->is_recovered;
  const trx_state_t state = trx->state.load(std::memory_order_relaxed);
  trx_mutex_exit(trx);

  if (!is_recovered) {
    ut_ad(state != TRX_STATE_COMMITTED_IN_MEMORY);
    return false;
  }

  switch (state) {
    case TRX_STATE_COMMITTED_IN_MEMORY:
      trx_sys_mutex_exit();
      ib::info(ER_IB_MSG_1188)
          << "Cleaning up trx with id " << trx_get_id_for_print(trx);

      trx_cleanup_at_db_startup(trx);
      trx_free_resurrected(trx);
      ut_ad(!trx->is_recovered);
      return true;
    case TRX_STATE_ACTIVE:
      if (all || trx->ddl_operation) {
        trx_sys_mutex_exit();
        trx_rollback_active(trx);
        trx_free_for_background(trx);
        ut_ad(!trx->is_recovered);
        return true;
      }
      return false;
    case TRX_STATE_PREPARED:
      return false;
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:
      break;
  }

  ut_error;
}

/** Rollback or clean up any incomplete transactions which were
 encountered in crash recovery.  If the transaction already was
 committed, then we clean up a possible insert undo log. If the
 transaction was not yet committed, then we roll it back. */
void trx_rollback_or_clean_recovered(
    bool all) /*!< in: false=roll back dictionary transactions;
               true=roll back all non-PREPARED transactions */
{
  ut_ad(!srv_read_only_mode);

  ut_a(srv_force_recovery < SRV_FORCE_NO_TRX_UNDO);
  ut_ad(!all || trx_sys_need_rollback());

  if (all) {
    ib::info(ER_IB_MSG_1189) << "Starting in background the rollback"
                                " of uncommitted transactions";
  }

  /* Note: For XA recovered transactions, we rely on MySQL to
  do rollback. They will be in TRX_STATE_PREPARED state. If the server
  is shutdown and they are still lingering in trx_sys_t::trx_list
  then the shutdown will hang. */

  /* Loop over the transaction list as long as there are
  recovered transactions to clean up or recover. */

  trx_sys_mutex_enter();
  for (bool need_one_more_scan = true; need_one_more_scan;) {
    need_one_more_scan = false;
    for (auto trx : trx_sys->rw_trx_list) {
      assert_trx_in_rw_list(trx);

      /* In case of slow shutdown, we have to wait for the background
      thread (trx_recovery_rollback) which is doing the rollbacks of
      recovered transactions. Note that it can add undo to purge.
      In case of fast shutdown we do not care if we left transactions
      not rolled back. But still we want to stop the thread, so since
      certain point of shutdown we might be sure there are no changes
      to transactions / undo. */
      if (srv_shutdown_state.load() >= SRV_SHUTDOWN_RECOVERY_ROLLBACK &&
          srv_fast_shutdown != 0) {
        ut_a(srv_shutdown_state_matches([](auto state) {
          return state == SRV_SHUTDOWN_RECOVERY_ROLLBACK ||
                 state == SRV_SHUTDOWN_EXIT_THREADS;
        }));

        trx_sys_mutex_exit();

        if (all) {
          ib::info(ER_IB_MSG_TRX_RECOVERY_ROLLBACK_NOT_COMPLETED);
        }
        return;
      }

      /* If this function does a cleanup or rollback
      then it will release the trx_sys->mutex, therefore
      we need to reacquire it before retrying the loop. */
      if (trx_rollback_or_clean_resurrected(trx, all)) {
        trx_sys_mutex_enter();
        need_one_more_scan = true;
        break;
      }
    }
  }
  trx_sys_mutex_exit();

  if (all) {
    ib::info(ER_IB_MSG_TRX_RECOVERY_ROLLBACK_COMPLETED);
  }
}

/** Rollback or clean up any incomplete transactions which were
encountered in crash recovery.  If the transaction already was
committed, then we clean up a possible insert undo log. If the
transaction was not yet committed, then we roll it back.
Note: this is done in a background thread. */
void trx_recovery_rollback(THD *thd) {
  std::vector<MDL_ticket *> shared_mdl_list;
  ut_ad(!srv_read_only_mode);

  // Take MDL locks
  /* During this stage the server is not open for external connections, and
   * there will not be any concurrent threads requesting MDL, hence we don't
   * have risk of hitting a deadlock.
   * TODO: We can improvize code by following some order while taking MDL locks.
   */
  for (const auto &element : to_rollback_trx_tables) {
    trx_id_t trx_id = element.first;
    table_id_t table_id = element.second;

    /* Passing false as we only intend to validate if the transaction is already
     * committed/rolled back during other stages of recovery. */
    auto trx = trx_rw_is_active(trx_id, false);

    /* Ignore transactions that has already finished. */
    if (trx == nullptr) {
      /* Currently these recovered transactions are not expected to finish
       * earlier. Assert in Debug mode. */
      ut_ad(false);
      continue;
    }
    auto table = dd_table_open_on_id(table_id, nullptr, nullptr, false, true);
    if (table == nullptr) {
      continue;
    }
    std::string table_name;
    std::string schema_name;
    table->get_table_name(schema_name, table_name);
    MDL_ticket *mdl_ticket;
    if (dd_mdl_acquire(thd, &mdl_ticket, schema_name.data(),
                       table_name.data())) {
      ut_error;
    }
    shared_mdl_list.push_back(mdl_ticket);
    lock_table_ix_resurrect(table, trx);
    ib::info(ER_IB_RESURRECT_ACQUIRE_TABLE_LOCK, ulong(table->id),
             table->name.m_name);
    dd_table_close(table, nullptr, nullptr, false);
  }

  /* Let the startup thread proceed now */
  os_event_set(recovery_lock_taken);

  while (DBUG_EVALUATE_IF("pause_rollback_on_recovery", true, false)) {
    if (srv_shutdown_state.load() >= SRV_SHUTDOWN_RECOVERY_ROLLBACK) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  trx_rollback_or_clean_recovered(true);

  // Release MDL locks
  for (auto mdl_ticket : shared_mdl_list) {
    dd_release_mdl(mdl_ticket);
  }
}

/** Rollback or clean up any incomplete transactions which were
encountered in crash recovery.  If the transaction already was
committed, then we clean up a possible insert undo log. If the
transaction was not yet committed, then we roll it back.
Note: this is done in a background thread. */
void trx_recovery_rollback_thread() {
  THD *thd = create_internal_thd();

  trx_recovery_rollback(thd);

  destroy_internal_thd(thd);
}

/** Tries truncate the undo logs. */
static void trx_roll_try_truncate(
    trx_t *trx,               /*!< in/out: transaction */
    trx_undo_ptr_t *undo_ptr) /*!< in: rollback segment to look
                              for next undo log record. */
{
  ut_ad(mutex_own(&trx->undo_mutex));
  ut_ad(mutex_own(&undo_ptr->rseg->mutex));

  trx->pages_undone = 0;

  if (undo_ptr->insert_undo) {
    trx_undo_truncate_end(trx, undo_ptr->insert_undo, trx->undo_no);
  }

  if (undo_ptr->update_undo) {
    trx_undo_truncate_end(trx, undo_ptr->update_undo, trx->undo_no);
  }
}

/** Pops the topmost undo log record in a single undo log and updates the info
about the topmost record in the undo log memory struct.
@param[in]      trx             transaction
@param[in]      undo            undo log
@param[in]      mtr             mtr
@param[out]     undo_offset     offset of undo record in the page
@return Undo page where undo log record resides, the page s-latched */
static const page_t *trx_roll_pop_top_rec(trx_t *trx, trx_undo_t *undo,
                                          mtr_t *mtr, uint32_t *undo_offset) {
  ut_ad(mutex_own(&trx->undo_mutex));

  const page_t *undo_page = trx_undo_page_get_s_latched(
      page_id_t(undo->space, undo->top_page_no), undo->page_size, mtr);

  *undo_offset = static_cast<uint32_t>(undo->top_offset);

  trx_undo_rec_t *prev_rec =
      trx_undo_get_prev_rec((trx_undo_rec_t *)(undo_page + *undo_offset),
                            undo->hdr_page_no, undo->hdr_offset, true, mtr);

  if (prev_rec == nullptr) {
    undo->empty = true;
  } else {
    page_t *prev_rec_page = page_align(prev_rec);

    if (prev_rec_page != undo_page) {
      trx->pages_undone++;
    }

    undo->top_page_no = page_get_page_no(prev_rec_page);
    undo->top_offset = prev_rec - prev_rec_page;
    undo->top_undo_no = trx_undo_rec_get_undo_no(prev_rec);
  }

  return (undo_page);
}

/** Pops the topmost record when the two undo logs of a transaction are seen
 as a single stack of records ordered by their undo numbers.
 @return undo log record copied to heap, NULL if none left, or if the
 undo number of the top record would be less than the limit */
static trx_undo_rec_t *trx_roll_pop_top_rec_of_trx_low(
    trx_t *trx,               /*!< in/out: transaction */
    trx_undo_ptr_t *undo_ptr, /*!< in: rollback segment to look
                              for next undo log record. */
    undo_no_t limit,          /*!< in: least undo number we need */
    roll_ptr_t *roll_ptr,     /*!< out: roll pointer to undo record */
    mem_heap_t *heap)         /*!< in/out: memory heap where copied */
{
  trx_undo_t *undo;
  trx_undo_t *ins_undo;
  trx_undo_t *upd_undo;
  trx_undo_rec_t *undo_rec_copy;
  const page_t *undo_page;
  undo_no_t undo_no;
  uint32_t undo_offset;
  trx_rseg_t *rseg;
  mtr_t mtr;

  rseg = undo_ptr->rseg;

  mutex_enter(&trx->undo_mutex);

  if (trx->pages_undone >= TRX_ROLL_TRUNC_THRESHOLD) {
    rseg->latch();

    trx_roll_try_truncate(trx, undo_ptr);

    rseg->unlatch();
  }

  ins_undo = undo_ptr->insert_undo;
  upd_undo = undo_ptr->update_undo;

  if (!ins_undo || ins_undo->empty) {
    undo = upd_undo;
  } else if (!upd_undo || upd_undo->empty) {
    undo = ins_undo;
  } else if (upd_undo->top_undo_no > ins_undo->top_undo_no) {
    undo = upd_undo;
  } else {
    undo = ins_undo;
  }

  if (!undo || undo->empty || limit > undo->top_undo_no) {
    rseg->latch();
    trx_roll_try_truncate(trx, undo_ptr);
    rseg->unlatch();
    mutex_exit(&trx->undo_mutex);
    return (nullptr);
  }

  auto is_insert = (undo == ins_undo);

  *roll_ptr = trx_undo_build_roll_ptr(is_insert, undo->rseg->space_id,
                                      undo->top_page_no, undo->top_offset);

  mtr_start(&mtr);

  undo_page = trx_roll_pop_top_rec(trx, undo, &mtr, &undo_offset);

  undo_no = trx_undo_rec_get_undo_no(undo_page + undo_offset);

  ut_ad(trx_roll_check_undo_rec_ordering(undo_no, undo->rseg->space_id, trx));

  /* We print rollback progress info if we are in a crash recovery
  and the transaction has at least 1000 row operations to undo. */

  if (trx == trx_roll_crash_recv_trx && trx_roll_max_undo_no > 1000) {
    ulint progress_pct = 100 - (ulint)((undo_no * 100) / trx_roll_max_undo_no);
    if (progress_pct != trx_roll_progress_printed_pct) {
      if (trx_roll_progress_printed_pct == 0) {
        fprintf(stderr,
                "\nInnoDB: Progress in percents:"
                " %lu",
                (ulong)progress_pct);
      } else {
        fprintf(stderr, " %lu", (ulong)progress_pct);
      }
      fflush(stderr);
      trx_roll_progress_printed_pct = progress_pct;
    }
  }

  trx->undo_no = undo_no;
  trx->undo_rseg_space = undo->rseg->space_id;

  undo_rec_copy =
      trx_undo_rec_copy(undo_page, static_cast<uint32_t>(undo_offset), heap);

  mutex_exit(&trx->undo_mutex);

  mtr_commit(&mtr);

  return (undo_rec_copy);
}

/** Get next undo log record from redo and noredo rollback segments.
 @return undo log record copied to heap, NULL if none left, or if the
 undo number of the top record would be less than the limit */
trx_undo_rec_t *trx_roll_pop_top_rec_of_trx(
    trx_t *trx,           /*!< in: transaction */
    undo_no_t limit,      /*!< in: least undo number we need */
    roll_ptr_t *roll_ptr, /*!< out: roll pointer to undo record */
    mem_heap_t *heap)     /*!< in: memory heap where copied */
{
  trx_undo_rec_t *undo_rec = nullptr;

  if (trx_is_redo_rseg_updated(trx)) {
    undo_rec = trx_roll_pop_top_rec_of_trx_low(trx, &trx->rsegs.m_redo, limit,
                                               roll_ptr, heap);
  }

  if (undo_rec == nullptr && trx_is_temp_rseg_updated(trx)) {
    undo_rec = trx_roll_pop_top_rec_of_trx_low(trx, &trx->rsegs.m_noredo, limit,
                                               roll_ptr, heap);
  }

  return (undo_rec);
}

/** Builds an undo 'query' graph for a transaction. The actual rollback is
 performed by executing this query graph like a query subprocedure call.
 The reply about the completion of the rollback will be sent by this
 graph.
@param[in,out]  trx                     transaction
@param[in]      partial_rollback        true if partial rollback
@return the query graph */
static que_t *trx_roll_graph_build(trx_t *trx, bool partial_rollback) {
  mem_heap_t *heap;
  que_fork_t *fork;
  que_thr_t *thr;

  ut_ad(trx_mutex_own(trx));

  heap = mem_heap_create(512, UT_LOCATION_HERE);
  fork = que_fork_create(nullptr, nullptr, QUE_FORK_ROLLBACK, heap);
  fork->trx = trx;

  thr = que_thr_create(fork, heap, nullptr);

  thr->child = row_undo_node_create(trx, thr, heap, partial_rollback);

  return (fork);
}

/** Starts a rollback operation, creates the UNDO graph that will do the
 actual undo operation.
@param[in]      trx     transaction
@param[in]      roll_limit       rollback to undo no (for
                                 partial undo), 0 if we are rolling back
                                 the entire transaction
@param[in]      partial_rollback true if partial rollback
@return query graph thread that will perform the UNDO operations. */
static que_thr_t *trx_rollback_start(trx_t *trx, ib_id_t roll_limit,
                                     bool partial_rollback) {
  ut_ad(trx_mutex_own(trx));

  /* Initialize the rollback field in the transaction */

  ut_ad(!trx->roll_limit);
  ut_ad(!trx->in_rollback);

  trx->roll_limit = roll_limit;
  ut_d(trx->in_rollback = true);

  ut_a(trx->roll_limit <= trx->undo_no);

  trx->pages_undone = 0;

  /* Build a 'query' graph which will perform the undo operations */

  que_t *roll_graph = trx_roll_graph_build(trx, partial_rollback);

  trx->graph = roll_graph;

  trx->lock.que_state = TRX_QUE_ROLLING_BACK;

  return (que_fork_start_command(roll_graph));
}

/** Finishes a transaction rollback. */
static void trx_rollback_finish(trx_t *trx) /*!< in: transaction */
{
  trx_commit(trx);

  trx->mod_tables.clear();

  trx->lock.que_state = TRX_QUE_RUNNING;
}

/** Creates a rollback command node struct.
 @return own: rollback node struct */
roll_node_t *roll_node_create(
    mem_heap_t *heap) /*!< in: mem heap where created */
{
  roll_node_t *node;

  node = static_cast<roll_node_t *>(mem_heap_zalloc(heap, sizeof(*node)));

  node->state = ROLL_NODE_SEND;

  node->common.type = QUE_NODE_ROLLBACK;

  return (node);
}

/** Performs an execution step for a rollback command node in a query graph.
 @return query thread to run next, or NULL */
que_thr_t *trx_rollback_step(que_thr_t *thr) /*!< in: query thread */
{
  roll_node_t *node;

  node = static_cast<roll_node_t *>(thr->run_node);

  ut_ad(que_node_get_type(node) == QUE_NODE_ROLLBACK);

  if (thr->prev_node == que_node_get_parent(node)) {
    node->state = ROLL_NODE_SEND;
  }

  if (node->state == ROLL_NODE_SEND) {
    trx_t *trx;
    ib_id_t roll_limit;

    trx = thr_get_trx(thr);

    trx_mutex_enter(trx);

    node->state = ROLL_NODE_WAIT;

    ut_a(node->undo_thr == nullptr);

    roll_limit = node->partial ? node->savept.least_undo_no : 0;

    trx_commit_or_rollback_prepare(trx);

    node->undo_thr = trx_rollback_start(trx, roll_limit, node->partial);

    trx_mutex_exit(trx);

  } else {
    ut_ad(node->state == ROLL_NODE_WAIT);

    thr->run_node = que_node_get_parent(node);
  }

  return (thr);
}
