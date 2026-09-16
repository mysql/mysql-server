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

/** @file include/trx0purge.h
 Purge old versions

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#ifndef trx0purge_h
#define trx0purge_h

#include "fil0fil.h"
#include "mtr0mtr.h"
#include "page0page.h"
#include "que0types.h"
#include "read0read_view_interface.h"
#include "trx0sys.h"
#include "trx0types.h"
#include "trx0undo_trunc.h"
#include "univ.i"
#include "usr0sess.h"
#ifdef UNIV_HOTBACKUP
#include "trx0sys.h"
#endif /* UNIV_HOTBACKUP */

/** The global data structure coordinating a purge */
extern trx_purge_t *purge_sys;

/** Calculates the file address of an undo log header when we have the file
 address of its history list node.
 @return file address of the log */
static inline fil_addr_t trx_purge_get_log_from_hist(
    fil_addr_t node_addr); /*!< in: file address of the history
                           list node of the log */

/** Initialize in-memory purge structures */
void trx_purge_sys_mem_create();

/** Creates the global purge system control structure and inits the history
mutex.
@param[in]      n_purge_threads   number of purge threads
@param[in,out]  purge_queue       UNDO log min binary heap */
void trx_purge_sys_initialize(uint32_t n_purge_threads,
                              purge_pq_t *purge_queue);

/** Frees the global purge system control structure. */
void trx_purge_sys_close(void);

/************************************************************************
Adds the update undo log as the first log in the history list. Removes the
update undo log segment from the rseg slot if it is too big for reuse. */
void trx_purge_add_update_undo_to_history(
    trx_t *trx,               /*!< in: transaction */
    trx_undo_ptr_t *undo_ptr, /*!< in: update undo log. */
    page_t *undo_page,        /*!< in: update undo log header page,
                              x-latched */
    bool update_rseg_history_len,
    /*!< in: if true: update rseg history
    len else skip updating it. */
    ulint n_added_logs, /*!< in: number of logs added */
    mtr_t *mtr);        /*!< in: mtr */

/** This function runs a purge batch.
 @return number of undo log pages handled in the batch */
ulint trx_purge(ulint n_purge_threads, /*!< in: number of purge tasks to
                                       submit to task queue. */
                ulint limit,           /*!< in: the maximum number of
                                       records to purge in one batch */
                bool truncate);        /*!< in: truncate history if true */

/** Stop purge and wait for it to stop, move to PURGE_STATE_STOP. */
void trx_purge_stop(void);
/** Resume purge, move to PURGE_STATE_RUN. */
void trx_purge_run();

/** Purge states */
enum purge_state_t {
  PURGE_STATE_INIT,    /*!< Purge instance created */
  PURGE_STATE_RUN,     /*!< Purge should be running */
  PURGE_STATE_STOP,    /*!< Purge should be stopped */
  PURGE_STATE_EXIT,    /*!< Purge has been shutdown */
  PURGE_STATE_DISABLED /*!< Purge was never started */
};

/** Get the purge state.
 @return purge state. */
purge_state_t trx_purge_state(void);

// Forward declaration
struct TrxUndoRsegsIterator;

/** This is the purge pointer/iterator. We need both the undo no and the
transaction no up to which purge has parsed and applied the records. */
struct purge_iter_t {
  purge_iter_t() : trx_no(), undo_no(), undo_rseg_space(SPACE_UNKNOWN) {
    // Do nothing
  }

  /** Purge has advanced past all transactions whose number
  is less than this */
  trx_id_t trx_no;

  /** Purge has advanced past all records whose undo number
  is less than this. */
  undo_no_t undo_no;

  /** The last undo record resided in this space id */
  space_id_t undo_rseg_space;

  /** The transaction that created the undo log record,
  the Modifier trx id */
  trx_id_t modifier_trx_id;
};

/** The control structure used in the purge operation */
struct trx_purge_t {
  /** System session running the purge query */
  sess_t *sess;

  /** System transaction running the purge query: this trx is not in the trx
  list of the trx system and it never ends */
  trx_t *trx;
#ifndef UNIV_HOTBACKUP
  /** The latch protecting the purge view. A purge operation must acquire an
  x-latch here for the instant at which it changes the purge view: an undo
  log operation can prevent this by obtaining an s-latch here. It also
  protects state and running */
  rw_lock_t latch;
#endif /* !UNIV_HOTBACKUP */

