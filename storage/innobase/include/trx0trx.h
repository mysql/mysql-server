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

/** @file include/trx0trx.h
 The transaction

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#ifndef trx0trx_h
#define trx0trx_h

#include <atomic>
#include <set>

#include "ha_prototypes.h"

#include "dict0types.h"
#include "trx0types.h"
#include "ut0new.h"

#include "lock0types.h"
#include "mem0mem.h"
#include "que0types.h"
#include "trx0xa.h"
#include "usr0types.h"
#include "ut0vec.h"
#ifndef UNIV_HOTBACKUP
#include "fts0fts.h"
#endif /* !UNIV_HOTBACKUP */
#include "read0read.h"
#include "sql/handler.h"  // Xa_state_list
#include "srv0srv.h"

/* std::vector to store the trx id & table id of tables that needs to be
 * rollbacked. We take SHARED MDL on these tables inside
 * trx_recovery_rollback_thread before letting server accept connections */
extern std::vector<std::pair<trx_id_t, table_id_t>> to_rollback_trx_tables;

// Forward declaration
struct mtr_t;

// Forward declaration
class ReadView;

// Forward declaration
class Flush_observer;

/** Dummy session used currently in MySQL interface */
extern sess_t *trx_dummy_sess;

#ifndef UNIV_HOTBACKUP
/** Set flush observer for the transaction
@param[in,out]  trx             transaction struct
@param[in]      observer        flush observer */
void trx_set_flush_observer(trx_t *trx, Flush_observer *observer);

/** Set detailed error message for the transaction.
@param[in] trx Transaction struct
@param[in] msg Detailed error message */
void trx_set_detailed_error(trx_t *trx, const char *msg);

/** Set detailed error message for the transaction from a file. Note that the
 file is rewinded before reading from it. */
void trx_set_detailed_error_from_file(
    trx_t *trx,  /*!< in: transaction struct */
    FILE *file); /*!< in: file to read message from */
/** Retrieves the error_info field from a trx.
 @return the error index */
static inline const dict_index_t *trx_get_error_index(
    const trx_t *trx); /*!< in: trx object */
/** Creates a transaction object for MySQL.
 @return own: transaction object */
trx_t *trx_allocate_for_mysql(void);
/** Creates a transaction object for background operations by the master thread.
 @return own: transaction object */
trx_t *trx_allocate_for_background(void);

/** Resurrect table locks for resurrected transactions.
@param[in]      all     false: resurrect locks for dictionary transactions,
                        true : resurrect locks for all transactions. */
void trx_resurrect_locks(bool all);

/** Clear all resurrected table IDs. Needs to be called after all tables locks
are resurrected. */
void trx_clear_resurrected_table_ids();

/** Free and initialize a transaction object instantiated during recovery.
@param[in,out]  trx     transaction object to free and initialize */
void trx_free_resurrected(trx_t *trx);

/** Free a transaction that was allocated by background or user threads.
@param[in,out]  trx     transaction object to free */
void trx_free_for_background(trx_t *trx);

/** At shutdown, frees a transaction object that represents either:
 - a PREPARED transaction,
 - or a recovered ACTIVE transaction.
@param[in, out]  trx   transaction object to free */
void trx_free_prepared_or_active_recovered(trx_t *trx);

/** Free a transaction object for MySQL.
@param[in,out]  trx     transaction */
void trx_free_for_mysql(trx_t *trx);

/** Disconnect a transaction from MySQL.
@param[in,out]  trx     transaction */
void trx_disconnect_plain(trx_t *trx);

/** Disconnect a prepared transaction from MySQL.
@param[in,out]  trx     transaction */
void trx_disconnect_prepared(trx_t *trx);

/** Creates trx objects for transactions and initializes the trx list of
 trx_sys at database start. Rollback segment and undo log lists must
 already exist when this function is called, because the lists of
 transactions to be rolled back or cleaned up are built based on the
 undo log lists. */
void trx_lists_init_at_db_start(void);

/** Starts the transaction if it is not yet started.
@param[in,out] trx Transaction
@param[in] read_write True if read write transaction */
void trx_start_if_not_started_xa_low(trx_t *trx, bool read_write);

/** Starts the transaction if it is not yet started.
@param[in] trx Transaction
@param[in] read_write True if read write transaction */
void trx_start_if_not_started_low(trx_t *trx, bool read_write);

/** Starts a transaction for internal processing. */
void trx_start_internal_low(trx_t *trx); /*!< in/out: transaction */

/** Starts a read-only transaction for internal processing.
@param[in,out] trx      transaction to be started */
void trx_start_internal_read_only_low(trx_t *trx);

/** Commits a transaction. */
void trx_commit(trx_t *trx); /*!< in/out: transaction */

/** Commits a transaction and a mini-transaction.
@param[in,out] trx Transaction
@param[in,out] mtr Mini-transaction (will be committed), or null if trx made no
modifications */
void trx_commit_low(trx_t *trx, mtr_t *mtr);

/** Cleans up a transaction at database startup. The cleanup is needed if
 the transaction already got to the middle of a commit when the database
 crashed, and we cannot roll it back. */
void trx_cleanup_at_db_startup(trx_t *trx); /*!< in: transaction */
/** Does the transaction commit for MySQL.
 @return DB_SUCCESS or error number */
dberr_t trx_commit_for_mysql(trx_t *trx); /*!< in/out: transaction */

/**
Does the transaction prepare for MySQL.
@param[in, out] trx             Transaction instance to prepare */

dberr_t trx_prepare_for_mysql(trx_t *trx);

