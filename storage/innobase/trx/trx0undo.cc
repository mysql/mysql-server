/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.

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

/** @file trx/trx0undo.cc
 Transaction undo log

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#include <stddef.h>

#include <sql_thd_internal_api.h>

#include "fsp0fsp.h"
#include "ha_prototypes.h"
#include "trx0undo.h"

#include "my_dbug.h"

#ifndef UNIV_HOTBACKUP
#include "clone0clone.h"
#include "current_thd.h"
#include "dict0dd.h"
#include "fil0fil.h"
#include "log0chkp.h"
#include "log0write.h"
#include "mach0data.h"
#include "mtr0log.h"
#include "srv0mon.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0purge.h"
#include "trx0rec.h"
#include "trx0rseg.h"
#include "trx0trx.h"

/* How should the old versions in the history list be managed?
   ----------------------------------------------------------
If each transaction is given a whole page for its update undo log, file
space consumption can be 10 times higher than necessary. Therefore,
partly filled update undo log pages should be reusable. But then there
is no way individual pages can be ordered so that the ordering agrees
with the serialization numbers of the transactions on the pages. Thus,
the history list must be formed of undo logs, not their header pages as
it was in the old implementation.
        However, on a single header page the transactions are placed in
the order of their serialization numbers. As old versions are purged, we
may free the page when the last transaction on the page has been purged.
        A problem is that the purge has to go through the transactions
in the serialization order. This means that we have to look through all
rollback segments for the one that has the smallest transaction number
in its history list.
        When should we do a purge? A purge is necessary when space is
running out in any of the rollback segments. Then we may have to purge
also old version which might be needed by some consistent read. How do
we trigger the start of a purge? When a transaction writes to an undo log,
it may notice that the space is running out. When a read view is closed,
it may make some history superfluous. The server can have an utility which
periodically checks if it can purge some history.
        In a parallelized purge we have the problem that a query thread
can remove a delete marked clustered index record before another query
thread has processed an earlier version of the record, which cannot then
be done because the row cannot be constructed from the clustered index
record. To avoid this problem, we will store in the update and delete mark
undo record also the columns necessary to construct the secondary index
entries which are modified.
        We can latch the stack of versions of a single clustered index record
by taking a latch on the clustered index page. As long as the latch is held,
no new versions can be added and no versions removed by undo. But, a purge
can still remove old versions from the bottom of the stack. */

/* How to protect rollback segments, undo logs, and history lists with
   -------------------------------------------------------------------
latches?
-------
The contention of the trx_sys_t::mutex should be minimized. When a transaction
does its first insert or modify in an index, an undo log is assigned for it.
Then we must have an x-latch to the rollback segment header.
        When the transaction does more modifications or rolls back, the undo log
is protected with undo_mutex in the transaction.
        When the transaction commits, its insert undo log is either reset and
cached for a fast reuse, or freed. In these cases we must have an x-latch on
the rollback segment page. The update undo log is put to the history list. If
it is not suitable for reuse, its slot in the rollback segment is reset. In
both cases, an x-latch must be acquired on the rollback segment.
        The purge operation steps through the history list without modifying
it until a truncate operation occurs, which can remove undo logs from the end
of the list and release undo log segments. In stepping through the list,
s-latches on the undo log pages are enough, but in a truncate, x-latches must
be obtained on the rollback segment and individual pages. */
#endif /* !UNIV_HOTBACKUP */

/** Initializes the fields in an undo log segment page. */
static void trx_undo_page_init(
    page_t *undo_page, /*!< in: undo log segment page */
    ulint type,        /*!< in: undo log segment type */
    mtr_t *mtr);       /*!< in: mtr */

#ifndef UNIV_HOTBACKUP
/** Creates and initializes an undo log memory object.
@param[in]   rseg     rollback segment memory object
@param[in]   id       slot index within rseg
@param[in]   type     type of the log: TRX_UNDO_INSERT or TRX_UNDO_UPDATE
@param[in]   trx_id   id of the trx for which the undo log is created
@param[in]   xid      X/Open XA transaction identification
@param[in]   page_no  undo log header page number
@param[in]   offset   undo log header byte offset on page
@return own: the undo log memory object */
static trx_undo_t *trx_undo_mem_create(trx_rseg_t *rseg, ulint id, ulint type,
                                       trx_id_t trx_id, const XID *xid,
                                       page_no_t page_no, ulint offset);
#endif /* !UNIV_HOTBACKUP */
/** Initializes a cached insert undo log header page for new use. NOTE that this
 function has its own log record type MLOG_UNDO_HDR_REUSE. You must NOT change
 the operation of this function!
 @return undo log header byte offset on page */
static ulint trx_undo_insert_header_reuse(
    page_t *undo_page, /*!< in/out: insert undo log segment
                       header page, x-latched */
    trx_id_t trx_id,   /*!< in: transaction id */
    mtr_t *mtr);       /*!< in: mtr */

#ifndef UNIV_HOTBACKUP
/** Gets the previous record in an undo log from the previous page.
 @return undo log record, the page s-latched, NULL if none */
static trx_undo_rec_t *trx_undo_get_prev_rec_from_prev_page(
    trx_undo_rec_t *rec, /*!< in: undo record */
    page_no_t page_no,   /*!< in: undo log header page number */
    ulint offset,        /*!< in: undo log header offset on page */
    bool shared,         /*!< in: true=S-latch, false=X-latch */
    mtr_t *mtr)          /*!< in: mtr */
{
  space_id_t space;
  page_no_t prev_page_no;
  page_t *prev_page;
  page_t *undo_page;

  undo_page = page_align(rec);

  prev_page_no = flst_get_prev_addr(
                     undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_NODE, mtr)
                     .page;

  if (prev_page_no == FIL_NULL) {
    return (nullptr);
  }

  space = page_get_space_id(undo_page);

  bool found;
  const page_size_t &page_size = fil_space_get_page_size(space, &found);

  ut_ad(found);

  buf_block_t *block =
      buf_page_get(page_id_t(space, prev_page_no), page_size,
                   shared ? RW_S_LATCH : RW_X_LATCH, UT_LOCATION_HERE, mtr);

  buf_block_dbg_add_level(block, SYNC_TRX_UNDO_PAGE);

  prev_page = buf_block_get_frame(block);

  return (trx_undo_page_get_last_rec(prev_page, page_no, offset));
}

/** Gets the previous record in an undo log.
 @return undo log record, the page s-latched, NULL if none */
trx_undo_rec_t *trx_undo_get_prev_rec(
    trx_undo_rec_t *rec, /*!< in: undo record */
    page_no_t page_no,   /*!< in: undo log header page number */
    ulint offset,        /*!< in: undo log header offset on page */
    bool shared,         /*!< in: true=S-latch, false=X-latch */
    mtr_t *mtr)          /*!< in: mtr */
{
  trx_undo_rec_t *prev_rec;

  prev_rec = trx_undo_page_get_prev_rec(rec, page_no, offset);

  if (prev_rec) {
    return (prev_rec);
  }

  /* We have to go to the previous undo log page to look for the
  previous record */

  return (
      trx_undo_get_prev_rec_from_prev_page(rec, page_no, offset, shared, mtr));
}

/** Gets the next record in an undo log from the next page.
@param[in]      space           Undo log header space
@param[in]      page_size       Page size
@param[in]      undo_page       Undo log page
@param[in]      page_no         Undo log header page number
@param[in]      offset          Undo log header offset on page
@param[in]      mode            Latch mode: RW_S_LATCH or RW_X_LATCH
@param[in,out]  mtr             Mini-transaction
@return undo log record, the page latched, NULL if none */
static trx_undo_rec_t *trx_undo_get_next_rec_from_next_page(
    space_id_t space, const page_size_t &page_size, const page_t *undo_page,
    page_no_t page_no, ulint offset, ulint mode, mtr_t *mtr) {
  const trx_ulogf_t *log_hdr;
  page_no_t next_page_no;
  page_t *next_page;
  ulint next;

  if (page_no == page_get_page_no(undo_page)) {
    log_hdr = undo_page + offset;
    next = mach_read_from_2(log_hdr + TRX_UNDO_NEXT_LOG);

    if (next != 0) {
      return (nullptr);
    }
  }

  next_page_no = flst_get_next_addr(
                     undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_NODE, mtr)
                     .page;
  if (next_page_no == FIL_NULL) {
    return (nullptr);
  }

  const page_id_t next_page_id(space, next_page_no);

  if (mode == RW_S_LATCH) {
    next_page = trx_undo_page_get_s_latched(next_page_id, page_size, mtr);
  } else {
    ut_ad(mode == RW_X_LATCH);
    next_page = trx_undo_page_get(next_page_id, page_size, mtr);
  }

  return (trx_undo_page_get_first_rec(next_page, page_no, offset));
}

/** Gets the next record in an undo log.
 @return undo log record, the page s-latched, NULL if none */
trx_undo_rec_t *trx_undo_get_next_rec(
    trx_undo_rec_t *rec, /*!< in: undo record */
    page_no_t page_no,   /*!< in: undo log header page number */
    ulint offset,        /*!< in: undo log header offset on page */
    mtr_t *mtr)          /*!< in: mtr */
{
  space_id_t space;
  trx_undo_rec_t *next_rec;

  next_rec = trx_undo_page_get_next_rec(rec, page_no, offset);

  if (next_rec) {
    return (next_rec);
  }

  space = page_get_space_id(page_align(rec));

  bool found;
  const page_size_t &page_size = fil_space_get_page_size(space, &found);

  ut_ad(found);

  return (trx_undo_get_next_rec_from_next_page(
      space, page_size, page_align(rec), page_no, offset, RW_S_LATCH, mtr));
}

