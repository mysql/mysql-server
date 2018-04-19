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

/** @file trx/trx0rseg.cc
 Rollback segment

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#include "trx0rseg.h"

#include <stddef.h>
#include <algorithm>

#include "fsp0sysspace.h"
#include "fut0lst.h"
#include "srv0mon.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0purge.h"
#include "trx0undo.h"

/** Creates a rollback segment header.
This function is called only when a new rollback segment is created in
the database.
@param[in]	space_id	space id
@param[in]	page_size	page size
@param[in]	max_size	max size in pages
@param[in]	rseg_slot	rseg id == slot number in RSEG_ARRAY
@param[in,out]	mtr		mini-transaction
@return page number of the created segment, FIL_NULL if fail */
page_no_t trx_rseg_header_create(space_id_t space_id,
                                 const page_size_t &page_size,
                                 page_no_t max_size, ulint rseg_slot,
                                 mtr_t *mtr) {
  page_no_t page_no;
  trx_rsegf_t *rsegf;
  trx_sysf_t *sys_header;
  trx_rsegsf_t *rsegs_header;
  ulint i;
  buf_block_t *block;

  ut_ad(mtr);
  ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space_id), MTR_MEMO_X_LOCK));

  /* Allocate a new file segment for the rollback segment */
  block = fseg_create(space_id, 0, TRX_RSEG + TRX_RSEG_FSEG_HEADER, mtr);

  if (block == NULL) {
    return (FIL_NULL); /* No space left */
  }

  buf_block_dbg_add_level(block, SYNC_RSEG_HEADER_NEW);

  page_no = block->page.id.page_no();

  /* Get the rollback segment file page */
  rsegf = trx_rsegf_get_new(space_id, page_no, page_size, mtr);

  /* Initialize max size field */
  mlog_write_ulint(rsegf + TRX_RSEG_MAX_SIZE, max_size, MLOG_4BYTES, mtr);

  /* Initialize the history list */
  mlog_write_ulint(rsegf + TRX_RSEG_HISTORY_SIZE, 0, MLOG_4BYTES, mtr);
  flst_init(rsegf + TRX_RSEG_HISTORY, mtr);

  /* Reset the undo log slots */
  for (i = 0; i < TRX_RSEG_N_SLOTS; i++) {
    trx_rsegf_set_nth_undo(rsegf, i, FIL_NULL, mtr);
  }

  if (space_id == TRX_SYS_SPACE) {
    /* All rollback segments in the system tablespace need
    to be found in the TRX_SYS page in the rseg_id slot.
    Add the rollback segment info to the free slot in the
    trx system header in the TRX_SYS page. */

    sys_header = trx_sysf_get(mtr);

    trx_sysf_rseg_set_space(sys_header, rseg_slot, space_id, mtr);

    trx_sysf_rseg_set_page_no(sys_header, rseg_slot, page_no, mtr);

  } else if (fsp_is_system_temporary(space_id)) {
    /* Rollback segments in the system temporary tablespace
    are re-created on restart. So they only need to be
    referenced in memory. */

  } else {
    /* Rollback Segments in independent undo tablespaces
    are tracked in the RSEG_ARRAY page. */
    rsegs_header = trx_rsegsf_get(space_id, mtr);

    trx_rsegsf_set_page_no(rsegs_header, rseg_slot, page_no, mtr);
  }

  return (page_no);
}

/** Free an instance of the rollback segment in memory.
@param[in]	rseg	pointer to an rseg to free */
void trx_rseg_mem_free(trx_rseg_t *rseg) {
  trx_undo_t *undo;
  trx_undo_t *next_undo;

  mutex_free(&rseg->mutex);

  /* There can't be any active transactions. */
  ut_a(UT_LIST_GET_LEN(rseg->update_undo_list) == 0);
  ut_a(UT_LIST_GET_LEN(rseg->insert_undo_list) == 0);

  for (undo = UT_LIST_GET_FIRST(rseg->update_undo_cached); undo != NULL;
       undo = next_undo) {
    next_undo = UT_LIST_GET_NEXT(undo_list, undo);

    UT_LIST_REMOVE(rseg->update_undo_cached, undo);

    MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_CACHED);

    trx_undo_mem_free(undo);
  }

  for (undo = UT_LIST_GET_FIRST(rseg->insert_undo_cached); undo != NULL;
       undo = next_undo) {
    next_undo = UT_LIST_GET_NEXT(undo_list, undo);

    UT_LIST_REMOVE(rseg->insert_undo_cached, undo);

    MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_CACHED);

    trx_undo_mem_free(undo);
  }

  ut_free(rseg);
}

