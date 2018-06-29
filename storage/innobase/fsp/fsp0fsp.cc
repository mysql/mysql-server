/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file fsp/fsp0fsp.cc
 File space management

 Created 11/29/1995 Heikki Tuuri
 ***********************************************************************/

#include "fsp0fsp.h"
#include "buf0buf.h"
#include "fil0fil.h"
#include "ha_prototypes.h"
#include "mtr0log.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "page0page.h"
#include "page0zip.h"
#include "ut0byte.h"
#ifdef UNIV_HOTBACKUP
#include "fut0lst.h"
#endif /* UNIV_HOTBACKUP */
#include <my_aes.h>

#ifndef UNIV_HOTBACKUP
#include <debug_sync.h>
#include "btr0btr.h"
#include "btr0sea.h"
#include "dict0boot.h"
#include "dict0dd.h"
#include "fut0fut.h"
#include "ibuf0ibuf.h"
#include "log0log.h"
#include "srv0srv.h"
#endif /* !UNIV_HOTBACKUP */
#include "dict0mem.h"
#include "fsp0sysspace.h"
#include "srv0start.h"
#include "trx0purge.h"

#ifndef UNIV_HOTBACKUP

#include "dd/types/tablespace.h"
#include "dict0dd.h"
#include "sql_backup_lock.h"
#include "sql_thd_internal_api.h"
#include "thd_raii.h"
#include "transaction.h"
#include "ut0stage.h"

/** DDL records for tablespace (un)encryption. */
std::vector<DDL_Record *> ts_encrypt_ddl_records;

/** Group of pages to be marked dirty together during (un)encryption. */
#define PAGE_GROUP_SIZE 1

/** Returns an extent to the free list of a space.
@param[in]	page_id		page id in the extent
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction */
static void fsp_free_extent(const page_id_t &page_id,
                            const page_size_t &page_size, mtr_t *mtr);

/** Determine if extent belongs to a given segment.
@param[in]	descr	extent descriptor
@param[in]	seg_id	segment identifier
@param[in]	mtr	mini-transaction
@return	true if extent is part of the segment, false otherwise */
static bool xdes_in_segment(const xdes_t *descr, ib_id_t seg_id, mtr_t *mtr);

/** Marks a page used. The page must reside within the extents of the given
 segment. */
static void fseg_mark_page_used(
    fseg_inode_t *seg_inode, /*!< in: segment inode */
    page_no_t page,          /*!< in: page offset */
    xdes_t *descr,           /*!< in: extent descriptor */
    mtr_t *mtr);             /*!< in/out: mini-transaction */

/** Returns the first extent descriptor for a segment.
We think of the extent lists of the segment catenated in the order
FSEG_FULL -> FSEG_NOT_FULL -> FSEG_FREE.
@param[in]	inode		segment inode
@param[in]	space_id	space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return the first extent descriptor, or NULL if none */
static xdes_t *fseg_get_first_extent(fseg_inode_t *inode, space_id_t space_id,
                                     const page_size_t &page_size, mtr_t *mtr);

/** Put new extents to the free list if there are free extents above the free
limit. If an extent happens to contain an extent descriptor page, the extent
is put to the FSP_FREE_FRAG list with the page marked as used.
@param[in]	init_space	true if this is a single-table tablespace
and we are only initializing the first extent and the first bitmap pages;
then we will not allocate more extents
@param[in,out]	space		tablespace
@param[in,out]	header		tablespace header
@param[in,out]	mtr		mini-transaction */
static UNIV_COLD void fsp_fill_free_list(bool init_space, fil_space_t *space,
                                         fsp_header_t *header, mtr_t *mtr);

/** Allocates a single free page from a segment.
This function implements the intelligent allocation strategy which tries
to minimize file space fragmentation.
@param[in,out]	space			tablespace
@param[in]	page_size		page size
@param[in,out]	seg_inode		segment inode
@param[in]	hint			hint of which page would be desirable
@param[in]	direction		if the new page is needed because of
an index page split, and records are inserted there in order, into which
direction they go alphabetically: FSP_DOWN, FSP_UP, FSP_NO_DIR
@param[in]	rw_latch		RW_SX_LATCH, RW_X_LATCH
@param[in,out]	mtr			mini-transaction
@param[in,out]	init_mtr		mtr or another mini-transaction in
which the page should be initialized. If init_mtr != mtr, but the page is
already latched in mtr, do not initialize the page
@param[in]	has_done_reservation	TRUE if the space has already been
reserved, in this case we will never return NULL
@retval NULL	if no page could be allocated
@retval block	rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block	(not allocated or initialized) otherwise */
static buf_block_t *fseg_alloc_free_page_low(
    fil_space_t *space, const page_size_t &page_size, fseg_inode_t *seg_inode,
    page_no_t hint, byte direction, rw_lock_type_t rw_latch, mtr_t *mtr,
    mtr_t *init_mtr
#ifdef UNIV_DEBUG
    ,
    ibool has_done_reservation
#endif /* UNIV_DEBUG */
    ) MY_ATTRIBUTE((warn_unused_result));
#endif /* !UNIV_HOTBACKUP */

/** Get the segment identifier to which the extent belongs to.
@param[in]	descr	extent descriptor
@return	the segment identifier */
inline ib_id_t xdes_get_segment_id(const xdes_t *descr) {
  return (mach_read_from_8(descr + XDES_ID));
}

/** Get the segment identifier to which the extent belongs to.
@param[in]	descr	extent descriptor
@param[in]	mtr	mini-transaction
@return	the segment identifier */
inline ib_id_t xdes_get_segment_id(const xdes_t *descr, mtr_t *mtr) {
#ifndef UNIV_HOTBACKUP
  ut_ad(mtr_memo_contains_page_flagged(
      mtr, descr,
      MTR_MEMO_PAGE_S_FIX | MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));
#endif /* !UNIV_HOTBACKUP */

  return (xdes_get_segment_id(descr));
}

#ifndef UNIV_HOTBACKUP
/** Gets a pointer to the space header and x-locks its page.
@param[in]	id		space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return pointer to the space header, page x-locked */
fsp_header_t *fsp_get_space_header(space_id_t id, const page_size_t &page_size,
                                   mtr_t *mtr) {
  buf_block_t *block;
  fsp_header_t *header;

  ut_ad(id != 0 || !page_size.is_compressed());

  block = buf_page_get(page_id_t(id, 0), page_size, RW_SX_LATCH, mtr);
  header = FSP_HEADER_OFFSET + buf_block_get_frame(block);
  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

  ut_ad(id == mach_read_from_4(FSP_SPACE_ID + header));
#ifdef UNIV_DEBUG
  const ulint flags = mach_read_from_4(FSP_SPACE_FLAGS + header);
  ut_ad(page_size_t(flags).equals_to(page_size));
#endif /* UNIV_DEBUG */
  return (header);
}

/** Convert a 32 bit integer tablespace flags to the 32 bit table flags.
This can only be done for a tablespace that was built as a file-per-table
tablespace. Note that the fsp_flags cannot show the difference between a
Compact and Redundant table, so an extra Compact boolean must be supplied.
                        Low order bit
                    | REDUNDANT | COMPACT | COMPRESSED | DYNAMIC
fil_space_t::flags  |     0     |    0    |     1      |    1
dict_table_t::flags |     0     |    1    |     1      |    1
@param[in]	fsp_flags	fil_space_t::flags
@param[in]	compact		true if not Redundant row format
@return tablespace flags (fil_space_t::flags) */
ulint fsp_flags_to_dict_tf(ulint fsp_flags, bool compact) {
  /* If the table in this file-per-table tablespace is Compact
  row format, the low order bit will not indicate Compact. */
  bool post_antelope = FSP_FLAGS_GET_POST_ANTELOPE(fsp_flags);
  ulint zip_ssize = FSP_FLAGS_GET_ZIP_SSIZE(fsp_flags);
  bool atomic_blobs = FSP_FLAGS_HAS_ATOMIC_BLOBS(fsp_flags);
  bool data_dir = FSP_FLAGS_HAS_DATA_DIR(fsp_flags);
  bool shared_space = FSP_FLAGS_GET_SHARED(fsp_flags);
  /* FSP_FLAGS_GET_TEMPORARY(fsp_flags) does not have an equivalent
  flag position in the table flags. But it would go into flags2 if
  any code is created where that is needed. */

  ulint flags = dict_tf_init(post_antelope | compact, zip_ssize, atomic_blobs,
                             data_dir, shared_space);

  return (flags);
}
#endif /* !UNIV_HOTBACKUP */

/** Check whether a space id is an undo tablespace ID
Undo tablespaces have space_id's starting 1 less than the redo logs.
They are numbered down from this.  Since rseg_id=0 always refers to the
system tablespace, undo_space_num values start at 1.  The current limit
is 127. The translation from an undo_space_num is:
   undo space_id = log_first_space_id - undo_space_num
@param[in]	space_id	space id to check
@return true if it is undo tablespace else false. */
bool fsp_is_undo_tablespace(space_id_t space_id) {
  /* Starting with v8, undo space_ids have a unique range. */
  if (space_id >= dict_sys_t::s_min_undo_space_id &&
      space_id <= dict_sys_t::s_max_undo_space_id) {
    return (true);
  }

  /* If upgrading from 5.7, there may be a list of old-style
  undo tablespaces.  Search them. */
  if (trx_sys_undo_spaces != nullptr) {
    return (trx_sys_undo_spaces->contains(space_id));
  }

  return (false);
}

/** Check if tablespace is global temporary.
@param[in]	space_id	tablespace ID
@return true if tablespace is global temporary. */
bool fsp_is_global_temporary(space_id_t space_id) {
  return (space_id == srv_tmp_space.space_id());
}

/** Check if the tablespace is session temporary.
@param[in]      space_id        tablespace ID
@return true if tablespace is a session temporary tablespace. */
bool fsp_is_session_temporary(space_id_t space_id) {
  return (space_id > dict_sys_t::s_min_temp_space_id &&
          space_id <= dict_sys_t::s_max_temp_space_id);
}

/** Check if tablespace is system temporary.
@param[in]	space_id	tablespace ID
@return true if tablespace is system temporary. */
bool fsp_is_system_temporary(space_id_t space_id) {
  return (fsp_is_global_temporary(space_id) ||
          fsp_is_session_temporary(space_id));
}

/** Check if checksum is disabled for the given space.
@param[in]	space_id	tablespace ID
@return true if checksum is disabled for given space. */
bool fsp_is_checksum_disabled(space_id_t space_id) {
  return (fsp_is_system_temporary(space_id));
}

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG

/** Skip some of the sanity checks that are time consuming even in debug mode
and can affect frequent verification runs that are done to ensure stability of
the product.
@return true if check should be skipped for given space. */
bool fsp_skip_sanity_check(space_id_t space_id) {
  return (srv_skip_temp_table_checks_debug &&
          fsp_is_system_temporary(space_id));
}

#endif /* UNIV_DEBUG */

/** Gets a descriptor bit of a page.
 @return true if free */
UNIV_INLINE
ibool xdes_mtr_get_bit(const xdes_t *descr, /*!< in: descriptor */
                       ulint bit, /*!< in: XDES_FREE_BIT or XDES_CLEAN_BIT */
                       page_no_t offset, /*!< in: page offset within extent:
                                         0 ... FSP_EXTENT_SIZE - 1 */
                       mtr_t *mtr)       /*!< in: mini-transaction */
{
  ut_ad(mtr->is_active());
  ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));

  return (xdes_get_bit(descr, bit, offset));
}

/** Sets a descriptor bit of a page. */
UNIV_INLINE
void xdes_set_bit(xdes_t *descr,    /*!< in: descriptor */
                  ulint bit,        /*!< in: XDES_FREE_BIT or XDES_CLEAN_BIT */
                  page_no_t offset, /*!< in: page offset within extent:
                                    0 ... FSP_EXTENT_SIZE - 1 */
                  ibool val,        /*!< in: bit value */
                  mtr_t *mtr)       /*!< in/out: mini-transaction */
{
  ulint index;
  ulint byte_index;
  ulint bit_index;
  ulint descr_byte;

  ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));
  ut_ad((bit == XDES_FREE_BIT) || (bit == XDES_CLEAN_BIT));
  ut_ad(offset < FSP_EXTENT_SIZE);

  index = bit + XDES_BITS_PER_PAGE * offset;

  byte_index = index / 8;
  bit_index = index % 8;

  descr_byte = mach_read_from_1(descr + XDES_BITMAP + byte_index);
  descr_byte = ut_bit_set_nth(descr_byte, bit_index, val);

  mlog_write_ulint(descr + XDES_BITMAP + byte_index, descr_byte, MLOG_1BYTE,
                   mtr);
}

/** Looks for a descriptor bit having the desired value. Starts from hint
 and scans upward; at the end of the extent the search is wrapped to
 the start of the extent.
 @return bit index of the bit, ULINT_UNDEFINED if not found */
UNIV_INLINE
page_no_t xdes_find_bit(
    xdes_t *descr,  /*!< in: descriptor */
    ulint bit,      /*!< in: XDES_FREE_BIT or XDES_CLEAN_BIT */
    ibool val,      /*!< in: desired bit value */
    page_no_t hint, /*!< in: hint of which bit position would
                    be desirable */
    mtr_t *mtr)     /*!< in/out: mini-transaction */
{
  page_no_t i;

  ut_ad(descr && mtr);
  ut_ad(val <= TRUE);
  ut_ad(hint < FSP_EXTENT_SIZE);
  ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));
  for (i = hint; i < FSP_EXTENT_SIZE; i++) {
    if (val == xdes_mtr_get_bit(descr, bit, i, mtr)) {
      return (i);
    }
  }

  for (i = 0; i < hint; i++) {
    if (val == xdes_mtr_get_bit(descr, bit, i, mtr)) {
      return (i);
    }
  }

  return (FIL_NULL);
}

/** Returns the number of used pages in a descriptor.
 @return number of pages used */
UNIV_INLINE
page_no_t xdes_get_n_used(const xdes_t *descr, /*!< in: descriptor */
                          mtr_t *mtr)          /*!< in/out: mini-transaction */
{
  page_no_t count = 0;

  ut_ad(descr && mtr);
  ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));
  for (page_no_t i = 0; i < FSP_EXTENT_SIZE; ++i) {
    if (FALSE == xdes_mtr_get_bit(descr, XDES_FREE_BIT, i, mtr)) {
      count++;
    }
  }

  return (count);
}

#ifdef UNIV_DEBUG
/** Check if the state of extent descriptor is valid.
@param[in]	state	the extent descriptor state
@return	true if state is valid, false otherwise */
bool xdes_state_is_valid(ulint state) {
  switch (state) {
    case XDES_NOT_INITED:
    case XDES_FREE:
    case XDES_FREE_FRAG:
    case XDES_FULL_FRAG:
    case XDES_FSEG:
    case XDES_FSEG_FRAG:
      return (true);
  }
  return (false);
}
#endif /* UNIV_DEBUG */

/** Returns true if extent contains no used pages.
 @return true if totally free */
UNIV_INLINE
ibool xdes_is_free(const xdes_t *descr, /*!< in: descriptor */
                   mtr_t *mtr)          /*!< in/out: mini-transaction */
{
  if (0 == xdes_get_n_used(descr, mtr)) {
    ut_ad(xdes_get_state(descr, mtr) != XDES_FSEG_FRAG);

    return (TRUE);
  }

  return (FALSE);
}

/** Returns true if extent contains no free pages.
 @return true if full */
UNIV_INLINE
ibool xdes_is_full(const xdes_t *descr, /*!< in: descriptor */
                   mtr_t *mtr)          /*!< in/out: mini-transaction */
{
  if (FSP_EXTENT_SIZE == xdes_get_n_used(descr, mtr)) {
    return (TRUE);
  }

  return (FALSE);
}

/** Sets the state of an xdes. */
UNIV_INLINE
void xdes_set_state(xdes_t *descr,      /*!< in/out: descriptor */
                    xdes_state_t state, /*!< in: state to set */
                    mtr_t *mtr)         /*!< in/out: mini-transaction */
{
  ut_ad(descr && mtr);
  ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));

#ifdef UNIV_DEBUG
  switch (xdes_get_state(descr, mtr)) {
    case XDES_FREE:
      ut_ad(state == XDES_FSEG || state == XDES_FREE_FRAG);
      break;
    case XDES_FREE_FRAG:
      ut_ad(state == XDES_FULL_FRAG || state == XDES_FSEG_FRAG ||
            state == XDES_FREE);
      break;
    case XDES_FULL_FRAG:
      ut_ad(state == XDES_FREE_FRAG);
      break;
    case XDES_FSEG:
      ut_ad(state == XDES_FREE);
      break;
    case XDES_FSEG_FRAG:
      ut_ad(state == XDES_FREE_FRAG || state == XDES_FULL_FRAG ||
            state == XDES_FREE);
      break;
    case XDES_NOT_INITED:
      /* The state is not yet initialized. */
      ut_ad(state == XDES_FREE);
      break;
  }
#endif /* UNIV_DEBUG */

  mlog_write_ulint(descr + XDES_STATE, state, MLOG_4BYTES, mtr);
}

/** Update the segment identifier to which the extent belongs to.
@param[in,out]	descr	extent descriptor
@param[in,out]	seg_id	segment identifier
@param[in]	state	state of the extent.
@param[in,out]	mtr	mini-transaction. */
inline void xdes_set_segment_id(xdes_t *descr, const ib_id_t seg_id,
                                xdes_state_t state, mtr_t *mtr) {
  ut_ad(mtr != NULL);
  mlog_write_ull(descr + XDES_ID, seg_id, mtr);
  xdes_set_state(descr, state, mtr);
}

/** Inits an extent descriptor to the free and clean state. */
UNIV_INLINE
void xdes_init(xdes_t *descr, /*!< in: descriptor */
               mtr_t *mtr)    /*!< in/out: mini-transaction */
{
  ulint i;

  ut_ad(descr && mtr);
  ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));
  ut_ad((XDES_SIZE - XDES_BITMAP) % 4 == 0);

  xdes_set_segment_id(descr, 0, XDES_FREE, mtr);
  flst_write_addr(descr + XDES_FLST_NODE + FLST_PREV, fil_addr_null, mtr);
  flst_write_addr(descr + XDES_FLST_NODE + FLST_NEXT, fil_addr_null, mtr);

  for (i = XDES_BITMAP; i < XDES_SIZE; i += 4) {
    mlog_write_ulint(descr + i, 0xFFFFFFFFUL, MLOG_4BYTES, mtr);
  }
}

/** Get pointer to a the extent descriptor of a page.
@param[in,out]	sp_header	tablespace header page, x-latched
@param[in]	space		tablespace identifier
@param[in]	offset		page offset
@param[in,out]	mtr		mini-transaction
@param[in]	init_space	whether the tablespace is being initialized
@param[out]	desc_block	descriptor block, or NULL if it is
the same as the tablespace header
@return pointer to the extent descriptor, NULL if the page does not
exist in the space or if the offset exceeds free limit */
UNIV_INLINE MY_ATTRIBUTE((warn_unused_result)) xdes_t
    *xdes_get_descriptor_with_space_hdr(fsp_header_t *sp_header,
                                        space_id_t space, page_no_t offset,
                                        mtr_t *mtr, bool init_space = false,
                                        buf_block_t **desc_block = NULL) {
  ulint limit;
  ulint size;
  page_no_t descr_page_no;
  ulint flags;
  page_t *descr_page;
#ifdef UNIV_DEBUG
  const fil_space_t *fspace = fil_space_get(space);
  ut_ad(fspace != NULL);
#endif /* UNIV_DEBUG */
  ut_ad(mtr_memo_contains(mtr, &fspace->latch, MTR_MEMO_X_LOCK));
  ut_ad(mtr_memo_contains_page(mtr, sp_header, MTR_MEMO_PAGE_SX_FIX));
  ut_ad(page_offset(sp_header) == FSP_HEADER_OFFSET);
  /* Read free limit and space size */
  limit = mach_read_from_4(sp_header + FSP_FREE_LIMIT);
  size = mach_read_from_4(sp_header + FSP_SIZE);
  flags = mach_read_from_4(sp_header + FSP_SPACE_FLAGS);
  ut_ad(limit == fspace->free_limit ||
        (fspace->free_limit == 0 &&
         (init_space || fspace->purpose == FIL_TYPE_TEMPORARY ||
          (srv_startup_is_before_trx_rollback_phase &&
           fsp_is_undo_tablespace(fspace->id)))));
  ut_ad(size == fspace->size_in_header);
#ifdef UNIV_DEBUG
  /* Exclude Encryption flag as it might have been changed In Memory flags but
  not on disk. */
  ut_ad(!((flags ^ fspace->flags) & ~(FSP_FLAGS_MASK_ENCRYPTION)));
#endif /* UNIV_DEBUG */

  if ((offset >= size) || (offset >= limit)) {
    return (NULL);
  }

  const page_size_t page_size(flags);

  descr_page_no = xdes_calc_descriptor_page(page_size, offset);

  buf_block_t *block;

  if (descr_page_no == 0) {
    /* It is on the space header page */

    descr_page = page_align(sp_header);
    block = NULL;
  } else {
    block = buf_page_get(page_id_t(space, descr_page_no), page_size,
                         RW_SX_LATCH, mtr);

    buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

    descr_page = buf_block_get_frame(block);
  }

  if (desc_block != NULL) {
    *desc_block = block;
  }

  return (descr_page + XDES_ARR_OFFSET +
          XDES_SIZE * xdes_calc_descriptor_index(page_size, offset));
}