/** Gets the first record in an undo log.
@param[out]     modifier_trx_id The modifier trx identifier.
@param[in]      space           Undo log header space
@param[in]      page_size       Page size
@param[in]      page_no         Undo log header page number
@param[in]      offset          Undo log header offset on page
@param[in]      mode            Latching mode: RW_S_LATCH or RW_X_LATCH
@param[in,out]  mtr             Mini-transaction
@return undo log record, the page latched, NULL if none */
trx_undo_rec_t *trx_undo_get_first_rec(trx_id_t *modifier_trx_id,
                                       space_id_t space,
                                       const page_size_t &page_size,
                                       page_no_t page_no, ulint offset,
                                       ulint mode, mtr_t *mtr) {
  page_t *undo_page;
  trx_undo_rec_t *rec;

  const page_id_t page_id(space, page_no);

  if (mode == RW_S_LATCH) {
    undo_page = trx_undo_page_get_s_latched(page_id, page_size, mtr);
  } else {
    undo_page = trx_undo_page_get(page_id, page_size, mtr);
  }

  if (modifier_trx_id != nullptr) {
    trx_ulogf_t *undo_header = undo_page + offset;
    *modifier_trx_id = mach_read_from_8(undo_header + TRX_UNDO_TRX_ID);
  }

  rec = trx_undo_page_get_first_rec(undo_page, page_no, offset);

  if (rec) {
    return (rec);
  }

  return (trx_undo_get_next_rec_from_next_page(space, page_size, undo_page,
                                               page_no, offset, mode, mtr));
}

/*============== UNDO LOG FILE COPY CREATION AND FREEING ==================*/

/** Writes the mtr log entry of an undo log page initialization. */
static inline void trx_undo_page_init_log(
    page_t *undo_page, /*!< in: undo log page */
    ulint type,        /*!< in: undo log type */
    mtr_t *mtr)        /*!< in: mtr */
{
  mlog_write_initial_log_record(undo_page, MLOG_UNDO_INIT, mtr);

  mlog_catenate_ulint_compressed(mtr, type);
}
#else /* !UNIV_HOTBACKUP */
#define trx_undo_page_init_log(undo_page, type, mtr) ((void)0)
#endif /* !UNIV_HOTBACKUP */

/** Parses the redo log entry of an undo log page initialization.
 @return end of log record or NULL */
byte *trx_undo_parse_page_init(const byte *ptr,     /*!< in: buffer */
                               const byte *end_ptr, /*!< in: buffer end */
                               page_t *page,        /*!< in: page or NULL */
                               mtr_t *mtr)          /*!< in: mtr or NULL */
{
  ulint type;

  type = mach_parse_compressed(&ptr, end_ptr);

  if (ptr == nullptr) {
    return (nullptr);
  }

  if (page) {
    trx_undo_page_init(page, type, mtr);
  }

  return (const_cast<byte *>(ptr));
}

/** Initializes the fields in an undo log segment page. */
static void trx_undo_page_init(
    page_t *undo_page, /*!< in: undo log segment page */
    ulint type,        /*!< in: undo log segment type */
    mtr_t *mtr)        /*!< in: mtr */
{
  trx_upagef_t *page_hdr;

  page_hdr = undo_page + TRX_UNDO_PAGE_HDR;

  mach_write_to_2(page_hdr + TRX_UNDO_PAGE_TYPE, type);

  mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START,
                  TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE);
  mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE,
                  TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_HDR_SIZE);

  fil_page_set_type(undo_page, FIL_PAGE_UNDO_LOG);

  trx_undo_page_init_log(undo_page, type, mtr);
}

#ifndef UNIV_HOTBACKUP
/** Creates a new undo log segment in file.
 @return DB_SUCCESS if page creation OK possible error codes are:
 DB_TOO_MANY_CONCURRENT_TRXS DB_OUT_OF_FILE_SPACE */
[[nodiscard]] static dberr_t trx_undo_seg_create(
    trx_rseg_t *rseg [[maybe_unused]], /*!< in: rollback segment */
    trx_rsegf_t *rseg_hdr,             /*!< in: rollback segment header,
                                      page x-latched */
    ulint type,                        /*!< in: type of the segment:
                                       TRX_UNDO_INSERT or            TRX_UNDO_UPDATE */
    ulint *id, /*!< out: slot index within rseg header */
    page_t **undo_page,
    /*!< out: segment header page x-latched, NULL
    if there was an error */
    mtr_t *mtr) /*!< in: mtr */
{
  ulint slot_no = ULINT_UNDEFINED;
  space_id_t space;
  buf_block_t *block;
  trx_upagef_t *page_hdr;
  trx_usegf_t *seg_hdr;
  ulint n_reserved;
  bool success;
  dberr_t err = DB_SUCCESS;

  ut_ad(mtr != nullptr);
  ut_ad(id != nullptr);
  ut_ad(rseg_hdr != nullptr);
  ut_ad(mutex_own(&(rseg->mutex)));

#ifdef UNIV_DEBUG
  if (!srv_inject_too_many_concurrent_trxs)
#endif
  {
    slot_no = trx_rsegf_undo_find_free(rseg_hdr, mtr);
  }
  if (slot_no == ULINT_UNDEFINED) {
    ib::error(ER_IB_MSG_1212)
        << "Cannot find a free slot for an undo log."
           " You may have too many active transactions running concurrently."
           " Please add more rollback segments or undo tablespaces.";

    return (DB_TOO_MANY_CONCURRENT_TRXS);
  }

  space = page_get_space_id(page_align(rseg_hdr));

  success = fsp_reserve_free_extents(&n_reserved, space, 2, FSP_UNDO, mtr);
  if (!success) {
    return (DB_OUT_OF_FILE_SPACE);
  }

  /* Allocate a new file segment for the undo log */
  block = fseg_create_general(space, 0, TRX_UNDO_SEG_HDR + TRX_UNDO_FSEG_HEADER,
                              true, mtr);

  fil_space_release_free_extents(space, n_reserved);

  if (block == nullptr) {
    /* No space left */

    return (DB_OUT_OF_FILE_SPACE);
  }

  buf_block_dbg_add_level(block, SYNC_TRX_UNDO_PAGE);

  *undo_page = buf_block_get_frame(block);

  page_hdr = *undo_page + TRX_UNDO_PAGE_HDR;
  seg_hdr = *undo_page + TRX_UNDO_SEG_HDR;

  trx_undo_page_init(*undo_page, type, mtr);

  mlog_write_ulint(page_hdr + TRX_UNDO_PAGE_FREE,
                   TRX_UNDO_SEG_HDR + TRX_UNDO_SEG_HDR_SIZE, MLOG_2BYTES, mtr);

  mlog_write_ulint(seg_hdr + TRX_UNDO_LAST_LOG, 0, MLOG_2BYTES, mtr);

  flst_init(seg_hdr + TRX_UNDO_PAGE_LIST, mtr);

  flst_add_last(seg_hdr + TRX_UNDO_PAGE_LIST, page_hdr + TRX_UNDO_PAGE_NODE,
                mtr);

  trx_rsegf_set_nth_undo(rseg_hdr, slot_no, page_get_page_no(*undo_page), mtr);
  *id = slot_no;

  MONITOR_INC(MONITOR_NUM_UNDO_SLOT_USED);

  return (err);
}

/** Writes the mtr log entry of an undo log header initialization. */
static inline void trx_undo_header_create_log(
    const page_t *undo_page, /*!< in: undo log header page */
    trx_id_t trx_id,         /*!< in: transaction id */
    mtr_t *mtr)              /*!< in: mtr */
{
  mlog_write_initial_log_record(undo_page, MLOG_UNDO_HDR_CREATE, mtr);

  mlog_catenate_ull_compressed(mtr, trx_id);
}
#else /* !UNIV_HOTBACKUP */
#define trx_undo_header_create_log(undo_page, trx_id, mtr) ((void)0)
#endif /* !UNIV_HOTBACKUP */

/** Creates a new undo log header in file. NOTE that this function has its own
 log record type MLOG_UNDO_HDR_CREATE. You must NOT change the operation of
 this function!
 @return header byte offset on page */
static ulint trx_undo_header_create(
    page_t *undo_page, /*!< in/out: undo log segment
                       header page, x-latched; it is
                       assumed that there is
                       TRX_UNDO_LOG_HDR_SIZE bytes
                       free space on it */
    trx_id_t trx_id,   /*!< in: transaction id */
    mtr_t *mtr)        /*!< in: mtr */
{
  trx_upagef_t *page_hdr;
  trx_usegf_t *seg_hdr;
  trx_ulogf_t *log_hdr;
  ulint prev_log;
  ulint free;
  ulint new_free;

  ut_ad(mtr && undo_page);

  page_hdr = undo_page + TRX_UNDO_PAGE_HDR;
  seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

  free = mach_read_from_2(page_hdr + TRX_UNDO_PAGE_FREE);

  log_hdr = undo_page + free;

  new_free = free + TRX_UNDO_LOG_OLD_HDR_SIZE;

  ut_a(free + TRX_UNDO_LOG_GTID_XA_HDR_SIZE < UNIV_PAGE_SIZE - 100);

  mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START, new_free);

  mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE, new_free);

  mach_write_to_2(seg_hdr + TRX_UNDO_STATE, TRX_UNDO_ACTIVE);

  prev_log = mach_read_from_2(seg_hdr + TRX_UNDO_LAST_LOG);

  if (prev_log != 0) {
    trx_ulogf_t *prev_log_hdr;

    prev_log_hdr = undo_page + prev_log;

    mach_write_to_2(prev_log_hdr + TRX_UNDO_NEXT_LOG, free);
  }

  mach_write_to_2(seg_hdr + TRX_UNDO_LAST_LOG, free);

  log_hdr = undo_page + free;

  mach_write_to_2(log_hdr + TRX_UNDO_DEL_MARKS, true);

  mach_write_to_8(log_hdr + TRX_UNDO_TRX_ID, trx_id);
  mach_write_to_2(log_hdr + TRX_UNDO_LOG_START, new_free);

  mach_write_to_1(log_hdr + TRX_UNDO_FLAGS, 0);
  mach_write_to_1(log_hdr + TRX_UNDO_DICT_TRANS, false);

  mach_write_to_2(log_hdr + TRX_UNDO_NEXT_LOG, 0);
  mach_write_to_2(log_hdr + TRX_UNDO_PREV_LOG, prev_log);

  /* Write the log record about the header creation */
  trx_undo_header_create_log(undo_page, trx_id, mtr);

  return (free);
}

