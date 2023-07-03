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

/** @file trx/trx0trx.cc
 The transaction

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#include <sys/types.h>
#include <time.h>
#include <algorithm>
#include <new>
#include <set>

#include <sql_thd_internal_api.h>

#include "btr0sea.h"
#include "clone0clone.h"
#include "current_thd.h"
#include "dict0dd.h"
#include "fsp0sysspace.h"
#include "ha_prototypes.h"
#include "lock0lock.h"
#include "log0chkp.h"
#include "log0write.h"
#include "os0proc.h"
#include "que0que.h"
#include "read0read.h"
#include "row0mysql.h"
#include "srv0mon.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "trx0roll.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0undo.h"
#include "trx0xa.h"
#include "usr0sess.h"
#include "ut0new.h"
#include "ut0pool.h"
#include "ut0vec.h"

#include "my_dbug.h"
#include "mysql/plugin.h"
#include "sql/clone_handler.h"

static const ulint MAX_DETAILED_ERROR_LEN = 256;

/** Set of table_id */
typedef std::set<table_id_t, std::less<table_id_t>, ut::allocator<table_id_t>>
    table_id_set;

/** Map of transactions to affected table_id */
typedef std::map<trx_t *, table_id_set, std::less<trx_t *>,
                 ut::allocator<std::pair<trx_t *const, table_id_set>>>
    trx_table_map;

/** Map of resurrected transactions to affected table_id */
static trx_table_map resurrected_trx_tables;

/* std::vector to store the trx id & table id of tables that needs to be
 * rollbacked. We take SHARED MDL on these tables inside
 * trx_recovery_rollback_thread before letting server accept connections */
std::vector<std::pair<trx_id_t, table_id_t>> to_rollback_trx_tables;

/** Dummy session used currently in MySQL interface */
sess_t *trx_dummy_sess = nullptr;

/** Constructor */
TrxVersion::TrxVersion(trx_t *trx) : m_trx(trx), m_version(trx->version) {
  /* No op */
}

/* The following function makes the transaction committed in memory
and makes its changes to data visible to other transactions.
In particular it releases implicit and explicit locks held by transaction and
transitions to the transaction to the TRX_STATE_COMMITTED_IN_MEMORY state.
NOTE that there is a small discrepancy from the strict formal
visibility rules here: a human user of the database can see
modifications made by another transaction T even before the necessary
log segment has been flushed to the disk. If the database happens to
crash before the flush, the user has seen modifications from T which
will never be a committed transaction. However, any transaction T2
which sees the modifications of the committing transaction T, and
which also itself makes modifications to the database, will get an lsn
larger than the committing transaction T. In the case where the log
flush fails, and T never gets committed, also T2 will never get
committed.
@param[in,out]  trx         The transaction for which will be committed in
                            memory
@param[in]      serialised  true if serialisation log was written. Affects the
                            list of things we need to clean up during
                            trx_erase_lists.
*/
static void trx_release_impl_and_expl_locks(trx_t *trx, bool serialised);

/** Tests the durability settings and flushes logs if needed.
@param[in,out] trx the transaction to flush the logs for.
@param[in]     lsn the identifier of the transaction to flush. */
static void trx_flush_logs(trx_t *trx, lsn_t lsn);

/** Set flush observer for the transaction
@param[in,out]  trx             transaction struct
@param[in]      observer        flush observer */
void trx_set_flush_observer(trx_t *trx, Flush_observer *observer) {
  trx->flush_observer = observer;
}

/** Set detailed error message for the transaction.
@param[in] trx Transaction struct
@param[in] msg Detailed error message */
void trx_set_detailed_error(trx_t *trx, const char *msg) {
  ut_strlcpy(trx->detailed_error, msg, MAX_DETAILED_ERROR_LEN);
}

/** Set detailed error message for the transaction from a file. Note that the
 file is rewinded before reading from it. */
void trx_set_detailed_error_from_file(
    trx_t *trx, /*!< in: transaction struct */
    FILE *file) /*!< in: file to read message from */
{
  os_file_read_string(file, trx->detailed_error, MAX_DETAILED_ERROR_LEN);
}

/** Initialize transaction object.
 @param trx trx to initialize */
static void trx_init(trx_t *trx) {
  /* This is called at the end of commit, do not reset the
  trx_t::state here to NOT_STARTED. The FORCED_ROLLBACK
  status is required for asynchronous handling. */

  trx->id = 0;

  trx->no = TRX_ID_MAX;

  trx->persists_gtid = false;

  trx->skip_lock_inheritance = false;

  trx->is_recovered = false;

  trx->op_info = "";

  trx->isolation_level = TRX_ISO_REPEATABLE_READ;

  trx->check_foreigns = true;

  trx->check_unique_secondary = true;

  trx->lock.n_rec_locks.store(0);

  trx->lock.blocking_trx.store(nullptr);

  trx->dict_operation = TRX_DICT_OP_NONE;

  trx->ddl_operation = false;

  trx->error_state = DB_SUCCESS;

  trx->error_key_num = ULINT_UNDEFINED;

  trx->undo_no = 0;

  trx->rsegs.m_redo.rseg = nullptr;

  trx->rsegs.m_noredo.rseg = nullptr;

  trx->read_only = false;

  trx->auto_commit = false;

  trx->will_lock = 0;

  trx->lock.inherit_all.store(false);

  trx->internal = false;

  trx->in_truncate = false;
#ifdef UNIV_DEBUG
  trx->is_dd_trx = false;
  trx->in_rollback = false;
  trx->lock.in_rollback = false;
#endif /* UNIV_DEBUG */

  ut_d(trx->start_file = nullptr);

  ut_d(trx->start_line = 0);

  trx->magic_n = TRX_MAGIC_N;

  trx->lock.que_state = TRX_QUE_RUNNING;

  trx->last_sql_stat_start.least_undo_no = 0;

  ut_ad(!MVCC::is_view_active(trx->read_view));

  trx->lock.rec_cached = 0;

  trx->lock.table_cached = 0;

  trx->error_index = nullptr;

  /* During asynchronous rollback, we should reset forced rollback flag
  only after rollback is complete to avoid race with the thread owning
  the transaction. */

  if (!TrxInInnoDB::is_async_rollback(trx)) {
    trx->killed_by.store(std::thread::id{});

    /* Note: Do not set to 0, the ref count is decremented inside
    the TrxInInnoDB() destructor. We only need to clear the flags. */

    trx->in_innodb &= TRX_FORCE_ROLLBACK_MASK;
  }

  trx->flush_observer = nullptr;

  ++trx->version;
}

/** For managing the life-cycle of the trx_t instance that we get
from the pool. */
struct TrxFactory {
  /** Initializes a transaction object. It must be explicitly started
  with trx_start_if_not_started() before using it. The default isolation
  level is TRX_ISO_REPEATABLE_READ.
  @param trx Transaction instance to initialise */
  static void init(trx_t *trx) {
    /* Explicitly call the constructor of the already
    allocated object. trx_t objects are allocated by
    ut::zalloc_withkey() in Pool::Pool() which would not call
    the constructors of the trx_t members. */
    new (trx) trx_t();

    trx_init(trx);

    trx->state.store(TRX_STATE_NOT_STARTED, std::memory_order_relaxed);

    trx->dict_operation_lock_mode = 0;

    trx->xid = ut::new_withkey<xid_t>(UT_NEW_THIS_FILE_PSI_KEY);

    trx->detailed_error = reinterpret_cast<char *>(
        ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, MAX_DETAILED_ERROR_LEN));

    trx->lock.lock_heap =
        mem_heap_create(1024, UT_LOCATION_HERE, MEM_HEAP_FOR_LOCK_HEAP);

    mutex_create(LATCH_ID_TRX, &trx->mutex);
    mutex_create(LATCH_ID_TRX_UNDO, &trx->undo_mutex);

    lock_trx_alloc_locks(trx);
  }

  /** Release resources held by the transaction object.
  @param trx the transaction for which to release resources */
  static void destroy(trx_t *trx) {
    ut_a(trx->magic_n == TRX_MAGIC_N);
    ut_ad(!trx->in_rw_trx_list);
    ut_ad(!trx->in_mysql_trx_list);

    ut_a(trx->lock.wait_lock == nullptr);
    ut_a(trx->lock.wait_thr == nullptr);
    ut_a(trx->lock.blocking_trx.load() == nullptr);

    ut_a(!trx->has_search_latch);

    ut_a(trx->dict_operation_lock_mode == 0);

    if (trx->lock.lock_heap != nullptr) {
      mem_heap_free(trx->lock.lock_heap);
      trx->lock.lock_heap = nullptr;
    }

    ut_a(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);

    ut::delete_(trx->xid);
    ut::free(trx->detailed_error);

    mutex_free(&trx->mutex);
    mutex_free(&trx->undo_mutex);

    trx->mod_tables.~trx_mod_tables_t();

    ut_ad(trx->read_view == nullptr);

    if (!trx->lock.rec_pool.empty()) {
      /* See lock_trx_alloc_locks() why we only free
      the first element. */

      ut::free(trx->lock.rec_pool[0]);
    }

    if (!trx->lock.table_pool.empty()) {
      /* See lock_trx_alloc_locks() why we only free
      the first element. */

      ut::free(trx->lock.table_pool[0]);
    }

    trx->lock.rec_pool.~lock_pool_t();

    trx->lock.table_pool.~lock_pool_t();
  }

  /** Enforce any invariants here, this is called before the transaction
  is added to the pool.
  @return true if all OK */
  static bool debug(const trx_t *trx) {
    ut_a(trx->error_state == DB_SUCCESS);

    ut_a(trx->magic_n == TRX_MAGIC_N);

    ut_ad(!trx->read_only);

    ut_ad(!trx_was_started(trx));

    ut_ad(trx->dict_operation == TRX_DICT_OP_NONE);

    ut_ad(trx->mysql_thd == nullptr);

    ut_ad(!trx->in_rw_trx_list);
    ut_ad(!trx->in_mysql_trx_list);

    ut_a(trx->lock.wait_thr == nullptr);
    ut_a(trx->lock.wait_lock == nullptr);
    ut_a(trx->lock.blocking_trx.load() == nullptr);

    ut_a(!trx->has_search_latch);

    ut_a(trx->dict_operation_lock_mode == 0);

    ut_a(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);

    ut_ad(trx->lock.autoinc_locks == nullptr);

    ut_ad(!trx->lock.inherit_all.load());

    ut_ad(!trx->abort);

    ut_ad(trx->killed_by == std::thread::id{});

    return (true);
  }
};

/** The lock strategy for TrxPool */
struct TrxPoolLock {
  TrxPoolLock() = default;

  /** Create the mutex */
  void create() { mutex_create(LATCH_ID_TRX_POOL, &m_mutex); }

  /** Acquire the mutex */
  void enter() { mutex_enter(&m_mutex); }

  /** Release the mutex */
  void exit() { mutex_exit(&m_mutex); }

  /** Free the mutex */
  void destroy() { mutex_free(&m_mutex); }

  /** Mutex to use */
  ib_mutex_t m_mutex;
};

/** The lock strategy for the TrxPoolManager */
struct TrxPoolManagerLock {
  TrxPoolManagerLock() = default;

  /** Create the mutex */
  void create() { mutex_create(LATCH_ID_TRX_POOL_MANAGER, &m_mutex); }

  /** Acquire the mutex */
  void enter() { mutex_enter(&m_mutex); }

  /** Release the mutex */
  void exit() { mutex_exit(&m_mutex); }

  /** Free the mutex */
  void destroy() { mutex_free(&m_mutex); }

  /** Mutex to use */
  ib_mutex_t m_mutex;
};

/** Use explicit mutexes for the trx_t pool and its manager. */
typedef Pool<trx_t, TrxFactory, TrxPoolLock> trx_pool_t;
typedef PoolManager<trx_pool_t, TrxPoolManagerLock> trx_pools_t;

/** The trx_t pool manager */
static trx_pools_t *trx_pools;

/** Size of on trx_t pool in bytes. */
static const ulint MAX_TRX_BLOCK_SIZE = 1024 * 1024 * 4;

/** Create the trx_t pool */
void trx_pool_init() {
  trx_pools = ut::new_withkey<trx_pools_t>(UT_NEW_THIS_FILE_PSI_KEY,
                                           MAX_TRX_BLOCK_SIZE);

  ut_a(trx_pools != nullptr);
}

/** Destroy the trx_t pool */
void trx_pool_close() {
  ut::delete_(trx_pools);

  trx_pools = nullptr;
}

/** @return a trx_t instance from trx_pools. */
static trx_t *trx_create_low() {
  trx_t *trx = trx_pools->get();

  assert_trx_is_free(trx);

  mem_heap_t *heap;
  ib_alloc_t *alloc;

  /* We just got trx from pool, it should be non locking */
  ut_ad(trx->will_lock == 0);

  trx->persists_gtid = false;

  trx->api_trx = false;

  trx->api_auto_commit = false;

  trx->read_write = true;

  trx->purge_sys_trx = false;

  /* Background trx should not be forced to rollback,
  we will unset the flag for user trx. */
  trx->in_innodb |= TRX_FORCE_ROLLBACK_DISABLE;

  /* Trx state can be TRX_STATE_FORCED_ROLLBACK if
  the trx was forced to rollback before it's reused.*/
  trx->state.store(TRX_STATE_NOT_STARTED, std::memory_order_relaxed);

  heap = mem_heap_create(sizeof(ib_vector_t) + sizeof(void *) * 8,
                         UT_LOCATION_HERE);

  alloc = ib_heap_allocator_create(heap);

  /* Remember to free the vector explicitly in trx_free(). */
  trx->lock.autoinc_locks = ib_vector_create(alloc, sizeof(void **), 4);

  /* Should have been either just initialized or .clear()ed by
  trx_free(). */
  ut_a(trx->mod_tables.size() == 0);

  return (trx);
}

/**
Release a trx_t instance back to the pool.
@param trx the instance to release. */
static void trx_free(trx_t *&trx) {
  assert_trx_is_free(trx);

  trx->mysql_thd = nullptr;

  // FIXME: We need to avoid this heap free/alloc for each commit.
  if (trx->lock.autoinc_locks != nullptr) {
    ut_ad(ib_vector_is_empty(trx->lock.autoinc_locks));
    /* We allocated a dedicated heap for the vector. */
    ib_vector_free(trx->lock.autoinc_locks);
    trx->lock.autoinc_locks = nullptr;
  }

  trx->mod_tables.clear();

  ut_ad(trx->read_view == nullptr);
  ut_ad(trx->is_dd_trx == false);

  /* trx locking state should have been reset before returning trx
  to pool */
  ut_ad(trx->will_lock == 0);

  trx_pools->mem_free(trx);

  trx = nullptr;
}

/** Creates a transaction object for background operations by the master thread.
 @return own: transaction object */
trx_t *trx_allocate_for_background(void) {
  trx_t *trx;

  trx = trx_create_low();

  trx->sess = trx_dummy_sess;

  return (trx);
}

/** Creates a transaction object for MySQL.
 @return own: transaction object */
trx_t *trx_allocate_for_mysql(void) {
  trx_t *trx;

  trx = trx_allocate_for_background();

  trx_sys_mutex_enter();

  ut_d(trx->in_mysql_trx_list = true);
  UT_LIST_ADD_FIRST(trx_sys->mysql_trx_list, trx);

  trx_sys_mutex_exit();

  return (trx);
}

