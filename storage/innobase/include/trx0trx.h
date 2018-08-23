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

/** @file include/trx0trx.h
 The transaction

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#ifndef trx0trx_h
#define trx0trx_h

#include <list>
#include <set>

#include "ha_prototypes.h"

#include "dict0types.h"
#include "sql/handler.h"
#include "trx0types.h"
#include "ut0new.h"

#include "lock0types.h"
#include "log0log.h"
#include "mem0mem.h"
#include "que0types.h"
#include "trx0xa.h"
#include "usr0types.h"
#include "ut0vec.h"
#ifndef UNIV_HOTBACKUP
#include "fts0fts.h"
#endif /* !UNIV_HOTBACKUP */
#include "srv0srv.h"

// Forward declaration
struct mtr_t;

// Forward declaration
class ReadView;

// Forward declaration
class FlushObserver;

/** Dummy session used currently in MySQL interface */
extern sess_t *trx_dummy_sess;

#ifndef UNIV_HOTBACKUP
/** Set flush observer for the transaction
@param[in,out]	trx		transaction struct
@param[in]	observer	flush observer */
void trx_set_flush_observer(trx_t *trx, FlushObserver *observer);

/** Set detailed error message for the transaction. */
void trx_set_detailed_error(trx_t *trx,       /*!< in: transaction struct */
                            const char *msg); /*!< in: detailed error message */
/** Set detailed error message for the transaction from a file. Note that the
 file is rewinded before reading from it. */
void trx_set_detailed_error_from_file(
    trx_t *trx,  /*!< in: transaction struct */
    FILE *file); /*!< in: file to read message from */
/** Retrieves the error_info field from a trx.
 @return the error info */
UNIV_INLINE
const dict_index_t *trx_get_error_info(const trx_t *trx); /*!< in: trx object */
/** Creates a transaction object for MySQL.
 @return own: transaction object */
trx_t *trx_allocate_for_mysql(void);
/** Creates a transaction object for background operations by the master thread.
 @return own: transaction object */
trx_t *trx_allocate_for_background(void);

/** Resurrect table locks for resurrected transactions. */
void trx_resurrect_locks();

/** Free and initialize a transaction object instantiated during recovery.
@param[in,out]	trx	transaction object to free and initialize */
void trx_free_resurrected(trx_t *trx);

/** Free a transaction that was allocated by background or user threads.
@param[in,out]	trx	transaction object to free */
void trx_free_for_background(trx_t *trx);

/** At shutdown, frees a transaction object that is in the PREPARED state. */
void trx_free_prepared(trx_t *trx); /*!< in, own: trx object */

/** Free a transaction object for MySQL.
@param[in,out]	trx	transaction */
void trx_free_for_mysql(trx_t *trx);

/** Disconnect a transaction from MySQL.
@param[in,out]	trx	transaction */
void trx_disconnect_plain(trx_t *trx);

/** Disconnect a prepared transaction from MySQL.
@param[in,out]	trx	transaction */
void trx_disconnect_prepared(trx_t *trx);

/** Creates trx objects for transactions and initializes the trx list of
 trx_sys at database start. Rollback segment and undo log lists must
 already exist when this function is called, because the lists of
 transactions to be rolled back or cleaned up are built based on the
 undo log lists. */
void trx_lists_init_at_db_start(void);

/** Starts the transaction if it is not yet started. */
void trx_start_if_not_started_xa_low(
    trx_t *trx,       /*!< in/out: transaction */
    bool read_write); /*!< in: true if read write transaction */
/** Starts the transaction if it is not yet started. */
void trx_start_if_not_started_low(
    trx_t *trx,       /*!< in/out: transaction */
    bool read_write); /*!< in: true if read write transaction */

/** Starts a transaction for internal processing. */
void trx_start_internal_low(trx_t *trx); /*!< in/out: transaction */

/** Starts a read-only transaction for internal processing.
@param[in,out] trx	transaction to be started */
void trx_start_internal_read_only_low(trx_t *trx);

#ifdef UNIV_DEBUG
#define trx_start_if_not_started_xa(t, rw)    \
  do {                                        \
    (t)->start_line = __LINE__;               \
    (t)->start_file = __FILE__;               \
    trx_start_if_not_started_xa_low((t), rw); \
  } while (false)

#define trx_start_if_not_started(t, rw)    \
  do {                                     \
    (t)->start_line = __LINE__;            \
    (t)->start_file = __FILE__;            \
    trx_start_if_not_started_low((t), rw); \
  } while (false)

#define trx_start_internal(t)    \
  do {                           \
    (t)->start_line = __LINE__;  \
    (t)->start_file = __FILE__;  \
    trx_start_internal_low((t)); \
  } while (false)

#define trx_start_internal_read_only(t)  \
  do {                                   \
    (t)->start_line = __LINE__;          \
    (t)->start_file = __FILE__;          \
    trx_start_internal_read_only_low(t); \
  } while (false)
#else
#define trx_start_if_not_started(t, rw) trx_start_if_not_started_low((t), rw)

#define trx_start_internal(t) trx_start_internal_low((t))

#define trx_start_internal_read_only(t) trx_start_internal_read_only_low(t)

#define trx_start_if_not_started_xa(t, rw) \
  trx_start_if_not_started_xa_low((t), (rw))
#endif /* UNIV_DEBUG */

/** Commits a transaction. */
void trx_commit(trx_t *trx); /*!< in/out: transaction */

/** Commits a transaction and a mini-transaction. */
void trx_commit_low(
    trx_t *trx,  /*!< in/out: transaction */
    mtr_t *mtr); /*!< in/out: mini-transaction (will be committed),
                 or NULL if trx made no modifications */
/** Cleans up a transaction at database startup. The cleanup is needed if
 the transaction already got to the middle of a commit when the database
 crashed, and we cannot roll it back. */
void trx_cleanup_at_db_startup(trx_t *trx); /*!< in: transaction */
/** Does the transaction commit for MySQL.
 @return DB_SUCCESS or error number */
dberr_t trx_commit_for_mysql(trx_t *trx); /*!< in/out: transaction */

/**
Does the transaction prepare for MySQL.
@param[in, out] trx		Transaction instance to prepare */

dberr_t trx_prepare_for_mysql(trx_t *trx);

/** This function is used to find number of prepared transactions and
 their transaction objects for a recovery.
 @return number of prepared transactions */
