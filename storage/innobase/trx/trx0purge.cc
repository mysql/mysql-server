/*****************************************************************************

Copyright (c) 1996, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file trx/trx0purge.cc
 Purge old versions

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#include <sys/types.h>
#include <algorithm>
#include <atomic>
#include <new>
#include <unordered_map>

#include "clone0api.h"
#include "clone0clone.h"
#include "debug_sync.h" /* CONDITIONAL_SYNC_POINT */
#include "dict0dd.h"
#include "fil0fil.h"
#include "fsp0fsp.h"
#include "fsp0sysspace.h"
#include "fsp0types.h"
#include "fut0fut.h"
#include "ha_prototypes.h"
#include "log0chkp.h"
#include "log0helpers.h"
#include "mach0data.h"
#include "mtr0log.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "os0thread.h"
#include "que0que.h"
#include "read0mvcc_interface.h"
#include "read0read_view_interface.h"
#include "row0purge.h"
#include "row0upd.h"
#include "srv0mon.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "sync0sync.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "trx0roll.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "ut0math.h"

/** Maximum allowable purge history length.  <=0 means 'infinite'. */
ulong srv_max_purge_lag = 0;

/** Max DML user threads delay in micro-seconds. */
ulong srv_max_purge_lag_delay = 0;

/** Whether the purge history length is above innodb_max_purge_lag. */
static std::atomic_bool srv_max_purge_lag_exceeded{false};

/** Whether DML statements are being delayed due to purge lag. */
static std::atomic_bool srv_dml_purge_delay_active{false};

/** The global data structure coordinating a purge */
trx_purge_t *purge_sys = nullptr;

/** Wait for a short delay between checks. */
#ifdef UNIV_DEBUG
static constexpr int64_t PURGE_CHECK_UNDO_TRUNCATE_DELAY_IN_MS = 10;
#else
static constexpr int64_t PURGE_CHECK_UNDO_TRUNCATE_DELAY_IN_MS = 1000;
#endif /* UNIV_DEBUG */

#ifdef UNIV_DEBUG
bool srv_purge_view_update_only_debug;
#endif /* UNIV_DEBUG */

/** Sentinel value */
const TrxUndoRsegs TrxUndoRsegsIterator::NullElement(UINT64_UNDEFINED);

/** A sentinel undo record used as a return value when we have a whole
undo log which can be skipped by purge */
static trx_undo_rec_t trx_purge_ignore_rec;

/** Constructor */
TrxUndoRsegsIterator::TrxUndoRsegsIterator(trx_purge_t *purge_sys)
    : m_purge_sys(purge_sys),
      m_trx_undo_rsegs(NullElement),
      m_iter(m_trx_undo_rsegs.end()) {}

/** Sets the next rseg to purge in m_purge_sys.
@return page size of the table for which the log is.
NOTE: if rseg is NULL when this function returns this means that
there are no rollback segments to purge and then the returned page
size object should not be used. */
const page_size_t TrxUndoRsegsIterator::set_next() {
  mutex_enter(&m_purge_sys->pq_mutex);

  /* Only purge consumes events from the priority queue, user
  threads only produce the events. */

  /* Check if there are more rsegs to process in the
  current element. */
  if (m_iter != m_trx_undo_rsegs.end()) {
    /* We are still processing rollback segment from
    the same transaction and so expected transaction
    number shouldn't increase. Undo increment of
    expected trx_no done by caller assuming rollback
    segments from given transaction are done. */
    m_purge_sys->iter.trx_no = (*m_iter)->last_trx_no;

  } else if (!m_purge_sys->purge_queue->empty()) {
    /* Read the next element from the queue.
    Combine elements if they have same transaction number.
    This can happen if a transaction shares redo rollback segment
    with another transaction that has already added it to purge
    queue and former transaction also needs to schedule non-redo
    rollback segment for purge. */
    m_trx_undo_rsegs = NullElement;

    while (!m_purge_sys->purge_queue->empty()) {
      if (m_trx_undo_rsegs.get_trx_no() == UINT64_UNDEFINED) {
        m_trx_undo_rsegs = purge_sys->purge_queue->top();
      } else if (purge_sys->purge_queue->top().get_trx_no() ==
                 m_trx_undo_rsegs.get_trx_no()) {
        m_trx_undo_rsegs.insert(purge_sys->purge_queue->top());
      } else {
        break;
      }

      m_purge_sys->purge_queue->pop();
    }

    m_iter = m_trx_undo_rsegs.begin();

  } else {
    /* Queue is empty, reset iterator. */
    m_trx_undo_rsegs = NullElement;
    m_iter = m_trx_undo_rsegs.end();

    mutex_exit(&m_purge_sys->pq_mutex);

    m_purge_sys->rseg = nullptr;

    /* return a dummy object, not going to be used by the caller */
    return (univ_page_size);
  }

  m_purge_sys->rseg = *m_iter++;

  mutex_exit(&m_purge_sys->pq_mutex);

  ut_a(m_purge_sys->rseg != nullptr);

  m_purge_sys->rseg->latch();

  ut_a(m_purge_sys->rseg->last_page_no != FIL_NULL);
  ut_ad(m_purge_sys->rseg->last_trx_no == m_trx_undo_rsegs.get_trx_no());

  /* The space_id must be a tablespace that contains rollback segments.
  That includes the system, temporary and all undo tablespaces. */
  ut_a(fsp_is_system_or_temp_tablespace(m_purge_sys->rseg->space_id) ||
       fsp_is_undo_tablespace(m_purge_sys->rseg->space_id));

  const page_size_t page_size(m_purge_sys->rseg->page_size);

  ut_a(purge_sys->iter.trx_no <= purge_sys->rseg->last_trx_no);

  m_purge_sys->iter.trx_no = m_purge_sys->rseg->last_trx_no;
  m_purge_sys->hdr_offset = m_purge_sys->rseg->last_offset;
  m_purge_sys->hdr_page_no = m_purge_sys->rseg->last_page_no;

  m_purge_sys->rseg->unlatch();

  return (page_size);
}

/** Builds a purge 'query' graph. The actual purge is performed by executing
this query graph.
@param[in]   trx               transaction
@param[in]   n_purge_threads   number of purge threads
@return own: the query graph */
static que_t *trx_purge_graph_build(trx_t *trx, ulint n_purge_threads) {
  ulint i;
  mem_heap_t *heap;
  que_fork_t *fork;

  heap = mem_heap_create(512, UT_LOCATION_HERE);
  fork = que_fork_create(nullptr, nullptr, QUE_FORK_PURGE, heap);
  fork->trx = trx;

  for (i = 0; i < n_purge_threads; ++i) {
    que_thr_t *thr;

    thr = que_thr_create(fork, heap, nullptr);

    thr->child = row_purge_node_create(thr, heap);
  }

  return (fork);
}

void trx_purge_sys_mem_create() {
  purge_sys = static_cast<trx_purge_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*purge_sys)));

  purge_sys->state = PURGE_STATE_INIT;
  purge_sys->event = os_event_create();

  new (&purge_sys->iter) purge_iter_t;
  new (&purge_sys->limit) purge_iter_t;
  new (&purge_sys->undo_trunc) undo_truncate::Truncate;
  new (&purge_sys->rsegs_queue) std::vector<trx_rseg_t *>;
#ifdef UNIV_DEBUG
  new (&purge_sys->done) purge_iter_t;
#endif /* UNIV_DEBUG */

  rw_lock_create(trx_purge_latch_key, &purge_sys->latch, LATCH_ID_TRX_PURGE);

  mutex_create(LATCH_ID_PURGE_SYS_PQ, &purge_sys->pq_mutex);

  purge_sys->heap = mem_heap_create(8 * 1024, UT_LOCATION_HERE);
}

/** Updates the purge_sys->view and purge_sys->m_lowest_needed_trx_no, to the
safe, most current estimates which are based on oldest currently open read view
and progress of GTID persistor.
Because the value of lowest needed transaction number returned by
Clone_persist_gtid::get_oldest_trx_no() is not monotone over time, the
m_lowest_needed_trx_no may sometimes get smaller, perhaps smaller than already
purged trx no - when this happens it doesn't mean that we've lost important data
as the value returned by GTID persistor in such cases is lower than really
needed.
The estimates should eventually converge on the right value if system is idle.
This function is called at startup and then periodically by purge thread, to
learn how much can be purged - which is limited by m_lowest_needed_trx_no.
Note that m_lowest_needed_trx_no might be lower than needed by purge_sys->view
in case GTID persistor is lagging. */
static void trx_purge_update_oldest_needed() {
  rw_lock_x_lock(&purge_sys->latch, UT_LOCATION_HERE);

  trx_sys->mvcc->clone_oldest_view(purge_sys->view);
  ut_a(purge_sys->view != nullptr);
  /* The purge_sys->view should be a *clone* of an open read view, but itself it
  should not be considered open, so it doesn't participate in competition for
  the oldest open read view in clone_oldest_view() when we call it next time.
  We'd like to assert ut_a(!trx_sys->mvcc->is_view_open(purge_sys->view)) here,
  but the is_view_open(..) can not be called on pointers obtained from
  clone_oldest_view(), because MVCC::is_view_open() uses information encoded in
  the lowest bit of a pointer by view_open(..) and view_close(..), which only
  works for trx->read_view fields. */
  const auto needed_by_purge_view = purge_sys->view->get_lowest_needed_trx_no();
  /* The Clone_persist_gtid::get_oldest_trx_no() can return TRX_ID_MAX, if there
  are no GTIDs pending to be persisted. It is crucial for correctness, that
  get_oldest_trx_no() must be called after the oldest view was cloned, so that
  in case it returns TRX_ID_MAX, we properly cap it to no more than
  trx_sys->serialisation_min_trx_no seen at an earlier moment (which is an upper
  bound for purge_sys->view->get_lowest_needed_trx_no() on the one hand, and at
  the same time a lower bound for trx->no of any trx assigned a new GTID since
  then and at the same time ). If we do that in opposite order, it could be the
  case that new GTIDs were assigned after the get_oldest_trx_no() has returned
  TRX_ID_MAX and we clone an oldest read view even later and thus decide to
  remove all Undo Logs not needed by this read view, including the Undo Log
  Header which stored the newly assigned GTID, before it is persisted.*/
  const auto needed_by_persistor =
      clone_sys->get_gtid_persistor().get_oldest_trx_no();
  purge_sys->m_lowest_needed_trx_no =
      std::min(needed_by_purge_view, needed_by_persistor);

  rw_lock_x_unlock(&purge_sys->latch);
}