/** Check state of transaction before freeing it.
@param[in,out]  trx     transaction object to validate */
static void trx_validate_state_before_free(trx_t *trx) {
  if (trx->declared_to_be_inside_innodb) {
    ib::error(ER_IB_MSG_1202)
        << "Freeing a trx (" << trx << ", " << trx_get_id_for_print(trx)
        << ") which is declared"
           " to be processing inside InnoDB";

    trx_print(stderr, trx, 600);
    putc('\n', stderr);

    /* This is an error but not a fatal error. We must keep
    the counters like srv_conc_n_threads accurate. */
    srv_conc_force_exit_innodb(trx);
  }

  if (trx->n_mysql_tables_in_use != 0 || trx->mysql_n_tables_locked != 0) {
    ib::error(ER_IB_MSG_1203)
        << "MySQL is freeing a thd though trx->n_mysql_tables_in_use is "
        << trx->n_mysql_tables_in_use << " and trx->mysql_n_tables_locked is "
        << trx->mysql_n_tables_locked << ".";

    trx_print(stderr, trx, 600);
    ut_print_buf(stderr, trx, sizeof(trx_t));
    putc('\n', stderr);
  }

  trx->dict_operation = TRX_DICT_OP_NONE;
  assert_trx_is_inactive(trx);
}

/** Free and initialize a transaction object instantiated during recovery.
@param[in,out]  trx     transaction object to free and initialize */
void trx_free_resurrected(trx_t *trx) {
  trx_validate_state_before_free(trx);

  trx_init(trx);

  trx_free(trx);
}

/** Free a transaction that was allocated by background or user threads.
@param[in,out]  trx     transaction object to free */
void trx_free_for_background(trx_t *trx) {
  trx_validate_state_before_free(trx);

  trx_free(trx);
}

void trx_free_prepared_or_active_recovered(trx_t *trx) {
  ut_a(trx->magic_n == TRX_MAGIC_N);

  bool was_prepared{false};
  if (trx->state.load(std::memory_order_relaxed) == TRX_STATE_ACTIVE) {
    ut_a(trx_state_eq(trx, TRX_STATE_ACTIVE));
    ut_a(trx->is_recovered);
  } else {
    ut_a(trx_state_eq(trx, TRX_STATE_PREPARED));
    was_prepared = true;
  }
  /* A PREPARED transaction which got disconnected often has nonzero will_lock,
  yet trx_free() expects it to be cleared. We clear it at the latest possible
  moment, instead of doing it immediately on disconnect, because this field is
  used to check if this transaction is "non-locking" in various functions which
  might be called either here, or when another client reconnects to XA COMMIT or
  XA ROLLBACK. Usually the field is cleared during rollback or commit, here we
  have to do it ourselves as we neither rollback nor commit, just "free" it. */
  ut_ad(!trx->will_lock || trx_state_eq(trx, TRX_STATE_PREPARED));
  assert_trx_in_rw_list(trx);

  trx_release_impl_and_expl_locks(trx, false);
  trx_undo_free_trx_with_prepared_or_active_logs(trx, was_prepared);

  ut_ad(!trx->in_rw_trx_list);
  ut_a(!trx->read_only);

  trx->state.store(TRX_STATE_NOT_STARTED, std::memory_order_relaxed);
  trx->will_lock = 0;

  trx_free(trx);
}

/** Disconnect a transaction from MySQL.
@param[in,out]  trx             transaction
@param[in]      prepared        boolean value to specify whether trx is in
                                TRX_STATE_PREPARED state (such as after XA
                                PREPARE) and we want to unlink it from the
                                mysql_thd object, so it can potentially be
                                linked to another session in future.
*/
inline void trx_disconnect_from_mysql(trx_t *trx, bool prepared) {
  trx_sys_mutex_enter();

  ut_ad(trx->in_mysql_trx_list);
  ut_d(trx->in_mysql_trx_list = false);

  UT_LIST_REMOVE(trx_sys->mysql_trx_list, trx);

  if (trx->read_view != nullptr) {
    trx_sys->mvcc->view_close(trx->read_view, true);
  }

  ut_ad(trx_sys_validate_trx_list());

  if (prepared) {
    ut_ad(trx_state_eq(trx, TRX_STATE_PREPARED));

    /* During disconnection there is a short period when the server layer
    already believes this XID is detached from this connection, and thus another
    connection may try to XA COMMIT/ROLLBACK it, yet InnoDB is still processing
    the trx object - another client can be executing innobase_commit_by_xid() in
    parallel to our trx_disconnect_from_mysql(). We use trx->mysql_thd field,
    under protection of trx_sys->mutex to synchronize with trx_get_trx_by_xid()
    and convey if this trx is still attached to this thd or not. */
    ut_ad(trx->mysql_thd != nullptr);
    trx->mysql_thd = nullptr;
  }

  trx_sys_mutex_exit();
}

/** Disconnect a transaction from MySQL.
@param[in,out]  trx     transaction */
inline void trx_disconnect_plain(trx_t *trx) {
  trx_disconnect_from_mysql(trx, false);
}

/** Disconnect a prepared transaction from MySQL.
@param[in,out]  trx     transaction */
void trx_disconnect_prepared(trx_t *trx) {
  trx_disconnect_from_mysql(trx, true);
}

/** Free a transaction object for MySQL.
@param[in,out]  trx     transaction */
void trx_free_for_mysql(trx_t *trx) {
  trx_disconnect_plain(trx);
  trx_free_for_background(trx);
}

/** Resurrect the table IDs for a resurrected transaction.
@param[in]      trx             resurrected transaction
@param[in]      undo_ptr        pointer to undo segment
@param[in]      undo            undo log */
static void trx_resurrect_table_ids(trx_t *trx, const trx_undo_ptr_t *undo_ptr,
                                    const trx_undo_t *undo) {
  mtr_t mtr;
  page_t *undo_page;
  trx_undo_rec_t *undo_rec;

  ut_ad(undo == undo_ptr->insert_undo || undo == undo_ptr->update_undo);

  if (trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY) || undo->empty) {
    return;
  }

  table_id_set empty;
  table_id_set &tables =
      resurrected_trx_tables.insert(trx_table_map::value_type(trx, empty))
          .first->second;

  mtr_start(&mtr);

  /* trx_rseg_mem_create() may have acquired an X-latch on this
  page, so we cannot acquire an S-latch. */
  undo_page = trx_undo_page_get(page_id_t(undo->space, undo->top_page_no),
                                undo->page_size, &mtr);

  undo_rec = undo_page + undo->top_offset;
  ulong n_undo_recs = 0;
  ulong n_undo_pages = 0;
  ulong n_tables = 0;

  auto last_progress_log_time = std::chrono::steady_clock::now();
  using namespace std::chrono_literals;
  // Since resurrecting a transaction can take a long time, progress is logged
  // at regular intervals to the error log. The debug value is provided for
  // testing
  auto const progress_log_interval =
      DBUG_EVALUATE_IF("resurrect_logs", 1s, 30s);

  do {
    ulint type;
    undo_no_t undo_no;
    table_id_t table_id;
    ulint cmpl_info;
    bool updated_extern;
    type_cmpl_t type_cmpl;
    page_t *undo_rec_page = page_align(undo_rec);

    if (undo_rec_page != undo_page) {
      n_undo_pages++;
      mtr.release_page(undo_page, MTR_MEMO_PAGE_X_FIX);
      undo_page = undo_rec_page;
    }

    trx_undo_rec_get_pars(undo_rec, &type, &cmpl_info, &updated_extern,
                          &undo_no, &table_id, type_cmpl);
    if (tables.insert(table_id).second == true) {
      n_tables++;
      ib::info(ER_IB_RESURRECT_IDENTIFY_TABLE_TO_LOCK, ulong(table_id));
    }

    undo_rec = trx_undo_get_prev_rec(undo_rec, undo->hdr_page_no,
                                     undo->hdr_offset, false, &mtr);
    n_undo_recs++;

    auto now = std::chrono::steady_clock::now();
    auto time_diff = now - last_progress_log_time;

    if (time_diff >= progress_log_interval) {
      ib::info(ER_IB_RESURRECT_RECORD_PROGRESS, n_undo_recs, n_undo_pages);
      last_progress_log_time = now;
    }
  } while (undo_rec);

  ib::info(ER_IB_RESURRECT_RECORD_COMPLETE, n_undo_recs, n_undo_pages,
           n_tables);

  mtr_commit(&mtr);
}

void trx_resurrect_locks(bool all) {
  for (const auto &element : resurrected_trx_tables) {
    trx_t *trx = element.first;

    /* We deal only with recovered transactions. If all is false,
    we skip non dictionary transactions. */
    if (!trx->is_recovered || (!all && !trx->ddl_operation)) {
      continue;
    }

    const table_id_set &tables = element.second;

    for (auto table_id : tables) {
      auto table = dd_table_open_on_id(table_id, nullptr, nullptr, false, true);

      if (table == nullptr) {
        continue;
      }

      ut_ad(!table->is_temporary());

      if (table->ibd_file_missing || table->is_temporary()) {
        dict_sys_mutex_enter();
        dd_table_close(table, nullptr, nullptr, true);
        dict_table_remove_from_cache(table);
        dict_sys_mutex_exit();
        continue;
      }

      bool is_XA = false;

      if (trx->state.load(std::memory_order_relaxed) == TRX_STATE_PREPARED &&
          !dict_table_is_sdi(table->id)) {
        trx->mod_tables.insert(table);
        is_XA = true;
      }
      DICT_TF2_FLAG_SET(table, DICT_TF2_RESURRECT_PREPARED);

      /* We don't rollback DDL or XA prepared transaction in background */
      if (!all || is_XA) {
        lock_table_ix_resurrect(table, trx);
        ib::info(ER_IB_RESURRECT_ACQUIRE_TABLE_LOCK, ulong(table->id),
                 table->name.m_name);
      } else {
        /* MDL & IX_LOCK is taken inside trx_recovery_rollback_thread */
        to_rollback_trx_tables.push_back(std::make_pair(trx->id, table_id));
      }

      DBUG_PRINT("ib_trx", ("resurrect" TRX_ID_FMT "  table '%s' IX lock",
                            trx_get_id_for_print(trx), table->name.m_name));

      dd_table_close(table, nullptr, nullptr, false);
    }
  }
}

void trx_clear_resurrected_table_ids() { resurrected_trx_tables.clear(); }

/** Resurrect the transactions that were doing inserts at the time of the
 crash, they need to be undone.
 @return trx_t instance */
static trx_t *trx_resurrect_insert(
    trx_undo_t *undo, /*!< in: entry to UNDO */
    trx_rseg_t *rseg) /*!< in: rollback segment */
{
  trx_t *trx;

  trx = trx_allocate_for_background();

  ut_d(trx->start_file = __FILE__);
  ut_d(trx->start_line = __LINE__);

  rseg->trx_ref_count++;
  trx->rsegs.m_redo.rseg = rseg;
  *trx->xid = undo->xid;
  trx->id = undo->trx_id;
  trx_sys_rw_trx_add(trx);
  trx->rsegs.m_redo.insert_undo = undo;
  trx->is_recovered = true;

  /* This is single-threaded startup code, we do not need the
  protection of trx->mutex or trx_sys->mutex here. */

  if (undo->state != TRX_UNDO_ACTIVE) {
    /* Prepared transactions are left in the prepared state
    waiting for a commit or abort decision from MySQL */

    if (undo->is_prepared()) {
      ib::info(ER_IB_MSG_1204) << "Transaction " << trx_get_id_for_print(trx)
                               << " was in the XA prepared state.";

      if (srv_force_recovery == 0) {
        trx->state.store(TRX_STATE_PREPARED, std::memory_order_relaxed);
        ++trx_sys->n_prepared_trx;
      } else {
        ib::info(ER_IB_MSG_1205) << "Since innodb_force_recovery"
                                    " > 0, we will force a rollback.";

        trx->state.store(TRX_STATE_ACTIVE, std::memory_order_relaxed);
      }
    } else {
      trx->state.store(TRX_STATE_COMMITTED_IN_MEMORY,
                       std::memory_order_relaxed);
    }

    /* We give a dummy value for the trx no; this should have no
    relevance since purge is not interested in committed
    transaction numbers, unless they are in the history
    list, in which case it looks the number from the disk based
    undo log structure */

    trx->no = trx->id;

  } else {
    trx->state.store(TRX_STATE_ACTIVE, std::memory_order_relaxed);

    /* A running transaction always has the number
    field inited to TRX_ID_MAX */

    trx->no = TRX_ID_MAX;
  }

  /* trx_start_low() is not called with resurrect, so need to initialize
  start time here.*/
  if (trx->state.load(std::memory_order_relaxed) == TRX_STATE_ACTIVE ||
      trx->state.load(std::memory_order_relaxed) == TRX_STATE_PREPARED) {
    trx->start_time.store(std::chrono::system_clock::from_time_t(time(nullptr)),
                          std::memory_order_relaxed);
  }

  trx->ddl_operation = undo->dict_operation;

  if (undo->dict_operation) {
    trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);
  }

  if (!undo->empty) {
    trx->undo_no = undo->top_undo_no + 1;
    trx->undo_rseg_space = undo->rseg->space_id;
  }

  return (trx);
}

/** Prepared transactions are left in the prepared state waiting for a
 commit or abort decision from MySQL */
static void trx_resurrect_update_in_prepared_state(
    trx_t *trx,             /*!< in,out: transaction */
    const trx_undo_t *undo) /*!< in: update UNDO record */
{
  /* This is single-threaded startup code, we do not need the
  protection of trx->mutex or trx_sys->mutex here. */

  if (undo->is_prepared()) {
    ib::info(ER_IB_MSG_1206) << "Transaction " << trx_get_id_for_print(trx)
                             << " was in the XA prepared state.";

    ut_ad(trx->state.load(std::memory_order_relaxed) !=
          TRX_STATE_FORCED_ROLLBACK);

    if (trx_state_eq(trx, TRX_STATE_NOT_STARTED)) {
      ++trx_sys->n_prepared_trx;
    } else {
      ut_ad(trx_state_eq(trx, TRX_STATE_PREPARED));
    }

    trx->state.store(TRX_STATE_PREPARED, std::memory_order_relaxed);
  } else {
    trx->state.store(TRX_STATE_COMMITTED_IN_MEMORY, std::memory_order_relaxed);
  }
}

/** Resurrect the transactions that were doing updates the time of the
 crash, they need to be undone. */
static void trx_resurrect_update(
    trx_t *trx,       /*!< in/out: transaction */
    trx_undo_t *undo, /*!< in/out: update UNDO record */
    trx_rseg_t *rseg) /*!< in/out: rollback segment */
{
  /* This resurected transaction might also have been doing inserts.
  If so, this rseg is already assigned by trx_resurrect_insert(). */
  if (trx->rsegs.m_redo.rseg != nullptr) {
    ut_a(trx->rsegs.m_redo.rseg == rseg);
    ut_ad(trx->id == undo->trx_id);
    ut_ad(trx->is_recovered);
    /* For GTID persistence, we might have empty update undo for
    insert only transactions. */
    if (undo->empty && trx_state_eq(trx, TRX_STATE_PREPARED)) {
      undo->set_prepared(trx->xid);
    }
    ut_ad(undo->xid.eq(trx->xid));
  } else {
    rseg->trx_ref_count++;
    trx->rsegs.m_redo.rseg = rseg;
    *trx->xid = undo->xid;
    trx->id = undo->trx_id;
    trx_sys_rw_trx_add(trx);
    trx->is_recovered = true;
  }

  /* Assign the update_undo segment. */
  ut_a(trx->rsegs.m_redo.update_undo == nullptr);
  trx->rsegs.m_redo.update_undo = undo;

  /* This is single-threaded startup code, we do not need the
  protection of trx->mutex or trx_sys->mutex here. */

  if (undo->state != TRX_UNDO_ACTIVE) {
    trx_resurrect_update_in_prepared_state(trx, undo);

    /* We give a dummy value for the trx number */

    trx->no = trx->id;

  } else {
    trx->state.store(TRX_STATE_ACTIVE, std::memory_order_relaxed);

    /* A running transaction always has the number field inited to
    TRX_ID_MAX */

    trx->no = TRX_ID_MAX;
  }

  /* trx_start_low() is not called with resurrect, so need to initialize
  start time here.*/
  if (trx->state.load(std::memory_order_relaxed) == TRX_STATE_ACTIVE ||
      trx->state.load(std::memory_order_relaxed) == TRX_STATE_PREPARED) {
    trx->start_time.store(std::chrono::system_clock::from_time_t(time(nullptr)),
                          std::memory_order_relaxed);
  }

  trx->ddl_operation = undo->dict_operation;

  if (undo->dict_operation) {
    trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);
  }

  if (!undo->empty && undo->top_undo_no >= trx->undo_no) {
    trx->undo_no = undo->top_undo_no + 1;
    trx->undo_rseg_space = undo->rseg->space_id;
  }
}