int trx_recover_for_mysql(
    XA_recover_txn *txn_list, /*!< in/out: prepared transactions */
    ulint len,                /*!< in: number of slots in xid_list */
    MEM_ROOT *mem_root);      /*!< in: memory for table names */
/** This function is used to find one X/Open XA distributed transaction
 which is in the prepared state
 @return trx or NULL; on match, the trx->xid will be invalidated;
 note that the trx may have been committed, unless the caller is
 holding lock_sys->mutex */
trx_t *trx_get_trx_by_xid(
    const XID *xid); /*!< in: X/Open XA transaction identifier */
/** If required, flushes the log to disk if we called trx_commit_for_mysql()
 with trx->flush_log_later == TRUE. */
void trx_commit_complete_for_mysql(trx_t *trx); /*!< in/out: transaction */
/** Marks the latest SQL statement ended. */
void trx_mark_sql_stat_end(trx_t *trx); /*!< in: trx handle */
/** Assigns a read view for a consistent read query. All the consistent reads
 within the same transaction will get the same read view, which is created
 when this function is first called for a new started transaction. */
ReadView *trx_assign_read_view(trx_t *trx); /*!< in: active transaction */

/** @return the transaction's read view or NULL if one not assigned. */
UNIV_INLINE
ReadView *trx_get_read_view(trx_t *trx);

/** @return the transaction's read view or NULL if one not assigned. */
UNIV_INLINE
const ReadView *trx_get_read_view(const trx_t *trx);

/** Prepares a transaction for commit/rollback. */
void trx_commit_or_rollback_prepare(trx_t *trx); /*!< in/out: transaction */
/** Creates a commit command node struct.
 @return own: commit node struct */
commit_node_t *trx_commit_node_create(
    mem_heap_t *heap); /*!< in: mem heap where created */
/** Performs an execution step for a commit type node in a query graph.
 @return query thread to run next, or NULL */
que_thr_t *trx_commit_step(que_thr_t *thr); /*!< in: query thread */

/** Prints info about a transaction.
 Caller must hold trx_sys->mutex. */
void trx_print_low(FILE *f,
                   /*!< in: output stream */
                   const trx_t *trx,
                   /*!< in: transaction */
                   ulint max_query_len,
                   /*!< in: max query length to print,
                   or 0 to use the default max length */
                   ulint n_rec_locks,
                   /*!< in: lock_number_of_rows_locked(&trx->lock) */
                   ulint n_trx_locks,
                   /*!< in: length of trx->lock.trx_locks */
                   ulint heap_size);
/*!< in: mem_heap_get_size(trx->lock.lock_heap) */

/** Prints info about a transaction.
 The caller must hold lock_sys->mutex and trx_sys->mutex.
 When possible, use trx_print() instead. */
void trx_print_latched(
    FILE *f,              /*!< in: output stream */
    const trx_t *trx,     /*!< in: transaction */
    ulint max_query_len); /*!< in: max query length to print,
                          or 0 to use the default max length */

/** Prints info about a transaction.
 Acquires and releases lock_sys->mutex and trx_sys->mutex. */
void trx_print(FILE *f,              /*!< in: output stream */
               const trx_t *trx,     /*!< in: transaction */
               ulint max_query_len); /*!< in: max query length to print,
                                     or 0 to use the default max length */

/** Determine if a transaction is a dictionary operation.
 @return dictionary operation mode */
UNIV_INLINE
enum trx_dict_op_t trx_get_dict_operation(
    const trx_t *trx) /*!< in: transaction */
    MY_ATTRIBUTE((warn_unused_result));

/** Flag a transaction a dictionary operation.
@param[in,out]	trx	transaction
@param[in]	op	operation, not TRX_DICT_OP_NONE */
UNIV_INLINE
void trx_set_dict_operation(trx_t *trx, enum trx_dict_op_t op);

/** Determines if a transaction is in the given state.
 The caller must hold trx_sys->mutex, or it must be the thread
 that is serving a running transaction.
 A running RW transaction must be in trx_sys->rw_trx_list.
 @return true if trx->state == state */
UNIV_INLINE
bool trx_state_eq(const trx_t *trx,  /*!< in: transaction */
                  trx_state_t state) /*!< in: state */
    MY_ATTRIBUTE((warn_unused_result));
#ifdef UNIV_DEBUG
/** Asserts that a transaction has been started.
 The caller must hold trx_sys->mutex.
 @return true if started */
ibool trx_assert_started(const trx_t *trx) /*!< in: transaction */
    MY_ATTRIBUTE((warn_unused_result));
#endif /* UNIV_DEBUG */

/** Determines if the currently running transaction has been interrupted.
 @return true if interrupted */
ibool trx_is_interrupted(const trx_t *trx); /*!< in: transaction */
/** Determines if the currently running transaction is in strict mode.
 @return true if strict */
ibool trx_is_strict(trx_t *trx); /*!< in: transaction */

/** Calculates the "weight" of a transaction. The weight of one transaction
 is estimated as the number of altered rows + the number of locked rows.
 @param t transaction
 @return transaction weight */
#define TRX_WEIGHT(t) ((t)->undo_no + UT_LIST_GET_LEN((t)->lock.trx_locks))

/** Compares the "weight" (or size) of two transactions. Transactions that
 have edited non-transactional tables are considered heavier than ones
 that have not.
 @return true if weight(a) >= weight(b) */
bool trx_weight_ge(const trx_t *a,  /*!< in: the transaction to be compared */
                   const trx_t *b); /*!< in: the transaction to be compared */
/* Maximum length of a string that can be returned by
trx_get_que_state_str(). */
#define TRX_QUE_STATE_STR_MAX_LEN 12 /* "ROLLING BACK" */

/** Retrieves transaction's que state in a human readable string. The string
 should not be free()'d or modified.
 @return string in the data segment */
UNIV_INLINE
const char *trx_get_que_state_str(const trx_t *trx); /*!< in: transaction */

/** Retreieves the transaction ID.
In a given point in time it is guaranteed that IDs of the running
transactions are unique. The values returned by this function for readonly
transactions may be reused, so a subsequent RO transaction may get the same ID
as a RO transaction that existed in the past. The values returned by this
function should be used for printing purposes only.
@param[in]	trx	transaction whose id to retrieve
@return transaction id */
UNIV_INLINE
trx_id_t trx_get_id_for_print(const trx_t *trx);