void trx_purge_sys_initialize(uint32_t n_purge_threads,
                              purge_pq_t *purge_queue) {
  /* Take ownership of purge_queue, we are responsible for freeing it. */
  purge_sys->purge_queue = purge_queue;

  ut_a(n_purge_threads > 0);

  purge_sys->sess = sess_open();

  purge_sys->trx = purge_sys->sess->trx;

  ut_a(purge_sys->trx->sess == purge_sys->sess);

  /* A purge transaction is not a real transaction, we use a transaction
  here only because the query threads code requires it. It is otherwise
  quite unnecessary. We should get rid of it eventually. */
  purge_sys->trx->id = 0;
  purge_sys->trx->start_time.store(std::chrono::system_clock::now(),
                                   std::memory_order_relaxed);
  purge_sys->trx->state.store(TRX_STATE_ACTIVE, std::memory_order_relaxed);
  purge_sys->trx->op_info = "purge trx";
  purge_sys->trx->purge_sys_trx = true;

  purge_sys->query = trx_purge_graph_build(purge_sys->trx, n_purge_threads);

  trx_sys->mvcc->undo_purge_is_starting();

  ut_a(purge_sys->view == nullptr);
  trx_purge_update_oldest_needed();

  purge_sys->rseg_iter = ut::new_withkey<TrxUndoRsegsIterator>(
      UT_NEW_THIS_FILE_PSI_KEY, purge_sys);
}

void trx_purge_sys_close() {
  que_graph_free(purge_sys->query);

  ut_a(purge_sys->trx->id == 0);
  ut_a(purge_sys->sess->trx == purge_sys->trx);

  purge_sys->trx->state.store(TRX_STATE_NOT_STARTED, std::memory_order_relaxed);

  sess_close(purge_sys->sess);

  purge_sys->sess = nullptr;

  trx_sys->mvcc->view_free(purge_sys->view);

  /* The MVCC will not be used by the Undo Purge anymore, any Undo-related
  support in the MVCC can be cleaned up now, as the trx_sys will be
  deinitialized soon after this call. */
  trx_sys->mvcc->undo_purge_has_shutdown();

  rw_lock_free(&purge_sys->latch);
  mutex_free(&purge_sys->pq_mutex);

  if (purge_sys->purge_queue != nullptr) {
    ut::delete_(purge_sys->purge_queue);
    purge_sys->purge_queue = nullptr;
  }

  os_event_destroy(purge_sys->event);

  purge_sys->event = nullptr;

  mem_heap_free(purge_sys->heap);

  purge_sys->heap = nullptr;

  ut::delete_(purge_sys->rseg_iter);

  call_destructor(&purge_sys->undo_trunc);
  call_destructor(&purge_sys->rsegs_queue);

  ut::free(purge_sys);

  purge_sys = nullptr;
}

/*================ UNDO LOG HISTORY LIST =============================*/

/** Adds the update undo log as the first log in the history list. Removes the
 update undo log segment from the rseg slot if it is too big for reuse. */
void trx_purge_add_update_undo_to_history(
    trx_t *trx,               /*!< in: transaction */
    trx_undo_ptr_t *undo_ptr, /*!< in/out: update undo log. */
    page_t *undo_page,        /*!< in: update undo log header page,
                              x-latched */
    bool update_rseg_history_len,
    /*!< in: if true: update rseg history
    len else skip updating it. */
    ulint n_added_logs, /*!< in: number of logs added */
    mtr_t *mtr)         /*!< in: mtr */
{
  trx_undo_t *undo;
  trx_rseg_t *rseg;
  trx_rsegf_t *rseg_header;
  trx_ulogf_t *undo_header;

  undo = undo_ptr->update_undo;
  rseg = undo->rseg;

  rseg_header = trx_rsegf_get(undo->rseg->space_id, undo->rseg->page_no,
                              undo->rseg->page_size, mtr);

  undo_header = undo_page + undo->hdr_offset;

  if (undo->state != TRX_UNDO_CACHED) {
    ulint hist_size;
#ifdef UNIV_DEBUG
    trx_usegf_t *seg_header = undo_page + TRX_UNDO_SEG_HDR;
#endif /* UNIV_DEBUG */

    /* The undo log segment will not be reused */

    if (UNIV_UNLIKELY(undo->id >= TRX_RSEG_N_SLOTS)) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_1165) << "undo->id is " << undo->id;
    }

    trx_rsegf_set_nth_undo(rseg_header, undo->id, FIL_NULL, mtr);

    MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_USED);

    hist_size =
        mtr_read_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, mtr);

    ut_ad(undo->size == flst_get_len(seg_header + TRX_UNDO_PAGE_LIST));

    mlog_write_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
                     hist_size + undo->size, MLOG_4BYTES, mtr);
  }

  /* Add the log as the first in the history list */
  flst_add_first(rseg_header + TRX_RSEG_HISTORY,
                 undo_header + TRX_UNDO_HISTORY_NODE, mtr);

  if (update_rseg_history_len) {
    trx_sys->rseg_history_len.fetch_add(n_added_logs);
    if (trx_sys->rseg_history_len.load() >
        srv_n_purge_threads * srv_purge_batch_size) {
      srv_wake_purge_thread_if_not_active();
    }
  }

  /* Update maximum transaction number for this rollback segment. */
  mlog_write_ull(rseg_header + TRX_RSEG_MAX_TRX_NO, trx->no, mtr);

  /* Write the trx number to the undo log header */
  mlog_write_ull(undo_header + TRX_UNDO_TRX_NO, trx->no, mtr);

  /* Write information about delete markings to the undo log header */

  if (!undo->del_marks) {
    mlog_write_ulint(undo_header + TRX_UNDO_DEL_MARKS, false, MLOG_2BYTES, mtr);
  }

  /* Write GTID information if there. */
  trx_undo_gtid_write(trx, undo_header, undo, mtr, false);

  if (rseg->last_page_no == FIL_NULL) {
    rseg->last_page_no = undo->hdr_page_no;
    rseg->last_offset = undo->hdr_offset;
    rseg->last_trx_no = trx->no;
    rseg->last_del_marks = undo->del_marks;
  }
}

/** Remove an rseg header from the history list.
@param[in,out]  rseg_hdr        Rollback segment header
@param[in]      log_hdr         Undo log segment header
@param[in,out]  mtr             Mini-transaction. */
static void trx_purge_remove_log_hdr(trx_rsegf_t *rseg_hdr,
                                     trx_ulogf_t *log_hdr, mtr_t *mtr) {
  flst_remove(rseg_hdr + TRX_RSEG_HISTORY, log_hdr + TRX_UNDO_HISTORY_NODE,
              mtr);

  trx_sys->rseg_history_len.fetch_sub(1);
}

/** Frees a rollback segment which is in the history list.
Removes the rseg hdr from the history list.
@param[in,out]  rseg            rollback segment
@param[in]      hdr_addr        file address of log_hdr
@param[in]      noredo          skip redo logging. */
static void trx_purge_free_segment(trx_rseg_t *rseg, fil_addr_t hdr_addr,
                                   bool noredo) {
  mtr_t mtr;
  trx_rsegf_t *rseg_hdr;
  trx_ulogf_t *log_hdr;
  trx_usegf_t *seg_hdr;
  ulint seg_size;
  ulint hist_size;
  bool marked = noredo;

  for (;;) {
    page_t *undo_page;

    mtr_start(&mtr);

    if (noredo) {
      mtr.set_log_mode(MTR_LOG_NO_REDO);
    }

    rseg->latch();

    rseg_hdr =
        trx_rsegf_get(rseg->space_id, rseg->page_no, rseg->page_size, &mtr);

    undo_page = trx_undo_page_get(page_id_t(rseg->space_id, hdr_addr.page),
                                  rseg->page_size, &mtr);

    seg_hdr = undo_page + TRX_UNDO_SEG_HDR;
    log_hdr = undo_page + hdr_addr.boffset;

    /* Mark the last undo log totally purged, so that if the
    system crashes, the tail of the undo log will not get accessed
    again. The list of pages in the undo log tail gets inconsistent
    during the freeing of the segment, and therefore purge should
    not try to access them again. */

    if (!marked) {
      marked = true;
      mlog_write_ulint(log_hdr + TRX_UNDO_DEL_MARKS, false, MLOG_2BYTES, &mtr);
    }

    if (fseg_free_step_not_header(seg_hdr + TRX_UNDO_FSEG_HEADER, false,
                                  &mtr)) {
      break;
    }

    rseg->unlatch();

    mtr_commit(&mtr);
  }

  /* The page list may now be inconsistent, but the length field
  stored in the list base node tells us how big it was before we
  started the freeing. */

  seg_size = flst_get_len(seg_hdr + TRX_UNDO_PAGE_LIST);

  /* We may free the undo log segment header page; it must be freed
  within the same mtr as the undo log header is removed from the
  history list: otherwise, in case of a database crash, the segment
  could become inaccessible garbage in the file space. */

  trx_purge_remove_log_hdr(rseg_hdr, log_hdr, &mtr);

  do {
    /* Here we assume that a file segment with just the header
    page can be freed in a few steps, so that the buffer pool
    is not flooded with bufferfixed pages: see the note in
    fsp0fsp.cc. */

  } while (!fseg_free_step(seg_hdr + TRX_UNDO_FSEG_HEADER, false, &mtr));

  hist_size =
      mtr_read_ulint(rseg_hdr + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, &mtr);
  ut_ad(hist_size >= seg_size);

  mlog_write_ulint(rseg_hdr + TRX_RSEG_HISTORY_SIZE, hist_size - seg_size,
                   MLOG_4BYTES, &mtr);

  rseg->decr_curr_size(seg_size);
  rseg->unlatch();
  mtr_commit(&mtr);
}