/** Resurrect the transactions that were doing inserts and updates at
the time of a crash, they need to be undone.
@param[in]      rseg    rollback segment */
static void trx_resurrect(trx_rseg_t *rseg) {
  ut_ad(rseg != nullptr);
  ulong ins_trx_count = 0;
  ulong upd_trx_count = 0;
  /* Resurrect transactions that were doing inserts. */
  for (auto undo : rseg->insert_undo_list) {
    auto trx = trx_resurrect_insert(undo, rseg);
    ins_trx_count++;
    ib::info(ER_IB_RESURRECT_TRX_INSERT, ulong(trx->id));
    trx_resurrect_table_ids(trx, &trx->rsegs.m_redo, undo);
  }
  if (ins_trx_count > 0) {
    ib::info(ER_IB_RESURRECT_TRX_INSERT_COMPLETE, ins_trx_count);
  }
  /* Ressurrect transactions that were doing updates. */
  for (auto undo : rseg->update_undo_list) {
    /* Check the active_rw_trxs.by_id first. */

    trx_t *trx = trx_sys->latch_and_execute_with_active_trx(
        undo->trx_id, [](trx_t *trx) { return trx; }, UT_LOCATION_HERE);

    if (trx == nullptr) {
      trx = trx_allocate_for_background();

      ut_d(trx->start_file = __FILE__);
      ut_d(trx->start_line = __LINE__);
    }

    trx_resurrect_update(trx, undo, rseg);
    upd_trx_count++;
    ib::info(ER_IB_RESURRECT_TRX_UPDATE, ulong(trx->id));
    trx_resurrect_table_ids(trx, &trx->rsegs.m_redo, undo);
  }
  if (upd_trx_count > 0) {
    ib::info(ER_IB_RESURRECT_TRX_UPDATE_COMPLETE, upd_trx_count);
  }
}

/** Adds the transaction to trx_sys->rw_trx_list
Requires trx_sys->mutex, unless called in the single threaded startup code.
@param[in]  trx   The transaction assumed to not be in the rw_trx_list yet
*/
static inline void trx_add_to_rw_trx_list(trx_t *trx) {
  ut_ad(srv_is_being_started || trx_sys_mutex_own());
  ut_ad(!trx->in_rw_trx_list);
  UT_LIST_ADD_FIRST(trx_sys->rw_trx_list, trx);
  ut_d(trx->in_rw_trx_list = true);
}

/** Removes the transaction from trx_sys->rw_trx_list.
Requires trx_sys->mutex, unless called in the single threaded startup code.
@param[in]  trx   The transaction assumed to be in the rw_trx_list
*/
static inline void trx_remove_from_rw_trx_list(trx_t *trx) {
  ut_ad(srv_is_being_started || trx_sys_mutex_own());
  ut_ad(trx->in_rw_trx_list);
  UT_LIST_REMOVE(trx_sys->rw_trx_list, trx);
  ut_d(trx->in_rw_trx_list = false);
}

/** Creates trx objects for transactions and initializes the trx list of
 trx_sys at database start. Rollback segments and undo log lists must
 already exist when this function is called, because the lists of
 transactions to be rolled back or cleaned up are built based on the
 undo log lists. */
void trx_lists_init_at_db_start(void) {
  ut_a(srv_is_being_started);

  /* Look through the rollback segments in the TRX_SYS for
  transaction undo logs. */
  for (auto rseg : trx_sys->rsegs) {
    trx_resurrect(rseg);
  }

  /* Look through the rollback segments in each RSEG_ARRAY for
  transaction undo logs. */
  undo::spaces->s_lock();
  for (auto undo_space : undo::spaces->m_spaces) {
    undo_space->rsegs()->s_lock();
    for (auto rseg : *undo_space->rsegs()) {
      trx_resurrect(rseg);
    }
    undo_space->rsegs()->s_unlock();
  }
  undo::spaces->s_unlock();

  ut::vector<trx_t *> trxs;
  for (auto &shard : trx_sys->shards) {
    shard.active_rw_trxs.latch_and_execute(
        [&](const Trx_by_id_with_min &trx_by_id_with_min) {
          for (const auto &trx_track : trx_by_id_with_min.by_id()) {
            trxs.emplace_back(trx_track.second);
          }
        },
        UT_LOCATION_HERE);
  }
  std::sort(trxs.begin(), trxs.end(),
            [&](trx_t *a, trx_t *b) { return a->id < b->id; });

  for (trx_t *trx : trxs) {
    if (trx->state.load(std::memory_order_relaxed) == TRX_STATE_ACTIVE ||
        trx->state.load(std::memory_order_relaxed) == TRX_STATE_PREPARED) {
      trx_sys->rw_trx_ids.push_back(trx->id);
    }
    trx_add_to_rw_trx_list(trx);
  }
}

/** Get next redo rollback segment in round-robin fashion.
While InnoDB is running in multi-threaded mode, the vectors of undo
tablespaces and rsegs do not shrink.  So they do not need protection
to get a pointer to an rseg.
If an rseg is not marked for undo tablespace truncation, we assign
it to a transaction. We increment trx_ref_count to keep the purge
thread from truncating the undo tablespace that contains this rseg
until the transaction is done with it.
@return assigned rollback segment instance */
static trx_rseg_t *get_next_redo_rseg_from_undo_spaces() {
  undo::Tablespace *undo_space;

  /* The number of undo tablespaces cannot be changed while
  we have this s_lock. */
  undo::spaces->s_lock();

  /* Use all known undo tablespaces.  Some may be inactive. */
  ulint target_undo_tablespaces = undo::spaces->size();

  ut_ad(target_undo_tablespaces > 0);

  /* The number of rollback segments may be changed at any instant.
  So use the value at this instant.  Rollback segments are never
  deleted from an rseg list, so srv_rollback_segments is always
  less than rsegs->size(). */
  ulint target_rollback_segments = srv_rollback_segments;

  static std::atomic<ulint> rseg_counter{0};
  trx_rseg_t *rseg = nullptr;
  ulint current = rseg_counter;

  while (rseg == nullptr) {
    /* Increment the static redo_rseg_slot so the next call from any thread
    starts with the next rseg. */
    rseg_counter.fetch_add(1);

    /* Traverse the rsegs like this: (space, rseg_id)
    (0,0), (1,0), ... (n,0), (0,1), (1,1), ... (n,1), ... */
    ulint window =
        current % (target_rollback_segments * target_undo_tablespaces);
    ulint spaces_slot = window % target_undo_tablespaces;
    ulint rseg_slot = window / target_undo_tablespaces;

    current++;

    undo_space = undo::spaces->at(spaces_slot);

    /* Avoid any rseg that resides in a tablespace that has been made
    inactive either explicitly or by being marked for truncate. We do
    not want to wait here on an x_lock for an rseg in an undo tablespace
    that is being truncated.  So check this first without the latch.
    It could be set immediately after this, but that is a very short gap
    and the get_active() call below will use an rseg->s_lock. */
    if (!undo_space->is_active_no_latch()) {
      continue;
    }

    /* This is done here because we know the rsegs() pointer is good. */
    ut_ad(target_rollback_segments <= undo_space->rsegs()->size());

    /* Check again with a shared lock. */
    rseg = undo_space->get_active(rseg_slot);
    if (rseg == nullptr) {
      continue;
    }
  }

  undo::spaces->s_unlock();

  ut_ad(rseg->trx_ref_count > 0);

  return (rseg);
}

/** Get the next redo rollback segment in round-robin fashion.
The assigned slots may have gaps but the vector does not.
@return assigned rollback segment instance */
static trx_rseg_t *get_next_redo_rseg_from_trx_sys() {
  static std::atomic<ulint> rseg_counter{0};
  ulong n_rollback_segments = srv_rollback_segments;

  /* Versions 5.6 and 5.7 of InnoDB would allow 128 as the max for
  innodb_rollback_segments but would only use 96 since 32 slots were
  used for temporary rsegs. Now those rsegs are in trx_sys_t::tmp_rsegs
  and trx_sys_t::rsegs which each can hold all 128.  As a result,
  an existing system tablespace might have gaps in the slot assignment.
  The Rsegs vector only contains the rsegs that exist. Since
  srv_rollback_segments can be set to a smaller number at runtime,
  it might be smaller than Rsegs::size().  But srv_rollback_segments
  can never be larger than Rsegs::size() because when the user increases
  innodb_rollback_segments, the rollback segments are created and rseg
  objects are added to the vector ready to use before
  srv_rollback_segments is increased. */
  ut_ad(n_rollback_segments <= trx_sys->rsegs.size());

  /* Try the next slot that no other thread is looking at */
  ulint slot = (rseg_counter.fetch_add(1) + 1) % n_rollback_segments;

  /* s_lock the vector since it might be sorted when added to. */
  trx_sys->rsegs.s_lock();
  trx_rseg_t *rseg = trx_sys->rsegs.at(slot);
  trx_sys->rsegs.s_unlock();

  /* It is not necessary to s_lock Rsegs::m_latch here because the
  system tablespace is never truncated like other undo tablespaces. */
  rseg->trx_ref_count++;

  ut_ad(rseg->space_id == TRX_SYS_SPACE);

  return (rseg);
}

/** Get next redo rollback segment in round-robin fashion.
We assume that the assigned slots are not contiguous and have gaps.
@return assigned rollback segment instance */
static trx_rseg_t *get_next_redo_rseg() {
  if (!trx_sys->rsegs.is_empty()) {
    return (get_next_redo_rseg_from_trx_sys());
  } else {
    return (get_next_redo_rseg_from_undo_spaces());
  }
}

/** Get the next noredo rollback segment.
@return assigned rollback segment instance */
static trx_rseg_t *get_next_temp_rseg() {
  static std::atomic<ulint> temp_rseg_counter{0};
  ulong n_rollback_segments = srv_rollback_segments;

  ut_ad(n_rollback_segments <= trx_sys->tmp_rsegs.size());

  /* Try the next slot that no other thread is looking at */
  ulint slot = (temp_rseg_counter.fetch_add(1) + 1) % n_rollback_segments;

  /* No need to s_lock the vector since it is only added to at the end,
  and it is never resized or sorted. */
  trx_rseg_t *rseg = trx_sys->tmp_rsegs.at(slot);

  ut_ad(rseg->id == slot);
  ut_ad(fsp_is_system_temporary(rseg->space_id));

  return (rseg);
}

/** Assign a durable rollback segment to a transaction in a round-robin
fashion.
@param[in,out]  trx     transaction that involves a durable write. */
void trx_assign_rseg_durable(trx_t *trx) {
  ut_ad(trx->rsegs.m_redo.rseg == nullptr);

  trx->rsegs.m_redo.rseg = srv_read_only_mode ? nullptr : get_next_redo_rseg();
}

/** Assign a temp-tablespace bound rollback-segment to a transaction.
@param[in,out]  trx     transaction that involves write to temp-table. */
void trx_assign_rseg_temp(trx_t *trx) {
  ut_ad(trx->rsegs.m_noredo.rseg == nullptr);
  ut_ad(!trx_is_autocommit_non_locking(trx));

  trx->rsegs.m_noredo.rseg =
      srv_read_only_mode ? nullptr : get_next_temp_rseg();

  if (trx->id == 0) {
    trx_sys_mutex_enter();

    trx->id = trx_sys_allocate_trx_id();

    trx_sys->rw_trx_ids.push_back(trx->id);

    trx_sys_mutex_exit();

    trx_sys_rw_trx_add(trx);
  }
}

/** Starts a transaction. */
static void trx_start_low(
    trx_t *trx,      /*!< in: transaction */
    bool read_write) /*!< in: true if read-write transaction */
{
  ut_ad(!trx->in_rollback);
  ut_ad(!trx->is_recovered);
  ut_ad(trx->start_line != 0);
  ut_ad(trx->start_file != nullptr);
  ut_ad(trx->roll_limit == 0);
  ut_ad(!trx->lock.in_rollback);
  ut_ad(trx->error_state == DB_SUCCESS);
  ut_ad(trx->rsegs.m_redo.rseg == nullptr);
  ut_ad(trx->rsegs.m_noredo.rseg == nullptr);
  ut_ad(trx_state_eq(trx, TRX_STATE_NOT_STARTED));
  ut_ad(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);
  ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK));
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  ++trx->version;

  /* Check whether it is an AUTOCOMMIT SELECT */
  trx->auto_commit = (trx->api_trx && trx->api_auto_commit) ||
                     thd_trx_is_auto_commit(trx->mysql_thd);

  trx->read_only = (trx->api_trx && !trx->read_write) ||
                   (!trx->internal && thd_trx_is_read_only(trx->mysql_thd)) ||
                   srv_read_only_mode;

  if (!trx->auto_commit) {
    ++trx->will_lock;
  } else if (trx->will_lock == 0) {
    trx->read_only = true;
  }
  trx->persists_gtid = false;

#ifdef UNIV_DEBUG
  /* If the transaction is DD attachable trx, it should be AC-NL-RO
  (AutoCommit-NonLocking-ReadOnly) trx */
  if (trx->is_dd_trx) {
    ut_ad(trx->read_only);
    ut_ad(trx->auto_commit);
    ut_ad(trx->isolation_level == TRX_ISO_READ_UNCOMMITTED ||
          trx->isolation_level == TRX_ISO_READ_COMMITTED);
  }