/** Assign a temp-tablespace bound rollback-segment to a transaction.
@param[in,out]	trx	transaction that involves write to temp-table. */
void trx_assign_rseg_temp(trx_t *trx);

/** Create the trx_t pool */
void trx_pool_init();

/** Destroy the trx_t pool */
void trx_pool_close();

/**
Set the transaction as a read-write transaction if it is not already
tagged as such.
@param[in,out] trx	Transaction that needs to be "upgraded" to RW from RO */
void trx_set_rw_mode(trx_t *trx);

/**
Increase the reference count. If the transaction is in state
TRX_STATE_COMMITTED_IN_MEMORY then the transaction is considered
committed and the reference count is not incremented.
@param trx Transaction that is being referenced
@param do_ref_count Increment the reference iff this is true
@return transaction instance if it is not committed */
UNIV_INLINE
trx_t *trx_reference(trx_t *trx, bool do_ref_count);

/**
Release the transaction. Decrease the reference count.
@param trx Transaction that is being released */
UNIV_INLINE
void trx_release_reference(trx_t *trx);

/**
Check if the transaction is being referenced. */
#define trx_is_referenced(t) ((t)->n_ref > 0)

/**
@param[in] requestor	Transaction requesting the lock
@param[in] holder	Transaction holding the lock
@return the transaction that will be rolled back, null don't care */

UNIV_INLINE
const trx_t *trx_arbitrate(const trx_t *requestor, const trx_t *holder);

/**
@param[in] trx		Transaction to check
@return true if the transaction is a high priority transaction.*/
UNIV_INLINE
bool trx_is_high_priority(const trx_t *trx);

/**
Kill all transactions that are blocking this transaction from acquiring locks.
@param[in,out] trx	High priority transaction */

void trx_kill_blocking(trx_t *trx);

/**
Check if redo/noredo rseg is modified for insert/update.
@param[in] trx		Transaction to check */
UNIV_INLINE
bool trx_is_rseg_updated(const trx_t *trx);

/**
Transactions that aren't started by the MySQL server don't set
the trx_t::mysql_thd field. For such transactions we set the lock
wait timeout to 0 instead of the user configured value that comes
from innodb_lock_wait_timeout via trx_t::mysql_thd.
@param	t transaction
@return lock wait timeout in seconds */
#define trx_lock_wait_timeout_get(t) \
  ((t)->mysql_thd != NULL ? thd_lock_wait_timeout((t)->mysql_thd) : 0)

/**
Determine if the transaction is a non-locking autocommit select
(implied read-only).
@param t transaction
@return true if non-locking autocommit select transaction. */
#define trx_is_autocommit_non_locking(t) \
  ((t)->auto_commit && (t)->will_lock == 0)

/**
Determine if the transaction is a non-locking autocommit select
with an explicit check for the read-only status.
@param t transaction
@return true if non-locking autocommit read-only transaction. */
#define trx_is_ac_nl_ro(t) \
  ((t)->read_only && trx_is_autocommit_non_locking((t)))

/**
Assert that the transaction is in the trx_sys_t::rw_trx_list */
#define assert_trx_in_rw_list(t)                         \
  do {                                                   \
    ut_ad(!(t)->read_only);                              \
    ut_ad((t)->in_rw_trx_list ==                         \
          !((t)->read_only || !(t)->rsegs.m_redo.rseg)); \
    check_trx_state(t);                                  \
  } while (0)

/**
Check transaction state */
#define check_trx_state(t)                      \
  do {                                          \
    ut_ad(!trx_is_autocommit_non_locking((t))); \
    switch ((t)->state) {                       \
      case TRX_STATE_PREPARED:                  \
        /* fall through */                      \
      case TRX_STATE_ACTIVE:                    \
      case TRX_STATE_COMMITTED_IN_MEMORY:       \
        continue;                               \
      case TRX_STATE_NOT_STARTED:               \
      case TRX_STATE_FORCED_ROLLBACK:           \
        break;                                  \
    }                                           \
    ut_error;                                   \
  } while (0)

/** Check if transaction is free so that it can be re-initialized.
@param t transaction handle */
#define assert_trx_is_free(t)                            \
  do {                                                   \
    ut_ad(trx_state_eq((t), TRX_STATE_NOT_STARTED) ||    \
          trx_state_eq((t), TRX_STATE_FORCED_ROLLBACK)); \
    ut_ad(!trx_is_rseg_updated(trx));                    \
    ut_ad(!MVCC::is_view_active((t)->read_view));        \
    ut_ad((t)->lock.wait_thr == NULL);                   \
    ut_ad(UT_LIST_GET_LEN((t)->lock.trx_locks) == 0);    \
    ut_ad((t)->dict_operation == TRX_DICT_OP_NONE);      \
  } while (0)

/** Check if transaction is in-active so that it can be freed and put back to
transaction pool.
@param t transaction handle */
#define assert_trx_is_inactive(t)              \
  do {                                         \
    assert_trx_is_free((t));                   \
    ut_ad((t)->dict_operation_lock_mode == 0); \
  } while (0)

#ifdef UNIV_DEBUG
/** Assert that an autocommit non-locking select cannot be in the
 rw_trx_list and that it is a read-only transaction.
 The tranasction must be in the mysql_trx_list. */
#define assert_trx_nonlocking_or_in_list(t)         \
  do {                                              \
    if (trx_is_autocommit_non_locking(t)) {         \
      trx_state_t t_state = (t)->state;             \
      ut_ad((t)->read_only);                        \
      ut_ad(!(t)->is_recovered);                    \
      ut_ad(!(t)->in_rw_trx_list);                  \
      ut_ad((t)->in_mysql_trx_list);                \
      ut_ad(t_state == TRX_STATE_NOT_STARTED ||     \
            t_state == TRX_STATE_FORCED_ROLLBACK || \
            t_state == TRX_STATE_ACTIVE);           \
    } else {                                        \
      check_trx_state(t);                           \
    }                                               \
  } while (0)
#else /* UNIV_DEBUG */
/** Assert that an autocommit non-locking slect cannot be in the
 rw_trx_list and that it is a read-only transaction.
 The tranasction must be in the mysql_trx_list. */
#define assert_trx_nonlocking_or_in_list(trx) ((void)0)
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

typedef std::vector<ib_lock_t *, ut_allocator<ib_lock_t *>> lock_pool_t;