/** Gets pointer to a the extent descriptor of a page.
The page where the extent descriptor resides is x-locked. If the page offset
is equal to the free limit of the space, adds new extents from above the free
limit to the space free list, if not free limit == space size. This adding
is necessary to make the descriptor defined, as they are uninitialized
above the free limit.
@param[in]	space_id	space id
@param[in]	offset		page offset; if equal to the free limit, we
try to add new extents to the space free list
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return pointer to the extent descriptor, NULL if the page does not
exist in the space or if the offset exceeds the free limit */
static MY_ATTRIBUTE((warn_unused_result)) xdes_t *xdes_get_descriptor(
    space_id_t space_id, page_no_t offset, const page_size_t &page_size,
    mtr_t *mtr) {
  buf_block_t *block;
  fsp_header_t *sp_header;

  block = buf_page_get(page_id_t(space_id, 0), page_size, RW_SX_LATCH, mtr);

  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

  sp_header = FSP_HEADER_OFFSET + buf_block_get_frame(block);
  return (xdes_get_descriptor_with_space_hdr(sp_header, space_id, offset, mtr));
}

/** Gets pointer to a the extent descriptor if the file address of the
descriptor list node is known. The page where the extent descriptor resides
is x-locked.
@param[in]	space		space id
@param[in]	page_size	page size
@param[in]	lst_node	file address of the list node contained in the
                                descriptor
@param[in,out]	mtr		mini-transaction
@return pointer to the extent descriptor */
UNIV_INLINE
xdes_t *xdes_lst_get_descriptor(space_id_t space, const page_size_t &page_size,
                                fil_addr_t lst_node, mtr_t *mtr) {
  xdes_t *descr;

  ut_ad(mtr);
  ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space), MTR_MEMO_X_LOCK));

  descr = fut_get_ptr(space, page_size, lst_node, RW_SX_LATCH, mtr) -
          XDES_FLST_NODE;

  return (descr);
}

/** Returns page offset of the first page in extent described by a descriptor.
 @return offset of the first page in extent */
UNIV_INLINE
page_no_t xdes_get_offset(const xdes_t *descr) /*!< in: extent descriptor */
{
  ut_ad(descr);

  return (page_get_page_no(page_align(descr)) +
          static_cast<page_no_t>(
              ((page_offset(descr) - XDES_ARR_OFFSET) / XDES_SIZE) *
              FSP_EXTENT_SIZE));
}
#endif /* !UNIV_HOTBACKUP */

/** Inits a file page whose prior contents should be ignored. */
static void fsp_init_file_page_low(
    buf_block_t *block) /*!< in: pointer to a page */
{
  page_t *page = buf_block_get_frame(block);
  page_zip_des_t *page_zip = buf_block_get_page_zip(block);

  if (!fsp_is_system_temporary(block->page.id.space())) {
    memset(page, 0, UNIV_PAGE_SIZE);
  }

  mach_write_to_4(page + FIL_PAGE_OFFSET, block->page.id.page_no());
  mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
                  block->page.id.space());

  /* Reset FRAME LSN, which otherwise points to the LSN of the last
  page that used this buffer block. This is needed by CLONE for
  tracking dirty pages. */
  memset(page + FIL_PAGE_LSN, 0, 8);

  if (page_zip) {
    memset(page_zip->data, 0, page_zip_get_size(page_zip));
    memcpy(page_zip->data + FIL_PAGE_OFFSET, page + FIL_PAGE_OFFSET, 4);
    memcpy(page_zip->data + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
           page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, 4);
    memcpy(page_zip->data + FIL_PAGE_LSN, page + FIL_PAGE_LSN, 8);
  }
}

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
/** Assert that the mini-transaction is compatible with
updating an allocation bitmap page.
@param[in]	id	tablespace identifier
@param[in]	mtr	mini-transaction */
static void fsp_space_modify_check(space_id_t id, const mtr_t *mtr) {
  switch (mtr->get_log_mode()) {
    case MTR_LOG_SHORT_INSERTS:
    case MTR_LOG_NONE:
      /* These modes are only allowed within a non-bitmap page
      when there is a higher-level redo log record written. */
      break;
    case MTR_LOG_NO_REDO:
#ifdef UNIV_DEBUG
    {
      const fil_type_t type = fil_space_get_type(id);
      ut_a(fsp_is_system_temporary(id) ||
           fil_space_get_flags(id) == ULINT_UNDEFINED ||
           type == FIL_TYPE_TEMPORARY || type == FIL_TYPE_IMPORT ||
           fil_space_is_redo_skipped(id) || undo::is_inactive(id));
    }
#endif /* UNIV_DEBUG */
      return;
    case MTR_LOG_ALL:
      /* We must not write redo log for the shared temporary
      tablespace. */
      ut_ad(!fsp_is_system_temporary(id));
      /* If we write redo log, the tablespace must exist. */
      ut_ad(fil_space_get_type(id) == FIL_TYPE_TABLESPACE);
      return;
  }

  ut_ad(0);
}
#endif /* UNIV_DEBUG */

/** Initialize a file page.
@param[in,out]	block	file page
@param[in,out]	mtr	mini-transaction */
static void fsp_init_file_page(buf_block_t *block, mtr_t *mtr) {
  fsp_init_file_page_low(block);

  ut_d(fsp_space_modify_check(block->page.id.space(), mtr));
  mlog_write_initial_log_record(buf_block_get_frame(block),
                                MLOG_INIT_FILE_PAGE2, mtr);
}
#endif /* !UNIV_HOTBACKUP */

/** Parses a redo log record of a file page init.
 @return end of log record or NULL */
byte *fsp_parse_init_file_page(
    byte *ptr,                            /*!< in: buffer */
    byte *end_ptr MY_ATTRIBUTE((unused)), /*!< in: buffer end */
    buf_block_t *block)                   /*!< in: block or NULL */
{
  ut_ad(ptr != NULL);
  ut_ad(end_ptr != NULL);

  if (block) {
    fsp_init_file_page_low(block);
  }

  return (ptr);
}

/** Initializes the fsp system. */
void fsp_init() {
  /* FSP_EXTENT_SIZE must be a multiple of page & zip size */
  ut_a(UNIV_PAGE_SIZE > 0);
  ut_a(0 == (UNIV_PAGE_SIZE % FSP_EXTENT_SIZE));

  static_assert(!(UNIV_PAGE_SIZE_MAX % FSP_EXTENT_SIZE_MAX),
                "UNIV_PAGE_SIZE_MAX % FSP_EXTENT_SIZE_MAX != 0");

  static_assert(!(UNIV_ZIP_SIZE_MIN % FSP_EXTENT_SIZE_MIN),
                "UNIV_ZIP_SIZE_MIN % FSP_EXTENT_SIZE_MIN != 0");

  /* Does nothing at the moment */
}

/** Writes the space id and flags to a tablespace header.  The flags contain
 row type, physical/compressed page size, and logical/uncompressed page
 size of the tablespace. */
void fsp_header_init_fields(
    page_t *page,        /*!< in/out: first page in the space */
    space_id_t space_id, /*!< in: space id */
    ulint flags)         /*!< in: tablespace flags
                         (FSP_SPACE_FLAGS) */
{
  ut_a(fsp_flags_is_valid(flags));

  mach_write_to_4(FSP_HEADER_OFFSET + FSP_SPACE_ID + page, space_id);
  mach_write_to_4(FSP_HEADER_OFFSET + FSP_SPACE_FLAGS + page, flags);
}

/** Get the offset of encrytion information in page 0.
@param[in]	page_size	page size.
@return	offset on success, otherwise 0. */
ulint fsp_header_get_encryption_offset(const page_size_t &page_size) {
  ulint offset;
#ifdef UNIV_DEBUG
  ulint left_size;
#endif

  offset = XDES_ARR_OFFSET + XDES_SIZE * xdes_arr_size(page_size);
#ifdef UNIV_DEBUG
  left_size =
      page_size.physical() - FSP_HEADER_OFFSET - offset - FIL_PAGE_DATA_END;

  ut_ad(left_size >= ENCRYPTION_INFO_SIZE);
#endif

  return offset;
}

#ifndef UNIV_HOTBACKUP
/** Write the (un)encryption progress info into the space header.
@param[in]      space_id		tablespace id
@param[in]      space_flags		tablespace flags
@param[in]      progress_info		max pages (un)encrypted
@param[in]      operation_type		Type of operation
@param[in]      update_operation_type   is operation to be updated
@param[in,out]	mtr			mini-transaction
@return true if success. */
bool fsp_header_write_encryption_progress(
    space_id_t space_id, ulint space_flags, ulint progress_info,
    byte operation_type, bool update_operation_type, mtr_t *mtr) {
  buf_block_t *block;
  ulint offset;

  const page_size_t page_size(space_flags);

  /* Save the encryption info to the page 0. */
  block = buf_page_get(page_id_t(space_id, 0), page_size, RW_SX_LATCH, mtr);

  if (block == nullptr) {
    return false;
  }

  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);
  ut_ad(space_id == page_get_space_id(buf_block_get_frame(block)));

  offset = fsp_header_get_encryption_progress_offset(page_size);
  ut_ad(offset != 0 && offset < UNIV_PAGE_SIZE);

  page_t *page = buf_block_get_frame(block);

  /* Write operation type if needed */
  if (update_operation_type) {
    mlog_write_ulint(page + offset, operation_type, MLOG_1BYTE, mtr);
  }

  mlog_write_ulint(page + offset + ENCRYPTION_OPERATION_INFO_SIZE,
                   progress_info, MLOG_4BYTES, mtr);
  return (true);
}

/** Get encryption operation type in progress from the first
page of a tablespace.
@param[in]	page		first page of a tablespace
@param[in]	page_size	tablespace page size
@return encryption operation
*/
encryption_op_type fsp_header_encryption_op_type_in_progress(
    const page_t *page, page_size_t page_size) {
  ulint offset;
  encryption_op_type op;
  offset = fsp_header_get_encryption_progress_offset(page_size);
  ut_ad(offset != 0 && offset < UNIV_PAGE_SIZE);

  /* Read operation type (1 byte) */
  byte operation = mach_read_from_1(page + offset);
  switch (operation) {
    case ENCRYPTION_IN_PROGRESS:
      op = ENCRYPTION;
      break;
    case UNENCRYPTION_IN_PROGRESS:
      op = UNENCRYPTION;
      break;
    default:
      op = NONE;
      break;
  }

  return (op);
}

/** Get encryption information from page 0 of tablespace.
@param[in]	space_id	space id.
@param[in]	space_flags	space flags
@param[out]	key		tablespace encryption key
@param[out]	iv		tablespace encryption iv
@return true, if information read successfully
*/
static bool fsp_header_read_encryption_info(space_id_t space_id,
                                            ulint space_flags, byte *key,
                                            byte *iv) {
  buf_block_t *block;
  ulint offset;
  page_t *page;
  mtr_t mtr;

  const page_size_t page_size(space_flags);

  mtr_start(&mtr);
  /* Read encryption info from page 0. */
  block = buf_page_get(page_id_t(space_id, 0), page_size, RW_SX_LATCH, &mtr);
  if (block == nullptr) {
    return (false);
  }

  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);
  ut_ad(space_id == page_get_space_id(buf_block_get_frame(block)));

  offset = fsp_header_get_encryption_offset(page_size);
  ut_ad(offset != 0 && offset < UNIV_PAGE_SIZE);

  page = buf_block_get_frame(block);

  if (!Encryption::decode_encryption_info(key, iv, page + offset, false)) {
    mtr_commit(&mtr);
    return (false);
  }

  mtr_commit(&mtr);
  byte buf[ENCRYPTION_KEY_LEN];
  memset(buf, 0, ENCRYPTION_KEY_LEN);
  if (memcmp(key, buf, ENCRYPTION_KEY_LEN) == 0) {
    return (false);
  }

  return (true);
}

/** Write the encryption info into the space header.
@param[in]      space_id		tablespace id
@param[in]      space_flags		tablespace flags
@param[in]      encrypt_info		buffer for re-encrypt key
@param[in]      update_fsp_flags	if it need to update the space flags
@param[in]      rotate_encryption	if it is called during key rotation
@param[in,out]	mtr			mini-transaction
@return true if success. */
bool fsp_header_write_encryption(space_id_t space_id, ulint space_flags,
                                 byte *encrypt_info, bool update_fsp_flags,
                                 bool rotate_encryption, mtr_t *mtr) {
  buf_block_t *block;
  ulint offset;
  page_t *page;
  uint32_t master_key_id;

  const page_size_t page_size(space_flags);

  /* Save the encryption info to the page 0. */
  block = buf_page_get(page_id_t(space_id, 0), page_size, RW_SX_LATCH, mtr);
  if (block == nullptr) {
    return (false);
  }

  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);
  ut_ad(space_id == page_get_space_id(buf_block_get_frame(block)));

  offset = fsp_header_get_encryption_offset(page_size);
  ut_ad(offset != 0 && offset < UNIV_PAGE_SIZE);

  page = buf_block_get_frame(block);

  /* Write the new fsp flags into be update to the header if needed */
  if (update_fsp_flags) {
    mlog_write_ulint(page + FSP_HEADER_OFFSET + FSP_SPACE_FLAGS, space_flags,
                     MLOG_4BYTES, mtr);
  }

  if (rotate_encryption) {
    /* If is in recovering, skip all master key id is rotated
    tablespaces. */
    master_key_id = mach_read_from_4(page + offset + ENCRYPTION_MAGIC_SIZE);
    if (master_key_id == Encryption::s_master_key_id) {
      ut_ad(memcmp(page + offset, ENCRYPTION_KEY_MAGIC_V1,
                   ENCRYPTION_MAGIC_SIZE) == 0 ||
            memcmp(page + offset, ENCRYPTION_KEY_MAGIC_V2,
                   ENCRYPTION_MAGIC_SIZE) == 0 ||
            memcmp(page + offset, ENCRYPTION_KEY_MAGIC_V3,
                   ENCRYPTION_MAGIC_SIZE) == 0);
      return (true);
    }
  }

  /* For user tablespace, don't erase encryption informtaion from page 0 */
  if (fsp_is_ibd_tablespace(space_id)) {
    byte buf[ENCRYPTION_INFO_SIZE];
    memset(buf, 0, ENCRYPTION_INFO_SIZE);
    if (memcmp(encrypt_info, buf, ENCRYPTION_INFO_SIZE) != 0) {
      mlog_write_string(page + offset, encrypt_info, ENCRYPTION_INFO_SIZE, mtr);
    }
  } else {
    mlog_write_string(page + offset, encrypt_info, ENCRYPTION_INFO_SIZE, mtr);
  }

  return (true);
}

/** Rotate the encryption info in the space header.
@param[in]	space		tablespace
@param[in]      encrypt_info	buffer for re-encrypt key.
@param[in,out]	mtr		mini-transaction
@return true if success. */
bool fsp_header_rotate_encryption(fil_space_t *space, byte *encrypt_info,
                                  mtr_t *mtr) {
  ut_ad(mtr);
  ut_ad(space->encryption_type != Encryption::NONE);

  DBUG_EXECUTE_IF("fsp_header_rotate_encryption_failure", return (false););

  /* Fill encryption info. */
  if (!Encryption::fill_encryption_info(
          space->encryption_key, space->encryption_iv, encrypt_info, false)) {
    return (false);
  }

  /* Write encryption info into space header. */
  return (fsp_header_write_encryption(space->id, space->flags, encrypt_info,
                                      false, true, mtr));
}

/** Initializes the space header of a new created space and creates also the
insert buffer tree root if space == 0.
@param[in]	space_id	space id
@param[in]	size		current size in blocks
@param[in,out]	mtr		min-transaction
@param[in]	is_boot		if it's for bootstrap
@return	true on success, otherwise false. */
bool fsp_header_init(space_id_t space_id, page_no_t size, mtr_t *mtr,
                     bool is_boot) {
  fsp_header_t *header;
  buf_block_t *block;
  page_t *page;

  ut_ad(mtr);

  fil_space_t *space = fil_space_get(space_id);

  mtr_x_lock_space(space, mtr);

  const page_id_t page_id(space_id, 0);
  const page_size_t page_size(space->flags);

  block = buf_page_create(page_id, page_size, RW_SX_LATCH, mtr);
  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

  space->size_in_header = size;
  space->free_len = 0;
  space->free_limit = 0;

  /* The prior contents of the file page should be ignored */

  fsp_init_file_page(block, mtr);
  page = buf_block_get_frame(block);

  mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_TYPE_FSP_HDR, MLOG_2BYTES,
                   mtr);

  mlog_write_ulint(page + FIL_PAGE_SRV_VERSION, DD_SPACE_CURRENT_SRV_VERSION,
                   MLOG_4BYTES, mtr);
  mlog_write_ulint(page + FIL_PAGE_SPACE_VERSION,
                   DD_SPACE_CURRENT_SPACE_VERSION, MLOG_4BYTES, mtr);

  header = FSP_HEADER_OFFSET + page;

  mlog_write_ulint(header + FSP_SPACE_ID, space_id, MLOG_4BYTES, mtr);
  mlog_write_ulint(header + FSP_NOT_USED, 0, MLOG_4BYTES, mtr);

  fsp_header_size_update(header, size, mtr);
  mlog_write_ulint(header + FSP_FREE_LIMIT, 0, MLOG_4BYTES, mtr);
  mlog_write_ulint(header + FSP_SPACE_FLAGS, space->flags, MLOG_4BYTES, mtr);
  mlog_write_ulint(header + FSP_FRAG_N_USED, 0, MLOG_4BYTES, mtr);

  flst_init(header + FSP_FREE, mtr);
  flst_init(header + FSP_FREE_FRAG, mtr);
  flst_init(header + FSP_FULL_FRAG, mtr);
  flst_init(header + FSP_SEG_INODES_FULL, mtr);
  flst_init(header + FSP_SEG_INODES_FREE, mtr);

  mlog_write_ull(header + FSP_SEG_ID, 1, mtr);

  fsp_fill_free_list(
      !fsp_is_system_tablespace(space_id) && !fsp_is_global_temporary(space_id),
      space, header, mtr);

  /* For encryption tablespace, we need to save the encryption
  info to the page 0. */
  if (FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
    ulint offset = fsp_header_get_encryption_offset(page_size);
    byte encryption_info[ENCRYPTION_INFO_SIZE];

    if (offset == 0) return (false);

    if (!Encryption::fill_encryption_info(space->encryption_key,
                                          space->encryption_iv, encryption_info,
                                          is_boot)) {
      space->encryption_type = Encryption::NONE;
      memset(space->encryption_key, 0, ENCRYPTION_KEY_LEN);
      memset(space->encryption_iv, 0, ENCRYPTION_KEY_LEN);
      return (false);
    }

    mlog_write_string(page + offset, encryption_info, ENCRYPTION_INFO_SIZE,
                      mtr);
  }
  space->encryption_op_in_progress = NONE;

  if (space_id == TRX_SYS_SPACE) {
    if (btr_create(DICT_CLUSTERED | DICT_IBUF, 0, univ_page_size,
                   DICT_IBUF_ID_MIN + space_id, dict_ind_redundant,
                   mtr) == FIL_NULL) {
      return (false);
    }
  }

  return (true);
}
#endif /* !UNIV_HOTBACKUP */

/** Reads the space id from the first page of a tablespace.
 @return space id, ULINT UNDEFINED if error */
space_id_t fsp_header_get_space_id(
    const page_t *page) /*!< in: first page of a tablespace */
{
  space_id_t fsp_id;
  space_id_t id;

  fsp_id = mach_read_from_4(FSP_HEADER_OFFSET + page + FSP_SPACE_ID);

  id = mach_read_from_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

  DBUG_EXECUTE_IF("fsp_header_get_space_id_failure", id = SPACE_UNKNOWN;);

  if (id != fsp_id) {
    ib::error(ER_IB_MSG_414) << "Space ID in fsp header is " << fsp_id
                             << ", but in the page header it is " << id << ".";
    return (SPACE_UNKNOWN);
  }

  return (id);
}