#ifndef UNIV_HOTBACKUP
/** Write X/Open XA Transaction Identification (XID) to undo log header */
static void trx_undo_write_xid(
    trx_ulogf_t *log_hdr, /*!< in: undo log header */
    const XID *xid,       /*!< in: X/Open XA Transaction Identification */
    mtr_t *mtr)           /*!< in: mtr */
{
  mlog_write_ulint(log_hdr + TRX_UNDO_XA_FORMAT,
                   static_cast<ulint>(xid->get_format_id()), MLOG_4BYTES, mtr);

  mlog_write_ulint(log_hdr + TRX_UNDO_XA_TRID_LEN,
                   static_cast<ulint>(xid->get_gtrid_length()), MLOG_4BYTES,
                   mtr);

  mlog_write_ulint(log_hdr + TRX_UNDO_XA_BQUAL_LEN,
                   static_cast<ulint>(xid->get_bqual_length()), MLOG_4BYTES,
                   mtr);

  mlog_write_string(log_hdr + TRX_UNDO_XA_XID,
                    reinterpret_cast<const byte *>(xid->get_data()),
                    XIDDATASIZE, mtr);
}

dberr_t trx_undo_gtid_add_update_undo(trx_t *trx, bool prepare, bool rollback) {
  ut_ad(!(prepare && rollback));
  /* Check if GTID persistence is needed. */
  auto &gtid_persistor = clone_sys->get_gtid_persistor();

  /* Caller could request GTID persistence explicitly for non-Innodb operations
  using an empty transaction. We also allocate undo for such cases. */
  bool gtid_explicit = false;

  bool alloc =
      gtid_persistor.trx_check_set(trx, prepare, rollback, gtid_explicit);

  auto undo_ptr = &trx->rsegs.m_redo;

  /* If update undo is already allocated, nothing to do. */
  if (!alloc || undo_ptr->is_update()) {
    return (DB_SUCCESS);
  }

  /* For GTID persistence we need update undo segment. Allocate update
  undo segment here if it is insert only transaction. If no undo segment
  is allocated yet, then transaction didn't do any modification and
  no GTID would be allotted to it. One exception is the explicit GTID
  request. */
  dberr_t db_err = DB_SUCCESS;

  if (undo_ptr->is_insert_only() || gtid_explicit) {
    mutex_enter(&trx->undo_mutex);
    db_err = trx_undo_assign_undo(trx, undo_ptr, TRX_UNDO_UPDATE);
    mutex_exit(&trx->undo_mutex);
  }
  /* In rare cases we might find no available update undo segment for insert
  only transactions. It is still fine to return error at prepare stage.
  Cannot do it earlier as GTID information is not known before. Keep the
  debug assert to know if it really happens ever. */
  if (db_err != DB_SUCCESS) {
    trx->persists_gtid = false;
    ib::error(ER_IB_CLONE_GTID_PERSIST)
        << "Could not allocate undo segment"
        << " slot for persisting GTID. DB Error: " << db_err;
    ut_d(ut_error);
  }
  return (db_err);
}

bool trx_undo_t::gtid_allocated(bool is_prepare) const {
  if (is_prepare) {
    return m_gtid_storage == Gtid_storage::PREPARE_AND_COMMIT;
  }
  return m_gtid_storage == Gtid_storage::COMMIT ||
         m_gtid_storage == Gtid_storage::PREPARE_AND_COMMIT;
}

std::tuple<int, size_t> trx_undo_t::gtid_get_details(bool is_prepare) const {
  int flag = is_prepare ? TRX_UNDO_FLAG_XA_PREPARE_GTID : TRX_UNDO_FLAG_GTID;
  size_t size = is_prepare ? TRX_UNDO_LOG_GTID_XA : TRX_UNDO_LOG_GTID;

  return std::make_tuple(flag, size);
}

void trx_undo_gtid_set(trx_t *trx, trx_undo_t *undo, bool is_xa_prepare) {
  int gtid_flag;
  /* For XA prepare we store the GTID separately. */
  std::tie(gtid_flag, std::ignore) = undo->gtid_get_details(is_xa_prepare);

  /* Reset GTID flag */
  undo->flag &= ~gtid_flag;

  if (!trx->persists_gtid) {
    return;
  }

  /* Verify that we have allocated for GTID */
  if (!undo->gtid_allocated(is_xa_prepare)) {
    ib::error(ER_IB_CLONE_GTID_PERSIST)
        << "Could not persist GTID as space for GTID is not allocated.";
    ut_d(ut_error);
    ut_o(return);
  }
  undo->flag |= gtid_flag;
}

void trx_undo_gtid_read_and_persist(trx_ulogf_t *undo_header) {
  /* Check if undo log has GTID. */
  auto flag = mach_read_ulint(undo_header + TRX_UNDO_FLAGS, MLOG_1BYTE);

  /* Extract and add GTID information of the transaction to the persister. */
  Gtid_desc gtid_desc;

  /* Get GTID persister */
  auto &gtid_persistor = clone_sys->get_gtid_persistor();

  /* Extract and add XA prepare GTID, if there and if and only if the
     transaction is in PREPARED_IN_TC state, otherwise, there is no assurance
     that the transaction will be kept in prepared state. */
  if ((flag & TRX_UNDO_FLAG_XA_PREPARE_GTID) != 0) {
    /* Get GTID format version. */
    gtid_desc.m_version = static_cast<uint32_t>(
        mach_read_from_1(undo_header + TRX_UNDO_LOG_GTID_VERSION));

    /* Get GTID information string. */
    memcpy(&gtid_desc.m_info[0], undo_header + TRX_UNDO_LOG_GTID_XA,
           TRX_UNDO_LOG_GTID_LEN);
    /* Mark GTID valid. */
    gtid_desc.m_is_set = true;

    /* No concurrency is involved during recovery but satisfy
    the interface requirement. */
    trx_sys_serialisation_mutex_enter();
    gtid_persistor.add(gtid_desc);
    trx_sys_serialisation_mutex_exit();
  }

  if ((flag & TRX_UNDO_FLAG_GTID) == 0) {
    return;
  }

  /* Get GTID format version. */
  gtid_desc.m_version = static_cast<uint32_t>(
      mach_read_from_1(undo_header + TRX_UNDO_LOG_GTID_VERSION));

  /* Get GTID information string. */
  memcpy(&gtid_desc.m_info[0], undo_header + TRX_UNDO_LOG_GTID,
         TRX_UNDO_LOG_GTID_LEN);
  /* Mark GTID valid. */
  gtid_desc.m_is_set = true;

  /* No concurrency is involved during recovery but satisfy
  the interface requirement. */
  trx_sys_serialisation_mutex_enter();
  gtid_persistor.add(gtid_desc);
  trx_sys_serialisation_mutex_exit();
}

void trx_undo_gtid_write(trx_t *trx, trx_ulogf_t *undo_header, trx_undo_t *undo,
                         mtr_t *mtr, bool is_xa_prepare) {
  int gtid_flag;
  size_t gtid_offset;

  std::tie(gtid_flag, gtid_offset) = undo->gtid_get_details(is_xa_prepare);

  if ((undo->flag & gtid_flag) == 0) {
    return;
  }

  /* Reset GTID flag */
  undo->flag &= ~gtid_flag;

  /* We must have allocated for GTID but add a safe check. */
  if (!undo->gtid_allocated(is_xa_prepare)) {
    ut_d(ut_error);
    ut_o(return);
  }

  Gtid_desc gtid_desc;
  auto &gtid_persistor = clone_sys->get_gtid_persistor();

  gtid_persistor.get_gtid_info(trx, gtid_desc);

  if (gtid_desc.m_is_set) {
    /* Persist GTID version */
    mlog_write_ulint(undo_header + TRX_UNDO_LOG_GTID_VERSION,
                     gtid_desc.m_version, MLOG_1BYTE, mtr);
    /* Persist fixed length GTID */
    static_assert(TRX_UNDO_LOG_GTID_LEN == GTID_INFO_SIZE);
    mlog_write_string(undo_header + gtid_offset, &gtid_desc.m_info[0],
                      TRX_UNDO_LOG_GTID_LEN, mtr);
    undo->flag |= gtid_flag;
  }
  mlog_write_ulint(undo_header + TRX_UNDO_FLAGS, undo->flag, MLOG_1BYTE, mtr);
}

/** Read X/Open XA Transaction Identification (XID) from undo log header */
static void trx_undo_read_xid(
    trx_ulogf_t *log_hdr, /*!< in: undo log header */
    XID *xid)             /*!< out: X/Open XA Transaction Identification */
{
  xid->set_format_id(
      static_cast<long>(mach_read_from_4(log_hdr + TRX_UNDO_XA_FORMAT)));

  xid->set_gtrid_length(
      static_cast<long>(mach_read_from_4(log_hdr + TRX_UNDO_XA_TRID_LEN)));

  xid->set_bqual_length(
      static_cast<long>(mach_read_from_4(log_hdr + TRX_UNDO_XA_BQUAL_LEN)));

  xid->set_data(log_hdr + TRX_UNDO_XA_XID, XIDDATASIZE);
}

/** Adds space for the XA XID after an undo log old-style header.
@param[in,out]  undo_page       Undo log segment header page
@param[in,out]  log_hdr         Undo log header
@param[in,out]  mtr             Mini-transaction
@param[in]      gtid_storage    GTID storage type */
static void trx_undo_header_add_space_for_xid(
    page_t *undo_page, trx_ulogf_t *log_hdr, mtr_t *mtr,
    trx_undo_t::Gtid_storage gtid_storage) {
  trx_upagef_t *page_hdr = undo_page + TRX_UNDO_PAGE_HDR;

  ulint free = mach_read_from_2(page_hdr + TRX_UNDO_PAGE_FREE);

  /* free is now the end offset of the old style undo log header */
  ut_a(free == (ulint)(log_hdr - undo_page) + TRX_UNDO_LOG_OLD_HDR_SIZE);

  ulint new_limit = TRX_UNDO_LOG_XA_HDR_SIZE;

  switch (gtid_storage) {
    case trx_undo_t::Gtid_storage::COMMIT:
      new_limit = TRX_UNDO_LOG_GTID_HDR_SIZE;
      break;

    case trx_undo_t::Gtid_storage::PREPARE_AND_COMMIT:
      new_limit = TRX_UNDO_LOG_GTID_XA_HDR_SIZE;
      break;

    case trx_undo_t::Gtid_storage::NONE:
      new_limit = TRX_UNDO_LOG_XA_HDR_SIZE;
      break;

    default:
      ut_d(ut_error);
  }

  ulint new_free = free + (new_limit - TRX_UNDO_LOG_OLD_HDR_SIZE);

  /* Add space for a XID after the header, update the free offset
  fields on the undo log page and in the undo log header */

  mlog_write_ulint(page_hdr + TRX_UNDO_PAGE_START, new_free, MLOG_2BYTES, mtr);

  mlog_write_ulint(page_hdr + TRX_UNDO_PAGE_FREE, new_free, MLOG_2BYTES, mtr);

  mlog_write_ulint(log_hdr + TRX_UNDO_LOG_START, new_free, MLOG_2BYTES, mtr);
}