#endif /* UNIV_DEBUG */

  /* Note, that trx->start_time is set without std::memory_order_release,
  and it is possible that trx->state below is set neither within critical
  section protected by trx_sys->mutex nor, with std::memory_order_release.
  That is possible for read-only transactions in code further below.
  This can result in an incorrect message printed to error log inside the
  buf_pool_resize thread about transaction lasting too long. The decision
  was to keep this issue for read-only transactions as it was, because
  providing a fix which would guarantee that state of printed information
  about such transactions is always consistent, would take much more work.
  TODO: check performance gain from this micro-optimization on ARM. */

  if (trx->mysql_thd != nullptr) {
    trx->start_time.store(thd_start_time(trx->mysql_thd),
                          std::memory_order_relaxed);
    if (!trx->ddl_operation) {
      trx->ddl_operation = thd_is_dd_update_stmt(trx->mysql_thd);
    }
  } else {
    trx->start_time.store(std::chrono::system_clock::from_time_t(time(nullptr)),
                          std::memory_order_relaxed);
  }

  /* The initial value for trx->no: TRX_ID_MAX is used in
  read_view_open_now: */

  trx->no = TRX_ID_MAX;

  ut_a(ib_vector_is_empty(trx->lock.autoinc_locks));

  /* This value will only be read by a thread inspecting lock sys queue after
  the thread which enqueues this trx releases the queue's latch. */
  trx->lock.schedule_weight.store(0, std::memory_order_relaxed);

  /* If this transaction came from trx_allocate_for_mysql(),
  trx->in_mysql_trx_list would hold. In that case, the trx->state
  change must be protected by the trx_sys->mutex, so that
  lock_print_info_all_transactions() will have a consistent view. */

  ut_ad(!trx->in_rw_trx_list);

  /* We tend to over assert and that complicates the code somewhat.
  e.g., the transaction state can be set earlier but we are forced to
  set it under the protection of the trx_sys_t::mutex because some
  trx list assertions are triggered unnecessarily. */

  /* By default all transactions are in the read-only list unless they
  are non-locking auto-commit read only transactions or background
  (internal) transactions. Note: Transactions marked explicitly as
  read only can write to temporary tables, we put those on the RO
  list too. */

  if (!trx->read_only &&
      (trx->mysql_thd == nullptr || read_write || trx->ddl_operation)) {
    trx_assign_rseg_durable(trx);

    /* Temporary rseg is assigned only if the transaction
    updates a temporary table */
    DEBUG_SYNC_C("trx_sys_before_assign_id");

    trx_sys_mutex_enter();

    trx->id = trx_sys_allocate_trx_id();

    trx_sys->rw_trx_ids.push_back(trx->id);

    ut_ad(trx->rsegs.m_redo.rseg != nullptr || srv_read_only_mode ||
          srv_force_recovery >= SRV_FORCE_NO_TRX_UNDO);

    trx_add_to_rw_trx_list(trx);

    trx->state.store(TRX_STATE_ACTIVE, std::memory_order_relaxed);

    ut_ad(trx_sys_validate_trx_list());

    trx_sys_mutex_exit();

    trx_sys_rw_trx_add(trx);

  } else {
    trx->id = 0;

    if (!trx_is_autocommit_non_locking(trx)) {
      /* If this is a read-only transaction that is writing
      to a temporary table then it needs a transaction id
      to write to the temporary table. */

      if (read_write) {
        trx_sys_mutex_enter();

        ut_ad(!srv_read_only_mode);

        trx->state.store(TRX_STATE_ACTIVE, std::memory_order_relaxed);

        trx->id = trx_sys_allocate_trx_id();

        trx_sys->rw_trx_ids.push_back(trx->id);

        trx_sys_mutex_exit();

        trx_sys_rw_trx_add(trx);

      } else {
        trx->state.store(TRX_STATE_ACTIVE, std::memory_order_relaxed);
      }
    } else {
      ut_ad(!read_write);
      trx->state.store(TRX_STATE_ACTIVE, std::memory_order_relaxed);
    }
  }

  ut_a(trx->error_state == DB_SUCCESS);

  MONITOR_INC(MONITOR_TRX_ACTIVE);
}

/** Assigns the trx->no and add the transaction to the serialisation_list.
Skips adding to the serialisation_list if the transaction is read-only, in
which case still the trx->no is assigned.
@param[in,out]  trx   the modified transaction
@return true if added to the serialisation_list (non read-only trx) */
static inline bool trx_add_to_serialisation_list(trx_t *trx) {
  trx_sys_serialisation_mutex_enter();

  trx->no = trx_sys_allocate_trx_no();

  /* Update the latest transaction number. */
  ut_d(trx_sys->rw_max_trx_no = trx->no);

  if (trx->read_only) {
    trx_sys_serialisation_mutex_exit();
    return false;
  }

  UT_LIST_ADD_LAST(trx_sys->serialisation_list, trx);

  if (UT_LIST_GET_LEN(trx_sys->serialisation_list) == 1) {
    trx_sys->serialisation_min_trx_no.store(trx->no);
  }

  trx_sys_serialisation_mutex_exit();
  return true;
}

/** Erases transaction from the serialisation_list. Caller must have
acquired trx_sys->serialisation_mutex prior to calling this function.
@param[in,out]  trx   the transaction to erase */
static inline void trx_erase_from_serialisation_list_low(trx_t *trx) {
  ut_ad(trx_sys_serialisation_mutex_own());

  UT_LIST_REMOVE(trx_sys->serialisation_list, trx);

  if (UT_LIST_GET_LEN(trx_sys->serialisation_list) > 0) {
    trx_sys->serialisation_min_trx_no.store(
        UT_LIST_GET_FIRST(trx_sys->serialisation_list)->no);

  } else {
    trx_sys->serialisation_min_trx_no.store(trx_sys_get_next_trx_id_or_no());
  }
}

/** Set the transaction serialisation number.
 @return true if the transaction number was added to the serialisation_list. */
static bool trx_serialisation_number_get(
    trx_t *trx,                         /*!< in/out: transaction */
    trx_undo_ptr_t *redo_rseg_undo_ptr, /*!< in/out: Set trx
                                        serialisation number in
                                        referred undo rseg. */
    trx_undo_ptr_t *temp_rseg_undo_ptr) /*!< in/out: Set trx
                                        serialisation number in
                                        referred undo rseg. */
{
  bool added_trx_no;
  trx_rseg_t *redo_rseg = nullptr;
  trx_rseg_t *temp_rseg = nullptr;

  if (redo_rseg_undo_ptr != nullptr) {
    ut_ad(mutex_own(&redo_rseg_undo_ptr->rseg->mutex));
    redo_rseg = redo_rseg_undo_ptr->rseg;
  }

  if (temp_rseg_undo_ptr != nullptr) {
    ut_ad(mutex_own(&temp_rseg_undo_ptr->rseg->mutex));
    temp_rseg = temp_rseg_undo_ptr->rseg;
  }

  /* If the rollack segment is not empty then the
  new trx_t::no can't be less than any trx_t::no
  already in the rollback segment. User threads only
  produce events when a rollback segment is empty. */
  if ((redo_rseg != nullptr && redo_rseg->last_page_no == FIL_NULL) ||
      (temp_rseg != nullptr && temp_rseg->last_page_no == FIL_NULL)) {
    TrxUndoRsegs elem;

    if (redo_rseg != nullptr && redo_rseg->last_page_no == FIL_NULL) {
      elem.insert(redo_rseg);
    }

    if (temp_rseg != nullptr && temp_rseg->last_page_no == FIL_NULL) {
      elem.insert(temp_rseg);
    }

    mutex_enter(&purge_sys->pq_mutex);

    added_trx_no = trx_add_to_serialisation_list(trx);

    elem.set_trx_no(trx->no);

    purge_sys->purge_queue->push(std::move(elem));

    mutex_exit(&purge_sys->pq_mutex);

  } else {
    added_trx_no = trx_add_to_serialisation_list(trx);
  }

  return (added_trx_no);
}

/** Assign the transaction its history serialisation number and write the
 update UNDO log record to the assigned rollback segment.
 @return true if a serialisation log was written */
static bool trx_write_serialisation_history(
    trx_t *trx, /*!< in/out: transaction */
    mtr_t *mtr) /*!< in/out: mini-transaction */
{
  /* Change the undo log segment states from TRX_UNDO_ACTIVE to some
  other state: these modifications to the file data structure define
  the transaction as committed in the file based domain, at the
  serialization point of the log sequence number lsn obtained below. */

  /* We have to hold the rseg mutex because update log headers have
  to be put to the history list in the (serialisation) order of the
  UNDO trx number. This is required for the purge in-memory data
  structures too. */

  bool own_redo_rseg_mutex = false;
  bool own_temp_rseg_mutex = false;

  /* Get rollback segment mutex. */
  if (trx->rsegs.m_redo.rseg != nullptr && trx_is_redo_rseg_updated(trx)) {
    trx->rsegs.m_redo.rseg->latch();
    own_redo_rseg_mutex = true;
  }

  mtr_t temp_mtr;

  if (trx->rsegs.m_noredo.rseg != nullptr && trx_is_temp_rseg_updated(trx)) {
    trx->rsegs.m_noredo.rseg->latch();
    own_temp_rseg_mutex = true;
    mtr_start(&temp_mtr);
    temp_mtr.set_log_mode(MTR_LOG_NO_REDO);
  }

  /* If transaction involves insert then truncate undo logs. */
  if (trx->rsegs.m_redo.insert_undo != nullptr) {
    trx_undo_set_state_at_finish(trx->rsegs.m_redo.insert_undo, mtr);
  }

  if (trx->rsegs.m_noredo.insert_undo != nullptr) {
    trx_undo_set_state_at_finish(trx->rsegs.m_noredo.insert_undo, &temp_mtr);
  }

  bool serialised = false;

  /* If transaction involves update then add rollback segments
  to purge queue. */
  if (trx->rsegs.m_redo.update_undo != nullptr ||
      trx->rsegs.m_noredo.update_undo != nullptr) {
    /* Assign the transaction serialisation number and add these
    rollback segments to purge trx-no sorted priority queue
    if this is the first UNDO log being written to assigned
    rollback segments. */

    trx_undo_ptr_t *redo_rseg_undo_ptr =
        trx->rsegs.m_redo.update_undo != nullptr ? &trx->rsegs.m_redo : nullptr;

    trx_undo_ptr_t *temp_rseg_undo_ptr =
        trx->rsegs.m_noredo.update_undo != nullptr ? &trx->rsegs.m_noredo
                                                   : nullptr;

    /* Will set trx->no and will add rseg to purge queue. */
    serialised = trx_serialisation_number_get(trx, redo_rseg_undo_ptr,
                                              temp_rseg_undo_ptr);

    /* It is not necessary to obtain trx->undo_mutex here because
    only a single OS thread is allowed to do the transaction commit
    for this transaction. */
    if (trx->rsegs.m_redo.update_undo != nullptr) {
      page_t *undo_hdr_page;

      undo_hdr_page =
          trx_undo_set_state_at_finish(trx->rsegs.m_redo.update_undo, mtr);

      /* Delay update of rseg_history_len if we plan to add
      non-redo update_undo too. This is to avoid immediate
      invocation of purge as we need to club these 2 segments
      with same trx-no as single unit. */
      bool update_rseg_len = !(trx->rsegs.m_noredo.update_undo != nullptr);

      /* Set flag if GTID information need to persist. */
      auto undo_ptr = &trx->rsegs.m_redo;
      trx_undo_gtid_set(trx, undo_ptr->update_undo, false);

      trx_undo_update_cleanup(trx, undo_ptr, undo_hdr_page, update_rseg_len,
                              (update_rseg_len ? 1 : 0), mtr);
    }

    DBUG_EXECUTE_IF("ib_trx_crash_during_commit", DBUG_SUICIDE(););

    if (trx->rsegs.m_noredo.update_undo != nullptr) {
      page_t *undo_hdr_page;

      undo_hdr_page = trx_undo_set_state_at_finish(
          trx->rsegs.m_noredo.update_undo, &temp_mtr);

      ulint n_added_logs = (redo_rseg_undo_ptr != nullptr) ? 2 : 1;

      trx_undo_update_cleanup(trx, &trx->rsegs.m_noredo, undo_hdr_page, true,
                              n_added_logs, &temp_mtr);
    }
  }

  if (own_redo_rseg_mutex) {
    trx->rsegs.m_redo.rseg->unlatch();
    own_redo_rseg_mutex = false;
  }

  if (own_temp_rseg_mutex) {
    trx->rsegs.m_noredo.rseg->unlatch();
    own_temp_rseg_mutex = false;
    mtr_commit(&temp_mtr);
  }

  MONITOR_INC(MONITOR_TRX_COMMIT_UNDO);

  /* Update the latest MySQL binlog name and offset information
  in trx sys header only if MySQL binary logging is on and clone
  is has ensured commit order at final stage. */
  if (Clone_handler::need_commit_order()) {
    trx_sys_update_mysql_binlog_offset(trx, mtr);
  }

  return (serialised);
}

/********************************************************************
Finalize a transaction containing updates for a FTS table. */
static void trx_finalize_for_fts_table(
    fts_trx_table_t *ftt) /* in: FTS trx table */
{
  fts_t *fts = ftt->table->fts;
  fts_doc_ids_t *doc_ids = ftt->added_doc_ids;

  mutex_enter(&fts->bg_threads_mutex);

  if (fts->fts_status & BG_THREAD_STOP) {
    /* The table is about to be dropped, no use
    adding anything to its work queue. */

    mutex_exit(&fts->bg_threads_mutex);
  } else {
    mem_heap_t *heap;
    mutex_exit(&fts->bg_threads_mutex);

    ut_a(fts->add_wq);

    heap = static_cast<mem_heap_t *>(doc_ids->self_heap->arg);

    ib_wqueue_add(fts->add_wq, doc_ids, heap);

    /* fts_trx_table_t no longer owns the list. */
    ftt->added_doc_ids = nullptr;
  }
}

/** Finalize a transaction containing updates to FTS tables. */
static void trx_finalize_for_fts(
    trx_t *trx,     /*!< in/out: transaction */
    bool is_commit) /*!< in: true if the transaction was
                    committed, false if it was rolled back. */
{
  if (is_commit) {
    const ib_rbt_node_t *node;
    ib_rbt_t *tables;
    fts_savepoint_t *savepoint;

    savepoint = static_cast<fts_savepoint_t *>(
        ib_vector_last(trx->fts_trx->savepoints));

    tables = savepoint->tables;

    for (node = rbt_first(tables); node; node = rbt_next(tables, node)) {
      fts_trx_table_t **ftt;

      ftt = rbt_value(fts_trx_table_t *, node);

      if ((*ftt)->added_doc_ids) {
        trx_finalize_for_fts_table(*ftt);
      }
    }
  }

  fts_trx_free(trx->fts_trx);
  trx->fts_trx = nullptr;
}

/** If required, flushes the log to disk based on the value of
 innodb_flush_log_at_trx_commit. */
static void trx_flush_log_if_needed_low(lsn_t lsn) /*!< in: lsn up to which logs
                                                   are to be flushed. */
{
#ifdef _WIN32
  bool flush = true;
#else
  bool flush = srv_unix_file_flush_method != SRV_UNIX_NOSYNC;
#endif /* _WIN32 */

  Wait_stats wait_stats;

  switch (srv_flush_log_at_trx_commit) {
    case 2:
      /* Write the log but do not flush it to disk */
      flush = false;
      [[fallthrough]];
    case 1:
      /* Write the log and optionally flush it to disk */
      wait_stats = log_write_up_to(*log_sys, lsn, flush);

      MONITOR_INC_WAIT_STATS(MONITOR_TRX_ON_LOG_, wait_stats);

      return;
    case 0:
      /* Do nothing */
      return;
  }
}

/** If required, flushes the log to disk based on the value of
 innodb_flush_log_at_trx_commit. */
static void trx_flush_log_if_needed(lsn_t lsn, /*!< in: lsn up to which logs are
                                               to be flushed. */
                                    trx_t *trx) /*!< in/out: transaction */
{
  trx->op_info = "flushing log";

  DEBUG_SYNC_C("trx_flush_log_if_needed");

  if (trx->ddl_operation || trx->ddl_must_flush) {
    auto wait_stats = log_write_up_to(*log_sys, lsn, true);
    MONITOR_INC_WAIT_STATS(MONITOR_TRX_ON_LOG_, wait_stats);
  } else {
    trx_flush_log_if_needed_low(lsn);
  }

  trx->op_info = "";
}

/** For each table that has been modified by the given transaction: update
 its dict_table_t::update_time with the current timestamp. Clear the list
 of the modified tables at the end. */
static void trx_update_mod_tables_timestamp(trx_t *trx) /*!< in: transaction */
{
  ut_ad(trx->id != 0);

  /* consider using trx->start_time if calling time() is too
  expensive here */
  const auto now = std::chrono::system_clock::from_time_t(time(nullptr));

  trx_mod_tables_t::const_iterator end = trx->mod_tables.end();

  for (trx_mod_tables_t::const_iterator it = trx->mod_tables.begin(); it != end;
       ++it) {
    (*it)->update_time = now;
  }

  trx->mod_tables.clear();
}