/** Create and initialize a rollback segment object.  Some of
the values for the fields are read from the segment header page.
The caller must insert it into the correct list.
@param[in]	id		rollback segment id
@param[in]	space_id	space where the segment is placed
@param[in]	page_no		page number of the segment header
@param[in]	page_size	page size
@param[in,out]	purge_queue	rseg queue
@param[in,out]	mtr		mini-transaction
@return own: rollback segment object */
trx_rseg_t *trx_rseg_mem_create(ulint id, space_id_t space_id,
                                page_no_t page_no, const page_size_t &page_size,
                                purge_pq_t *purge_queue, mtr_t *mtr) {
  ulint len;
  trx_rseg_t *rseg;
  fil_addr_t node_addr;
  trx_rsegf_t *rseg_header;
  trx_ulogf_t *undo_log_hdr;
  ulint sum_of_undo_sizes;

  rseg = static_cast<trx_rseg_t *>(ut_zalloc_nokey(sizeof(trx_rseg_t)));

  rseg->id = id;
  rseg->space_id = space_id;
  rseg->page_size.copy_from(page_size);
  rseg->page_no = page_no;
  rseg->trx_ref_count = 0;

  if (fsp_is_system_temporary(space_id)) {
    mutex_create(LATCH_ID_TEMP_SPACE_RSEG, &rseg->mutex);
  } else if (fsp_is_undo_tablespace(space_id)) {
    mutex_create(LATCH_ID_UNDO_SPACE_RSEG, &rseg->mutex);
  } else {
    mutex_create(LATCH_ID_TRX_SYS_RSEG, &rseg->mutex);
  }

  UT_LIST_INIT(rseg->update_undo_list, &trx_undo_t::undo_list);
  UT_LIST_INIT(rseg->update_undo_cached, &trx_undo_t::undo_list);
  UT_LIST_INIT(rseg->insert_undo_list, &trx_undo_t::undo_list);
  UT_LIST_INIT(rseg->insert_undo_cached, &trx_undo_t::undo_list);

  rseg_header = trx_rsegf_get_new(space_id, page_no, page_size, mtr);

  rseg->max_size =
      mtr_read_ulint(rseg_header + TRX_RSEG_MAX_SIZE, MLOG_4BYTES, mtr);

  /* Initialize the undo log lists according to the rseg header */

  sum_of_undo_sizes = trx_undo_lists_init(rseg);

  rseg->curr_size =
      mtr_read_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, mtr) +
      1 + sum_of_undo_sizes;

  len = flst_get_len(rseg_header + TRX_RSEG_HISTORY);

  if (len > 0) {
    trx_sys->rseg_history_len += len;

    node_addr = trx_purge_get_log_from_hist(
        flst_get_last(rseg_header + TRX_RSEG_HISTORY, mtr));

    rseg->last_page_no = node_addr.page;
    rseg->last_offset = node_addr.boffset;

    undo_log_hdr = trx_undo_page_get(page_id_t(rseg->space_id, node_addr.page),
                                     rseg->page_size, mtr) +
                   node_addr.boffset;

    rseg->last_trx_no = mach_read_from_8(undo_log_hdr + TRX_UNDO_TRX_NO);

    rseg->last_del_marks =
        mtr_read_ulint(undo_log_hdr + TRX_UNDO_DEL_MARKS, MLOG_2BYTES, mtr);

    TrxUndoRsegs elem(rseg->last_trx_no);
    elem.push_back(rseg);

    if (rseg->last_page_no != FIL_NULL) {
      /* The only time an rseg is added that has existing
      undo is when the server is being started. So no
      mutex is needed here. */
      ut_ad(srv_is_being_started);

      ut_ad(space_id == TRX_SYS_SPACE ||
            (srv_is_upgrade_mode != undo::is_reserved(space_id)));

      purge_queue->push(elem);
    }
  } else {
    rseg->last_page_no = FIL_NULL;
  }

  return (rseg);
}