/** Reads the page size from the first page of a tablespace.
@param[in]	page	first page of a tablespace
@return page size */
page_size_t fsp_header_get_page_size(const page_t *page) {
  return (page_size_t(fsp_header_get_flags(page)));
}

/** Reads the encryption key from the first page of a tablespace.
@param[in]	fsp_flags	tablespace flags
@param[in,out]	key		tablespace key
@param[in,out]	iv		tablespace iv
@param[in]	page	first page of a tablespace
@return true if success */
bool fsp_header_get_encryption_key(ulint fsp_flags, byte *key, byte *iv,
                                   page_t *page) {
  ulint offset;
  const page_size_t page_size(fsp_flags);

  offset = fsp_header_get_encryption_offset(page_size);
  if (offset == 0) {
    return (false);
  }

  return (Encryption::decode_encryption_info(key, iv, page + offset));
}

#ifndef UNIV_HOTBACKUP
/** Increases the space size field of a space. */
void fsp_header_inc_size(space_id_t space_id, /*!< in: space id */
                         page_no_t size_inc, /*!< in: size increment in pages */
                         mtr_t *mtr)         /*!< in/out: mini-transaction */
{
  fil_space_t *space = fil_space_get(space_id);

  mtr_x_lock_space(space, mtr);

  ut_d(fsp_space_modify_check(space_id, mtr));

  fsp_header_t *header;

  header = fsp_get_space_header(space_id, page_size_t(space->flags), mtr);

  page_no_t size;

  size = mach_read_from_4(header + FSP_SIZE);
  ut_ad(size == space->size_in_header);

  size += size_inc;

  fsp_header_size_update(header, size, mtr);
  space->size_in_header = size;
}

/** Gets the size of the system tablespace from the tablespace header.  If
 we do not have an auto-extending data file, this should be equal to
 the size of the data files.  If there is an auto-extending data file,
 this can be smaller.
 @return size in pages */
page_no_t fsp_header_get_tablespace_size(void) {
  fil_space_t *space = fil_space_get_sys_space();

  mtr_t mtr;

  mtr_start(&mtr);

  mtr_x_lock_space(space, &mtr);

  fsp_header_t *header;

  header = fsp_get_space_header(TRX_SYS_SPACE, univ_page_size, &mtr);

  page_no_t size;

  size = mach_read_from_4(header + FSP_SIZE);

  ut_ad(space->size_in_header == size);

  mtr_commit(&mtr);

  return (size);
}

/** Try to extend a single-table tablespace so that a page would fit in the
data file.
@param[in,out]	space	tablespace
@param[in]	page_no	page number
@param[in,out]	header	tablespace header
@param[in,out]	mtr	mini-transaction
@return true if success */
static UNIV_COLD
MY_ATTRIBUTE((warn_unused_result)) bool fsp_try_extend_data_file_with_pages(
    fil_space_t *space, page_no_t page_no, fsp_header_t *header, mtr_t *mtr) {
  DBUG_ENTER("fsp_try_extend_data_file_with_pages");

  ut_ad(!fsp_is_system_tablespace(space->id));
  ut_ad(!fsp_is_global_temporary(space->id));
  ut_d(fsp_space_modify_check(space->id, mtr));

  page_no_t size = mach_read_from_4(header + FSP_SIZE);
  ut_ad(size == space->size_in_header);

  ut_a(page_no >= size);

  bool success = fil_space_extend(space, page_no + 1);

  /* The size may be less than we wanted if we ran out of disk space. */
  fsp_header_size_update(header, space->size, mtr);
  space->size_in_header = space->size;

  DBUG_RETURN(success);
}

/** Try to extend the last data file of a tablespace if it is auto-extending.
@param[in,out]	space	tablespace
@param[in,out]	header	tablespace header
@param[in,out]	mtr	mini-transaction
@return whether the tablespace was extended */
static UNIV_COLD ulint fsp_try_extend_data_file(fil_space_t *space,
                                                fsp_header_t *header,
                                                mtr_t *mtr) {
  page_no_t size;          /* current number of pages
                           in the datafile */
  page_no_t size_increase; /* number of pages to extend
                           this file */
  const char *OUT_OF_SPACE_MSG =
      "ran out of space. Please add another file or use"
      " 'autoextend' for the last file in setting";
  DBUG_ENTER("fsp_try_extend_data_file");

  ut_d(fsp_space_modify_check(space->id, mtr));

  if (space->id == TRX_SYS_SPACE &&
      !srv_sys_space.can_auto_extend_last_file()) {
    /* We print the error message only once to avoid
    spamming the error log. Note that we don't need
    to reset the flag to false as dealing with this
    error requires server restart. */
    if (!srv_sys_space.get_tablespace_full_status()) {
      ib::error(ER_IB_MSG_415) << "Tablespace " << srv_sys_space.name() << " "
                               << OUT_OF_SPACE_MSG << " innodb_data_file_path.";
      srv_sys_space.set_tablespace_full_status(true);
    }
    DBUG_RETURN(false);
  } else if (fsp_is_global_temporary(space->id) &&
             !srv_tmp_space.can_auto_extend_last_file()) {
    /* We print the error message only once to avoid
    spamming the error log. Note that we don't need
    to reset the flag to false as dealing with this
    error requires server restart. */
    if (!srv_tmp_space.get_tablespace_full_status()) {
      ib::error(ER_IB_MSG_416)
          << "Tablespace " << srv_tmp_space.name() << " " << OUT_OF_SPACE_MSG
          << " innodb_temp_data_file_path.";
      srv_tmp_space.set_tablespace_full_status(true);
    }
    DBUG_RETURN(false);
  }

  size = mach_read_from_4(header + FSP_SIZE);
  ut_ad(size == space->size_in_header);

  const page_size_t page_size(mach_read_from_4(header + FSP_SPACE_FLAGS));

  if (space->id == TRX_SYS_SPACE) {
    size_increase = srv_sys_space.get_increment();

  } else if (fsp_is_global_temporary(space->id)) {
    size_increase = srv_tmp_space.get_increment();

  } else {
    page_no_t extent_pages = fsp_get_extent_size_in_pages(page_size);
    if (size < extent_pages) {
      /* Let us first extend the file to extent_size */
      if (!fsp_try_extend_data_file_with_pages(space, extent_pages - 1, header,
                                               mtr)) {
        DBUG_RETURN(false);
      }

      size = extent_pages;
    }

    size_increase = fsp_get_pages_to_extend_ibd(page_size, size);
  }

  if (size_increase == 0) {
    DBUG_RETURN(false);
  }

  if (!fil_space_extend(space, size + size_increase)) {
    DBUG_RETURN(false);
  }

  /* We ignore any fragments of a full megabyte when storing the size
  to the space header */

  space->size_in_header =
      ut_calc_align_down(space->size, (1024 * 1024) / page_size.physical());

  fsp_header_size_update(header, space->size_in_header, mtr);

  DBUG_RETURN(true);
}

/** Calculate the number of pages to extend a datafile.
We extend single-table and general tablespaces first one extent at a time,
but 4 at a time for bigger tablespaces. It is not enough to extend always
by one extent, because we need to add at least one extent to FSP_FREE.
A single extent descriptor page will track many extents. And the extent
that uses its extent descriptor page is put onto the FSP_FREE_FRAG list.
Extents that do not use their extent descriptor page are added to FSP_FREE.
The physical page size is used to determine how many extents are tracked
on one extent descriptor page. See xdes_calc_descriptor_page().
@param[in]	page_size	page_size of the datafile
@param[in]	size		current number of pages in the datafile
@return number of pages to extend the file. */
page_no_t fsp_get_pages_to_extend_ibd(const page_size_t &page_size,
                                      page_no_t size) {
  page_no_t size_increase; /* number of pages to extend this file */
  page_no_t extent_size;   /* one megabyte, in pages */
  page_no_t threshold;     /* The size of the tablespace (in number
                           of pages) where we start allocating more
                           than one extent at a time. */

  extent_size = fsp_get_extent_size_in_pages(page_size);

  /* The threshold is set at 32MiB except when the physical page
  size is small enough that it must be done sooner. */
  threshold =
      std::min(32 * extent_size, static_cast<page_no_t>(page_size.physical()));

  if (size < threshold) {
    size_increase = extent_size;
  } else {
    /* Below in fsp_fill_free_list() we assume
    that we add at most FSP_FREE_ADD extents at
    a time */
    size_increase = FSP_FREE_ADD * extent_size;
  }

  return (size_increase);
}

/** Initialize a fragment extent and puts it into the free fragment list.
@param[in,out]	header	tablespace header
@param[in,out]	descr	extent descriptor
@param[in,out]	mtr	mini-transaction */
static void fsp_init_xdes_free_frag(fsp_header_t *header, xdes_t *descr,
                                    mtr_t *mtr) {
  ulint n_used;

  /* The first page in the extent is a extent descriptor page
  and the second is an ibuf bitmap page: mark them used */
  xdes_set_bit(descr, XDES_FREE_BIT, FSP_XDES_OFFSET, FALSE, mtr);
  xdes_set_bit(descr, XDES_FREE_BIT, FSP_IBUF_BITMAP_OFFSET, FALSE, mtr);

  xdes_set_segment_id(descr, 0, XDES_FREE_FRAG, mtr);
  flst_add_last(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE, mtr);

  n_used = mtr_read_ulint(header + FSP_FRAG_N_USED, MLOG_4BYTES, mtr);
  mlog_write_ulint(header + FSP_FRAG_N_USED, n_used + XDES_FRAG_N_USED,
                   MLOG_4BYTES, mtr);
}

/** Put new extents to the free list if there are free extents above the free
limit. If an extent happens to contain an extent descriptor page, the extent
is put to the FSP_FREE_FRAG list with the page marked as used.
@param[in]	init_space	true if this is a single-table tablespace
and we are only initializing the first extent and the first bitmap pages;
then we will not allocate more extents
@param[in,out]	space		tablespace
@param[in,out]	header		tablespace header
@param[in,out]	mtr		mini-transaction */
static void fsp_fill_free_list(bool init_space, fil_space_t *space,
                               fsp_header_t *header, mtr_t *mtr) {
  page_no_t limit;
  page_no_t size;
  ulint flags;
  xdes_t *descr;
  ulint count = 0;
  page_no_t i;

  ut_ad(page_offset(header) == FSP_HEADER_OFFSET);
  ut_d(fsp_space_modify_check(space->id, mtr));

  /* Check if we can fill free list from above the free list limit */
  size = mach_read_from_4(header + FSP_SIZE);
  limit = mach_read_from_4(header + FSP_FREE_LIMIT);
  flags = mach_read_from_4(header + FSP_SPACE_FLAGS);

  ut_ad(size == space->size_in_header);
  ut_ad(limit == space->free_limit);

  /* Exclude Encryption flag as it might have been changed In Memory flags but
  not on disk. */
  ut_ad(!((flags ^ space->flags) & ~(FSP_FLAGS_MASK_ENCRYPTION)));

  const page_size_t page_size(flags);

  if (size < limit + FSP_EXTENT_SIZE * FSP_FREE_ADD) {
    if ((!init_space && !fsp_is_system_tablespace(space->id) &&
         !fsp_is_global_temporary(space->id)) ||
        (space->id == TRX_SYS_SPACE &&
         srv_sys_space.can_auto_extend_last_file()) ||
        (fsp_is_global_temporary(space->id) &&
         srv_tmp_space.can_auto_extend_last_file())) {
      fsp_try_extend_data_file(space, header, mtr);
      size = space->size_in_header;
    }
  }

  i = limit;

  while ((init_space && i < 1) ||
         ((i + FSP_EXTENT_SIZE <= size) && (count < FSP_FREE_ADD))) {
    bool init_xdes = (ut_2pow_remainder(i, page_size.physical()) == 0);

    space->free_limit = i + FSP_EXTENT_SIZE;
    mlog_write_ulint(header + FSP_FREE_LIMIT, i + FSP_EXTENT_SIZE, MLOG_4BYTES,
                     mtr);

    if (init_xdes) {
      buf_block_t *block;

      /* We are going to initialize a new descriptor page
      and a new ibuf bitmap page: the prior contents of the
      pages should be ignored. */

      if (i > 0) {
        const page_id_t page_id(space->id, i);

        block = buf_page_create(page_id, page_size, RW_SX_LATCH, mtr);

        buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

        fsp_init_file_page(block, mtr);
        mlog_write_ulint(buf_block_get_frame(block) + FIL_PAGE_TYPE,
                         FIL_PAGE_TYPE_XDES, MLOG_2BYTES, mtr);
      }

      /* Initialize the ibuf bitmap page in a separate
      mini-transaction because it is low in the latching
      order, and we must be able to release its latch.
      Note: Insert-Buffering is disabled for tables that
      reside in the temp-tablespace. */
      if (!fsp_is_system_temporary(space->id)) {
        mtr_t ibuf_mtr;

        mtr_start(&ibuf_mtr);

        if (space->purpose == FIL_TYPE_TEMPORARY) {
          mtr_set_log_mode(&ibuf_mtr, MTR_LOG_NO_REDO);
        }

        const page_id_t page_id(space->id, i + FSP_IBUF_BITMAP_OFFSET);

        block = buf_page_create(page_id, page_size, RW_SX_LATCH, &ibuf_mtr);

        buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

        fsp_init_file_page(block, &ibuf_mtr);

        ibuf_bitmap_page_init(block, &ibuf_mtr);

        mtr_commit(&ibuf_mtr);
      }
    }

    buf_block_t *desc_block = NULL;
    descr = xdes_get_descriptor_with_space_hdr(header, space->id, i, mtr,
                                               init_space, &desc_block);
    if (desc_block != NULL) {
      fil_block_check_type(desc_block, FIL_PAGE_TYPE_XDES, mtr);
    }
    xdes_init(descr, mtr);

    if (init_xdes) {
      fsp_init_xdes_free_frag(header, descr, mtr);
    } else {
      flst_add_last(header + FSP_FREE, descr + XDES_FLST_NODE, mtr);
      count++;
    }

    i += FSP_EXTENT_SIZE;
  }
  ut_a(count < std::numeric_limits<uint32_t>::max());
  space->free_len += (uint32_t)count;
}

/** Allocates a new free extent.
@param[in]	space_id	tablespace identifier
@param[in]	page_size	page size
@param[in]	hint		hint of which extent would be desirable: any
page offset in the extent goes; the hint must not be > FSP_FREE_LIMIT
@param[in,out]	mtr		mini-transaction
@return extent descriptor, NULL if cannot be allocated */
static xdes_t *fsp_alloc_free_extent(space_id_t space_id,
                                     const page_size_t &page_size,
                                     page_no_t hint, mtr_t *mtr) {
  fsp_header_t *header;
  fil_addr_t first;
  xdes_t *descr;
  buf_block_t *desc_block = NULL;

  header = fsp_get_space_header(space_id, page_size, mtr);

  descr = xdes_get_descriptor_with_space_hdr(header, space_id, hint, mtr, false,
                                             &desc_block);

  fil_space_t *space = fil_space_get(space_id);
  ut_a(space != NULL);

  if (desc_block != NULL) {
    fil_block_check_type(desc_block, FIL_PAGE_TYPE_XDES, mtr);
  }

  if (descr && (xdes_get_state(descr, mtr) == XDES_FREE)) {
    /* Ok, we can take this extent */
  } else {
    /* Take the first extent in the free list */
    first = flst_get_first(header + FSP_FREE, mtr);

    if (fil_addr_is_null(first)) {
      fsp_fill_free_list(false, space, header, mtr);

      first = flst_get_first(header + FSP_FREE, mtr);
    }

    if (fil_addr_is_null(first)) {
      return (NULL); /* No free extents left */
    }

    descr = xdes_lst_get_descriptor(space_id, page_size, first, mtr);
  }

  flst_remove(header + FSP_FREE, descr + XDES_FLST_NODE, mtr);
  space->free_len--;

  return (descr);
}

/** Allocates a single free page from a space. */
static void fsp_alloc_from_free_frag(
    fsp_header_t *header, /*!< in/out: tablespace header */
    xdes_t *descr,        /*!< in/out: extent descriptor */
    page_no_t bit,        /*!< in: slot to allocate in the extent */
    mtr_t *mtr)           /*!< in/out: mini-transaction */
{
  ulint frag_n_used;

  ut_ad(xdes_get_state(descr, mtr) == XDES_FREE_FRAG);
  ut_a(xdes_mtr_get_bit(descr, XDES_FREE_BIT, bit, mtr));
  xdes_set_bit(descr, XDES_FREE_BIT, bit, FALSE, mtr);

  /* Update the FRAG_N_USED field */
  frag_n_used = mach_read_from_4(header + FSP_FRAG_N_USED);
  frag_n_used++;
  mlog_write_ulint(header + FSP_FRAG_N_USED, frag_n_used, MLOG_4BYTES, mtr);
  if (xdes_is_full(descr, mtr)) {
    /* The fragment is full: move it to another list */
    flst_remove(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE, mtr);
    xdes_set_state(descr, XDES_FULL_FRAG, mtr);

    flst_add_last(header + FSP_FULL_FRAG, descr + XDES_FLST_NODE, mtr);
    mlog_write_ulint(header + FSP_FRAG_N_USED, frag_n_used - FSP_EXTENT_SIZE,
                     MLOG_4BYTES, mtr);
  }
}

/** Gets a buffer block for an allocated page.
NOTE: If init_mtr != mtr, the block will only be initialized if it was
not previously x-latched. It is assumed that the block has been
x-latched only by mtr, and freed in mtr in that case.
@param[in]	page_id		page id of the allocated page
@param[in]	page_size	page size of the allocated page
@param[in]	rw_latch	RW_SX_LATCH, RW_X_LATCH
@param[in,out]	mtr		mini-transaction of the allocation
@param[in,out]	init_mtr	mini-transaction for initializing the page
@return block, initialized if init_mtr==mtr
or rw_lock_x_lock_count(&block->lock) == 1 */
static buf_block_t *fsp_page_create(const page_id_t &page_id,
                                    const page_size_t &page_size,
                                    rw_lock_type_t rw_latch, mtr_t *mtr,
                                    mtr_t *init_mtr) {
  ut_ad(rw_latch == RW_X_LATCH || rw_latch == RW_SX_LATCH);
  buf_block_t *block = buf_page_create(page_id, page_size, rw_latch, init_mtr);

  if (init_mtr == mtr ||
      (rw_latch == RW_X_LATCH ? rw_lock_get_x_lock_count(&block->lock) == 1
                              : rw_lock_get_sx_lock_count(&block->lock) == 1)) {
    /* Initialize the page, unless it was already
    SX-latched in mtr. (In this case, we would want to
    allocate another page that has not been freed in mtr.) */
    ut_ad(init_mtr == mtr ||
          !mtr_memo_contains_flagged(
              mtr, block, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX));

    fsp_init_file_page(block, init_mtr);
  }

  return (block);
}

/** Allocates a single free page from a space.
The page is marked as used.
@param[in]	space		space id
@param[in]	page_size	page size
@param[in]	hint		hint of which page would be desirable
@param[in]	rw_latch	RW_SX_LATCH, RW_X_LATCH
@param[in,out]	mtr		mini-transaction
@param[in,out]	init_mtr	mini-transaction in which the page should be
initialized (may be the same as mtr)
@retval NULL	if no page could be allocated
@retval block	rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block	(not allocated or initialized) otherwise */
static MY_ATTRIBUTE((warn_unused_result)) buf_block_t *fsp_alloc_free_page(
    space_id_t space, const page_size_t &page_size, page_no_t hint,
    rw_lock_type_t rw_latch, mtr_t *mtr, mtr_t *init_mtr) {
  fsp_header_t *header;
  fil_addr_t first;
  xdes_t *descr;
  page_no_t free;
  page_no_t page_no;
  page_no_t space_size;

  ut_ad(mtr);
  ut_ad(init_mtr);

  ut_d(fsp_space_modify_check(space, mtr));
  header = fsp_get_space_header(space, page_size, mtr);

  /* Get the hinted descriptor */
  descr = xdes_get_descriptor_with_space_hdr(header, space, hint, mtr);

  if (descr && (xdes_get_state(descr, mtr) == XDES_FREE_FRAG)) {
    /* Ok, we can take this extent */
  } else {
    /* Else take the first extent in free_frag list */
    first = flst_get_first(header + FSP_FREE_FRAG, mtr);

    if (fil_addr_is_null(first)) {
      /* There are no partially full fragments: allocate
      a free extent and add it to the FREE_FRAG list. NOTE
      that the allocation may have as a side-effect that an
      extent containing a descriptor page is added to the
      FREE_FRAG list. But we will allocate our page from the
      the free extent anyway. */

      descr = fsp_alloc_free_extent(space, page_size, hint, mtr);

      if (descr == NULL) {
        /* No free space left */

        return (NULL);
      }

      xdes_set_state(descr, XDES_FREE_FRAG, mtr);
      flst_add_last(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE, mtr);
    } else {
      descr = xdes_lst_get_descriptor(space, page_size, first, mtr);
    }

    /* Reset the hint */
    hint = 0;
  }

  /* Now we have in descr an extent with at least one free page. Look
  for a free page in the extent. */

  free = xdes_find_bit(descr, XDES_FREE_BIT, TRUE, hint % FSP_EXTENT_SIZE, mtr);
  if (free == FIL_NULL) {
    ut_print_buf(stderr, ((byte *)descr) - 500, 1000);
    putc('\n', stderr);

    ut_error;
  }

  page_no = xdes_get_offset(descr) + free;

  space_size = mach_read_from_4(header + FSP_SIZE);
  ut_ad(space_size == fil_space_get(space)->size_in_header ||
        (space == TRX_SYS_SPACE && srv_startup_is_before_trx_rollback_phase));

  if (space_size <= page_no) {
    /* It must be that we are extending a single-table tablespace
    whose size is still < 64 pages */

    ut_a(!fsp_is_system_tablespace(space));
    ut_a(!fsp_is_global_temporary(space));
    if (page_no >= FSP_EXTENT_SIZE) {
      ib::error(ER_IB_MSG_417) << "Trying to extend a single-table"
                                  " tablespace "
                               << space
                               << " , by single"
                                  " page(s) though the space size "
                               << space_size << ". Page no " << page_no << ".";
      return (NULL);
    }

    fil_space_t *fspace = fil_space_get(space);

    if (!fsp_try_extend_data_file_with_pages(fspace, page_no, header, mtr)) {
      /* No disk space left */
      return (NULL);
    }
  }

  fsp_alloc_from_free_frag(header, descr, free, mtr);
  return (fsp_page_create(page_id_t(space, page_no), page_size, rw_latch, mtr,
                          init_mtr));
}