/** Removes unnecessary history data from a rollback segment. */
static void trx_purge_truncate_rseg_history(
    trx_rseg_t *rseg,          /*!< in: rollback segment */
    const purge_iter_t *limit) /*!< in: truncate offset */
{
  fil_addr_t hdr_addr;
  fil_addr_t prev_hdr_addr;
  trx_rsegf_t *rseg_hdr;
  page_t *undo_page;
  trx_ulogf_t *log_hdr;
  trx_usegf_t *seg_hdr;
  mtr_t mtr;
  trx_id_t undo_trx_no;
  const bool is_temp = fsp_is_system_temporary(rseg->space_id);

  mtr_start(&mtr);

  if (is_temp) {
    mtr.set_log_mode(MTR_LOG_NO_REDO);
  }

  rseg->latch();

  rseg_hdr =
      trx_rsegf_get(rseg->space_id, rseg->page_no, rseg->page_size, &mtr);

  hdr_addr = trx_purge_get_log_from_hist(
      flst_get_last(rseg_hdr + TRX_RSEG_HISTORY, &mtr));
loop:
  if (hdr_addr.page == FIL_NULL) {
    rseg->unlatch();
    mtr_commit(&mtr);

    return;
  }

  undo_page = trx_undo_page_get(page_id_t(rseg->space_id, hdr_addr.page),
                                rseg->page_size, &mtr);

  log_hdr = undo_page + hdr_addr.boffset;

  undo_trx_no = mach_read_from_8(log_hdr + TRX_UNDO_TRX_NO);

  if (undo_trx_no >= limit->trx_no) {
    /* limit space_id should match the rollback segment
    space id to avoid freeing if the page belongs to a
    different rollback segment for the same trx_no. */
    if (undo_trx_no == limit->trx_no &&
        rseg->space_id == limit->undo_rseg_space) {
      trx_undo_truncate_start(rseg, hdr_addr.page, hdr_addr.boffset,
                              limit->undo_no);
    }

    rseg->unlatch();
    mtr_commit(&mtr);

    return;
  }

  prev_hdr_addr = trx_purge_get_log_from_hist(
      flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE, &mtr));

  seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

  if ((mach_read_from_2(seg_hdr + TRX_UNDO_STATE) == TRX_UNDO_TO_PURGE) &&
      (mach_read_from_2(log_hdr + TRX_UNDO_NEXT_LOG) == 0)) {
    /* We can free the whole log segment */

    rseg->unlatch();
    mtr_commit(&mtr);

    /* calls the trx_purge_remove_log_hdr()
    inside trx_purge_free_segment(). */
    trx_purge_free_segment(rseg, hdr_addr, is_temp);

  } else {
    /* Remove the log hdr from the rseg history. */

    trx_purge_remove_log_hdr(rseg_hdr, log_hdr, &mtr);

    rseg->unlatch();
    mtr_commit(&mtr);
  }

  mtr_start(&mtr);

  if (is_temp) {
    mtr.set_log_mode(MTR_LOG_NO_REDO);
  }

  rseg->latch();

  rseg_hdr =
      trx_rsegf_get(rseg->space_id, rseg->page_no, rseg->page_size, &mtr);

  hdr_addr = prev_hdr_addr;

  goto loop;
}

/** Decide if an undo truncation needs to be done at this time. If an undo
tablespace is already marked, return that so that this truncation will get
finished. If none is marked, iterate over all the UNDO tablespaces and check
if any qualify to be truncated.
  Normal operation; Choose the marked space.
                    If none are marked, choose an explicitly inactive space.
                    If none exist, check if conditions allow implicit
                       truncation.
                    If conditions allow, choose an implicitly inactive space.
                    If none exist and no space has been truncated yet,
                       look for an undo space that is too big.
  Fast shutdown;    Do not truncate.  This routine is not called.
  Slow shutdown;    Choose the marked space.
                    If none are marked, choose an explicitly inactive
                       space.
                    If none exist, check if conditions allow implicit
                       truncation.
                    If conditions allow, ignoring the previous truncate count,
                    look for an undo space that is too big.
@param[in]  truncate_count    number of times this is called in a loop
@return true if an undo tablespace was marked for truncate. */
static bool trx_purge_mark_undo_for_truncate(size_t truncate_count) {
  /* We always have at least 2 undo spaces, even though one of them may be
  inactive. */
  ut_a(undo_truncate::spaces->size() >= FSP_IMPLICIT_UNDO_TABLESPACES);

  /* Note if we are currently in a fast or slow shutdown. */
  bool normal_operation = (srv_shutdown_state == SRV_SHUTDOWN_NONE);
  bool in_fast_shutdown = (!normal_operation && srv_fast_shutdown > 0);

  /* Save time during a fast shutdown by skipping undo truncation.
  This does not affect correctness since undo tablespaces that need
  truncation can be truncated during or after startup.*/
  if (in_fast_shutdown) {
    return (false);
  }

  /* Return true if an undo tablespace is already marked for truncate. */
  auto undo_trunc = &purge_sys->undo_trunc;
  if (undo_trunc->is_marked()) {
    return (true);
  }

  undo_truncate::spaces->s_lock(UT_LOCATION_HERE);

  /* In order to implicitly select an undo space to truncate, we need
  at least 2 active UNDO tablespaces.  As long as there is one undo
  tablespace active the server will continue to operate. */
  size_t num_active = 0;

  /* Look for any undo space that is inactive explicitly. */
  auto undo_ts =
      undo_truncate::spaces->find_first_inactive_explicit(&num_active);
  if (undo_ts != nullptr) {
    undo_trunc->mark(undo_ts);
    undo_truncate::spaces->s_unlock();
    return (true);
  }

  undo_truncate::spaces->s_unlock();

  /* If we get here, there are no undo spaces currently being truncated
  and none that are SET INACTIVE explicitly. */
  ut_a(num_active > 0);

  /* There may be some reasons not to truncate implicitly.
  If truncate is disabled, do not truncate. */
  if (!srv_undo_log_truncate) {
    return (false);
  }

  if (normal_operation) {
    /* Skip truncate if there is only one active undo tablespace to check. */
    if (num_active == 1) {
      return (false);
    }

    /* Skip truncate if the caller has already truncated an undo space. */
    if (truncate_count > 0) {
      return (false);
    }

    /* Wait at least one second between searches. */
    if (undo_trunc->check_timer() < PURGE_CHECK_UNDO_TRUNCATE_DELAY_IN_MS) {
      return (false);
    }
    undo_trunc->reset_timer();
  }

  /* Find an undo tablespace that is too big and needs to be truncated. */
  undo_truncate::spaces->s_lock(UT_LOCATION_HERE);

  /* Avoid bias selection and so start the scan immediately after the
  last space selected for truncate. Scan through all undo tablespaces. */
  space_id_t space_num = undo_trunc->get_scan_space_num();
  space_id_t first_space_num_scanned = space_num;

  do {
    auto undo_space = undo_truncate::spaces->find(space_num);

    if (undo_space->needs_truncation()) {
      /* Tablespace qualifies for truncate.
      We don't want the current tablespace's ID to be modified as it would be
      needed later for truncation, so we ignore the return value below. */
      (void)undo_trunc->increment_scan();
      undo_trunc->mark(undo_space);
      break;
    }

    space_num = undo_trunc->increment_scan();

  } while (space_num != first_space_num_scanned);

  undo_truncate::spaces->s_unlock();

  /* Return false if no undo space needs to be truncated. */
  if (!undo_trunc->is_marked()) {
    return (false);
  }

  ut_ad(space_num == undo_trunc->get_marked_space_num());

  return (true);
}

void undo_truncate::Truncate::mark(Tablespace *undo_space) {
  /* Set the internal state of this undo space to inactive_implicit so that its
  rsegs will not be allocated to any new transaction.
  If the space is already in the inactive_explicit state, it will stay there.
  Note that the DD is not modified since in case of crash, the action must be
  completed before the DD is available. Set both the state and this marked id
  while this routine has an x_lock on m_rsegs because a concurrent user thread
  might issue undo_space->alter_active(). */
  undo_space->set_inactive_implicit(&m_space_id_marked);

  m_marked_space_is_empty = false;

  ut_d(ib::info(ER_IB_MSG_UNDO_MARKED_FOR_TRUNCATE, undo_space->file_name()));
}

size_t undo_truncate::Truncate::s_scan_pos;