/** This function is used to find number of prepared transactions and
 their transaction objects for a recovery.
 @return number of prepared transactions */
int trx_recover_for_mysql(
    XA_recover_txn *txn_list, /*!< in/out: prepared transactions */
    ulint len,                /*!< in: number of slots in xid_list */
    MEM_ROOT *mem_root);      /*!< in: memory for table names */

/** Find prepared transactions that are marked as prepared in TC, for recovery
purposes.
@param[in,out] xa_list prepared transactions state
@return 0 if successful or error number */
int trx_recover_tc_for_mysql(Xa_state_list &xa_list);

/** This function is used to find one X/Open XA distributed transaction
 which is in the prepared state
 @param[in]   xid   X/Open XA transaction identifier
 @return trx or NULL; on match, the trx->xid will be invalidated;
 note that the trx may have been committed */
trx_t *trx_get_trx_by_xid(const XID *xid);

/** If required, flushes the log to disk if we called trx_commit_for_mysql()
 with trx->flush_log_later == true. */
void trx_commit_complete_for_mysql(trx_t *trx); /*!< in/out: transaction */
/** Marks the latest SQL statement ended. */
void trx_mark_sql_stat_end(trx_t *trx); /*!< in: trx handle */
/** Assigns a read view for a consistent read query. All the consistent reads
 within the same transaction will get the same read view, which is created
 when this function is first called for a new started transaction. */
ReadView *trx_assign_read_view(trx_t *trx); /*!< in: active transaction */

/** @return the transaction's read view or NULL if one not assigned. */
static inline ReadView *trx_get_read_view(trx_t *trx);

/** @return the transaction's read view or NULL if one not assigned. */
static inline const ReadView *trx_get_read_view(const trx_t *trx);

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
The caller must hold lock_sys exclusive global latch and trx_sys->mutex.
@param[in]  f               output stream
@param[in]  trx             transaction
@param[in]  max_query_len   max query length to print, or 0 to use the default
                            max length */
void trx_print_latched(FILE *f, const trx_t *trx, ulint max_query_len);

/** Prints info about a transaction.
Acquires and releases lock_sys exclusive global latch and trx_sys->mutex.
@param[in]  f               output stream
@param[in]  trx             transaction
@param[in]  max_query_len   max query length to print, or 0 to use the default
                            max length */
void trx_print(FILE *f, const trx_t *trx, ulint max_query_len);

/** Determine if a transaction is a dictionary operation.
 @return dictionary operation mode */
[[nodiscard]] static inline enum trx_dict_op_t trx_get_dict_operation(
    const trx_t *trx); /*!< in: transaction */

/** Flag a transaction a dictionary operation.
@param[in,out]  trx     transaction
@param[in]      op      operation, not TRX_DICT_OP_NONE */
static inline void trx_set_dict_operation(trx_t *trx, enum trx_dict_op_t op);

/** Determines if a transaction is in the given state.
The caller must hold trx_sys->mutex, or it must be the thread
that is serving a running transaction.
A running RW transaction must be in trx_sys->rw_trx_list.
@param[in] trx   Transaction.
@param[in] state State.
@return true if trx->state == state */
[[nodiscard]] static inline bool trx_state_eq(const trx_t *trx,
                                              trx_state_t state);
#ifdef UNIV_DEBUG
/** Determines if trx can be handled by current thread, which is when
trx->mysql_thd is nullptr (a "background" trx) or equals current_thd.
@param[in]    trx   The transaction to check
@return true iff current thread can handle the transaction
*/
bool trx_can_be_handled_by_current_thread(const trx_t *trx);

/** Determines if trx can be handled by current thread, which is when
trx->mysql_thd is nullptr (a "background" trx) or equals current_thd,
or is a victim being killed by HP transaction run by the current thread.
@param[in]    trx   The transaction to check
@return true iff current thread can handle the transaction
*/
bool trx_can_be_handled_by_current_thread_or_is_hp_victim(const trx_t *trx);

/** Asserts that a transaction has been started.
 The caller must hold trx_sys->mutex.
 @return true if started */
[[nodiscard]] bool trx_assert_started(const trx_t *trx); /*!< in: transaction */
#endif                                                   /* UNIV_DEBUG */

/** Determines if the currently running transaction has been interrupted.
 @return true if interrupted */
bool trx_is_interrupted(const trx_t *trx); /*!< in: transaction */
/** Determines if the currently running transaction is in strict mode.
 @return true if strict */
bool trx_is_strict(trx_t *trx); /*!< in: transaction */

/** Compares the "weight" (or size) of two transactions. Transactions that
 have edited non-transactional tables are considered heavier than ones
 that have not.
 @return true if weight(a) >= weight(b) */
bool trx_weight_ge(const trx_t *a,  /*!< in: the transaction to be compared */
                   const trx_t *b); /*!< in: the transaction to be compared */
/* Maximum length of a string that can be returned by
trx_get_que_state_str(). */
constexpr uint32_t TRX_QUE_STATE_STR_MAX_LEN = 12; /* "ROLLING BACK" */

/** Retrieves transaction's que state in a human readable string. The string
 should not be free()'d or modified.
 @return string in the data segment */
static inline const char *trx_get_que_state_str(
    const trx_t *trx); /*!< in: transaction */

/** Retreieves the transaction ID.
In a given point in time it is guaranteed that IDs of the running
transactions are unique. The values returned by this function for readonly
transactions may be reused, so a subsequent RO transaction may get the same ID
as a RO transaction that existed in the past. The values returned by this
function should be used for printing purposes only.
@param[in]      trx     transaction whose id to retrieve
@return transaction id */
static inline trx_id_t trx_get_id_for_print(const trx_t *trx);

