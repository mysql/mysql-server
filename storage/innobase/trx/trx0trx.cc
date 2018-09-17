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

/** @file trx/trx0trx.cc
 The transaction

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#include <sys/types.h>
#include <time.h>
#include <new>
#include <set>

#include <sql_thd_internal_api.h>

#include "btr0sea.h"
#include "dict0dd.h"
#include "fsp0sysspace.h"
#include "ha_prototypes.h"
#include "lock0lock.h"
#include "log0log.h"
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

static const ulint MAX_DETAILED_ERROR_LEN = 256;

/** Set of table_id */
typedef std::set<table_id_t, std::less<table_id_t>, ut_allocator<table_id_t>>
    table_id_set;

/** Map of transactions to affected table_id */
typedef std::map<trx_t *, table_id_set, std::less<trx_t *>,
                 ut_allocator<std::pair<trx_t *const, table_id_set>>>
    trx_table_map;

/** Map of resurrected transactions to affected table_id */
static trx_table_map resurrected_trx_tables;

/** Dummy session used currently in MySQL interface */
sess_t *trx_dummy_sess = NULL;

/** Constructor */
TrxVersion::TrxVersion(trx_t *trx) : m_trx(trx), m_version(trx->version) {
  /* No op */
}

/** Set flush observer for the transaction
@param[in,out]	trx		transaction struct
@param[in]	observer	flush observer */
void trx_set_flush_observer(trx_t *trx, FlushObserver *observer) {
  trx->flush_observer = observer;
}

/** Set detailed error message for the transaction. */
void trx_set_detailed_error(trx_t *trx,      /*!< in: transaction struct */
                            const char *msg) /*!< in: detailed error message */
{
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

  trx->skip_lock_inheritance = false;

  trx->is_recovered = false;

  trx->op_info = "";

  trx->isolation_level = TRX_ISO_REPEATABLE_READ;

  trx->check_foreigns = true;

  trx->check_unique_secondary = true;

  trx->lock.n_rec_locks = 0;

  trx->dict_operation = TRX_DICT_OP_NONE;

  trx->ddl_operation = false;

  trx->error_state = DB_SUCCESS;

  trx->error_key_num = ULINT_UNDEFINED;

  trx->undo_no = 0;

  trx->rsegs.m_redo.rseg = NULL;

  trx->rsegs.m_noredo.rseg = NULL;

  trx->read_only = false;

  trx->auto_commit = false;

  trx->will_lock = 0;

  trx->internal = false;

  trx->in_truncate = false;
#ifdef UNIV_DEBUG
  trx->is_dd_trx = false;
  trx->in_rollback = false;
  trx->lock.in_rollback = false;
#endif /* UNIV_DEBUG */

  ut_d(trx->start_file = 0);

  ut_d(trx->start_line = 0);

  trx->magic_n = TRX_MAGIC_N;

  trx->lock.que_state = TRX_QUE_RUNNING;

  trx->last_sql_stat_start.least_undo_no = 0;

  ut_ad(!MVCC::is_view_active(trx->read_view));

  trx->lock.rec_cached = 0;

  trx->lock.table_cached = 0;

  /* During asynchronous rollback, we should reset forced rollback flag
  only after rollback is complete to avoid race with the thread owning
  the transaction. */

  if (!TrxInInnoDB::is_async_rollback(trx)) {
    os_thread_id_t thread_id = trx->killed_by;
    os_compare_and_swap_thread_id(&trx->killed_by, thread_id, 0);

    /* Note: Do not set to 0, the ref count is decremented inside
    the TrxInInnoDB() destructor. We only need to clear the flags. */

    trx->in_innodb &= TRX_FORCE_ROLLBACK_MASK;
  }

  /* Note: It's possible that this list is not empty if a transaction
  was interrupted after it collected the victim transactions and before
  it got a chance to roll them back asynchronously. */

  trx->hit_list.clear();

  trx->flush_observer = NULL;

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
    ut_zalloc() in Pool::Pool() which would not call
    the constructors of the trx_t members. */
    new (&trx->mod_tables) trx_mod_tables_t();

    new (&trx->lock.rec_pool) lock_pool_t();

    new (&trx->lock.table_pool) lock_pool_t();

    new (&trx->lock.table_locks) lock_pool_t();

    new (&trx->hit_list) hit_list_t();

    trx_init(trx);

    trx->state = TRX_STATE_NOT_STARTED;

    trx->dict_operation_lock_mode = 0;

    trx->xid = UT_NEW_NOKEY(xid_t());

    trx->detailed_error =
        reinterpret_cast<char *>(ut_zalloc_nokey(MAX_DETAILED_ERROR_LEN));

    trx->lock.lock_heap = mem_heap_create_typed(1024, MEM_HEAP_FOR_LOCK_HEAP);

    lock_trx_lock_list_init(&trx->lock.trx_locks);

    UT_LIST_INIT(trx->trx_savepoints, &trx_named_savept_t::trx_savepoints);

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

    ut_a(trx->lock.wait_lock == NULL);
    ut_a(trx->lock.wait_thr == NULL);

    ut_a(!trx->has_search_latch);

    ut_a(trx->dict_operation_lock_mode == 0);

    if (trx->lock.lock_heap != NULL) {
      mem_heap_free(trx->lock.lock_heap);
      trx->lock.lock_heap = NULL;
    }

    ut_a(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);

    UT_DELETE(trx->xid);
    ut_free(trx->detailed_error);

    mutex_free(&trx->mutex);
    mutex_free(&trx->undo_mutex);

    trx->mod_tables.~trx_mod_tables_t();

    ut_ad(trx->read_view == NULL);

    if (!trx->lock.rec_pool.empty()) {
      /* See lock_trx_alloc_locks() why we only free
      the first element. */

      ut_free(trx->lock.rec_pool[0]);
    }

    if (!trx->lock.table_pool.empty()) {
      /* See lock_trx_alloc_locks() why we only free
      the first element. */

      ut_free(trx->lock.table_pool[0]);
    }

    trx->lock.rec_pool.~lock_pool_t();

    trx->lock.table_pool.~lock_pool_t();

    trx->lock.table_locks.~lock_pool_t();

    trx->hit_list.~hit_list_t();
  }

  /** Enforce any invariants here, this is called before the transaction
  is added to the pool.
  @return true if all OK */
  static bool debug(const trx_t *trx) {
    ut_a(trx->error_state == DB_SUCCESS);

    ut_a(trx->magic_n == TRX_MAGIC_N);

    ut_ad(!trx->read_only);

    ut_ad(trx->state == TRX_STATE_NOT_STARTED ||
          trx->state == TRX_STATE_FORCED_ROLLBACK);

    ut_ad(trx->dict_operation == TRX_DICT_OP_NONE);

    ut_ad(trx->mysql_thd == 0);

    ut_ad(!trx->in_rw_trx_list);
    ut_ad(!trx->in_mysql_trx_list);

    ut_a(trx->lock.wait_thr == NULL);
    ut_a(trx->lock.wait_lock == NULL);

    ut_a(!trx->has_search_latch);

    ut_a(trx->dict_operation_lock_mode == 0);

    ut_a(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);

    ut_ad(trx->autoinc_locks == NULL);

    ut_ad(trx->lock.table_locks.empty());

    ut_ad(!trx->abort);

    ut_ad(trx->hit_list.empty());

    ut_ad(trx->killed_by == 0);

    return (true);
  }
};

/** The lock strategy for TrxPool */
struct TrxPoolLock {
  TrxPoolLock() {}

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
  TrxPoolManagerLock() {}

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
  trx_pools = UT_NEW_NOKEY(trx_pools_t(MAX_TRX_BLOCK_SIZE));

  ut_a(trx_pools != 0);
}

/** Destroy the trx_t pool */
void trx_pool_close() {
  UT_DELETE(trx_pools);

  trx_pools = 0;
}