/** Return a page number from a slot in the rseg_array page of an
undo tablespace.
@param[in]	space_id	undo tablespace ID
@param[in]	rseg_id		rollback segment ID
@return page_no Page number of the rollback segment header page */
page_no_t trx_rseg_get_page_no(space_id_t space_id, ulint rseg_id) {
  mtr_t mtr;
  mtr.start();

  trx_rsegsf_t *rsegs_header = trx_rsegsf_get(space_id, &mtr);

  page_no_t page_no = trx_rsegsf_get_page_no(rsegs_header, rseg_id, &mtr);

  mtr.commit();

  return (page_no);
}

/** Read each rollback segment slot in the TRX_SYS page and the RSEG_ARRAY
page of each undo tablespace. Create trx_rseg_t objects for all rollback
segments found.  This runs at database startup and initializes the in-memory
lists of trx_rseg_t objects.  We need to look at all slots in TRX_SYS and
each RSEG_ARRAY page because we need to look for any existing undo log that
may need to be recovered by purge.  No latch is needed since this is still
single-threaded startup.  If we find existing rseg slots in TRX_SYS page
that reference undo tablespaces and have active undo logs, then quit.
They require an upgrade of undo tablespaces and that cannot happen with
active undo logs.
@param[in]	purge_queue	queue of rsegs to purge */
void trx_rsegs_init(purge_pq_t *purge_queue) {
  trx_sys->rseg_history_len = 0;

  ulint slot;
  mtr_t mtr;
  space_id_t space_id;
  page_no_t page_no;
  trx_rseg_t *rseg = nullptr;

  for (slot = 0; slot < TRX_SYS_N_RSEGS; slot++) {
    mtr.start();
    trx_sysf_t *sys_header = trx_sysf_get(&mtr);

    page_no = trx_sysf_rseg_get_page_no(sys_header, slot, &mtr);

    if (page_no != FIL_NULL) {
      space_id = trx_sysf_rseg_get_space(sys_header, slot, &mtr);

      /* Create the trx_rseg_t object.
      Note that all tablespaces with rollback segments
      use univ_page_size. (system, temp & undo) */
      rseg = trx_rseg_mem_create(slot, space_id, page_no, univ_page_size,
                                 purge_queue, &mtr);

      ut_a(rseg->id == slot);

      trx_sys->rsegs.push_back(rseg);
    }
    mtr.commit();
  }

  undo::spaces->x_lock();
  for (auto undo_space : undo::spaces->m_spaces) {
    undo_space->rsegs()->x_lock();
    for (slot = 0; slot < FSP_MAX_ROLLBACK_SEGMENTS; slot++) {
      page_no = trx_rseg_get_page_no(undo_space->id(), slot);

      /* There are no gaps in an RSEG_ARRAY page. New rsegs
      are added sequentially and never deleted until the
      undo tablespace is truncated.*/
      if (page_no == FIL_NULL) {
        break;
      }

      mtr.start();

      /* Create the trx_rseg_t object.
      Note that all tablespaces with rollback segments
      use univ_page_size. */
      rseg = trx_rseg_mem_create(slot, undo_space->id(), page_no,
                                 univ_page_size, purge_queue, &mtr);

      ut_a(rseg->id == slot);

      undo_space->rsegs()->push_back(rseg);

      mtr.commit();
    }
    undo_space->rsegs()->x_unlock();
  }
  undo::spaces->x_unlock();
}