/**
Erase the transaction from running transaction lists and serialization
list. Active RW transaction list of a MVCC snapshot(ReadView::prepare)
won't include this transaction after this call. All implicit locks are
also released by this call as trx is removed from rw_trx_list.
@param[in]      trx             Transaction to erase, must have an ID > 0 */
static void trx_erase_lists(trx_t *trx) {
  ut_ad(trx->id > 0);
  ut_ad(trx_sys_mutex_own());

  trx_ids_t::iterator it = std::lower_bound(trx_sys->rw_trx_ids.begin(),
                                            trx_sys->rw_trx_ids.end(), trx->id);

  ut_ad(*it == trx->id);
  trx_sys->rw_trx_ids.erase(it);

  if (trx->read_only || trx->rsegs.m_redo.rseg == nullptr) {
    ut_ad(!trx->in_rw_trx_list);
  } else {
    trx_remove_from_rw_trx_list(trx);
    ut_ad(trx_sys_validate_trx_list());

    if (trx->read_view != nullptr) {
      trx_sys->mvcc->view_close(trx->read_view, true);
    }
  }
  DEBUG_SYNC_C("after_trx_erase_lists");
}

static void trx_release_impl_and_expl_locks(trx_t *trx, bool serialised) {
  check_trx_state(trx);
  ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE) ||
        trx_state_eq(trx, TRX_STATE_PREPARED));

  bool trx_sys_latch_is_needed =
      (trx->id > 0) || trx_state_eq(trx, TRX_STATE_PREPARED);

  /* Check and get GTID to be persisted. Do it outside mutex. It must be done
  before trx->state is changed to TRX_STATE_COMMITTED_IN_MEMORY, because the
  gtid_persistor.get_gtid_info() calls gtid_persistor.has_gtid() which checks
  if trx->state is TRX_STATE_PREPARED when thd == nullptr, and updates the thd
  with thd_get_current_thd() in such case. */
  Gtid_desc gtid_desc{};
  if (serialised) {
    auto &gtid_persistor = clone_sys->get_gtid_persistor();
    gtid_persistor.get_gtid_info(trx, gtid_desc);
  }

  if (trx_sys_latch_is_needed) {
    trx_sys_mutex_enter();
  }

  if (trx->id > 0) {
    /* For consistent snapshot, we need to remove current
    transaction from running transaction id list for mvcc
    before doing commit and releasing locks. */
    trx_erase_lists(trx);
  }

  if (trx_state_eq(trx, TRX_STATE_PREPARED)) {
    ut_a(trx_sys->n_prepared_trx > 0);
    --trx_sys->n_prepared_trx;
  }

  if (trx_sys_latch_is_needed) {
    trx_sys_mutex_exit();
  }

  auto state_transition = [&]() {
    trx_mutex_enter(trx);
    /* Please consider this particular point in time as the moment the trx's
    implicit locks become released.
    This change is protected by both Trx_shard's mutex and trx->mutex.
    Therefore, there are two secure ways to check if the trx still can hold
    implicit locks:
    (1) if you only know id of the trx, then you can obtain Trx_shard's mutex
    and check if trx is still in the Trx_shard's active_rw_trxs. This works,
        because the removal from the active_rw_trxs is also protected by the
        same mutex. We use this approach in lock_rec_convert_impl_to_expl() by
        using trx_rw_is_active()
    (2) if you have pointer to trx, and you know it is safe to access (say, you
        hold reference to this trx which prevents it from being freed) then you
        can obtain trx->mutex and check if trx->state is equal to
        TRX_STATE_COMMITTED_IN_MEMORY. We use this approach in
        lock_rec_convert_impl_to_expl_for_trx() when deciding for the final time
        if we really want to create explicit lock on behalf of implicit lock
        holder. */
    trx->state.store(TRX_STATE_COMMITTED_IN_MEMORY, std::memory_order_relaxed);
    trx_mutex_exit(trx);
  };
  if (trx->id > 0) {
    trx_sys->get_shard_by_trx_id(trx->id).active_rw_trxs.latch_and_execute(
        [&](Trx_by_id_with_min &trx_by_id_with_min) {
          state_transition();
          ut_d(const size_t trx_shard_no = trx_get_shard_no(trx->id));
          ut_ad(trx_get_shard_no(trx_by_id_with_min.min_id()) == trx_shard_no);
          trx_by_id_with_min.erase(trx->id);
          ut_ad(trx_get_shard_no(trx_by_id_with_min.min_id()) == trx_shard_no);
        },
        UT_LOCATION_HERE);
  } else {
    state_transition();
  }

  /* It is important to remove the transaction from the serialisation list
  after it is erased from the rw_trx_ids / rw_trx_list (not before!).
  Otherwise a read-view could be created, which could still pretend that
  changes of this transaction are invisible, but related undo records could
  become purged (because trx->no would no longer protect them). */

  if (serialised) {
    trx_sys_serialisation_mutex_enter();

    /* Add GTID to be persisted to disk table. It must be done ...
    1.After the transaction is marked committed in undo. Otherwise
      GTID might get committed before the transaction commit on disk.
    2.Before it is removed from serialization list. Otherwise the transaction
      undo could get purged before persisting GTID on disk table. */
    if (gtid_desc.m_is_set) {
      auto &gtid_persistor = clone_sys->get_gtid_persistor();
      /* The gtid_persistor.add(gtid_desc) might release and re-acquire
      the trx_sys_serialisation_mutex, so must be called before trx is
      removed from the serialisation_list - to satisfy [2]. */
      gtid_persistor.add(gtid_desc);
    }

    trx_erase_from_serialisation_list_low(trx);

    trx_sys_serialisation_mutex_exit();
  }

  lock_trx_release_locks(trx);
}

/** Commits a transaction in memory. */
static void trx_commit_in_memory(
    trx_t *trx,       /*!< in/out: transaction */
    const mtr_t *mtr, /*!< in: mini-transaction of
                      trx_write_serialisation_history(), or NULL if
                      the transaction did not modify anything */
    bool serialised)
/*!< in: true if serialisation log was
written */
{
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  trx->must_flush_log_later = false;
  trx->ddl_must_flush = false;

  if (trx_is_autocommit_non_locking(trx)) {
    ut_ad(trx->id == 0);
    ut_ad(trx->read_only);
    ut_a(!trx->is_recovered);
    ut_ad(trx->rsegs.m_redo.rseg == nullptr);
    ut_ad(!trx->in_rw_trx_list);

    /* Note: We are asserting without holding the locksys latch. But
    that is OK because this transaction is not waiting and cannot
    be rolled back and no new locks can (or should not) be added
    because it is flagged as a non-locking read-only transaction. */

    ut_a(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);

    /* This state change is not protected by any mutex, therefore
    there is an inherent race here around state transition during
    printouts. We ignore this race for the sake of efficiency.
    However, the trx_sys_t::mutex will protect the trx_t instance
    and it cannot be removed from the mysql_trx_list and freed
    without first acquiring the trx_sys_t::mutex. */

    ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));

    if (trx->read_view != nullptr) {
      trx_sys->mvcc->view_close(trx->read_view, false);
    }

    MONITOR_INC(MONITOR_TRX_NL_RO_COMMIT);

    /* AC-NL-RO transactions can't be rolled back asynchronously. */
    ut_ad(!trx->abort);
    ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK));

    trx->state.store(TRX_STATE_NOT_STARTED, std::memory_order_relaxed);

  } else {
    trx_release_impl_and_expl_locks(trx, serialised);

    /* Removed the transaction from the list of active transactions.
    It no longer holds any user locks. */

    ut_ad(trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY));
    DEBUG_SYNC_C("after_trx_committed_in_memory");

    if (trx->read_only || trx->rsegs.m_redo.rseg == nullptr) {
      MONITOR_INC(MONITOR_TRX_RO_COMMIT);
      if (trx->read_view != nullptr) {
        trx_sys->mvcc->view_close(trx->read_view, false);
      }

    } else {
      ut_ad(trx->id > 0);
      MONITOR_INC(MONITOR_TRX_RW_COMMIT);
    }
  }

  /* Reset flag that SE persists GTID. */
  auto &gtid_persistor = clone_sys->get_gtid_persistor();
  gtid_persistor.set_persist_gtid(trx, false);

  if (mtr != nullptr) {
    if (trx->rsegs.m_redo.insert_undo != nullptr) {
      trx_undo_insert_cleanup(&trx->rsegs.m_redo, false);
    }

    if (trx->rsegs.m_noredo.insert_undo != nullptr) {
      trx_undo_insert_cleanup(&trx->rsegs.m_noredo, true);
    }

    /* NOTE that we could possibly make a group commit more
    efficient here: call std::this_thread::yield() here to allow also other
    trxs to come to commit! */

    /*-------------------------------------*/

    /* Depending on the my.cnf options, we may now write the log
    buffer to the log files, making the transaction durable if
    the OS does not crash. We may also flush the log files to
    disk, making the transaction durable also at an OS crash or a
    power outage.

    The idea in InnoDB's group commit is that a group of
    transactions gather behind a trx doing a physical disk write
    to log files, and when that physical write has been completed,
    one of those transactions does a write which commits the whole
    group. Note that this group commit will only bring benefit if
    there are > 2 users in the database. Then at least 2 users can
    gather behind one doing the physical log write to disk.

    If we are calling trx_commit() under prepare_commit_mutex, we
    will delay possible log write and flush to a separate function
    trx_commit_complete_for_mysql(), which is only called when the
    thread has released the mutex. This is to make the
    group commit algorithm to work. Otherwise, the prepare_commit
    mutex would serialize all commits and prevent a group of
    transactions from gathering. */

    lsn_t lsn = mtr->commit_lsn();

    if (lsn == 0) {
      /* Nothing to be done. */
    } else if (trx->flush_log_later) {
      /* Do nothing yet */
      trx->must_flush_log_later = true;

      /* Remember current ddl_operation, because trx_init()
      later will set ddl_operation to false. And the final
      flush is even later. */
      trx->ddl_must_flush = trx->ddl_operation;
    } else if ((srv_flush_log_at_trx_commit == 0 ||
                thd_requested_durability(trx->mysql_thd) ==
                    HA_IGNORE_DURABILITY) &&
               (!trx->ddl_operation)) {
      /* Do nothing */
    } else {
      trx_flush_log_if_needed(lsn, trx);
    }

    trx->commit_lsn = lsn;

    /* Tell server some activity has happened, since the trx
    does changes something. Background utility threads like
    master thread, purge thread or page_cleaner thread might
    have some work to do. */
    srv_active_wake_master_thread();
  }

  /* Do not decrement the reference count before this point.
  There is a potential issue where a thread attempting to drop
  an undo tablespace may end up dropping this undo space
  before this thread can complete the cleanup.
  While marking a undo space as inactive, the server tries
  to find if any transaction is actively using the undo log
  being truncated. A non-zero reference count ensures that the
  thread attempting to truncate/drop the undo tablespace
  cannot be successful as the undo log cannot be dropped until
  is it empty. */
  if (trx->rsegs.m_redo.rseg != nullptr) {
    trx_rseg_t *rseg = trx->rsegs.m_redo.rseg;
    ut_ad(rseg->trx_ref_count > 0);

    /* Multiple transactions can simultaneously decrement
    the atomic counter. */
    rseg->trx_ref_count--;

    trx->rsegs.m_redo.rseg = nullptr;
  }

  /* Free all savepoints, starting from the first. */
  trx_named_savept_t *savep = UT_LIST_GET_FIRST(trx->trx_savepoints);

  trx_roll_savepoints_free(trx, savep);

  if (trx->fts_trx != nullptr) {
    trx_finalize_for_fts(trx, trx->undo_no != 0);
  }

  trx_mutex_enter(trx);
  trx->dict_operation = TRX_DICT_OP_NONE;

  /* Because we can rollback transactions asynchronously, we change
  the state at the last step. trx_t::abort cannot change once commit
  or rollback has started because we will have released the locks by
  the time we get here. */

  if (trx->abort) {
    trx->abort = false;
    trx->state.store(TRX_STATE_FORCED_ROLLBACK, std::memory_order_relaxed);
  } else {
    trx->state.store(TRX_STATE_NOT_STARTED, std::memory_order_relaxed);
  }

  /* trx->in_mysql_trx_list would hold between
  trx_allocate_for_mysql() and trx_free_for_mysql(). It does not
  hold for recovered transactions or system transactions. */
  assert_trx_is_free(trx);

  trx_init(trx);

  trx_mutex_exit(trx);

  ut_a(trx->error_state == DB_SUCCESS);
}

/** Commits a transaction and a mini-transaction.
@param[in,out] trx Transaction
@param[in,out] mtr Mini-transaction (will be committed), or null if trx made no
modifications */
void trx_commit_low(trx_t *trx, mtr_t *mtr) {
  assert_trx_nonlocking_or_in_list(trx);
  ut_ad(!trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY));
  ut_ad(!mtr || mtr->is_active());
  /* undo_no is non-zero if we're doing the final commit. */
  if (trx->fts_trx != nullptr && trx->undo_no != 0 &&
      trx->lock.que_state != TRX_QUE_ROLLING_BACK) {
    dberr_t error;

    ut_a(!trx_is_autocommit_non_locking(trx));

    error = fts_commit(trx);

    /* FTS-FIXME: Temporarily tolerate DB_DUPLICATE_KEY
    instead of dying. This is a possible scenario if there
    is a crash between insert to DELETED table committing
    and transaction committing. The fix would be able to
    return error from this function */
    if (error != DB_SUCCESS && error != DB_DUPLICATE_KEY) {
      /* FTS-FIXME: once we can return values from this
      function, we should do so and signal an error
      instead of just dying. */

      ut_error;
    }
  }

  bool serialised;

  if (mtr != nullptr) {
    mtr->set_sync();

    DEBUG_SYNC_C("trx_sys_before_assign_no");

    serialised = trx_write_serialisation_history(trx, mtr);

    /* The following call commits the mini-transaction, making the
    whole transaction committed in the file-based world, at this
    log sequence number. The transaction becomes 'durable' when
    we write the log to disk, but in the logical sense the commit
    in the file-based data structures (undo logs etc.) happens
    here.

    NOTE that transaction numbers, which are assigned only to
    transactions with an update undo log, do not necessarily come
    in exactly the same order as commit lsn's, if the transactions
    have different rollback segments. To get exactly the same
    order we should hold the kernel mutex up to this point,
    adding to the contention of the kernel mutex. However, if
    a transaction T2 is able to see modifications made by
    a transaction T1, T2 will always get a bigger transaction
    number and a bigger commit lsn than T1. */

    /*--------------*/

    DBUG_EXECUTE_IF("trx_commit_to_the_end_of_log_block", {
      const size_t space_left = mtr->get_expected_log_size();
      mtr_commit_mlog_test_filling_block(*log_sys, space_left);
    });

    mtr_commit(mtr);

    DBUG_PRINT("trx_commit", ("commit lsn at " LSN_PF, mtr->commit_lsn()));

    DBUG_EXECUTE_IF(
        "ib_crash_during_trx_commit_in_mem", if (trx_is_rseg_updated(trx)) {
          log_make_latest_checkpoint();
          DBUG_SUICIDE();
        });
    /*--------------*/

  } else {
    serialised = false;
  }
#ifdef UNIV_DEBUG
  /* In case of this function is called from a stack executing
     THD::release_resources -> ...
        innobase_connection_close() ->
               trx_rollback_for_mysql... -> .
     mysql's thd does not seem to have
     thd->debug_sync_control defined any longer. However the stack
     is possible only with a prepared trx not updating any data.
  */
  if (trx->mysql_thd != nullptr && trx_is_redo_rseg_updated(trx)) {
    DEBUG_SYNC_C("before_trx_state_committed_in_memory");
  }
#endif

  trx_commit_in_memory(trx, mtr, serialised);
}