/** @return a trx_t instance from trx_pools. */
static trx_t *trx_create_low() {
  trx_t *trx = trx_pools->get();

  assert_trx_is_free(trx);

  mem_heap_t *heap;
  ib_alloc_t *alloc;

  /* We just got trx from pool, it should be non locking */
  ut_ad(trx->will_lock == 0);

  trx->api_trx = false;

  trx->api_auto_commit = false;

  trx->read_write = true;

  /* Background trx should not be forced to rollback,
  we will unset the flag for user trx. */
  trx->in_innodb |= TRX_FORCE_ROLLBACK_DISABLE;

  /* Trx state can be TRX_STATE_FORCED_ROLLBACK if
  the trx was forced to rollback before it's reused.*/
  trx->state = TRX_STATE_NOT_STARTED;

  heap = mem_heap_create(sizeof(ib_vector_t) + sizeof(void *) * 8);

  alloc = ib_heap_allocator_create(heap);

  /* Remember to free the vector explicitly in trx_free(). */
  trx->autoinc_locks = ib_vector_create(alloc, sizeof(void **), 4);

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

  trx->mysql_thd = 0;

  // FIXME: We need to avoid this heap free/alloc for each commit.
  if (trx->autoinc_locks != NULL) {
    ut_ad(ib_vector_is_empty(trx->autoinc_locks));
    /* We allocated a dedicated heap for the vector. */
    ib_vector_free(trx->autoinc_locks);
    trx->autoinc_locks = NULL;
  }

  trx->mod_tables.clear();

  ut_ad(trx->read_view == NULL);
  ut_ad(trx->is_dd_trx == false);

  /* trx locking state should have been reset before returning trx
  to pool */
  ut_ad(trx->will_lock == 0);

  trx_pools->mem_free(trx);

  trx = NULL;
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

  ut_d(trx->in_mysql_trx_list = TRUE);
  UT_LIST_ADD_FIRST(trx_sys->mysql_trx_list, trx);

  trx_sys_mutex_exit();

  return (trx);
}

/** Check state of transaction before freeing it.
@param[in,out]	trx	transaction object to validate */
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
        << "MySQL is freeing a thd though"
           " trx->n_mysql_tables_in_use is "
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
@param[in,out]	trx	transaction object to free and initialize */
void trx_free_resurrected(trx_t *trx) {
  trx_validate_state_before_free(trx);

  trx_init(trx);

  trx_free(trx);
}

/** Free a transaction that was allocated by background or user threads.
@param[in,out]	trx	transaction object to free */
void trx_free_for_background(trx_t *trx) {
  trx_validate_state_before_free(trx);

  trx_free(trx);
}

/** At shutdown, frees a transaction object that is in the PREPARED state. */
void trx_free_prepared(trx_t *trx) /*!< in, own: trx object */
{
  ut_a(trx_state_eq(trx, TRX_STATE_PREPARED));
  ut_a(trx->magic_n == TRX_MAGIC_N);

  lock_trx_release_locks(trx);
  trx_undo_free_prepared(trx);

  assert_trx_in_rw_list(trx);

  ut_a(!trx->read_only);

  ut_d(trx->in_rw_trx_list = FALSE);

  trx->state = TRX_STATE_NOT_STARTED;

  /* Undo trx_resurrect_table_locks(). */
  lock_trx_lock_list_init(&trx->lock.trx_locks);

  /* Note: This vector is not guaranteed to be empty because the
  transaction was never committed and therefore lock_trx_release()
  was not called. */
  trx->lock.table_locks.clear();

  trx_free(trx);
}

/** Disconnect a transaction from MySQL and optionally mark it as if
it's been recovered. For the marking the transaction must be in prepared state.
The recovery-marked transaction is going to survive "alone" so its association
with the mysql handle is destroyed now rather than when it will be
finally freed.
@param[in,out]	trx		transaction
@param[in]	prepared	boolean value to specify whether trx is
                                for recovery or not. */
inline void trx_disconnect_from_mysql(trx_t *trx, bool prepared) {
  trx_sys_mutex_enter();

  ut_ad(trx->in_mysql_trx_list);
  ut_d(trx->in_mysql_trx_list = FALSE);

  UT_LIST_REMOVE(trx_sys->mysql_trx_list, trx);

  if (trx->read_view != NULL) {
    trx_sys->mvcc->view_close(trx->read_view, true);
  }

  ut_ad(trx_sys_validate_trx_list());

  if (prepared) {
    ut_ad(trx_state_eq(trx, TRX_STATE_PREPARED));

    trx->is_recovered = true;
    trx->mysql_thd = NULL;
    /* todo/fixme: suggest to do it at innodb prepare */
    trx->will_lock = 0;
  }

  trx_sys_mutex_exit();
}

/** Disconnect a transaction from MySQL.
@param[in,out]	trx	transaction */
inline void trx_disconnect_plain(trx_t *trx) {
  trx_disconnect_from_mysql(trx, false);
}

/** Disconnect a prepared transaction from MySQL.
@param[in,out]	trx	transaction */
void trx_disconnect_prepared(trx_t *trx) {
  trx_disconnect_from_mysql(trx, true);
}

/** Free a transaction object for MySQL.
@param[in,out]	trx	transaction */
void trx_free_for_mysql(trx_t *trx) {
  trx_disconnect_plain(trx);
  trx_free_for_background(trx);
}

/** Resurrect the table IDs for a resurrected transaction.
@param[in]	trx		resurrected transaction
@param[in]	undo_ptr	pointer to undo segment
@param[in]	undo		undo log */
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

  do {
    ulint type;
    undo_no_t undo_no;
    table_id_t table_id;
    ulint cmpl_info;
    bool updated_extern;
    type_cmpl_t type_cmpl;

    page_t *undo_rec_page = page_align(undo_rec);

    if (undo_rec_page != undo_page) {
      mtr.release_page(undo_page, MTR_MEMO_PAGE_X_FIX);
      undo_page = undo_rec_page;
    }

    trx_undo_rec_get_pars(undo_rec, &type, &cmpl_info, &updated_extern,
                          &undo_no, &table_id, type_cmpl);
    tables.insert(table_id);

    undo_rec = trx_undo_get_prev_rec(undo_rec, undo->hdr_page_no,
                                     undo->hdr_offset, false, &mtr);
  } while (undo_rec);

  mtr_commit(&mtr);
}

/** Resurrect table locks for resurrected transactions. */
void trx_resurrect_locks() {
  for (trx_table_map::const_iterator t = resurrected_trx_tables.begin();
       t != resurrected_trx_tables.end(); t++) {
    trx_t *trx = t->first;
    const table_id_set &tables = t->second;
    ut_ad(trx->is_recovered);

    for (table_id_set::const_iterator i = tables.begin(); i != tables.end();
         i++) {
      dict_table_t *table = dd_table_open_on_id(*i, NULL, NULL, false, true);
      if (table) {
        ut_ad(!table->is_temporary());

        if (table->ibd_file_missing || table->is_temporary()) {
          mutex_enter(&dict_sys->mutex);
          dd_table_close(table, NULL, NULL, true);
          dict_table_remove_from_cache(table);
          mutex_exit(&dict_sys->mutex);
          continue;
        }

        if (trx->state == TRX_STATE_PREPARED && !dict_table_is_sdi(table->id)) {
          trx->mod_tables.insert(table);
        }
        DICT_TF2_FLAG_SET(table, DICT_TF2_RESURRECT_PREPARED);

        lock_table_ix_resurrect(table, trx);

        DBUG_PRINT("ib_trx", ("resurrect" TRX_ID_FMT "  table '%s' IX lock",
                              trx_get_id_for_print(trx), table->name.m_name));

        dd_table_close(table, NULL, NULL, false);
      }
    }
  }

  resurrected_trx_tables.clear();
}

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
  trx->rsegs.m_redo.insert_undo = undo;
  trx->is_recovered = true;

  /* This is single-threaded startup code, we do not need the
  protection of trx->mutex or trx_sys->mutex here. */

  if (undo->state != TRX_UNDO_ACTIVE) {
    /* Prepared transactions are left in the prepared state
    waiting for a commit or abort decision from MySQL */

    if (undo->state == TRX_UNDO_PREPARED) {
      ib::info(ER_IB_MSG_1204) << "Transaction " << trx_get_id_for_print(trx)
                               << " was in the XA prepared state.";

      if (srv_force_recovery == 0) {
        trx->state = TRX_STATE_PREPARED;
        ++trx_sys->n_prepared_trx;
      } else {
        ib::info(ER_IB_MSG_1205) << "Since innodb_force_recovery"
                                    " > 0, we will force a rollback.";

        trx->state = TRX_STATE_ACTIVE;
      }
    } else {
      trx->state = TRX_STATE_COMMITTED_IN_MEMORY;
    }

    /* We give a dummy value for the trx no; this should have no
    relevance since purge is not interested in committed
    transaction numbers, unless they are in the history
    list, in which case it looks the number from the disk based
    undo log structure */

    trx->no = trx->id;

  } else {
    trx->state = TRX_STATE_ACTIVE;

    /* A running transaction always has the number
    field inited to TRX_ID_MAX */

    trx->no = TRX_ID_MAX;
  }

  /* trx_start_low() is not called with resurrect, so need to initialize
  start time here.*/
  if (trx->state == TRX_STATE_ACTIVE || trx->state == TRX_STATE_PREPARED) {
    trx->start_time = ut_time();
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

  if (undo->state == TRX_UNDO_PREPARED) {
    ib::info(ER_IB_MSG_1206) << "Transaction " << trx_get_id_for_print(trx)
                             << " was in the XA prepared state.";

    ut_ad(trx->state != TRX_STATE_FORCED_ROLLBACK);

    if (trx_state_eq(trx, TRX_STATE_NOT_STARTED)) {
      ++trx_sys->n_prepared_trx;
    } else {
      ut_ad(trx_state_eq(trx, TRX_STATE_PREPARED));
    }

    trx->state = TRX_STATE_PREPARED;
  } else {
    trx->state = TRX_STATE_COMMITTED_IN_MEMORY;
  }
}