/** Assign a temp-tablespace bound rollback-segment to a transaction.
@param[in,out]  trx     transaction that involves write to temp-table. */
void trx_assign_rseg_temp(trx_t *trx);

/** Create the trx_t pool */
void trx_pool_init();

/** Destroy the trx_t pool */
void trx_pool_close();

/**
Set the transaction as a read-write transaction if it is not already
tagged as such.
@param[in,out] trx      Transaction that needs to be "upgraded" to RW from RO */
void trx_set_rw_mode(trx_t *trx);

/**
@param[in] requestor    Transaction requesting the lock
@param[in] holder       Transaction holding the lock
@return the transaction that will be rolled back, null don't care */

static inline const trx_t *trx_arbitrate(const trx_t *requestor,
                                         const trx_t *holder);

/**
@param[in] trx          Transaction to check
@return true if the transaction is a high priority transaction.*/
static inline bool trx_is_high_priority(const trx_t *trx);

/**
If this is a high priority transaction,
kill all transactions that are blocking this transaction from acquiring locks.
@param[in,out] trx      High priority transaction */
void trx_kill_blocking(trx_t *trx);

/** Provides an id of the transaction which does not change over time.
Contrast this with trx->id and trx_get_id_for_print(trx) which change value once
a transaction can no longer be treated as read-only and becomes read-write.
@param[in]  trx   The transaction for which you want an immutable id
@return the transaction's immutable id */
static inline uint64_t trx_immutable_id(const trx_t *trx) {
  return (uint64_t{reinterpret_cast<uintptr_t>(trx)});
}

/**
Check if redo/noredo rseg is modified for insert/update.
@param[in] trx          Transaction to check */
static inline bool trx_is_rseg_updated(const trx_t *trx);
#endif /* !UNIV_HOTBACKUP */

typedef std::vector<ib_lock_t *, ut::allocator<ib_lock_t *>> lock_pool_t;

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
 changes within the locking code must latch the shard with the wait_lock and
 the trx->mutex when changing trx->lock.que_state to TRX_QUE_LOCK_WAIT or
 trx->lock.wait_lock to non-NULL but when the lock wait ends it is sufficient
 to only acquire the trx->mutex.
 To query the state either of the mutexes is sufficient within the locking
 code and no mutex is required when the query thread is no longer waiting. */

/** The locks and state of an active transaction.
Protected by exclusive lock_sys latch or trx->mutex combined with shared
lock_sys latch (unless stated otherwise for particular field). */
struct trx_lock_t {
  /** Default constructor. */
  trx_lock_t() = default;

  ulint n_active_thrs; /*!< number of active query threads */

  trx_que_t que_state; /*!< valid when trx->state
                       == TRX_STATE_ACTIVE: TRX_QUE_RUNNING,
                       TRX_QUE_LOCK_WAIT, ... */

  /** Incremented each time a lock is added or removed from the
  trx->lock.trx_locks, so that the thread which iterates over the list can spot
  a change if it occurred while it was reacquiring latches.
  Protected by trx->mutex. */
  uint64_t trx_locks_version;

  /** If this transaction is waiting for a lock, then blocking_trx points to a
  transaction which holds a conflicting lock.
  It is possible that the transaction has trx->lock.wait_lock == nullptr, yet it
  has non-null value of trx->lock.blocking_trx. For example this can happen when
  we are in the process of moving locks from one heap_no to another. This
  however is always done while the lock_sys shards which contain the queues
  involved are latched and conceptually it is true that the blocking_trx is
  the one for which the transaction waits, even though temporarily there is no
  pointer to a particular WAITING lock object.

  This field is changed from null to non-null, when holding this->mutex and
  mutex for lock_sys shard containing the new value of trx->lock.wait_lock.
  The field is changed from non-null to different non-null value, while holding
  mutex for lock_sys shard containing the trx->lock.wait_lock.
  The field is changed from non-null to null, while holding this->mutex,
  mutex for lock_sys shard containing the old value of trx->lock.wait_lock,
  before it was changed to null.

  Readers might read it without any latch, but then they should validate the
  value, i.e. test if it is not-null, and points to a valid trx.
  To make any definite judgments one needs to latch the lock_sys shard
  containing the trx->lock.wait_lock. */
  std::atomic<trx_t *> blocking_trx;

  /** The lock request of this transaction is waiting for.
  It might be NULL if the transaction is not currently waiting, or if the lock
  was temporarily removed during B-tree reorganization and will be recreated in
  a different queue. Such move is protected by latching the shards containing
  both queues, so the intermediate state should not be observed by readers who
  latch the old shard.

  Changes from NULL to non-NULL while holding trx->mutex and latching the shard
  containing the new wait_lock value.
  Changes from non-NULL to NULL while latching the shard containing the old
  wait_lock value.
  Never changes from non-NULL to other non-NULL directly.

  Readers should hold exclusive global latch on lock_sys, as in general they
  can't know what shard the lock belongs to before reading it.
  However, in debug assertions, where we strongly believe to know the value of
  this field in advance, we can:
  - read without any latch if we believe the value should be NULL
  - read latching only the shard containing the wait_lock we expect */
  std::atomic<lock_t *> wait_lock;