/** Writes the mtr log entry of an undo log header reuse. */
static inline void trx_undo_insert_header_reuse_log(
    const page_t *undo_page, /*!< in: undo log header page */
    trx_id_t trx_id,         /*!< in: transaction id */
    mtr_t *mtr)              /*!< in: mtr */
{
  mlog_write_initial_log_record(undo_page, MLOG_UNDO_HDR_REUSE, mtr);

  mlog_catenate_ull_compressed(mtr, trx_id);
}
#else /* !UNIV_HOTBACKUP */
#define trx_undo_insert_header_reuse_log(undo_page, trx_id, mtr) ((void)0)
#endif /* !UNIV_HOTBACKUP */

/** Parse the redo log entry of an undo log page header create or reuse.
@param[in]      type     MLOG_UNDO_HDR_CREATE or MLOG_UNDO_HDR_REUSE
@param[in]      ptr      Redo log record
@param[in]      end_ptr  End of log buffer
@param[in,out]  page     Page frame or NULL
@param[in,out]  mtr      Mini-transaction or NULL
@return end of log record or NULL */
byte *trx_undo_parse_page_header(mlog_id_t type, const byte *ptr,
                                 const byte *end_ptr, page_t *page,
                                 mtr_t *mtr) {
  trx_id_t trx_id = mach_u64_parse_compressed(&ptr, end_ptr);

  if (ptr != nullptr && page != nullptr) {
    switch (type) {
      case MLOG_UNDO_HDR_CREATE:
        trx_undo_header_create(page, trx_id, mtr);
        return (const_cast<byte *>(ptr));
      case MLOG_UNDO_HDR_REUSE:
        trx_undo_insert_header_reuse(page, trx_id, mtr);
        return (const_cast<byte *>(ptr));
      default:
        break;
    }
    ut_d(ut_error);
  }

  return (const_cast<byte *>(ptr));
}

/** Initializes a cached insert undo log header page for new use. NOTE that this
 function has its own log record type MLOG_UNDO_HDR_REUSE. You must NOT change
 the operation of this function!
 @return undo log header byte offset on page */
static ulint trx_undo_insert_header_reuse(
    page_t *undo_page, /*!< in/out: insert undo log segment
                       header page, x-latched */
    trx_id_t trx_id,   /*!< in: transaction id */
    mtr_t *mtr)        /*!< in: mtr */
{
  trx_upagef_t *page_hdr;
  trx_usegf_t *seg_hdr;
  trx_ulogf_t *log_hdr;
  ulint free;
  ulint new_free;

  ut_ad(mtr && undo_page);

  page_hdr = undo_page + TRX_UNDO_PAGE_HDR;
  seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

  free = TRX_UNDO_SEG_HDR + TRX_UNDO_SEG_HDR_SIZE;

  ut_a(free + TRX_UNDO_LOG_GTID_XA_HDR_SIZE < UNIV_PAGE_SIZE - 100);

  log_hdr = undo_page + free;

  new_free = free + TRX_UNDO_LOG_OLD_HDR_SIZE;

  /* Insert undo data is not needed after commit: we may free all
  the space on the page */

  ut_a(mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_TYPE) ==
       TRX_UNDO_INSERT);

  mach_write_to_2(page_hdr + TRX_UNDO_PAGE_START, new_free);

  mach_write_to_2(page_hdr + TRX_UNDO_PAGE_FREE, new_free);

  mach_write_to_2(seg_hdr + TRX_UNDO_STATE, TRX_UNDO_ACTIVE);

  log_hdr = undo_page + free;

  mach_write_to_8(log_hdr + TRX_UNDO_TRX_ID, trx_id);
  mach_write_to_2(log_hdr + TRX_UNDO_LOG_START, new_free);

  mach_write_to_1(log_hdr + TRX_UNDO_FLAGS, 0);
  mach_write_to_1(log_hdr + TRX_UNDO_DICT_TRANS, false);

  /* Write the log record MLOG_UNDO_HDR_REUSE */
  trx_undo_insert_header_reuse_log(undo_page, trx_id, mtr);

  return (free);
}

#ifndef UNIV_HOTBACKUP
/** Tries to add a page to the undo log segment where the undo log is placed.
 @return X-latched block if success, else NULL */
buf_block_t *trx_undo_add_page(
    trx_t *trx,               /*!< in: transaction */
    trx_undo_t *undo,         /*!< in: undo log memory object */
    trx_undo_ptr_t *undo_ptr, /*!< in: assign undo log from
                              referred rollback segment. */
    mtr_t *mtr)               /*!< in: mtr which does not have
                              a latch to any undo log page;
                              the caller must have reserved
                              the rollback segment mutex */
{
  page_t *header_page;
  buf_block_t *new_block;
  page_t *new_page;
  trx_rseg_t *rseg;
  ulint n_reserved;

  ut_ad(mutex_own(&(trx->undo_mutex)));
  ut_ad(mutex_own(&(undo_ptr->rseg->mutex)));

  rseg = undo_ptr->rseg;

  if (rseg->get_curr_size() == rseg->max_size) {
    return (nullptr);
  }

  header_page = trx_undo_page_get(page_id_t(undo->space, undo->hdr_page_no),
                                  undo->page_size, mtr);

  if (!fsp_reserve_free_extents(&n_reserved, undo->space, 1, FSP_UNDO, mtr)) {
    return (nullptr);
  }

  new_block = fseg_alloc_free_page_general(
      TRX_UNDO_SEG_HDR + TRX_UNDO_FSEG_HEADER + header_page,
      undo->top_page_no + 1, FSP_UP, true, mtr, mtr);

  fil_space_release_free_extents(undo->space, n_reserved);

  if (new_block == nullptr) {
    /* No space left */

    return (nullptr);
  }

  ut_ad(rw_lock_get_x_lock_count(&new_block->lock) == 1);
  buf_block_dbg_add_level(new_block, SYNC_TRX_UNDO_PAGE);
  undo->last_page_no = new_block->page.id.page_no();

  new_page = buf_block_get_frame(new_block);

  trx_undo_page_init(new_page, undo->type, mtr);

  flst_add_last(header_page + TRX_UNDO_SEG_HDR + TRX_UNDO_PAGE_LIST,
                new_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_NODE, mtr);
  undo->size++;
  rseg->incr_curr_size();

  return (new_block);
}

/** Frees an undo log page that is not the header page.
 @return last page number in remaining log */
static page_no_t trx_undo_free_page(
    trx_rseg_t *rseg,      /*!< in: rollback segment */
    bool in_history,       /*!< in: true if the undo log is in the history
                            list */
    space_id_t space,      /*!< in: space */
    page_no_t hdr_page_no, /*!< in: header page number */
    page_no_t page_no,     /*!< in: page number to free: must not be the
                           header page */
    mtr_t *mtr)            /*!< in: mtr which does not have a latch to any
                           undo log page; the caller must have reserved
                           the rollback segment mutex */
{
  page_t *header_page;
  page_t *undo_page;
  fil_addr_t last_addr;
  trx_rsegf_t *rseg_header;
  ulint hist_size;

  ut_a(hdr_page_no != page_no);
  ut_ad(mutex_own(&(rseg->mutex)));

  undo_page =
      trx_undo_page_get(page_id_t(space, page_no), rseg->page_size, mtr);

  header_page =
      trx_undo_page_get(page_id_t(space, hdr_page_no), rseg->page_size, mtr);

  flst_remove(header_page + TRX_UNDO_SEG_HDR + TRX_UNDO_PAGE_LIST,
              undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_NODE, mtr);

  fseg_free_page(header_page + TRX_UNDO_SEG_HDR + TRX_UNDO_FSEG_HEADER, space,
                 page_no, false, mtr);

  last_addr =
      flst_get_last(header_page + TRX_UNDO_SEG_HDR + TRX_UNDO_PAGE_LIST, mtr);

  rseg->decr_curr_size();

  if (in_history) {
    rseg_header = trx_rsegf_get(space, rseg->page_no, rseg->page_size, mtr);

    hist_size =
        mtr_read_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, mtr);
    ut_ad(hist_size > 0);
    mlog_write_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE, hist_size - 1,
                     MLOG_4BYTES, mtr);
  }

  return (last_addr.page);
}

void trx_undo_free_last_page_func(IF_DEBUG(const trx_t *trx, ) trx_undo_t *undo,
                                  mtr_t *mtr) {
  ut_ad(mutex_own(&trx->undo_mutex));
  ut_ad(undo->hdr_page_no != undo->last_page_no);
  ut_ad(undo->size > 0);

  undo->last_page_no =
      trx_undo_free_page(undo->rseg, false, undo->space, undo->hdr_page_no,
                         undo->last_page_no, mtr);

  undo->size--;
}

/** Empties an undo log header page of undo records for that undo log.
Other undo logs may still have records on that page, if it is an update
undo log.
@param[in]      space_id     Tablespace ID
@param[in]      page_size    Page size
@param[in]      hdr_page_no  Header page number
@param[in]      hdr_offset   Header offset
@param[in,out]  mtr          Mini-transaction */
static void trx_undo_empty_header_page(space_id_t space_id,
                                       const page_size_t &page_size,
                                       page_no_t hdr_page_no, ulint hdr_offset,
                                       mtr_t *mtr) {
  page_t *header_page;
  trx_ulogf_t *log_hdr;
  ulint end;

  header_page =
      trx_undo_page_get(page_id_t(space_id, hdr_page_no), page_size, mtr);

  log_hdr = header_page + hdr_offset;

  end = trx_undo_page_get_end(header_page, hdr_page_no, hdr_offset);

  mlog_write_ulint(log_hdr + TRX_UNDO_LOG_START, end, MLOG_2BYTES, mtr);
}

