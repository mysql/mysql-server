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

/** @file include/trx0rseg.h
 Rollback segment

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#ifndef trx0rseg_h
#define trx0rseg_h

#include <vector>
#include "fut0lst.h"
#include "trx0sys.h"
#include "trx0types.h"
#include "univ.i"

/** Gets a rollback segment header.
@param[in]	space		space where placed
@param[in]	page_no		page number of the header
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return rollback segment header, page x-latched */
UNIV_INLINE
trx_rsegf_t *trx_rsegf_get(space_id_t space, page_no_t page_no,
                           const page_size_t &page_size, mtr_t *mtr);

/** Gets a newly created rollback segment header.
@param[in]	space		space where placed
@param[in]	page_no		page number of the header
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return rollback segment header, page x-latched */
UNIV_INLINE
trx_rsegf_t *trx_rsegf_get_new(space_id_t space, page_no_t page_no,
                               const page_size_t &page_size, mtr_t *mtr);

/** Gets the file page number of the nth undo log slot.
@param[in]	rsegf	rollback segment header
@param[in]	n	index of slot
@param[in]	mtr	mtr
@return page number of the undo log segment */
UNIV_INLINE
page_no_t trx_rsegf_get_nth_undo(trx_rsegf_t *rsegf, ulint n, mtr_t *mtr);

/** Sets the file page number of the nth undo log slot.
@param[in]	rsegf	rollback segment header
@param[in]	n	index of slot
@param[in]	page_no	page number of the undo log segment
@param[in]	mtr	mtr */
UNIV_INLINE
void trx_rsegf_set_nth_undo(trx_rsegf_t *rsegf, ulint n, page_no_t page_no,
                            mtr_t *mtr);

/** Looks for a free slot for an undo log segment.
@param[in]	rsegf	rollback segment header
@param[in]	mtr	mtr
@return slot index or ULINT_UNDEFINED if not found */
UNIV_INLINE
ulint trx_rsegf_undo_find_free(trx_rsegf_t *rsegf, mtr_t *mtr);

/** Creates a rollback segment header.
This function is called only when a new rollback segment is created in
the database.
@param[in]	space_id		space id
@param[in]	page_size	page size
@param[in]	max_size	max size in pages
@param[in]	rseg_slot	rseg id == slot number in trx sys
@param[in,out]	mtr		mini-transaction
@return page number of the created segment, FIL_NULL if fail */
page_no_t trx_rseg_header_create(space_id_t space_id,
                                 const page_size_t &page_size,
                                 page_no_t max_size, ulint rseg_slot,
                                 mtr_t *mtr);

/** Add more rsegs to the rseg list in each tablespace until there are
srv_rollback_segments of them.  Use any rollback segment that already
exists so that the purge_queue can be filled and processed with any
existing undo log. If the rollback segments do not exist in this
tablespace and we need them according to target_rollback_segments,
then build them in the tablespace.
@param[in]	target_rollback_segments	new number of rollback
                                                segments per space
@return true if all necessary rollback segments and trx_rseg_t objects
were created. */
bool trx_rseg_adjust_rollback_segments(ulong target_rollback_segments);

/** Create the requested number of Rollback Segments in a newly created undo
tablespace and add them to the Rsegs object.
@param[in]  space_id                  undo tablespace ID
@param[in]  target_rollback_segments  number of rollback segments per space
@return true if all necessary rollback segments and trx_rseg_t objects
were created. */
bool trx_rseg_init_rollback_segments(space_id_t space_id,
                                     ulong target_rollback_segments);

/** Create the memory copies for rollback segments and initialize the
rseg array in trx_sys at a database startup.
@param[in]	purge_queue	queue of rsegs to purge */
void trx_rsegs_init(purge_pq_t *purge_queue);

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
                                purge_pq_t *purge_queue, mtr_t *mtr);

/** Create a rollback segment in the given tablespace. This could be either
the system tablespace, the temporary tablespace, or an undo tablespace.
@param[in]	space_id	tablespace to get the rollback segment
@param[in]	rseg_id		slot number of the rseg within this tablespace
@return page number of the rollback segment header page created */
page_no_t trx_rseg_create(space_id_t space_id, ulint rseg_id);