  /** Stores the type of the most recent lock for which this trx had to wait.
  Set to lock_get_type_low(wait_lock) together with wait_lock in
  lock_set_lock_and_trx_wait().
  This field is not cleared when wait_lock is set to NULL during
  lock_reset_lock_and_trx_wait() as in lock_wait_suspend_thread() we are
  interested in reporting the last known value of this field via
  thd_wait_begin(). When a thread has to wait for a lock, it first releases
  lock-sys latch, and then calls lock_wait_suspend_thread() where among other
  things it tries to report statistic via thd_wait_begin() about the kind of
  lock (THD_WAIT_ROW_LOCK vs THD_WAIT_TABLE_LOCK) that caused the wait. But
  there is a possibility that before it gets to call thd_wait_begin() some other
  thread could latch lock-sys and grant the lock and call
  lock_reset_lock_and_trx_wait(). In other words: in case another thread was
  quick enough to grant the lock, we still would like to report the reason for
  attempting to sleep.
  Another common scenario of "setting trx->lock.wait_lock to NULL" is page
  reorganization: when we have to move records between pages, we also move
  locks, and when doing so, we temporarily remove the old waiting lock, and then
  add another one. For example look at lock_rec_move_low(). It first calls
  lock_reset_lock_and_trx_wait() which changes trx->lock.wait_lock to NULL, but
  then calls lock_rec_add_to_queue() -> RecLock::create() -> RecLock::lock_add()
  -> lock_set_lock_and_trx_wait() to set it again to the new lock. This all
  happens while holding lock-sys latch, but we read wait_lock_type without this
  latch, so we should not clear the wait_lock_type simply because somebody
  changed wait_lock to NULL.
  Protected by trx->mutex. */
  uint32_t wait_lock_type;

  bool was_chosen_as_deadlock_victim;
  /*!< when the transaction decides to
  wait for a lock, it sets this to false;
  if another transaction chooses this
  transaction as a victim in deadlock
  resolution, it sets this to true.
  Protected by trx->mutex. */

  /** Lock wait started at this time.
  Writes under shared lock_sys latch combined with trx->mutex.
  Reads require either trx->mutex or exclusive lock_sys latch. */
  std::chrono::system_clock::time_point wait_started;

  /** query thread belonging to this trx that is in QUE_THR_LOCK_WAIT state.
  For threads suspended in a lock wait, this is protected by lock_sys latch for
  the wait_lock's shard.
  Otherwise, this may only be modified by the thread that is serving the running
  transaction.
  */
  que_thr_t *wait_thr;

  /** Pre-allocated record locks. Protected by trx->mutex. */
  lock_pool_t rec_pool;

  /** Pre-allocated table locks. Protected by trx->mutex. */
  lock_pool_t table_pool;

  /** Next free record lock in pool. Protected by trx->mutex. */
  ulint rec_cached;

  /** Next free table lock in pool. Protected by trx->mutex. */
  ulint table_cached;

  /** Memory heap for trx_locks. Protected by trx->mutex */
  mem_heap_t *lock_heap;

  /** Locks requested by the transaction.
  It is sorted so that LOCK_TABLE locks are before LOCK_REC locks.
  Modifications are protected by trx->mutex and shard of lock_sys mutex.
  Reads can be performed while holding trx->mutex or exclusive lock_sys latch.
  One can also check if this list is empty or not from the thread running this
  transaction without holding any latches, keeping in mind that other threads
  might modify the list in parallel (for example during implicit-to-explicit
  conversion, or when B-tree split or merge causes locks to be moved from one
  page to another) - we rely on assumption that such operations do not change
  the "emptiness" of the list and that one can check for emptiness in a safe
  manner (in current implementation length of the list is stored explicitly so
  one can read it without risking unsafe pointer operations) */
  trx_lock_list_t trx_locks;

  /** AUTOINC locks held by this transaction.
  Note that these are also in the trx_locks list.
  This vector needs to be freed explicitly when the trx instance is destroyed.
  Protected by trx->mutex. */
  ib_vector_t *autoinc_locks;

  /** Number of rec locks in this trx.
  It is modified with shared lock_sys latch.
  It is read with exclusive lock_sys latch. */
  std::atomic<ulint> n_rec_locks;

  /** Used to indicate that every lock of this transaction placed on a record
  which is being purged should be inherited to the gap.
  Readers should hold a latch on the lock they'd like to learn about whether or
  not it should be inherited.
  Writers who want to set it to true, should hold a latch on the lock-sys queue
  they intend to add a lock to.
  Writers may set it to false at any time. */
  std::atomic<bool> inherit_all;

  /** Weight of the waiting transaction used for scheduling.
  The higher the weight the more we are willing to grant a lock to this
  transaction.
  Values are updated and read without any synchronization beyond that provided
  by atomics, as slightly stale values do not hurt correctness, just the
  performance. */
  std::atomic<trx_schedule_weight_t> schedule_weight;

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
                 ut::allocator<dict_table_t *>>
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
Therefore we do not have std::this_thread::get_id() assertions in the code.

Normally, only the thread that is currently associated with a running
transaction may access (read and modify) the trx object, and it may do
so without holding any mutex. The following are exceptions to this:

* trx_rollback_resurrected() may access resurrected (connectionless)
transactions while the system is already processing new user
transactions. The trx_sys->mutex prevents a race condition between it
and lock_trx_release_locks() [invoked by trx_commit()].

* Print of transactions may access transactions not associated with
the current thread. The caller must be holding trx_sys->mutex and
exclusive global lock_sys latch.

* When a transaction handle is in the trx_sys->mysql_trx_list or
trx_sys->trx_list, some of its fields must not be modified without
holding trx_sys->mutex exclusively.

* The locking code (in particular, deadlock checking and implicit to
explicit conversion) will access transactions associated to other
connections. The locks of transactions are protected by lock_sys latches
and sometimes by trx->mutex.

* Killing of asynchronous transactions. */

/** Represents an instance of rollback segment along with its state variables.*/
struct trx_undo_ptr_t {
  /** @return true iff no undo segment is allocated yet. */
  bool is_empty() { return (insert_undo == nullptr && update_undo == nullptr); }