/** Iterate over selected UNDO tablespace and check if all the rsegs
that resides in the tablespace have been freed. */
static bool trx_purge_check_if_marked_undo_is_empty() {
  undo_truncate::Truncate *undo_trunc = &purge_sys->undo_trunc;

  ut_ad(undo_trunc->is_marked());

  /* Return immediately if the marked UNDO tablespace has already been
  found to be empty. */
  if (undo_trunc->is_marked_space_empty()) {
    return (true);
  }

  undo_truncate::spaces->s_lock(UT_LOCATION_HERE);
  space_id_t space_num = undo_trunc->get_marked_space_num();
  undo_truncate::Tablespace *marked_space =
      undo_truncate::spaces->find(space_num);
  Rsegs *marked_rsegs = marked_space->rsegs();

  /* Scan over each rseg in this inactive undo tablespace and ensure that it
  does not hold any active undo records. */
  bool all_free = true;

  marked_rsegs->x_lock();

  /* If an undo tablespace is marked, its rsegs are inactive. */
  ut_ad(!marked_rsegs->is_active());

  for (auto rseg : *marked_rsegs) {
    rseg->latch();

    if (rseg->trx_ref_count > 0) {
      /* This rseg is still being held by an active transaction. */
      all_free = false;
    } else if (rseg->last_page_no != FIL_NULL) {
      /* This rseg still has data to be purged. */
      all_free = false;
    }

    rseg->unlatch();

    if (!all_free) {
      break;
    }
  }

  if (all_free) {
    undo_trunc->set_marked_space_empty();
  }

  marked_rsegs->x_unlock();

  undo_truncate::spaces->s_unlock();

  return (all_free);
}

/** Truncate the marked undo tablespace. This layer handles locking of
marked_rsegs and undo_truncate::spaces.
@return true for success, false for failure */
static bool trx_purge_truncate_marked_undo_low(space_id_t space_num,
                                               std::string space_name) {
  undo_truncate::Truncate *undo_trunc = &purge_sys->undo_trunc;

  /* Get the undo space pointer again. */
  undo_truncate::spaces->s_lock(UT_LOCATION_HERE);

  undo_truncate::Tablespace *marked_space =
      undo_truncate::spaces->find(space_num);

  undo_truncate::spaces->s_unlock();

#ifdef UNIV_DEBUG
  static undo_truncate::Inject_failure_once inject_marked_space(
      "ib_undo_trunc_fail_marked_space");
  if (inject_marked_space.should_fail()) {
    marked_space = nullptr;
  };
#endif /* UNIV_DEBUG */

  if (marked_space == nullptr) {
    return (false);
  }

  ut_d(undo_truncate::inject_crash(
      "ib_undo_trunc_before_writing_unusable_flag"));

  if (fil_space_get(marked_space->id())) {
    marked_space->truncate_in_progress = true;
    mtr_t mtr;
    mtr.start();
    undo_truncate::mark_undo_tablespace_unusable(marked_space->id(), &mtr);
    mtr.commit();
  } else {
    /* During UNDO tablespace truncation, fil_space_t is removed from the fil
    cache. If truncation is unsuccessful for some reason, fil_space_t for this
    marked UNDO tablespace is not loaded again into the fil cache. However, in
    the next iteration, truncation is attempted again for this tablespace, and
    fil_space_get() may return nullptr. This is acceptable because, in the
    previous attempt, the unusable flag would have been persisted in the
    tablespace header. With this assert, we are making sure that truncation was
    in progress for this UNDO tablespace. */
    ut_ad(marked_space->truncate_in_progress);
  }

  ut_d(
      undo_truncate::inject_crash("ib_undo_trunc_after_writing_unusable_flag"));

  /* Don't do the actual truncate if we are doing a fast shutdown.
  The fixup routines will do it at startup. */
  bool in_fast_shutdown = (srv_shutdown_state.load() != SRV_SHUTDOWN_NONE &&
                           srv_fast_shutdown != 0);
#ifdef UNIV_DEBUG
  static undo_truncate::Inject_failure_once inject_fast_shutdown(
      "ib_undo_trunc_fail_fast_shutdown");
  if (inject_fast_shutdown.should_fail()) {
    in_fast_shutdown = true;
  };
#endif /* UNIV_DEBUG */

  if (in_fast_shutdown) {
    return (false);
  }

  Clone_notify notifier(Clone_notify::Type::SPACE_UNDO_DDL, marked_space->id(),
                        true);

  if (notifier.failed()) {
    /* purecov: begin inspected */
    ib::info(ER_IB_MSG_UNDO_TRUNCATE_DELAY_BY_CLONE, space_name.c_str());
    return false;
    /* purecov: end */
  }

  /* Do the truncate.  This will change the space_id of the marked_space. */
  bool success = trx_undo_truncate_tablespace(marked_space);

  if (!success) {
    /* Note: In case of error we don't enable the rsegs nor unmark the
     tablespace. So the tablespace will continue to remain inactive. */
    ib::warn(ER_IB_MSG_UNDO_TRUNCATE_DELAY_BY_FAILURE, space_name.c_str());
    return (false);
  }

  ut_d(undo_truncate::inject_crash("ib_undo_trunc_before_state_update"));

  space_id_t new_space_id = marked_space->id();

  /* Determine the next state. */
  dd_space_states next_state;

  undo_truncate::spaces->s_lock(UT_LOCATION_HERE);

  Rsegs *marked_rsegs = marked_space->rsegs();
  marked_rsegs->x_lock();

  if (marked_rsegs->is_inactive_explicit()) {
    next_state = DD_SPACE_STATE_EMPTY;

    /* This was made inactive and truncated due to an ALTER TABLESPACE SET
    INACTIVE statement. Mark it empty now so that it can be DROPPED. */
    marked_rsegs->set_empty();
    ut_d(ib::info(ER_IB_MSG_UNDO_MARKED_EMPTY, marked_space->file_name()));

  } else {
    ut_ad(marked_rsegs->is_inactive_implicit());
    next_state = DD_SPACE_STATE_ACTIVE;

    /* This was made inactive and truncated due to normal background undo
    tablespace truncation. Make it 'active' again. */
    marked_rsegs->set_active();
    ut_d(ib::info(ER_IB_MSG_UNDO_MARKED_ACTIVE, marked_space->file_name()));
  }

  marked_rsegs->x_unlock();
  undo_truncate::spaces->s_unlock();

  undo_trunc->reset();

  mtr_t mtr;
  mtr.start();
  /* Unset the header flag to indicate that undo tablespace is usable */
  undo_truncate::mark_undo_tablespace_usable(new_space_id, &mtr);
  mtr.commit();

  ut_d(undo_truncate::inject_crash("ib_undo_trunc_after_unsetting_the_flag"));

  /* Update the DD with the new space ID and state. */
  bool dd_result = dd_tablespace_get_mdl(space_name.c_str());
  if (dd_result == DD_SUCCESS) {
    dd_result = dd_tablespace_set_id_and_state(space_name.c_str(), new_space_id,
                                               next_state);
  }

  if (dd_result == DD_FAILURE) {
    return false;
  }

  ut_d(undo_truncate::inject_crash("ib_undo_trunc_after_state_update"));

  marked_space->truncate_in_progress = false;

  return (true);
}

/** Truncate the marked undo tablespace.
This wrapper does initial preparation and handles cleanup.
@return true for success, false for failure */
static bool trx_purge_truncate_marked_undo() {
  MONITOR_INC_VALUE(MONITOR_UNDO_TRUNCATE_COUNT, 1);
  auto counter_time_truncate = std::chrono::steady_clock::now();

  /* Initialize variables */
  undo_truncate::Truncate *undo_trunc = &purge_sys->undo_trunc;
  ut_ad(undo_trunc->is_marked());
  ut_ad(undo_trunc->is_marked_space_empty());

  undo_truncate::spaces->s_lock(UT_LOCATION_HERE);
  space_id_t space_num = undo_trunc->get_marked_space_num();
  undo_truncate::Tablespace *marked_space =
      undo_truncate::spaces->find(space_num);
  std::string space_name = marked_space->space_name();
  undo_truncate::spaces->s_unlock();

  ib::info(ER_IB_MSG_UNDO_TRUNCATE_START, space_name.c_str());

  ut_d(undo_truncate::inject_crash("ib_undo_trunc_before_mdl"));

  /* Get the MDL lock to prevent an ALTER or DROP command from interfering
  with this undo tablespace while it is being truncated. */
  MDL_ticket *mdl_ticket;
  bool dd_result =
      dd_tablespace_get_mdl(space_name.c_str(), &mdl_ticket, false);

#ifdef UNIV_DEBUG
  static undo_truncate::Inject_failure_once injector(
      "ib_undo_trunc_fail_get_mdl");
  if (injector.should_fail()) {
    dd_release_mdl(mdl_ticket);
    dd_result = DD_FAILURE;
  };
#endif /* UNIV_DEBUG */

  if (dd_result != DD_SUCCESS) {
    MONITOR_INC_TIME(MONITOR_UNDO_TRUNCATE_MICROSECOND, counter_time_truncate);
    ib::info(ER_IB_MSG_UNDO_TRUNCATE_DELAY_BY_MDL, space_name.c_str());
    return (false);
  }
  ut_ad(mdl_ticket != nullptr);

  /* Re-check for clone after acquiring MDL. The Backup MDL from clone is
  released by clone during shutdown while provisioning. We should not allow
  truncate to proceed here. */
  if (clone_check_provisioning()) {
    dd_release_mdl(mdl_ticket);
    ib::info(ER_IB_MSG_UNDO_TRUNCATE_DELAY_BY_CLONE, space_name.c_str());
    return (false);
  }

  /* Serialize this truncate with all undo tablespace DDLs */
  mutex_enter(&undo_truncate::ddl_mutex);

  if (!trx_purge_truncate_marked_undo_low(space_num, space_name)) {
    mutex_exit(&undo_truncate::ddl_mutex);
    dd_release_mdl(mdl_ticket);
    MONITOR_INC_TIME(MONITOR_UNDO_TRUNCATE_MICROSECOND, counter_time_truncate);
    return (false);
  }

  /* Truncate is complete. Now it is safe to re-use the tablespace. */
  ib::info(ER_IB_MSG_UNDO_TRUNCATE_COMPLETE, space_name.c_str());

  dd_release_mdl(mdl_ticket);

  ut_d(undo_truncate::inject_crash("ib_undo_trunc_done"));

  mutex_exit(&undo_truncate::ddl_mutex);

  MONITOR_INC_TIME(MONITOR_UNDO_TRUNCATE_MICROSECOND, counter_time_truncate);
  return true;
}