/** Get page offset up to which undo logs can be truncated.
There are three possibilities.
1. Truncate nothing on this page. Return -1
2. Truncate part of the page. Return the offset
3. Truncate the whole page. Return 0
@param[in]  undo       undo log to truncate
@param[in]  undo_page  undo log page to check
@param[in]  limit      limit up to which undo logs to be truncated
@return page offset to truncate to, 0 for whole page, -1 for nothing. */
int trx_undo_page_truncate_offset(trx_undo_t *undo, page_t *undo_page,
                                  undo_no_t limit) {
  auto rec = trx_undo_page_get_last_rec(undo_page, undo->hdr_page_no,
                                        undo->hdr_offset);
  trx_undo_rec_t *trunc_rec = nullptr;

  while (rec != nullptr) {
    /* Check if current record has gone below the limit. */
    if (trx_undo_rec_get_undo_no(rec) < limit) {
      /* If this is the first record on the page, don't truncate anything */
      if (trunc_rec == nullptr) {
        return (-1);
      }

      /* Return an offset within the page. */
      return (trunc_rec - undo_page);
    }

    /* Truncate at least up to this record, maybe more */
    trunc_rec = rec;
    rec = trx_undo_page_get_prev_rec(rec, undo->hdr_page_no, undo->hdr_offset);
  }

  /* All records on the page are >= limit */
  if (undo->last_page_no == undo->hdr_page_no) {
    /* This is the header page. Return an offset
    if there are any records on the page. */
    if (trunc_rec != nullptr) {
      return (trunc_rec - undo_page);
    }

    /* Header page is empty.  Do not truncate anything. */
    return (-1);
  }

  /* Truncate the whole page. */
  return (0);
}

/** Truncates an undo log from the end. This function is used during a rollback
 to free space from an undo log.
@param[in]  trx    transaction for this undo log
@param[in]  undo   undo log
@param[in]  limit  all undo records with undo number;
                   This value should be truncated. */
void trx_undo_truncate_end_func(IF_DEBUG(const trx_t *trx, ) trx_undo_t *undo,
                                undo_no_t limit) {
  ut_ad(mutex_own(&trx->undo_mutex));
  ut_ad(mutex_own(&undo->rseg->mutex));

  mtr_t mtr;

  for (;;) {
    mtr.start();

    /* Set NO_REDO for temporary undo logs. */
    if (fsp_is_system_temporary(undo->rseg->space_id)) {
      ut_ad(trx->rsegs.m_noredo.rseg == undo->rseg);
      mtr.set_log_mode(MTR_LOG_NO_REDO);
    } else {
      ut_ad(trx->rsegs.m_redo.rseg == undo->rseg);
    }

    const page_id_t page_id(undo->space, undo->last_page_no);

    auto undo_page = trx_undo_page_get(page_id, undo->page_size, &mtr);

    int trunc_offset = trx_undo_page_truncate_offset(undo, undo_page, limit);

    /* If offset is within the page, truncate part of the page and quit.*/
    if (trunc_offset > 0) {
      mlog_write_ulint(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE,
                       trunc_offset, MLOG_2BYTES, &mtr);
      break;
    }

    /* If all recs are < limit, don't truncate anything. */
    if (trunc_offset < 0) {
      break;
    }

    /* Free the last page and move on to the next. */
    ut_ad(undo->last_page_no != undo->hdr_page_no);
    trx_undo_free_last_page_func(IF_DEBUG(trx, ) undo, &mtr);

    mtr.commit();
  }

  mtr.commit();
}

/** Truncate the head of an undo log.
NOTE that only whole pages are freed; the header page is not
freed, but emptied, if all the records there are below the limit.
@param[in,out]  rseg            rollback segment
@param[in]      hdr_page_no     header page number
@param[in]      hdr_offset      header offset on the page
@param[in]      limit           first undo number to preserve
(everything below the limit will be truncated) */
void trx_undo_truncate_start(trx_rseg_t *rseg, page_no_t hdr_page_no,
                             ulint hdr_offset, undo_no_t limit) {
  page_t *undo_page;
  trx_undo_rec_t *rec;
  trx_undo_rec_t *last_rec;
  page_no_t page_no;
  mtr_t mtr;

  ut_ad(mutex_own(&(rseg->mutex)));

  if (!limit) {
    return;
  }
loop:
  mtr.start();

  if (fsp_is_system_temporary(rseg->space_id)) {
    mtr.set_log_mode(MTR_LOG_NO_REDO);
  }

  rec = trx_undo_get_first_rec(nullptr, rseg->space_id, rseg->page_size,
                               hdr_page_no, hdr_offset, RW_X_LATCH, &mtr);
  if (rec == nullptr) {
    /* Already empty */

    mtr.commit();

    return;
  }

  undo_page = page_align(rec);

  last_rec = trx_undo_page_get_last_rec(undo_page, hdr_page_no, hdr_offset);
  if (trx_undo_rec_get_undo_no(last_rec) >= limit) {
    mtr.commit();

    return;
  }

  page_no = page_get_page_no(undo_page);

  if (page_no == hdr_page_no) {
    trx_undo_empty_header_page(rseg->space_id, rseg->page_size, hdr_page_no,
                               hdr_offset, &mtr);
  } else {
    trx_undo_free_page(rseg, true, rseg->space_id, hdr_page_no, page_no, &mtr);
  }

  mtr.commit();

  goto loop;
}

/** Frees an undo log segment which is not in the history list.
@param[in]      undo    undo log
@param[in]      noredo  whether the undo tablespace is redo logged */
static void trx_undo_seg_free(const trx_undo_t *undo, bool noredo) {
  bool finished;

  auto rseg = undo->rseg;

  do {
    mtr_t mtr;
    mtr.start();

    if (noredo) {
      mtr.set_log_mode(MTR_LOG_NO_REDO);
    }

    rseg->latch();

    auto seg_header =
        trx_undo_page_get(page_id_t(undo->space, undo->hdr_page_no),
                          undo->page_size, &mtr) +
        TRX_UNDO_SEG_HDR;

    auto file_seg = seg_header + TRX_UNDO_FSEG_HEADER;

    finished = fseg_free_step(file_seg, false, &mtr);

    if (finished) {
      /* Update the rseg header */
      auto rseg_header =
          trx_rsegf_get(rseg->space_id, rseg->page_no, rseg->page_size, &mtr);
      trx_rsegf_set_nth_undo(rseg_header, undo->id, FIL_NULL, &mtr);

      rseg->decr_curr_size(undo->size);
      MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_USED);
    }

    rseg->unlatch();
    mtr.commit();
  } while (!finished);
}

/*========== UNDO LOG MEMORY COPY INITIALIZATION =====================*/

/** Creates and initializes an undo log memory object for a newly created
rseg. The memory object is inserted in the appropriate list in the rseg.
 @return own: the undo log memory object */
static trx_undo_t *trx_undo_mem_init(
    trx_rseg_t *rseg,  /*!< in: rollback segment memory object */
    ulint id,          /*!< in: slot index within rseg */
    page_no_t page_no, /*!< in: undo log segment page number */
    mtr_t *mtr)        /*!< in: mtr */
{
  page_t *undo_page;
  trx_upagef_t *page_header;
  trx_usegf_t *seg_header;
  trx_ulogf_t *undo_header;
  trx_undo_t *undo;
  ulint type;
  ulint state;
  trx_id_t trx_id;
  ulint offset;
  fil_addr_t last_addr;
  page_t *last_page;
  trx_undo_rec_t *rec;
  XID xid;

  ut_a(id < TRX_RSEG_N_SLOTS);

  undo_page = trx_undo_page_get(page_id_t(rseg->space_id, page_no),
                                rseg->page_size, mtr);

  page_header = undo_page + TRX_UNDO_PAGE_HDR;

  type = mtr_read_ulint(page_header + TRX_UNDO_PAGE_TYPE, MLOG_2BYTES, mtr);
  seg_header = undo_page + TRX_UNDO_SEG_HDR;

  state = mach_read_from_2(seg_header + TRX_UNDO_STATE);

  offset = mach_read_from_2(seg_header + TRX_UNDO_LAST_LOG);

  undo_header = undo_page + offset;

  trx_id = mach_read_from_8(undo_header + TRX_UNDO_TRX_ID);

  auto flag = mtr_read_ulint(undo_header + TRX_UNDO_FLAGS, MLOG_1BYTE, mtr);

  bool xid_exists = ((flag & TRX_UNDO_FLAG_XID) != 0);

  /* Read X/Open XA transaction identification if it exists, or
  set it to NULL. */
  xid.reset();

  if (xid_exists) {
    trx_undo_read_xid(undo_header, &xid);
  }

  undo = trx_undo_mem_create(rseg, id, type, trx_id, &xid, page_no, offset);

  undo->dict_operation =
      mtr_read_ulint(undo_header + TRX_UNDO_DICT_TRANS, MLOG_1BYTE, mtr);

  undo->flag = flag;

  undo->m_gtid_storage = trx_undo_t::Gtid_storage::NONE;

  if ((flag & TRX_UNDO_FLAG_XA_PREPARE_GTID) != 0) {
    /* Prepare GTID implies space is allocated for both
    prepare and commit GTID. */
    undo->m_gtid_storage = trx_undo_t::Gtid_storage::PREPARE_AND_COMMIT;
  } else if ((flag & TRX_UNDO_FLAG_GTID) != 0) {
    /* Space is allocated for only commit GTID. */
    undo->m_gtid_storage = trx_undo_t::Gtid_storage::COMMIT;
  }

  undo->state = state;
  undo->size = flst_get_len(seg_header + TRX_UNDO_PAGE_LIST);

  /* If the log segment is being freed, the page list is inconsistent! */
  if (state == TRX_UNDO_TO_FREE) {
    goto add_to_list;
  }

  last_addr = flst_get_last(seg_header + TRX_UNDO_PAGE_LIST, mtr);

  undo->last_page_no = last_addr.page;
  undo->top_page_no = last_addr.page;

  last_page = trx_undo_page_get(page_id_t(rseg->space_id, undo->last_page_no),
                                rseg->page_size, mtr);

  rec = trx_undo_page_get_last_rec(last_page, page_no, offset);

  if (rec == nullptr) {
    undo->empty = true;
  } else {
    undo->empty = false;
    undo->top_offset = rec - last_page;
    undo->top_undo_no = trx_undo_rec_get_undo_no(rec);
  }
add_to_list:
  if (type == TRX_UNDO_INSERT) {
    if (state != TRX_UNDO_CACHED) {
      UT_LIST_ADD_LAST(rseg->insert_undo_list, undo);
    } else {
      UT_LIST_ADD_LAST(rseg->insert_undo_cached, undo);

      MONITOR_INC(MONITOR_NUM_UNDO_SLOT_CACHED);
    }
  } else {
    ut_ad(type == TRX_UNDO_UPDATE);
    if (state != TRX_UNDO_CACHED) {
      UT_LIST_ADD_LAST(rseg->update_undo_list, undo);
      /* For XA prepared transaction and XA rolled back transaction, we
      could have GTID to be persisted. */
      if (undo->is_prepared() || state == TRX_UNDO_ACTIVE) {
        trx_undo_gtid_read_and_persist(undo_header);
      }
    } else {
      UT_LIST_ADD_LAST(rseg->update_undo_cached, undo);

      MONITOR_INC(MONITOR_NUM_UNDO_SLOT_CACHED);
    }
  }

  return (undo);
}