  /** @return true iff only insert undo segment is allocated. */
  bool is_insert_only() {
    return (insert_undo != nullptr && update_undo == nullptr);
  }

  /** @return true iff update undo segment is allocated. */
  bool is_update() { return update_undo != nullptr; }

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

  /** Default constructor */
  trx_t() = default;

  /** Mutex protecting the fields `state` and `lock` (except some fields of
  `lock`,  which are protected by lock_sys latches) */
  mutable TrxMutex mutex;

  /* Note: in_depth was split from in_innodb for fixing a RO
  performance issue. Acquiring the trx_t::mutex for each row
  costs ~3% in performance. It is not required for correctness.
  Therefore we increment/decrement in_depth without holding any
  mutex. The assumption is that the Server will only ever call
  the handler from one thread. This is not true for kill_connection.
  Therefore in innobase_kill_connection. We don't increment this
  counter via TrxInInnoDB. */

  uint32_t in_depth; /*!< Track nested TrxInInnoDB
                        count */

  uint32_t in_innodb; /*!< if the thread is executing
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

  Transitions to COMMITTED are protected by trx->mutex.

  NOTE: Some of these state change constraints are an overkill,
  currently only required for a consistent view for printing stats.
  This unnecessarily adds a huge cost for the general case. */

  std::atomic<trx_state_t> state;

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

  /** Information about the transaction locks and state.
  Protected by trx->mutex or lock_sys latches or both */
  trx_lock_t lock;

  /**
  false:  a normal transaction
  true:   a recovered transaction

  Set to true when srv_is_being_started for recovered transactions.
  Set to false without any protection in trx_init (where no other thread should
  access this object anyway).
  Can be read safely when holding trx_sys->mutex and trx belongs to rw_trx_list,
  as trx_init can not be called until trx leaves rw_trx_list which requires the
  trx_sys->mutex.
  */
  bool is_recovered;

  std::atomic<std::thread::id> killed_by; /*!< The thread ID that wants to
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
  const char *op_info; /*!< English text describing the
                       current operation, or an empty
                       string */

  /** Current isolation level */
  isolation_level_t isolation_level;

  bool check_foreigns; /*!< normally true, but if the user
                       wants to suppress foreign key checks,
                       (in table imports, for example) we
                       set this false */
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
  /*!< normally true, but if the user
  wants to speed up inserts by
  suppressing unique key checks
  for secondary indexes when we decide
  if we can use the insert buffer for
  them, we set this false */
  bool flush_log_later;      /* In 2PC, we hold the
                             prepare_commit mutex across
                             both phases. In that case, we
                             defer flush of the logs to disk
                             until after we release the
                             mutex. */
  bool must_flush_log_later; /*!< this flag is set to true in
                        trx_commit() if flush_log_later was
                        true, and there were modifications by
                        the transaction; in that case we must
                        flush the log in
                        trx_commit_complete_for_mysql() */
  bool has_search_latch;
  /*!< true if this trx has latched the
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
  /*!< this is true if we have declared
  this transaction in
  srv_conc_enter_innodb to be inside the
  InnoDB engine */
  uint32_t n_tickets_to_enter_innodb;
  /*!< this can be > 0 only when
  declared_to_... is true; when we come
  to srv_conc_innodb_enter, if the value
  here is > 0, we decrement this by 1 */
  uint32_t dict_operation_lock_mode;
  /*!< 0, RW_S_LATCH, or RW_X_LATCH:
  the latch mode trx currently holds
  on dict_operation_lock. Protected
  by dict_operation_lock. */

  /** Time the state last time became TRX_STATE_ACTIVE. */
  std::atomic<std::chrono::system_clock::time_point> start_time{
      std::chrono::system_clock::time_point{}};
  static_assert(decltype(start_time)::is_always_lock_free);

  lsn_t commit_lsn; /*!< lsn at the time of the commit */

  /*------------------------------*/
  THD *mysql_thd; /*!< MySQL thread handle corresponding
                  to this trx, or NULL */

  const char *mysql_log_file_name;
  /*!< if MySQL binlog is used, this field
  contains a pointer to the latest file
  name; this is NULL if binlog is not
  used */
  uint64_t mysql_log_offset;
  /*!< if MySQL binlog is used, this
  field contains the end offset of the
  binlog entry */
  /*------------------------------*/
  uint32_t n_mysql_tables_in_use; /*!< number of Innobase tables
                              used in the processing of the current
                              SQL statement in MySQL */
  uint32_t mysql_n_tables_locked;
  /*!< how many tables the current SQL
  statement uses, except those
  in consistent read */
  /*------------------------------*/
#ifdef UNIV_DEBUG
  /** True iff in trx_sys->rw_trx_list */
  bool in_rw_trx_list;

#endif /* UNIV_DEBUG */
  UT_LIST_NODE_T(trx_t)
  mysql_trx_list; /*!< list of transactions created for
                  MySQL; protected by trx_sys->mutex */
#ifdef UNIV_DEBUG
  bool in_mysql_trx_list;
  /*!< true if in
  trx_sys->mysql_trx_list */
#endif /* UNIV_DEBUG */
  /*------------------------------*/