/** Removes unnecessary history data from rollback segments.
NOTE that when this function is called, the caller must not
have any latches on undo log pages!
@param[in]  limit  Truncate limit
*/
static void trx_purge_truncate_history(purge_iter_t *limit) {
  MONITOR_INC_VALUE(MONITOR_PURGE_TRUNCATE_HISTORY_COUNT, 1);

  auto counter_time_truncate_history = std::chrono::steady_clock::now();
  const auto lowest_needed_trx_no = purge_sys->m_lowest_needed_trx_no;
  /* We play safe and set the truncate limit at most to the purge view
  low_limit number, though this is not necessary */

  if (limit->trx_no >= lowest_needed_trx_no) {
    limit->trx_no = lowest_needed_trx_no;
    limit->undo_no = 0;
    limit->undo_rseg_space = SPACE_UNKNOWN;
  }

  ut_ad(limit->trx_no <= lowest_needed_trx_no);

  /* Purge rollback segments in all undo tablespaces.  This may take
  some time and we do not want an undo DDL to attempt an x_lock during
  this time.  If it did, all other transactions seeking a short s_lock()
  would line up behind it.  So get the ddl_mutex before this s_lock(). */
  mutex_enter(&undo_truncate::ddl_mutex);
  undo_truncate::spaces->s_lock(UT_LOCATION_HERE);
  for (auto undo_space : undo_truncate::spaces->m_spaces) {
    /* Skip undo tablespace that is already empty and marked for truncation. */
    undo_truncate::Truncate &ut = purge_sys->undo_trunc;

    if (ut.is_equal(undo_space->id()) && ut.is_marked() &&
        ut.is_marked_space_empty()) {
      continue;
    }

    /* Purge rollback segments in this undo tablespace. */
    undo_space->rsegs()->s_lock();

    for (auto rseg : *undo_space->rsegs()) {
      trx_purge_truncate_rseg_history(rseg, limit);
    }
    undo_space->rsegs()->s_unlock();
  }

  undo_truncate::spaces->s_unlock();
  mutex_exit(&undo_truncate::ddl_mutex);

  /* Purge rollback segments in the temporary tablespace. */
  trx_sys->tmp_rsegs.s_lock();
  for (auto rseg : trx_sys->tmp_rsegs) {
    trx_purge_truncate_rseg_history(rseg, limit);
  }
  trx_sys->tmp_rsegs.s_unlock();

  MONITOR_INC_TIME(MONITOR_PURGE_TRUNCATE_HISTORY_MICROSECOND,
                   counter_time_truncate_history);
}

/** Select an undo tablespace to truncate, make sure it is empty of undo logs,
then finally truncate it. */
static void trx_purge_truncate_undo_spaces() {
  auto &undo_trunc = purge_sys->undo_trunc;

  /* Truncate as many undo spaces as can be truncated.
  Break the loop and return whenever the process cannot be completed. */
  for (size_t i = 0; i < undo_truncate::spaces->size(); ++i) {
    /* Check current activity and if conditions allow, mark the undo space that
    needs to be truncated. */
    if (!trx_purge_mark_undo_for_truncate(i)) {
      break; /* No truncation is needed at this time. */
    }

    /* A space was marked but may not be yet empty. */
    ut_a(undo_trunc.is_marked());

    /* If any undo logs need to be purged from this marked space, try again
    later. */
    if (!trx_purge_check_if_marked_undo_is_empty()) {
      break;
    }

    /* A space has been marked and is now empty. */
    ut_a(undo_trunc.is_marked_space_empty());

    /* Truncate the marked space. */
    if (!trx_purge_truncate_marked_undo()) {
      /* If the marked and empty space did not get truncated due to a concurrent
      clone or something else, try again later. */
      break;
    }
  }
}

/** Updates the last not yet purged history log info in rseg when we have purged
 a whole undo log. Advances also purge_sys->purge_trx_no past the purged log. */
static void trx_purge_rseg_get_next_history_log(
    trx_rseg_t *rseg,       /*!< in: rollback segment */
    ulint *n_pages_handled) /*!< in/out: number of UNDO pages
                            handled */
{
  mtr_t mtr;

  rseg->latch();

  ut_a(rseg->last_page_no != FIL_NULL);

  purge_sys->iter.trx_no = rseg->last_trx_no + 1;
  purge_sys->iter.undo_no = 0;
  purge_sys->iter.undo_rseg_space = SPACE_UNKNOWN;
  purge_sys->next_stored = false;

  mtr_start(&mtr);

  auto undo_page = trx_undo_page_get_s_latched(
      page_id_t(rseg->space_id, rseg->last_page_no), rseg->page_size, &mtr);

  auto log_hdr = undo_page + rseg->last_offset;

  /* Increase the purge page count by one for every handled log */

  (*n_pages_handled)++;

  auto prev_log_addr = trx_purge_get_log_from_hist(
      flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE, &mtr));

  if (prev_log_addr.page == FIL_NULL) {
    /* No logs left in the history list */

    rseg->last_page_no = FIL_NULL;

    mtr_commit(&mtr);
    rseg->unlatch();

#ifdef UNIV_DEBUG
    /* Add debug code to track history list corruption reported on the MySQL
    mailing list on Nov 9, 2004. The fut0lst.cc file-based list was corrupt. The
    prev node pointer was FIL_NULL, even though the list length was over 8
    million nodes! We assume that purge truncates the history list in large size
    pieces, and if we here reach the head of the list, the
    list cannot be longer than 2000 000 undo logs now. */

    const auto rseg_history_len = trx_sys->rseg_history_len.load();
    if (rseg_history_len > 2000000) {
      ib::warn(ER_IB_MSG_1177)
          << "Purge reached the head of the history"
             " list, but its length is still reported as "
          << rseg_history_len << " which is unusually high.";
      ib::info(ER_IB_MSG_1178) << "This can happen for multiple reasons";
      ib::info(ER_IB_MSG_1179) << "1. A long running transaction is"
                                  " withholding purging of undo logs or a read"
                                  " view is open. Please try to commit the long"
                                  " running transaction.";
      ib::info(ER_IB_MSG_1180) << "2. Try increasing the number of purge"
                                  " threads to expedite purging of undo logs.";
    }
#endif
    return;
  }

  mtr_commit(&mtr);
  rseg->unlatch();

  /* Read the trx number and del marks from the previous log header */
  mtr_start(&mtr);

  log_hdr =
      trx_undo_page_get_s_latched(page_id_t(rseg->space_id, prev_log_addr.page),
                                  rseg->page_size, &mtr) +
      prev_log_addr.boffset;

  trx_id_t trx_no = mach_read_from_8(log_hdr + TRX_UNDO_TRX_NO);

  auto del_marks = mach_read_from_2(log_hdr + TRX_UNDO_DEL_MARKS);

  mtr_commit(&mtr);

  rseg->latch();

  rseg->last_page_no = prev_log_addr.page;
  rseg->last_offset = prev_log_addr.boffset;
  rseg->last_trx_no = trx_no;
  rseg->last_del_marks = del_marks;

  TrxUndoRsegs elem(rseg->last_trx_no);
  elem.insert(rseg);

  /* Purge can also produce events, however these are already ordered in the
  rollback segment and any user generated event will be greater than the events
  that Purge produces. ie. Purge can never produce
  events from an empty rollback segment. */

  mutex_enter(&purge_sys->pq_mutex);

  purge_sys->purge_queue->push(std::move(elem));

  mutex_exit(&purge_sys->pq_mutex);

  rseg->unlatch();
}

/** Position the purge sys "iterator" on the undo record to use for purging.
@param[in,out]  purge_sys       purge instance
@param[in]      page_size       page size */
static void trx_purge_read_undo_rec(trx_purge_t *purge_sys,
                                    const page_size_t &page_size) {
  ulint offset;
  page_no_t page_no;
  uint64_t undo_no;
  space_id_t undo_rseg_space;
  trx_id_t modifier_trx_id;

  purge_sys->hdr_offset = purge_sys->rseg->last_offset;
  page_no = purge_sys->hdr_page_no = purge_sys->rseg->last_page_no;

  if (purge_sys->rseg->last_del_marks) {
    mtr_t mtr;
    trx_undo_rec_t *undo_rec = nullptr;

    mtr_start(&mtr);

    undo_rec = trx_undo_get_first_rec(
        &modifier_trx_id, purge_sys->rseg->space_id, page_size,
        purge_sys->hdr_page_no, purge_sys->hdr_offset, RW_S_LATCH, &mtr);

    if (undo_rec != nullptr) {
      offset = page_offset(undo_rec);
      undo_no = trx_undo_rec_get_undo_no(undo_rec);
      undo_rseg_space = purge_sys->rseg->space_id;
      page_no = page_get_page_no(page_align(undo_rec));
    } else {
      offset = 0;
      undo_no = 0;
      undo_rseg_space = SPACE_UNKNOWN;
    }

    mtr_commit(&mtr);
  } else {
    offset = 0;
    undo_no = 0;
    undo_rseg_space = SPACE_UNKNOWN;
    modifier_trx_id = 0;
  }

  purge_sys->offset = offset;
  purge_sys->page_no = page_no;
  purge_sys->iter.undo_no = undo_no;
  purge_sys->iter.modifier_trx_id = modifier_trx_id;
  purge_sys->iter.undo_rseg_space = undo_rseg_space;

  purge_sys->next_stored = true;
}

