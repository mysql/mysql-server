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

/** @file trx/trx0purge.cc
 Purge old versions

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#include <sys/types.h>
#include <new>

#include "clone0api.h"
#include "fsp0fsp.h"
#include "fsp0sysspace.h"
#include "fsp0types.h"
#include "fut0fut.h"
#include "ha_prototypes.h"
#include "mach0data.h"
#include "mtr0log.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "os0thread.h"
#include "que0que.h"
#include "read0read.h"
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

/** Maximum allowable purge history length.  <=0 means 'infinite'. */
ulong srv_max_purge_lag = 0;

/** Max DML user threads delay in micro-seconds. */
ulong srv_max_purge_lag_delay = 0;

/** The global data structure coordinating a purge */
trx_purge_t *purge_sys = NULL;

#ifdef UNIV_DEBUG
bool srv_purge_view_update_only_debug;
bool trx_commit_disallowed = false;
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
        m_trx_undo_rsegs.append(purge_sys->purge_queue->top());
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

    m_purge_sys->rseg = NULL;

    /* return a dummy object, not going to be used by the caller */
    return (univ_page_size);
  }

  m_purge_sys->rseg = *m_iter++;

  mutex_exit(&m_purge_sys->pq_mutex);

  ut_a(m_purge_sys->rseg != NULL);

  mutex_enter(&m_purge_sys->rseg->mutex);

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

  mutex_exit(&m_purge_sys->rseg->mutex);

  return (page_size);
}

/** Builds a purge 'query' graph. The actual purge is performed by executing
 this query graph.
 @return own: the query graph */
static que_t *trx_purge_graph_build(
    trx_t *trx,            /*!< in: transaction */
    ulint n_purge_threads) /*!< in: number of purge
                           threads */
{
  ulint i;
  mem_heap_t *heap;
  que_fork_t *fork;

  heap = mem_heap_create(512);
  fork = que_fork_create(NULL, NULL, QUE_FORK_PURGE, heap);
  fork->trx = trx;

  for (i = 0; i < n_purge_threads; ++i) {
    que_thr_t *thr;

    thr = que_thr_create(fork, heap, NULL);

    thr->child = row_purge_node_create(thr, heap);
  }

  return (fork);
}

/** Creates the global purge system control structure and inits the history
 mutex. */
void trx_purge_sys_create(ulint n_purge_threads,   /*!< in: number of purge
                                                   threads */
                          purge_pq_t *purge_queue) /*!< in, own: UNDO log min
                                                   binary heap */
{
  purge_sys = static_cast<trx_purge_t *>(ut_zalloc_nokey(sizeof(*purge_sys)));

  purge_sys->state = PURGE_STATE_INIT;
  purge_sys->event = os_event_create(0);

  new (&purge_sys->iter) purge_iter_t;
  new (&purge_sys->limit) purge_iter_t;
  new (&purge_sys->undo_trunc) undo::Truncate;
#ifdef UNIV_DEBUG
  new (&purge_sys->done) purge_iter_t;
#endif /* UNIV_DEBUG */

  /* Take ownership of purge_queue, we are responsible for freeing it. */
  purge_sys->purge_queue = purge_queue;

  rw_lock_create(trx_purge_latch_key, &purge_sys->latch, SYNC_PURGE_LATCH);

  mutex_create(LATCH_ID_PURGE_SYS_PQ, &purge_sys->pq_mutex);

  ut_a(n_purge_threads > 0);

  purge_sys->sess = sess_open();

  purge_sys->trx = purge_sys->sess->trx;

  ut_a(purge_sys->trx->sess == purge_sys->sess);

  /* A purge transaction is not a real transaction, we use a transaction
  here only because the query threads code requires it. It is otherwise
  quite unnecessary. We should get rid of it eventually. */
  purge_sys->trx->id = 0;
  purge_sys->trx->start_time = ut_time();
  purge_sys->trx->state = TRX_STATE_ACTIVE;
  purge_sys->trx->op_info = "purge trx";

  purge_sys->query = trx_purge_graph_build(purge_sys->trx, n_purge_threads);

  new (&purge_sys->view) ReadView();

  trx_sys->mvcc->clone_oldest_view(&purge_sys->view);

  purge_sys->view_active = true;

  purge_sys->rseg_iter = UT_NEW_NOKEY(TrxUndoRsegsIterator(purge_sys));

  /* Allocate 8K bytes for the initial heap. */
  purge_sys->heap = mem_heap_create(8 * 1024);
}

/************************************************************************
Frees the global purge system control structure. */
void trx_purge_sys_close(void) {
  que_graph_free(purge_sys->query);

  ut_a(purge_sys->trx->id == 0);
  ut_a(purge_sys->sess->trx == purge_sys->trx);

  purge_sys->trx->state = TRX_STATE_NOT_STARTED;

  sess_close(purge_sys->sess);

  purge_sys->sess = NULL;

  purge_sys->view.close();
  purge_sys->view.~ReadView();

  rw_lock_free(&purge_sys->latch);
  mutex_free(&purge_sys->pq_mutex);

  if (purge_sys->purge_queue != NULL) {
    UT_DELETE(purge_sys->purge_queue);
    purge_sys->purge_queue = NULL;
  }

  os_event_destroy(purge_sys->event);

  purge_sys->event = NULL;

  mem_heap_free(purge_sys->heap);

  purge_sys->heap = nullptr;

  UT_DELETE(purge_sys->rseg_iter);

  ut_free(purge_sys);

  purge_sys = NULL;
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
      ib::fatal(ER_IB_MSG_1165) << "undo->id is " << undo->id;
    }

    trx_rsegf_set_nth_undo(rseg_header, undo->id, FIL_NULL, mtr);

    MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_USED);

    hist_size =
        mtr_read_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, mtr);

    ut_ad(undo->size == flst_get_len(seg_header + TRX_UNDO_PAGE_LIST));

    mlog_write_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE,
                     hist_size + undo->size, MLOG_4BYTES, mtr);
  }

  ut_ad(!trx_commit_disallowed);

  /* Add the log as the first in the history list */
  flst_add_first(rseg_header + TRX_RSEG_HISTORY,
                 undo_header + TRX_UNDO_HISTORY_NODE, mtr);

  if (update_rseg_history_len) {
    os_atomic_increment_ulint(&trx_sys->rseg_history_len, n_added_logs);
    srv_wake_purge_thread_if_not_active();
  }

  /* Write the trx number to the undo log header */
  mlog_write_ull(undo_header + TRX_UNDO_TRX_NO, trx->no, mtr);

  /* Write information about delete markings to the undo log header */

  if (!undo->del_marks) {
    mlog_write_ulint(undo_header + TRX_UNDO_DEL_MARKS, FALSE, MLOG_2BYTES, mtr);
  }

  if (rseg->last_page_no == FIL_NULL) {
    rseg->last_page_no = undo->hdr_page_no;
    rseg->last_offset = undo->hdr_offset;
    rseg->last_trx_no = trx->no;
    rseg->last_del_marks = undo->del_marks;
  }
}