/** Create a rollback segment in the given tablespace. This could be either
the system tablespace, the temporary tablespace, or an undo tablespace.
@param[in]	space_id	tablespace to get the rollback segment
@param[in]	rseg_id		slot number of the rseg within this tablespace
@return page number of the rollback segment header page created */
page_no_t trx_rseg_create(space_id_t space_id, ulint rseg_id) {
  mtr_t mtr;
  fil_space_t *space = fil_space_get(space_id);

  mtr_start(&mtr);

  /* To obey the latching order, acquire the file space
  x-latch before the mutex for trx_sys. */
  mtr_x_lock(&space->latch, &mtr);

  ut_ad(space->purpose == (fsp_is_system_temporary(space_id)
                               ? FIL_TYPE_TEMPORARY
                               : FIL_TYPE_TABLESPACE));
  ut_ad(univ_page_size.equals_to(page_size_t(space->flags)));

  if (fsp_is_system_temporary(space_id)) {
    mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
  } else if (space_id == TRX_SYS_SPACE) {
    /* We will modify TRX_SYS_RSEGS in TRX_SYS page. */
  }

  page_no_t page_no = trx_rseg_header_create(space_id, univ_page_size,
                                             PAGE_NO_MAX, rseg_id, &mtr);

  mtr_commit(&mtr);

  return (page_no);
}

/** Initialize */
void Rsegs::init() {
  m_rsegs.reserve(TRX_SYS_N_RSEGS);

  m_latch = static_cast<rw_lock_t *>(ut_zalloc_nokey(sizeof(*m_latch)));

  rw_lock_create(rsegs_lock_key, m_latch, SYNC_RSEGS);
}

/** De-initialize */
void Rsegs::deinit() {
  clear();

  rw_lock_free(m_latch);
  ut_free(m_latch);
  m_latch = nullptr;
}

/** Clear the vector of cached rollback segments leaving the
reserved space allocated. */
void Rsegs::clear() {
  for (auto rseg : m_rsegs) {
    trx_rseg_mem_free(rseg);
  }
  m_rsegs.clear();
  m_rsegs.shrink_to_fit();
}

/** Find an rseg in the std::vector that uses the rseg_id given.
@param[in]	rseg_id		A slot in a durable array such as
the TRX_SYS page or RSEG_ARRAY page.
@return a pointer to an trx_rseg_t that uses the rseg_id. */
trx_rseg_t *Rsegs::find(ulint rseg_id) {
  trx_rseg_t *rseg;

  /* In most cases, the rsegs will be in slot order with no gaps. */
  if (rseg_id < m_rsegs.size()) {
    rseg = m_rsegs.at(rseg_id);
    if (rseg->id == rseg_id) {
      return (rseg);
    }
  }

  /* If there are gaps in the numbering, do a search. */
  for (auto rseg : m_rsegs) {
    if (rseg->id == rseg_id) {
      return (rseg);
    }
  }

  return (nullptr);
}