/** Chooses the next undo log to purge and updates the info in purge_sys. This
 function is used to initialize purge_sys when the next record to purge is not
 known, and also to update the purge system info on the next record when purge
 has handled the whole undo log for a transaction. */
static void trx_purge_choose_next_log(void) {
  ut_ad(purge_sys->next_stored == false);

  const page_size_t &page_size = purge_sys->rseg_iter->set_next();

  if (purge_sys->rseg != nullptr) {
    trx_purge_read_undo_rec(purge_sys, page_size);
  } else {
    /* There is nothing to do yet. */
    std::this_thread::yield();
  }
}

/** Gets the next record to purge and updates the info in the purge system.
 @return copy of an undo log record or pointer to the dummy undo log record */
static trx_undo_rec_t *trx_purge_get_next_rec(
    ulint *n_pages_handled, /*!< in/out: number of UNDO pages
                            handled */
    mem_heap_t *heap)       /*!< in: memory heap where copied */
{
  trx_undo_rec_t *rec;
  trx_undo_rec_t *rec_copy;
  trx_undo_rec_t *rec2;
  page_t *undo_page;
  page_t *page;
  ulint offset;
  page_no_t page_no;
  space_id_t space;
  mtr_t mtr;

  ut_ad(purge_sys->next_stored);
  ut_ad(purge_sys->iter.trx_no < purge_sys->m_lowest_needed_trx_no);

  space = purge_sys->rseg->space_id;
  page_no = purge_sys->page_no;
  offset = purge_sys->offset;

  const page_size_t page_size(purge_sys->rseg->page_size);

  if (offset == 0) {
    /* It is the dummy undo log record, which means that there is no need to
    purge this undo log */

    trx_purge_rseg_get_next_history_log(purge_sys->rseg, n_pages_handled);

    /* Look for the next undo log and record to purge */

    trx_purge_choose_next_log();

    return (&trx_purge_ignore_rec);
  }

  mtr_start(&mtr);

  undo_page =
      trx_undo_page_get_s_latched(page_id_t(space, page_no), page_size, &mtr);

  rec = undo_page + offset;

  rec2 = rec;

  for (;;) {
    ulint type;
    trx_undo_rec_t *next_rec;
    ulint cmpl_info;

    /* Try first to find the next record which requires a purge operation from
    the same page of the same undo log */

    next_rec = trx_undo_page_get_next_rec(rec2, purge_sys->hdr_page_no,
                                          purge_sys->hdr_offset);

    if (next_rec == nullptr) {
      rec2 = trx_undo_get_next_rec(rec2, purge_sys->hdr_page_no,
                                   purge_sys->hdr_offset, &mtr);
      break;
    }

    rec2 = next_rec;

    type = trx_undo_rec_get_type(rec2);

    if (type == TRX_UNDO_DEL_MARK_REC) {
      break;
    }

    cmpl_info = trx_undo_rec_get_cmpl_info(rec2);

    if (trx_undo_rec_get_extern_storage(rec2)) {
      break;
    }

    if ((type == TRX_UNDO_UPD_EXIST_REC) &&
        !(cmpl_info & UPD_NODE_NO_ORD_CHANGE)) {
      break;
    }
  }

  if (rec2 == nullptr) {
    mtr_commit(&mtr);

    trx_purge_rseg_get_next_history_log(purge_sys->rseg, n_pages_handled);

    /* Look for the next undo log and record to purge */

    trx_purge_choose_next_log();

    mtr_start(&mtr);

    undo_page =
        trx_undo_page_get_s_latched(page_id_t(space, page_no), page_size, &mtr);

  } else {
    page = page_align(rec2);

    purge_sys->offset = rec2 - page;
    purge_sys->page_no = page_get_page_no(page);
    purge_sys->iter.undo_no = trx_undo_rec_get_undo_no(rec2);
    purge_sys->iter.undo_rseg_space = space;

    if (undo_page != page) {
      /* We advance to a new page of the undo log: */
      (*n_pages_handled)++;
    }
  }

  rec_copy = trx_undo_rec_copy(undo_page, static_cast<uint32_t>(offset), heap);

  mtr_commit(&mtr);

  return (rec_copy);
}

struct Purge_groups_t {
  Purge_groups_t(std::size_t n_threads, mem_heap_t *heap)
      : m_grpid_umap{n_threads, mem_heap_allocator<GroupBy::value_type>{heap}},
        m_groups(n_threads, nullptr,
                 mem_heap_allocator<purge_node_t::Recs *>(heap)),
        m_heap(heap),
        m_total_rec(0) {}

  void init() {
    const std::size_t n_purge_threads = m_groups.size();

    /* Initialize the grouping vector. */
    for (std::size_t grpid = 0; grpid < n_purge_threads; ++grpid) {
      void *ptr;
      purge_node_t::Recs *recs;

      ptr = mem_heap_alloc(m_heap, sizeof(purge_node_t::Recs));

      /* Call the destructor explicitly in row_purge_end() */
      recs = new (ptr)
          purge_node_t::Recs{mem_heap_allocator<purge_node_t::rec_t>{m_heap}};

      m_groups[grpid] = recs;
    }
  }

  std::size_t find_smallest_group();

  /** Check the history list length and decide if distribution of workload
  between purge threads is needed or not.  If needed, do the distribution,
  otherwise do nothing. */
  void distribute_if_needed();

  std::ostream &print(std::ostream &out) const;

  void assign(que_thr_t **thrs) {
    const std::size_t n_purge_threads = m_groups.size();
    for (std::size_t grpid = 0; grpid < n_purge_threads; ++grpid) {
      ut_ad(thrs[grpid] != nullptr);
      purge_node_t *node = static_cast<purge_node_t *>(thrs[grpid]->child);
      ut_a(que_node_get_type(node) == QUE_NODE_PURGE);
      ut_ad(node->recs == nullptr);
      node->recs = m_groups[grpid];
    }
  }

#ifdef UNIV_DEBUG
  bool is_grouping_uniform() const;
#endif /* UNIV_DEBUG */

  void add(purge_node_t::rec_t &rec) {
    /* Identify the table id */
    const table_id_t id = trx_undo_rec_get_table_id(rec.undo_rec);
    std::size_t grpid;

    GroupBy::iterator lb = m_grpid_umap.find(id);
    if (lb != m_grpid_umap.end()) {
      grpid = lb->second;
    } else {
      grpid = find_smallest_group();
      m_grpid_umap.insert(std::make_pair(id, grpid));
    }

    m_groups[grpid]->push_back(rec);
    m_total_rec++;
  }

  using GroupBy = std::unordered_map<
      table_id_t, std::size_t, std::hash<table_id_t>, std::equal_to<table_id_t>,
      mem_heap_allocator<std::pair<const table_id_t, std::size_t>>>;

  /** Given a table_id obtain the group id to which it belongs. */
  GroupBy m_grpid_umap;

  /** Allocator used for the vector below. */
  using vec_alloc = mem_heap_allocator<purge_node_t::Recs *>;

  /** A vector of groups.  The size of this vector is equal to the number of
  purge threads.  Each undo record is assigned to one of the groups, based on
  its table_id. The index into this vector is the group_id. */
  std::vector<purge_node_t::Recs *, vec_alloc> m_groups;

  /** Memory heap in which memory for unordered_map & vector is allocated.*/
  mem_heap_t *m_heap;

  /** Total number of undo records parsed and grouped. */
  std::size_t m_total_rec;

 private:
  /** Redistribute the undo records across different groups.  If a group has
  more records than it should, move all the extra records to the next group.
  Maximum two passes might be needed. */
  void distribute();
};

std::size_t Purge_groups_t::find_smallest_group() {
  std::size_t result = 0;
  std::size_t n = std::numeric_limits<std::size_t>::max();
  const std::size_t n_purge_threads = m_groups.size();

  for (std::size_t grpid = 0; grpid < n_purge_threads; ++grpid) {
    const std::size_t grp_count = m_groups[grpid]->size();
    if (grp_count < n) {
      n = grp_count;
      result = grpid;
    }
  }
  return result;
}

std::ostream &Purge_groups_t::print(std::ostream &out) const {
  const std::size_t n_purge_threads = m_groups.size();
  const std::size_t max_n = ut::div_ceil(m_total_rec, n_purge_threads);
  const std::size_t min_n =
      (max_n > n_purge_threads) ? max_n - n_purge_threads : 0;

  if (m_total_rec > 0) {
    out << "[n_purge_threads=" << n_purge_threads
        << ", m_total_rec=" << m_total_rec << ", max=" << max_n
        << ", min=" << min_n << ", [";
    for (std::size_t i = 0; i < n_purge_threads; ++i) {
      out << m_groups[i]->size() << ", ";
    }
    out << "]]" << std::endl;
  }
  return out;
}