/** Remove undo log header from the history list.
@param[in,out]	rseg_hdr	rollback segment header
@param[in]	log_hdr		undo log segment header
@param[in,out]	mtr		mini transaction. */
static void trx_purge_remove_log_hdr(trx_rsegf_t *rseg_hdr,
                                     trx_ulogf_t *log_hdr, mtr_t *mtr) {
  flst_remove(rseg_hdr + TRX_RSEG_HISTORY, log_hdr + TRX_UNDO_HISTORY_NODE,
              mtr);

  os_atomic_decrement_ulint(&trx_sys->rseg_history_len, 1);
}

/** Frees an undo log segment which is in the history list. Removes the
undo log hdr from the history list.
@param[in,out]	rseg		rollback segment
@param[in]	hdr_addr	file address of log_hdr
@param[in]	noredo		skip redo logging. */
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

    mutex_enter(&rseg->mutex);

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
      mlog_write_ulint(log_hdr + TRX_UNDO_DEL_MARKS, FALSE, MLOG_2BYTES, &mtr);
    }

    if (fseg_free_step_not_header(seg_hdr + TRX_UNDO_FSEG_HEADER, false,
                                  &mtr)) {
      break;
    }

    mutex_exit(&rseg->mutex);

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

  ut_ad(rseg->curr_size >= seg_size);

  rseg->curr_size -= seg_size;

  mutex_exit(&(rseg->mutex));

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

  mutex_enter(&(rseg->mutex));

  rseg_hdr =
      trx_rsegf_get(rseg->space_id, rseg->page_no, rseg->page_size, &mtr);

  hdr_addr = trx_purge_get_log_from_hist(
      flst_get_last(rseg_hdr + TRX_RSEG_HISTORY, &mtr));
loop:
  if (hdr_addr.page == FIL_NULL) {
    mutex_exit(&(rseg->mutex));

    mtr_commit(&mtr);

    return;
  }

  undo_page = trx_undo_page_get(page_id_t(rseg->space_id, hdr_addr.page),
                                rseg->page_size, &mtr);

  log_hdr = undo_page + hdr_addr.boffset;

  undo_trx_no = mach_read_from_8(log_hdr + TRX_UNDO_TRX_NO);

  if (undo_trx_no >= limit->trx_no) {
    /* limit space_id should match the rollback segment
    space id to avoid freeing of the page belongs to
    different rollback segment for the same trx_no. */
    if (undo_trx_no == limit->trx_no &&
        rseg->space_id == limit->undo_rseg_space) {
      trx_undo_truncate_start(rseg, hdr_addr.page, hdr_addr.boffset,
                              limit->undo_no);
    }

    mutex_exit(&(rseg->mutex));
    mtr_commit(&mtr);

    return;
  }

  prev_hdr_addr = trx_purge_get_log_from_hist(
      flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE, &mtr));

  seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

  if ((mach_read_from_2(seg_hdr + TRX_UNDO_STATE) == TRX_UNDO_TO_PURGE) &&
      (mach_read_from_2(log_hdr + TRX_UNDO_NEXT_LOG) == 0)) {
    /* We can free the whole log segment */

    mutex_exit(&(rseg->mutex));
    mtr_commit(&mtr);

    /* calls the trx_purge_remove_log_hdr()
    inside trx_purge_free_segment(). */
    trx_purge_free_segment(rseg, hdr_addr, is_temp);

  } else {
    /* Remove the log hdr from the rseg history. */

    trx_purge_remove_log_hdr(rseg_hdr, log_hdr, &mtr);

    mutex_exit(&(rseg->mutex));
    mtr_commit(&mtr);
  }

  mtr_start(&mtr);

  if (is_temp) {
    mtr.set_log_mode(MTR_LOG_NO_REDO);
  }

  mutex_enter(&(rseg->mutex));

  rseg_hdr =
      trx_rsegf_get(rseg->space_id, rseg->page_no, rseg->page_size, &mtr);

  hdr_addr = prev_hdr_addr;

  goto loop;
}

/** UNDO log truncate logger. Needed to track state of truncate during crash.
An auxiliary redo log file undo_<space_id>_trunc.log will created while the
truncate of the UNDO is in progress. This file is required during recovery
to complete the truncate. */