/** Frees a single page of a space.
The page is marked as free and clean.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction */
static void fsp_free_page(const page_id_t &page_id,
                          const page_size_t &page_size, mtr_t *mtr) {
  fsp_header_t *header;
  xdes_t *descr;
  ulint state;
  ulint frag_n_used;

  ut_ad(mtr);
  ut_d(fsp_space_modify_check(page_id.space(), mtr));

  /* fprintf(stderr, "Freeing page %lu in space %lu\n", page, space); */

  header = fsp_get_space_header(page_id.space(), page_size, mtr);

  descr = xdes_get_descriptor_with_space_hdr(header, page_id.space(),
                                             page_id.page_no(), mtr);

  state = xdes_get_state(descr, mtr);

  if (state != XDES_FREE_FRAG && state != XDES_FULL_FRAG) {
    ib::error(ER_IB_MSG_418) << "File space extent descriptor of page "
                             << page_id << " has state " << state;
    fputs("InnoDB: Dump of descriptor: ", stderr);
    ut_print_buf(stderr, ((byte *)descr) - 50, 200);
    putc('\n', stderr);
    /* Crash in debug version, so that we get a core dump
    of this corruption. */
    ut_ad(0);

    if (state == XDES_FREE) {
      /* We put here some fault tolerance: if the page
      is already free, return without doing anything! */

      return;
    }

    ut_error;
  }

  if (xdes_mtr_get_bit(descr, XDES_FREE_BIT,
                       page_id.page_no() % FSP_EXTENT_SIZE, mtr)) {
    ib::error(ER_IB_MSG_419)
        << "File space extent descriptor of page " << page_id
        << " says it is free. Dump of descriptor: ";
    ut_print_buf(stderr, ((byte *)descr) - 50, 200);
    putc('\n', stderr);
    /* Crash in debug version, so that we get a core dump
    of this corruption. */
    ut_ad(0);

    /* We put here some fault tolerance: if the page
    is already free, return without doing anything! */

    return;
  }

  const page_no_t bit = page_id.page_no() % FSP_EXTENT_SIZE;

  xdes_set_bit(descr, XDES_FREE_BIT, bit, TRUE, mtr);
  xdes_set_bit(descr, XDES_CLEAN_BIT, bit, TRUE, mtr);

  frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED, MLOG_4BYTES, mtr);
  if (state == XDES_FULL_FRAG) {
    /* The fragment was full: move it to another list */
    flst_remove(header + FSP_FULL_FRAG, descr + XDES_FLST_NODE, mtr);
    xdes_set_state(descr, XDES_FREE_FRAG, mtr);
    flst_add_last(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE, mtr);
    mlog_write_ulint(header + FSP_FRAG_N_USED,
                     frag_n_used + FSP_EXTENT_SIZE - 1, MLOG_4BYTES, mtr);
  } else {
    ut_a(frag_n_used > 0);
    mlog_write_ulint(header + FSP_FRAG_N_USED, frag_n_used - 1, MLOG_4BYTES,
                     mtr);
  }

  if (xdes_is_free(descr, mtr)) {
    /* The extent has become free: move it to another list */
    flst_remove(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE, mtr);
    fsp_free_extent(page_id, page_size, mtr);
  }
}

/** Returns an extent to the free list of a space.
@param[in]	page_id		page id in the extent
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction */
static void fsp_free_extent(const page_id_t &page_id,
                            const page_size_t &page_size, mtr_t *mtr) {
  fsp_header_t *header;
  xdes_t *descr;

  ut_ad(mtr);

  header = fsp_get_space_header(page_id.space(), page_size, mtr);

  descr = xdes_get_descriptor_with_space_hdr(header, page_id.space(),
                                             page_id.page_no(), mtr);

  switch (xdes_get_state(descr, mtr)) {
    case XDES_FSEG_FRAG:
      /* The extent is being returned to the FSP_FREE_FRAG list. */
      xdes_init(descr, mtr);
      fsp_init_xdes_free_frag(header, descr, mtr);
      break;
    case XDES_FSEG:
    case XDES_FREE_FRAG:
    case XDES_FULL_FRAG:

      xdes_init(descr, mtr);

      flst_add_last(header + FSP_FREE, descr + XDES_FLST_NODE, mtr);

      fil_space_t *space;

      space = fil_space_get(page_id.space());

      ++space->free_len;

      break;

    case XDES_FREE:
    case XDES_NOT_INITED:
      ut_error;
  }
}

/** Returns the nth inode slot on an inode page.
@param[in]	page		segment inode page
@param[in]	i		inode index on page
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return segment inode */
UNIV_INLINE
fseg_inode_t *fsp_seg_inode_page_get_nth_inode(page_t *page, page_no_t i,
                                               const page_size_t &page_size,
                                               mtr_t *mtr) {
  ut_ad(i < FSP_SEG_INODES_PER_PAGE(page_size));
  ut_ad(mtr_memo_contains_page(mtr, page, MTR_MEMO_PAGE_SX_FIX));

  return (page + FSEG_ARR_OFFSET + FSEG_INODE_SIZE * i);
}

/** Looks for a used segment inode on a segment inode page.
@param[in]	page		segment inode page
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return segment inode index, or FIL_NULL if not found */
static page_no_t fsp_seg_inode_page_find_used(page_t *page,
                                              const page_size_t &page_size,
                                              mtr_t *mtr) {
  page_no_t i;
  fseg_inode_t *inode;

  for (i = 0; i < FSP_SEG_INODES_PER_PAGE(page_size); i++) {
    inode = fsp_seg_inode_page_get_nth_inode(page, i, page_size, mtr);

    if (mach_read_from_8(inode + FSEG_ID)) {
      /* This is used */

      ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
      return (i);
    }
  }

  return (FIL_NULL);
}

/** Looks for an unused segment inode on a segment inode page.
@param[in]	page		segment inode page
@param[in]	i		search forward starting from this index
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return segment inode index, or FIL_NULL if not found */
static page_no_t fsp_seg_inode_page_find_free(page_t *page, page_no_t i,
                                              const page_size_t &page_size,
                                              mtr_t *mtr) {
  for (; i < FSP_SEG_INODES_PER_PAGE(page_size); i++) {
    fseg_inode_t *inode;

    inode = fsp_seg_inode_page_get_nth_inode(page, i, page_size, mtr);

    if (!mach_read_from_8(inode + FSEG_ID)) {
      /* This is unused */
      return (i);
    }

    ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
  }

  return (FIL_NULL);
}

/** Allocates a new file segment inode page.
 @return true if could be allocated */
static ibool fsp_alloc_seg_inode_page(
    fsp_header_t *space_header, /*!< in: space header */
    mtr_t *mtr)                 /*!< in/out: mini-transaction */
{
  fseg_inode_t *inode;
  buf_block_t *block;
  page_t *page;
  space_id_t space;

  ut_ad(page_offset(space_header) == FSP_HEADER_OFFSET);

  space = page_get_space_id(page_align(space_header));

  const page_size_t page_size(mach_read_from_4(FSP_SPACE_FLAGS + space_header));

  block = fsp_alloc_free_page(space, page_size, 0, RW_SX_LATCH, mtr, mtr);

  if (block == NULL) {
    return (FALSE);
  }

  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);
  ut_ad(rw_lock_get_sx_lock_count(&block->lock) == 1);

  page = buf_block_get_frame(block);

  mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_INODE, MLOG_2BYTES, mtr);

  for (page_no_t i = 0; i < FSP_SEG_INODES_PER_PAGE(page_size); i++) {
    inode = fsp_seg_inode_page_get_nth_inode(page, i, page_size, mtr);

    mlog_write_ull(inode + FSEG_ID, 0, mtr);
  }

  flst_add_last(space_header + FSP_SEG_INODES_FREE, page + FSEG_INODE_PAGE_NODE,
                mtr);

  return (TRUE);
}

/** Allocates a new file segment inode.
 @return segment inode, or NULL if not enough space */
static fseg_inode_t *fsp_alloc_seg_inode(
    fsp_header_t *space_header, /*!< in: space header */
    mtr_t *mtr)                 /*!< in/out: mini-transaction */
{
  buf_block_t *block;
  page_t *page;
  fseg_inode_t *inode;
  page_no_t n;

  ut_ad(page_offset(space_header) == FSP_HEADER_OFFSET);

  /* Allocate a new segment inode page if needed. */
  if (flst_get_len(space_header + FSP_SEG_INODES_FREE) == 0 &&
      !fsp_alloc_seg_inode_page(space_header, mtr)) {
    return (NULL);
  }

  const page_size_t page_size(mach_read_from_4(FSP_SPACE_FLAGS + space_header));

  const page_id_t page_id(
      page_get_space_id(page_align(space_header)),
      flst_get_first(space_header + FSP_SEG_INODES_FREE, mtr).page);

  block = buf_page_get(page_id, page_size, RW_SX_LATCH, mtr);
  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);
  fil_block_check_type(block, FIL_PAGE_INODE, mtr);

  page = buf_block_get_frame(block);

  n = fsp_seg_inode_page_find_free(page, 0, page_size, mtr);

  ut_a(n != FIL_NULL);

  inode = fsp_seg_inode_page_get_nth_inode(page, n, page_size, mtr);

  if (FIL_NULL == fsp_seg_inode_page_find_free(page, n + 1, page_size, mtr)) {
    /* There are no other unused headers left on the page: move it
    to another list */

    flst_remove(space_header + FSP_SEG_INODES_FREE, page + FSEG_INODE_PAGE_NODE,
                mtr);

    flst_add_last(space_header + FSP_SEG_INODES_FULL,
                  page + FSEG_INODE_PAGE_NODE, mtr);
  }

  ut_ad(!mach_read_from_8(inode + FSEG_ID) ||
        mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
  return (inode);
}

/** Frees a file segment inode.
@param[in]	space		space id
@param[in]	page_size	page size
@param[in,out]	inode		segment inode
@param[in,out]	mtr		mini-transaction */
static void fsp_free_seg_inode(space_id_t space, const page_size_t &page_size,
                               fseg_inode_t *inode, mtr_t *mtr) {
  page_t *page;
  fsp_header_t *space_header;

  ut_d(fsp_space_modify_check(space, mtr));

  page = page_align(inode);

  space_header = fsp_get_space_header(space, page_size, mtr);

  ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

  if (FIL_NULL == fsp_seg_inode_page_find_free(page, 0, page_size, mtr)) {
    /* Move the page to another list */

    flst_remove(space_header + FSP_SEG_INODES_FULL, page + FSEG_INODE_PAGE_NODE,
                mtr);

    flst_add_last(space_header + FSP_SEG_INODES_FREE,
                  page + FSEG_INODE_PAGE_NODE, mtr);
  }

  mlog_write_ull(inode + FSEG_ID, 0, mtr);
  mlog_write_ulint(inode + FSEG_MAGIC_N, 0xfa051ce3, MLOG_4BYTES, mtr);

  if (FIL_NULL == fsp_seg_inode_page_find_used(page, page_size, mtr)) {
    /* There are no other used headers left on the page: free it */

    flst_remove(space_header + FSP_SEG_INODES_FREE, page + FSEG_INODE_PAGE_NODE,
                mtr);

    fsp_free_page(page_id_t(space, page_get_page_no(page)), page_size, mtr);
  }
}

/** Returns the file segment inode, page x-latched.
@param[in]	header		segment header
@param[in]	space		space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@param[out]	block		inode block, or NULL to ignore
@return segment inode, page x-latched; NULL if the inode is free */
static fseg_inode_t *fseg_inode_try_get(fseg_header_t *header, space_id_t space,
                                        const page_size_t &page_size,
                                        mtr_t *mtr, buf_block_t **block) {
  fil_addr_t inode_addr;
  fseg_inode_t *inode;

  inode_addr.page = mach_read_from_4(header + FSEG_HDR_PAGE_NO);
  inode_addr.boffset = mach_read_from_2(header + FSEG_HDR_OFFSET);
  ut_ad(space == mach_read_from_4(header + FSEG_HDR_SPACE));

  inode = fut_get_ptr(space, page_size, inode_addr, RW_SX_LATCH, mtr, block);

  if (UNIV_UNLIKELY(!mach_read_from_8(inode + FSEG_ID))) {
    inode = NULL;
  } else {
    ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
  }

  return (inode);
}

/** Returns the file segment inode, page x-latched.
@param[in]	header		segment header
@param[in]	space		space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@param[out]	block		inode block
@return segment inode, page x-latched */
static fseg_inode_t *fseg_inode_get(fseg_header_t *header, space_id_t space,
                                    const page_size_t &page_size, mtr_t *mtr,
                                    buf_block_t **block = NULL) {
  fseg_inode_t *inode =
      fseg_inode_try_get(header, space, page_size, mtr, block);
  ut_a(inode);
  return (inode);
}

/** Gets the page number from the nth fragment page slot.
 @return page number, FIL_NULL if not in use */
UNIV_INLINE
page_no_t fseg_get_nth_frag_page_no(
    fseg_inode_t *inode, /*!< in: segment inode */
    ulint n,             /*!< in: slot index */
    mtr_t *mtr MY_ATTRIBUTE((unused)))
/*!< in/out: mini-transaction */
{
  ut_ad(inode && mtr);
  ut_ad(n < FSEG_FRAG_ARR_N_SLOTS);
  ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_SX_FIX));
  ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
  return (mach_read_from_4(inode + FSEG_FRAG_ARR + n * FSEG_FRAG_SLOT_SIZE));
}

/** Sets the page number in the nth fragment page slot. */
UNIV_INLINE
void fseg_set_nth_frag_page_no(fseg_inode_t *inode, /*!< in: segment inode */
                               ulint n,             /*!< in: slot index */
                               page_no_t page_no, /*!< in: page number to set */
                               mtr_t *mtr) /*!< in/out: mini-transaction */
{
  ut_ad(inode && mtr);
  ut_ad(n < FSEG_FRAG_ARR_N_SLOTS);
  ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_SX_FIX));
  ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

  mlog_write_ulint(inode + FSEG_FRAG_ARR + n * FSEG_FRAG_SLOT_SIZE, page_no,
                   MLOG_4BYTES, mtr);
}

/** Finds a fragment page slot which is free.
 @return slot index; ULINT_UNDEFINED if none found */
static ulint fseg_find_free_frag_page_slot(
    fseg_inode_t *inode, /*!< in: segment inode */
    mtr_t *mtr)          /*!< in/out: mini-transaction */
{
  ulint i;
  page_no_t page_no;

  ut_ad(inode && mtr);

  for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
    page_no = fseg_get_nth_frag_page_no(inode, i, mtr);

    if (page_no == FIL_NULL) {
      return (i);
    }
  }

  return (ULINT_UNDEFINED);
}

/** Finds a fragment page slot which is used and last in the array.
 @return slot index; ULINT_UNDEFINED if none found */
static ulint fseg_find_last_used_frag_page_slot(
    fseg_inode_t *inode, /*!< in: segment inode */
    mtr_t *mtr)          /*!< in/out: mini-transaction */
{
  ulint i;
  page_no_t page_no;

  ut_ad(inode && mtr);

  for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
    page_no =
        fseg_get_nth_frag_page_no(inode, FSEG_FRAG_ARR_N_SLOTS - i - 1, mtr);

    if (page_no != FIL_NULL) {
      return (FSEG_FRAG_ARR_N_SLOTS - i - 1);
    }
  }

  return (ULINT_UNDEFINED);
}

/** Calculates reserved fragment page slots.
 @return number of fragment pages */
static ulint fseg_get_n_frag_pages(
    fseg_inode_t *inode, /*!< in: segment inode */
    mtr_t *mtr)          /*!< in/out: mini-transaction */
{
  ulint i;
  ulint count = 0;

  ut_ad(inode && mtr);

  for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
    if (FIL_NULL != fseg_get_nth_frag_page_no(inode, i, mtr)) {
      count++;
    }
  }

  return (count);
}

/** Creates a new segment.
 @return the block where the segment header is placed, x-latched, NULL
 if could not create segment because of lack of space */
buf_block_t *fseg_create_general(
    space_id_t space_id, /*!< in: space id */
    page_no_t page,      /*!< in: page where the segment header is
                         placed: if this is != 0, the page must belong
                         to another segment, if this is 0, a new page
                         will be allocated and it will belong to the
                         created segment */
    ulint byte_offset,   /*!< in: byte offset of the created segment header
                    on the page */
    ibool has_done_reservation, /*!< in: TRUE if the caller has already
                  done the reservation for the pages with
                  fsp_reserve_free_extents (at least 2 extents: one for
                  the inode and the other for the segment) then there is
                  no need to do the check for this individual
                  operation */
    mtr_t *mtr)                 /*!< in/out: mini-transaction */
{
  fsp_header_t *space_header;
  fseg_inode_t *inode;
  ib_id_t seg_id;
  buf_block_t *block = 0;    /* remove warning */
  fseg_header_t *header = 0; /* remove warning */
  ulint n_reserved = 0;
  ulint i;

  DBUG_ENTER("fseg_create_general");

  ut_ad(mtr);
  ut_ad(byte_offset + FSEG_HEADER_SIZE <= UNIV_PAGE_SIZE - FIL_PAGE_DATA_END);
  ut_d(fsp_space_modify_check(space_id, mtr));

  fil_space_t *space = fil_space_get(space_id);

  mtr_x_lock_space(space, mtr);

  const page_size_t page_size(space->flags);

  if (page != 0) {
    block =
        buf_page_get(page_id_t(space_id, page), page_size, RW_SX_LATCH, mtr);

    header = byte_offset + buf_block_get_frame(block);

    const ulint type = space_id == TRX_SYS_SPACE && page == TRX_SYS_PAGE_NO
                           ? FIL_PAGE_TYPE_TRX_SYS
                           : FIL_PAGE_TYPE_SYS;

    fil_block_check_type(block, type, mtr);
  }

  if (rw_lock_get_x_lock_count(&space->latch) == 1) {
    /* This thread did not own the latch before this call: free
    excess pages from the insert buffer free list */

    if (space_id == IBUF_SPACE_ID) {
      ibuf_free_excess_pages();
    }
  }

  if (!has_done_reservation &&
      !fsp_reserve_free_extents(&n_reserved, space_id, 2, FSP_NORMAL, mtr)) {
    DBUG_RETURN(NULL);
  }

  space_header = fsp_get_space_header(space_id, page_size, mtr);

  inode = fsp_alloc_seg_inode(space_header, mtr);

  if (inode == NULL) {
    goto funct_exit;
  }

  /* Read the next segment id from space header and increment the
  value in space header */

  seg_id = mach_read_from_8(space_header + FSP_SEG_ID);

  mlog_write_ull(space_header + FSP_SEG_ID, seg_id + 1, mtr);

  mlog_write_ull(inode + FSEG_ID, seg_id, mtr);
  mlog_write_ulint(inode + FSEG_NOT_FULL_N_USED, 0, MLOG_4BYTES, mtr);

  flst_init(inode + FSEG_FREE, mtr);
  flst_init(inode + FSEG_NOT_FULL, mtr);
  flst_init(inode + FSEG_FULL, mtr);

  mlog_write_ulint(inode + FSEG_MAGIC_N, FSEG_MAGIC_N_VALUE, MLOG_4BYTES, mtr);
  for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
    fseg_set_nth_frag_page_no(inode, i, FIL_NULL, mtr);
  }

  if (page == 0) {
    block = fseg_alloc_free_page_low(space, page_size, inode, 0, FSP_UP,
                                     RW_SX_LATCH, mtr, mtr
#ifdef UNIV_DEBUG
                                     ,
                                     has_done_reservation
#endif /* UNIV_DEBUG */
    );

    /* The allocation cannot fail if we have already reserved a
    space for the page. */
    ut_ad(!has_done_reservation || block != NULL);

    if (block == NULL) {
      fsp_free_seg_inode(space_id, page_size, inode, mtr);

      goto funct_exit;
    }

    ut_ad(rw_lock_get_sx_lock_count(&block->lock) == 1);

    header = byte_offset + buf_block_get_frame(block);
    mlog_write_ulint(buf_block_get_frame(block) + FIL_PAGE_TYPE,
                     FIL_PAGE_TYPE_SYS, MLOG_2BYTES, mtr);
  }

  mlog_write_ulint(header + FSEG_HDR_OFFSET, page_offset(inode), MLOG_2BYTES,
                   mtr);

  mlog_write_ulint(header + FSEG_HDR_PAGE_NO,
                   page_get_page_no(page_align(inode)), MLOG_4BYTES, mtr);

  mlog_write_ulint(header + FSEG_HDR_SPACE, space_id, MLOG_4BYTES, mtr);

