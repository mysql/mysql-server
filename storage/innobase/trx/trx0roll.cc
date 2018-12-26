/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** true if trx_rollback_or_clean_all_recovered() thread is active */
bool trx_rollback_or_clean_is_active;

/** In crash recovery, the current trx to be rolled back; NULL otherwise */
static const trx_t *trx_roll_crash_recv_trx = NULL;

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

  heap = mem_heap_create(512);

  roll_node = roll_node_create(heap);

  if (savept != NULL) {
    roll_node->partial = TRUE;
    roll_node->savept = *savept;
    check_trx_state(trx);
  } else {
    assert_trx_nonlocking_or_in_list(trx);
  }

  trx->error_state = DB_SUCCESS;

  if (trx_is_rseg_updated(trx)) {
    ut_ad(trx->rsegs.m_redo.rseg != 0 || trx->rsegs.m_noredo.rseg != 0);

    thr = pars_complete_graph_for_exec(roll_node, trx, heap, NULL);

    ut_a(thr == que_fork_start_command(
                    static_cast<que_fork_t *>(que_node_get_parent(thr))));

    que_run_threads(thr);

    ut_a(roll_node->undo_thr != NULL);
    que_run_threads(roll_node->undo_thr);

    /* Free the memory reserved by the undo graph. */
    que_graph_free(static_cast<que_t *>(roll_node->undo_thr->common.parent));
  }

  if (savept == NULL) {
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

  trx_start_if_not_started_xa(trx, true);

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

  trx_rollback_to_savepoint_low(trx, NULL);

  trx->op_info = "";

  ut_a(trx->error_state == DB_SUCCESS);

  return (trx->error_state);
}