  /** DB_SUCCESS if no error, otherwise error number.
  Accessed without any mutex only by the thread doing the transaction or, if it
  is suspended (waiting for a lock), by the thread holding this->mutex which
  has changed trx->lock.wait_lock to nullptr and will wake up the transaction.*/
  dberr_t error_state;

  const dict_index_t *error_index; /*!< if the error number indicates a
                                   duplicate key error, a pointer to
                                   the problematic index is stored here */
  ulint error_key_num;             /*!< if the index creation fails to a
                                   duplicate key error, a mysql key
                                   number of that index is stored here */
  sess_t *sess;                    /*!< session of the trx, NULL if none */
  que_t *graph;                    /*!< query currently run in the session,
                                   or NULL if none; NOTE that the query
                                   belongs to the session, and it can
                                   survive over a transaction commit, if
                                   it is a stored procedure with a COMMIT
                                   WORK statement, for instance */
  /*------------------------------*/
  UT_LIST_BASE_NODE_T_EXTERN(trx_named_savept_t, trx_savepoints)
  trx_savepoints{}; /*!< savepoints set with SAVEPOINT ..., oldest first */
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
  ulint n_autoinc_rows; /*!< no. of AUTO-INC rows required for
                        an SQL statement. This is useful for
                        multi-row INSERTs */
  /*------------------------------*/
  bool read_only;     /*!< true if transaction is flagged
                      as a READ-ONLY transaction.
                      if auto_commit && will_lock == 0
                      then it will be handled as a
                      AC-NL-RO-SELECT (Auto Commit Non-Locking
                      Read Only Select). A read only
                      transaction will not be assigned an
                      UNDO log. */
  bool auto_commit;   /*!< true if it is an autocommit */
  uint32_t will_lock; /*!< Will acquire some locks. Increment
                         each time we determine that a lock will
                         be acquired by the MySQL layer. */
#ifndef UNIV_HOTBACKUP
  /*------------------------------*/
  fts_trx_t *fts_trx;       /*!< FTS information, or NULL if
                            transaction hasn't modified tables
                            with FTS indexes (yet). */
  doc_id_t fts_next_doc_id; /* The document id used for updates */
  /*------------------------------*/
  uint32_t flush_tables; /*!< if "covering" the FLUSH TABLES",
                            count of tables being flushed. */

  /*------------------------------*/
  bool internal; /*!< true if it is a system/internal
                 transaction background task. Such
                 transactions are always treated as
                 read-write. */
                 /*------------------------------*/
  /** Transaction persists GTID. */
  bool persists_gtid;

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
  std::atomic_uint64_t version;

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

  /** This flag is set for trx_t objects used by the purge sys. We use the flag
  when validating mysql_trx_list in trx_sys_before_pre_dd_shutdown_validate.
  Purge threads can have allocated trx_t objects visible in the mysql_trx_list
  at this point during shutdown, this is acceptable so we need a way to signal
  this fact. */
  bool purge_sys_trx;
  /*------------------------------*/
  char *detailed_error;           /*!< detailed error message for last
                                  error, or empty. */
  Flush_observer *flush_observer; /*!< flush observer */

#ifdef UNIV_DEBUG
  bool is_dd_trx; /*!< True if the transaction is used for
                  doing Non-locking Read-only Read
                  Committed on DD tables */
#endif            /* UNIV_DEBUG */
  ulint magic_n;

  bool is_read_uncommitted() const {
    return (isolation_level == READ_UNCOMMITTED);
  }

  bool releases_gap_locks_at_prepare() const {
    return isolation_level <= READ_COMMITTED;
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
    ut_d(ut_error);
    ut_o(return (false));
  }