funct_exit:
  if (!has_done_reservation) {
    fil_space_release_free_extents(space_id, n_reserved);
  }

  DBUG_RETURN(block);
}

/** Creates a new segment.
 @return the block where the segment header is placed, x-latched, NULL
 if could not create segment because of lack of space */
buf_block_t *fseg_create(
    space_id_t space,  /*!< in: space id */
    page_no_t page,    /*!< in: page where the segment header is
                       placed: if this is != 0, the page must belong
                       to another segment, if this is 0, a new page
                       will be allocated and it will belong to the
                       created segment */
    ulint byte_offset, /*!< in: byte offset of the created
                       segment header on the page */
    mtr_t *mtr)        /*!< in/out: mini-transaction */
{
  return (fseg_create_general(space, page, byte_offset, FALSE, mtr));
}

/** Calculates the number of pages reserved by a segment, and how many pages are
 currently used.
 @return number of reserved pages */
static ulint fseg_n_reserved_pages_low(
    fseg_inode_t *inode, /*!< in: segment inode */
    ulint *used,         /*!< out: number of pages used (not
                         more than reserved) */
    mtr_t *mtr)          /*!< in/out: mini-transaction */
{
  ulint ret;

  ut_ad(inode && used && mtr);
  ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_SX_FIX));

  *used = mach_read_from_4(inode + FSEG_NOT_FULL_N_USED) +
          FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FULL) +
          fseg_get_n_frag_pages(inode, mtr);

  ret = fseg_get_n_frag_pages(inode, mtr) +
        FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FREE) +
        FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_NOT_FULL) +
        FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FULL);

  return (ret);
}

/** Calculates the number of pages reserved by a segment, and how many pages are
 currently used.
 @return number of reserved pages */
ulint fseg_n_reserved_pages(
    fseg_header_t *header, /*!< in: segment header */
    ulint *used,           /*!< out: number of pages used (<= reserved) */
    mtr_t *mtr)            /*!< in/out: mini-transaction */
{
  space_id_t space_id;

  space_id = page_get_space_id(page_align(header));

  fil_space_t *space = fil_space_get(space_id);

  mtr_x_lock_space(space, mtr);

  const page_size_t page_size(space->flags);

  fseg_inode_t *inode;

  inode = fseg_inode_get(header, space_id, page_size, mtr);

  ulint ret = fseg_n_reserved_pages_low(inode, used, mtr);

  return (ret);
}

/** Tries to fill the free list of a segment with consecutive free extents.
This happens if the segment is big enough to allow extents in the free list,
the free list is empty, and the extents can be allocated consecutively from
the hint onward.
@param[in]	inode		segment inode
@param[in]	space		space id
@param[in]	page_size	page size
@param[in]	hint		hint which extent would be good as the first
extent
@param[in,out]	mtr		mini-transaction */
static void fseg_fill_free_list(fseg_inode_t *inode, space_id_t space,
                                const page_size_t &page_size, page_no_t hint,
                                mtr_t *mtr) {
  xdes_t *descr;
  page_no_t i;
  ib_id_t seg_id;
  ulint reserved;
  ulint used;

  ut_ad(inode && mtr);
  ut_ad(!((page_offset(inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
  ut_d(fsp_space_modify_check(space, mtr));

  reserved = fseg_n_reserved_pages_low(inode, &used, mtr);

  if (reserved < FSEG_FREE_LIST_LIMIT * FSP_EXTENT_SIZE) {
    /* The segment is too small to allow extents in free list */

    return;
  }

  if (flst_get_len(inode + FSEG_FREE) > 0) {
    /* Free list is not empty */

    return;
  }

  for (i = 0; i < FSEG_FREE_LIST_MAX_LEN; i++) {
    descr = xdes_get_descriptor(space, hint, page_size, mtr);

    if ((descr == NULL) || (XDES_FREE != xdes_get_state(descr, mtr))) {
      /* We cannot allocate the desired extent: stop */

      return;
    }

    descr = fsp_alloc_free_extent(space, page_size, hint, mtr);

    seg_id = mach_read_from_8(inode + FSEG_ID);
    ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
    xdes_set_segment_id(descr, seg_id, XDES_FSEG, mtr);

    flst_add_last(inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);
    hint += FSP_EXTENT_SIZE;
  }
}

/** A fragment extent can be leased if it is the special kind that has a
descriptor page and no other pages are being used except the descriptor
and ibuf bitmap pages.  The number of used pages will be equal to
XDES_FRAG_N_USED.
@param[in]	descr		extent descriptor
@param[in]	page_size	the page size
@param[in,out]	mtr		mini transaction
@return	true if the extent is leasable, false otherwise. */
UNIV_INLINE
bool xdes_is_leasable(const xdes_t *descr, const page_size_t &page_size,
                      mtr_t *mtr) {
  ut_ad(descr && mtr);
  ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));

  const page_no_t page_no = xdes_get_offset(descr);
  const bool has_xdes_page = !ut_2pow_remainder(page_no, page_size.physical());

  if (!has_xdes_page) {
    return (false);
  }
  /* Page 0 and 1 must not be free */
  if (xdes_mtr_get_bit(descr, XDES_FREE_BIT, 0, mtr) ||
      xdes_mtr_get_bit(descr, XDES_FREE_BIT, 1, mtr)) {
    return (false);
  }

  /* All other pages must be free */
  for (page_no_t i = 2; i < FSP_EXTENT_SIZE; ++i) {
    if (!xdes_mtr_get_bit(descr, XDES_FREE_BIT, i, mtr)) {
      return (false);
    }
  }

  return (true);
}

/** Get the extent descriptor of the last fragmented extent from the
free_frag list.
@param[in]	header		tablespace header
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return	the extent descriptor, or NULL if none */
static xdes_t *fsp_get_last_free_frag_extent(fsp_header_t *header,
                                             const page_size_t &page_size,
                                             mtr_t *mtr) {
  space_id_t space;
  fil_addr_t node;
  xdes_t *descr;

  node = flst_get_last(header + FSP_FREE_FRAG, mtr);

  if (fil_addr_is_null(node)) {
    return (NULL);
  }

  space = mach_read_from_4(header + FSEG_HDR_SPACE);
  descr = xdes_lst_get_descriptor(space, page_size, node, mtr);
  ut_ad(xdes_get_state(descr, mtr) == XDES_FREE_FRAG);

  return (descr);
}

/** Allocate an extent from free fragment extent to a segment.
@param[in]	space		space id
@param[in,out]	inode		segment to which extent is leased
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return	extent descriptor or NULL */
static xdes_t *fsp_alloc_xdes_free_frag(space_id_t space, fseg_inode_t *inode,
                                        const page_size_t &page_size,
                                        mtr_t *mtr) {
  xdes_t *descr;
  ib_id_t seg_id;
  ulint n_used;

  ut_ad(mtr);
  ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space), MTR_MEMO_X_LOCK));

  fsp_header_t *header = fsp_get_space_header(space, page_size, mtr);

  /* If available, take an extent from the free_frag list. */
  if (!(descr = fsp_get_last_free_frag_extent(header, page_size, mtr))) {
    return (NULL);
  }

  if (!xdes_is_leasable(descr, page_size, mtr)) {
    return (NULL);
  }
  ut_ad(xdes_get_n_used(descr, mtr) == XDES_FRAG_N_USED);

  /* Remove from the FSP_FREE_FRAG list */
  flst_remove(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE, mtr);
  n_used = mtr_read_ulint(header + FSP_FRAG_N_USED, MLOG_4BYTES, mtr);
  mlog_write_ulint(header + FSP_FRAG_N_USED, n_used - XDES_FRAG_N_USED,
                   MLOG_4BYTES, mtr);

  /* Transition the extent (and its ownership) to the segment. */
  seg_id = mach_read_from_8(inode + FSEG_ID);
  xdes_set_segment_id(descr, seg_id, XDES_FSEG_FRAG, mtr);

  /* Add to the end of FSEG_NOT_FULL list. */
  flst_add_last(inode + FSEG_NOT_FULL, descr + XDES_FLST_NODE, mtr);
  n_used = mtr_read_ulint(inode + FSEG_NOT_FULL_N_USED, MLOG_4BYTES, mtr);
  mlog_write_ulint(inode + FSEG_NOT_FULL_N_USED, n_used + XDES_FRAG_N_USED,
                   MLOG_4BYTES, mtr);

  return (descr);
}