/** Commits a transaction. */
void trx_commit(trx_t *trx) /*!< in/out: transaction */
{
  mtr_t *mtr;
  mtr_t local_mtr;

  DBUG_EXECUTE_IF("ib_trx_commit_crash_before_trx_commit_start",
                  DBUG_SUICIDE(););

  if (trx_is_rseg_updated(trx)) {
    mtr = &local_mtr;

    DBUG_EXECUTE_IF("ib_trx_commit_crash_rseg_updated", DBUG_SUICIDE(););

    mtr_start_sync(mtr);

  } else {
    mtr = nullptr;
  }

  trx_commit_low(trx, mtr);
}

/** Cleans up a transaction at database startup. The cleanup is needed if
 the transaction already got to the middle of a commit when the database
 crashed, and we cannot roll it back. */
void trx_cleanup_at_db_startup(trx_t *trx) /*!< in: transaction */
{
  ut_ad(trx->is_recovered);

  /* Cleanup any durable undo logs in non-temporary rollback segments.
  At database start-up there are no active transactions recorded in
  any rollback segments in the temporary tablespace because all those
  changes are all lost on restart. */
  if (trx->rsegs.m_redo.insert_undo != nullptr) {
    trx_undo_insert_cleanup(&trx->rsegs.m_redo, false);
  }

  memset(&trx->rsegs, 0x0, sizeof(trx->rsegs));
  trx->undo_no = 0;
  trx->undo_rseg_space = 0;
  trx->last_sql_stat_start.least_undo_no = 0;

  trx_sys_mutex_enter();

  ut_a(!trx->read_only);
  trx_remove_from_rw_trx_list(trx);

  trx_sys_mutex_exit();

  /* Change the transaction state without mutex protection, now
  that it no longer is in the trx_list. Recovered transactions
  are never placed in the mysql_trx_list. */
  ut_ad(trx->is_recovered);
  ut_ad(!trx->in_rw_trx_list);
  ut_ad(!trx->in_mysql_trx_list);
  trx->state.store(TRX_STATE_NOT_STARTED, std::memory_order_relaxed);
}

/** Assigns a read view for a consistent read query. All the consistent reads
 within the same transaction will get the same read view, which is created
 when this function is first called for a new started transaction.
 @return consistent read view */
ReadView *trx_assign_read_view(trx_t *trx) /*!< in/out: active transaction */
{
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));
  ut_ad(trx->state.load(std::memory_order_relaxed) == TRX_STATE_ACTIVE);

  if (srv_read_only_mode) {
    ut_ad(trx->read_view == nullptr);
    return (nullptr);

  } else if (!MVCC::is_view_active(trx->read_view)) {
    trx_sys->mvcc->view_open(trx->read_view, trx);
  }

  return (trx->read_view);
}

/** Prepares a transaction for commit/rollback. */
void trx_commit_or_rollback_prepare(trx_t *trx) /*!< in/out: transaction */
{
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

      trx_start_low(trx, true);
      [[fallthrough]];

    case TRX_STATE_ACTIVE:
    case TRX_STATE_PREPARED:

      /* If the trx is in a lock wait state, moves the waiting
      query thread to the suspended state */

      if (trx->lock.que_state == TRX_QUE_LOCK_WAIT) {
        ut_a(trx->lock.wait_thr != nullptr);
        trx->lock.wait_thr->state = QUE_THR_SUSPENDED;
        trx->lock.wait_thr = nullptr;

        trx->lock.que_state = TRX_QUE_RUNNING;
      }

      ut_a(trx->lock.n_active_thrs == 1);
      return;

    case TRX_STATE_COMMITTED_IN_MEMORY:
      break;
  }

  ut_error;
}

/** Creates a commit command node struct.
 @return own: commit node struct */
commit_node_t *trx_commit_node_create(
    mem_heap_t *heap) /*!< in: mem heap where created */
{
  commit_node_t *node;

  node = static_cast<commit_node_t *>(mem_heap_alloc(heap, sizeof(*node)));
  node->common.type = QUE_NODE_COMMIT;
  node->state = COMMIT_NODE_SEND;

  return (node);
}

/** Performs an execution step for a commit type node in a query graph.
 @return query thread to run next, or NULL */
que_thr_t *trx_commit_step(que_thr_t *thr) /*!< in: query thread */
{
  commit_node_t *node;

  node = static_cast<commit_node_t *>(thr->run_node);

  ut_ad(que_node_get_type(node) == QUE_NODE_COMMIT);

  if (thr->prev_node == que_node_get_parent(node)) {
    node->state = COMMIT_NODE_SEND;
  }

  if (node->state == COMMIT_NODE_SEND) {
    trx_t *trx;

    node->state = COMMIT_NODE_WAIT;

    trx = thr_get_trx(thr);

    ut_a(trx->lock.wait_thr == nullptr);
    ut_a(trx->lock.que_state != TRX_QUE_LOCK_WAIT);

    trx_commit_or_rollback_prepare(trx);

    trx->lock.que_state = TRX_QUE_COMMITTING;

    trx_commit(trx);

    ut_ad(trx->lock.wait_thr == nullptr);

    trx->lock.que_state = TRX_QUE_RUNNING;

    thr = nullptr;
  } else {
    ut_ad(node->state == COMMIT_NODE_WAIT);

    node->state = COMMIT_NODE_SEND;

    thr->run_node = que_node_get_parent(node);
  }

  return (thr);
}

/** Does the transaction commit for MySQL.
 @return DB_SUCCESS or error number */
dberr_t trx_commit_for_mysql(trx_t *trx) /*!< in/out: transaction */
{
  DEBUG_SYNC_C("trx_commit_for_mysql_checks_for_aborted");
  TrxInInnoDB trx_in_innodb(trx, true);

  if (trx_in_innodb.is_aborted() &&
      trx->killed_by != std::this_thread::get_id()) {
    return (DB_FORCED_ABORT);
  }

  /* Because we do not do the commit by sending an Innobase
  sig to the transaction, we must here make sure that trx has been
  started. */

  dberr_t db_err = DB_SUCCESS;

  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  switch (trx->state.load(std::memory_order_relaxed)) {
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:

      ut_d(trx->start_file = __FILE__);
      ut_d(trx->start_line = __LINE__);

      trx_start_low(trx, true);
      [[fallthrough]];
    case TRX_STATE_ACTIVE:
    case TRX_STATE_PREPARED:
      trx->op_info = "committing";

      /* For GTID persistence we need update undo segment. */
      db_err = trx_undo_gtid_add_update_undo(trx, false, false);
      if (db_err != DB_SUCCESS) {
        return (db_err);
      }

      if (trx->id != 0) {
        trx_update_mod_tables_timestamp(trx);
      }

      trx_commit(trx);

      MONITOR_DEC(MONITOR_TRX_ACTIVE);
      trx->op_info = "";
      return (DB_SUCCESS);
    case TRX_STATE_COMMITTED_IN_MEMORY:
      break;
  }
  ut_error;
  return (DB_CORRUPTION);
}

/** If required, flushes the log to disk if we called trx_commit_for_mysql()
 with trx->flush_log_later == true. */
void trx_commit_complete_for_mysql(trx_t *trx) /*!< in/out: transaction */
{
  if (trx->id != 0 || !trx->must_flush_log_later ||
      (thd_requested_durability(trx->mysql_thd) == HA_IGNORE_DURABILITY &&
       !trx->ddl_must_flush)) {
    /* If we removed trx->ddl_must_flush from condition above, we would
    need to take care of fixing innobase_flush_logs for a scenario in
    which srv_flush_log_at_trx_commit == 0. */
    return;
  }

  trx_flush_log_if_needed(trx->commit_lsn, trx);

  trx->must_flush_log_later = false;
  trx->ddl_must_flush = false;
}

/** Marks the latest SQL statement ended. */
void trx_mark_sql_stat_end(trx_t *trx) /*!< in: trx handle */
{
  ut_a(trx);

  lock_on_statement_end(trx);

  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  switch (trx->state.load(std::memory_order_relaxed)) {
    case TRX_STATE_PREPARED:
    case TRX_STATE_COMMITTED_IN_MEMORY:
      break;
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:
      trx->undo_no = 0;
      trx->undo_rseg_space = 0;
      [[fallthrough]];
    case TRX_STATE_ACTIVE:
      trx->last_sql_stat_start.least_undo_no = trx->undo_no;

      if (trx->fts_trx != nullptr) {
        fts_savepoint_laststmt_refresh(trx);
      }

      return;
  }

  ut_error;
}

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
                   ulint heap_size)
/*!< in: mem_heap_get_size(trx->lock.lock_heap) */
{
  bool newline;
  const char *op_info;

  ut_ad(trx_sys_mutex_own());

  fprintf(f, "TRANSACTION " TRX_ID_FMT, trx_get_id_for_print(trx));

  const auto trx_state = trx->state.load(std::memory_order_relaxed);

  switch (trx_state) {
    case TRX_STATE_NOT_STARTED:
      fputs(", not started", f);
      break;
    case TRX_STATE_FORCED_ROLLBACK:
      fputs(", forced rollback", f);
      break;
    case TRX_STATE_ACTIVE:
      fprintf(f, ", ACTIVE %lu sec",
              (ulong)std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now() -
                  trx->start_time.load(std::memory_order_relaxed))
                  .count());
      break;
    case TRX_STATE_PREPARED:
      fprintf(f, ", ACTIVE (PREPARED) %lu sec",
              (ulong)std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now() -
                  trx->start_time.load(std::memory_order_relaxed))
                  .count());
      break;
    case TRX_STATE_COMMITTED_IN_MEMORY:
      fputs(", COMMITTED IN MEMORY", f);
      break;
    default:
      fprintf(f, ", state %lu", static_cast<ulong>(trx_state));
      ut_d(ut_error);
      ut_o(break);
  }

  /* prevent a race condition */
  op_info = trx->op_info;

  if (*op_info) {
    putc(' ', f);
    fputs(op_info, f);
  }

  if (trx->is_recovered) {
    fputs(" recovered trx", f);
  }

  if (trx->declared_to_be_inside_innodb) {
    fprintf(f, ", thread declared inside InnoDB %lu",
            (ulong)trx->n_tickets_to_enter_innodb);
  }

  putc('\n', f);

  if (trx->n_mysql_tables_in_use > 0 || trx->mysql_n_tables_locked > 0) {
    fprintf(f, "mysql tables in use %lu, locked %lu\n",
            (ulong)trx->n_mysql_tables_in_use,
            (ulong)trx->mysql_n_tables_locked);
  }

  newline = true;

  /* trx->lock.que_state of an ACTIVE transaction may change
  while we are not holding trx->mutex. We perform a dirty read
  for performance reasons. */

  switch (trx->lock.que_state) {
    case TRX_QUE_RUNNING:
      newline = false;
      break;
    case TRX_QUE_LOCK_WAIT:
      fputs("LOCK WAIT ", f);
      break;
    case TRX_QUE_ROLLING_BACK:
      fputs("ROLLING BACK ", f);
      break;
    case TRX_QUE_COMMITTING:
      fputs("COMMITTING ", f);
      break;
    default:
      fprintf(f, "que state %lu ", (ulong)trx->lock.que_state);
  }

  if (n_trx_locks > 0 || heap_size > 400) {
    newline = true;

    fprintf(f,
            "%lu lock struct(s), heap size %lu,"
            " %lu row lock(s)",
            (ulong)n_trx_locks, (ulong)heap_size, (ulong)n_rec_locks);
  }

  if (trx->has_search_latch) {
    newline = true;
    fputs(", holds adaptive hash latch", f);
  }

  if (trx->undo_no != 0) {
    newline = true;
    fprintf(f, ", undo log entries " TRX_ID_FMT, trx->undo_no);
  }

  if (newline) {
    putc('\n', f);
  }

  if (trx_state != TRX_STATE_NOT_STARTED && trx->mysql_thd != nullptr) {
    innobase_mysql_print_thd(f, trx->mysql_thd,
                             static_cast<uint>(max_query_len));
  }
}

void trx_print_latched(FILE *f, const trx_t *trx, ulint max_query_len) {
  /* We need exclusive access to lock_sys for lock_number_of_rows_locked(),
  and accessing trx->lock fields without trx->mutex.*/
  ut_ad(locksys::owns_exclusive_global_latch());
  ut_ad(trx_sys_mutex_own());

  trx_print_low(f, trx, max_query_len, lock_number_of_rows_locked(&trx->lock),
                UT_LIST_GET_LEN(trx->lock.trx_locks),
                mem_heap_get_size(trx->lock.lock_heap));
}

void trx_print(FILE *f, const trx_t *trx, ulint max_query_len) {
  /* trx_print_latched() requires exclusive global latch */
  locksys::Global_exclusive_latch_guard guard{UT_LOCATION_HERE};
  mutex_enter(&trx_sys->mutex);
  trx_print_latched(f, trx, max_query_len);
  mutex_exit(&trx_sys->mutex);
}

#ifdef UNIV_DEBUG
bool trx_can_be_handled_by_current_thread(const trx_t *trx) {
  return trx->mysql_thd == nullptr || trx->mysql_thd == current_thd ||
         /* THD::restore_globals() set current_thd to nullptr and it is called
         in Table_access::~Table_access() before THD::release_resources().
         On the other hand THD::release_resources() calls ha_close_connection(),
         which needs to enter InnoDB (then assertion based on this function
         wants to validate that the thread has permission to enter). Until
         Table_access::~Table_access() is fixed, we disable this assertion
         when current_thd == nullptr. */
         current_thd == nullptr;
}

bool trx_can_be_handled_by_current_thread_or_is_hp_victim(const trx_t *trx) {
  return trx_can_be_handled_by_current_thread(trx) ||
         trx->killed_by.load() == std::this_thread::get_id();
}

/** Asserts that a transaction has been started.
 The caller must hold trx_sys->mutex.
 @return true if started */
bool trx_assert_started(const trx_t *trx) /*!< in: transaction */
{
  ut_ad(trx_sys_mutex_own());

  /* Non-locking autocommits should not hold any locks and this
  function is only called from the locking code. */
  check_trx_state(trx);

  /* trx->state can change from or to NOT_STARTED while we are holding
  trx_sys->mutex for non-locking autocommit selects but not for other
  types of transactions. It may change from ACTIVE to PREPARED. */

  switch (trx->state.load(std::memory_order_relaxed)) {
    case TRX_STATE_PREPARED:
      return true;

    case TRX_STATE_ACTIVE:
    case TRX_STATE_COMMITTED_IN_MEMORY:
      return true;

    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:
      break;
  }

  ut_error;
}