  /** State signal event */
  os_event_t event;

  /** Counter to track number stops */
  ulint n_stop;

  /** true, if purge is active, we check this without the latch too */
  std::atomic<bool> running;

  /** Purge coordinator thread states, we check this in several places without
  holding the latch. */
  volatile purge_state_t state;

  /** The query graph which will do the parallelized purge operation */
  que_t *query;

  /** The purge will not remove undo logs which are >= this view (purge view) */
  Read_view_interface *view;

  /** This is computed as a lower-bound of minimum of:
  - the smallest trx->no still needed by the oldest open read view
  - the smallest trx->no still needed by GTID persistor
  The purge can remove only the Undo Logs which have TRX_UNDO_TRX_NO strictly
  smaller than this value. */
  trx_id_t m_lowest_needed_trx_no;

  /** Count of total tasks submitted to the task queue */
  ulint n_submitted;

  /** Count of total tasks completed */
  std::atomic<ulint> n_completed;

  /* The following two fields form the 'purge pointer' which advances
  during a purge, and which is used in history list truncation */

  /** Limit up to which we have read and parsed the UNDO log records.  Not
  necessarily purged from the indexes.  Note that this can never be less than
  the limit below, we check for this invariant in trx0purge.cc */
  purge_iter_t iter;

  /** The 'purge pointer' which advances during a purge, and which is used in
  history list truncation */
  purge_iter_t limit;
#ifdef UNIV_DEBUG
  /** Indicate 'purge pointer' which have purged already accurately. */
  purge_iter_t done;
#endif /* UNIV_DEBUG */

  /** true if the info of the next record to purge is stored below: if yes, then
  the transaction number and the undo number of the record are stored in
  purge_trx_no and purge_undo_no above */
  bool next_stored;

  /** Rollback segment for the next undo record to purge */
  trx_rseg_t *rseg;

  /** Page number for the next undo record to purge, page number of the log
  header, if dummy record */
  page_no_t page_no;

  /** Page offset for the next undo record to purge, 0 if the dummy record */
  ulint offset;

  /** Header page of the undo log where the next record to purge belongs */
  page_no_t hdr_page_no;

  /** Header byte offset on the page */
  ulint hdr_offset;

  /** Iterator to get the next rseg to process */
  TrxUndoRsegsIterator *rseg_iter;

  /** Binary min-heap, ordered on TrxUndoRsegs::trx_no. It is protected
  by the pq_mutex */
  purge_pq_t *purge_queue;

  /** Mutex protecting purge_queue */
  PQMutex pq_mutex;

  /** Track UNDO tablespace marked for truncate. */
  undo_truncate::Truncate undo_trunc;

  /** Heap for reading the undo log records */
  mem_heap_t *heap;

  /** Is the this thread related to purge? This is false by default and set to
  true by srv_purge_coordinator_thread() and srv_worker_thread() only. */
  static inline thread_local bool is_this_a_purge_thread{false};

  /** Set of all rseg queue. */
  std::vector<trx_rseg_t *> rsegs_queue;
};

/** Choose the rollback segment with the smallest trx_no. */
struct TrxUndoRsegsIterator {
  /** Constructor */
  TrxUndoRsegsIterator(trx_purge_t *purge_sys);

  /** Sets the next rseg to purge in m_purge_sys.
  @return page size of the table for which the log is.
  NOTE: if rseg is NULL when this function returns this means that
  there are no rollback segments to purge and then the returned page
  size object should not be used. */
  const page_size_t set_next();

 private:
  // Disable copying
  TrxUndoRsegsIterator(const TrxUndoRsegsIterator &);
  TrxUndoRsegsIterator &operator=(const TrxUndoRsegsIterator &);

  /** The purge system pointer */
  trx_purge_t *m_purge_sys;

  /** The current element to process */
  TrxUndoRsegs m_trx_undo_rsegs;

  /** Track the current element in m_trx_undo_rseg */
  typename Rsegs_array<2>::iterator m_iter;

  /** Sentinel value */
  static const TrxUndoRsegs NullElement;
};

#include "trx0purge.ic"

#endif /* trx0purge_h */