/** Allocates a free extent for the segment: looks first in the free list of
the segment, then tries to allocate from the space free list.
NOTE that the extent returned still resides in the segment free list, it is
not yet taken off it!
@param[in]	inode		segment inode
@param[in]	space		space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@retval NULL	if no page could be allocated
@retval block	rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block	(not allocated or initialized) otherwise */
static xdes_t *fseg_alloc_free_extent(fseg_inode_t *inode, space_id_t space,
                                      const page_size_t &page_size,
                                      mtr_t *mtr) {
  xdes_t *descr;
  ib_id_t seg_id;
  fil_addr_t first;

  ut_ad(!((page_offset(inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
  ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
  ut_d(fsp_space_modify_check(space, mtr));

  if (flst_get_len(inode + FSEG_FREE) > 0) {
    /* Segment free list is not empty, allocate from it */

    first = flst_get_first(inode + FSEG_FREE, mtr);

    descr = xdes_lst_get_descriptor(space, page_size, first, mtr);
  } else {
    /* Segment free list was empty. */

    /* Check if we can allocate an extent from free frag
    list of tablespace. */
    descr = fsp_alloc_xdes_free_frag(space, inode, page_size, mtr);

    if (descr != NULL) {
      return (descr);
    }

    /* Allocate from space */
    descr = fsp_alloc_free_extent(space, page_size, 0, mtr);

    if (descr == NULL) {
      return (NULL);
    }

    seg_id = mach_read_from_8(inode + FSEG_ID);

    xdes_set_segment_id(descr, seg_id, XDES_FSEG, mtr);
    flst_add_last(inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);

    /* Try to fill the segment free list */
    fseg_fill_free_list(inode, space, page_size,
                        xdes_get_offset(descr) + FSP_EXTENT_SIZE, mtr);
  }

  return (descr);
}

/** Allocates a single free page from a segment.
This function implements the intelligent allocation strategy which tries to
minimize file space fragmentation.
@param[in,out]	space			tablespace
@param[in]	page_size		page size
@param[in,out]	seg_inode		segment inode
@param[in]	hint			hint of which page would be desirable
@param[in]	direction		if the new page is needed because of
an index page split, and records are inserted there in order, into which
direction they go alphabetically: FSP_DOWN, FSP_UP, FSP_NO_DIR
@param[in]	rw_latch		RW_SX_LATCH, RW_X_LATCH
@param[in,out]	mtr			mini-transaction
@param[in,out]	init_mtr		mtr or another mini-transaction in
which the page should be initialized. If init_mtr != mtr, but the page is
already latched in mtr, do not initialize the page
@param[in]	has_done_reservation	TRUE if the space has already been
reserved, in this case we will never return NULL
@retval NULL	if no page could be allocated
@retval block	rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block	(not allocated or initialized) otherwise */
static buf_block_t *fseg_alloc_free_page_low(fil_space_t *space,
                                             const page_size_t &page_size,
                                             fseg_inode_t *seg_inode,
                                             page_no_t hint, byte direction,
                                             rw_lock_type_t rw_latch,
                                             mtr_t *mtr, mtr_t *init_mtr
#ifdef UNIV_DEBUG
                                             ,
                                             ibool has_done_reservation
#endif /* UNIV_DEBUG */
) {
  fsp_header_t *space_header;
  ib_id_t seg_id;
  ulint used;
  ulint reserved;
  xdes_t *descr;      /*!< extent of the hinted page */
  page_no_t ret_page; /*!< the allocated page offset, FIL_NULL
                      if could not be allocated */
  xdes_t *ret_descr;  /*!< the extent of the allocated page */
  ulint n;
  const space_id_t space_id = space->id;

  ut_ad(mtr);
  ut_ad((direction >= FSP_UP) && (direction <= FSP_NO_DIR));
  ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
  ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
  ut_ad(space->purpose == FIL_TYPE_TEMPORARY ||
        space->purpose == FIL_TYPE_TABLESPACE);
  seg_id = mach_read_from_8(seg_inode + FSEG_ID);

  ut_ad(seg_id);
  ut_d(fsp_space_modify_check(space_id, mtr));
  ut_ad(fil_page_get_type(page_align(seg_inode)) == FIL_PAGE_INODE);

  reserved = fseg_n_reserved_pages_low(seg_inode, &used, mtr);

  space_header = fsp_get_space_header(space_id, page_size, mtr);

  descr = xdes_get_descriptor_with_space_hdr(space_header, space_id, hint, mtr);
  if (descr == NULL) {
    /* Hint outside space or too high above free limit: reset
    hint */
    /* The file space header page is always allocated. */
    hint = 0;
    descr = xdes_get_descriptor(space_id, hint, page_size, mtr);
  }

  /* In the big if-else below we look for ret_page and ret_descr */
  /*-------------------------------------------------------------*/
  if (xdes_in_segment(descr, seg_id, mtr) &&
      (xdes_mtr_get_bit(descr, XDES_FREE_BIT, hint % FSP_EXTENT_SIZE, mtr) ==
       TRUE)) {
  take_hinted_page:
    /* 1. We can take the hinted page
    =================================*/
    ret_descr = descr;
    ret_page = hint;
    /* Skip the check for extending the tablespace. If the
    page hint were not within the size of the tablespace,
    we would have got (descr == NULL) above and reset the hint. */
    goto got_hinted_page;
    /*-----------------------------------------------------------*/
  } else if (xdes_get_state(descr, mtr) == XDES_FREE &&
             reserved - used < reserved / FSEG_FILLFACTOR &&
             used >= FSEG_FRAG_LIMIT) {
    /* 2. We allocate the free extent from space and can take
    =========================================================
    the hinted page
    ===============*/
    ret_descr = fsp_alloc_free_extent(space_id, page_size, hint, mtr);

    ut_a(ret_descr == descr);

    xdes_set_segment_id(ret_descr, seg_id, XDES_FSEG, mtr);
    flst_add_last(seg_inode + FSEG_FREE, ret_descr + XDES_FLST_NODE, mtr);

    /* Try to fill the segment free list */
    fseg_fill_free_list(seg_inode, space_id, page_size, hint + FSP_EXTENT_SIZE,
                        mtr);
    goto take_hinted_page;
    /*-----------------------------------------------------------*/
  } else if ((direction != FSP_NO_DIR) &&
             ((reserved - used) < reserved / FSEG_FILLFACTOR) &&
             (used >= FSEG_FRAG_LIMIT) &&
             (!!(ret_descr = fseg_alloc_free_extent(seg_inode, space_id,
                                                    page_size, mtr)))) {
    /* 3. We take any free extent (which was already assigned above
    ===============================================================
    in the if-condition to ret_descr) and take the lowest or
    ========================================================
    highest page in it, depending on the direction
    ==============================================*/
    ret_page = xdes_get_offset(ret_descr);

    if (direction == FSP_DOWN) {
      ret_page += FSP_EXTENT_SIZE - 1;
    } else if (xdes_get_state(ret_descr, mtr) == XDES_FSEG_FRAG) {
      ret_page += xdes_find_bit(ret_descr, XDES_FREE_BIT, TRUE, 0, mtr);
    }

    ut_ad(!has_done_reservation || ret_page != FIL_NULL);
    /*-----------------------------------------------------------*/
  } else if (xdes_in_segment(descr, seg_id, mtr) &&
             (!xdes_is_full(descr, mtr))) {
    /* 4. We can take the page from the same extent as the
    ======================================================
    hinted page (and the extent already belongs to the
    ==================================================
    segment)
    ========*/
    ret_descr = descr;
    ret_page = xdes_get_offset(ret_descr) +
               xdes_find_bit(ret_descr, XDES_FREE_BIT, TRUE,
                             hint % FSP_EXTENT_SIZE, mtr);
    ut_ad(!has_done_reservation || ret_page != FIL_NULL);
    /*-----------------------------------------------------------*/
  } else if (reserved - used > 0) {
    /* 5. We take any unused page from the segment
    ==============================================*/
    fil_addr_t first;

    if (flst_get_len(seg_inode + FSEG_NOT_FULL) > 0) {
      first = flst_get_first(seg_inode + FSEG_NOT_FULL, mtr);
    } else if (flst_get_len(seg_inode + FSEG_FREE) > 0) {
      first = flst_get_first(seg_inode + FSEG_FREE, mtr);
    } else {
      ut_ad(!has_done_reservation);
      return (NULL);
    }

    ret_descr = xdes_lst_get_descriptor(space_id, page_size, first, mtr);
    ret_page = xdes_get_offset(ret_descr) +
               xdes_find_bit(ret_descr, XDES_FREE_BIT, TRUE, 0, mtr);
    ut_ad(!has_done_reservation || ret_page != FIL_NULL);
    /*-----------------------------------------------------------*/
  } else if (used < FSEG_FRAG_LIMIT) {
    /* 6. We allocate an individual page from the space
    ===================================================*/
    buf_block_t *block =
        fsp_alloc_free_page(space_id, page_size, hint, rw_latch, mtr, init_mtr);

    ut_ad(!has_done_reservation || block != NULL);

    if (block != NULL) {
      /* Put the page in the fragment page array of the
      segment */
      n = fseg_find_free_frag_page_slot(seg_inode, mtr);
      ut_a(n != ULINT_UNDEFINED);

      fseg_set_nth_frag_page_no(seg_inode, n, block->page.id.page_no(), mtr);
    }

    /* fsp_alloc_free_page() invoked fsp_init_file_page()
    already. */
    return (block);
    /*-----------------------------------------------------------*/
  } else {
    /* 7. We allocate a new extent and take its first page
    ======================================================*/
    ret_descr = fseg_alloc_free_extent(seg_inode, space_id, page_size, mtr);

    if (ret_descr == NULL) {
      ret_page = FIL_NULL;
      ut_ad(!has_done_reservation);
    } else {
      const xdes_state_t state = xdes_get_state(ret_descr, mtr);
      ret_page = xdes_get_offset(ret_descr);

      if (state == XDES_FSEG_FRAG) {
        ret_page += xdes_find_bit(ret_descr, XDES_FREE_BIT, TRUE, 0, mtr);
      }

      ut_ad(!has_done_reservation || ret_page != FIL_NULL);
    }
  }

  if (ret_page == FIL_NULL) {
    /* Page could not be allocated */

    ut_ad(!has_done_reservation);
    return (NULL);
  }

  if (space->size <= ret_page && !fsp_is_system_or_temp_tablespace(space_id)) {
    /* It must be that we are extending a single-table
    tablespace whose size is still < 64 pages */

    if (ret_page >= FSP_EXTENT_SIZE) {
      ib::error(ER_IB_MSG_420)
          << "Error (2): trying to extend"
             " a single-table tablespace "
          << space_id << " by single page(s) though the"
          << " space size " << space->size << ". Page no " << ret_page << ".";
      ut_ad(!has_done_reservation);
      return (NULL);
    }

    if (!fsp_try_extend_data_file_with_pages(space, ret_page, space_header,
                                             mtr)) {
      /* No disk space left */
      ut_ad(!has_done_reservation);
      return (NULL);
    }
  }

got_hinted_page:
  /* ret_descr == NULL if the block was allocated from free_frag
  (XDES_FREE_FRAG) */
  if (ret_descr != NULL) {
    /* At this point we know the extent and the page offset.
    The extent is still in the appropriate list (FSEG_NOT_FULL
    or FSEG_FREE), and the page is not yet marked as used. */

    ut_ad(xdes_get_descriptor(space_id, ret_page, page_size, mtr) == ret_descr);

    ut_ad(xdes_mtr_get_bit(ret_descr, XDES_FREE_BIT, ret_page % FSP_EXTENT_SIZE,
                           mtr));

    fseg_mark_page_used(seg_inode, ret_page, ret_descr, mtr);
  }

  /* Exclude Encryption flag as it might have been changed In Memory flags but
  not on disk. */
  ut_ad(!((space->flags ^ mach_read_from_4(FSP_SPACE_FLAGS + space_header)) &
          ~(FSP_FLAGS_MASK_ENCRYPTION)));

  return (fsp_page_create(page_id_t(space_id, ret_page), page_size, rw_latch,
                          mtr, init_mtr));
}

/** Allocates a single free page from a segment. This function implements
 the intelligent allocation strategy which tries to minimize file space
 fragmentation.
 @retval NULL if no page could be allocated
 @retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
 (init_mtr == mtr, or the page was not previously freed in mtr)
 @retval block (not allocated or initialized) otherwise */
buf_block_t *fseg_alloc_free_page_general(
    fseg_header_t *seg_header,  /*!< in/out: segment header */
    page_no_t hint,             /*!< in: hint of which page would be
                                desirable */
    byte direction,             /*!< in: if the new page is needed because
                              of an index page split, and records are
                              inserted there in order, into which
                              direction they go alphabetically: FSP_DOWN,
                              FSP_UP, FSP_NO_DIR */
    ibool has_done_reservation, /*!< in: TRUE if the caller has
                  already done the reservation for the page
                  with fsp_reserve_free_extents, then there
                  is no need to do the check for this individual
                  page */
    mtr_t *mtr,                 /*!< in/out: mini-transaction */
    mtr_t *init_mtr)            /*!< in/out: mtr or another mini-transaction
                               in which the page should be initialized.
                               If init_mtr!=mtr, but the page is already
                               latched in mtr, do not initialize the page. */
{
  fseg_inode_t *inode;
  space_id_t space_id;
  buf_block_t *iblock;
  buf_block_t *block;
  ulint n_reserved = 0;

  space_id = page_get_space_id(page_align(seg_header));

  fil_space_t *space = fil_space_get(space_id);

  mtr_x_lock_space(space, mtr);

  const page_size_t page_size(space->flags);

  if (rw_lock_get_x_lock_count(&space->latch) == 1) {
    /* This thread did not own the latch before this call: free
    excess pages from the insert buffer free list */

    if (space_id == IBUF_SPACE_ID) {
      ibuf_free_excess_pages();
    }
  }

  inode = fseg_inode_get(seg_header, space_id, page_size, mtr, &iblock);
  fil_block_check_type(iblock, FIL_PAGE_INODE, mtr);

  if (!has_done_reservation &&
      !fsp_reserve_free_extents(&n_reserved, space_id, 2, FSP_NORMAL, mtr)) {
    return (NULL);
  }

  block = fseg_alloc_free_page_low(space, page_size, inode, hint, direction,
                                   RW_X_LATCH, mtr, init_mtr
#ifdef UNIV_DEBUG
                                   ,
                                   has_done_reservation
#endif /* UNIV_DEBUG */
  );

  /* The allocation cannot fail if we have already reserved a
  space for the page. */
  ut_ad(!has_done_reservation || block != NULL);

  if (!has_done_reservation) {
    fil_space_release_free_extents(space_id, n_reserved);
  }

  return (block);
}

/** Check that we have at least n_pages frag pages free in the first extent
of a single-table tablespace, and they are also physically initialized to
the data file. That is we have already extended the data file so that those
pages are inside the data file. If not, this function extends the tablespace
with pages.
@param[in,out]	space		tablespace
@param[in,out]	space_header	tablespace header, x-latched
@param[in]	size		size of the tablespace in pages,
must be less than FSP_EXTENT_SIZE
@param[in,out]	mtr		mini-transaction
@param[in]	n_pages		number of pages to reserve
@return true if there were at least n_pages free pages, or we were able
to extend */
static bool fsp_reserve_free_pages(fil_space_t *space,
                                   fsp_header_t *space_header, page_no_t size,
                                   mtr_t *mtr, page_no_t n_pages) {
  xdes_t *descr;

  ut_a(!fsp_is_system_tablespace(space->id));
  ut_a(!fsp_is_global_temporary(space->id));
  ut_a(size < FSP_EXTENT_SIZE);

  descr = xdes_get_descriptor_with_space_hdr(space_header, space->id, 0, mtr);
  page_no_t n_used = xdes_get_n_used(descr, mtr);

  ut_a(n_used <= size);

  return (size >= n_used + n_pages ||
          fsp_try_extend_data_file_with_pages(space, n_used + n_pages - 1,
                                              space_header, mtr));
}

/** Reserves free pages from a tablespace. All mini-transactions which may
use several pages from the tablespace should call this function beforehand
and reserve enough free extents so that they certainly will be able
to do their operation, like a B-tree page split, fully. Reservations
must be released with function fil_space_release_free_extents!

The alloc_type below has the following meaning: FSP_NORMAL means an
operation which will probably result in more space usage, like an
insert in a B-tree; FSP_UNDO means allocation to undo logs: if we are
deleting rows, then this allocation will in the long run result in
less space usage (after a purge); FSP_CLEANING means allocation done
in a physical record delete (like in a purge) or other cleaning operation
which will result in less space usage in the long run. We prefer the latter
two types of allocation: when space is scarce, FSP_NORMAL allocations
will not succeed, but the latter two allocations will succeed, if possible.
The purpose is to avoid dead end where the database is full but the
user cannot free any space because these freeing operations temporarily
reserve some space.

Single-table tablespaces whose size is < FSP_EXTENT_SIZE pages are a special
case. In this function we would liberally reserve several extents for
every page split or merge in a B-tree. But we do not want to waste disk space
if the table only occupies < FSP_EXTENT_SIZE pages. That is why we apply
different rules in that special case, just ensuring that there are n_pages
free pages available.

@param[out]	n_reserved	number of extents actually reserved; if we
                                return true and the tablespace size is <
                                FSP_EXTENT_SIZE pages, then this can be 0,
                                otherwise it is n_ext
@param[in]	space_id	tablespace identifier
@param[in]	n_ext		number of extents to reserve
@param[in]	alloc_type	page reservation type (FSP_BLOB, etc)
@param[in,out]	mtr		the mini transaction
@param[in]	n_pages		for small tablespaces (tablespace size is
                                less than FSP_EXTENT_SIZE), number of free
                                pages to reserve.
@return true if we were able to make the reservation */
bool fsp_reserve_free_extents(ulint *n_reserved, space_id_t space_id,
                              ulint n_ext, fsp_reserve_t alloc_type, mtr_t *mtr,
                              page_no_t n_pages) {
  fsp_header_t *space_header;
  ulint n_free_list_ext;
  page_no_t free_limit;
  page_no_t size;
  ulint n_free;
  ulint n_free_up;
  ulint reserve;
  DBUG_ENTER("fsp_reserve_free_extents");

  *n_reserved = n_ext;

  fil_space_t *space = fil_space_get(space_id);

  mtr_x_lock_space(space, mtr);

  const page_size_t page_size(space->flags);

  space_header = fsp_get_space_header(space_id, page_size, mtr);
try_again:
  size = mach_read_from_4(space_header + FSP_SIZE);
  ut_ad(size == space->size_in_header);

  if (size < FSP_EXTENT_SIZE && n_pages < FSP_EXTENT_SIZE / 2) {
    /* Use different rules for small single-table tablespaces */
    *n_reserved = 0;
    DBUG_RETURN(
        fsp_reserve_free_pages(space, space_header, size, mtr, n_pages));
  }

  n_free_list_ext = flst_get_len(space_header + FSP_FREE);
  ut_ad(space->free_len == n_free_list_ext);

  free_limit = mtr_read_ulint(space_header + FSP_FREE_LIMIT, MLOG_4BYTES, mtr);
  ut_ad(space->free_limit == free_limit);

  /* Below we play safe when counting free extents above the free limit:
  some of them will contain extent descriptor pages, and therefore
  will not be free extents */

  if (size >= free_limit) {
    n_free_up = (size - free_limit) / FSP_EXTENT_SIZE;
  } else {
    ut_ad(alloc_type == FSP_BLOB);
    n_free_up = 0;
  }

  if (n_free_up > 0) {
    n_free_up--;
    n_free_up -= n_free_up / (page_size.physical() / FSP_EXTENT_SIZE);
  }

  n_free = n_free_list_ext + n_free_up;

  switch (alloc_type) {
    case FSP_NORMAL:
      /* We reserve 1 extent + 0.5 % of the space size to undo logs
      and 1 extent + 0.5 % to cleaning operations; NOTE: this source
      code is duplicated in the function below! */

      reserve = 2 + ((size / FSP_EXTENT_SIZE) * 2) / 200;

      if (n_free <= reserve + n_ext) {
        goto try_to_extend;
      }
      break;
    case FSP_UNDO:
      /* We reserve 0.5 % of the space size to cleaning operations */

      reserve = 1 + ((size / FSP_EXTENT_SIZE) * 1) / 200;

      if (n_free <= reserve + n_ext) {
        goto try_to_extend;
      }
      break;
    case FSP_CLEANING:
    case FSP_BLOB:
      break;
    default:
      ut_error;
  }

  if (fil_space_reserve_free_extents(space_id, n_free, n_ext)) {
    DBUG_RETURN(true);
  }
try_to_extend:
  if (fsp_try_extend_data_file(space, space_header, mtr)) {
    goto try_again;
  }

  DBUG_RETURN(false);
}

/** Calculate how many KiB of new data we will be able to insert to the
tablespace without running out of space.
@param[in]	space_id	tablespace ID
@return available space in KiB
@retval UINTMAX_MAX if unknown */
uintmax_t fsp_get_available_space_in_free_extents(space_id_t space_id) {
  fil_space_t *space = fil_space_acquire(space_id);

  if (space == nullptr) {
    return (UINTMAX_MAX);
  }

  auto n_free_extents = fsp_get_available_space_in_free_extents(space);

  fil_space_release(space);

  return (n_free_extents);
}

/** Calculate how many KiB of new data we will be able to insert to the
tablespace without running out of space. Start with a space object that has
been acquired by the caller who holds it for the calculation,
@param[in]	space		tablespace object from fil_space_acquire()
@return available space in KiB */
uintmax_t fsp_get_available_space_in_free_extents(const fil_space_t *space) {
  ut_ad(space->n_pending_ops > 0);

  ulint size_in_header = space->size_in_header;
  if (size_in_header < FSP_EXTENT_SIZE) {
    return (0); /* TODO: count free frag pages and
                return a value based on that */
  }

  /* Below we play safe when counting free extents above the free limit:
  some of them will contain extent descriptor pages, and therefore
  will not be free extents */
  ut_ad(size_in_header >= space->free_limit);
  ulint n_free_up = (size_in_header - space->free_limit) / FSP_EXTENT_SIZE;

  page_size_t page_size(space->flags);
  if (n_free_up > 0) {
    n_free_up--;
    n_free_up -= n_free_up / (page_size.physical() / FSP_EXTENT_SIZE);
  }

  /* We reserve 1 extent + 0.5 % of the space size to undo logs
  and 1 extent + 0.5 % to cleaning operations; NOTE: this source
  code is duplicated in the function above! */

  ulint reserve = 2 + ((size_in_header / FSP_EXTENT_SIZE) * 2) / 200;
  ulint n_free = space->free_len + n_free_up;

  if (reserve > n_free) {
    return (0);
  }

  return (static_cast<uintmax_t>(n_free - reserve) * FSP_EXTENT_SIZE *
          (page_size.physical() / 1024));
}

/** Marks a page used. The page must reside within the extents of the given
 segment. */
static void fseg_mark_page_used(
    fseg_inode_t *seg_inode, /*!< in: segment inode */
    page_no_t page,          /*!< in: page offset */
    xdes_t *descr,           /*!< in: extent descriptor */
    mtr_t *mtr)              /*!< in/out: mini-transaction */
{
  ulint not_full_n_used;

  ut_ad(fil_page_get_type(page_align(seg_inode)) == FIL_PAGE_INODE);
  ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
  ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

  ut_ad(mach_read_from_8(seg_inode + FSEG_ID) ==
        xdes_get_segment_id(descr, mtr));

  if (xdes_is_free(descr, mtr)) {
    /* We move the extent from the free list to the
    NOT_FULL list */
    flst_remove(seg_inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);
    flst_add_last(seg_inode + FSEG_NOT_FULL, descr + XDES_FLST_NODE, mtr);
  }

  ut_ad(xdes_mtr_get_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, mtr));

  /* We mark the page as used */
  xdes_set_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, FALSE, mtr);

  not_full_n_used =
      mtr_read_ulint(seg_inode + FSEG_NOT_FULL_N_USED, MLOG_4BYTES, mtr);
  not_full_n_used++;
  mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED, not_full_n_used,
                   MLOG_4BYTES, mtr);
  if (xdes_is_full(descr, mtr)) {
    /* We move the extent from the NOT_FULL list to the
    FULL list */
    flst_remove(seg_inode + FSEG_NOT_FULL, descr + XDES_FLST_NODE, mtr);
    flst_add_last(seg_inode + FSEG_FULL, descr + XDES_FLST_NODE, mtr);

    mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
                     not_full_n_used - FSP_EXTENT_SIZE, MLOG_4BYTES, mtr);
  }
}

/** Frees a single page of a segment.
@param[in]	seg_inode	segment inode
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	ahi		whether we may need to drop the adaptive
hash index
@param[in,out]	mtr		mini-transaction */
static void fseg_free_page_low(fseg_inode_t *seg_inode,
                               const page_id_t &page_id,
                               const page_size_t &page_size, bool ahi,
                               mtr_t *mtr) {
  xdes_t *descr;
  ulint not_full_n_used;
  ib_id_t descr_id;
  ib_id_t seg_id;
  ulint i;
  DBUG_ENTER("fseg_free_page_low");

  ut_ad(seg_inode != NULL);
  ut_ad(mtr != NULL);
  ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
  ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
  ut_d(fsp_space_modify_check(page_id.space(), mtr));

  /* Drop search system page hash index if the page is found in
  the pool and is hashed */

  if (ahi) {
    btr_search_drop_page_hash_when_freed(page_id, page_size);
  }

  descr =
      xdes_get_descriptor(page_id.space(), page_id.page_no(), page_size, mtr);

  if (xdes_mtr_get_bit(descr, XDES_FREE_BIT,
                       page_id.page_no() % FSP_EXTENT_SIZE, mtr)) {
    fputs("InnoDB: Dump of the tablespace extent descriptor: ", stderr);
    ut_print_buf(stderr, descr, 40);

    ib::error(ER_IB_MSG_421) << "InnoDB is trying to free page " << page_id
                             << " though it is already marked as free in the"
                                " tablespace! The tablespace free space info is"
                                " corrupt. You may need to dump your tables and"
                                " recreate the whole database!";
  crash:
    ib::fatal(ER_IB_MSG_422) << FORCE_RECOVERY_MSG;
  }

  xdes_state_t state = xdes_get_state(descr, mtr);

  switch (state) {
    case XDES_FSEG:
    case XDES_FSEG_FRAG:
      /* The page belongs to a segment */
      break;
    case XDES_FREE_FRAG:
    case XDES_FULL_FRAG:
      /* The page is in the fragment pages of the segment */

      for (i = 0;; i++) {
        if (fseg_get_nth_frag_page_no(seg_inode, i, mtr) == page_id.page_no()) {
          fseg_set_nth_frag_page_no(seg_inode, i, FIL_NULL, mtr);
          break;
        }
      }

      fsp_free_page(page_id, page_size, mtr);

      DBUG_VOID_RETURN;
    case XDES_FREE:
    case XDES_NOT_INITED:
      ut_error;
  }

  /* If we get here, the page is in some extent of the segment */

  descr_id = xdes_get_segment_id(descr);
  seg_id = mach_read_from_8(seg_inode + FSEG_ID);

  if (UNIV_UNLIKELY(descr_id != seg_id)) {
    fputs("InnoDB: Dump of the tablespace extent descriptor: ", stderr);
    ut_print_buf(stderr, descr, 40);
    fputs("\nInnoDB: Dump of the segment inode: ", stderr);
    ut_print_buf(stderr, seg_inode, 40);
    putc('\n', stderr);

    ib::error(ER_IB_MSG_423)
        << "InnoDB is trying to free page " << page_id
        << ", which does not belong to segment " << descr_id
        << " but belongs to segment " << seg_id << ".";
    goto crash;
  }

  not_full_n_used =
      mtr_read_ulint(seg_inode + FSEG_NOT_FULL_N_USED, MLOG_4BYTES, mtr);
  if (xdes_is_full(descr, mtr)) {
    /* The fragment is full: move it to another list */
    flst_remove(seg_inode + FSEG_FULL, descr + XDES_FLST_NODE, mtr);
    flst_add_last(seg_inode + FSEG_NOT_FULL, descr + XDES_FLST_NODE, mtr);
    not_full_n_used += FSP_EXTENT_SIZE - 1;
  } else {
    ut_a(not_full_n_used > 0);
    not_full_n_used -= 1;
  }

  const page_no_t bit = page_id.page_no() % FSP_EXTENT_SIZE;

  xdes_set_bit(descr, XDES_FREE_BIT, bit, TRUE, mtr);
  xdes_set_bit(descr, XDES_CLEAN_BIT, bit, TRUE, mtr);

  page_no_t n_used = xdes_get_n_used(descr, mtr);

  ut_ad(state != XDES_FSEG_FRAG || (bit != 0 && bit != 1));
  ut_ad(state != XDES_FSEG_FRAG || n_used > 1);
  ut_ad(xdes_is_leasable(descr, page_size, mtr) ==
        (state == XDES_FSEG_FRAG && n_used == XDES_FRAG_N_USED));

  /* A leased fragment extent might have no more pages belonging to
  the segment.*/
  if (state == XDES_FSEG_FRAG && n_used == XDES_FRAG_N_USED) {
    n_used = 0;
    not_full_n_used -= XDES_FRAG_N_USED;
  }

  mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED, not_full_n_used,
                   MLOG_4BYTES, mtr);

  if (n_used == 0) {
    /* The extent has become free: free it to space */
    flst_remove(seg_inode + FSEG_NOT_FULL, descr + XDES_FLST_NODE, mtr);
    fsp_free_extent(page_id, page_size, mtr);
  }

  DBUG_VOID_RETURN;
}

/** Frees a single page of a segment. */
void fseg_free_page(fseg_header_t *seg_header, /*!< in: segment header */
                    space_id_t space_id,       /*!< in: space id */
                    page_no_t page,            /*!< in: page offset */
                    bool ahi,   /*!< in: whether we may need to drop
                                the adaptive hash index */
                    mtr_t *mtr) /*!< in/out: mini-transaction */
{
  DBUG_ENTER("fseg_free_page");
  fseg_inode_t *seg_inode;
  buf_block_t *iblock;

  fil_space_t *space = fil_space_get(space_id);

  mtr_x_lock_space(space, mtr);

  const page_size_t page_size(space->flags);

  DBUG_LOG("fseg_free_page", "space_id: " << space_id << ", page_no: " << page);

  seg_inode = fseg_inode_get(seg_header, space_id, page_size, mtr, &iblock);
  fil_block_check_type(iblock, FIL_PAGE_INODE, mtr);

  const page_id_t page_id(space_id, page);

  fseg_free_page_low(seg_inode, page_id, page_size, ahi, mtr);

  ut_d(buf_page_set_file_page_was_freed(page_id));

  DBUG_VOID_RETURN;
}

/** Checks if a single page of a segment is free.
 @return true if free */
bool fseg_page_is_free(fseg_header_t *seg_header, /*!< in: segment header */
                       space_id_t space_id,       /*!< in: space id */
                       page_no_t page)            /*!< in: page offset */
{
  mtr_t mtr;
  ibool is_free;
  xdes_t *descr;
  fseg_inode_t *seg_inode;

  fil_space_t *space = fil_space_get(space_id);

  mtr_start(&mtr);

  mtr_x_lock_space(space, &mtr);

  const page_size_t page_size(space->flags);

  seg_inode = fseg_inode_get(seg_header, space_id, page_size, &mtr);

  ut_a(seg_inode);
  ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
  ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));

  descr = xdes_get_descriptor(space_id, page, page_size, &mtr);
  ut_a(descr);

  is_free =
      xdes_mtr_get_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, &mtr);

  mtr_commit(&mtr);

  return (is_free);
}