/** Resurrect the transactions that were doing updates the time of the
 crash, they need to be undone. */
static void trx_resurrect_update(
    trx_t *trx,       /*!< in/out: transaction */
    trx_undo_t *undo, /*!< in/out: update UNDO record */
    trx_rseg_t *rseg) /*!< in/out: rollback segment */
{
  rseg->trx_ref_count++;
  trx->rsegs.m_redo.rseg = rseg;
  *trx->xid = undo->xid;
  trx->id = undo->trx_id;
  trx->rsegs.m_redo.update_undo = undo;
  trx->is_recovered = true;

  /* This is single-threaded startup code, we do not need the
  protection of trx->mutex or trx_sys->mutex here. */

  if (undo->state != TRX_UNDO_ACTIVE) {
    trx_resurrect_update_in_prepared_state(trx, undo);

    /* We give a dummy value for the trx number */

    trx->no = trx->id;

  } else {
    trx->state = TRX_STATE_ACTIVE;

    /* A running transaction always has the number field inited to
    TRX_ID_MAX */

    trx->no = TRX_ID_MAX;
  }

  /* trx_start_low() is not called with resurrect, so need to initialize
  start time here.*/
  if (trx->state == TRX_STATE_ACTIVE || trx->state == TRX_STATE_PREPARED) {
    trx->start_time = ut_time();
  }

  if (!undo->empty && undo->top_undo_no >= trx->undo_no) {
    trx->undo_no = undo->top_undo_no + 1;
    trx->undo_rseg_space = undo->rseg->space_id;
  }
}