/** Latching protocol for trx_lock_t::que_state.  trx_lock_t::que_state
 captures the state of the query thread during the execution of a query.
 This is different from a transaction state. The query state of a transaction
 can be updated asynchronously by other threads.  The other threads can be
 system threads, like the timeout monitor thread or user threads executing
 other queries. Another thing to be mindful of is that there is a delay between
 when a query thread is put into LOCK_WAIT state and before it actually starts
 waiting.  Between these two events it is possible that the query thread is
 granted the lock it was waiting for, which implies that the state can be
 changed asynchronously.

 All these operations take place within the context of locking. Therefore state
 changes within the locking code must acquire both the lock mutex and the
 trx->mutex when changing trx->lock.que_state to TRX_QUE_LOCK_WAIT or
 trx->lock.wait_lock to non-NULL but when the lock wait ends it is sufficient
 to only acquire the trx->mutex.
 To query the state either of the mutexes is sufficient within the locking
 code and no mutex is required when the query thread is no longer waiting. */

/** The locks and state of an active transaction. Protected by
lock_sys->mutex, trx->mutex or both. */
struct trx_lock_t {
  ulint n_active_thrs; /*!< number of active query threads */

  trx_que_t que_state; /*!< valid when trx->state
                       == TRX_STATE_ACTIVE: TRX_QUE_RUNNING,
                       TRX_QUE_LOCK_WAIT, ... */

  lock_t *wait_lock;         /*!< if trx execution state is
                             TRX_QUE_LOCK_WAIT, this points to
                             the lock request, otherwise this is
                             NULL; set to non-NULL when holding
                             both trx->mutex and lock_sys->mutex;
                             set to NULL when holding
                             lock_sys->mutex; readers should
                             hold lock_sys->mutex, except when
                             they are holding trx->mutex and
                             wait_lock==NULL */
  ib_uint64_t deadlock_mark; /*!< A mark field that is initialized
                             to and checked against lock_mark_counter
                             by lock_deadlock_recursive(). */
  bool was_chosen_as_deadlock_victim;
  /*!< when the transaction decides to
  wait for a lock, it sets this to false;
  if another transaction chooses this
  transaction as a victim in deadlock
  resolution, it sets this to true.
  Protected by trx->mutex. */
  time_t wait_started; /*!< lock wait started at this time,
                       protected only by lock_sys->mutex */

  que_thr_t *wait_thr; /*!< query thread belonging to this
                       trx that is in QUE_THR_LOCK_WAIT
                       state. For threads suspended in a
                       lock wait, this is protected by
                       lock_sys->mutex. Otherwise, this may
                       only be modified by the thread that is
                       serving the running transaction. */

  lock_pool_t rec_pool; /*!< Pre-allocated record locks */

  lock_pool_t table_pool; /*!< Pre-allocated table locks */

  ulint rec_cached; /*!< Next free rec lock in pool */

  ulint table_cached; /*!< Next free table lock in pool */

  mem_heap_t *lock_heap; /*!< memory heap for trx_locks;
                         protected by lock_sys->mutex */

  trx_lock_list_t trx_locks; /*!< locks requested by the transaction;
                             insertions are protected by trx->mutex
                             and lock_sys->mutex; removals are
                             protected by lock_sys->mutex */

  lock_pool_t table_locks; /*!< All table locks requested by this
                           transaction, including AUTOINC locks */

  bool cancel;       /*!< true if the transaction is being
                     rolled back either via deadlock
                     detection or due to lock timeout. The
                     caller has to acquire the trx_t::mutex
                     in order to cancel the locks. In
                     lock_trx_table_locks_remove() we
                     check for this cancel of a transaction's
                     locks and avoid reacquiring the trx
                     mutex to prevent recursive deadlocks.
                     Protected by both the lock sys mutex
                     and the trx_t::mutex. */
  ulint n_rec_locks; /*!< number of rec locks in this trx */
#ifdef UNIV_DEBUG
  /** When a transaction is forced to rollback due to a deadlock
  check or by another high priority transaction this is true. Used
  by debug checks in lock0lock.cc */
  bool in_rollback;
#endif /* UNIV_DEBUG */

  /** The transaction called ha_innobase::start_stmt() to
  lock a table. Most likely a temporary table. */
  bool start_stmt;
};

/** Type used to store the list of tables that are modified by a given
transaction. We store pointers to the table objects in memory because
we know that a table object will not be destroyed while a transaction
that modified it is running. */
typedef std::set<dict_table_t *, std::less<dict_table_t *>,
                 ut_allocator<dict_table_t *>>
    trx_mod_tables_t;

/** The transaction handle

Normally, there is a 1:1 relationship between a transaction handle
(trx) and a session (client connection). One session is associated
with exactly one user transaction. There are some exceptions to this:

* For DDL operations, a subtransaction is allocated that modifies the
data dictionary tables. Lock waits and deadlocks are prevented by
acquiring the dict_operation_lock before starting the subtransaction
and releasing it after committing the subtransaction.

* The purge system uses a special transaction that is not associated
with any session.

* If the system crashed or it was quickly shut down while there were
transactions in the ACTIVE or PREPARED state, these transactions would
no longer be associated with a session when the server is restarted.

A session may be served by at most one thread at a time. The serving
thread of a session might change in some MySQL implementations.
Therefore we do not have os_thread_get_curr_id() assertions in the code.

Normally, only the thread that is currently associated with a running
transaction may access (read and modify) the trx object, and it may do
so without holding any mutex. The following are exceptions to this:

* trx_rollback_resurrected() may access resurrected (connectionless)
transactions while the system is already processing new user
transactions. The trx_sys->mutex prevents a race condition between it
and lock_trx_release_locks() [invoked by trx_commit()].

* Print of transactions may access transactions not associated with
the current thread. The caller must be holding trx_sys->mutex and
lock_sys->mutex.

* When a transaction handle is in the trx_sys->mysql_trx_list or
trx_sys->trx_list, some of its fields must not be modified without
holding trx_sys->mutex exclusively.

* The locking code (in particular, deadlock checking and implicit to
explicit conversion) will access transactions associated to other
connections. The locks of transactions are protected by lock_sys->mutex
and sometimes by trx->mutex.

* Killing of asynchronous transactions. */

/** Represents an instance of rollback segment along with its state variables.*/
struct trx_undo_ptr_t {
  trx_rseg_t *rseg;        /*!< rollback segment assigned to the
                           transaction, or NULL if not assigned
                           yet */
  trx_undo_t *insert_undo; /*!< pointer to the insert undo log, or
                           NULL if no inserts performed yet */
  trx_undo_t *update_undo; /*!< pointer to the update undo log, or
                           NULL if no update performed yet */
};