/** Frees an extent of a segment to the space free list.
@param[in]	seg_inode	segment inode
@param[in]	space		space id
@param[in]	page		a page in the extent
@param[in]	page_size	page size
@param[in]	ahi		whether we may need to drop the adaptive hash
                                index
@param[in,out]	mtr		mini-transaction */
static void fseg_free_extent(fseg_inode_t *seg_inode, space_id_t space,
                             const page_size_t &page_size, page_no_t page,
                             bool ahi, mtr_t *mtr) {
  page_no_t first_page_in_extent;
  xdes_t *descr;
  page_no_t i;

  ut_ad(seg_inode != NULL);
  ut_ad(mtr != NULL);

  descr = xdes_get_descriptor(space, page, page_size, mtr);

  const xdes_state_t state = xdes_get_state(descr, mtr);
  ut_a(state == XDES_FSEG || state == XDES_FSEG_FRAG);

  ut_a(!memcmp(descr + XDES_ID, seg_inode + FSEG_ID, 8));
  ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
  ut_d(fsp_space_modify_check(space, mtr));

  first_page_in_extent = page - (page % FSP_EXTENT_SIZE);

  if (ahi) {
    for (i = state == XDES_FSEG ? 0 : XDES_FRAG_N_USED; i < FSP_EXTENT_SIZE;
         i++) {
      if (!xdes_mtr_get_bit(descr, XDES_FREE_BIT, i, mtr)) {
        /* Drop search system page hash index
        if the page is found in the pool and
        is hashed */

        btr_search_drop_page_hash_when_freed(
            page_id_t(space, first_page_in_extent + i), page_size);
      }
    }
  }

  if (xdes_is_full(descr, mtr)) {
    flst_remove(seg_inode + FSEG_FULL, descr + XDES_FLST_NODE, mtr);
  } else if (xdes_is_free(descr, mtr)) {
    flst_remove(seg_inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);
  } else {
    flst_remove(seg_inode + FSEG_NOT_FULL, descr + XDES_FLST_NODE, mtr);

    page_no_t not_full_n_used =
        mtr_read_ulint(seg_inode + FSEG_NOT_FULL_N_USED, MLOG_4BYTES, mtr);

    page_no_t descr_n_used = xdes_get_n_used(descr, mtr);
    ut_a(not_full_n_used >= descr_n_used);
    mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
                     not_full_n_used - descr_n_used, MLOG_4BYTES, mtr);
  }

  fsp_free_extent(page_id_t(space, page), page_size, mtr);

#ifdef UNIV_DEBUG
  for (i = state == XDES_FSEG ? 0 : XDES_FRAG_N_USED; i < FSP_EXTENT_SIZE;
       i++) {
    buf_page_set_file_page_was_freed(
        page_id_t(space, first_page_in_extent + i));
  }
#endif /* UNIV_DEBUG */
}

/** Frees part of a segment. This function can be used to free a segment by
 repeatedly calling this function in different mini-transactions. Doing
 the freeing in a single mini-transaction might result in too big a
 mini-transaction.
 @return true if freeing completed */
ibool fseg_free_step(
    fseg_header_t *header, /*!< in, own: segment header; NOTE: if the header
                           resides on the first page of the frag list
                           of the segment, this pointer becomes obsolete
                           after the last freeing step */
    bool ahi,              /*!< in: whether we may need to drop
                           the adaptive hash index */
    mtr_t *mtr)            /*!< in/out: mini-transaction */
{
  ulint n;
  page_no_t page;
  xdes_t *descr;
  fseg_inode_t *inode;
  space_id_t space_id;
  page_no_t header_page;

  DBUG_ENTER("fseg_free_step");

  space_id = page_get_space_id(page_align(header));
  header_page = page_get_page_no(page_align(header));

  fil_space_t *space = fil_space_get(space_id);

  mtr_x_lock_space(space, mtr);

  const page_size_t page_size(space->flags);

  descr = xdes_get_descriptor(space_id, header_page, page_size, mtr);

  /* Check that the header resides on a page which has not been
  freed yet */

  ut_a(xdes_mtr_get_bit(descr, XDES_FREE_BIT, header_page % FSP_EXTENT_SIZE,
                        mtr) == FALSE);
  buf_block_t *iblock;

  inode = fseg_inode_try_get(header, space_id, page_size, mtr, &iblock);

  if (inode == NULL) {
    ib::info(ER_IB_MSG_424)
        << "Double free of inode from " << page_id_t(space_id, header_page);
    DBUG_RETURN(TRUE);
  }

  fil_block_check_type(iblock, FIL_PAGE_INODE, mtr);
  descr = fseg_get_first_extent(inode, space_id, page_size, mtr);

  if (descr != NULL) {
    /* Free the extent held by the segment */
    page = xdes_get_offset(descr);

    fseg_free_extent(inode, space_id, page_size, page, ahi, mtr);

    DBUG_RETURN(FALSE);
  }

  /* Free a frag page */
  n = fseg_find_last_used_frag_page_slot(inode, mtr);

  if (n == ULINT_UNDEFINED) {
    /* Freeing completed: free the segment inode */
    fsp_free_seg_inode(space_id, page_size, inode, mtr);

    DBUG_RETURN(TRUE);
  }

  fseg_free_page_low(
      inode, page_id_t(space_id, fseg_get_nth_frag_page_no(inode, n, mtr)),
      page_size, ahi, mtr);

  n = fseg_find_last_used_frag_page_slot(inode, mtr);

  if (n == ULINT_UNDEFINED) {
    /* Freeing completed: free the segment inode */
    fsp_free_seg_inode(space_id, page_size, inode, mtr);

    DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}

/** Frees part of a segment. Differs from fseg_free_step because this function
 leaves the header page unfreed.
 @return true if freeing completed, except the header page */
ibool fseg_free_step_not_header(
    fseg_header_t *header, /*!< in: segment header which must reside on
                           the first fragment page of the segment */
    bool ahi,              /*!< in: whether we may need to drop
                           the adaptive hash index */
    mtr_t *mtr)            /*!< in/out: mini-transaction */
{
  ulint n;
  xdes_t *descr;
  fseg_inode_t *inode;
  space_id_t space_id;
  page_no_t page_no;

  space_id = page_get_space_id(page_align(header));

  fil_space_t *space = fil_space_get(space_id);

  mtr_x_lock_space(space, mtr);

  const page_size_t page_size(space->flags);
  buf_block_t *iblock;

  inode = fseg_inode_get(header, space_id, page_size, mtr, &iblock);
  fil_block_check_type(iblock, FIL_PAGE_INODE, mtr);

  descr = fseg_get_first_extent(inode, space_id, page_size, mtr);

  if (descr != NULL) {
    /* Free the extent held by the segment */
    page_no = xdes_get_offset(descr);

    fseg_free_extent(inode, space_id, page_size, page_no, ahi, mtr);

    return (FALSE);
  }

  /* Free a frag page */

  n = fseg_find_last_used_frag_page_slot(inode, mtr);

  if (n == ULINT_UNDEFINED) {
    ut_error;
  }

  page_no = fseg_get_nth_frag_page_no(inode, n, mtr);

  if (page_no == page_get_page_no(page_align(header))) {
    return (TRUE);
  }

  fseg_free_page_low(inode, page_id_t(space_id, page_no), page_size, ahi, mtr);

  return (FALSE);
}

/** Returns the first extent descriptor for a segment.
We think of the extent lists of the segment catenated in the order
FSEG_FULL -> FSEG_NOT_FULL -> FSEG_FREE.
@param[in]	inode		segment inode
@param[in]	space_id	space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return the first extent descriptor, or NULL if none */
static xdes_t *fseg_get_first_extent(fseg_inode_t *inode, space_id_t space_id,
                                     const page_size_t &page_size, mtr_t *mtr) {
  fil_addr_t first;
  xdes_t *descr;

  ut_ad(inode && mtr);

  ut_ad(space_id == page_get_space_id(page_align(inode)));
  ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

  first = fil_addr_null;

  if (flst_get_len(inode + FSEG_FULL) > 0) {
    first = flst_get_first(inode + FSEG_FULL, mtr);

  } else if (flst_get_len(inode + FSEG_NOT_FULL) > 0) {
    first = flst_get_first(inode + FSEG_NOT_FULL, mtr);

  } else if (flst_get_len(inode + FSEG_FREE) > 0) {
    first = flst_get_first(inode + FSEG_FREE, mtr);
  }

  if (first.page == FIL_NULL) {
    return (NULL);
  }
  descr = xdes_lst_get_descriptor(space_id, page_size, first, mtr);

  return (descr);
}

#ifdef UNIV_BTR_PRINT
/** Writes info of a segment. */
static void fseg_print_low(fseg_inode_t *inode, /*!< in: segment inode */
                           mtr_t *mtr)          /*!< in/out: mini-transaction */
{
  space_id_t space;
  ulint n_used;
  ulint n_frag;
  ulint n_free;
  ulint n_not_full;
  ulint n_full;
  ulint reserved;
  ulint used;
  page_no_t page_no;
  ib_id_t seg_id;

  ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_SX_FIX));
  space = page_get_space_id(page_align(inode));
  page_no = page_get_page_no(page_align(inode));

  reserved = fseg_n_reserved_pages_low(inode, &used, mtr);

  seg_id = mach_read_from_8(inode + FSEG_ID);

  n_used = mtr_read_ulint(inode + FSEG_NOT_FULL_N_USED, MLOG_4BYTES, mtr);
  n_frag = fseg_get_n_frag_pages(inode, mtr);
  n_free = flst_get_len(inode + FSEG_FREE);
  n_not_full = flst_get_len(inode + FSEG_NOT_FULL);
  n_full = flst_get_len(inode + FSEG_FULL);

  ib::info(ER_IB_MSG_425) << "SEGMENT id " << seg_id << " space " << space
                          << ";"
                          << " page " << page_no << ";"
                          << " res " << reserved << " used " << used << ";"
                          << " full ext " << n_full << ";"
                          << " fragm pages " << n_frag << ";"
                          << " free extents " << n_free << ";"
                          << " not full extents " << n_not_full << ": pages "
                          << n_used;

  ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
}

/** Writes info of a segment. */
void fseg_print(fseg_header_t *header, /*!< in: segment header */
                mtr_t *mtr)            /*!< in/out: mini-transaction */
{
  fseg_inode_t *inode;
  space_id_t space_id;

  space_id = page_get_space_id(page_align(header));

  fil_space_t *space = fil_space_get();

  mtr_x_lock_space(space, mtr);

  const page_size_t page_size(space->flags);

  inode = fseg_inode_get(header, space_id, page_size, mtr);

  fseg_print_low(inode, mtr);
}
#endif /* UNIV_BTR_PRINT */

/** Retrieve tablespace dictionary index root page number stored in the
page 0
@param[in]	space		tablespace id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return root page num of the tablspace dictionary index copy */
page_no_t fsp_sdi_get_root_page_num(space_id_t space,
                                    const page_size_t &page_size, mtr_t *mtr) {
  ut_ad(mtr != NULL);

  buf_block_t *block =
      buf_page_get(page_id_t(space, 0), page_size, RW_S_LATCH, mtr);
  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

  page_t *page = buf_block_get_frame(block);

  ulint sdi_offset = fsp_header_get_sdi_offset(page_size);

  uint32_t sdi_ver = mach_read_from_4(page + sdi_offset);

  if (sdi_ver != SDI_VERSION) {
    ib::warn(ER_IB_MSG_426) << "SDI version mismatch. Expected: " << SDI_VERSION
                            << " Current version: " << sdi_ver;
  }
  ut_ad(sdi_ver == SDI_VERSION);

  page_no_t root = mach_read_from_4(page + sdi_offset + 4);

  ut_ad(root > 2);

  return (root);
}

/** Write SDI Index root page num to page 0 of tablespace.
@param[in,out]	page		page 0 frame
@param[in]	page_size	size of page
@param[in]	root_page_num	root page number of SDI
@param[in,out]	mtr		mini-transaction */
void fsp_sdi_write_root_to_page(page_t *page, const page_size_t &page_size,
                                page_no_t root_page_num, mtr_t *mtr) {
  ut_ad(page_get_page_no(page) == 0);

  ulint sdi_offset = fsp_header_get_sdi_offset(page_size);

  /* Write SDI version here. */
  mlog_write_ulint(page + sdi_offset, SDI_VERSION, MLOG_4BYTES, mtr);

  /* Write SDI root page number */
  mlog_write_ulint(page + sdi_offset + 4, root_page_num, MLOG_4BYTES, mtr);
}

#ifdef UNIV_DEBUG
/** Print the file segment header to the given output stream.
@param[in]	out	the output stream into which the object is printed.
@retval	the output stream into which the object was printed. */
std::ostream &fseg_header::to_stream(std::ostream &out) const {
  const space_id_t space =
      mtr_read_ulint(m_header + FSEG_HDR_SPACE, MLOG_4BYTES, m_mtr);

  const page_no_t page_no =
      mtr_read_ulint(m_header + FSEG_HDR_PAGE_NO, MLOG_4BYTES, m_mtr);

  const ulint offset =
      mtr_read_ulint(m_header + FSEG_HDR_OFFSET, MLOG_2BYTES, m_mtr);

  out << "[fseg_header_t: space=" << space << ", page=" << page_no
      << ", offset=" << offset << "]";

  return (out);
}
#endif /* UNIV_DEBUG */

/** Determine if extent belongs to a given segment.
@param[in]	descr	extent descriptor
@param[in]	seg_id	segment identifier
@param[in]	mtr	mini-transaction
@return	true if extent is part of the segment, false otherwise */
static bool xdes_in_segment(const xdes_t *descr, ib_id_t seg_id, mtr_t *mtr) {
  const xdes_state_t state = xdes_get_state(descr, mtr);
  return ((state == XDES_FSEG || state == XDES_FSEG_FRAG) &&
          xdes_get_segment_id(descr, mtr) == seg_id);
}

#ifdef UNIV_DEBUG
fsp_header_mem_t::fsp_header_mem_t(const fsp_header_t *header, mtr_t *mtr)
    : m_space_id(mach_read_from_4(header + FSP_SPACE_ID)),
      m_notused(0),
      m_fsp_size(mach_read_from_4(header + FSP_SIZE)),
      m_free_limit(mach_read_from_4(header + FSP_FREE_LIMIT)),
      m_flags(mach_read_from_4(header + FSP_SPACE_FLAGS)),
      m_fsp_frag_n_used(mach_read_from_4(header + FSP_FRAG_N_USED)),
      m_fsp_free(header + FSP_FREE, mtr),
      m_free_frag(header + FSP_FREE_FRAG, mtr),
      m_full_frag(header + FSP_FULL_FRAG, mtr),
      m_segid(mach_read_from_8(header + FSP_SEG_ID)),
      m_inodes_full(header + FSP_SEG_INODES_FULL, mtr),
      m_inodes_free(header + FSP_SEG_INODES_FREE, mtr) {}

std::ostream &fsp_header_mem_t::print(std::ostream &out) const {
  out << "[fsp_header_t: "
      << "m_space_id=" << m_space_id << ", m_fsp_size=" << m_fsp_size
      << ", m_free_limit=" << m_free_limit << ", m_flags=" << m_flags
      << ", m_fsp_frag_n_used=" << m_fsp_frag_n_used
      << ", m_fsp_free=" << m_fsp_free << ", m_free_frag=" << m_free_frag
      << ", m_full_frag=" << m_full_frag << ", m_segid=" << m_segid
      << ", m_inodes_full=" << m_inodes_full
      << ", m_inodes_free=" << m_inodes_free << "]";
  return (out);
}

/** Print the extent descriptor page in user-friendly format.
@param[in]	out	the output file stream
@param[in]	xdes	the extent descriptor page
@param[in]	page_no	the page number of xdes page
@param[in]	mtr	the mini transaction.
@return None. */
std::ostream &xdes_page_print(std::ostream &out, const page_t *xdes,
                              page_no_t page_no, mtr_t *mtr) {
  out << "[Extent Descriptor Page: page_no=" << page_no << "\n";

  if (page_no == 0) {
    const fsp_header_t *tmp = xdes + FSP_HEADER_OFFSET;
    fsp_header_mem_t header(tmp, mtr);
    out << header << "\n";
  }

  ulint N = UNIV_PAGE_SIZE / FSP_EXTENT_SIZE;

  for (ulint i = 0; i < N; ++i) {
    const byte *desc = xdes + XDES_ARR_OFFSET + (i * XDES_SIZE);
    xdes_mem_t x(desc);

    if (x.is_valid()) {
      out << x << "\n";
    }
  }
  out << "]\n";
  return (out);
}

std::ostream &xdes_mem_t::print(std::ostream &out) const {
  ut_ad(m_xdes != NULL);

  const page_no_t page_no = xdes_get_offset(m_xdes);
  const ib_id_t seg_id = xdes_get_segment_id(m_xdes);

  out << "[xdes_t: segid=" << seg_id << ",page=" << page_no
      << ",state=" << state_name() << ",bitmap=[";
  for (page_no_t i = 0; i < FSP_EXTENT_SIZE; ++i) {
    const bool is_free = xdes_get_bit(m_xdes, XDES_FREE_BIT, i);
    out << (is_free ? "." : "+");
  }
  out << "]]";
  return (out);
}

/** Check if the tablespace size information is valid.
@param[in]	space_id	the tablespace identifier
@return true if valid, false if invalid. */
bool fsp_check_tablespace_size(space_id_t space_id) {
  mtr_t mtr;

  mtr_start(&mtr);

  fil_space_t *space = fil_space_get(space_id);

  mtr_x_lock_space(space, &mtr);

  const page_size_t page_size(space->flags);

  fsp_header_t *space_header = fsp_get_space_header(space_id, page_size, &mtr);

  xdes_t *descr =
      xdes_get_descriptor_with_space_hdr(space_header, space->id, 0, &mtr);

  ulint n_used = xdes_get_n_used(descr, &mtr);
  ulint size = mach_read_from_4(space_header + FSP_SIZE);
  ut_a(n_used <= size);

  mtr_commit(&mtr);

  return (true);
}
#endif /* UNIV_DEBUG */

/** Determine if the tablespace has SDI.
@param[in]	space_id	Tablespace id
@return DB_SUCCESS if SDI is present else DB_ERROR
or DB_TABLESPACE_NOT_FOUND */
dberr_t fsp_has_sdi(space_id_t space_id) {
  fil_space_t *space = fil_space_acquire_silent(space_id);
  if (space == NULL) {
    DBUG_EXECUTE_IF(
        "ib_sdi", ib::warn(ER_IB_MSG_427)
                      << "Tablespace doesn't exist for space_id: " << space_id;
        ib::warn(ER_IB_MSG_428) << "Is the tablespace dropped or discarded";);
    return (DB_TABLESPACE_NOT_FOUND);
  }

#ifdef UNIV_DEBUG
  mtr_t mtr;
  mtr.start();
  ut_ad(fsp_sdi_get_root_page_num(space_id, page_size_t(space->flags), &mtr) !=
        0);
  mtr.commit();
#endif /* UNIV_DEBUG */

  fil_space_release(space);
  DBUG_EXECUTE_IF("ib_sdi", if (!FSP_FLAGS_HAS_SDI(space->flags)) {
    ib::warn(ER_IB_MSG_429)
        << "SDI doesn't exist in tablespace: " << space->name;
  });
  return (FSP_FLAGS_HAS_SDI(space->flags) ? DB_SUCCESS : DB_ERROR);
}