/** This does two things to the target tablespace.
1. Find or create (trx_rseg_create) the requested number of rollback segments.
2. Make sure each rollback segment is tracked in memory (trx_rseg_mem_create).
All existing rollback segments were found earlier in trx_rsegs_init().
This will add new ones if we need them according to the
target_undo_tablespaces and target_rollback_segments, we will build them in
the tablespace and in memory.
@param[in]	space_id	tablespace ID that should contain rollback
                                segments
@param[in]	target_spaces	target number of undo tablespaces
@param[in]	target_rsegs	target number of rollback segments per
                                tablespace
@param[in]	rsegs		list of rsegs to add to */
bool trx_rseg_add_rollback_segments(space_id_t space_id, ulong target_spaces,
                                    ulong target_rsegs, Rsegs *rsegs) {
  bool success = true;
  bool creating_rsegs = false;
  mtr_t mtr;
  page_no_t page_no;
  trx_rseg_t *rseg;
  ulong n_existing = 0;
  ulong n_created = 0;
  ulong n_tracked = 0;

  enum space_type_t { TEMP, SYSTEM, UNDO } type;

  if (fsp_is_undo_tablespace(space_id)) {
    type = UNDO;
    creating_rsegs = (target_spaces > 0);
  } else if (space_id == TRX_SYS_SPACE) {
    type = SYSTEM;
    creating_rsegs = (target_spaces == 0);
  } else {
    ut_ad(fsp_is_system_temporary(space_id));
    type = TEMP;
    creating_rsegs = false;
  }

  /* Protect against two threads trying to add rollback segments
  at the same time. */
  rsegs->x_lock();

  for (ulint num = 0; num < FSP_MAX_ROLLBACK_SEGMENTS; num++) {
    if (rsegs->size() >= target_rsegs) {
      break;
    }

    ulint rseg_id = num;
    if (type == SYSTEM) {
      mtr.start();
      rseg_id = trx_sysf_rseg_find_free(&mtr);
      mtr.commit();
    }

    /* If the rseg object exists, move to the next rseg_id. */
    rseg = rsegs->find(rseg_id);
    if (rseg != nullptr) {
      ut_ad(rseg->id == rseg_id);
      n_existing++;
      continue;
    }

    /* Look in the tablespace to discover if the rollback segment
    already exists. */
    if (type == UNDO) {
      page_no = trx_rseg_get_page_no(space_id, rseg_id);

    } else if (type == SYSTEM) {
      page_no = trx_sysf_rseg_find_page_no(rseg_id);

    } else {
      /* There is no durable list of rollback segments in
      the temporary tablespace. Since it was not found in
      the rsegs vector, assume the rollback segment does
      not exist in the temp tablespace. */
      page_no = FIL_NULL;
    }

    if (page_no == FIL_NULL) {
      /* Create the missing rollback segment if allowed. */
      if (type == TEMP ||
          (!srv_read_only_mode && srv_force_recovery == 0 && creating_rsegs)) {
        page_no = trx_rseg_create(space_id, rseg_id);
        if (page_no == FIL_NULL) {
          /* There may not be enough space in
          the system or temporary tablespace
          since it is possible to limit their
          size. */
          ut_ad(type != UNDO);
          continue;
        }
        n_created++;
      } else {
        /* trx_rseg_create() is being prevented in
        a SYSTEM or UNDO tablespace. Don't try to
        create any more. */
        break;
      }
    } else {
      n_existing++;
    }

    /* Create the trx_rseg_t object. */
    mtr.start();

    fil_space_t *space = fil_space_get(space_id);
    ut_ad(univ_page_size.equals_to(page_size_t(space->flags)));
    mtr_x_lock(&space->latch, &mtr);

    if (type == TEMP) {
      mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
    }

    rseg = trx_rseg_mem_create(rseg_id, space_id, page_no, univ_page_size,
                               purge_sys->purge_queue, &mtr);

    mtr.commit();

    if (rseg != nullptr) {
      ut_a(rseg->id == rseg_id);
      rsegs->push_back(rseg);
      n_tracked++;
    }
  }

  /* TEMP and UNDO Rsegs are always added to in order of rseg_id so
  they never need to be sorted.  But the SYSTEM Rsegs might initially
  have a 32 slot gap if it was created by an earlier engine.  New
  rollback segments might be added in that gap. So the SYSTEM Rseg
  list is the only one that needs to be sorted. */
  if (type == SYSTEM) {
    rsegs->sort();
  }

  rsegs->x_unlock();

  if (n_created > 0 && type != TEMP && !srv_read_only_mode &&
      srv_force_recovery == 0) {
    log_make_latest_checkpoint();
  }

  std::ostringstream loc;
  switch (type) {
    case UNDO:
      loc << "undo tablespace number " << undo::id2num(space_id);
      break;
    case SYSTEM:
      loc << "the system tablespace";
      break;
    case TEMP:
      loc << "the temporary tablespace";
      break;
  }

  ulint n_known = rsegs->size();
  if (n_known < target_rsegs) {
    if (srv_read_only_mode || srv_force_recovery > 0) {
      bool use_and = srv_read_only_mode && srv_force_recovery > 0;

      ib::info(ER_IB_MSG_1191)
          << "Could not create all " << target_rsegs << " rollback segments in "
          << loc.str() << " because "
          << (srv_read_only_mode ? " read-only mode is set" : "")
          << (use_and ? " and" : "")
          << (srv_force_recovery > 0 ? " innodb_force_recovery is set" : "")
          << ". Only " << n_known << " are active.";

      srv_rollback_segments =
          ut_min(srv_rollback_segments, static_cast<ulong>(n_known));

    } else if (creating_rsegs) {
      ib::warn(ER_IB_MSG_1192)
          << "Could not create all " << target_rsegs << " rollback segments in "
          << loc.str() << ". Only " << n_known << " are active.";

      srv_rollback_segments =
          ut_min(srv_rollback_segments, static_cast<ulong>(n_known));

      success = false;
    }

  } else if (n_created > 0) {
    ib::info(ER_IB_MSG_1193)
        << "Created " << n_created << " and tracked " << n_tracked
        << " new rollback segment(s) in " << loc.str() << ". " << target_rsegs
        << " are now active.";

  } else if (n_tracked > 0) {
    ib::info(ER_IB_MSG_1194)
        << "Using " << n_tracked << " more rollback segment(s) in " << loc.str()
        << ". " << target_rsegs << " are now active.";

  } else if (target_rsegs < n_known) {
    ib::info(ER_IB_MSG_1195)
        << target_rsegs << " rollback segment(s) are now active in "
        << loc.str() << ".";
  }

  return (success);
}