/*
Interaction between Lock-sys and trx->mutex-es is rather complicated.
In particular we allow a thread performing Lock-sys operations to request
another trx->mutex even though it already holds one for a different trx.
Therefore one has to prove that it is impossible to form a deadlock cycle in the
imaginary wait-for-graph in which edges go from thread trying to obtain
trx->mutex to a thread which holds it at the moment.

In the past it was simple, because Lock-sys was protected by a global mutex,
which meant that there was at most one thread which could try to posses more
than one trx->mutex - one can not form a cycle in a graph in which only
one node has both incoming and outgoing edges.

Today it is much harder to prove, because we have sharded the Lock-sys mutex,
and now multiple threads can perform Lock-sys operations in parallel, as long
as they happen in different shards.

Here's my attempt at the proof.

Assumption 1.
  If a thread attempts to acquire more then one trx->mutex, then it either has
  exclusive global latch, or it attempts to acquire exactly two of them, and at
  just before calling mutex_enter for the second time it saw
  1.1 trx1->lock.wait_lock==nullptr or it held the latch for the shard
      containing the trx1->lock.wait_lock
  AND
  1.2. trx2->lock.wait_lock!=nullptr, and it held the latch for the shard
       containing trx2->lock.wait_lock.

@see asserts in trx_before_mutex_enter

Assumption 2.
  The Lock-sys latches are taken before any trx->mutex.

@see asserts in sync0debug.cc

Assumption 3.
  Changing trx->lock.wait_lock from NULL to non-NULL requires latching
  trx->mutex and the shard containing new wait_lock value.

@see asserts in lock_set_lock_and_trx_wait()

Assumption 4.
  Changing trx->lock.wait_lock from non-NULL to NULL requires latching the shard
  containing old wait_lock value.

@see asserts in lock_reset_lock_and_trx_wait()

Assumption 5.
  If a thread is latching two Lock-sys shards then it's acquiring and releasing
  both shards together (that is, without interleaving it with trx->mutex
  operations).

@see Shard_latches_guard

Theorem 1.
  If the Assumptions 1-5 hold, then it's impossible for trx_mutex_enter() call
  to deadlock.

By proving the theorem, and observing that the assertions hold for multiple runs
of test suite on debug build, we gain more and more confidence that
trx_mutex_enter() calls can not deadlock.

The intuitive, albeit imprecise, version of the proof is that by Assumption 1,
for each i, the edge of the deadlock cycle caused by thread[i] - which has
already latched transaction start[i] and waits to latch transaction end[i] -
must have non-NULL end[i]->lock.wait_lock in a shard latched by thread[i], and
either start[i]->lock.wait_lock == NULL, or the shard containing this lock is
latched by thread[i], in either case no other j!=i can have end[j]=start[i], as
that would mean start[i]->lock.wait_lock is non-NULL and in shard latched by
thread[j] instead.

The difficulty lays in that wait_lock is a field which can be modified over time
from several threads, so care must be taken to clarify at which moment in time
we make our observations and from whose perspective.

We will now formally prove Theorem 1.
Assume otherwise, that is that we are in a thread which have just started a call
to mutex_enter(trx_a->mutex) and caused a deadlock.

Fact 0. There is no thread which possesses exclusive Lock-sys latch, since to
        form a deadlock one needs at least two threads inside Lock-sys
Fact 1. Each thread participating in the deadlock holds one trx mutex and waits
        for the second one it tried to acquire
Fact 2. Thus each thread participating in the deadlock had gone through "else"
        branch inside trx_before_mutex_enter(), so it verifies Assumption 1.
Fact 3. Our thread owns_lock_shard(trx_a->lock.wait_lock)
Fact 4. Another thread has latched trx_a->mutex as the first of its two latches

Consider the situation from the point of view of this other thread, which is now
in the deadlock waiting for mutex_enter(trx_b->mutex) for some trx_b!=trx_a.
By Fact 2 and assumption 1, it had to take the "else" branch on the way there,
and thus at some moment in time it has saw either
a) trx_a->lock.wait_lock == nullptr or
b) it held the latch of the shard containing trx_a->lock.wait_lock.
This observation was either before or after our observation that
trx_a->lock.wait_lock != nullptr and that we hold the latch on this shard
(again Fact 2 and Assumption 1).

Let us first rule out case b). As both threads are presumably in a deadlock,
they are still holding the latches on the lock-sys shards that they had latched,
so in case b) both threads hold a latch on a shard which contained the current
trx_a->lock.wait_lock value, which implies they must have latched two different
shards, which implies they saw two different values of wait_lock field, which in
turn means that it has changed, but by Assumption 4 it can not change while
somebody holds a latch on its current value's shard. Thus b) is impossible!

This leaves us with case a).

If our thread observed non-NULL value first, then it means a change from
non-NULL to NULL has happened, which by Assumption 4 requires a shard latch,
which only our thread posses - and we couldn't manipulate the wait_lock as we
are in a deadlock.

If the other thread observed NULL first, then it means that the value has
changed to non-NULL, which requires trx_a->mutex according to Assumption 3, yet
this mutex was held entire time by the other thread, since it observed the NULL
just before it deadlock, so it could not change it, either.

So, there is no way the value of wait_lock has changed from NULL to non-NULL or
vice-versa, yet one thread sees NULL and the other non-NULL - contradiction ends
the proof.
*/

static thread_local const trx_t *trx_first_latched_trx = nullptr;
static thread_local int32_t trx_latched_count = 0;
static thread_local bool trx_allowed_two_latches = false;

void trx_before_mutex_enter(const trx_t *trx, bool first_of_two) {
  if (0 == trx_latched_count++) {
    ut_a(trx_first_latched_trx == nullptr);
    trx_first_latched_trx = trx;
    if (first_of_two) {
      trx_allowed_two_latches = true;
    }
  } else {
    ut_a(!first_of_two);
    if (!locksys::owns_exclusive_global_latch()) {
      ut_a(trx_allowed_two_latches);
      ut_a(trx_latched_count == 2);
      /* In theory wait_lock can change from non-null to null at any moment
      unless we indeed hold the wait_lock's shard. Thankfully, this is exactly
      what we assert. */
      ut_a(trx_first_latched_trx->lock.wait_lock == nullptr ||
           locksys::owns_lock_shard(trx_first_latched_trx->lock.wait_lock));
      ut_a(trx_first_latched_trx != trx);
      /* This is not very safe, because to read trx->lock.wait_lock we
      should already either latch trx->mutex (which we don't) or shard with
      trx->lock.wait_lock. But our claim is precisely that we have latched
      this shard, and we want to check that here. */
      ut_a(trx->lock.wait_lock != nullptr);
      ut_a(locksys::owns_lock_shard(trx->lock.wait_lock));
    }
  }
}
void trx_before_mutex_exit(const trx_t *trx) {
  ut_a(0 < trx_latched_count);
  if (0 == --trx_latched_count) {
    ut_a(trx_first_latched_trx == trx);
    trx_first_latched_trx = nullptr;
    trx_allowed_two_latches = false;
  }
}
#endif /* UNIV_DEBUG */

/** Compares the "weight" (or size) of two transactions. Transactions that
 have edited non-transactional tables are considered heavier than ones
 that have not.
 @return true if weight(a) >= weight(b) */
bool trx_weight_ge(const trx_t *a, /*!< in: transaction to be compared */
                   const trx_t *b) /*!< in: transaction to be compared */
{
  /* To read TRX_WEIGHT we need a exclusive global lock_sys latch */
  ut_ad(locksys::owns_exclusive_global_latch());

  /* If mysql_thd is NULL for a transaction we assume that it has
  not edited non-transactional tables. */

  auto a_notrans_edit =
      a->mysql_thd != nullptr && thd_has_edited_nontrans_tables(a->mysql_thd);

  auto b_notrans_edit =
      b->mysql_thd != nullptr && thd_has_edited_nontrans_tables(b->mysql_thd);

  if (a_notrans_edit != b_notrans_edit) {
    return (a_notrans_edit);
  }

  /* Either both had edited non-transactional tables or both had
  not, we fall back to comparing the number of altered/locked
  rows. */

  return (TRX_WEIGHT(a) >= TRX_WEIGHT(b));
}

/** Prepares a transaction for given rollback segment.
 @return lsn_t: lsn assigned for commit of scheduled rollback segment */
static lsn_t trx_prepare_low(
    trx_t *trx,               /*!< in/out: transaction */
    trx_undo_ptr_t *undo_ptr, /*!< in/out: pointer to rollback
                              segment scheduled for prepare. */
    bool noredo_logging)      /*!< in: turn-off redo logging. */
{
  if (undo_ptr->insert_undo != nullptr || undo_ptr->update_undo != nullptr) {
    mtr_t mtr;
    trx_rseg_t *rseg = undo_ptr->rseg;

    mtr_start_sync(&mtr);

    if (noredo_logging) {
      mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
    }

    /* Change the undo log segment states from TRX_UNDO_ACTIVE to
    TRX_UNDO_PREPARED: these modifications to the file data
    structure define the transaction as prepared in the file-based
    world, at the serialization point of lsn. */

    rseg->latch();

    if (undo_ptr->insert_undo != nullptr) {
      /* It is not necessary to obtain trx->undo_mutex here
      because only a single OS thread is allowed to do the
      transaction prepare for this transaction. */
      trx_undo_set_state_at_prepare(trx, undo_ptr->insert_undo, false, &mtr);
    }

    if (undo_ptr->update_undo != nullptr) {
      if (!noredo_logging) {
        trx_undo_gtid_set(trx, undo_ptr->update_undo, true);
      }
      trx_undo_set_state_at_prepare(trx, undo_ptr->update_undo, false, &mtr);
    }

    rseg->unlatch();

    /*--------------*/
    /* This mtr commit makes the transaction prepared in
    file-based world. */
    mtr_commit(&mtr);
    /*--------------*/

    if (!noredo_logging) {
      const lsn_t lsn = mtr.commit_lsn();
      ut_ad(lsn > 0 || !mtr_t::s_logging.is_enabled());
      return lsn;
    }
  }

  return 0;
}

bool trx_is_mysql_xa(const trx_t *trx) {
  auto my_xid = trx->xid->get_my_xid();
  return (my_xid != 0);
}

/** Prepares a transaction.
@param[in]     trx the transction to prepare. */
static void trx_prepare(trx_t *trx) {
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  /* This transaction has crossed the point of no return and cannot
  be rolled back asynchronously now. It must commit or rollback
  synchronously. */

  lsn_t lsn = 0;

  /* Only fresh user transactions can be prepared.
  Recovered transactions cannot. */
  ut_a(!trx->is_recovered);

  DBUG_EXECUTE_IF("ib_trx_crash_during_xa_prepare_step", DBUG_SUICIDE(););

  if (trx->rsegs.m_redo.rseg != nullptr && trx_is_redo_rseg_updated(trx)) {
    lsn = trx_prepare_low(trx, &trx->rsegs.m_redo, false);
  }

  if (trx->rsegs.m_noredo.rseg != nullptr && trx_is_temp_rseg_updated(trx)) {
    trx_prepare_low(trx, &trx->rsegs.m_noredo, true);
  }

  ut_a(trx->state.load(std::memory_order_relaxed) == TRX_STATE_ACTIVE);

  trx_sys_mutex_enter();
  trx->state.store(TRX_STATE_PREPARED, std::memory_order_relaxed);
  trx_sys->n_prepared_trx++;
  trx_sys_mutex_exit();

  /* Force isolation level to RC and release GAP locks
  for test purpose. */
  DBUG_EXECUTE_IF("ib_force_release_gap_lock_prepare",
                  trx->isolation_level = TRX_ISO_READ_COMMITTED;);

  /* Release read locks after PREPARE for READ COMMITTED
  and lower isolation. */
  if (trx->releases_gap_locks_at_prepare()) {
    /* Stop inheriting GAP locks. */
    trx->skip_lock_inheritance = true;

    /* Release only GAP locks for now. */
    lock_trx_release_read_locks(trx, true);
  }

  if (lsn > 0) {
    trx_flush_logs(trx, lsn);
  }
}

/** Sets the transaction as prepared in the transaction coordinator for
the given rollback segment.
@param[in,out] trx      The rollback segment parent transaction.
@param[in]     undo_ptr The rollback segment.
@return lsn assigned for commit of scheduled rollback segment */
static lsn_t trx_set_prepared_in_tc_low(trx_t *trx, trx_undo_ptr_t *undo_ptr) {
  if (undo_ptr->insert_undo != nullptr || undo_ptr->update_undo != nullptr) {
    mtr_t mtr;
    trx_rseg_t *rseg = undo_ptr->rseg;

    mtr_start_sync(&mtr);

    /* Change the undo log segment states from TRX_UNDO_ACTIVE to
    TRX_UNDO_PREPARED: these modifications to the file data
    structure define the transaction as prepared in the file-based
    world, at the serialization point of lsn. */

    rseg->latch();

    if (undo_ptr->insert_undo != nullptr) {
      /* It is not necessary to obtain trx->undo_mutex here
      because only a single OS thread is allowed to do the
      transaction prepare for this transaction. */
      trx_undo_set_prepared_in_tc(trx, undo_ptr->insert_undo, &mtr);
    }

    if (undo_ptr->update_undo != nullptr) {
      trx_undo_gtid_set(trx, undo_ptr->update_undo, true);
      trx_undo_set_prepared_in_tc(trx, undo_ptr->update_undo, &mtr);
    }

    rseg->unlatch();

    /*--------------*/
    /* This mtr commit makes the transaction prepared in
    file-based world. */
    mtr_commit(&mtr);
    /*--------------*/

    const lsn_t lsn = mtr.commit_lsn();
    ut_ad(lsn > 0 || !mtr_t::s_logging.is_enabled());
    return lsn;
  }

  return 0;
}

/** Marks a transaction as prepared in the transaction coordinator.
@param[in]     trx the transction to mark. */
static void trx_set_prepared_in_tc(trx_t *trx) {
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));
  ut_a(trx->state.load(std::memory_order_relaxed) == TRX_STATE_PREPARED);

  lsn_t lsn = trx_set_prepared_in_tc_low(trx, &trx->rsegs.m_redo);

  /* Check and get GTID to be persisted. Do it outside trx_sys mutex. */
  auto &gtid_persistor = clone_sys->get_gtid_persistor();
  Gtid_desc gtid_desc;
  gtid_persistor.get_gtid_info(trx, gtid_desc);

  /* Add GTID to be persisted to disk table, if needed. */
  if (gtid_desc.m_is_set) {
    /* The gtid_persistor.add() might release and re-acquire the mutex. */
    trx_sys_serialisation_mutex_enter();
    gtid_persistor.add(gtid_desc);
    trx_sys_serialisation_mutex_exit();
  }

  /* Reset after successfully adding GTID to in memory table. */
  trx->persists_gtid = false;

  if (lsn > 0) {
    trx_flush_logs(trx, lsn);
  }
}

/**
Does the transaction prepare for MySQL.
@param[in, out] trx             Transaction instance to prepare */
dberr_t trx_prepare_for_mysql(trx_t *trx) {
  trx_start_if_not_started_xa(trx, false, UT_LOCATION_HERE);

  TrxInInnoDB trx_in_innodb(trx, true);

  if (trx_in_innodb.is_aborted() &&
      trx->killed_by != std::this_thread::get_id()) {
    return (DB_FORCED_ABORT);
  }

  trx->op_info = "preparing";

  trx_prepare(trx);

  trx->op_info = "";

  return (DB_SUCCESS);
}

/**
  Get the table name and database name for the given dd_table object.

  @param[in,out]  table Handler table name object pointer.
  @param[in]      dd_table  Pointer table name DD object.
  @param[in]      mem_root  Mem_root for space allocation.

  @retval     true   Error, e.g. Memory allocation failure.
  @retval     false  Success
*/

static bool get_table_name_info(st_handler_tablename *table,
                                const dict_table_t *dd_table,
                                MEM_ROOT *mem_root) {
  std::string db_str;
  std::string table_str;
  dict_name::get_table(dd_table->name.m_name, db_str, table_str);

  table->db = strmake_root(mem_root, db_str.c_str(), db_str.size());
  if (table->db == nullptr) return true;

  table->tablename =
      strmake_root(mem_root, table_str.c_str(), table_str.size());
  if (table->tablename == nullptr) return true;

  return false;
}

/**
  Get prepared transaction info from InnoDB data structure.

  @param[in,out]  txn_list  Handler layer transaction list.
  @param[in]      trx       Innodb transaction info.
  @param[in]      mem_root  Mem_root for space allocation.

  @retval     true          Error, e.g. Memory allocation failure.
  @retval     false         Success
*/

static bool get_info_about_prepared_transaction(XA_recover_txn *txn_list,
                                                const trx_t *trx,
                                                MEM_ROOT *mem_root) {
  txn_list->id = *trx->xid;
  txn_list->mod_tables = new (mem_root) List<st_handler_tablename>();
  if (!txn_list->mod_tables) return true;

  for (auto dd_table : trx->mod_tables) {
    st_handler_tablename *table = new (mem_root) st_handler_tablename();

    if (!table || get_table_name_info(table, dd_table, mem_root) ||
        txn_list->mod_tables->push_back(table, mem_root))
      return true;
  }
  return false;
}