/** Resurrect the transactions that were doing inserts and updates at
the time of a crash, they need to be undone.
@param[in]	rseg	rollback segment */
static void trx_resurrect(trx_rseg_t *rseg) {
  trx_t *trx;
  trx_undo_t *undo;

  ut_ad(rseg != nullptr);

  /* Resurrect transactions that were doing inserts. */
  for (undo = UT_LIST_GET_FIRST(rseg->insert_undo_list); undo != NULL;
       undo = UT_LIST_GET_NEXT(undo_list, undo)) {
    trx = trx_resurrect_insert(undo, rseg);

    trx_sys_rw_trx_add(trx);

    trx_resurrect_table_ids(trx, &trx->rsegs.m_redo, undo);
  }

  /* Ressurrect transactions that were doing updates. */
  for (undo = UT_LIST_GET_FIRST(rseg->update_undo_list); undo != NULL;
       undo = UT_LIST_GET_NEXT(undo_list, undo)) {
    /* Check the trx_sys->rw_trx_set first. */
    trx_sys_mutex_enter();

    trx_t *trx = trx_get_rw_trx_by_id(undo->trx_id);

    trx_sys_mutex_exit();

    if (trx == NULL) {
      trx = trx_allocate_for_background();

      ut_d(trx->start_file = __FILE__);
      ut_d(trx->start_line = __LINE__);
    }

    trx_resurrect_update(trx, undo, rseg);

    trx_sys_rw_trx_add(trx);

    trx_resurrect_table_ids(trx, &trx->rsegs.m_redo, undo);
  }
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

  TrxIdSet::iterator end = trx_sys->rw_trx_set.end();

  for (TrxIdSet::iterator it = trx_sys->rw_trx_set.begin(); it != end; ++it) {
    ut_ad(it->m_trx->in_rw_trx_list);
#ifdef UNIV_DEBUG
    if (it->m_trx->id > trx_sys->rw_max_trx_id) {
      trx_sys->rw_max_trx_id = it->m_trx->id;
    }
#endif /* UNIV_DEBUG */

    if (it->m_trx->state == TRX_STATE_ACTIVE ||
        it->m_trx->state == TRX_STATE_PREPARED) {
      trx_sys->rw_trx_ids.push_back(it->m_id);
    }

    UT_LIST_ADD_FIRST(trx_sys->rw_trx_list, it->m_trx);
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
@param[in]	target_undo_tablespaces
                                number of undo tablespaces to use when
                                calculating which rseg to assign.
@return assigned rollback segment instance */
static trx_rseg_t *get_next_redo_rseg_from_undo_spaces(
    ulong target_undo_tablespaces) {
  undo::Tablespace *undo_space;
  ulong target_rollback_segments = srv_rollback_segments;

  ut_ad(target_undo_tablespaces > 0);
  ut_d(undo::spaces->s_lock());
  ut_ad(target_undo_tablespaces <= undo::spaces->size());
  ut_d(undo::spaces->s_unlock());

  static ulint rseg_counter = 0;
  trx_rseg_t *rseg = nullptr;
  ulint current = rseg_counter;

  /* Increment the static redo_rseg_slot so the next call from any thread
  starts with the next rseg. */
  os_atomic_increment_ulint(&rseg_counter, 1);

  while (rseg == nullptr) {
    /* Traverse the rsegs like this: (space, rseg_id)
    (0,0), (1,0), ... (n,0), (0,1), (1,1), ... (n,1), ... */
    ulint window =
        current % (target_rollback_segments * target_undo_tablespaces);
    ulint spaces_slot = window % target_undo_tablespaces;
    ulint rseg_slot = window / target_undo_tablespaces;

    current++;

    undo_space = undo::spaces->at(spaces_slot);

    ut_ad(target_rollback_segments <= undo_space->rsegs()->size());

    /* Avoid any rseg that resides in a tablespace that has
    been marked for truncate. This is possible only if there
    are at least 2 UNDO tablespaces active.
    We do not want to wait below on an s_lock for an rseg in
    an undo tablespace that is being truncated.  So check
    this first.  It could be set immediately after this, but
    that is a very short gap, compared to the time it takes
    to truncate an undo tablespace.*/
    if (undo_space->rsegs()->is_inactive()) {
      rseg = nullptr;
      continue;
    }

    /* Check again with a shared lock. */
    undo_space->rsegs()->s_lock();
    if (undo_space->rsegs()->is_inactive()) {
      rseg = nullptr;
      undo_space->rsegs()->s_unlock();
      continue;
    }

    /* Mark the chosen rseg so that it will not be selected
    for UNDO truncation. */
    rseg = undo_space->rsegs()->at(rseg_slot);
    rseg->trx_ref_count++;

    undo_space->rsegs()->s_unlock();
  }

  ut_ad(rseg->trx_ref_count > 0);

  return (rseg);
}

/** Get the next redo rollback segment in round-robin fashion.
The assigned slots may have gaps but the vector does not.
@return assigned rollback segment instance */
static trx_rseg_t *get_next_redo_rseg_from_trx_sys() {
  static ulint rseg_counter = 0;
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
  ulint slot =
      os_atomic_increment_ulint(&rseg_counter, 1) % n_rollback_segments;

  /* s_lock the vector since it might be sorted when added to. */
  trx_sys->rsegs.s_lock();
  trx_rseg_t *rseg = trx_sys->rsegs.at(slot);
  trx_sys->rsegs.s_unlock();

  /* It is not neccessary to s_lock Rsegs::m_latch here because the
  system tablespace is never truncated like other undo tablespaces. */
  rseg->trx_ref_count++;

  ut_ad(rseg->space_id == TRX_SYS_SPACE);

  return (rseg);
}

/** Get next redo rollback segment in round-robin fashion.
We assume that the assigned slots are not contiguous and have gaps.
@return assigned rollback segment instance */
static trx_rseg_t *get_next_redo_rseg() {
  /* Since this global value can change at any instant, use the
  value at this instant. */
  ulong target = srv_undo_tablespaces;

  if (target == 0) {
    return (get_next_redo_rseg_from_trx_sys());
  } else {
    return (get_next_redo_rseg_from_undo_spaces(target));
  }
}

/** Get the next noredo rollback segment.
@return assigned rollback segment instance */
static trx_rseg_t *get_next_temp_rseg() {
  static ulint temp_rseg_counter = 0;
  ulong n_rollback_segments = srv_rollback_segments;

  ut_ad(n_rollback_segments <= trx_sys->tmp_rsegs.size());

  /* Try the next slot that no other thread is looking at */
  ulint slot =
      os_atomic_increment_ulint(&temp_rseg_counter, 1) % n_rollback_segments;

  /* No need to s_lock the vector since it is only added to at the end,
  and it is never resized or sorted. */
  trx_rseg_t *rseg = trx_sys->tmp_rsegs.at(slot);

  ut_ad(rseg->id == slot);
  ut_ad(fsp_is_system_temporary(rseg->space_id));

  return (rseg);
}

/** Assign a durable rollback segment to a transaction in a round-robin
fashion.
@param[in,out]	trx	transaction that involves a durable write. */
void trx_assign_rseg_durable(trx_t *trx) {
  ut_ad(trx->rsegs.m_redo.rseg == nullptr);

  trx->rsegs.m_redo.rseg = srv_read_only_mode ? nullptr : get_next_redo_rseg();
}

/** Assign a temp-tablespace bound rollback-segment to a transaction.
@param[in,out]	trx	transaction that involves write to temp-table. */
void trx_assign_rseg_temp(trx_t *trx) {
  ut_ad(trx->rsegs.m_noredo.rseg == 0);
  ut_ad(!trx_is_autocommit_non_locking(trx));

  trx->rsegs.m_noredo.rseg =
      srv_read_only_mode ? nullptr : get_next_temp_rseg();

  if (trx->id == 0) {
    mutex_enter(&trx_sys->mutex);

    trx->id = trx_sys_get_new_trx_id();

    trx_sys->rw_trx_ids.push_back(trx->id);

    trx_sys->rw_trx_set.insert(TrxTrack(trx->id, trx));

    mutex_exit(&trx_sys->mutex);
  }
}

/** Starts a transaction. */
static void trx_start_low(
    trx_t *trx,      /*!< in: transaction */
    bool read_write) /*!< in: true if read-write transaction */
{
  ut_ad(!trx->in_rollback);
  ut_ad(!trx->is_recovered);
  ut_ad(trx->hit_list.empty());
  ut_ad(trx->start_line != 0);
  ut_ad(trx->start_file != 0);
  ut_ad(trx->roll_limit == 0);
  ut_ad(!trx->lock.in_rollback);
  ut_ad(trx->error_state == DB_SUCCESS);
  ut_ad(trx->rsegs.m_redo.rseg == NULL);
  ut_ad(trx->rsegs.m_noredo.rseg == NULL);
  ut_ad(trx_state_eq(trx, TRX_STATE_NOT_STARTED));
  ut_ad(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);
  ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK));
  ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK_ASYNC));

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

  if (trx->mysql_thd != nullptr && !trx->ddl_operation) {
    trx->ddl_operation = thd_is_dd_update_stmt(trx->mysql_thd);
  }

  /* The initial value for trx->no: TRX_ID_MAX is used in
  read_view_open_now: */

  trx->no = TRX_ID_MAX;

  ut_a(ib_vector_is_empty(trx->autoinc_locks));
  ut_a(trx->lock.table_locks.empty());

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
      (trx->mysql_thd == 0 || read_write || trx->ddl_operation)) {
    trx_assign_rseg_durable(trx);

    /* Temporary rseg is assigned only if the transaction
    updates a temporary table */

    trx_sys_mutex_enter();

    trx->id = trx_sys_get_new_trx_id();

    trx_sys->rw_trx_ids.push_back(trx->id);

    trx_sys_rw_trx_add(trx);

    ut_ad(trx->rsegs.m_redo.rseg != 0 || srv_read_only_mode ||
          srv_force_recovery >= SRV_FORCE_NO_TRX_UNDO);

    UT_LIST_ADD_FIRST(trx_sys->rw_trx_list, trx);

    ut_d(trx->in_rw_trx_list = true);
#ifdef UNIV_DEBUG
    if (trx->id > trx_sys->rw_max_trx_id) {
      trx_sys->rw_max_trx_id = trx->id;
    }
#endif /* UNIV_DEBUG */

    trx->state = TRX_STATE_ACTIVE;

    ut_ad(trx_sys_validate_trx_list());

    trx_sys_mutex_exit();

  } else {
    trx->id = 0;

    if (!trx_is_autocommit_non_locking(trx)) {
      /* If this is a read-only transaction that is writing
      to a temporary table then it needs a transaction id
      to write to the temporary table. */

      if (read_write) {
        trx_sys_mutex_enter();

        ut_ad(!srv_read_only_mode);

        trx->id = trx_sys_get_new_trx_id();

        trx_sys->rw_trx_ids.push_back(trx->id);

        trx_sys->rw_trx_set.insert(TrxTrack(trx->id, trx));

        trx_sys_mutex_exit();
      }

      trx->state = TRX_STATE_ACTIVE;

    } else {
      ut_ad(!read_write);
      trx->state = TRX_STATE_ACTIVE;
    }
  }

  if (trx->mysql_thd != NULL) {
    trx->start_time = thd_start_time_in_secs(trx->mysql_thd);
  } else {
    trx->start_time = ut_time();
  }

  trx->age = 0;
  trx->age_updated = 0;

  ut_a(trx->error_state == DB_SUCCESS);

  MONITOR_INC(MONITOR_TRX_ACTIVE);
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
  trx_rseg_t *redo_rseg = 0;
  trx_rseg_t *temp_rseg = 0;

  if (redo_rseg_undo_ptr != NULL) {
    ut_ad(mutex_own(&redo_rseg_undo_ptr->rseg->mutex));
    redo_rseg = redo_rseg_undo_ptr->rseg;
  }

  if (temp_rseg_undo_ptr != NULL) {
    ut_ad(mutex_own(&temp_rseg_undo_ptr->rseg->mutex));
    temp_rseg = temp_rseg_undo_ptr->rseg;
  }

  trx_sys_mutex_enter();

  trx->no = trx_sys_get_new_trx_id();

  /* Track the minimum serialisation number. */
  if (!trx->read_only) {
    UT_LIST_ADD_LAST(trx_sys->serialisation_list, trx);
    added_trx_no = true;
  } else {
    added_trx_no = false;
  }

  /* If the rollack segment is not empty then the
  new trx_t::no can't be less than any trx_t::no
  already in the rollback segment. User threads only
  produce events when a rollback segment is empty. */
  if ((redo_rseg != NULL && redo_rseg->last_page_no == FIL_NULL) ||
      (temp_rseg != NULL && temp_rseg->last_page_no == FIL_NULL)) {
    TrxUndoRsegs elem(trx->no);

    if (redo_rseg != NULL && redo_rseg->last_page_no == FIL_NULL) {
      elem.push_back(redo_rseg);
    }

    if (temp_rseg != NULL && temp_rseg->last_page_no == FIL_NULL) {
      elem.push_back(temp_rseg);
    }

    mutex_enter(&purge_sys->pq_mutex);

    /* This is to reduce the pressure on the trx_sys_t::mutex
    though in reality it should make very little (read no)
    difference because this code path is only taken when the
    rbs is empty. */

    trx_sys_mutex_exit();

    purge_sys->purge_queue->push(elem);

    mutex_exit(&purge_sys->pq_mutex);
  } else {
    trx_sys_mutex_exit();
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
  if (trx->rsegs.m_redo.rseg != NULL && trx_is_redo_rseg_updated(trx)) {
    mutex_enter(&trx->rsegs.m_redo.rseg->mutex);
    own_redo_rseg_mutex = true;
  }

  mtr_t temp_mtr;

  if (trx->rsegs.m_noredo.rseg != NULL && trx_is_temp_rseg_updated(trx)) {
    mutex_enter(&trx->rsegs.m_noredo.rseg->mutex);
    own_temp_rseg_mutex = true;
    mtr_start(&temp_mtr);
    temp_mtr.set_log_mode(MTR_LOG_NO_REDO);
  }

  /* If transaction involves insert then truncate undo logs. */
  if (trx->rsegs.m_redo.insert_undo != NULL) {
    trx_undo_set_state_at_finish(trx->rsegs.m_redo.insert_undo, mtr);
  }

  if (trx->rsegs.m_noredo.insert_undo != NULL) {
    trx_undo_set_state_at_finish(trx->rsegs.m_noredo.insert_undo, &temp_mtr);
  }

  bool serialised = false;

  /* If transaction involves update then add rollback segments
  to purge queue. */
  if (trx->rsegs.m_redo.update_undo != NULL ||
      trx->rsegs.m_noredo.update_undo != NULL) {
    /* Assign the transaction serialisation number and add these
    rollback segments to purge trx-no sorted priority queue
    if this is the first UNDO log being written to assigned
    rollback segments. */

    trx_undo_ptr_t *redo_rseg_undo_ptr =
        trx->rsegs.m_redo.update_undo != NULL ? &trx->rsegs.m_redo : NULL;

    trx_undo_ptr_t *temp_rseg_undo_ptr =
        trx->rsegs.m_noredo.update_undo != NULL ? &trx->rsegs.m_noredo : NULL;

    /* Will set trx->no and will add rseg to purge queue. */
    serialised = trx_serialisation_number_get(trx, redo_rseg_undo_ptr,
                                              temp_rseg_undo_ptr);

    /* It is not necessary to obtain trx->undo_mutex here because
    only a single OS thread is allowed to do the transaction commit
    for this transaction. */
    if (trx->rsegs.m_redo.update_undo != NULL) {
      page_t *undo_hdr_page;

      undo_hdr_page =
          trx_undo_set_state_at_finish(trx->rsegs.m_redo.update_undo, mtr);

      /* Delay update of rseg_history_len if we plan to add
      non-redo update_undo too. This is to avoid immediate
      invocation of purge as we need to club these 2 segments
      with same trx-no as single unit. */
      bool update_rseg_len = !(trx->rsegs.m_noredo.update_undo != NULL);

      trx_undo_update_cleanup(trx, &trx->rsegs.m_redo, undo_hdr_page,
                              update_rseg_len, (update_rseg_len ? 1 : 0), mtr);
    }

    DBUG_EXECUTE_IF("ib_trx_crash_during_commit", DBUG_SUICIDE(););

    if (trx->rsegs.m_noredo.update_undo != NULL) {
      page_t *undo_hdr_page;

      undo_hdr_page = trx_undo_set_state_at_finish(
          trx->rsegs.m_noredo.update_undo, &temp_mtr);

      ulint n_added_logs = (redo_rseg_undo_ptr != NULL) ? 2 : 1;

      trx_undo_update_cleanup(trx, &trx->rsegs.m_noredo, undo_hdr_page, true,
                              n_added_logs, &temp_mtr);
    }
  }

  if (own_redo_rseg_mutex) {
    mutex_exit(&trx->rsegs.m_redo.rseg->mutex);
    own_redo_rseg_mutex = false;
  }

  if (own_temp_rseg_mutex) {
    mutex_exit(&trx->rsegs.m_noredo.rseg->mutex);
    own_temp_rseg_mutex = false;
    mtr_commit(&temp_mtr);
  }

  MONITOR_INC(MONITOR_TRX_COMMIT_UNDO);

  /* Update the latest MySQL binlog name and offset info
  in trx sys header if MySQL binlogging is on or the database
  server is a MySQL replication slave */

  if (trx->mysql_log_file_name != NULL && trx->mysql_log_file_name[0] != '\0') {
    trx_sys_update_mysql_binlog_offset(trx->mysql_log_file_name,
                                       trx->mysql_log_offset,
                                       TRX_SYS_MYSQL_LOG_INFO, mtr);

    trx->mysql_log_file_name = NULL;
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
    ftt->added_doc_ids = NULL;
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
  trx->fts_trx = NULL;
}

/** If required, flushes the log to disk based on the value of
 innodb_flush_log_at_trx_commit. */
static void trx_flush_log_if_needed_low(
    lsn_t lsn) /*!< in: lsn up to which logs are to be
               flushed. */
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
      /* fall through */
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
static void trx_flush_log_if_needed(
    lsn_t lsn,  /*!< in: lsn up to which logs are to be
                flushed. */
    trx_t *trx) /*!< in/out: transaction */
{
  trx->op_info = "flushing log";

  DEBUG_SYNC_C("trx_flush_log_if_needed");

  if (trx->ddl_operation || trx->ddl_must_flush) {
    log_write_up_to(*log_sys, lsn, true);
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
  time_t now = ut_time();

  trx_mod_tables_t::const_iterator end = trx->mod_tables.end();

  for (trx_mod_tables_t::const_iterator it = trx->mod_tables.begin(); it != end;
       ++it) {
    /* This could be executed by multiple threads concurrently
    on the same table object. This is fine because time_t is
    word size or less. And _purely_ _theoretically_, even if
    time_t write is not atomic, likely the value of 'now' is
    the same in all threads and even if it is not, getting a
    "garbage" in table->update_time is justified because
    protecting it with a latch here would be too performance
    intrusive. */
    (*it)->update_time = now;
  }

  trx->mod_tables.clear();
}

/**
Erase the transaction from running transaction lists and serialization
list. Active RW transaction list of a MVCC snapshot(ReadView::prepare)
won't include this transaction after this call. All implicit locks are
also released by this call as trx is removed from rw_trx_list.
@param[in] trx		Transaction to erase, must have an ID > 0
@param[in] serialised	true if serialisation log was written */
static void trx_erase_lists(trx_t *trx, bool serialised) {
  ut_ad(trx->id > 0);
  trx_sys_mutex_enter();

  if (serialised) {
    UT_LIST_REMOVE(trx_sys->serialisation_list, trx);
  }

  trx_ids_t::iterator it = std::lower_bound(trx_sys->rw_trx_ids.begin(),
                                            trx_sys->rw_trx_ids.end(), trx->id);
  ut_ad(*it == trx->id);
  trx_sys->rw_trx_ids.erase(it);

  if (trx->read_only || trx->rsegs.m_redo.rseg == NULL) {
    ut_ad(!trx->in_rw_trx_list);
  } else {
    UT_LIST_REMOVE(trx_sys->rw_trx_list, trx);
    ut_d(trx->in_rw_trx_list = false);
    ut_ad(trx_sys_validate_trx_list());

    if (trx->read_view != NULL) {
      trx_sys->mvcc->view_close(trx->read_view, true);
    }
  }

  trx_sys->rw_trx_set.erase(TrxTrack(trx->id));

  /* Set minimal active trx id. */
  trx_id_t min_id = trx_sys->rw_trx_ids.empty() ? trx_sys->max_trx_id
                                                : trx_sys->rw_trx_ids.front();

  trx_sys->min_active_id.store(min_id);

  trx_sys_mutex_exit();
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
  trx->must_flush_log_later = false;
  trx->ddl_must_flush = false;

  if (trx_is_autocommit_non_locking(trx)) {
    ut_ad(trx->id == 0);
    ut_ad(trx->read_only);
    ut_a(!trx->is_recovered);
    ut_ad(trx->rsegs.m_redo.rseg == NULL);
    ut_ad(!trx->in_rw_trx_list);

    /* Note: We are asserting without holding the lock mutex. But
    that is OK because this transaction is not waiting and cannot
    be rolled back and no new locks can (or should not) be added
    becuase it is flagged as a non-locking read-only transaction. */

    ut_a(UT_LIST_GET_LEN(trx->lock.trx_locks) == 0);

    /* This state change is not protected by any mutex, therefore
    there is an inherent race here around state transition during
    printouts. We ignore this race for the sake of efficiency.
    However, the trx_sys_t::mutex will protect the trx_t instance
    and it cannot be removed from the mysql_trx_list and freed
    without first acquiring the trx_sys_t::mutex. */

    ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));

    if (trx->read_view != NULL) {
      trx_sys->mvcc->view_close(trx->read_view, false);
    }

    MONITOR_INC(MONITOR_TRX_NL_RO_COMMIT);

    /* AC-NL-RO transactions can't be rolled back asynchronously. */
    ut_ad(!trx->abort);
    ut_ad(!(trx->in_innodb & (TRX_FORCE_ROLLBACK | TRX_FORCE_ROLLBACK_ASYNC)));

    trx->state = TRX_STATE_NOT_STARTED;

  } else {
    if (trx->id > 0) {
      /* For consistent snapshot, we need to remove current
      transaction from running transaction id list for mvcc
      before doing commit and releasing locks. */
      trx_erase_lists(trx, serialised);
    }

    lock_trx_release_locks(trx);

    /* Remove the transaction from the list of active
    transactions now that it no longer holds any user locks. */

    ut_ad(trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY));
    DEBUG_SYNC_C("after_trx_committed_in_memory");

    if (trx->read_only || trx->rsegs.m_redo.rseg == NULL) {
      MONITOR_INC(MONITOR_TRX_RO_COMMIT);
      if (trx->read_view != NULL) {
        trx_sys->mvcc->view_close(trx->read_view, false);
      }

    } else {
      ut_ad(trx->id > 0);
      MONITOR_INC(MONITOR_TRX_RW_COMMIT);
    }
  }

  if (trx->rsegs.m_redo.rseg != NULL) {
    trx_rseg_t *rseg = trx->rsegs.m_redo.rseg;
    ut_ad(rseg->trx_ref_count > 0);

    /* Multiple transactions can simultaneously decrement
    the atomic counter. */
    rseg->trx_ref_count--;
  }

  if (mtr != NULL) {
    if (trx->rsegs.m_redo.insert_undo != NULL) {
      trx_undo_insert_cleanup(&trx->rsegs.m_redo, false);
    }

    if (trx->rsegs.m_noredo.insert_undo != NULL) {
      trx_undo_insert_cleanup(&trx->rsegs.m_noredo, true);
    }

    /* NOTE that we could possibly make a group commit more
    efficient here: call os_thread_yield here to allow also other
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

  /* Free all savepoints, starting from the first. */
  trx_named_savept_t *savep = UT_LIST_GET_FIRST(trx->trx_savepoints);

  trx_roll_savepoints_free(trx, savep);

  if (trx->fts_trx != NULL) {
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
    trx->state = TRX_STATE_FORCED_ROLLBACK;
  } else {
    trx->state = TRX_STATE_NOT_STARTED;
  }

  /* trx->in_mysql_trx_list would hold between
  trx_allocate_for_mysql() and trx_free_for_mysql(). It does not
  hold for recovered transactions or system transactions. */
  assert_trx_is_free(trx);

  trx_init(trx);

  trx_mutex_exit(trx);

  ut_a(trx->error_state == DB_SUCCESS);
}