namespace undo {

/** Build a standard undo tablespace name from a space_id.
@param[in]	space_id	id of the undo tablespace.
@return tablespace name of the undo tablespace file */
char *Tablespace::make_space_name(space_id_t space_id) {
  /* 8.0 undo tablespace names have an extra '_' */
  bool old = (id2num(space_id) == space_id);

  size_t size = sizeof("innodb_undo000") + (old ? 0 : 1);

  char *name = static_cast<char *>(ut_malloc_nokey(size));

  snprintf(
      name, size,
      (old ? "innodb_undo%03" SPACE_ID_PFS : "innodb_undo_%03" SPACE_ID_PFS),
      static_cast<unsigned>(id2num(space_id)));

  return (name);
}

/** Build a standard undo tablespace file name from a space_id.
This will create a name like 'undo_001' if the space_id is in the
reserved range, else it will be like 'undo001'.
@param[in]	space_id	id of the undo tablespace.
@return file_name of the undo tablespace file */
char *Tablespace::make_file_name(space_id_t space_id) {
  /* 8.0 undo tablespace names have an extra '_' */
  size_t len = strlen(srv_undo_dir);
  bool with_sep = (srv_undo_dir[len - 1] == OS_PATH_SEPARATOR);
  bool old = (id2num(space_id) == space_id);

  size_t size = strlen(srv_undo_dir) + (with_sep ? 0 : 1) + sizeof("undo000") +
                (old ? 0 : 1);

  char *name = static_cast<char *>(ut_malloc_nokey(size));

  memcpy(name, srv_undo_dir, len);

  if (!with_sep) {
    name[len++] = OS_PATH_SEPARATOR;
  }

  memcpy(&name[len], "undo", 4);
  len += 4;

  if (!old) {
    name[len++] = '_';
  }

  snprintf(&name[len], size - len, "%03" SPACE_ID_PFS,
           static_cast<unsigned>(id2num(space_id)));

  return (name);
}

/** Populate log file name based on space_id
@param[in]	space_id	id of the undo tablespace.
@return DB_SUCCESS or error code */
char *Tablespace::make_log_file_name(space_id_t space_id) {
  size_t size = strlen(srv_log_group_home_dir) + 22 + 1 /* NUL */
                + strlen(undo::s_log_prefix) + strlen(undo::s_log_ext);

  char *name = static_cast<char *>(ut_malloc_nokey(size));

  memset(name, 0, size);

  strcpy(name, srv_log_group_home_dir);
  ulint len = strlen(name);

  if (name[len - 1] != OS_PATH_SEPARATOR) {
    name[len] = OS_PATH_SEPARATOR;
    len = strlen(name);
  }

  snprintf(name + len, size - len, "%s%lu_%s", undo::s_log_prefix,
           (ulong)id2num(space_id), s_log_ext);

  return (name);
}

/** Create the truncate log file.
@param[in]	space_id	id of the undo tablespace to truncate.
@return DB_SUCCESS or error code. */
dberr_t start_logging(space_id_t space_id) {
  dberr_t err;
  Tablespace undo_space(space_id);
  char *log_file_name = undo_space.log_file_name();

  /* Delete the log file if it exists. */
  os_file_delete_if_exists(innodb_log_file_key, log_file_name, NULL);

  /* Create the log file, open it and write 0 to indicate
  init phase. */
  bool ret;
  pfs_os_file_t handle =
      os_file_create(innodb_log_file_key, log_file_name, OS_FILE_CREATE,
                     OS_FILE_NORMAL, OS_LOG_FILE, srv_read_only_mode, &ret);
  if (!ret) {
    return (DB_IO_ERROR);
  }

  ulint sz = UNIV_PAGE_SIZE;
  void *buf = ut_zalloc_nokey(sz + UNIV_PAGE_SIZE);
  if (buf == NULL) {
    os_file_close(handle);
    return (DB_OUT_OF_MEMORY);
  }

  byte *log_buf = static_cast<byte *>(ut_align(buf, UNIV_PAGE_SIZE));

  IORequest request(IORequest::WRITE);

  request.disable_compression();

  err = os_file_write(request, log_file_name, handle, log_buf, 0, sz);

  os_file_flush(handle);
  os_file_close(handle);
  ut_free(buf);

  return (err);
}

/** Mark completion of undo truncate action by writing magic number to
the log file and then removing it from the disk.
If we are going to remove it from disk then why write magic number ?
This is to safeguard from unlink (file-system) anomalies that will keep
the link to the file even after unlink action is successfull and
ref-count = 0.
@param[in]	space_id	id of the undo tablespace to truncate.*/
void done_logging(space_id_t space_id) {
  dberr_t err;
  Tablespace undo_space(space_id);
  char *log_file_name = undo_space.log_file_name();
  bool exist;
  os_file_type_t type;

  /* If this file does not exist, there is nothing to do. */
  os_file_status(log_file_name, &exist, &type);
  if (!exist) {
    return;
  }

  /* Open log file and write magic number to indicate
  done phase. */
  bool ret;
  pfs_os_file_t handle = os_file_create_simple_no_error_handling(
      innodb_log_file_key, log_file_name, OS_FILE_OPEN, OS_FILE_READ_WRITE,
      srv_read_only_mode, &ret);

  if (!ret) {
    os_file_delete(innodb_log_file_key, log_file_name);
    return;
  }

  ulint sz = UNIV_PAGE_SIZE;
  void *buf = ut_zalloc_nokey(sz + UNIV_PAGE_SIZE);
  if (buf == NULL) {
    os_file_close(handle);
    os_file_delete(innodb_log_file_key, log_file_name);
    return;
  }

  byte *log_buf = static_cast<byte *>(ut_align(buf, UNIV_PAGE_SIZE));

  mach_write_to_4(log_buf, undo::s_magic);

  IORequest request(IORequest::WRITE);

  request.disable_compression();

  err = os_file_write(request, log_file_name, handle, log_buf, 0, sz);

  ut_a(err == DB_SUCCESS);

  os_file_flush(handle);
  os_file_close(handle);

  ut_free(buf);
  os_file_delete(innodb_log_file_key, log_file_name);
}

/** Check if TRUNCATE_DDL_LOG file exist.
@param[in]	space_id	id of the undo tablespace.
@return true if exist else false. */
bool is_active_truncate_log_present(space_id_t space_id) {
  Tablespace undo_space(space_id);
  char *log_file_name = undo_space.log_file_name();

  /* Check for existence of the file. */
  bool exist;
  os_file_type_t type;
  os_file_status(log_file_name, &exist, &type);

  /* If file exists, check it for presence of magic
  number.  If found, then delete the file and report file
  doesn't exist as presence of magic number suggest that
  truncate action was complete. */

  if (exist) {
    bool ret;
    pfs_os_file_t handle = os_file_create_simple_no_error_handling(
        innodb_log_file_key, log_file_name, OS_FILE_OPEN, OS_FILE_READ_WRITE,
        srv_read_only_mode, &ret);
    if (!ret) {
      os_file_delete(innodb_log_file_key, log_file_name);
      return (false);
    }

    ulint sz = UNIV_PAGE_SIZE;
    void *buf = ut_zalloc_nokey(sz + UNIV_PAGE_SIZE);
    if (buf == NULL) {
      os_file_close(handle);
      os_file_delete(innodb_log_file_key, log_file_name);
      return (false);
    }

    byte *log_buf = static_cast<byte *>(ut_align(buf, UNIV_PAGE_SIZE));

    IORequest request(IORequest::READ);

    request.disable_compression();

    dberr_t err;

    err = os_file_read(request, handle, log_buf, 0, sz);

    os_file_close(handle);

    if (err != DB_SUCCESS) {
      ib::info(ER_IB_MSG_1166)
          << "Unable to read '" << log_file_name << "' : " << ut_strerr(err);

      os_file_delete(innodb_log_file_key, log_file_name);

      ut_free(buf);

      return (false);
    }

    ulint magic_no = mach_read_from_4(log_buf);

    ut_free(buf);

    if (magic_no == undo::s_magic) {
      /* Found magic number. */
      os_file_delete(innodb_log_file_key, log_file_name);
      return (false);
    }
  }

  return (exist);
}

/** Add undo tablespace to s_under_construction vector.
@param[in]	space_id	space id of tablespace to
truncate */
void add_space_to_construction_list(space_id_t space_id) {
  s_under_construction.push_back(space_id);
}

/** Clear the s_under_construction vector. */
void clear_construction_list() { s_under_construction.clear(); }

/** Is an undo tablespace under constuction at the moment.
@param[in]	space_id	space id to check
@return true if marked for truncate, else false. */
bool is_under_construction(space_id_t space_id) {
  for (auto construct_id : s_under_construction) {
    if (construct_id == space_id) {
      return (true);
    }
  }

  return (false);
}

/* Return whether the undo tablespace is active.  If this is a
non-undo tablespace, then it will not be found in spaces and it
will not be under construction, so this function will return true.
@return true if active (non-undo spaces are always active) */
bool is_active(space_id_t space_id) {
  if (spaces == nullptr) {
    return (!is_under_construction(space_id));
  }

  Tablespace *undo_space = spaces->find(space_id);

  if (undo_space == nullptr) {
    return (!is_under_construction(space_id));
  }

  return (undo_space->rsegs()->is_active());
}
}  // namespace undo