  bool allow_semi_consistent() const { return (skip_gap_locks()); }
  /** Checks if this transaction releases locks on non matching records due to
  low isolation level.
  @return true iff in this transaction's isolation level locks on records which
               do not match the WHERE clause are released */
  bool releases_non_matching_rows() const { return skip_gap_locks(); }
};

#ifndef UNIV_HOTBACKUP
/**
Transactions that aren't started by the MySQL server don't set
the trx_t::mysql_thd field. For such transactions we set the lock
wait timeout to 0 instead of the user configured value that comes
from innodb_lock_wait_timeout via trx_t::mysql_thd.
@param  t transaction
@return lock wait timeout in seconds */
static inline std::chrono::seconds trx_lock_wait_timeout_get(const trx_t *t) {
  return thd_lock_wait_timeout(t->mysql_thd);
}

/**
Determine if the transaction is a non-locking autocommit select
(implied read-only).
@param t transaction
@return true if non-locking autocommit select transaction. */
static inline bool trx_is_autocommit_non_locking(const trx_t *t) {
  return t->auto_commit && t->will_lock == 0;
}

/** Check transaction state */
static inline void check_trx_state(const trx_t *t) {
  ut_ad(!trx_is_autocommit_non_locking(t));
  switch (t->state) {
    case TRX_STATE_PREPARED:
      /* fall through */
    case TRX_STATE_ACTIVE:
    case TRX_STATE_COMMITTED_IN_MEMORY:
      return;
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:
      break;
  }
  ut_error;
}

/**
Assert that the transaction is in the trx_sys_t::rw_trx_list */
static inline void assert_trx_in_rw_list(const trx_t *t) {
  ut_ad(!t->read_only);
  ut_ad(t->in_rw_trx_list == !(t->read_only || !t->rsegs.m_redo.rseg));
  check_trx_state(t);
}

/** Check if transaction is free so that it can be re-initialized.
@param t transaction handle */
static inline void assert_trx_is_free(const trx_t *t) {
  ut_ad(trx_state_eq(t, TRX_STATE_NOT_STARTED) ||
        trx_state_eq(t, TRX_STATE_FORCED_ROLLBACK));
  ut_ad(!trx_is_rseg_updated(t));
  ut_ad(!MVCC::is_view_active(t->read_view));
  ut_ad((t)->lock.wait_thr == nullptr);
  ut_ad(UT_LIST_GET_LEN((t)->lock.trx_locks) == 0);
  ut_ad((t)->dict_operation == TRX_DICT_OP_NONE);
}

/** Check if transaction is in-active so that it can be freed and put back to
transaction pool.
@param t transaction handle */
static inline void assert_trx_is_inactive(const trx_t *t) {
  assert_trx_is_free(t);
  ut_ad(t->dict_operation_lock_mode == 0);
}

#ifdef UNIV_DEBUG
/** Assert that an autocommit non-locking select cannot be in the
 rw_trx_list and that it is a read-only transaction.
 The transaction must be in the mysql_trx_list. */
static inline void assert_trx_nonlocking_or_in_list(const trx_t *t) {
  if (trx_is_autocommit_non_locking(t)) {
    trx_state_t t_state = t->state;
    ut_ad(t->read_only);
    ut_ad(!t->is_recovered);
    ut_ad(!t->in_rw_trx_list);
    ut_ad(t->in_mysql_trx_list);
    ut_ad(t_state == TRX_STATE_NOT_STARTED ||
          t_state == TRX_STATE_FORCED_ROLLBACK || t_state == TRX_STATE_ACTIVE);
  } else {
    check_trx_state(t);
  }
}
#else /* UNIV_DEBUG */
/** Assert that an autocommit non-locking select cannot be in the
 rw_trx_list and that it is a read-only transaction.
 The transaction must be in the mysql_trx_list. */
#define assert_trx_nonlocking_or_in_list(trx) ((void)0)
#endif /* UNIV_DEBUG */

/**
Determine if the transaction is a non-locking autocommit select
with an explicit check for the read-only status.
@param t transaction
@return true if non-locking autocommit read-only transaction. */
static inline bool trx_is_ac_nl_ro(const trx_t *t) {
  return t->read_only && trx_is_autocommit_non_locking(t);
}

/**
Increase the reference count. If the transaction is in state
TRX_STATE_COMMITTED_IN_MEMORY then the transaction is considered
committed and the reference count is not incremented.
@param trx Transaction that is being referenced */
static inline void trx_reference(trx_t *trx);

/**
Release the transaction. Decrease the reference count.
@param trx Transaction that is being released */
static inline void trx_release_reference(trx_t *trx);

/**
Check if the transaction is being referenced. */
static inline bool trx_is_referenced(const trx_t *t) { return t->n_ref > 0; }

/** Calculates the "weight" of a transaction. The weight of one transaction
 is estimated as the number of altered rows + the number of locked rows.
 @param t transaction
 @return transaction weight */
static inline uint64_t TRX_WEIGHT(const trx_t *t) {
  return t->undo_no + UT_LIST_GET_LEN(t->lock.trx_locks);
}

#ifdef UNIV_DEBUG
static inline void trx_start_if_not_started_xa(trx_t *t, bool rw,
                                               ut::Location loc) {
  t->start_line = loc.line;
  t->start_file = loc.filename;
  trx_start_if_not_started_xa_low(t, rw);
}

static inline void trx_start_if_not_started(trx_t *t, bool rw, ut::Location l) {
  t->start_line = l.line;
  t->start_file = l.filename;
  trx_start_if_not_started_low(t, rw);
}

static inline void trx_start_internal(trx_t *t, ut::Location loc) {
  t->start_line = loc.line;
  t->start_file = loc.filename;
  trx_start_internal_low(t);
}

static inline void trx_start_internal_read_only(trx_t *t, ut::Location loc) {
  t->start_line = loc.line;
  t->start_file = loc.filename;
  trx_start_internal_read_only_low(t);
}
#else
static inline void trx_start_if_not_started_xa(trx_t *t, bool rw,
                                               ut::Location loc) {
  trx_start_if_not_started_low(t, rw);
}

static inline void trx_start_internal(trx_t *t, ut::Location loc) {
  trx_start_internal_low(t);
}

static inline void trx_start_internal_read_only(trx_t *t, ut::Location loc) {
  trx_start_internal_read_only_low(t);
}

static inline void trx_start_if_not_started(trx_t *t, bool rw, ut::Location l) {
  trx_start_if_not_started_xa_low(t, rw);
}
#endif /* UNIV_DEBUG */

/* Transaction isolation levels (trx->isolation_level) */
#define TRX_ISO_READ_UNCOMMITTED trx_t::READ_UNCOMMITTED
#define TRX_ISO_READ_COMMITTED trx_t::READ_COMMITTED
#define TRX_ISO_REPEATABLE_READ trx_t::REPEATABLE_READ
#define TRX_ISO_SERIALIZABLE trx_t::SERIALIZABLE

/**
Check if transaction was started. Note, that after the check
situation might have already been changed (and note that holding
the trx_sys->mutex does not prevent state transitions for read-only
transactions).
@param[in] trx          Transaction whose state we need to check
@return true if transaction is in state started */
inline bool trx_was_started(const trx_t *trx) {
  const auto trx_state = trx->state.load(std::memory_order_relaxed);
  return trx_state != TRX_STATE_NOT_STARTED &&
         trx_state != TRX_STATE_FORCED_ROLLBACK;
}

/**
Check if transaction is started.
@param[in] trx          Transaction whose state we need to check
@return true if transaction is in state started */
inline bool trx_is_started(const trx_t *trx) {
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));
  return trx_was_started(trx);
}

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