#ifdef UNIV_DEBUG
bool Purge_groups_t::is_grouping_uniform() const {
  const std::size_t n_purge_threads = m_groups.size();
  const std::size_t max_n = ut::div_ceil(m_total_rec, n_purge_threads);
  const std::size_t min_n =
      (max_n > n_purge_threads) ? max_n - n_purge_threads : 0;
  bool result = true;

  for (std::size_t grpid = 0; grpid < n_purge_threads; ++grpid) {
    const std::size_t grp_count = m_groups[grpid]->size();
    if (grp_count < min_n || grp_count > max_n) {
      result = false;
    }
  }
  return result;
}
#endif /* UNIV_DEBUG */

void Purge_groups_t::distribute() {
  const std::size_t n_purge_threads = m_groups.size();
  const std::size_t max_n = ut::div_ceil(m_total_rec, n_purge_threads);

  for (std::size_t i = 0; i < 2; ++i) {
    bool need_second_pass = false;
    for (std::size_t grpid = 0; grpid < n_purge_threads; ++grpid) {
      std::size_t grp_count = m_groups[grpid]->size();
      if (grp_count > max_n) {
        auto from_list = m_groups[grpid];
        std::size_t target_grpid = grpid + 1;
        if (target_grpid == n_purge_threads) {
          target_grpid = 0;
          /* Undo records are moved to the first group. So a second pass is
          needed. */
          need_second_pass = true;
        }
        auto to_list = m_groups[target_grpid];
        auto from_iter = from_list->begin();
        std::advance(from_iter, max_n);
        to_list->splice(to_list->end(), *from_list, from_iter,
                        from_list->end());
      } else if (i == 1) {
        /* In the second pass, stop as soon as we encounter a group with <=
        max_n records. */
        break;
      }
    }
    if (!need_second_pass) {
      break;
    }
  }

#ifdef UNIV_DEBUG
  if (!is_grouping_uniform()) {
    print(std::cerr);
    ut_error;
  }
#endif /* UNIV_DEBUG */
}

void Purge_groups_t::distribute_if_needed() {
  const uint64_t rseg_history_len = trx_sys->rseg_history_len.load();

  /* If the history list length is greater than maximum allowed purge lag,
  then distribute the workload across all purge threads. */
  if (srv_max_purge_lag > 0 && rseg_history_len > srv_max_purge_lag) {
    distribute();
  }
}

/** Fetches the next undo log record from the history list to purge. It must
 be released with the corresponding release function.
 @return copy of an undo log record or pointer to trx_purge_ignore_rec,
 if the whole undo log can skipped in purge; NULL if none left */
[[nodiscard]] static trx_undo_rec_t *trx_purge_fetch_next_rec(
    trx_id_t *modifier_trx_id,
    /*!< out: modifier trx id. this is the
    trx that created the undo record. */
    roll_ptr_t *roll_ptr,   /*!< out: roll pointer to undo record */
    ulint *n_pages_handled, /*!< in/out: number of UNDO log pages
                            handled */
    mem_heap_t *heap)       /*!< in: memory heap where copied */
{
  if (!purge_sys->next_stored) {
    trx_purge_choose_next_log();

    if (!purge_sys->next_stored) {
      DBUG_PRINT("ib_purge", ("no logs left in the history list"));
      return nullptr;
    }
  }

  if (purge_sys->iter.trx_no >= purge_sys->m_lowest_needed_trx_no) {
    return nullptr;
  }

  /* fprintf(stderr, "Thread %s purging trx %llu undo record %llu\n",
  to_string(std::this_thread::get_id()), iter->trx_no, iter->undo_no); */

  *roll_ptr = trx_undo_build_roll_ptr(false, purge_sys->rseg->space_id,
                                      purge_sys->page_no, purge_sys->offset);

  *modifier_trx_id = purge_sys->iter.modifier_trx_id;

  /* The following call will advance the stored values of the
  purge iterator. */

  return (trx_purge_get_next_rec(n_pages_handled, heap));
}

/** This function runs a purge batch.
@param[in]      n_purge_threads  number of purge threads
@param[in]      batch_size       number of pages to purge
@return number of undo log pages handled in the batch */
static ulint trx_purge_attach_undo_recs(const ulint n_purge_threads,
                                        ulint batch_size) {
  ulint n_pages_handled = 0;

  ut_a(n_purge_threads > 0);
  ut_a(n_purge_threads <= MAX_PURGE_THREADS);

  purge_sys->limit = purge_sys->iter;

  que_thr_t *run_thrs[MAX_PURGE_THREADS];

  /* Validate some pre-requisites and reset done flag. */
  ulint i = 0;

  for (auto thr : purge_sys->query->thrs) {
    if (n_purge_threads <= i) break;
    purge_node_t *node;

    /* Get the purge node. */
    node = static_cast<purge_node_t *>(thr->child);

    ut_a(que_node_get_type(node) == QUE_NODE_PURGE);
    ut_a(node->recs == nullptr);
    ut_a(node->done);

    node->done = false;

    ut_a(!thr->is_active);

    run_thrs[i++] = thr;
  }

  /* There should never be fewer nodes than threads, the inverse
  however is allowed because we only use purge threads as needed. */
  ut_a(i == n_purge_threads);
  ut_ad(trx_purge_check_limit());

  mem_heap_t *heap = purge_sys->heap;

  mem_heap_empty(heap);

  Purge_groups_t purge_groups(n_purge_threads, heap);
  purge_groups.init();

  while (n_pages_handled < batch_size) {
    /* Track the max {trx_id, undo_no} for truncating the
    UNDO logs once we have purged the records. */

    if (trx_purge_check_limit()) {
      purge_sys->limit = purge_sys->iter;
    }

    purge_node_t::rec_t rec;

    /* Fetch the next record, and advance the purge_sys->iter. */
    rec.undo_rec = trx_purge_fetch_next_rec(&rec.modifier_trx_id, &rec.roll_ptr,
                                            &n_pages_handled, heap);

    if (rec.undo_rec == &trx_purge_ignore_rec) {
      continue;

    } else if (rec.undo_rec == nullptr) {
      break;
    }

    purge_groups.add(rec);
  }

  purge_groups.distribute_if_needed();
  purge_groups.assign(run_thrs);

  ut_ad(trx_purge_check_limit());

  return (n_pages_handled);
}

/** Calculate the DML delay required.
 @return delay in microseconds or ULINT_MAX */
static ulint trx_purge_dml_delay(void) {
  /* Determine how much data manipulation language (DML) statements
  need to be delayed in order to reduce the lagging of the purge
  thread. */
  ulint delay = 0; /* in microseconds; default: no delay */

  const ulong max_purge_lag = srv_max_purge_lag;
  const ulong max_purge_lag_delay = srv_max_purge_lag_delay;
  const ulong purge_threads = srv_n_purge_threads;
  const ulong purge_batch_size = srv_purge_batch_size;
  const auto rseg_history_len = trx_sys->rseg_history_len.load();
  const auto purge_lag_rearm_threshold =
      static_cast<ulint>(static_cast<ulonglong>(max_purge_lag) * 9 / 10);
  const auto dml_delay_activation_threshold = std::max(
      ulonglong{max_purge_lag}, ulonglong{purge_threads} * purge_batch_size);
  const auto dml_delay_rearm_threshold =
      static_cast<ulint>(dml_delay_activation_threshold * 9 / 10);

  if (max_purge_lag > 0 && rseg_history_len > max_purge_lag) {
    if (!srv_max_purge_lag_exceeded.exchange(true, std::memory_order_relaxed)) {
      ib::warn(ER_IB_MSG_MAX_PURGE_LAG_EXCEEDED, ulonglong{rseg_history_len},
               max_purge_lag);
    }
  } else if (max_purge_lag == 0 ||
             rseg_history_len <= purge_lag_rearm_threshold) {
    /* Re-arm after the purge lag has recovered by 10%, avoiding warning
    churn when the history length hovers around the configured limit. */
    srv_max_purge_lag_exceeded.store(false, std::memory_order_relaxed);
  }

  /* If purge lag is set (ie. > 0) then calculate the new DML delay.
  Note: we do a dirty read of the trx_sys_t data structure here,
  without holding trx_sys->mutex. */

  if (max_purge_lag > 0 && rseg_history_len > dml_delay_activation_threshold) {
    float ratio;

    ratio = float(rseg_history_len) / max_purge_lag;

    if (ratio > 1.0) {
      /* If the history list length exceeds the srv_max_purge_lag, the data
      manipulation statements are delayed by at least 5 microseconds. */
      delay = (ulint)((ratio - 0.9995) * 10000);
    }

    if (delay > max_purge_lag_delay) {
      delay = max_purge_lag_delay;
    }
  }

  MONITOR_SET(MONITOR_DML_PURGE_DELAY, delay);

  if (delay > 0) {
    if (!srv_dml_purge_delay_active.exchange(true, std::memory_order_relaxed)) {
      ib::warn(ER_IB_MSG_DML_PURGE_DELAY_STARTED, static_cast<ulong>(delay),
               ulonglong{rseg_history_len}, max_purge_lag, max_purge_lag_delay);
    }
  } else if (max_purge_lag == 0 || max_purge_lag_delay == 0 ||
             rseg_history_len <= dml_delay_rearm_threshold) {
    /* Re-arm after the DML delay activation threshold has recovered by 10%,
    avoiding warning churn when the history length hovers around that limit. */
    srv_dml_purge_delay_active.store(false, std::memory_order_relaxed);
  }

  return (delay);
}