/** Rollback segments assigned to a transaction for undo logging. */
struct trx_rsegs_t {
  /** undo log ptr holding reference to a rollback segment that resides in
  system/undo tablespace used for undo logging of tables that needs
  to be recovered on crash. */
  trx_undo_ptr_t m_redo;

  /** undo log ptr holding reference to a rollback segment that resides in
  temp tablespace used for undo logging of tables that doesn't need
  to be recovered on crash. */
  trx_undo_ptr_t m_noredo;
};

enum trx_rseg_type_t {
  TRX_RSEG_TYPE_NONE = 0, /*!< void rollback segment type. */
  TRX_RSEG_TYPE_REDO,     /*!< redo rollback segment. */
  TRX_RSEG_TYPE_NOREDO    /*!< non-redo rollback segment. */
};

struct TrxVersion {
  TrxVersion(trx_t *trx);

  /**
  @return true if the trx_t instance is the same */
  bool operator==(const TrxVersion &rhs) const { return (rhs.m_trx == m_trx); }

  trx_t *m_trx;
  ulint m_version;
};

typedef std::list<TrxVersion, ut_allocator<TrxVersion>> hit_list_t;

struct trx_t {
  enum isolation_level_t {

    /** dirty read: non-locking SELECTs are performed so that we
    do not look at a possible earlier version of a record; thus
    they are not 'consistent' reads under this isolation level;
    otherwise like level 2 */
    READ_UNCOMMITTED,

    /** somewhat Oracle-like isolation, except that in range UPDATE
    and DELETE we must block phantom rows with next-key locks;
    SELECT ... FOR UPDATE and ...  LOCK IN SHARE MODE only lock
    the index records, NOT the gaps before them, and thus allow
    free inserting; each consistent read reads its own snapshot */
    READ_COMMITTED,

    /** this is the default; all consistent reads in the same trx
    read the same snapshot; full next-key locking used in locking
    reads to block insertions into gaps */
    REPEATABLE_READ,

    /** all plain SELECTs are converted to LOCK IN SHARE MODE
    reads */
    SERIALIZABLE
  };

  TrxMutex mutex; /*!< Mutex protecting the fields
                  state and lock (except some fields
                  of lock, which are protected by
                  lock_sys->mutex) */

  bool owns_mutex; /*!< Set to the transaction that owns
                   the mutex during lock acquire and/or
                   release.

                   This is used to avoid taking the
                   trx_t::mutex recursively. */

  /* Note: in_depth was split from in_innodb for fixing a RO
  performance issue. Acquiring the trx_t::mutex for each row
  costs ~3% in performance. It is not required for correctness.
  Therefore we increment/decrement in_depth without holding any
  mutex. The assumption is that the Server will only ever call
  the handler from one thread. This is not true for kill_connection.
  Therefore in innobase_kill_connection. We don't increment this
  counter via TrxInInnoDB. */

  ib_uint32_t in_depth; /*!< Track nested TrxInInnoDB
                        count */

  ib_uint32_t in_innodb; /*!< if the thread is executing
                         in the InnoDB context count > 0. */

  bool abort; /*!< if this flag is set then
              this transaction must abort when
              it can */

  trx_id_t id; /*!< transaction id */

  trx_id_t no; /*!< transaction serialization number:
               max trx id shortly before the
               transaction is moved to
               COMMITTED_IN_MEMORY state.
               Protected by trx_sys_t::mutex
               when trx->in_rw_trx_list. Initially
               set to TRX_ID_MAX. */

  /** State of the trx from the point of view of concurrency control
  and the valid state transitions.

  Possible states:

  TRX_STATE_NOT_STARTED
  TRX_STATE_FORCED_ROLLBACK
  TRX_STATE_ACTIVE
  TRX_STATE_PREPARED
  TRX_STATE_COMMITTED_IN_MEMORY (alias below COMMITTED)

  Valid state transitions are:

  Regular transactions:
  * NOT_STARTED -> ACTIVE -> COMMITTED -> NOT_STARTED

  Auto-commit non-locking read-only:
  * NOT_STARTED -> ACTIVE -> NOT_STARTED

  XA (2PC):
  * NOT_STARTED -> ACTIVE -> PREPARED -> COMMITTED -> NOT_STARTED

  Recovered XA:
  * NOT_STARTED -> PREPARED -> COMMITTED -> (freed)

  XA (2PC) (shutdown or disconnect before ROLLBACK or COMMIT):
  * NOT_STARTED -> PREPARED -> (freed)

  Disconnected XA can become recovered:
  * ... -> ACTIVE -> PREPARED (connected) -> PREPARED (disconnected)
  Disconnected means from mysql e.g due to the mysql client disconnection.
  Latching and various transaction lists membership rules:

  XA (2PC) transactions are always treated as non-autocommit.

  Transitions to ACTIVE or NOT_STARTED occur when
  !in_rw_trx_list (no trx_sys->mutex needed).

  Autocommit non-locking read-only transactions move between states
  without holding any mutex. They are !in_rw_trx_list.

  All transactions, unless they are determined to be ac-nl-ro,
  explicitly tagged as read-only or read-write, will first be put
  on the read-only transaction list. Only when a !read-only transaction
  in the read-only list tries to acquire an X or IX lock on a table
  do we remove it from the read-only list and put it on the read-write
  list. During this switch we assign it a rollback segment.

  When a transaction is NOT_STARTED, it can be in_mysql_trx_list if
  it is a user transaction. It cannot be in rw_trx_list.

  ACTIVE->PREPARED->COMMITTED is only possible when trx->in_rw_trx_list.
  The transition ACTIVE->PREPARED is protected by trx_sys->mutex.

  ACTIVE->COMMITTED is possible when the transaction is in
  rw_trx_list.

  Transitions to COMMITTED are protected by both lock_sys->mutex
  and trx->mutex.

  NOTE: Some of these state change constraints are an overkill,
  currently only required for a consistent view for printing stats.
  This unnecessarily adds a huge cost for the general case. */

  trx_state_t state;

  /* If set, this transaction should stop inheriting (GAP)locks.
  Generally set to true during transaction prepare for RC or lower
  isolation, if requested. Needed for replication replay where
  we don't want to get blocked on GAP locks taken for protecting
  concurrent unique insert or replace operation. */
  bool skip_lock_inheritance;