/** Build a list of unique undo tablespaces found in the TRX_SYS page.
Do not count the system tablespace. The vector will be sorted on space id.
@param[in,out]	spaces_to_open		list of undo tablespaces found. */
void trx_rseg_get_n_undo_tablespaces(Space_Ids *spaces_to_open);

/** Upgrade the TRX_SYS page so that it no longer tracks rsegs in undo
tablespaces other than the system tablespace.  Add these tablespaces to
undo::spaces and put FIL_NULL in the slots in TRX_SYS.*/
void trx_rseg_upgrade_undo_tablespaces();

/** Create the file page for the rollback segment directory in an undo
tablespace. This function is called just after an undo tablespace is
created so the next page created here should by FSP_FSEG_DIR_PAGE_NUM.
@param[in]	space_id	Undo Tablespace ID
@param[in]	mtr		mtr */
void trx_rseg_array_create(space_id_t space_id, mtr_t *mtr);

/** Sets the page number of the nth rollback segment slot in the
independent undo tablespace.
@param[in]	rsegs_header	rollback segment array page header
@param[in]	slot		slot number on page  == rseg id
@param[in]	page_no		rollback regment header page number
@param[in]	mtr		mtr */
UNIV_INLINE
void trx_rsegsf_set_page_no(trx_rsegsf_t *rsegs_header, ulint slot,
                            page_no_t page_no, mtr_t *mtr);

/* Number of undo log slots in a rollback segment file copy */
#define TRX_RSEG_N_SLOTS (UNIV_PAGE_SIZE / 16)

/* Maximum number of transactions supported by a single rollback segment */
#define TRX_RSEG_MAX_N_TRXS (TRX_RSEG_N_SLOTS / 2)

/* Undo log segment slot in a rollback segment header */
/*-------------------------------------------------------------*/
#define TRX_RSEG_SLOT_PAGE_NO            \
  0 /* Page number of the header page of \
    an undo log segment */
/*-------------------------------------------------------------*/
/* Slot size */
#define TRX_RSEG_SLOT_SIZE 4

/* The offset of the rollback segment header on its page */
#define TRX_RSEG FSEG_PAGE_DATA

/* Transaction rollback segment header */
/*-------------------------------------------------------------*/
#define TRX_RSEG_MAX_SIZE                \
  0 /* Maximum allowed size for rollback \
    segment in pages */
#define TRX_RSEG_HISTORY_SIZE        \
  4 /* Number of file pages occupied \
    by the logs in the history list */
#define TRX_RSEG_HISTORY                  \
  8 /* The update undo logs for committed \
    transactions */
#define TRX_RSEG_FSEG_HEADER (8 + FLST_BASE_NODE_SIZE)
/* Header for the file segment where
this page is placed */
#define TRX_RSEG_UNDO_SLOTS (8 + FLST_BASE_NODE_SIZE + FSEG_HEADER_SIZE)
/* Undo log segment slots */
/*-------------------------------------------------------------*/

/** The offset of the Rollback Segment Directory header on an RSEG_ARRAY page */
#define RSEG_ARRAY_HEADER FSEG_PAGE_DATA

/** Rollback Segment Array Header */
/*------------------------------------------------------------- */
/** The RSEG ARRAY base version is a number derived from the string
'RSEG' [0x 52 53 45 47] for extra validation. Each new version
increments the base version by 1. */
#define RSEG_ARRAY_VERSION 0x52534547 + 1

/** The RSEG ARRAY version offset in the header. */
#define RSEG_ARRAY_VERSION_OFFSET 0

/** The current number of rollback segments being tracked in this array */
#define RSEG_ARRAY_SIZE_OFFSET 4

/** This is the pointer to the file segment inode that tracks this
rseg array page. */
#define RSEG_ARRAY_FSEG_HEADER_OFFSET 8

/** The start of the array of rollback segment header page numbers for this
undo tablespace. The potential size of this array is limited only by the
page size minus overhead. The actual size of the array is limited by
srv_rollback_segments. */
#define RSEG_ARRAY_PAGES_OFFSET (8 + FSEG_HEADER_SIZE)

/** Reserved space at the end of an RSEG_ARRAY page reserved for future use. */
#define RSEG_ARRAY_RESERVED_BYTES 200

/* Slot size of the array of rollback segment header page numbers */
#define RSEG_ARRAY_SLOT_SIZE 4
/*------------------------------------------------------------- */

#include "trx0rseg.ic"

#endif