/** Commits a transaction and a mini-transaction. */
void trx_commit_low(
    trx_t *trx, /*!< in/out: transaction */
    mtr_t *mtr) /*!< in/out: mini-transaction (will be committed),
                or NULL if trx made no modifications */
{
  assert_trx_nonlocking_or_in_list(trx);
  ut_ad(!trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY));
  ut_ad(!mtr || mtr->is_active());
  /* undo_no is non-zero if we're doing the final commit. */
  if (trx->fts_trx != NULL && trx->undo_no != 0 &&
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

  if (mtr != NULL) {
    mtr->set_sync();

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
    mtr_commit(mtr);

    DBUG_EXECUTE_IF("ib_crash_during_trx_commit_in_mem",
                    if (trx_is_rseg_updated(trx)) {
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
  if (trx->mysql_thd != NULL && trx_is_redo_rseg_updated(trx)) {
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
    mtr = NULL;
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
  if (trx->rsegs.m_redo.insert_undo != NULL) {
    trx_undo_insert_cleanup(&trx->rsegs.m_redo, false);
  }

  memset(&trx->rsegs, 0x0, sizeof(trx->rsegs));
  trx->undo_no = 0;
  trx->undo_rseg_space = 0;
  trx->last_sql_stat_start.least_undo_no = 0;

  trx_sys_mutex_enter();

  ut_a(!trx->read_only);

  UT_LIST_REMOVE(trx_sys->rw_trx_list, trx);

  ut_d(trx->in_rw_trx_list = FALSE);

  trx_sys_mutex_exit();

  /* Change the transaction state without mutex protection, now
  that it no longer is in the trx_list. Recovered transactions
  are never placed in the mysql_trx_list. */
  ut_ad(trx->is_recovered);
  ut_ad(!trx->in_rw_trx_list);
  ut_ad(!trx->in_mysql_trx_list);
  trx->state = TRX_STATE_NOT_STARTED;
}

/** Assigns a read view for a consistent read query. All the consistent reads
 within the same transaction will get the same read view, which is created
 when this function is first called for a new started transaction.
 @return consistent read view */
ReadView *trx_assign_read_view(trx_t *trx) /*!< in/out: active transaction */
{
  ut_ad(trx->state == TRX_STATE_ACTIVE);

  if (srv_read_only_mode) {
    ut_ad(trx->read_view == NULL);
    return (NULL);

  } else if (!MVCC::is_view_active(trx->read_view)) {
    trx_sys->mvcc->view_open(trx->read_view, trx);
  }

  return (trx->read_view);
}

/** Prepares a transaction for commit/rollback. */
void trx_commit_or_rollback_prepare(trx_t *trx) /*!< in/out: transaction */
{
  /* We are reading trx->state without holding trx_sys->mutex
  here, because the commit or rollback should be invoked for a
  running (or recovered prepared) transaction that is associated
  with the current thread. */

  switch (trx->state) {
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:

      trx_start_low(trx, true);
      /* fall through */

    case TRX_STATE_ACTIVE:
    case TRX_STATE_PREPARED:

      /* If the trx is in a lock wait state, moves the waiting
      query thread to the suspended state */

      if (trx->lock.que_state == TRX_QUE_LOCK_WAIT) {
        ut_a(trx->lock.wait_thr != NULL);
        trx->lock.wait_thr->state = QUE_THR_SUSPENDED;
        trx->lock.wait_thr = NULL;

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

    ut_a(trx->lock.wait_thr == NULL);
    ut_a(trx->lock.que_state != TRX_QUE_LOCK_WAIT);

    trx_commit_or_rollback_prepare(trx);

    trx->lock.que_state = TRX_QUE_COMMITTING;

    trx_commit(trx);

    ut_ad(trx->lock.wait_thr == NULL);

    trx->lock.que_state = TRX_QUE_RUNNING;

    thr = NULL;
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

  if (trx_in_innodb.is_aborted() && trx->killed_by != os_thread_get_curr_id()) {
    return (DB_FORCED_ABORT);
  }

  /* Because we do not do the commit by sending an Innobase
  sig to the transaction, we must here make sure that trx has been
  started. */

  switch (trx->state) {
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:

      ut_d(trx->start_file = __FILE__);
      ut_d(trx->start_line = __LINE__);

      trx_start_low(trx, true);
      /* fall through */
    case TRX_STATE_ACTIVE:
    case TRX_STATE_PREPARED:

      trx->op_info = "committing";

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
 with trx->flush_log_later == TRUE. */
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

  switch (trx->state) {
    case TRX_STATE_PREPARED:
    case TRX_STATE_COMMITTED_IN_MEMORY:
      break;
    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:
      trx->undo_no = 0;
      trx->undo_rseg_space = 0;
      /* fall through */
    case TRX_STATE_ACTIVE:
      trx->last_sql_stat_start.least_undo_no = trx->undo_no;

      if (trx->fts_trx != NULL) {
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
  ibool newline;
  const char *op_info;

  ut_ad(trx_sys_mutex_own());

  fprintf(f, "TRANSACTION " TRX_ID_FMT, trx_get_id_for_print(trx));

  /* trx->state cannot change from or to NOT_STARTED while we
  are holding the trx_sys->mutex. It may change from ACTIVE to
  PREPARED or COMMITTED. */
  switch (trx->state) {
    case TRX_STATE_NOT_STARTED:
      fputs(", not started", f);
      goto state_ok;
    case TRX_STATE_FORCED_ROLLBACK:
      fputs(", forced rollback", f);
      goto state_ok;
    case TRX_STATE_ACTIVE:
      fprintf(f, ", ACTIVE %lu sec",
              (ulong)difftime(time(NULL), trx->start_time));
      goto state_ok;
    case TRX_STATE_PREPARED:
      fprintf(f, ", ACTIVE (PREPARED) %lu sec",
              (ulong)difftime(time(NULL), trx->start_time));
      goto state_ok;
    case TRX_STATE_COMMITTED_IN_MEMORY:
      fputs(", COMMITTED IN MEMORY", f);
      goto state_ok;
  }
  fprintf(f, ", state %lu", (ulong)trx->state);
  ut_ad(0);
state_ok:

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

  newline = TRUE;

  /* trx->lock.que_state of an ACTIVE transaction may change
  while we are not holding trx->mutex. We perform a dirty read
  for performance reasons. */

  switch (trx->lock.que_state) {
    case TRX_QUE_RUNNING:
      newline = FALSE;
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
    newline = TRUE;

    fprintf(f,
            "%lu lock struct(s), heap size %lu,"
            " %lu row lock(s)",
            (ulong)n_trx_locks, (ulong)heap_size, (ulong)n_rec_locks);
  }

  if (trx->has_search_latch) {
    newline = TRUE;
    fputs(", holds adaptive hash latch", f);
  }

  if (trx->undo_no != 0) {
    newline = TRUE;
    fprintf(f, ", undo log entries " TRX_ID_FMT, trx->undo_no);
  }

  if (newline) {
    putc('\n', f);
  }

  if (trx->state != TRX_STATE_NOT_STARTED && trx->mysql_thd != NULL) {
    innobase_mysql_print_thd(f, trx->mysql_thd,
                             static_cast<uint>(max_query_len));
  }
}

/** Prints info about a transaction.
 The caller must hold lock_sys->mutex and trx_sys->mutex.
 When possible, use trx_print() instead. */
void trx_print_latched(
    FILE *f,             /*!< in: output stream */
    const trx_t *trx,    /*!< in: transaction */
    ulint max_query_len) /*!< in: max query length to print,
                         or 0 to use the default max length */
{
  ut_ad(lock_mutex_own());
  ut_ad(trx_sys_mutex_own());

  trx_print_low(f, trx, max_query_len, lock_number_of_rows_locked(&trx->lock),
                UT_LIST_GET_LEN(trx->lock.trx_locks),
                mem_heap_get_size(trx->lock.lock_heap));
}

/** Prints info about a transaction.
 Acquires and releases lock_sys->mutex and trx_sys->mutex. */
void trx_print(FILE *f,             /*!< in: output stream */
               const trx_t *trx,    /*!< in: transaction */
               ulint max_query_len) /*!< in: max query length to print,
                                    or 0 to use the default max length */
{
  ulint n_rec_locks;
  ulint n_trx_locks;
  ulint heap_size;

  lock_mutex_enter();
  n_rec_locks = lock_number_of_rows_locked(&trx->lock);
  n_trx_locks = UT_LIST_GET_LEN(trx->lock.trx_locks);
  heap_size = mem_heap_get_size(trx->lock.lock_heap);
  lock_mutex_exit();

  mutex_enter(&trx_sys->mutex);

  trx_print_low(f, trx, max_query_len, n_rec_locks, n_trx_locks, heap_size);

  mutex_exit(&trx_sys->mutex);
}

#ifdef UNIV_DEBUG
/** Asserts that a transaction has been started.
 The caller must hold trx_sys->mutex.
 @return true if started */
ibool trx_assert_started(const trx_t *trx) /*!< in: transaction */
{
  ut_ad(trx_sys_mutex_own());

  /* Non-locking autocommits should not hold any locks and this
  function is only called from the locking code. */
  check_trx_state(trx);

  /* trx->state can change from or to NOT_STARTED while we are holding
  trx_sys->mutex for non-locking autocommit selects but not for other
  types of transactions. It may change from ACTIVE to PREPARED. Unless
  we are holding lock_sys->mutex, it may also change to COMMITTED. */

  switch (trx->state) {
    case TRX_STATE_PREPARED:
      return (TRUE);

    case TRX_STATE_ACTIVE:
    case TRX_STATE_COMMITTED_IN_MEMORY:
      return (TRUE);

    case TRX_STATE_NOT_STARTED:
    case TRX_STATE_FORCED_ROLLBACK:
      break;
  }

  ut_error;
}
#endif /* UNIV_DEBUG */

/** Compares the "weight" (or size) of two transactions. Transactions that
 have edited non-transactional tables are considered heavier than ones
 that have not.
 @return true if weight(a) >= weight(b) */
bool trx_weight_ge(const trx_t *a, /*!< in: transaction to be compared */
                   const trx_t *b) /*!< in: transaction to be compared */
{
  ibool a_notrans_edit;
  ibool b_notrans_edit;

  /* If mysql_thd is NULL for a transaction we assume that it has
  not edited non-transactional tables. */

  a_notrans_edit =
      a->mysql_thd != NULL && thd_has_edited_nontrans_tables(a->mysql_thd);

  b_notrans_edit =
      b->mysql_thd != NULL && thd_has_edited_nontrans_tables(b->mysql_thd);

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
  if (undo_ptr->insert_undo != NULL || undo_ptr->update_undo != NULL) {
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

    mutex_enter(&rseg->mutex);

    if (undo_ptr->insert_undo != NULL) {
      /* It is not necessary to obtain trx->undo_mutex here
      because only a single OS thread is allowed to do the
      transaction prepare for this transaction. */
      trx_undo_set_state_at_prepare(trx, undo_ptr->insert_undo, false, &mtr);
    }

    if (undo_ptr->update_undo != NULL) {
      trx_undo_set_state_at_prepare(trx, undo_ptr->update_undo, false, &mtr);
    }

    mutex_exit(&rseg->mutex);

    /*--------------*/
    /* This mtr commit makes the transaction prepared in
    file-based world. */
    mtr_commit(&mtr);
    /*--------------*/

    if (!noredo_logging) {
      const lsn_t lsn = mtr.commit_lsn();
      ut_ad(lsn > 0);
      return lsn;
    }
  }

  return 0;
}

/** Prepares a transaction. */
static void trx_prepare(trx_t *trx) /*!< in/out: transaction */
{
  /* This transaction has crossed the point of no return and cannot
  be rolled back asynchronously now. It must commit or rollback
  synhronously. */

  lsn_t lsn = 0;

  /* Only fresh user transactions can be prepared.
  Recovered transactions cannot. */
  ut_a(!trx->is_recovered);

  DBUG_EXECUTE_IF("ib_trx_crash_during_xa_prepare_step", DBUG_SUICIDE(););

  if (trx->rsegs.m_redo.rseg != NULL && trx_is_redo_rseg_updated(trx)) {
    lsn = trx_prepare_low(trx, &trx->rsegs.m_redo, false);
  }

  if (trx->rsegs.m_noredo.rseg != NULL && trx_is_temp_rseg_updated(trx)) {
    trx_prepare_low(trx, &trx->rsegs.m_noredo, true);
  }

  /*--------------------------------------*/
  ut_a(trx->state == TRX_STATE_ACTIVE);
  trx_sys_mutex_enter();
  trx->state = TRX_STATE_PREPARED;
  trx_sys->n_prepared_trx++;
  trx_sys_mutex_exit();
  /*--------------------------------------*/

  /* Force isolation level to RC and release GAP locks
  for test purpose. */
  DBUG_EXECUTE_IF("ib_force_release_gap_lock_prepare",
                  trx->isolation_level = TRX_ISO_READ_COMMITTED;);

  /* Release read locks after PREPARE for READ COMMITTED
  and lower isolation. */
  if (trx->isolation_level <= TRX_ISO_READ_COMMITTED) {
    /* Stop inheriting GAP locks. */
    trx->skip_lock_inheritance = true;

    /* Release only GAP locks for now. */
    lock_trx_release_read_locks(trx, true);
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
      if (lsn == 0) {
        break;
      }
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

/**
Does the transaction prepare for MySQL.
@param[in, out] trx		Transaction instance to prepare */
dberr_t trx_prepare_for_mysql(trx_t *trx) {
  trx_start_if_not_started_xa(trx, false);

  TrxInInnoDB trx_in_innodb(trx, true);

  if (trx_in_innodb.is_aborted() && trx->killed_by != os_thread_get_curr_id()) {
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
  const char *ptr;

  size_t len = dict_get_db_name_len(dd_table->name.m_name);
  table->db = strmake_root(mem_root, dd_table->name.m_name, len);
  if (table->db == nullptr) return true;

  ptr = dict_remove_db_name(dd_table->name.m_name);
  len = ut_strlen(ptr);
  table->tablename = strmake_root(mem_root, ptr, len);
  if (table->tablename == nullptr) return true;

  return false;
}

/**
  Get prepared transaction info from InnoDB data structure.

  @param[in,out]  txn_list  Handler layer tansaction list.
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
  const trx_t *trx;
  ulint count = 0;

  ut_ad(txn_list);
  ut_ad(len);

  /* We should set those transactions which are in the prepared state
  to the xid_list */

  trx_sys_mutex_enter();

  for (trx = UT_LIST_GET_FIRST(trx_sys->rw_trx_list); trx != NULL;
       trx = UT_LIST_GET_NEXT(trx_list, trx)) {
    assert_trx_in_rw_list(trx);

    /* The state of a read-write transaction cannot change
    from or to NOT_STARTED while we are holding the
    trx_sys->mutex. It may change to PREPARED, but not if
    trx->is_recovered. It may also change to COMMITTED. */
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

/** This function is used to find one X/Open XA distributed transaction
 which is in the prepared state
 @return trx on match, the trx->xid will be invalidated;
 note that the trx may have been committed, unless the caller is
 holding lock_sys->mutex */
static MY_ATTRIBUTE((warn_unused_result)) trx_t *trx_get_trx_by_xid_low(
    const XID *xid) /*!< in: X/Open XA transaction
                    identifier */
{
  trx_t *trx;

  ut_ad(trx_sys_mutex_own());

  for (trx = UT_LIST_GET_FIRST(trx_sys->rw_trx_list); trx != NULL;
       trx = UT_LIST_GET_NEXT(trx_list, trx)) {
    assert_trx_in_rw_list(trx);

    /* Compare two X/Open XA transaction id's: their
    length should be the same and binary comparison
    of gtrid_length+bqual_length bytes should be
    the same */

    if (trx->is_recovered && trx_state_eq(trx, TRX_STATE_PREPARED) &&
        xid->eq(trx->xid)) {
      /* Invalidate the XID, so that subsequent calls
      will not find it. */
      trx->xid->reset();
      break;
    }
  }

  return (trx);
}

/** This function is used to find one X/Open XA distributed transaction
 which is in the prepared state
 @return trx or NULL; on match, the trx->xid will be invalidated;
 note that the trx may have been committed, unless the caller is
 holding lock_sys->mutex */
trx_t *trx_get_trx_by_xid(
    const XID *xid) /*!< in: X/Open XA transaction identifier */
{
  trx_t *trx;

  if (xid == NULL) {
    return (NULL);
  }

  trx_sys_mutex_enter();

  /* Recovered/Resurrected transactions are always only on the
  trx_sys_t::rw_trx_list. */
  trx = trx_get_trx_by_xid_low(xid);

  trx_sys_mutex_exit();

  return (trx);
}

/** Starts the transaction if it is not yet started. */
void trx_start_if_not_started_xa_low(
    trx_t *trx,      /*!< in/out: transaction */
    bool read_write) /*!< in: true if read write transaction */
{
  switch (trx->state) {
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

/** Starts the transaction if it is not yet started. */
void trx_start_if_not_started_low(
    trx_t *trx,      /*!< in: transaction */
    bool read_write) /*!< in: true if read write transaction */
{
  switch (trx->state) {
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
@param[in,out] trx	transaction to be started */
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
  ut_ad(trx->rsegs.m_redo.rseg == 0);
  ut_ad(!trx->in_rw_trx_list);
  ut_ad(!trx_is_autocommit_non_locking(trx));
  ut_ad(!trx->read_only);

  if (srv_force_recovery >= SRV_FORCE_NO_TRX_UNDO) {
    return;
  }

  /* Function is promoting existing trx from ro mode to rw mode.
  In this process it has acquired trx_sys->mutex as it plan to
  move trx from ro list to rw list. If in future, some other thread
  looks at this trx object while it is being promoted then ensure
  that both threads are synced by acquring trx->mutex to avoid decision
  based on in-consistent view formed during promotion. */

  trx_assign_rseg_durable(trx);

  ut_ad(trx->rsegs.m_redo.rseg != 0);

  mutex_enter(&trx_sys->mutex);

  ut_ad(trx->id == 0);
  trx->id = trx_sys_get_new_trx_id();

  trx_sys->rw_trx_ids.push_back(trx->id);

  trx_sys->rw_trx_set.insert(TrxTrack(trx->id, trx));

  /* So that we can see our own changes. */
  if (MVCC::is_view_active(trx->read_view)) {
    MVCC::set_view_creator_trx_id(trx->read_view, trx->id);
  }

#ifdef UNIV_DEBUG
  if (trx->id > trx_sys->rw_max_trx_id) {
    trx_sys->rw_max_trx_id = trx->id;
  }
#endif /* UNIV_DEBUG */

  UT_LIST_ADD_FIRST(trx_sys->rw_trx_list, trx);

  ut_d(trx->in_rw_trx_list = true);

  mutex_exit(&trx_sys->mutex);
}

/**
Kill all transactions that are blocking this transaction from acquiring locks.
@param[in,out] trx	High priority transaction */

void trx_kill_blocking(trx_t *trx) {
  if (trx->hit_list.empty()) {
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
  hit_list_t::reverse_iterator end = trx->hit_list.rend();

  for (hit_list_t::reverse_iterator it = trx->hit_list.rbegin(); it != end;
       ++it) {
    trx_t *victim_trx = it->m_trx;
    ulint version = it->m_version;

    /* Shouldn't commit suicide. */
    ut_ad(victim_trx != trx);
    ut_ad(victim_trx->mysql_thd != trx->mysql_thd);

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

      os_thread_sleep(sleep_time);

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
    ut_ad(victim_trx->mysql_thd != NULL);

    ut_ad(!(trx->in_innodb & TRX_FORCE_ROLLBACK_DISABLE));
    ut_ad(victim_trx->in_innodb & TRX_FORCE_ROLLBACK);
    ut_ad(victim_trx->in_innodb & TRX_FORCE_ROLLBACK_ASYNC);
    ut_ad(victim_trx->killed_by == os_thread_get_curr_id());
    ut_ad(victim_trx->version == it->m_version);

    /* We don't kill Read Only, Background or high priority
    transactions. */
    ut_a(!victim_trx->read_only);
    ut_a(victim_trx->mysql_thd != NULL);

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
    ib::info(ER_IB_MSG_1211)
        << "High Priority Transaction (ID): " << trx->id
        << " killed transaction (ID): " << id << " in hit list"
        << " - " << thr_text;
#endif /* UNIV_DEBUG */
    trx_mutex_enter(victim_trx);

    version++;
    ut_ad(victim_trx->version == version);

    os_thread_id_t thread_id = victim_trx->killed_by;
    os_compare_and_swap_thread_id(&victim_trx->killed_by, thread_id, 0);

    victim_trx->in_innodb &= TRX_FORCE_ROLLBACK_MASK;

    trx_mutex_exit(victim_trx);
  }

  trx->hit_list.clear();

  if (had_dict_lock) {
    row_mysql_freeze_data_dictionary(trx);
  }
}