/** Mark all pages in tablespace dirty
@param[in]	thd		current thread
@param[in]	space_id	tablespace id
@param[in]	space_flags	tablespace flags
@param[in]	total_pages	total pages in tablespace
@param[in]	from_page	page number from where to start the operation */
static void mark_all_page_dirty_in_tablespace(THD *thd, space_id_t space_id,
                                              ulint space_flags,
                                              page_no_t total_pages,
                                              page_no_t from_page) {
#ifdef HAVE_PSI_STAGE_INTERFACE
  ut_stage_alter_ts progress_monitor;
#endif
  page_size_t pageSize(space_flags);
  page_no_t current_page = from_page;
  mtr_t mtr;

  /* Page 0 is never encrypted */
  ut_ad(current_page != 0);

#ifdef HAVE_PSI_STAGE_INTERFACE
  progress_monitor.init(srv_stage_alter_tablespace_encryption.m_key);
  progress_monitor.set_estimate(total_pages - current_page);
#endif

  while (current_page < total_pages) {
    /* Mark group of PAGE_GROUP_SIZE pages dirty */
    mtr_start(&mtr);
    page_no_t inner_count = 0;
    for (; inner_count < PAGE_GROUP_SIZE && current_page < total_pages;
         inner_count++, current_page++) {
      /* As we are trying to read each and every page of
      tablespace, there might be few pages which are freed.
      Take them into consideration. */
      buf_block_t *block = buf_page_get_gen(
          page_id_t(space_id, current_page), pageSize, RW_X_LATCH, NULL,
          BUF_GET_POSSIBLY_FREED, __FILE__, __LINE__, &mtr);

      if (block == nullptr) {
        continue;
      }

      page_t *page = buf_block_get_frame(block);
      page_zip_des_t *page_zip = buf_block_get_page_zip(block);

      /* If page is not initialized */
      if (page_get_space_id(page) == 0 || page_get_page_no(page) == 0) {
        continue;
      }

      if (page_zip != nullptr &&
          fil_page_type_is_index(fil_page_get_type(page))) {
        mach_write_to_4(page + FIL_PAGE_SPACE_ID, space_id);
        page_zip_write_header(page_zip, page + FIL_PAGE_SPACE_ID, 4, &mtr);
      } else {
        mlog_write_ulint(page + FIL_PAGE_SPACE_ID, space_id, MLOG_4BYTES, &mtr);
      }

      DBUG_INJECT_CRASH_WITH_LOG_FLUSH("alter_encrypt_tablespace_inner_page",
                                       current_page - 1);
    }
    mtr_commit(&mtr);

    mtr_start(&mtr);
    /* Write (Un)Encryption progress on page 0 */
    fsp_header_write_encryption_progress(space_id, space_flags,
                                         current_page - 1, 0, false, &mtr);
    mtr_commit(&mtr);

#ifdef HAVE_PSI_STAGE_INTERFACE
    /* Update progress stats */
    progress_monitor.update_work(inner_count);
#endif

    DBUG_EXECUTE_IF("alter_encrypt_tablespace_insert_delay", sleep(1););

    DBUG_INJECT_CRASH_WITH_LOG_FLUSH("alter_encrypt_tablespace_page",
                                     current_page - 1);

#ifdef UNIV_DEBUG
    if ((current_page - 1) == 5) {
      DEBUG_SYNC(thd, "alter_encrypt_tablespace_wait_after_page5");
    }
#endif /* UNIV_DEBUG */
  }
}

/** Encrypt/Unencrypt a tablespace.
@param[in]	thd		current thread
@param[in]	space_id	Tablespace id
@param[in]	from_page	page id from where operation to be done
@param[in]	to_encrypt	true if to encrypt, false if to unencrypt
@param[in]	in_recovery	true if its called after recovery
@param[in,out]	dd_space_in	dd tablespace object
@return 0 for success, otherwise error code */
dberr_t fsp_alter_encrypt_tablespace(THD *thd, space_id_t space_id,
                                     page_no_t from_page, bool to_encrypt,
                                     bool in_recovery, void *dd_space_in) {
  dberr_t err = DB_SUCCESS;
  fil_space_t *space = fil_space_get(space_id);
  ulint space_flags = 0;
  page_no_t total_pages = 0;
  dd::Tablespace *dd_space = reinterpret_cast<dd::Tablespace *>(dd_space_in);
  byte operation_type = 0;
  byte encryption_info[ENCRYPTION_INFO_SIZE];
  memset(encryption_info, 0, ENCRYPTION_INFO_SIZE);
  mtr_t mtr;

  DBUG_ENTER("fsp_encrypt_or_unencrypt_tablespace");

  /* Page 0 is never encrypted */
  ut_ad(from_page != 0);

  operation_type |=
      (to_encrypt) ? ENCRYPTION_IN_PROGRESS : UNENCRYPTION_IN_PROGRESS;

  if (!in_recovery) { /* NOT IN RECOVERY */
    ut_ad(space->encryption_op_in_progress == NONE);
    if (to_encrypt) {
      /* Assert that tablespace is not encrypted */
      ut_ad(!FSP_FLAGS_GET_ENCRYPTION(space->flags));

      /* Fill key, iv and prepare encryption_info to be
      written in page 0 */
      byte key[ENCRYPTION_KEY_LEN];
      byte iv[ENCRYPTION_KEY_LEN];

      /* Try to read encryption information from page 0. If found, that will be
      used, otherwise a new encryption key, iv will be generated and used. */
      if (fsp_header_read_encryption_info(space->id, space->flags,
                                          space->encryption_key,
                                          space->encryption_iv)) {
        memcpy(key, space->encryption_key, ENCRYPTION_KEY_LEN);
        memcpy(iv, space->encryption_iv, ENCRYPTION_KEY_LEN);
      } else {
        Encryption::random_value(key);
        Encryption::random_value(iv);
      }

      /* Prepare encrypted encryption information to be written on page 0. */
      if (!Encryption::fill_encryption_info(key, iv, encryption_info, false)) {
        ut_ad(0);
      }

      /* Write Encryption information and space flags now on page 0
      NOTE : Not modifying space->flags as of now, because we want to persist
      the changes on disk and then modify in memory flags. */
      mtr_start(&mtr);
      if (!fsp_header_write_encryption(space_id,
                                       space->flags | FSP_FLAGS_MASK_ENCRYPTION,
                                       encryption_info, true, false, &mtr)) {
        ut_ad(0);
      }

      /* Write on page 0
              - Operation type (Encryption/Unencryption)
              - Write (Un)Encryption progress (0 now) */
      fsp_header_write_encryption_progress(space_id, space->flags, 0,
                                           operation_type, true, &mtr);
      mtr_commit(&mtr);

      /* Make sure REDO logs are flushed till this point */
      log_buffer_flush_to_disk();

      /* As DMLs are allowed in parallel, pass false for 'strict' */
      buf_LRU_flush_or_remove_pages(space_id, BUF_REMOVE_FLUSH_WRITE, 0, false);

      /* Set encryption for tablespace */
      rw_lock_x_lock(&space->latch);
      err = fil_set_encryption(space_id, Encryption::AES, key, iv);
      rw_lock_x_unlock(&space->latch);
      ut_ad(err == DB_SUCCESS);

      /* Set encryption operation in progress flag */
      space->encryption_op_in_progress = ENCRYPTION;

      /* Update Encryption flag for tablespace */
      space->flags |= FSP_FLAGS_MASK_ENCRYPTION;
    } else {
      /* Assert that tablespace is encrypted */
      ut_ad(FSP_FLAGS_GET_ENCRYPTION(space->flags));

      mtr_start(&mtr);
      /* Write on page 0
              - Operation type (Encryption/Unencryption)
              - Write (Un)Encryption progress (0 now) */
      fsp_header_write_encryption_progress(space_id, space->flags, 0,
                                           operation_type, true, &mtr);
      mtr_commit(&mtr);

      /* Make sure REDO logs are flushed till this point */
      log_buffer_flush_to_disk();

      /* As DMLs are allowed in parallel, pass false for 'strict' */
      buf_LRU_flush_or_remove_pages(space_id, BUF_REMOVE_FLUSH_WRITE, 0, false);

      /* Set encryption operation in progress flag */
      space->encryption_op_in_progress = UNENCRYPTION;

      /* Update Encryption flag for tablespace */
      space->flags &= ~FSP_FLAGS_MASK_ENCRYPTION;

      /* Don't erase Encryption info from page 0 yet */
    }

    /* Till this point,
            - ddl_log entry has been made.
            For encryption :
             - In-mem Encryption information set for tablesapace.
             - In-mem Tablespace flags have been updated.
             - Encryption Info, Tablespace updated flags have been
               written to page 0.
             - Page 0 have been updated to indicate operation type.
            For Unencryption :
             - In-mem Tablespace flags have been updated.
             - Page 0 have been updated to indicate operation type.

    Now, read tablespace pages one by one and mark them dirty. */
  } else { /* IN RECOVERY */

    /* A corner case when crash happened after last page was processed but
    page 0 wasn't updated with this information. */
    if (from_page == space->size) {
      goto all_done;
    }

    /* If in recovery, update Tablespace Encryption flag again now
    as DD flags wouldn't have been updated before crash. */
    if (to_encrypt) {
      /* Tablespace Encryption flag were written on page 0
      before crash. */
      ut_ad(FSP_FLAGS_GET_ENCRYPTION(space->flags));

      /* It should have already been set */
      ut_ad(space->encryption_op_in_progress == ENCRYPTION);
    } else {
      /* Tablespace Encryption flag were not written on page 0
      before crash. */
      ut_ad(FSP_FLAGS_GET_ENCRYPTION(space->flags));

      /* It should have already been set */
      ut_ad(space->encryption_op_in_progress == UNENCRYPTION);

      /* Update Encryption flag for tablespace */
      space->flags &= ~FSP_FLAGS_MASK_ENCRYPTION;

      /* Don't erase Encryption information from page 0 yet */
    }
  }

  space_flags = space->flags;
  total_pages = space->size;

  /* Mark all pages in tablespace dirty */
  mark_all_page_dirty_in_tablespace(thd, space_id, space_flags, total_pages,
                                    from_page);

  /* As DMLs are allowed in parallel, pass false for 'strict' */
  buf_LRU_flush_or_remove_pages(space_id, BUF_REMOVE_FLUSH_WRITE, 0, false);

  /* Till this point, all pages in tablespace have been marked dirty and
  flushed to disk . */

all_done:
  /* For unencryption, if server crashed, before tablespace flags were flushed
  on disk. Set them now. */
  if (in_recovery && !to_encrypt) {
    space->flags &= ~FSP_FLAGS_MASK_ENCRYPTION;
  }

  /* If it was an Unencryption operation */
  if (!to_encrypt) {
    /* Crash before updating tablespace flags on page 0 */
    DBUG_EXECUTE_IF("alter_encrypt_tablespace_crash_before_updating_flags",
                    log_buffer_flush_to_disk();
                    DBUG_SUICIDE(););

    ut_ad(!FSP_FLAGS_GET_ENCRYPTION(space->flags));
#ifdef UNIV_DEBUG
    byte buf[ENCRYPTION_INFO_SIZE];
    memset(buf, 0, ENCRYPTION_INFO_SIZE);
    ut_ad(memcmp(encryption_info, buf, ENCRYPTION_INFO_SIZE) == 0);
#endif
    /* Now on page 0
            - erase Encryption information
            - write updated Tablespace flag */
    mtr_start(&mtr);
    if (!fsp_header_write_encryption(space_id, space->flags, encryption_info,
                                     true, false, &mtr)) {
      ut_ad(0);
    }
    mtr_commit(&mtr);

    rw_lock_x_lock(&space->latch);
    /* Reset In-mem encryption for tablespace */
    err = fil_reset_encryption(space_id);
    rw_lock_x_unlock(&space->latch);
    ut_ad(err == DB_SUCCESS);
  }

  /* Reset encryption in progress flag */
  space->encryption_op_in_progress = NONE;

  if (!in_recovery) {
    ut_ad(dd_space != nullptr);
    /* Update DD flags for tablespace */
    dd_space->se_private_data().set_uint32(dd_space_key_strings[DD_SPACE_FLAGS],
                                           static_cast<uint32>(space->flags));
  }

  /* Crash before resetting progress on page 0 */
  DBUG_EXECUTE_IF("alter_encrypt_tablespace_crash_before_resetting_progress",
                  log_buffer_flush_to_disk();
                  DBUG_SUICIDE(););

  /* Erase Operation type and encryption progress from page 0 */
  mtr_start(&mtr);
  fsp_header_write_encryption_progress(space_id, space->flags, 0, 0, true,
                                       &mtr);
  mtr_commit(&mtr);

  /* Crash before flushing page 0 on disk */
  DBUG_EXECUTE_IF("alter_encrypt_tablespace_crash_before_flushing_page_0",
                  log_buffer_flush_to_disk();
                  DBUG_SUICIDE(););

  /* As DMLs are allowed in parallel, pass false for 'strict' */
  buf_LRU_flush_or_remove_pages(space_id, BUF_REMOVE_FLUSH_WRITE, 0, false);

  /* Crash after flushing page 0 on disk */
  DBUG_EXECUTE_IF("alter_encrypt_tablespace_crash_after_flushing_page_0",
                  log_buffer_flush_to_disk();
                  DBUG_SUICIDE(););

  DBUG_RETURN(err);
}

#ifdef UNIV_DEBUG
/** Validate tablespace encryption settings. */
static void validate_tablespace_encryption(fil_space_t *space) {
  byte buf[ENCRYPTION_KEY_LEN];
  memset(buf, 0, ENCRYPTION_KEY_LEN);

  if (FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
    ut_ad(memcmp(space->encryption_key, buf, ENCRYPTION_KEY_LEN) != 0);
    ut_ad(memcmp(space->encryption_iv, buf, ENCRYPTION_KEY_LEN) != 0);
    ut_ad(space->encryption_klen != 0);
    ut_ad(space->encryption_type == Encryption::AES);
  } else {
    ut_ad(memcmp(space->encryption_key, buf, ENCRYPTION_KEY_LEN) == 0);
    ut_ad(memcmp(space->encryption_iv, buf, ENCRYPTION_KEY_LEN) == 0);
    ut_ad(space->encryption_klen == 0);
    ut_ad(space->encryption_type == Encryption::NONE);
  }
  ut_ad(space->encryption_op_in_progress == NONE);
}
#endif

/** Resume Encrypt/Unencrypt for tablespace(s) post recovery.
@param[in]	thd	background thread
@return 0 for success, otherwise error code */
static dberr_t resume_alter_encrypt_tablespace(THD *thd) {
  dberr_t err = DB_SUCCESS;
  mtr_t mtr;
  char operation_name[3][20] = {"NONE", "ENCRYPTION", "UNENCRYPTION"};
  /* List of MDLs taken. One for each tablespace. */
  std::list<MDL_ticket *> shared_mdl_list;

  Disable_autocommit_guard autocommit_guard(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);
  dd::Tablespace *recv_dd_space = nullptr;

  /* Take a SHARED MDL to make sure no one could run any DDL on it and DMLs
  are allowed. */
  for (auto it : ts_encrypt_ddl_records) {
    /* Get the space_id and then read page0 to get
    (un)encryption progress */
    space_id_t space_id = it->get_space_id();
    fil_space_t *space = fil_space_get(space_id);
    if (space == nullptr) {
      continue;
    }

    MDL_ticket *mdl_ticket;
    if (dd::acquire_shared_tablespace_mdl(thd, space->name, false, &mdl_ticket,
                                          false)) {
      ut_a(false);
    }
    shared_mdl_list.push_back(mdl_ticket);
  }

  /* Let the startup thread proceed now */
  mysql_cond_signal(&resume_encryption_cond);

  /* In following loop :
      - traverse every tablespace one by one and roll forward (un)encryption
        operation.
      - remove EXPLICIT MDL taken on tablespace explicitly */
  std::list<MDL_ticket *>::iterator mdl_it = shared_mdl_list.begin();
  for (auto it : ts_encrypt_ddl_records) {
    /* Get the space_id and then read page 0 to get (un)encryption progress */
    space_id_t space_id = it->get_space_id();
    fil_space_t *space = fil_space_get(space_id);
    if (space == nullptr) {
      ib::error(ER_IB_MSG_1277)
          << "Tablespace is missing for tablespace id" << space_id
          << ". Skipping (un)encryption resume operation.";
      continue;
    }

    /* MDL list must not be empty */
    ut_ad(mdl_it != shared_mdl_list.end());

    page_size_t pageSize(space->flags);

    mtr_start(&mtr);
    buf_block_t *block =
        buf_page_get(page_id_t(space_id, 0), pageSize, RW_X_LATCH, &mtr);

    page_t *page = buf_block_get_frame(block);

    /* Get the offset of Encryption progress information */
    ulint offset = fsp_header_get_encryption_progress_offset(pageSize);

    /* Read operation type (1 byte) */
    byte operation = mach_read_from_1(page + offset);

    /* Read maximum pages (4 byte) */
    uint progress =
        mach_read_from_4(page + offset + ENCRYPTION_OPERATION_INFO_SIZE);
    mtr_commit(&mtr);

    if (!(operation & ENCRYPTION_IN_PROGRESS) &&
        !(operation & UNENCRYPTION_IN_PROGRESS)) {
      /* There are two possibilities:
      1. Crash happened even before operation/progress
         was written to page 0. Nohting to do.
      2. Crash happened after (un)encryption was done and progress/operation
         was reset but before DD is updated.
      Update DD in that case. */
      ib::info(ER_IB_MSG_1278) << "No operation/progress found."
                                  " Updating DD for tablespace "
                               << space->name << ":" << space_id << ".";
      goto update_dd;
    }

    ib::info(ER_IB_MSG_1279)
        << "Resuming " << operation_name[operation] << " for tablespace "
        << space->name << ":" << space_id << " from page " << progress + 1;

    /* Resume (Un)Encryption operation next page onwards */
    err = fsp_alter_encrypt_tablespace(
        thd, space_id, progress + 1,
        (operation & ENCRYPTION_IN_PROGRESS) ? true : false, true,
        recv_dd_space);

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_1280)
          << operation_name[operation] << " for tablespace " << space->name
          << ":" << space_id << " could not be done successfully.";
      return (err);
    } else {
    update_dd:
      /* At this point, encryption/unencryption process would have been
      finished and all pages in tablespace should have been written
      correctly and flushed to disk. Now :
        - Set/Update tablespace flags encryption.
        - Remove In-mem encryption info from tablespace (If Unencrypted).
        - Reset operation in progress to NONE. */
      mtr_start(&mtr);
      buf_block_t *block =
          buf_page_get(page_id_t(space_id, 0), pageSize, RW_X_LATCH, &mtr);
      page_t *page = buf_block_get_frame(block);
      ulint latest_fsp_flags = fsp_header_get_flags(page);
      if (FSP_FLAGS_GET_ENCRYPTION(latest_fsp_flags)) {
        space->flags |= FSP_FLAGS_MASK_ENCRYPTION;
      } else {
        space->flags &= ~FSP_FLAGS_MASK_ENCRYPTION;
      }
      ut_ad(space->flags == latest_fsp_flags);
      mtr_commit(&mtr);

      if (!FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
        /* Reset In-mem encryption for tablespace */
        err = fil_reset_encryption(space_id);
        ut_ad(err == DB_SUCCESS);
      }

      space->encryption_op_in_progress = NONE;

    /* In case of crash/recovery, following has to be set explicitly
        - DD tablespace flags.
        - DD encryption option value. */
    retry:
      if (dd::acquire_exclusive_tablespace_mdl(thd, space->name, false)) {
        os_thread_sleep(20);
        goto retry;
      }

      if (client->acquire_for_modification<dd::Tablespace>(space->name,
                                                           &recv_dd_space)) {
        os_thread_sleep(20);
        goto retry;
      }

      if (!FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
        /* Update DD Option value, for Unencryption */
        recv_dd_space->options().set("encryption", "N");

      } else {
        /* Update DD Option value, for Encryption */
        recv_dd_space->options().set("encryption", "Y");
      }

      /* Update DD flags for tablespace */
      recv_dd_space->se_private_data().set_uint32(
          dd_space_key_strings[DD_SPACE_FLAGS],
          static_cast<uint32>(space->flags));

      /* Validate tablespace In-mem representation */
      ut_d(validate_tablespace_encryption(space));

      /* Pass 'true' for 'release_mdl_on_commit' parameter because we want
      transactional locks to be released only in case of successful commit */
      if (dd::commit_or_rollback_tablespace_change(thd, recv_dd_space, false,
                                                   true)) {
        os_thread_sleep(20);
        goto retry;
      }

      ib::info(ER_IB_MSG_1281)
          << "Finished " << operation_name[operation] << " for tablespace "
          << space->name << ":" << space_id << ".";
    }
    /* Release MDL on tablespace explicitly */
    dd_release_mdl((*mdl_it));
    mdl_it = shared_mdl_list.erase(mdl_it);
  }
  /* Delete DDL logs now */
  log_ddl->post_ts_encryption(ts_encrypt_ddl_records);
  ts_encrypt_ddl_records.clear();
  /* All MDLs should have been released and removed from list by now */
  ut_ad(shared_mdl_list.empty());
  shared_mdl_list.clear();
  return (err);
}

/* Initiate roll-forward of alter encrypt in background thread */
void fsp_init_resume_alter_encrypt_tablespace() {
  dberr_t err = DB_SUCCESS;

  my_thread_init();
#ifdef UNIV_PFS_THREAD
  THD *thd =
      create_thd(false, true, true, srv_ts_alter_encrypt_thread_key.m_value);
#else
  THD *thd = create_thd(false, true, true, 0);
#endif

  err = resume_alter_encrypt_tablespace(thd);
  ut_a(err == DB_SUCCESS);

  srv_threads.m_ts_alter_encrypt_thread_active = false;

  destroy_thd(thd);
  my_thread_end();

  return;
}
#endif /* !UNIV_HOTBACKUP */