  ReadView *read_view; /*!< consistent read view used in the
                       transaction, or NULL if not yet set */

  UT_LIST_NODE_T(trx_t)
  trx_list; /*!< list of transactions;
            protected by trx_sys->mutex. */
  UT_LIST_NODE_T(trx_t)
  no_list; /*!< Required during view creation
           to check for the view limit for
           transactions that are committing */

  trx_lock_t lock;   /*!< Information about the transaction
                     locks and state. Protected by
                     trx->mutex or lock_sys->mutex
                     or both */
  bool is_recovered; /*!< 0=normal transaction,
                     1=recovered, must be rolled back,
                     protected by trx_sys->mutex when
                     trx->in_rw_trx_list holds */

  hit_list_t hit_list; /*!< List of transactions to kill,
                       when a high priority transaction
                       is blocked on a lock wait. */

  os_thread_id_t killed_by; /*!< The thread ID that wants to
                            kill this transaction asynchronously.
                            This is required because we recursively
                            enter the handlerton methods and need
                            to distinguish between the kill thread
                            and the transaction thread.

                            Note: We need to be careful w.r.t the
                            Thread Pool. The thread doing the kill
                            should not leave InnoDB between the
                            mark and the actual async kill because
                            the running thread can change. */

  /* These fields are not protected by any mutex. */
  const char *op_info;   /*!< English text describing the
                         current operation, or an empty
                         string */
  ulint isolation_level; /*!< TRX_ISO_REPEATABLE_READ, ... */
  bool check_foreigns;   /*!< normally TRUE, but if the user
                         wants to suppress foreign key checks,
                         (in table imports, for example) we
                         set this FALSE */
  /*------------------------------*/
  /* MySQL has a transaction coordinator to coordinate two phase
  commit between multiple storage engines and the binary log. When
  an engine participates in a transaction, it's responsible for
  registering itself using the trans_register_ha() API. */
  bool is_registered; /* This flag is set to true after the
                      transaction has been registered with
                      the coordinator using the XA API, and
                      is set to false  after commit or
                      rollback. */
  /*------------------------------*/
  bool check_unique_secondary;
  /*!< normally TRUE, but if the user
  wants to speed up inserts by
  suppressing unique key checks
  for secondary indexes when we decide
  if we can use the insert buffer for
  them, we set this FALSE */
  bool flush_log_later;      /* In 2PC, we hold the
                             prepare_commit mutex across
                             both phases. In that case, we
                             defer flush of the logs to disk
                             until after we release the
                             mutex. */
  bool must_flush_log_later; /*!< this flag is set to TRUE in
                        trx_commit() if flush_log_later was
                        TRUE, and there were modifications by
                        the transaction; in that case we must
                        flush the log in
                        trx_commit_complete_for_mysql() */
  ulint duplicates;          /*!< TRX_DUP_IGNORE | TRX_DUP_REPLACE */
  bool has_search_latch;
  /*!< TRUE if this trx has latched the
  search system latch in S-mode.
  This now can only be true in
  row_search_mvcc, the btr search latch
  must has been released before exiting,
  and this flag would be set to false */
  trx_dict_op_t dict_operation; /**< @see enum trx_dict_op_t */

  bool ddl_operation;  /*!< True if this trx involves dd table
                        change */
  bool ddl_must_flush; /*!< True if this trx involves dd table
                       change, and must flush */
  bool in_truncate;    /* This trx is doing truncation */

  /* Fields protected by the srv_conc_mutex. */
  bool declared_to_be_inside_innodb;
  /*!< this is TRUE if we have declared
  this transaction in
  srv_conc_enter_innodb to be inside the
  InnoDB engine */
  ib_uint32_t n_tickets_to_enter_innodb;
  /*!< this can be > 0 only when
  declared_to_... is TRUE; when we come
  to srv_conc_innodb_enter, if the value
  here is > 0, we decrement this by 1 */
  ib_uint32_t dict_operation_lock_mode;
  /*!< 0, RW_S_LATCH, or RW_X_LATCH:
  the latch mode trx currently holds
  on dict_operation_lock. Protected
  by dict_operation_lock. */

  time_t start_time; /*!< time the state last time became
                     TRX_STATE_ACTIVE */

  /** Weight/Age of the transaction in the record lock wait queue. */
  int32_t age;

  /** For tracking if Weight/age has been updated. */
  uint64_t age_updated;

  lsn_t commit_lsn; /*!< lsn at the time of the commit */

  /*------------------------------*/
  THD *mysql_thd; /*!< MySQL thread handle corresponding
                  to this trx, or NULL */

  const char *mysql_log_file_name;
  /*!< if MySQL binlog is used, this field
  contains a pointer to the latest file
  name; this is NULL if binlog is not
  used */
  int64_t mysql_log_offset;
  /*!< if MySQL binlog is used, this
  field contains the end offset of the
  binlog entry */
  /*------------------------------*/
  ib_uint32_t n_mysql_tables_in_use; /*!< number of Innobase tables
                              used in the processing of the current
                              SQL statement in MySQL */
  ib_uint32_t mysql_n_tables_locked;
  /*!< how many tables the current SQL
  statement uses, except those
  in consistent read */
  /*------------------------------*/
#ifdef UNIV_DEBUG
  /** The following two fields are mutually exclusive. */
  /* @{ */