/* Declare this global object. */
Space_Ids undo::s_under_construction;

/** Iterate over all the UNDO tablespaces and check if any of the UNDO
tablespace qualifies for TRUNCATE (size > threshold).
@param[in,out]	undo_trunc	undo truncate tracker */
static void trx_purge_mark_undo_for_truncate(undo::Truncate *undo_trunc) {
  /* We need at least 2 active UNDO tablespaces so that if one undo
  tablespace is being truncated the server will continue to operate.
  The minimum is now 2 so assert that we have at least 2. */
  ut_a(undo::spaces->size() >= FSP_MIN_UNDO_TABLESPACES);

  /* Return immediately if
   1. truncate is disabled or
   2. an undo tablespace is currently marked for truncate. */
  if (!srv_undo_log_truncate || undo_trunc->is_marked()) {
    return;
  }

  /* Find an undo tablespace with size > threshold.
  Avoid bias selection and so start the scan from immediate
  next of last UNDO tablespace selected for truncate.
  Scan active as well as inactive undo tablespaces. */

  undo::spaces->s_lock();

  space_id_t space_id = undo_trunc->get_scan_space_id();
  space_id_t first_space_id_scanned = space_id;
  do {
    if (fil_space_get_size(space_id) >
        (srv_max_undo_tablespace_size / srv_page_size)) {
      /* Tablespace qualifies for truncate. */

      undo_trunc->increment_scan();

      undo_trunc->mark(space_id);

      break;
    }

    space_id = undo_trunc->increment_scan();

  } while (space_id != first_space_id_scanned);

  undo::spaces->s_unlock();

  /* Couldn't make any selection. */
  if (!undo_trunc->is_marked()) {
    return;
  }

#ifdef UNIV_DEBUG
  ut_ad(space_id == undo_trunc->get_marked_space_id());
  ib::info(ER_IB_MSG_1167) << "Undo tablespace number "
                           << undo::id2num(space_id)
                           << " is marked for truncate";
#endif /* UNIV_DEBUG */
}

size_t undo::Truncate::s_scan_pos;