/** Add more rsegs to the rseg list in each tablespace until there are
srv_rollback_segments of them.  Use any rollback segment that already
exists so that the purge_queue can be filled and processed with any
existing undo log. If the rollback segments do not exist in this
tablespace and we need them according to the target_undo_tablespaces
and target_rollback_segments, build them in the tablespace.
@param[in]	target_undo_tablespaces		target number of undo
                                                tablespaces
@param[in]	target_rollback_segments	new number of rollback
                                                segments per space
@return true if all necessary rollback segments and trx_rseg_t objects
were created. */
bool trx_rseg_adjust_rollback_segments(ulong target_undo_tablespaces,
                                       ulong target_rollback_segments) {
  /* Make sure Temporary Tablespace has enough rsegs. */
  if (!trx_rseg_add_rollback_segments(
          srv_tmp_space.space_id(), target_undo_tablespaces,
          target_rollback_segments, &(trx_sys->tmp_rsegs))) {
    return (false);
  }

  /* Only the temp rsegs are used with a high force_recovery. */
  if (srv_force_recovery >= SRV_FORCE_NO_UNDO_LOG_SCAN) {
    return (true);
  }

  /* Adjust the number of rollback segments in each Undo Tablespace
  whether or not it is currently active. */
  if (target_undo_tablespaces > 0) {
    undo::spaces->s_lock();
    for (auto undo_space : undo::spaces->m_spaces) {
      undo::spaces->s_unlock();

      if (!trx_rseg_add_rollback_segments(
              undo_space->id(), target_undo_tablespaces,
              target_rollback_segments, undo_space->rsegs())) {
        return (false);
      }
      undo::spaces->s_lock();
    }
    undo::spaces->s_unlock();
  }

  /* System tablespace */
  if (target_undo_tablespaces == 0) {
    if (!trx_rseg_add_rollback_segments(TRX_SYS_SPACE, target_undo_tablespaces,
                                        target_rollback_segments,
                                        &(trx_sys->rsegs))) {
      return (false);
    }
  }
  return (true);
}

/** Build a list of unique undo tablespaces found in the TRX_SYS page.
Do not count the system tablespace. The vector will be sorted on space id.
@param[in,out]	spaces_to_open		list of undo tablespaces found. */
void trx_rseg_get_n_undo_tablespaces(Space_Ids *spaces_to_open) {
  ulint i;
  mtr_t mtr;
  trx_sysf_t *sys_header;

  ut_ad(spaces_to_open->empty());

  mtr_start(&mtr);

  sys_header = trx_sysf_get(&mtr);

  for (i = 0; i < TRX_SYS_N_RSEGS; i++) {
    page_no_t page_no;
    space_id_t space_id;

    page_no = trx_sysf_rseg_get_page_no(sys_header, i, &mtr);

    if (page_no == FIL_NULL) {
      continue;
    }

    space_id = trx_sysf_rseg_get_space(sys_header, i, &mtr);

    /* The system space id should not be in this array. */
    if (space_id != TRX_SYS_SPACE && !spaces_to_open->contains(space_id)) {
      spaces_to_open->push_back(space_id);
    }
  }

  mtr_commit(&mtr);

  ut_a(spaces_to_open->size() <= TRX_SYS_N_RSEGS);
}