  bool in_rw_trx_list; /*!< true if in trx_sys->rw_trx_list */
                       /* @} */
#endif                 /* UNIV_DEBUG */
  UT_LIST_NODE_T(trx_t)
  mysql_trx_list; /*!< list of transactions created for
                  MySQL; protected by trx_sys->mutex */
#ifdef UNIV_DEBUG
  bool in_mysql_trx_list;
  /*!< true if in
  trx_sys->mysql_trx_list */
#endif /* UNIV_DEBUG */
  /*------------------------------*/
  dberr_t error_state;            /*!< 0 if no error, otherwise error
                                  number; NOTE That ONLY the thread
                                  doing the transaction is allowed to
                                  set this field: this is NOT protected
                                  by any mutex */
  const dict_index_t *error_info; /*!< if the error number indicates a
                                  duplicate key error, a pointer to
                                  the problematic index is stored here */
  ulint error_key_num;            /*!< if the index creation fails to a
                                  duplicate key error, a mysql key
                                  number of that index is stored here */
  sess_t *sess;                   /*!< session of the trx, NULL if none */
  que_t *graph;                   /*!< query currently run in the session,
                                  or NULL if none; NOTE that the query
                                  belongs to the session, and it can
                                  survive over a transaction commit, if
                                  it is a stored procedure with a COMMIT
                                  WORK statement, for instance */
  /*------------------------------*/
  UT_LIST_BASE_NODE_T(trx_named_savept_t)
  trx_savepoints; /*!< savepoints set with SAVEPOINT ...,
                  oldest first */
  /*------------------------------*/
  UndoMutex undo_mutex; /*!< mutex protecting the fields in this
                        section (down to undo_no_arr), EXCEPT
                        last_sql_stat_start, which can be
                        accessed only when we know that there
                        cannot be any activity in the undo
                        logs! */
  undo_no_t undo_no;    /*!< next undo log record number to
                        assign; since the undo log is
                        private for a transaction, this
                        is a simple ascending sequence
                        with no gaps; thus it represents
                        the number of modified/inserted
                        rows in a transaction */
  space_id_t undo_rseg_space;
  /*!< space id where last undo record
  was written */
  trx_savept_t last_sql_stat_start;
  /*!< undo_no when the last sql statement
  was started: in case of an error, trx
  is rolled back down to this undo
  number; see note at undo_mutex! */
  trx_rsegs_t rsegs;    /* rollback segments for undo logging */
  undo_no_t roll_limit; /*!< least undo number to undo during
                        a partial rollback; 0 otherwise */
#ifdef UNIV_DEBUG
  bool in_rollback;   /*!< true when the transaction is
                      executing a partial or full rollback */
#endif                /* UNIV_DEBUG */
  ulint pages_undone; /*!< number of undo log pages undone
                      since the last undo log truncation */
  /*------------------------------*/
  ulint n_autoinc_rows;       /*!< no. of AUTO-INC rows required for
                              an SQL statement. This is useful for
                              multi-row INSERTs */
  ib_vector_t *autoinc_locks; /* AUTOINC locks held by this
                              transaction. Note that these are
                              also in the lock list trx_locks. This
                              vector needs to be freed explicitly
                              when the trx instance is destroyed.
                              Protected by lock_sys->mutex. */
  /*------------------------------*/
  bool read_only;        /*!< true if transaction is flagged
                         as a READ-ONLY transaction.
                         if auto_commit && will_lock == 0
                         then it will be handled as a
                         AC-NL-RO-SELECT (Auto Commit Non-Locking
                         Read Only Select). A read only
                         transaction will not be assigned an
                         UNDO log. */
  bool auto_commit;      /*!< true if it is an autocommit */
  ib_uint32_t will_lock; /*!< Will acquire some locks. Increment
                         each time we determine that a lock will
                         be acquired by the MySQL layer. */
#ifndef UNIV_HOTBACKUP
  /*------------------------------*/
  fts_trx_t *fts_trx;       /*!< FTS information, or NULL if
                            transaction hasn't modified tables
                            with FTS indexes (yet). */
  doc_id_t fts_next_doc_id; /* The document id used for updates */
  /*------------------------------*/
  ib_uint32_t flush_tables; /*!< if "covering" the FLUSH TABLES",
                            count of tables being flushed. */

  /*------------------------------*/
  bool internal; /*!< true if it is a system/internal
                 transaction background task. Such
                 transactions are always treated as
                 read-write. */
                 /*------------------------------*/
#ifdef UNIV_DEBUG
  ulint start_line;       /*!< Track where it was started from */
  const char *start_file; /*!< Filename where it was started */
#endif                    /* UNIV_DEBUG */

  lint n_ref; /*!< Count of references, protected
              by trx_t::mutex. We can't release the
              locks nor commit the transaction until
              this reference is 0.  We can change
              the state to COMMITTED_IN_MEMORY to
              signify that it is no longer
              "active". */

  /** Version of this instance. It is incremented each time the
  instance is re-used in trx_start_low(). It is used to track
  whether a transaction has been restarted since it was tagged
  for asynchronous rollback. */
  ulint version;

  XID *xid;                    /*!< X/Open XA transaction
                               identification to identify a
                               transaction branch */
  trx_mod_tables_t mod_tables; /*!< List of tables that were modified
                               by this transaction */
#endif                         /* !UNIV_HOTBACKUP */
                               /*------------------------------*/
  bool api_trx;                /*!< trx started by InnoDB API */
  bool api_auto_commit;        /*!< automatic commit */
  bool read_write;             /*!< if read and write operation */

  /*------------------------------*/
  char *detailed_error;          /*!< detailed error message for last
                                 error, or empty. */
  FlushObserver *flush_observer; /*!< flush observer */

#ifdef UNIV_DEBUG
  bool is_dd_trx; /*!< True if the transaction is used for
                  doing Non-locking Read-only Read
                  Committed on DD tables */
#endif            /* UNIV_DEBUG */
  ulint magic_n;

  bool is_read_uncommitted() const {
    return (isolation_level == READ_UNCOMMITTED);
  }

  bool skip_gap_locks() const {
    switch (isolation_level) {
      case READ_UNCOMMITTED:
      case READ_COMMITTED:
        return (true);
      case REPEATABLE_READ:
      case SERIALIZABLE:
        return (false);
    }
    ut_ad(0);
    return (false);
  }

  bool allow_semi_consistent() const { return (skip_gap_locks()); }
};
#ifndef UNIV_HOTBACKUP

/* Transaction isolation levels (trx->isolation_level) */
#define TRX_ISO_READ_UNCOMMITTED trx_t::READ_UNCOMMITTED
#define TRX_ISO_READ_COMMITTED trx_t::READ_COMMITTED
#define TRX_ISO_REPEATABLE_READ trx_t::REPEATABLE_READ
#define TRX_ISO_SERIALIZABLE trx_t::SERIALIZABLE

/**
Check if transaction is started.
@param[in] trx		Transaction whose state we need to check
@return true if transaction is in state started */
inline bool trx_is_started(const trx_t *trx) {
  return (trx->state != TRX_STATE_NOT_STARTED &&
          trx->state != TRX_STATE_FORCED_ROLLBACK);
}

/* Treatment of duplicate values (trx->duplicates; for example, in inserts).
Multiple flags can be combined with bitwise OR. */
#define TRX_DUP_IGNORE 1  /* duplicate rows are to be updated */
#define TRX_DUP_REPLACE 2 /* duplicate rows are to be replaced */