#ifdef UNIV_DEBUG

/** Test if trx->mutex is owned by the current thread. */
bool inline trx_mutex_own(const trx_t *trx) { return mutex_own(&trx->mutex); }

/**
Verifies the invariants and records debug state related to latching rules.
Called during trx_mutex_enter before the actual mutex acquisition.
@param[in]  trx             The transaction for which trx_mutex_enter(trx) is
                            called
@param[in]  allow_another   If false, then no further calls to trx_mutex_enter
                            are allowed, until trx_mutex_exit().
                            If true, then this must be the first trx acquisition
                            and we will allow one more.
*/
void trx_before_mutex_enter(const trx_t *trx, bool allow_another);

/**
Verifies the invariants and records debug state related to latching rules.
Called during trx_mutex_exit before the actual mutex release.
@param[in]  trx   The transaction for which trx_mutex_exit(trx) is called
*/
void trx_before_mutex_exit(const trx_t *trx);
#endif

/**
Please do not use this low-level macro.
Use trx_mutex_enter(t) instead.
In rare cases where you need to take two trx->mutex-es, take the first one
using trx_mutex_enter_first_of_two(t1), and the second one with
trx_mutex(2)
*/
#define trx_mutex_enter_low(t, first_of_two)       \
  do {                                             \
    ut_ad(!trx_mutex_own(t));                      \
    ut_d(trx_before_mutex_enter(t, first_of_two)); \
    mutex_enter(&t->mutex);                        \
  } while (0)

/** Acquire the trx->mutex (and promise not to request any more). */
#define trx_mutex_enter(t) trx_mutex_enter_low(t, false)

/** Acquire the trx->mutex (and indicate we might request one more). */
#define trx_mutex_enter_first_of_two(t) trx_mutex_enter_low(t, true)

/** Release the trx->mutex. */
#define trx_mutex_exit(t)           \
  do {                              \
    ut_ad(trx_mutex_own(t));        \
    ut_d(trx_before_mutex_exit(t)); \
    mutex_exit(&t->mutex);          \
  } while (0)

/** Track if a transaction is executing inside InnoDB code. It acts
like a gate between the Server and InnoDB.  */
class TrxInInnoDB {
 public:
  /**
  @param[in,out] trx    Transaction entering InnoDB via the handler
  @param[in] disable    true if called from COMMIT/ROLLBACK method */
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
    ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

    const auto trx_state = trx->state.load(std::memory_order_relaxed);

    if (trx_state == TRX_STATE_NOT_STARTED) {
      return (false);
    }

    ut_ad(srv_read_only_mode || trx->in_depth > 0);
    ut_ad(srv_read_only_mode || trx->in_innodb > 0);

    return (trx->abort || trx_state == TRX_STATE_FORCED_ROLLBACK);
  }

  /**
  Start statement requested for transaction.
  @param[in, out] trx   Transaction at the start of a SQL statement */
  static void begin_stmt(trx_t *trx) { enter(trx, false); }

  /**
  Note an end statement for transaction
  @param[in, out] trx   Transaction at end of a SQL statement */
  static void end_stmt(trx_t *trx) { exit(trx); }

  /**
  @return true if the rollback is being initiated by the thread that
          marked the transaction for asynchronous rollback */
  static bool is_async_rollback(const trx_t *trx) {
    return trx->killed_by == std::this_thread::get_id();
  }

 private:
  /** Note that we have crossed into InnoDB code.
  @param[in]    trx     transaction
  @param[in]    disable true if called from COMMIT/ROLLBACK method */
  static void enter(trx_t *trx, bool disable) {
    if (srv_read_only_mode) {
      return;
    }

    ut_ad(!is_async_rollback(trx));
    ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

    /* If it hasn't already been marked for async rollback.
    and it will be committed/rolled back. */
    if (disable) {
      trx_mutex_enter(trx);
      if (!is_forced_rollback(trx) && is_started(trx) &&
          !trx_is_autocommit_non_locking(trx)) {
        ut_ad(trx->killed_by == std::thread::id{});

        /* This transaction has crossed the point of
        no return and cannot be rolled back
        asynchronously now. It must commit or rollback
        synchronously. */

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
  static void wait(const trx_t *trx) {
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

      std::this_thread::sleep_for(std::chrono::microseconds(sleep_time));

      trx_mutex_enter(trx);
    }
  }

 private:
  /**
  @return true if transaction is started */
  static bool is_started(const trx_t *trx) {
    ut_ad(trx_mutex_own(trx));

    return trx_is_started(trx);
  }

  /**
  Transaction instance crossing the handler boundary from the Server. */
  trx_t *m_trx;
};

/** Check if transaction is internal XA transaction
@param[in]      trx     transaction
@return true, iff internal XA transaction. */
bool trx_is_mysql_xa(const trx_t *trx);

/** Update transaction binlog file name and position from session THD.
@param[in,out] trx     current transaction. */
void trx_sys_update_binlog_position(trx_t *trx);

/** Checks whether or not the transaction has been marked as prepared in TC.
@param[in]     trx the transaction
@return true if the transaction is marked as prepared in TC, false otherwise. */
bool trx_is_prepared_in_tc(trx_t const *trx);

/** Does the 2nd phase of an XA transaction prepare for MySQL.
@param[in,out] trx Transaction instance to finish prepare
@return DB_SUCCESS or error number */
dberr_t trx_set_prepared_in_tc_for_mysql(trx_t *trx);

#include "trx0trx.ic"
#endif /* !UNIV_HOTBACKUP */

#endif