/** Upgrade the TRX_SYS page so that it no longer tracks rsegs in undo
tablespaces. It should only track rollback segments in the system tablespace.
Put FIL_NULL in the slots in TRX_SYS. Latch protection is not needed since
this is during single-threaded startup. */
void trx_rseg_upgrade_undo_tablespaces() {
  ulint i;
  mtr_t mtr;
  trx_sysf_t *sys_header;

  mtr_start(&mtr);
  fil_space_t *space = fil_space_get(TRX_SYS_SPACE);
  mtr_x_lock(&space->latch, &mtr);

  sys_header = trx_sysf_get(&mtr);

  /* First, put FIL_NULL in all the slots that contain the space_id
  of any non-system tablespace. The rollback segments in those
  tablespaces are replaced when the file is replaced. */
  for (i = 0; i < TRX_SYS_N_RSEGS; i++) {
    page_no_t page_no;
    space_id_t space_id;

    page_no = trx_sysf_rseg_get_page_no(sys_header, i, &mtr);

    if (page_no == FIL_NULL) {
      continue;
    }

    space_id = trx_sysf_rseg_get_space(sys_header, i, &mtr);

    /* The TRX_SYS page only tracks older undo tablespaces
    that do not use the RSEG_ARRAY page. */
    ut_a(space_id < dict_sys_t::s_min_undo_space_id);

    /* Leave rollback segments in the system tablespace
    untouched in case innodb_undo_tablespaces is later
    set back to 0. */
    if (space_id != 0) {
      trx_sysf_rseg_set_space(sys_header, i, FIL_NULL, &mtr);

      trx_sysf_rseg_set_page_no(sys_header, i, FIL_NULL, &mtr);
    }
  }

  mtr_commit(&mtr);

  /* After upgrading, undo tablespaces track their own rsegs.
  Clear the list of rsegs in old undo tablespaces. */
  trx_sys->rsegs.x_lock();
  trx_sys->rsegs.clear();
  ut_ad(trx_sys->rsegs.size() == 0);
  trx_sys->rsegs.x_unlock();
}

/** Create the file page for the rollback segment directory in an undo
tablespace. This function is called just after an undo tablespace is
created so the next page created here should by FSP_FSEG_DIR_PAGE_NUM.
@param[in]	space_id	Undo Tablespace ID
@param[in]	mtr		mtr */
void trx_rseg_array_create(space_id_t space_id, mtr_t *mtr) {
  trx_rsegsf_t *rsegs_header;
  buf_block_t *block;
  page_t *page;
  byte *ptr;
  ulint len;

  fil_space_t *space = fil_space_get(space_id);
  mtr_x_lock_space(space, mtr);

  /* Create the fseg directory file block in a new allocated file segment */
  block = fseg_create(space_id, 0,
                      RSEG_ARRAY_HEADER + RSEG_ARRAY_FSEG_HEADER_OFFSET, mtr);
  buf_block_dbg_add_level(block, SYNC_RSEG_ARRAY_HEADER);

  ut_a(block->page.id.page_no() == FSP_RSEG_ARRAY_PAGE_NO);

  page = buf_block_get_frame(block);

  mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_TYPE_RSEG_ARRAY, MLOG_2BYTES,
                   mtr);

  rsegs_header = page + RSEG_ARRAY_HEADER;

  /* Initialize the rseg array version. */
  mach_write_to_4(rsegs_header + RSEG_ARRAY_VERSION_OFFSET, RSEG_ARRAY_VERSION);

  /* Initialize the directory size. */
  mach_write_to_4(rsegs_header + RSEG_ARRAY_SIZE_OFFSET, 0);

  /* Reset the rollback segment header page slots. Use the full page
  minus overhead.  Reserve some extra room for future use.  */
  ptr = RSEG_ARRAY_PAGES_OFFSET + rsegs_header;
  len = UNIV_PAGE_SIZE - RSEG_ARRAY_HEADER - RSEG_ARRAY_PAGES_OFFSET -
        RSEG_ARRAY_RESERVED_BYTES - FIL_PAGE_DATA_END;
  memset(ptr, 0xff, len);

  mlog_log_string(rsegs_header,
                  UNIV_PAGE_SIZE - RSEG_ARRAY_HEADER - FIL_PAGE_DATA_END, mtr);
}