/** Wait for pending purge jobs to complete. */
static void trx_purge_wait_for_workers_to_complete() {
  ulint i = 0;
  ulint n_submitted = purge_sys->n_submitted;

  /* Ensure that the work queue empties out. */
  while (purge_sys->n_completed.load() != n_submitted) {
    if (++i < 10) {
      std::this_thread::yield();
    } else {
      if (srv_get_task_queue_length() > 0) {
        srv_release_threads(SRV_WORKER, 1);
      }

      std::this_thread::sleep_for(std::chrono::microseconds(20));
      i = 0;
    }
  }

  /* None of the worker threads should be doing any work. */
  ut_a(purge_sys->n_submitted == purge_sys->n_completed);

  /* There should be no outstanding tasks as long
  as the worker threads are active. */
  ut_a(srv_get_task_queue_length() == 0);
}

/** Remove old historical changes from the rollback segments. */
static void trx_purge_truncate(void) {
  ut_ad(trx_purge_check_limit());
  if (purge_sys->limit.trx_no == 0) {
    trx_purge_truncate_history(&purge_sys->iter);
  } else {
    trx_purge_truncate_history(&purge_sys->limit);
  }

  /* Attempt to truncate an undo tablespace. */
  trx_purge_truncate_undo_spaces();
}

/** This function runs a purge batch.
 @return number of undo log pages handled in the batch */
ulint trx_purge(ulint n_purge_threads, /*!< in: number of purge tasks
                                       to submit to the work queue */
                ulint batch_size,      /*!< in: the maximum number of records
                                       to purge in one batch */
                bool truncate)         /*!< in: truncate history if true */
{
  que_thr_t *thr = nullptr;
  ulint n_pages_handled;

  ut_a(n_purge_threads > 0);

  srv_dml_needed_delay = trx_purge_dml_delay();

  /* The number of tasks submitted should be completed. */
  ut_a(purge_sys->n_submitted == purge_sys->n_completed);

  trx_purge_update_oldest_needed();

  CONDITIONAL_SYNC_POINT("after_clone_oldest_view");
#ifdef UNIV_DEBUG
  if (srv_purge_view_update_only_debug) {
    return (0);
  }
#endif /* UNIV_DEBUG */

  /* Fetch the UNDO recs that need to be purged. */
  n_pages_handled = trx_purge_attach_undo_recs(n_purge_threads, batch_size);

  /* Submit the tasks to the work queue. */
  for (ulint i = 0; i < n_purge_threads - 1; ++i) {
    thr = que_fork_scheduler_round_robin(purge_sys->query, thr);

    ut_a(thr != nullptr);

    srv_que_task_enqueue_low(thr);
  }

  thr = que_fork_scheduler_round_robin(purge_sys->query, thr);
  ut_a(thr != nullptr);

  purge_sys->n_submitted += n_purge_threads - 1;

  que_run_threads(thr);

  trx_purge_wait_for_workers_to_complete();

  ut_a(purge_sys->n_submitted == purge_sys->n_completed);

#ifdef UNIV_DEBUG
  rw_lock_x_lock(&purge_sys->latch, UT_LOCATION_HERE);
  if (purge_sys->limit.trx_no == 0) {
    purge_sys->done = purge_sys->iter;
  } else {
    purge_sys->done = purge_sys->limit;
  }
  rw_lock_x_unlock(&purge_sys->latch);
#endif /* UNIV_DEBUG */

  /* The first page of LOBs are freed at the end of a purge batch because
  multiple purge threads will access the same LOB as part of the purge
  process.  Some purge threads will free only portion of the LOB related to
  the partial update of the LOB.  But 1 of the purge thread will free the LOB
  completely if it is not needed anymore (either because of full update or
  because of deletion).  If the LOB is freed, and a purge thread attempts to
  access the LOB, then it is a bug.  To avoid this, we delay the freeing of
  the first page of LOB till the end of a purge batch.  */
  for (thr = UT_LIST_GET_FIRST(purge_sys->query->thrs); thr != nullptr;
       thr = UT_LIST_GET_NEXT(thrs, thr)) {
    purge_node_t *node = static_cast<purge_node_t *>(thr->child);
    node->free_lob_pages();
  }

  /* During upgrade, to know whether purge is empty,
  we rely on purge history length. So truncate the
  undo logs during upgrade to update purge history
  length. */
  if (truncate) {
    trx_purge_truncate();
  }

  MONITOR_INC_VALUE(MONITOR_PURGE_INVOKED, 1);
  MONITOR_INC_VALUE(MONITOR_PURGE_N_PAGE_HANDLED, n_pages_handled);

  return (n_pages_handled);
}

/** Get the purge state.
 @return purge state. */
purge_state_t trx_purge_state(void) {
  purge_state_t state;

  rw_lock_x_lock(&purge_sys->latch, UT_LOCATION_HERE);

  state = purge_sys->state;

  rw_lock_x_unlock(&purge_sys->latch);

  return (state);
}

/** Stop purge and wait for it to stop, move to PURGE_STATE_STOP. */
void trx_purge_stop(void) {
  purge_state_t state;
  int64_t sig_count = os_event_reset(purge_sys->event);

  ut_a(srv_n_purge_threads > 0);

  rw_lock_x_lock(&purge_sys->latch, UT_LOCATION_HERE);

  ut_a(purge_sys->state != PURGE_STATE_INIT);
  ut_a(purge_sys->state != PURGE_STATE_EXIT);
  ut_a(purge_sys->state != PURGE_STATE_DISABLED);

  ++purge_sys->n_stop;

  state = purge_sys->state;

  if (state == PURGE_STATE_RUN) {
    ib::info(ER_IB_MSG_1181) << "Stopping purge";

    /* We need to wakeup the purge thread in case it is suspended,
    so that it can acknowledge the state change. */

    srv_purge_wakeup();
  }

  purge_sys->state = PURGE_STATE_STOP;

  rw_lock_x_unlock(&purge_sys->latch);

  if (state != PURGE_STATE_STOP) {
    /* Wait for purge coordinator to signal that it
    is suspended. */
    os_event_wait_low(purge_sys->event, sig_count);
  } else {
    bool once = true;

    rw_lock_x_lock(&purge_sys->latch, UT_LOCATION_HERE);

    /* Wait for purge to signal that it has actually stopped. */
    while (purge_sys->running) {
      if (once) {
        ib::info(ER_IB_MSG_1182) << "Waiting for purge to stop";
        once = false;
      }

      rw_lock_x_unlock(&purge_sys->latch);

      std::this_thread::sleep_for(std::chrono::milliseconds(10));

      rw_lock_x_lock(&purge_sys->latch, UT_LOCATION_HERE);
    }

    rw_lock_x_unlock(&purge_sys->latch);
  }

  MONITOR_INC_VALUE(MONITOR_PURGE_STOP_COUNT, 1);
}

/** Resume purge, move to PURGE_STATE_RUN. */
void trx_purge_run() {
  rw_lock_x_lock(&purge_sys->latch, UT_LOCATION_HERE);

  switch (purge_sys->state) {
    case PURGE_STATE_INIT:
    case PURGE_STATE_EXIT:
    case PURGE_STATE_DISABLED:
      ut_error;

    case PURGE_STATE_RUN:
    case PURGE_STATE_STOP:
      break;
  }

  if (purge_sys->n_stop > 0) {
    ut_a(purge_sys->state == PURGE_STATE_STOP);

    --purge_sys->n_stop;

    if (purge_sys->n_stop == 0) {
      ib::info(ER_IB_MSG_1183) << "Resuming purge";

      purge_sys->state = PURGE_STATE_RUN;
    }

    MONITOR_INC_VALUE(MONITOR_PURGE_RESUME_COUNT, 1);
  } else {
    ut_a(purge_sys->state == PURGE_STATE_RUN);
  }

  rw_lock_x_unlock(&purge_sys->latch);

  srv_purge_wakeup();
}

/** Initialize the undo_truncate::Tablespaces object. */
void undo_truncate::Tablespaces::init() {
  /** Fix the size of the vector so that it will not do
  allocations for inserts. This way the contents can be
  read without using a latch. */
  m_spaces.reserve(FSP_MAX_UNDO_TABLESPACES);

  m_latch = static_cast<rw_lock_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*m_latch)));

  rw_lock_create(undo_spaces_lock_key, m_latch, LATCH_ID_UNDO_SPACES);

  mutex_create(LATCH_ID_UNDO_DDL, &ddl_mutex);
}

/** De-initialize the undo_truncate::Tablespaces object. */
void undo_truncate::Tablespaces::deinit() {
  clear();

  rw_lock_free(m_latch);
  ut::free(m_latch);
  m_latch = nullptr;
  mutex_free(&ddl_mutex);
}

void undo_truncate::Tablespaces::add(Tablespace &ref_undo_space) {
  ut_ad(is_reserved(ref_undo_space.id()));

  if (contains(ref_undo_space.num())) {
    return;
  }

  auto undo_space =
      ut::new_withkey<Tablespace>(UT_NEW_THIS_FILE_PSI_KEY, ref_undo_space);

  m_spaces.push_back(undo_space);
}

void undo_truncate::Tablespaces::drop(Tablespace *undo_space) {
  ut_ad(is_reserved(undo_space->id()));
  ut_ad(contains(undo_space->num()));

  for (auto it = m_spaces.begin(); it != m_spaces.end(); it++) {
    if (*it == undo_space) {
      ut::delete_(undo_space);
      m_spaces.erase(it);
      break;
    }
  }
}

void undo_truncate::Tablespaces::drop(Tablespace &ref_undo_space) {
  ut_ad(is_reserved(ref_undo_space.id()));

  for (auto it = m_spaces.begin(); it != m_spaces.end(); it++) {
    if ((*it)->id() == ref_undo_space.id()) {
      ut::delete_(*it);
      m_spaces.erase(it);
      break;
    }
  }
}