/** Iterate over selected UNDO tablespace and check if all the rsegs
that resides in the tablespace are free.
@param[in]	limit		truncate_limit
@param[in,out]	undo_trunc	undo truncate tracker */
static void trx_purge_initiate_truncate(purge_iter_t *limit,
                                        undo::Truncate *undo_trunc) {
  /* Step-1: Early check to find out if any UNDO tablespace
  is marked for truncate. */
  if (!undo_trunc->is_marked()) {
    /* No tablespace is marked and ready for truncate. */
    return;
  }

  /* If an undo tablespace is marked, its rsegs are inactive. */
  ut_ad(undo_trunc->rsegs()->is_inactive());

  /* Step-2: Scan over each rseg in this inactive undo tablespace
  and ensure that it does not hold any active undo records. */
  bool all_free = true;

  undo_trunc->rsegs()->x_lock();
  for (auto rseg : *undo_trunc->rsegs()) {
    mutex_enter(&rseg->mutex);

    if (rseg->trx_ref_count > 0) {
      /* This rseg is still being held by an active transaction. */
      all_free = false;
    } else if (rseg->last_page_no != FIL_NULL) {
      /* This rseg still has data to be purged. */
      all_free = false;
    }

    mutex_exit(&rseg->mutex);

    if (!all_free) {
      break;
    }
  }
  undo_trunc->rsegs()->x_unlock();

  if (!all_free) {
    /* rseg still holds active data.*/
    return;
  }

  /* Step-3: Start the actual truncate.
  a. log-checkpoint
  b. Write the DDL log to protect truncate action from CRASH
  c. Remove rseg instance if added to purge queue before we
     initiate truncate.
  d. Execute actual truncate
  e. Remove the DDL log. */
  DBUG_EXECUTE_IF("ib_undo_trunc_before_checkpoint",
                  ib::info(ER_IB_MSG_1168) << "ib_undo_trunc_before_checkpoint";
                  DBUG_SUICIDE(););

  /* After truncate if server crashes then redo logging done for this
  undo tablespace might not stand valid as tablespace has been
  truncated. */
  log_make_latest_checkpoint();

  ib::info(ER_IB_MSG_1169) << "Truncating UNDO tablespace number "
                           << undo::id2num(undo_trunc->get_marked_space_id());

  DBUG_EXECUTE_IF("ib_undo_trunc_before_ddl_log_start",
                  ib::info(ER_IB_MSG_1170)
                      << "ib_undo_trunc_before_ddl_log_start";
                  DBUG_SUICIDE(););

  dberr_t err = undo::start_logging(undo_trunc->get_marked_space_id());
  if (err != DB_SUCCESS) {
    ib::error(ER_IB_MSG_1171) << "Cannot create truncate log for undo"
                                 " tablespace ID="
                              << undo_trunc->get_marked_space_id();
  }
  ut_ad(err == DB_SUCCESS);

  DBUG_EXECUTE_IF("ib_undo_trunc_before_truncate",
                  ib::info(ER_IB_MSG_1172) << "ib_undo_trunc_before_truncate";
                  DBUG_SUICIDE(););

  bool success = trx_undo_truncate_tablespace(undo_trunc);
  if (!success) {
    /* Note: In case of error we don't enable the rsegs
    and neither unmark the tablespace so the tablespace
    continue to remain inactive. */
    ib::error(ER_IB_MSG_1173)
        << "Failed to truncate undo tablespace number"
        << undo::id2num(undo_trunc->get_marked_space_id());
    return;
  }

  DBUG_EXECUTE_IF("ib_undo_trunc_before_ddl_log_end",
                  ib::info(ER_IB_MSG_1174)
                      << "ib_undo_trunc_before_ddl_log_end";
                  DBUG_SUICIDE(););

  log_make_latest_checkpoint();

  undo::done_logging(undo_trunc->get_marked_space_id());

  /* Completed truncate. Now it is safe to re-use the tablespace. */
  space_id_t marked_space_id = undo_trunc->get_marked_space_id();
  Rsegs *marked_rsegs = undo_trunc->rsegs();

  marked_rsegs->x_lock();

  marked_rsegs->set_active();

  undo_trunc->reset();

  marked_rsegs->x_unlock();

  ib::info(ER_IB_MSG_1175) << "Completed truncate of undo tablespace number "
                           << undo::id2num(marked_space_id);

  DBUG_EXECUTE_IF("ib_undo_trunc_trunc_done", ib::info(ER_IB_MSG_1176)
                                                  << "ib_undo_trunc_trunc_done";
                  DBUG_SUICIDE(););
}

/** Removes unnecessary history data from rollback segments. NOTE that when this
 function is called, the caller must not have any latches on undo log pages! */