/** Initializes the undo log lists for a rollback segment memory copy. This
 function is only called when the database is started or a new rollback
 segment is created.
 @return the combined size of undo log segments in pages */
ulint trx_undo_lists_init(
    trx_rseg_t *rseg) /*!< in: rollback segment memory object */
{
  ulint size = 0;
  trx_rsegf_t *rseg_header;
  ulint i;
  mtr_t mtr;

  mtr.start();

  rseg_header =
      trx_rsegf_get_new(rseg->space_id, rseg->page_no, rseg->page_size, &mtr);

  for (i = 0; i < TRX_RSEG_N_SLOTS; i++) {
    page_no_t page_no;

    page_no = trx_rsegf_get_nth_undo(rseg_header, i, &mtr);

    /* In forced recovery: try to avoid operations which look
    at database pages; undo logs are rapidly changing data, and
    the probability that they are in an inconsistent state is
    high */

    if (page_no != FIL_NULL &&
        srv_force_recovery < SRV_FORCE_NO_UNDO_LOG_SCAN) {
      trx_undo_t *undo;

      undo = trx_undo_mem_init(rseg, i, page_no, &mtr);

      size += undo->size;

      mtr.commit();

      mtr.start();

      rseg_header =
          trx_rsegf_get(rseg->space_id, rseg->page_no, rseg->page_size, &mtr);

      /* Found a used slot */
      MONITOR_INC(MONITOR_NUM_UNDO_SLOT_USED);
    }
  }

  mtr.commit();

  return (size);
}

/** Creates and initializes an undo log memory object.
@param[in]   rseg     rollback segment memory object
@param[in]   id       slot index within rseg
@param[in]   type     type of the log: TRX_UNDO_INSERT or TRX_UNDO_UPDATE
@param[in]   trx_id   id of the trx for which the undo log is created
@param[in]   xid      X/Open XA transaction identification
@param[in]   page_no  undo log header page number
@param[in]   offset   undo log header byte offset on page
@return own: the undo log memory object */
static trx_undo_t *trx_undo_mem_create(trx_rseg_t *rseg, ulint id, ulint type,
                                       trx_id_t trx_id, const XID *xid,
                                       page_no_t page_no, ulint offset) {
  trx_undo_t *undo;

  ut_a(id < TRX_RSEG_N_SLOTS);

  undo = static_cast<trx_undo_t *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*undo)));

  if (undo == nullptr) {
    return (nullptr);
  }

  undo->id = id;
  undo->type = type;
  undo->state = TRX_UNDO_ACTIVE;
  undo->del_marks = false;
  undo->trx_id = trx_id;
  undo->xid = *xid;

  undo->dict_operation = false;
  undo->flag = 0;
  undo->m_gtid_storage = trx_undo_t::Gtid_storage::NONE;

  undo->rseg = rseg;

  undo->space = rseg->space_id;
  undo->page_size.copy_from(rseg->page_size);
  undo->hdr_page_no = page_no;
  undo->hdr_offset = offset;
  undo->last_page_no = page_no;
  undo->size = 1;

  undo->empty = true;
  undo->top_page_no = page_no;
  undo->guess_block = nullptr;

  return (undo);
}

/** Initializes a cached undo log object for new use. */
static void trx_undo_mem_init_for_reuse(
    trx_undo_t *undo, /*!< in: undo log to init */
    trx_id_t trx_id,  /*!< in: id of the trx for which the undo log
                      is created */
    const XID *xid,   /*!< in: X/Open XA transaction identification*/
    ulint offset)     /*!< in: undo log header byte offset on page */
{
  ut_ad(mutex_own(&((undo->rseg)->mutex)));

  ut_a(undo->id < TRX_RSEG_N_SLOTS);

  undo->state = TRX_UNDO_ACTIVE;
  undo->del_marks = false;
  undo->trx_id = trx_id;
  undo->xid = *xid;

  undo->dict_operation = false;
  undo->flag = 0;
  undo->m_gtid_storage = trx_undo_t::Gtid_storage::NONE;

  undo->hdr_offset = offset;
  undo->empty = true;
}

/** Frees an undo log memory copy. */
void trx_undo_mem_free(trx_undo_t *undo) /*!< in: the undo object to be freed */
{
  ut_a(undo->id < TRX_RSEG_N_SLOTS);

  ut::free(undo);
}

/** Create a new undo log in the given rollback segment.
@param[in]   rseg   rollback segment memory copy
@param[in]   type   type of the log: TRX_UNDO_INSERT or TRX_UNDO_UPDATE
@param[in]   trx_id  id of the trx for which the undo log is created
@param[in]   xid     X/Open transaction identification
@param[in]   gtid_storage GTID storage type
@param[out]  undo    the new undo log object, undefined if did not succeed
@param[in]   mtr     mini-transation
@retval DB_SUCCESS if successful in creating the new undo lob object,
@retval DB_TOO_MANY_CONCURRENT_TRXS too many concurrent trxs
@retval DB_OUT_OF_FILE_SPACE        out of file-space
@retval DB_OUT_OF_MEMORY            out of memory */
[[nodiscard]] static dberr_t trx_undo_create(
    trx_rseg_t *rseg, ulint type, trx_id_t trx_id, const XID *xid,
    trx_undo_t::Gtid_storage gtid_storage, trx_undo_t **undo, mtr_t *mtr) {
  trx_rsegf_t *rseg_header;
  page_no_t page_no;
  ulint offset;
  ulint id;
  page_t *undo_page;

  ut_ad(mutex_own(&(rseg->mutex)));

  if (rseg->get_curr_size() == rseg->max_size) {
    return DB_OUT_OF_FILE_SPACE;
  }

  rseg->incr_curr_size();

  rseg_header =
      trx_rsegf_get(rseg->space_id, rseg->page_no, rseg->page_size, mtr);

  auto err = trx_undo_seg_create(rseg, rseg_header, type, &id, &undo_page, mtr);

  if (err != DB_SUCCESS) {
    /* Did not succeed */
    rseg->decr_curr_size();
    return err;
  }

  page_no = page_get_page_no(undo_page);

  offset = trx_undo_header_create(undo_page, trx_id, mtr);

  /* GTID storage is needed only for update undo log. */
  if (type != TRX_UNDO_UPDATE) {
    gtid_storage = trx_undo_t::Gtid_storage::NONE;
  }

  trx_undo_header_add_space_for_xid(undo_page, undo_page + offset, mtr,
                                    gtid_storage);

  *undo = trx_undo_mem_create(rseg, id, type, trx_id, xid, page_no, offset);
  if (*undo == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  (*undo)->m_gtid_storage = gtid_storage;
  return DB_SUCCESS;
}

/*================ UNDO LOG ASSIGNMENT AND CLEANUP =====================*/

/** Reuses a cached undo log.
@param[in,out]  rseg    Rollback segment memory object
@param[in]      type    Type of the log: TRX_UNDO_INSERT or TRX_UNDO_UPDATE
@param[in]      trx_id  Id of the trx for which the undo log is used
@param[in]      xid     X/Open XA transaction identification
@param[in]      gtid_storage GTID storage type
@param[in,out]  mtr     Mini-transaction
@return the undo log memory object, NULL if none cached */
static trx_undo_t *trx_undo_reuse_cached(trx_rseg_t *rseg, ulint type,
                                         trx_id_t trx_id, const XID *xid,
                                         trx_undo_t::Gtid_storage gtid_storage,
                                         mtr_t *mtr) {
  trx_undo_t *undo;

  ut_ad(mutex_own(&(rseg->mutex)));

  if (type == TRX_UNDO_INSERT) {
    undo = UT_LIST_GET_FIRST(rseg->insert_undo_cached);
    if (undo == nullptr) {
      return (nullptr);
    }

    UT_LIST_REMOVE(rseg->insert_undo_cached, undo);

    MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_CACHED);
  } else {
    ut_ad(type == TRX_UNDO_UPDATE);

    undo = UT_LIST_GET_FIRST(rseg->update_undo_cached);
    if (undo == nullptr) {
      return (nullptr);
    }

    UT_LIST_REMOVE(rseg->update_undo_cached, undo);

    MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_CACHED);
  }

  ut_ad(undo->size == 1);
  ut_a(undo->id < TRX_RSEG_N_SLOTS);

  auto undo_page = trx_undo_page_get(page_id_t(undo->space, undo->hdr_page_no),
                                     undo->page_size, mtr);
  ulint offset;

  if (type == TRX_UNDO_INSERT) {
    offset = trx_undo_insert_header_reuse(undo_page, trx_id, mtr);
    gtid_storage = trx_undo_t::Gtid_storage::NONE;

  } else {
    ut_a(mach_read_from_2(undo_page + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_TYPE) ==
         TRX_UNDO_UPDATE);

    offset = trx_undo_header_create(undo_page, trx_id, mtr);
  }

  trx_undo_header_add_space_for_xid(undo_page, undo_page + offset, mtr,
                                    gtid_storage);

  trx_undo_mem_init_for_reuse(undo, trx_id, xid, offset);

  undo->m_gtid_storage = gtid_storage;

  return (undo);
}