/** Commit node states */
enum commit_node_state {
  COMMIT_NODE_SEND = 1, /*!< about to send a commit signal to
                        the transaction */
  COMMIT_NODE_WAIT      /*!< commit signal sent to the transaction,
                        waiting for completion */
};

/** Commit command node in a query graph */
struct commit_node_t {
  que_common_t common;          /*!< node type: QUE_NODE_COMMIT */
  enum commit_node_state state; /*!< node execution state */
};

/** Test if trx->mutex is owned. */
#define trx_mutex_own(t) mutex_own(&t->mutex)

/** Acquire the trx->mutex. */
#define trx_mutex_enter(t)  \
  do {                      \
    mutex_enter(&t->mutex); \
  } while (0)

/** Release the trx->mutex. */
#define trx_mutex_exit(t)  \
  do {                     \
    mutex_exit(&t->mutex); \
  } while (0)

/** Track if a transaction is executing inside InnoDB code. It acts
like a gate between the Server and InnoDB.  */
class TrxInInnoDB {
 public:
  /**
  @param[in,out] trx	Transaction entering InnoDB via the handler
  @param[in] disable	true if called from COMMIT/ROLLBACK method */
  TrxInInnoDB(trx_t *trx, bool disable = false) : m_trx(trx) {
    enter(trx, disable);
  }

  /**
  Destructor */
  ~TrxInInnoDB() { exit(m_trx); }

  /**
  @return true if the transaction has been marked for asynchronous
          rollback */
  bool is_aborted() const { return (is_aborted(m_trx)); }

  /**
  @return true if the transaction can't be rolled back asynchronously */
  bool is_rollback_disabled() const {
    return ((m_trx->in_innodb & TRX_FORCE_ROLLBACK_DISABLE) > 0);
  }

  /**
  @return true if the transaction has been marked for asynchronous
          rollback */
  static bool is_aborted(const trx_t *trx) {
    if (trx->state == TRX_STATE_NOT_STARTED) {
      return (false);
    }

    ut_ad(srv_read_only_mode || trx->in_depth > 0);
    ut_ad(srv_read_only_mode || trx->in_innodb > 0);

    return (trx->abort || trx->state == TRX_STATE_FORCED_ROLLBACK);
  }

  /**
  Start statement requested for transaction.
  @param[in, out] trx	Transaction at the start of a SQL statement */
  static void begin_stmt(trx_t *trx) { enter(trx, false); }

  /**
  Note an end statement for transaction
  @param[in, out] trx	Transaction at end of a SQL statement */
  static void end_stmt(trx_t *trx) { exit(trx); }

  /**
  @return true if the rollback is being initiated by the thread that
          marked the transaction for asynchronous rollback */
  static bool is_async_rollback(const trx_t *trx) {
    return (trx->killed_by == os_thread_get_curr_id());
  }

 private:
  /** Note that we have crossed into InnoDB code.
  @param[in]	trx	transaction
  @param[in]	disable	true if called from COMMIT/ROLLBACK method */
  static void enter(trx_t *trx, bool disable) {
    if (srv_read_only_mode) {
      return;
    }

    ut_ad(!is_async_rollback(trx));

    /* If it hasn't already been marked for async rollback.
    and it will be committed/rolled back. */
    if (disable) {
      trx_mutex_enter(trx);
      if (!is_forced_rollback(trx) && is_started(trx) &&
          !trx_is_autocommit_non_locking(trx)) {
        ut_ad(trx->killed_by == 0);

        /* This transaction has crossed the point of
        no return and cannot be rolled back
        asynchronously now. It must commit or rollback
        synhronously. */

        trx->in_innodb |= TRX_FORCE_ROLLBACK_DISABLE;
      }
      trx_mutex_exit(trx);
    }

    /* Avoid excessive mutex acquire/release */
    ++trx->in_depth;

    /* If trx->in_depth is greater than 1 then
    transaction is already in InnoDB. */
    if (trx->in_depth > 1) {
      return;
    }

    trx_mutex_enter(trx);

    wait(trx);

    ut_ad((trx->in_innodb & TRX_FORCE_ROLLBACK_MASK) == 0);

    ++trx->in_innodb;

    trx_mutex_exit(trx);
  }

  /**
  Note that we are exiting InnoDB code */
  static void exit(trx_t *trx) {
    if (srv_read_only_mode) {
      return;
    }

    /* Avoid excessive mutex acquire/release */

    ut_ad(trx->in_depth > 0);

    --trx->in_depth;

    if (trx->in_depth > 0) {
      return;
    }

    trx_mutex_enter(trx);

    ut_ad((trx->in_innodb & TRX_FORCE_ROLLBACK_MASK) > 0);

    --trx->in_innodb;

    trx_mutex_exit(trx);
  }

  /*
  @return true if it is a forced rollback, asynchronously */
  static bool is_forced_rollback(const trx_t *trx) {
    ut_ad(trx_mutex_own(trx));

    return ((trx->in_innodb & TRX_FORCE_ROLLBACK)) > 0;
  }

  /**
  Wait for the asynchronous rollback to complete, if it is in progress */
  static void wait(trx_t *trx) {
    ut_ad(trx_mutex_own(trx));

    ulint loop_count = 0;
    /* start with optimistic sleep time - 20 micro seconds. */
    ulint sleep_time = 20;

    while (is_forced_rollback(trx)) {
      /* Wait for the async rollback to complete */

      trx_mutex_exit(trx);

      loop_count++;
      /* If the wait is long, don't hog the cpu. */
      if (loop_count < 100) {
        /* 20 microseconds */
        sleep_time = 20;
      } else if (loop_count < 1000) {
        /* 1 millisecond */
        sleep_time = 1000;
      } else {
        /* 100 milliseconds */
        sleep_time = 100000;
      }

      os_thread_sleep(sleep_time);

      trx_mutex_enter(trx);
    }
  }

  /**
  @return true if transaction is started */
  static bool is_started(const trx_t *trx) {
    ut_ad(trx_mutex_own(trx));

    return (trx_is_started(trx));
  }

 private:
  /**
  Transaction instance crossing the handler boundary from the Server. */
  trx_t *m_trx;
};

#include "trx0trx.ic"
#endif /* !UNIV_HOTBACKUP */

#endif