static void trx_purge_truncate_history(
    purge_iter_t *limit,  /*!< in: truncate limit */
    const ReadView *view) /*!< in: purge view */
{
  ulint i;

  /* We play safe and set the truncate limit at most to the purge view
  low_limit number, though this is not necessary */

  if (limit->trx_no >= view->low_limit_no()) {
    limit->trx_no = view->low_limit_no();
    limit->undo_no = 0;
    limit->undo_rseg_space = SPACE_UNKNOWN;
  }

  ut_ad(limit->trx_no <= purge_sys->view.low_limit_no());

  /* Purge rollback segments in all undo tablespaces. */
  bool undo_list_is_locked = false;
  for (auto undo_space : undo::spaces->m_spaces) {
    if (!undo_list_is_locked && undo_space->num() >= srv_undo_tablespaces) {
      /* These undo spaces are inactive */
      undo::spaces->s_lock();
      undo_list_is_locked = true;
    }

    /* Purge rollback segments in this undo tablespace.
    Use an s-lock only for inactive rsegs. */
    bool rseg_list_is_locked = false;
    for (auto rseg : *undo_space->rsegs()) {
      if (!rseg_list_is_locked && rseg->id >= srv_rollback_segments) {
        /* These rsegs are inactive */
        undo_space->rsegs()->s_lock();
        rseg_list_is_locked = true;
      }

      trx_purge_truncate_rseg_history(rseg, limit);
    }
    if (rseg_list_is_locked) {
      undo_space->rsegs()->s_unlock();
    }
  }
  if (undo_list_is_locked) {
    undo::spaces->s_unlock();
  }

  /* Purge rollback segments in the system tablespace, if any.
  Use an s-lock for the whole list since it can have gaps and
  may be sorted when added to. */
  trx_sys->rsegs.s_lock();
  for (auto rseg : trx_sys->rsegs) {
    trx_purge_truncate_rseg_history(rseg, limit);
  }
  trx_sys->rsegs.s_unlock();

  /* Purge rollback segments in the temporary tablespace.
  Use an s-lock only for inactive rsegs. */
  bool tmp_rseg_list_is_locked = false;
  for (auto rseg : trx_sys->tmp_rsegs) {
    if (!tmp_rseg_list_is_locked && rseg->id >= srv_rollback_segments) {
      /* These rsegs are inactive */
      trx_sys->tmp_rsegs.s_lock();
      tmp_rseg_list_is_locked = true;
    }

    trx_purge_truncate_rseg_history(rseg, limit);
  }
  if (tmp_rseg_list_is_locked) {
    trx_sys->tmp_rsegs.s_unlock();
  }

  /* UNDO tablespace truncate. We will try to truncate as much as we
  can (greedy approach). This will ensure when the server is idle we
  try and truncate all the UNDO tablespaces. */
  undo::spaces->s_lock();
  ulint n_spaces = undo::spaces->size();
  undo::spaces->s_unlock();
  for (i = 0; i < n_spaces; i++) {
    trx_purge_mark_undo_for_truncate(&purge_sys->undo_trunc);

    /* Don't truncate if concurrent clone in progress. */
    if (clone_mark_abort(false)) {
      trx_purge_initiate_truncate(limit, &purge_sys->undo_trunc);
      clone_mark_active();
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
  page_t *undo_page;
  trx_ulogf_t *log_hdr;
  fil_addr_t prev_log_addr;
  trx_id_t trx_no;
  ibool del_marks;
  mtr_t mtr;

  mutex_enter(&(rseg->mutex));

  ut_a(rseg->last_page_no != FIL_NULL);

  purge_sys->iter.trx_no = rseg->last_trx_no + 1;
  purge_sys->iter.undo_no = 0;
  purge_sys->iter.undo_rseg_space = SPACE_UNKNOWN;
  purge_sys->next_stored = FALSE;

  mtr_start(&mtr);

  undo_page = trx_undo_page_get_s_latched(
      page_id_t(rseg->space_id, rseg->last_page_no), rseg->page_size, &mtr);

  log_hdr = undo_page + rseg->last_offset;

  /* Increase the purge page count by one for every handled log */

  (*n_pages_handled)++;

  prev_log_addr = trx_purge_get_log_from_hist(
      flst_get_prev_addr(log_hdr + TRX_UNDO_HISTORY_NODE, &mtr));

  if (prev_log_addr.page == FIL_NULL) {
    /* No logs left in the history list */

    rseg->last_page_no = FIL_NULL;

    mutex_exit(&(rseg->mutex));
    mtr_commit(&mtr);

#ifdef UNIV_DEBUG
    trx_sys_mutex_enter();

    /* Add debug code to track history list corruption reported
    on the MySQL mailing list on Nov 9, 2004. The fut0lst.cc
    file-based list was corrupt. The prev node pointer was
    FIL_NULL, even though the list length was over 8 million nodes!
    We assume that purge truncates the history list in large
    size pieces, and if we here reach the head of the list, the
    list cannot be longer than 2000 000 undo logs now. */

    if (trx_sys->rseg_history_len > 2000000) {
      ib::warn(ER_IB_MSG_1177) << "Purge reached the head of the history"
                                  " list, but its length is still reported as "
                               << trx_sys->rseg_history_len
                               << " which is"
                                  " unusually high.";
      ib::info(ER_IB_MSG_1178) << "This can happen for multiple reasons";
      ib::info(ER_IB_MSG_1179) << "1. A long running transaction is"
                                  " withholding purging of undo logs or a read"
                                  " view is open. Please try to commit the long"
                                  " running transaction.";
      ib::info(ER_IB_MSG_1180) << "2. Try increasing the number of purge"
                                  " threads to expedite purging of undo logs.";
    }

    trx_sys_mutex_exit();
#endif
    return;
  }

  mutex_exit(&rseg->mutex);

  mtr_commit(&mtr);

  /* Read the trx number and del marks from the previous log header */
  mtr_start(&mtr);

  log_hdr =
      trx_undo_page_get_s_latched(page_id_t(rseg->space_id, prev_log_addr.page),
                                  rseg->page_size, &mtr) +
      prev_log_addr.boffset;

  trx_no = mach_read_from_8(log_hdr + TRX_UNDO_TRX_NO);

  del_marks = mach_read_from_2(log_hdr + TRX_UNDO_DEL_MARKS);

  mtr_commit(&mtr);

  mutex_enter(&(rseg->mutex));

  rseg->last_page_no = prev_log_addr.page;
  rseg->last_offset = prev_log_addr.boffset;
  rseg->last_trx_no = trx_no;
  rseg->last_del_marks = del_marks;

  TrxUndoRsegs elem(rseg->last_trx_no);
  elem.push_back(rseg);

  /* Purge can also produce events, however these are already ordered
  in the rollback segment and any user generated event will be greater
  than the events that Purge produces. ie. Purge can never produce
  events from an empty rollback segment. */

  mutex_enter(&purge_sys->pq_mutex);

  purge_sys->purge_queue->push(elem);

  mutex_exit(&purge_sys->pq_mutex);

  mutex_exit(&rseg->mutex);
}

/** Position the purge sys "iterator" on the undo record to use for purging.
@param[in,out]	purge_sys	purge instance
@param[in]	page_size	page size */
static void trx_purge_read_undo_rec(trx_purge_t *purge_sys,
                                    const page_size_t &page_size) {
  ulint offset;
  page_no_t page_no;
  ib_uint64_t undo_no;
  space_id_t undo_rseg_space;
  trx_id_t modifier_trx_id;

  purge_sys->hdr_offset = purge_sys->rseg->last_offset;
  page_no = purge_sys->hdr_page_no = purge_sys->rseg->last_page_no;

  if (purge_sys->rseg->last_del_marks) {
    mtr_t mtr;
    trx_undo_rec_t *undo_rec = NULL;

    mtr_start(&mtr);

    undo_rec = trx_undo_get_first_rec(
        &modifier_trx_id, purge_sys->rseg->space_id, page_size,
        purge_sys->hdr_page_no, purge_sys->hdr_offset, RW_S_LATCH, &mtr);

    if (undo_rec != NULL) {
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

  purge_sys->next_stored = TRUE;
}

/** Chooses the next undo log to purge and updates the info in purge_sys. This
 function is used to initialize purge_sys when the next record to purge is
 not known, and also to update the purge system info on the next record when
 purge has handled the whole undo log for a transaction. */
static void trx_purge_choose_next_log(void) {
  ut_ad(purge_sys->next_stored == FALSE);

  const page_size_t &page_size = purge_sys->rseg_iter->set_next();

  if (purge_sys->rseg != NULL) {
    trx_purge_read_undo_rec(purge_sys, page_size);
  } else {
    /* There is nothing to do yet. */
    os_thread_yield();
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
  ut_ad(purge_sys->iter.trx_no < purge_sys->view.low_limit_no());

  space = purge_sys->rseg->space_id;
  page_no = purge_sys->page_no;
  offset = purge_sys->offset;

  const page_size_t page_size(purge_sys->rseg->page_size);

  if (offset == 0) {
    /* It is the dummy undo log record, which means that there is
    no need to purge this undo log */

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

    /* Try first to find the next record which requires a purge
    operation from the same page of the same undo log */

    next_rec = trx_undo_page_get_next_rec(rec2, purge_sys->hdr_page_no,
                                          purge_sys->hdr_offset);

    if (next_rec == NULL) {
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

  if (rec2 == NULL) {
    mtr_commit(&mtr);

    trx_purge_rseg_get_next_history_log(purge_sys->rseg, n_pages_handled);

    /* Look for the next undo log and record to purge */

    trx_purge_choose_next_log();

    mtr_start(&mtr);

    undo_page =
        trx_undo_page_get_s_latched(page_id_t(space, page_no), page_size, &mtr);

    rec = undo_page + offset;
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

  rec_copy = trx_undo_rec_copy(rec, heap);

  mtr_commit(&mtr);

  return (rec_copy);
}

/** Fetches the next undo log record from the history list to purge. It must be
 released with the corresponding release function.
 @return copy of an undo log record or pointer to trx_purge_ignore_rec,
 if the whole undo log can skipped in purge; NULL if none left */
static MY_ATTRIBUTE((warn_unused_result))
    trx_undo_rec_t *trx_purge_fetch_next_rec(
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
      return (NULL);
    }
  }

  if (purge_sys->iter.trx_no >= purge_sys->view.low_limit_no()) {
    return (NULL);
  }

  /* fprintf(stderr, "Thread %lu purging trx %llu undo record %llu\n",
  os_thread_get_curr_id(), iter->trx_no, iter->undo_no); */

  *roll_ptr = trx_undo_build_roll_ptr(FALSE, purge_sys->rseg->space_id,
                                      purge_sys->page_no, purge_sys->offset);

  *modifier_trx_id = purge_sys->iter.modifier_trx_id;

  /* The following call will advance the stored values of the
  purge iterator. */

  return (trx_purge_get_next_rec(n_pages_handled, heap));
}

/** This function runs a purge batch.
@param[in]	n_purge_threads	number of purge threads
@param[in,out]	purge_sys	purge instance
@param[in]	batch_size	no. of pages to purge
@return number of undo log pages handled in the batch */
static ulint trx_purge_attach_undo_recs(const ulint n_purge_threads,
                                        trx_purge_t *purge_sys,
                                        ulint batch_size) {
  que_thr_t *thr;
  ulint n_pages_handled = 0;

  ut_a(n_purge_threads > 0);
  ut_a(n_purge_threads <= MAX_PURGE_THREADS);

  purge_sys->limit = purge_sys->iter;

  que_thr_t *run_thrs[MAX_PURGE_THREADS];

  /* Validate some pre-requisites and reset done flag. */
  ulint i = 0;

  for (thr = UT_LIST_GET_FIRST(purge_sys->query->thrs);
       thr != NULL && i < n_purge_threads;
       thr = UT_LIST_GET_NEXT(thrs, thr), ++i) {
    purge_node_t *node;

    /* Get the purge node. */
    node = static_cast<purge_node_t *>(thr->child);

    ut_a(que_node_get_type(node) == QUE_NODE_PURGE);
    ut_a(node->recs == nullptr);
    ut_a(node->done);

    node->done = false;

    ut_a(!thr->is_active);

    run_thrs[i] = thr;
  }

  /* There should never be fewer nodes than threads, the inverse
  however is allowed because we only use purge threads as needed. */
  ut_a(i == n_purge_threads);
  ut_ad(trx_purge_check_limit());

  mem_heap_t *heap = purge_sys->heap;

  mem_heap_empty(heap);

  using GroupBy = std::map<
      table_id_t, purge_node_t::Recs *, std::less<table_id_t>,
      mem_heap_allocator<std::pair<const table_id_t, purge_node_t::Recs *>>>;

  GroupBy group_by{GroupBy::key_compare{},
                   mem_heap_allocator<GroupBy::value_type>{heap}};

  for (ulint i = 0; n_pages_handled < batch_size; ++i) {
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

    table_id_t table_id;

    table_id = trx_undo_rec_get_table_id(rec.undo_rec);

    GroupBy::iterator lb = group_by.lower_bound(table_id);

    if (lb != group_by.end() && !(group_by.key_comp()(table_id, lb->first))) {
      lb->second->push_back(rec);

    } else {
      using value_type = GroupBy::value_type;

      void *ptr;
      purge_node_t::Recs *recs;

      ptr = mem_heap_alloc(heap, sizeof(purge_node_t::Recs));

      /* Call the destructor explicitly in row_purge_end() */
      recs = new (ptr)
          purge_node_t::Recs{mem_heap_allocator<purge_node_t::rec_t>{heap}};

      recs->push_back(rec);

      group_by.insert(lb, value_type(table_id, recs));
    }
  }

  /* Objective is to ensure that all the table entries in one
  batch are handled by the same thread. Ths is to avoid contention
  on the dict_index_t::lock */

  GroupBy::const_iterator end = group_by.cend();

  for (GroupBy::const_iterator it = group_by.cbegin(); it != end;) {
    for (ulint i = 0; i < n_purge_threads && it != end; ++i, ++it) {
      purge_node_t *node;

      node = static_cast<purge_node_t *>(run_thrs[i]->child);

      ut_a(que_node_get_type(node) == QUE_NODE_PURGE);

      if (node->recs == nullptr) {
        node->recs = it->second;
      } else {
        for (auto iter = it->second->begin(); iter != it->second->end();
             ++iter) {
          node->recs->push_back(*iter);
        }
      }
    }
  }

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

  /* If purge lag is set (ie. > 0) then calculate the new DML delay.
  Note: we do a dirty read of the trx_sys_t data structure here,
  without holding trx_sys->mutex. */

  if (srv_max_purge_lag > 0) {
    float ratio;

    ratio = float(trx_sys->rseg_history_len) / srv_max_purge_lag;

    if (ratio > 1.0) {
      /* If the history list length exceeds the
      srv_max_purge_lag, the data manipulation
      statements are delayed by at least 5000
      microseconds. */
      delay = (ulint)((ratio - .5) * 10000);
    }

    if (delay > srv_max_purge_lag_delay) {
      delay = srv_max_purge_lag_delay;
    }

    MONITOR_SET(MONITOR_DML_PURGE_DELAY, delay);
  }

  return (delay);
}

/** Wait for pending purge jobs to complete. */
static void trx_purge_wait_for_workers_to_complete(
    trx_purge_t *purge_sys) /*!< in: purge instance */
{
  ulint i = 0;
  ulint n_submitted = purge_sys->n_submitted;

  /* Ensure that the work queue empties out. */
  while (!os_compare_and_swap_ulint(&purge_sys->n_completed, n_submitted,
                                    n_submitted)) {
    if (++i < 10) {
      os_thread_yield();
    } else {
      if (srv_get_task_queue_length() > 0) {
        srv_release_threads(SRV_WORKER, 1);
      }

      os_thread_sleep(20);
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
    trx_purge_truncate_history(&purge_sys->iter, &purge_sys->view);
  } else {
    trx_purge_truncate_history(&purge_sys->limit, &purge_sys->view);
  }
}

/** This function runs a purge batch.
 @return number of undo log pages handled in the batch */
ulint trx_purge(ulint n_purge_threads, /*!< in: number of purge tasks
                                       to submit to the work queue */
                ulint batch_size,      /*!< in: the maximum number of records
                                       to purge in one batch */
                bool truncate)         /*!< in: truncate history if true */
{
  que_thr_t *thr = NULL;
  ulint n_pages_handled;

  ut_a(n_purge_threads > 0);

  srv_dml_needed_delay = trx_purge_dml_delay();

  /* The number of tasks submitted should be completed. */
  ut_a(purge_sys->n_submitted == purge_sys->n_completed);

  rw_lock_x_lock(&purge_sys->latch);

  purge_sys->view_active = false;

  trx_sys->mvcc->clone_oldest_view(&purge_sys->view);

  purge_sys->view_active = true;

  rw_lock_x_unlock(&purge_sys->latch);

#ifdef UNIV_DEBUG
  if (srv_purge_view_update_only_debug) {
    return (0);
  }
#endif /* UNIV_DEBUG */

  /* Fetch the UNDO recs that need to be purged. */
  n_pages_handled =
      trx_purge_attach_undo_recs(n_purge_threads, purge_sys, batch_size);

  /* Do we do an asynchronous purge or not ? */
  if (n_purge_threads > 1) {
    /* Submit the tasks to the work queue. */
    for (ulint i = 0; i < n_purge_threads - 1; ++i) {
      thr = que_fork_scheduler_round_robin(purge_sys->query, thr);

      ut_a(thr != NULL);

      srv_que_task_enqueue_low(thr);
    }

    thr = que_fork_scheduler_round_robin(purge_sys->query, thr);
    ut_a(thr != NULL);

    purge_sys->n_submitted += n_purge_threads - 1;

    goto run_synchronously;

    /* Do it synchronously. */
  } else {
    thr = que_fork_scheduler_round_robin(purge_sys->query, NULL);
    ut_ad(thr);

  run_synchronously:
    ++purge_sys->n_submitted;

    que_run_threads(thr);

    os_atomic_inc_ulint(&purge_sys->pq_mutex, &purge_sys->n_completed, 1);

    if (n_purge_threads > 1) {
      trx_purge_wait_for_workers_to_complete(purge_sys);
    }
  }

  ut_a(purge_sys->n_submitted == purge_sys->n_completed);

#ifdef UNIV_DEBUG
  rw_lock_x_lock(&purge_sys->latch);
  if (purge_sys->limit.trx_no == 0) {
    purge_sys->done = purge_sys->iter;
  } else {
    purge_sys->done = purge_sys->limit;
  }
  rw_lock_x_unlock(&purge_sys->latch);
#endif /* UNIV_DEBUG */

  /* During upgrade, to know whether purge is empty,
  we rely on purge history length. So truncate the
  undo logs during upgrade to update purge history
  length. */
  if (truncate || srv_upgrade_old_undo_found) {
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

  rw_lock_x_lock(&purge_sys->latch);

  state = purge_sys->state;

  rw_lock_x_unlock(&purge_sys->latch);

  return (state);
}

/** Stop purge and wait for it to stop, move to PURGE_STATE_STOP. */
void trx_purge_stop(void) {
  purge_state_t state;
  int64_t sig_count = os_event_reset(purge_sys->event);

  ut_a(srv_n_purge_threads > 0);

  rw_lock_x_lock(&purge_sys->latch);

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

    rw_lock_x_lock(&purge_sys->latch);

    /* Wait for purge to signal that it has actually stopped. */
    while (purge_sys->running) {
      if (once) {
        ib::info(ER_IB_MSG_1182) << "Waiting for purge to stop";
        once = false;
      }

      rw_lock_x_unlock(&purge_sys->latch);

      os_thread_sleep(10000);

      rw_lock_x_lock(&purge_sys->latch);
    }

    rw_lock_x_unlock(&purge_sys->latch);
  }

  MONITOR_INC_VALUE(MONITOR_PURGE_STOP_COUNT, 1);
}

/** Resume purge, move to PURGE_STATE_RUN. */
void trx_purge_run(void) {
  rw_lock_x_lock(&purge_sys->latch);

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

/** Initialize the undo::Tablespaces object. */
void undo::Tablespaces::init() {
  /** Fix the size of the vector so that it will not do
  allocations for inserts. This way the contents can be
  read without using a latch. */
  m_spaces.reserve(FSP_MAX_UNDO_TABLESPACES);

  m_latch = static_cast<rw_lock_t *>(ut_zalloc_nokey(sizeof(*m_latch)));

  rw_lock_create(undo_spaces_lock_key, m_latch, SYNC_UNDO_SPACES);
}

/** De-initialize the undo::Tablespaces object. */
void undo::Tablespaces::deinit() {
  clear();

  rw_lock_free(m_latch);
  ut_free(m_latch);
  m_latch = nullptr;
}

/** Add a new space_id to the back of the vector.
The vector has been pre-allocated to 128 so read threads will
not loose what is pointed to.
@param[in]	id	tablespace ID */
void undo::Tablespaces::add(space_id_t id) {
  ut_ad(is_reserved(id));

  if (contains(id)) {
    return;
  }

  auto undo_space = UT_NEW_NOKEY(Tablespace(id, true));

  m_spaces.push_back(undo_space);

  /* This list is set active in trx_rsegs_init() and
  trx_rseg_add_rollback_segments() once all the rsegs are
  ready to be used. */
  undo_space->rsegs()->set_inactive();
}