/** Marks an undo log header as a header of a data dictionary operation
 transaction. */
static void trx_undo_mark_as_dict_operation(
    trx_undo_t *undo, /*!< in: assigned undo log */
    mtr_t *mtr)       /*!< in: mtr */
{
  page_t *hdr_page;

  hdr_page = trx_undo_page_get(page_id_t(undo->space, undo->hdr_page_no),
                               undo->page_size, mtr);

  mlog_write_ulint(hdr_page + undo->hdr_offset + TRX_UNDO_DICT_TRANS, true,
                   MLOG_1BYTE, mtr);

  undo->dict_operation = true;
}

/** Assigns an undo log for a transaction. A new undo log is created or a cached
 undo log reused.
 @return DB_SUCCESS if undo log assign successful, possible error codes
 are: DB_TOO_MANY_CONCURRENT_TRXS DB_OUT_OF_FILE_SPACE DB_READ_ONLY
 DB_OUT_OF_MEMORY */
dberr_t trx_undo_assign_undo(
    trx_t *trx,               /*!< in: transaction */
    trx_undo_ptr_t *undo_ptr, /*!< in: assign undo log from
                              referred rollback segment. */
    ulint type)               /*!< in: TRX_UNDO_INSERT or
                              TRX_UNDO_UPDATE */
{
  trx_rseg_t *rseg;
  trx_undo_t *undo;
  mtr_t mtr;
  dberr_t err = DB_SUCCESS;

  ut_ad(trx);

  /* In case of read-only scenario trx->rsegs.m_redo.rseg can be NULL but
  still request for assigning undo logs is valid as temporary tables
  can be updated in read-only mode.
  If there is no rollback segment assigned to trx and still there is
  object being updated there is something wrong and so this condition
  check. */
  ut_ad(trx_is_rseg_assigned(trx));

  rseg = undo_ptr->rseg;

  ut_ad(mutex_own(&(trx->undo_mutex)));

  bool no_redo = (&trx->rsegs.m_noredo == undo_ptr);

  /* If none of the undo pointers are assigned then this is
  first time transaction is allocating undo segment. */
  bool is_first = undo_ptr->is_empty();

  /* If any undo segment is assigned it is guaranteed that
  Innodb would persist GTID. Call it before any undo segment
  is assigned for transaction. We allocate space for GTID
  only if GTID is persisted. */
  trx_undo_t::Gtid_storage gtid_storage{trx_undo_t::Gtid_storage::NONE};

  if (!no_redo) {
    auto &gtid_persistor = clone_sys->get_gtid_persistor();
    if (is_first) {
      gtid_persistor.set_persist_gtid(trx, true);
    }
    /* Check if the undo segment needs to allocate for GTID. */
    gtid_storage = gtid_persistor.persists_gtid(trx);
  }

  mtr.start();
  if (no_redo) {
    mtr.set_log_mode(MTR_LOG_NO_REDO);
  } else {
    ut_ad(&trx->rsegs.m_redo == undo_ptr);
  }

  rseg->latch();

  DBUG_EXECUTE_IF("ib_create_table_fail_too_many_trx",
                  err = DB_TOO_MANY_CONCURRENT_TRXS;
                  goto func_exit;);
  undo =
#ifdef UNIV_DEBUG
      srv_inject_too_many_concurrent_trxs
          ? nullptr
          :
#endif
          trx_undo_reuse_cached(rseg, type, trx->id, trx->xid, gtid_storage,
                                &mtr);

  if (undo == nullptr) {
    err = trx_undo_create(rseg, type, trx->id, trx->xid, gtid_storage, &undo,
                          &mtr);
    if (err != DB_SUCCESS) {
      goto func_exit;
    }
  }

  if (type == TRX_UNDO_INSERT) {
    UT_LIST_ADD_FIRST(rseg->insert_undo_list, undo);
    ut_ad(undo_ptr->insert_undo == nullptr);
    undo_ptr->insert_undo = undo;
  } else {
    UT_LIST_ADD_FIRST(rseg->update_undo_list, undo);
    ut_ad(undo_ptr->update_undo == nullptr);
    undo_ptr->update_undo = undo;
  }

  if (trx->mysql_thd && !trx->ddl_operation &&
      thd_is_dd_update_stmt(trx->mysql_thd)) {
    trx->ddl_operation = true;
  }

  if (trx->ddl_operation || trx_get_dict_operation(trx) != TRX_DICT_OP_NONE) {
    trx_undo_mark_as_dict_operation(undo, &mtr);
  }

  /* For GTID persistence we might add undo segment to prepared transaction. If
  the transaction is in prepared state, we need to set XA properties. */
  if (trx_state_eq(trx, TRX_STATE_PREPARED)) {
    ut_ad(!is_first);
    undo->set_prepared(trx->xid);
  }

func_exit:
  rseg->unlatch();
  mtr.commit();

  return (err);
}

/** Sets the state of the undo log segment at a transaction finish.
 @return undo log segment header page, x-latched */
page_t *trx_undo_set_state_at_finish(
    trx_undo_t *undo, /*!< in: undo log memory copy */
    mtr_t *mtr)       /*!< in: mtr */
{
  trx_usegf_t *seg_hdr;
  trx_upagef_t *page_hdr;
  page_t *undo_page;
  ulint state;

  ut_a(undo->id < TRX_RSEG_N_SLOTS);

  undo_page = trx_undo_page_get(page_id_t(undo->space, undo->hdr_page_no),
                                undo->page_size, mtr);

  seg_hdr = undo_page + TRX_UNDO_SEG_HDR;
  page_hdr = undo_page + TRX_UNDO_PAGE_HDR;

  if (undo->size == 1 && mach_read_from_2(page_hdr + TRX_UNDO_PAGE_FREE) <
                             TRX_UNDO_PAGE_REUSE_LIMIT) {
    state = TRX_UNDO_CACHED;

  } else if (undo->type == TRX_UNDO_INSERT) {
    state = TRX_UNDO_TO_FREE;
  } else {
    state = TRX_UNDO_TO_PURGE;
  }

  undo->state = state;

  mlog_write_ulint(seg_hdr + TRX_UNDO_STATE, state, MLOG_2BYTES, mtr);

  return (undo_page);
}

/** Set the state of the undo log segment at a XA PREPARE or XA ROLLBACK.
@param[in,out]  trx             Transaction
@param[in,out]  undo            Insert_undo or update_undo log
@param[in]      rollback        false=XA PREPARE, true=XA ROLLBACK
@param[in,out]  mtr             Mini-transaction
@return undo log segment header page, x-latched */
page_t *trx_undo_set_state_at_prepare(trx_t *trx, trx_undo_t *undo,
                                      bool rollback, mtr_t *mtr) {
  trx_usegf_t *seg_hdr;
  trx_ulogf_t *undo_header;
  page_t *undo_page;
  ulint offset;

  ut_ad(trx && undo && mtr);

  ut_a(undo->id < TRX_RSEG_N_SLOTS);

  undo_page = trx_undo_page_get(page_id_t(undo->space, undo->hdr_page_no),
                                undo->page_size, mtr);

  seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

  offset = mach_read_from_2(seg_hdr + TRX_UNDO_LAST_LOG);
  undo_header = undo_page + offset;

  if (rollback) {
    ut_ad(undo->is_prepared());

    /* Write GTID information if there. */
    trx_undo_gtid_write(trx, undo_header, undo, mtr, !rollback);

    mlog_write_ulint(seg_hdr + TRX_UNDO_STATE, TRX_UNDO_ACTIVE, MLOG_2BYTES,
                     mtr);
    return (undo_page);
  }

  ut_ad(undo->state == TRX_UNDO_ACTIVE);
  undo->set_prepared(trx->xid);

  mlog_write_ulint(seg_hdr + TRX_UNDO_STATE, undo->state, MLOG_2BYTES, mtr);

  mlog_write_ulint(undo_header + TRX_UNDO_FLAGS, undo->flag, MLOG_1BYTE, mtr);

  trx_undo_write_xid(undo_header, &undo->xid, mtr);

  return (undo_page);
}

page_t *trx_undo_set_prepared_in_tc(trx_t *trx, trx_undo_t *undo, mtr_t *mtr) {
  trx_usegf_t *seg_hdr;
  trx_ulogf_t *undo_header;
  page_t *undo_page;
  ulint offset;

  ut_ad(trx && undo && mtr);

  ut_a(undo->id < TRX_RSEG_N_SLOTS);

  undo_page = trx_undo_page_get(page_id_t(undo->space, undo->hdr_page_no),
                                undo->page_size, mtr);

  seg_hdr = undo_page + TRX_UNDO_SEG_HDR;

  offset = mach_read_from_2(seg_hdr + TRX_UNDO_LAST_LOG);
  undo_header = undo_page + offset;

  ut_ad(undo->state == TRX_UNDO_PREPARED_80028 ||
        undo->state == TRX_UNDO_PREPARED);

  /* Write GTID information if there. */
  trx_undo_gtid_write(trx, undo_header, undo, mtr, true);

  undo->set_prepared_in_tc();

  mlog_write_ulint(seg_hdr + TRX_UNDO_STATE, undo->state, MLOG_2BYTES, mtr);

  return (undo_page);
}

/** Adds the update undo log header as the first in the history list, and
 frees the memory object, or puts it to the list of cached update undo log
 segments.
@param[in] trx Trx owning the update undo log
@param[in] undo_ptr Update undo log.
@param[in] undo_page Update undo log header page, x-latched
@param[in] update_rseg_history_len If true: update rseg history len else
skip updating it.
@param[in] n_added_logs Number of logs added
@param[in] mtr Mini-transaction */
void trx_undo_update_cleanup(trx_t *trx, trx_undo_ptr_t *undo_ptr,
                             page_t *undo_page, bool update_rseg_history_len,

                             ulint n_added_logs, mtr_t *mtr) {
  trx_rseg_t *rseg;
  trx_undo_t *undo;

  undo = undo_ptr->update_undo;
  rseg = undo_ptr->rseg;

  ut_ad(mutex_own(&(rseg->mutex)));

  trx_purge_add_update_undo_to_history(
      trx, undo_ptr, undo_page, update_rseg_history_len, n_added_logs, mtr);

  UT_LIST_REMOVE(rseg->update_undo_list, undo);

  undo_ptr->update_undo = nullptr;

  if (undo->state == TRX_UNDO_CACHED) {
    UT_LIST_ADD_FIRST(rseg->update_undo_cached, undo);

    MONITOR_INC(MONITOR_NUM_UNDO_SLOT_CACHED);
  } else {
    ut_ad(undo->state == TRX_UNDO_TO_PURGE);

    trx_undo_mem_free(undo);
  }
}