/** This function is used to find number of prepared transactions and
 their transaction objects for a recovery.
 @return number of prepared transactions stored in xid_list */
int trx_recover_for_mysql(
    XA_recover_txn *txn_list, /*!< in/out: prepared transactions */
    ulint len,                /*!< in: number of slots in xid_list */
    MEM_ROOT *mem_root)       /*!< in: memory for table names */
{
  ulint count = 0;

  ut_ad(txn_list);
  ut_ad(len);

  /* We should set those transactions which are in the prepared state
  to the xid_list */

  trx_sys_mutex_enter();

  for (const trx_t *trx : trx_sys->rw_trx_list) {
    assert_trx_in_rw_list(trx);

    /* The state of a read-write transaction cannot change
    from or to NOT_STARTED while we are holding the
    trx_sys->mutex. It may change to PREPARED, but not if
    trx->is_recovered. */
    if (trx_state_eq(trx, TRX_STATE_PREPARED)) {
      if (get_info_about_prepared_transaction(&txn_list[count], trx, mem_root))
        break;

      if (count == 0) {
        ib::info(ER_IB_MSG_1207) << "Starting recovery for"
                                    " XA transactions...";
      }

      ib::info(ER_IB_MSG_1208) << "Transaction " << trx_get_id_for_print(trx)
                               << " in prepared state after recovery";

      ib::info(ER_IB_MSG_1209)
          << "Transaction contains changes to " << trx->undo_no << " rows";

      count++;

      if (count == len) {
        break;
      }
    }
  }

  trx_sys_mutex_exit();

  if (count > 0) {
    ib::info(ER_IB_MSG_1210) << count
                             << " transactions in prepared state"
                                " after recovery";
  }

  return (int(count));
}

int trx_recover_tc_for_mysql(Xa_state_list &xa_list) {
  /* We should set those transactions which are in the prepared state
  to the xid_list */

  trx_sys_mutex_enter();

  for (trx_t *trx : trx_sys->rw_trx_list) {
    assert_trx_in_rw_list(trx);

    /* The state of a read-write transaction cannot change
    from or to NOT_STARTED while we are holding the
    trx_sys->mutex. It may change to PREPARED, but not if
    trx->is_recovered. */
    if (trx_state_eq(trx, TRX_STATE_PREPARED)) {
      if (trx_is_prepared_in_tc(trx)) {
        /* We found the transaction in 2nd phase of prepare, add to XA
           transaction state list as PREPARED_IN_TC */
        xa_list.add(*trx->xid, enum_ha_recover_xa_state::PREPARED_IN_TC);
      } else {
        /* Otherwise, just add as PREPARED_IN_SE */
        xa_list.add(*trx->xid, enum_ha_recover_xa_state::PREPARED_IN_SE);
      }
    }
  }

  trx_sys_mutex_exit();

  return 0;
}

/** This function is used to find one X/Open XA distributed transaction
 which is in the prepared state
 @return trx on match, the trx->xid will be invalidated;
 */
[[nodiscard]] static trx_t *trx_get_trx_by_xid_low(
    const XID *xid) /*!< in: X/Open XA transaction
                    identifier */
{
  ut_ad(trx_sys_mutex_own());

  for (auto trx : trx_sys->rw_trx_list) {
    assert_trx_in_rw_list(trx);

    /* Most of the time server layer takes care of synchronizing access to a XID
    from several connections, but when disconnecting there is a short period in
    which server allows a new connection to pick up XID still processed by old
    connection at InnoDB layer. To synchronize with trx_disconnect_from_mysql(),
    we use trx->mysql_thd under protection of trx_sys->mutex. */

    if (trx->mysql_thd == nullptr && trx_state_eq(trx, TRX_STATE_PREPARED) &&
        xid->eq(trx->xid)) {
      /* Invalidate the XID, so that subsequent calls
      will not find it. */
      trx->xid->reset();
      return trx;
    }
  }

  return nullptr;
}

trx_t *trx_get_trx_by_xid(const XID *xid) {
  trx_t *trx;

  if (xid == nullptr) {
    return (nullptr);
  }

  trx_sys_mutex_enter();

  /* Recovered/Resurrected transactions are always only on the
  trx_sys_t::rw_trx_list. */
  trx = trx_get_trx_by_xid_low(xid);

  trx_sys_mutex_exit();

  return (trx);
}

/** Starts the transaction if it is not yet started.
@param[in,out] trx Transaction
@param[in] read_write True if read write transaction */
void trx_start_if_not_started_xa_low(trx_t *trx, bool read_write) {
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));
  switch (trx->state.load(std::memory_order_relaxed)) {
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:
      trx_start_low(trx, read_write);
      return;

    case TRX_STATE_ACTIVE:
      if (trx->id == 0 && read_write) {
        /* If the transaction is tagged as read-only then
        it can only write to temp tables and for such
        transactions we don't want to move them to the
        trx_sys_t::rw_trx_list. */
        if (!trx->read_only) {
          trx_set_rw_mode(trx);
        } else if (!srv_read_only_mode) {
          trx_assign_rseg_temp(trx);
        }
      }
      return;
    case TRX_STATE_PREPARED:
    case TRX_STATE_COMMITTED_IN_MEMORY:
      break;
  }

  ut_error;
}

/** Starts the transaction if it is not yet started.
@param[in] trx Transaction
@param[in] read_write True if read write transaction */
void trx_start_if_not_started_low(trx_t *trx, bool read_write) {
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));
  switch (trx->state.load(std::memory_order_relaxed)) {
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:

      trx_start_low(trx, read_write);
      return;

    case TRX_STATE_ACTIVE:

      if (read_write && trx->id == 0 && !trx->read_only) {
        trx_set_rw_mode(trx);
      }
      return;

    case TRX_STATE_PREPARED:
    case TRX_STATE_COMMITTED_IN_MEMORY:
      break;
  }

  ut_error;
}

/** Starts a transaction for internal processing. */
void trx_start_internal_low(trx_t *trx) /*!< in/out: transaction */
{
  /* Ensure it is not flagged as an auto-commit-non-locking
  transaction. */

  trx->will_lock = 1;

  trx->internal = true;

  trx_start_low(trx, true);
}

/** Starts a read-only transaction for internal processing.
@param[in,out] trx      transaction to be started */
void trx_start_internal_read_only_low(trx_t *trx) {
  /* Ensure it is not flagged as an auto-commit-non-locking
  transaction. */

  trx->will_lock = 1;

  trx->internal = true;

  trx_start_low(trx, false);
}

/** Set the transaction as a read-write transaction if it is not already
 tagged as such. Read-only transactions that are writing to temporary
 tables are assigned an ID and a rollback segment but are not added
 to the trx read-write list because their updates should not be visible
 to other transactions and therefore their changes can be ignored by
 by MVCC. */
void trx_set_rw_mode(trx_t *trx) /*!< in/out: transaction that is RW */
{
  ut_ad(trx->rsegs.m_redo.rseg == nullptr);
  ut_ad(!trx->in_rw_trx_list);
  ut_ad(!trx_is_autocommit_non_locking(trx));
  ut_ad(!trx->read_only);
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  if (srv_force_recovery >= SRV_FORCE_NO_TRX_UNDO) {
    return;
  }

  /* Function is promoting existing trx from ro mode to rw mode.
  In this process it has acquired trx_sys->mutex as it plan to
  move trx from ro list to rw list. If in future, some other thread
  looks at this trx object while it is being promoted then ensure
  that both threads are synced by acquiring trx->mutex to avoid decision
  based on in-consistent view formed during promotion. */

  trx_assign_rseg_durable(trx);

  ut_ad(trx->rsegs.m_redo.rseg != nullptr);

  DEBUG_SYNC_C("trx_sys_before_assign_id");

  trx_sys_mutex_enter();

  ut_ad(trx->id == 0);
  trx->id = trx_sys_allocate_trx_id();

  trx_sys->rw_trx_ids.push_back(trx->id);

  /* So that we can see our own changes. */
  if (MVCC::is_view_active(trx->read_view)) {
    MVCC::set_view_creator_trx_id(trx->read_view, trx->id);
  }
  trx_add_to_rw_trx_list(trx);

  trx_sys_mutex_exit();

  trx_sys_rw_trx_add(trx);
}

void trx_kill_blocking(trx_t *trx) {
  if (!trx_is_high_priority(trx)) {
    return;
  }
  hit_list_t hit_list;
  lock_make_trx_hit_list(trx, hit_list);
  if (hit_list.empty()) {
    return;
  }

  DEBUG_SYNC_C("trx_kill_blocking_enter");

  ulint had_dict_lock = trx->dict_operation_lock_mode;

  switch (had_dict_lock) {
    case 0:
      break;

    case RW_S_LATCH:
      /* Release foreign key check latch */
      row_mysql_unfreeze_data_dictionary(trx);
      break;

    default:
      /* There should never be a lock wait when the
      dictionary latch is reserved in X mode.  Dictionary
      transactions should only acquire locks on dictionary
      tables, not other tables. All access to dictionary
      tables should be covered by dictionary
      transactions. */
      ut_error;
  }

  ut_a(trx->dict_operation_lock_mode == 0);

  /** Kill the transactions in the lock acquisition order old -> new. */
  hit_list_t::reverse_iterator end = hit_list.rend();

  for (hit_list_t::reverse_iterator it = hit_list.rbegin(); it != end; ++it) {
    trx_t *victim_trx = it->m_trx;
    auto version = it->m_version;

    /* Shouldn't commit suicide. */
    ut_ad(victim_trx != trx);
    ut_ad(victim_trx->mysql_thd != trx->mysql_thd);

    if (lock_cancel_if_waiting_and_release(*it)) {
      continue;
    }

    /* Check that the transaction isn't active inside
    InnoDB code. We have to wait while it is executing
    in the InnoDB context. This can potentially take a
    long time */

    trx_mutex_enter(victim_trx);
    ut_ad(version <= victim_trx->version);

    ulint loop_count = 0;
    /* start with optimistic sleep time of 20 micro seconds. */
    ulint sleep_time = 20;

    bool exited_innodb = false;

    while ((victim_trx->in_innodb & TRX_FORCE_ROLLBACK_MASK) > 0 &&
           victim_trx->version == version) {
      trx_mutex_exit(victim_trx);

      /* Declare this OS thread to exit InnoDB, before waiting */
      if (trx->declared_to_be_inside_innodb) {
        exited_innodb = true;
        srv_conc_force_exit_innodb(trx);
      }

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

      trx_mutex_enter(victim_trx);
    }

    /* Return back inside InnoDB */
    if (exited_innodb) {
      exited_innodb = false;
      /* Exit transaction mutex before entering Innodb. */
      trx_mutex_exit(victim_trx);
      srv_conc_force_enter_innodb(trx);
      trx_mutex_enter(victim_trx);
    }

    /* Compare the version to check if the transaction has
    already finished */
    if (victim_trx->version != version) {
      trx_mutex_exit(victim_trx);
      continue;
    }

    /* We should never kill background transactions. */
    ut_ad(victim_trx->mysql_thd != nullptr);

    ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK_DISABLE));
    ut_ad(victim_trx->in_innodb & TRX_FORCE_ROLLBACK);
    ut_ad(victim_trx->killed_by == std::this_thread::get_id());
    ut_ad(victim_trx->version == it->m_version);

    /* We don't kill Read Only, Background or high priority
    transactions. */
    ut_a(!victim_trx->read_only);
    ut_a(victim_trx->mysql_thd != nullptr);

    trx_mutex_exit(victim_trx);

#ifdef UNIV_DEBUG
    char buffer[1024];
    char *thr_text;
    trx_id_t id;

    thr_text = thd_security_context(victim_trx->mysql_thd, buffer,
                                    sizeof(buffer), 512);
    id = victim_trx->id;
#endif /* UNIV_DEBUG */
    trx_rollback_for_mysql(victim_trx);

#ifdef UNIV_DEBUG
    ib::info(ER_IB_MSG_1211, ulonglong{trx->id},
             to_string(std::this_thread::get_id()).c_str(), ulonglong{id},
             thr_text);
#endif /* UNIV_DEBUG */
    trx_mutex_enter(victim_trx);

    version++;
    ut_ad(victim_trx->version == version);

    victim_trx->killed_by.store(std::thread::id{});

    victim_trx->in_innodb &= TRX_FORCE_ROLLBACK_MASK;

    trx_mutex_exit(victim_trx);
  }

  if (had_dict_lock) {
    row_mysql_freeze_data_dictionary(trx, UT_LOCATION_HERE);
  }
}

/* To get current session thread default THD */
THD *thd_get_current_thd();

void trx_sys_update_binlog_position(trx_t *trx) {
  THD *thd = trx->mysql_thd;
  /* For XA commit/rollback by XID, transaction thd could be null. */
  if (thd == nullptr) {
    thd = thd_get_current_thd();
    if (thd == nullptr) {
      return;
    }
  }
  ulonglong pos;
  thd_binlog_pos(thd, &trx->mysql_log_file_name, &pos);
  trx->mysql_log_offset = static_cast<uint64_t>(pos);
}

bool trx_is_prepared_in_tc(trx_t const *trx) {
  trx_undo_t *undo{nullptr};

  if (trx->rsegs.m_redo.rseg != nullptr && trx_is_redo_rseg_updated(trx)) {
    if (trx->rsegs.m_redo.insert_undo != nullptr) {
      undo = trx->rsegs.m_redo.insert_undo;
    } else {
      undo = trx->rsegs.m_redo.update_undo;
    }
  }

  return undo != nullptr && (undo->state == TRX_UNDO_PREPARED_80028 ||
                             undo->state == TRX_UNDO_PREPARED_IN_TC);
}

dberr_t trx_set_prepared_in_tc_for_mysql(trx_t *trx) {
  ut_a(trx->state.load(std::memory_order_relaxed) == TRX_STATE_PREPARED);

  /* For GTID persistence we need update undo segment. */
  auto db_err = trx_undo_gtid_add_update_undo(trx, true, false);
  if (db_err != DB_SUCCESS) {
    return (db_err);
  }

  trx->op_info = "marking transaction as prepared in TC";

  trx_set_prepared_in_tc(trx);

  trx->op_info = "";

  return (DB_SUCCESS);
}

static void trx_flush_logs(trx_t *trx, lsn_t lsn) {
  if (lsn == 0) {
    return;
  }
  switch (thd_requested_durability(trx->mysql_thd)) {
    case HA_IGNORE_DURABILITY:
      /* We set the HA_IGNORE_DURABILITY during prepare phase of
      binlog group commit to not flush redo log for every transaction
      here. So that we can flush prepared records of transactions to
      redo log in a group right before writing them to binary log
      during flush stage of binlog group commit. */
      break;
    case HA_REGULAR_DURABILITY:
      /* Depending on the my.cnf options, we may now write the log
      buffer to the log files, making the prepared state of the
      transaction durable if the OS does not crash. We may also
      flush the log files to disk, making the prepared state of the
      transaction durable also at an OS crash or a power outage.

      The idea in InnoDB's group prepare is that a group of
      transactions gather behind a trx doing a physical disk write
      to log files, and when that physical write has been completed,
      one of those transactions does a write which prepares the whole
      group. Note that this group prepare will only bring benefit if
      there are > 2 users in the database. Then at least 2 users can
      gather behind one doing the physical log write to disk.

      We must not be holding any mutexes or latches here. */

      /* We should trust trx->ddl_operation instead of
      ddl_must_flush here */
      trx->ddl_must_flush = false;
      trx_flush_log_if_needed(lsn, trx);
  }
}