/** Rollback a transaction used in MySQL
@param[in, out]	trx	transaction
@return error code or DB_SUCCESS */
static dberr_t trx_rollback_low(trx_t *trx) {
  /* We are reading trx->state without holding trx_sys->mutex
  here, because the rollback should be invoked for a running
  active MySQL transaction (or recovered prepared transaction)
  that is associated with the current thread. */

  switch (trx->state) {
    case TRX_STATE_FORCED_ROLLBACK:
    case TRX_STATE_NOT_STARTED:
      trx->will_lock = 0;
      ut_ad(trx->in_mysql_trx_list);
      return (DB_SUCCESS);

    case TRX_STATE_ACTIVE:
      ut_ad(trx->in_mysql_trx_list);
      assert_trx_nonlocking_or_in_list(trx);
      return (trx_rollback_for_mysql_low(trx));

    case TRX_STATE_PREPARED:
      ut_ad(!trx_is_autocommit_non_locking(trx));
      if (trx->rsegs.m_redo.rseg != NULL && trx_is_redo_rseg_updated(trx)) {
        /* Change the undo log state back from
        TRX_UNDO_PREPARED to TRX_UNDO_ACTIVE
        so that if the system gets killed,
        recovery will perform the rollback. */
        trx_undo_ptr_t *undo_ptr = &trx->rsegs.m_redo;

        mtr_t mtr;

        mtr.start();

        mutex_enter(&trx->rsegs.m_redo.rseg->mutex);

        if (undo_ptr->insert_undo != NULL) {
          trx_undo_set_state_at_prepare(trx, undo_ptr->insert_undo, true, &mtr);
        }
        if (undo_ptr->update_undo != NULL) {
          trx_undo_set_state_at_prepare(trx, undo_ptr->update_undo, true, &mtr);
        }
        mutex_exit(&trx->rsegs.m_redo.rseg->mutex);
        /* Persist the XA ROLLBACK, so that crash
        recovery will replay the rollback in case
        the redo log gets applied past this point. */
        mtr.commit();
        ut_ad(mtr.commit_lsn() > 0);
      }
#ifdef ENABLED_DEBUG_SYNC
      if (trx->mysql_thd == NULL) {
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

  /* We are reading trx->state without holding trx_sys->mutex
  here, because the statement rollback should be invoked for a
  running active MySQL transaction that is associated with the
  current thread. */
  ut_ad(trx->in_mysql_trx_list);

  switch (trx->state) {
    case TRX_STATE_FORCED_ROLLBACK:
    case TRX_STATE_NOT_STARTED:
      return (DB_SUCCESS);

    case TRX_STATE_ACTIVE:
      assert_trx_nonlocking_or_in_list(trx);

      trx->op_info = "rollback of SQL statement";

      err = trx_rollback_to_savepoint(trx, &trx->last_sql_stat_start);

      if (trx->fts_trx != NULL) {
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
  trx_named_savept_t *savep;

  for (savep = UT_LIST_GET_FIRST(trx->trx_savepoints); savep != NULL;
       savep = UT_LIST_GET_NEXT(trx_savepoints, savep)) {
    if (0 == ut_strcmp(savep->name, name)) {
      return (savep);
    }
  }

  return (NULL);
}

/** Frees a single savepoint struct. */
static void trx_roll_savepoint_free(
    trx_t *trx,                /*!< in: transaction handle */
    trx_named_savept_t *savep) /*!< in: savepoint to free */
{
  UT_LIST_REMOVE(trx->trx_savepoints, savep);

  ut_free(savep->name);
  ut_free(savep);
}

/** Frees savepoint structs starting from savep. */
void trx_roll_savepoints_free(
    trx_t *trx,                /*!< in: transaction handle */
    trx_named_savept_t *savep) /*!< in: free all savepoints starting
                               with this savepoint i*/
{
  while (savep != NULL) {
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
static MY_ATTRIBUTE((warn_unused_result)) dberr_t
    trx_rollback_to_savepoint_for_mysql_low(
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

  /* We are reading trx->state without holding trx_sys->mutex
  here, because the savepoint rollback should be invoked for a
  running active MySQL transaction that is associated with the
  current thread. */
  ut_ad(trx->in_mysql_trx_list);

  savep = trx_savepoint_find(trx, savepoint_name);

  if (savep == NULL) {
    return (DB_NO_SAVEPOINT);
  }

  switch (trx->state) {
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
  return (DB_CORRUPTION);
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

  trx_start_if_not_started_xa(trx, false);

  savep = trx_savepoint_find(trx, savepoint_name);

  if (savep) {
    /* There is a savepoint with the same name: free that */

    UT_LIST_REMOVE(trx->trx_savepoints, savep);

    ut_free(savep->name);
    ut_free(savep);
  }

  /* Create a new savepoint and add it as the last in the list */

  savep = static_cast<trx_named_savept_t *>(ut_malloc_nokey(sizeof(*savep)));

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

  if (savep != NULL) {
    trx_roll_savepoint_free(trx, savep);
  }

  return (savep != NULL ? DB_SUCCESS : DB_NO_SAVEPOINT);
}

/** Determines if this transaction is rolling back an incomplete transaction
 in crash recovery.
 @return true if trx is an incomplete transaction that is being rolled
 back in crash recovery */
ibool trx_is_recv(const trx_t *trx) /*!< in: transaction */
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

  heap = mem_heap_create(512);

  fork = que_fork_create(NULL, NULL, QUE_FORK_RECOVERY, heap);
  fork->trx = trx;

  thr = que_thr_create(fork, heap, NULL);

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
  ut_a(roll_node->undo_thr != NULL);

  que_run_threads(roll_node->undo_thr);

  trx_rollback_finish(thr_get_trx(roll_node->undo_thr));

  /* Free the memory reserved by the undo graph */
  que_graph_free(static_cast<que_t *>(roll_node->undo_thr->common.parent));

  ut_a(trx->lock.que_state == TRX_QUE_RUNNING);

  ib::info(ER_IB_MSG_1187) << "Rollback of trx with id " << trx_id
                           << " completed";

  mem_heap_free(heap);

  trx_roll_crash_recv_trx = NULL;
}

/** Rollback or clean up any resurrected incomplete transactions. It assumes
 that the caller holds the trx_sys_t::mutex and it will release the
 lock if it does a clean up or rollback.
 @return true if the transaction was cleaned up or rolled back
 and trx_sys->mutex was released. */
static ibool trx_rollback_resurrected(
    trx_t *trx, /*!< in: transaction to rollback or clean */
    ibool all)  /*!< in: FALSE=roll back dictionary transactions;
                TRUE=roll back all non-PREPARED transactions */
{
  ut_ad(trx_sys_mutex_own());

  /* The trx->is_recovered flag and trx->state are set
  atomically under the protection of the trx->mutex (and
  lock_sys->mutex) in lock_trx_release_locks(). We do not want
  to accidentally clean up a non-recovered transaction here. */

  trx_mutex_enter(trx);
  bool is_recovered = trx->is_recovered;
  trx_state_t state = trx->state;
  trx_mutex_exit(trx);

  if (!is_recovered) {
    return (FALSE);
  }

  switch (state) {
    case TRX_STATE_COMMITTED_IN_MEMORY:
      trx_sys_mutex_exit();
      ib::info(ER_IB_MSG_1188)
          << "Cleaning up trx with id " << trx_get_id_for_print(trx);

      trx_cleanup_at_db_startup(trx);
      trx_free_resurrected(trx);
      return (TRUE);
    case TRX_STATE_ACTIVE:
      if (all || trx->ddl_operation) {
        trx_sys_mutex_exit();
        trx_rollback_active(trx);
        trx_free_for_background(trx);
        return (TRUE);
      }
      return (FALSE);
    case TRX_STATE_PREPARED:
      return (FALSE);
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:
      break;
  }

  ut_error;
  return (FALSE);
}

/** Rollback or clean up any incomplete transactions which were
 encountered in crash recovery.  If the transaction already was
 committed, then we clean up a possible insert undo log. If the
 transaction was not yet committed, then we roll it back. */
void trx_rollback_or_clean_recovered(
    ibool all) /*!< in: FALSE=roll back dictionary transactions;
               TRUE=roll back all non-PREPARED transactions */
{
  trx_t *trx;

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

  do {
    trx_sys_mutex_enter();

    for (trx = UT_LIST_GET_FIRST(trx_sys->rw_trx_list); trx != NULL;
         trx = UT_LIST_GET_NEXT(trx_list, trx)) {
      assert_trx_in_rw_list(trx);

      /* If this function does a cleanup or rollback
      then it will release the trx_sys->mutex, therefore
      we need to reacquire it before retrying the loop. */

      if (trx_rollback_resurrected(trx, all)) {
        trx_sys_mutex_enter();

        break;
      }
    }

    trx_sys_mutex_exit();

  } while (trx != NULL);

  if (all) {
    ib::info(ER_IB_MSG_1190) << "Rollback of non-prepared transactions"
                                " completed";
  }
}

/** Rollback or clean up any incomplete transactions which were
encountered in crash recovery.  If the transaction already was
committed, then we clean up a possible insert undo log. If the
transaction was not yet committed, then we roll it back.
Note: this is done in a background thread. */
void trx_recovery_rollback_thread() {
#ifdef UNIV_PFS_THREAD
  THD *thd =
      create_thd(false, true, true, trx_recovery_rollback_thread_key.m_value);
#else
  THD *thd = create_thd(false, true, true, 0);
#endif /* UNIV_PFS_THREAD */

  my_thread_init();

  ut_ad(!srv_read_only_mode);

  trx_rollback_or_clean_recovered(TRUE);

  trx_rollback_or_clean_is_active = false;

  destroy_thd(thd);

  my_thread_end();
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
 @return undo log record, the page s-latched */
static trx_undo_rec_t *trx_roll_pop_top_rec(
    trx_t *trx,       /*!< in: transaction */
    trx_undo_t *undo, /*!< in: undo log */
    mtr_t *mtr)       /*!< in: mtr */
{
  ut_ad(mutex_own(&trx->undo_mutex));

  page_t *undo_page = trx_undo_page_get_s_latched(
      page_id_t(undo->space, undo->top_page_no), undo->page_size, mtr);

  ulint offset = undo->top_offset;

  trx_undo_rec_t *prev_rec = trx_undo_get_prev_rec(
      undo_page + offset, undo->hdr_page_no, undo->hdr_offset, true, mtr);

  if (prev_rec == NULL) {
    undo->empty = TRUE;
  } else {
    page_t *prev_rec_page = page_align(prev_rec);

    if (prev_rec_page != undo_page) {
      trx->pages_undone++;
    }

    undo->top_page_no = page_get_page_no(prev_rec_page);
    undo->top_offset = prev_rec - prev_rec_page;
    undo->top_undo_no = trx_undo_rec_get_undo_no(prev_rec);
  }

  return (undo_page + offset);
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
  trx_undo_rec_t *undo_rec;
  trx_undo_rec_t *undo_rec_copy;
  undo_no_t undo_no;
  ibool is_insert;
  trx_rseg_t *rseg;
  mtr_t mtr;

  rseg = undo_ptr->rseg;

  mutex_enter(&trx->undo_mutex);

  if (trx->pages_undone >= TRX_ROLL_TRUNC_THRESHOLD) {
    mutex_enter(&rseg->mutex);

    trx_roll_try_truncate(trx, undo_ptr);

    mutex_exit(&rseg->mutex);
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
    mutex_enter(&rseg->mutex);
    trx_roll_try_truncate(trx, undo_ptr);
    mutex_exit(&rseg->mutex);
    mutex_exit(&trx->undo_mutex);
    return (NULL);
  }

  is_insert = (undo == ins_undo);

  *roll_ptr = trx_undo_build_roll_ptr(is_insert, undo->rseg->space_id,
                                      undo->top_page_no, undo->top_offset);

  mtr_start(&mtr);

  undo_rec = trx_roll_pop_top_rec(trx, undo, &mtr);

  undo_no = trx_undo_rec_get_undo_no(undo_rec);

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

  undo_rec_copy = trx_undo_rec_copy(undo_rec, heap);

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
  trx_undo_rec_t *undo_rec = 0;

  if (trx_is_redo_rseg_updated(trx)) {
    undo_rec = trx_roll_pop_top_rec_of_trx_low(trx, &trx->rsegs.m_redo, limit,
                                               roll_ptr, heap);
  }

  if (undo_rec == 0 && trx_is_temp_rseg_updated(trx)) {
    undo_rec = trx_roll_pop_top_rec_of_trx_low(trx, &trx->rsegs.m_noredo, limit,
                                               roll_ptr, heap);
  }

  return (undo_rec);
}

/** Builds an undo 'query' graph for a transaction. The actual rollback is
 performed by executing this query graph like a query subprocedure call.
 The reply about the completion of the rollback will be sent by this
 graph.
 @return own: the query graph */
static que_t *trx_roll_graph_build(trx_t *trx) /*!< in/out: transaction */
{
  mem_heap_t *heap;
  que_fork_t *fork;
  que_thr_t *thr;

  ut_ad(trx_mutex_own(trx));

  heap = mem_heap_create(512);
  fork = que_fork_create(NULL, NULL, QUE_FORK_ROLLBACK, heap);
  fork->trx = trx;

  thr = que_thr_create(fork, heap, NULL);

  thr->child = row_undo_node_create(trx, thr, heap);

  return (fork);
}

/** Starts a rollback operation, creates the UNDO graph that will do the
 actual undo operation.
 @return query graph thread that will perform the UNDO operations. */
static que_thr_t *trx_rollback_start(
    trx_t *trx,         /*!< in: transaction */
    ib_id_t roll_limit) /*!< in: rollback to undo no (for
                        partial undo), 0 if we are rolling back
                        the entire transaction */
{
  ut_ad(trx_mutex_own(trx));

  /* Initialize the rollback field in the transaction */

  ut_ad(!trx->roll_limit);
  ut_ad(!trx->in_rollback);

  trx->roll_limit = roll_limit;
  ut_d(trx->in_rollback = true);

  ut_a(trx->roll_limit <= trx->undo_no);

  trx->pages_undone = 0;

  /* Build a 'query' graph which will perform the undo operations */

  que_t *roll_graph = trx_roll_graph_build(trx);

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

    ut_a(node->undo_thr == NULL);

    roll_limit = node->partial ? node->savept.least_undo_no : 0;

    trx_commit_or_rollback_prepare(trx);

    node->undo_thr = trx_rollback_start(trx, roll_limit);

    trx_mutex_exit(trx);

  } else {
    ut_ad(node->state == ROLL_NODE_WAIT);

    thr->run_node = que_node_get_parent(node);
  }

  return (thr);
}