/** Frees an insert undo log after a transaction commit or rollback.
Knowledge of inserts is not needed after a commit or rollback, therefore
the data can be discarded.
@param[in,out]  undo_ptr        undo log to clean up
@param[in]      noredo          whether the undo tablespace is redo logged */
void trx_undo_insert_cleanup(trx_undo_ptr_t *undo_ptr, bool noredo) {
  trx_undo_t *undo;
  trx_rseg_t *rseg;

  undo = undo_ptr->insert_undo;
  ut_ad(undo != nullptr);

  rseg = undo_ptr->rseg;

  ut_ad(noredo == fsp_is_system_temporary(rseg->space_id));

  rseg->latch();

  UT_LIST_REMOVE(rseg->insert_undo_list, undo);
  undo_ptr->insert_undo = nullptr;

  if (undo->state == TRX_UNDO_CACHED) {
    UT_LIST_ADD_FIRST(rseg->insert_undo_cached, undo);

    MONITOR_INC(MONITOR_NUM_UNDO_SLOT_CACHED);
  } else {
    ut_ad(undo->state == TRX_UNDO_TO_FREE);

    /* Delete first the undo log segment in the file */

    rseg->unlatch();

    DEBUG_SYNC_C("innodb_commit_wait_for_truncate");
    trx_undo_seg_free(undo, noredo);

    rseg->latch();

    trx_undo_mem_free(undo);
  }

  rseg->unlatch();
}

void trx_undo_free_trx_with_prepared_or_active_logs(trx_t *trx, bool prepared) {
  ut_a(srv_shutdown_state.load() == SRV_SHUTDOWN_EXIT_THREADS);

  if (trx->rsegs.m_redo.update_undo) {
    ut_a(trx->rsegs.m_redo.update_undo->is_prepared() == prepared);

    UT_LIST_REMOVE(trx->rsegs.m_redo.rseg->update_undo_list,
                   trx->rsegs.m_redo.update_undo);
    trx_undo_mem_free(trx->rsegs.m_redo.update_undo);

    trx->rsegs.m_redo.update_undo = nullptr;
  }

  if (trx->rsegs.m_redo.insert_undo) {
    ut_a(trx->rsegs.m_redo.insert_undo->is_prepared() == prepared);

    UT_LIST_REMOVE(trx->rsegs.m_redo.rseg->insert_undo_list,
                   trx->rsegs.m_redo.insert_undo);
    trx_undo_mem_free(trx->rsegs.m_redo.insert_undo);

    trx->rsegs.m_redo.insert_undo = nullptr;
  }

  if (trx->rsegs.m_noredo.update_undo) {
    ut_a(trx->rsegs.m_noredo.update_undo->is_prepared() == prepared);

    UT_LIST_REMOVE(trx->rsegs.m_noredo.rseg->update_undo_list,
                   trx->rsegs.m_noredo.update_undo);
    trx_undo_mem_free(trx->rsegs.m_noredo.update_undo);

    trx->rsegs.m_noredo.update_undo = nullptr;
  }
  if (trx->rsegs.m_noredo.insert_undo) {
    ut_a(trx->rsegs.m_noredo.insert_undo->is_prepared() == prepared);

    UT_LIST_REMOVE(trx->rsegs.m_noredo.rseg->insert_undo_list,
                   trx->rsegs.m_noredo.insert_undo);
    trx_undo_mem_free(trx->rsegs.m_noredo.insert_undo);

    trx->rsegs.m_noredo.insert_undo = nullptr;
  }
}

bool trx_undo_truncate_tablespace(undo::Tablespace *marked_space) {
#ifdef UNIV_DEBUG
  static undo::Inject_failure_once injector("ib_undo_trunc_fail_truncate");
  if (injector.should_fail()) {
    return (false);
  };
#endif /* UNIV_DEBUG */

  bool is_encrypted;

  auto old_space_id = marked_space->id();
  auto space_num = undo::id2num(old_space_id);
  auto marked_rsegs = marked_space->rsegs();

  undo::unuse_space_id(old_space_id);

  auto new_space_id = undo::use_next_space_id(space_num);

  auto space = fil_space_get(old_space_id);

  auto n_pages = UNDO_INITIAL_SIZE_IN_PAGES;

  /* If the default extend amount has been increased greater than the default
  and it has been less than 1 second since the last time the file was extended,
  then we consider the undo tablespace to be growing aggressively. */
  if (space != nullptr && space->m_undo_extend > UNDO_INITIAL_SIZE_IN_PAGES &&
      space->m_last_extended.elapsed() < 1000) {
    /* UNDO is being extended aggressively, don't reduce size to default. */
    n_pages = fil_space_get_size(old_space_id) / 4;
  }

  /* Step-1: Delete the old tablespace. */
  /* Tablespace might have been attempted to be deleted before. If it was
  removed, then possibly only new space creation failed, or old space file
  deletion failed.*/
  if (space != nullptr) {
    is_encrypted = FSP_FLAGS_GET_ENCRYPTION(space->flags);

    if (fil_delete_tablespace(old_space_id, BUF_REMOVE_NONE) != DB_SUCCESS) {
      return false;
    }
  } else {
    /* For example on Windows the file deletion can fail if the file
    is being used. Just try again to remove it if it still exists. */
    os_file_delete_if_exists(innodb_data_file_key, marked_space->file_name(),
                             nullptr);

    /* We don't know if the undo was encrypted or not, just use the
    srv_undo_log_encrypt value. */
    is_encrypted = true;
  }

  /* Step-2: Re-create tablespace with new file. */
  ulint flags = fsp_flags_init(univ_page_size, false, false, false, false);

  /* Create the new UNDO tablespace. */
  if (fil_ibd_create(new_space_id, marked_space->space_name(),
                     marked_space->file_name(), flags, n_pages) != DB_SUCCESS) {
    return false;
  }

  ut_d(undo::inject_crash("ib_undo_trunc_empty_file"));

  /* This undo tablespace is unused. Lock the Rsegs before the
  file_space because SYNC_RSEGS > SYNC_FSP. */
  marked_rsegs->x_lock();

  /* Step-3: Re-initialize tablespace header. */
  log_free_check();

  mtr_t mtr;

  mtr.start();

  fsp_header_init(new_space_id, n_pages, &mtr);

  /* If tablespace is to be encrypted, encrypt it now */
  if (is_encrypted && srv_undo_log_encrypt) {
    ut_d(bool ret =) set_undo_tablespace_encryption(new_space_id, &mtr);
    /* Don't expect any error here (unless keyring plugin is uninstalled). In
    that case too, continue truncation processing of tablespace. */
    ut_ad(!ret);
  }

  /* Step-4: Add the RSEG_ARRAY page. */
  trx_rseg_array_create(new_space_id, &mtr);

  mtr.commit();

  /* Step-5: Add rollback segment header pages.
  This is different from trx_rseg_add_rollback_segments() in that the
  undo::Tablespace::m_rsegs already exist and we are assigning a new
  space_id to each rseg as we create the rseg header page. */

  ut_d(undo::inject_crash("ib_undo_trunc_before_rsegs"));

  for (auto rseg : *marked_rsegs) {
    log_free_check();

    mtr.start();

    mtr_x_lock(fil_space_get_latch(new_space_id), &mtr, UT_LOCATION_HERE);

    rseg->space_id = new_space_id;

    rseg->page_no = trx_rseg_header_create(new_space_id, univ_page_size,
                                           PAGE_NO_MAX, rseg->id, &mtr);

    ut_a(rseg->page_no != FIL_NULL);

    auto rseg_header =
        trx_rsegf_get_new(new_space_id, rseg->page_no, rseg->page_size, &mtr);

    /* Before re-initialization ensure that we free the existing
    structure. There can't be any active transactions. */
    ut_a(UT_LIST_GET_LEN(rseg->update_undo_list) == 0);
    ut_a(UT_LIST_GET_LEN(rseg->insert_undo_list) == 0);

    for (auto undo : rseg->update_undo_cached.removable()) {
      UT_LIST_REMOVE(rseg->update_undo_cached, undo);
      MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_CACHED);
      trx_undo_mem_free(undo);
    }

    for (auto undo : rseg->insert_undo_cached.removable()) {
      UT_LIST_REMOVE(rseg->insert_undo_cached, undo);
      MONITOR_DEC(MONITOR_NUM_UNDO_SLOT_CACHED);
      trx_undo_mem_free(undo);
    }

    rseg->update_undo_list.clear();
    rseg->update_undo_cached.clear();
    rseg->insert_undo_list.clear();
    rseg->insert_undo_cached.clear();

    rseg->max_size =
        mtr_read_ulint(rseg_header + TRX_RSEG_MAX_SIZE, MLOG_4BYTES, &mtr);

    /* Initialize the undo log lists according to the rseg header */
    rseg->set_curr_size(
        mtr_read_ulint(rseg_header + TRX_RSEG_HISTORY_SIZE, MLOG_4BYTES, &mtr) +
        1);

    mtr.commit();

    ut_ad(rseg->get_curr_size() == 1);
    ut_ad(rseg->trx_ref_count == 0);

    rseg->last_page_no = FIL_NULL;
    rseg->last_offset = 0;
    rseg->last_trx_no = 0;
    rseg->last_del_marks = false;
  }

  marked_rsegs->x_unlock();

  /* Increment the space ID for this undo space now so that if anyone refers
  to this space, it is completely initialized. */
  marked_space->set_space_id(new_space_id);

  /* Set the amount in which an undo tablespace grows. */
  fil_space_set_undo_size(new_space_id, true);

  return true;
}

#endif /* !UNIV_HOTBACKUP */
