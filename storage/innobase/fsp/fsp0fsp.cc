/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/******************************************************************//**
@file fsp/fsp0fsp.cc
File space management

Created 11/29/1995 Heikki Tuuri
***********************************************************************/

#include "ha_prototypes.h"

#include "fsp0fsp.h"

#ifdef UNIV_NONINL
#include "fsp0fsp.ic"
#endif

#ifdef UNIV_HOTBACKUP
# include "fut0lst.h"
#else /* UNIV_HOTBACKUP */
#include "buf0buf.h"
#include "fil0fil.h"
#include "mtr0log.h"
#include "ut0byte.h"
#include "page0page.h"
#include "fut0fut.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "ibuf0ibuf.h"
#include "btr0btr.h"
#include "btr0sea.h"
#include "dict0boot.h"
#include "log0log.h"
#include "fsp0sysspace.h"
#include "dict0mem.h"
#include "fsp0types.h"

#include <my_aes.h>

/** Returns an extent to the free list of a space.
@param[in]	page_id		page id in the extent
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction */
static
void
fsp_free_extent(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	mtr_t*			mtr);

/********************************************************************//**
Marks a page used. The page must reside within the extents of the given
segment. */
static
void
fseg_mark_page_used(
/*================*/
	fseg_inode_t*	seg_inode,/*!< in: segment inode */
	ulint		page,	/*!< in: page offset */
	xdes_t*		descr,  /*!< in: extent descriptor */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */

/** Returns the first extent descriptor for a segment.
We think of the extent lists of the segment catenated in the order
FSEG_FULL -> FSEG_NOT_FULL -> FSEG_FREE.
@param[in]	inode		segment inode
@param[in]	space_id	space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return the first extent descriptor, or NULL if none */
static
xdes_t*
fseg_get_first_extent(
	fseg_inode_t*		inode,
	ulint			space_id,
	const page_size_t&	page_size,
	mtr_t*			mtr);

/** Put new extents to the free list if there are free extents above the free
limit. If an extent happens to contain an extent descriptor page, the extent
is put to the FSP_FREE_FRAG list with the page marked as used.
@param[in]	init_space	true if this is a single-table tablespace
and we are only initializing the first extent and the first bitmap pages;
then we will not allocate more extents
@param[in,out]	space		tablespace
@param[in,out]	header		tablespace header
@param[in,out]	mtr		mini-transaction */
static UNIV_COLD
void
fsp_fill_free_list(
	bool		init_space,
	fil_space_t*	space,
	fsp_header_t*	header,
	mtr_t*		mtr);

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
static
buf_block_t*
fseg_alloc_free_page_low(
	fil_space_t*		space,
	const page_size_t&	page_size,
	fseg_inode_t*		seg_inode,
	ulint			hint,
	byte			direction,
	rw_lock_type_t		rw_latch,
	mtr_t*			mtr,
	mtr_t*			init_mtr
#ifdef UNIV_DEBUG
	, ibool			has_done_reservation
#endif /* UNIV_DEBUG */
)
	MY_ATTRIBUTE((warn_unused_result));

/** Gets a pointer to the space header and x-locks its page.
@param[in]	id		space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return pointer to the space header, page x-locked */
UNIV_INLINE
fsp_header_t*
fsp_get_space_header(
	ulint			id,
	const page_size_t&	page_size,
	mtr_t*			mtr)
{
	buf_block_t*	block;
	fsp_header_t*	header;

	ut_ad(id != 0 || !page_size.is_compressed());

	block = buf_page_get(page_id_t(id, 0), page_size, RW_SX_LATCH, mtr);
	header = FSP_HEADER_OFFSET + buf_block_get_frame(block);
	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

	ut_ad(id == mach_read_from_4(FSP_SPACE_ID + header));
#ifdef UNIV_DEBUG
	const ulint	flags = mach_read_from_4(FSP_SPACE_FLAGS + header);
	ut_ad(page_size_t(flags).equals_to(page_size));
#endif /* UNIV_DEBUG */
	return(header);
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
ulint
fsp_flags_to_dict_tf(
	ulint	fsp_flags,
	bool	compact)
{
	/* If the table in this file-per-table tablespace is Compact
	row format, the low order bit will not indicate Compact. */
	bool	post_antelope	= FSP_FLAGS_GET_POST_ANTELOPE(fsp_flags);
	ulint	zip_ssize	= FSP_FLAGS_GET_ZIP_SSIZE(fsp_flags);
	bool	atomic_blobs	= FSP_FLAGS_HAS_ATOMIC_BLOBS(fsp_flags);
	bool	data_dir	= FSP_FLAGS_HAS_DATA_DIR(fsp_flags);
	bool	shared_space	= FSP_FLAGS_GET_SHARED(fsp_flags);
	/* FSP_FLAGS_GET_TEMPORARY(fsp_flags) does not have an equivalent
	flag position in the table flags. But it would go into flags2 if
	any code is created where that is needed. */

	ulint	flags = dict_tf_init(post_antelope | compact, zip_ssize,
				     atomic_blobs, data_dir, shared_space);

	return(flags);
}
#endif /* !UNIV_HOTBACKUP */

/** Validate the tablespace flags.
These flags are stored in the tablespace header at offset FSP_SPACE_FLAGS.
They should be 0 for ROW_FORMAT=COMPACT and ROW_FORMAT=REDUNDANT.
The newer row formats, COMPRESSED and DYNAMIC, use a file format > Antelope
so they should have a file format number plus the DICT_TF_COMPACT bit set.
@param[in]	flags	Tablespace flags
@return true if valid, false if not */
bool
fsp_flags_is_valid(
	ulint	flags)
{
	bool	post_antelope = FSP_FLAGS_GET_POST_ANTELOPE(flags);
	ulint	zip_ssize = FSP_FLAGS_GET_ZIP_SSIZE(flags);
	bool	atomic_blobs = FSP_FLAGS_HAS_ATOMIC_BLOBS(flags);
	ulint	page_ssize = FSP_FLAGS_GET_PAGE_SSIZE(flags);
	bool	has_data_dir = FSP_FLAGS_HAS_DATA_DIR(flags);
	bool	is_shared = FSP_FLAGS_GET_SHARED(flags);
	bool	is_temp = FSP_FLAGS_GET_TEMPORARY(flags);
	bool	is_encryption = FSP_FLAGS_GET_ENCRYPTION(flags);

	ulint	unused = FSP_FLAGS_GET_UNUSED(flags);

	DBUG_EXECUTE_IF("fsp_flags_is_valid_failure", return(false););

	/* The Antelope row formats REDUNDANT and COMPACT did
	not use tablespace flags, so the entire 4-byte field
	is zero for Antelope row formats. */
	if (flags == 0) {
		return(true);
	}

	/* Barracuda row formats COMPRESSED and DYNAMIC use a feature called
	ATOMIC_BLOBS which builds on the page structure introduced for the
	COMPACT row format by allowing long fields to be broken into prefix
	and externally stored parts. So if it is Post_antelope, it uses
	Atomic BLOBs. */
	if (post_antelope != atomic_blobs) {
		return(false);
	}

	/* Make sure there are no bits that we do not know about. */
	if (unused != 0) {
		return(false);
	}

	/* The zip ssize can be zero if it is other than compressed row format,
	or it could be from 1 to the max. */
	if (zip_ssize > PAGE_ZIP_SSIZE_MAX) {
		return(false);
	}

	/* The actual page size must be within 4k and 16K (3 =< ssize =< 5). */
	if (page_ssize != 0
	    && (page_ssize < UNIV_PAGE_SSIZE_MIN
	        || page_ssize > UNIV_PAGE_SSIZE_MAX)) {
		return(false);
	}

	/* Only single-table tablespaces use the DATA DIRECTORY clause.
	It is not compatible with the TABLESPACE clause.  Nor is it
	compatible with the TEMPORARY clause. */
	if (has_data_dir && (is_shared || is_temp)) {
		return(false);
	}

	/* Only single-table and not temp tablespaces use the encryption
	clause. */
	if (is_encryption && (is_shared || is_temp)) {
		return(false);
	}

#if UNIV_FORMAT_MAX != UNIV_FORMAT_B
# error UNIV_FORMAT_MAX != UNIV_FORMAT_B, Add more validations.
#endif
#if FSP_FLAGS_POS_UNUSED != 14
# error You have added a new FSP_FLAG without adding a validation check.
#endif

	return(true);
}

/** Check if tablespace is system temporary.
@param[in]	space_id	tablespace ID
@return true if tablespace is system temporary. */
bool
fsp_is_system_temporary(
	ulint	space_id)
{
	return(space_id == srv_tmp_space.space_id());
}

/** Check if checksum is disabled for the given space.
@param[in]	space_id	tablespace ID
@return true if checksum is disabled for given space. */
bool
fsp_is_checksum_disabled(
	ulint	space_id)
{
	return(fsp_is_system_temporary(space_id));
}

/** Check if tablespace is file-per-table.
@param[in]	space_id	tablespace ID
@param[in]	fsp_flags	tablespace flags
@return true if tablespace is file-per-table. */
bool
fsp_is_file_per_table(
	ulint	space_id,
	ulint	fsp_flags)
{
	return(!is_system_tablespace(space_id)
		&& !fsp_is_shared_tablespace(fsp_flags));
}
#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG

/** Skip some of the sanity checks that are time consuming even in debug mode
and can affect frequent verification runs that are done to ensure stability of
the product.
@return true if check should be skipped for given space. */
bool
fsp_skip_sanity_check(
	ulint	space_id)
{
	return(srv_skip_temp_table_checks_debug
	       && fsp_is_system_temporary(space_id));
}

#endif /* UNIV_DEBUG */

/**********************************************************************//**
Gets a descriptor bit of a page.
@return TRUE if free */
UNIV_INLINE
ibool
xdes_mtr_get_bit(
/*=============*/
	const xdes_t*	descr,	/*!< in: descriptor */
	ulint		bit,	/*!< in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	ulint		offset,	/*!< in: page offset within extent:
				0 ... FSP_EXTENT_SIZE - 1 */
	mtr_t*		mtr)	/*!< in: mini-transaction */
{
	ut_ad(mtr->is_active());
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));

	return(xdes_get_bit(descr, bit, offset));
}

/**********************************************************************//**
Sets a descriptor bit of a page. */
UNIV_INLINE
void
xdes_set_bit(
/*=========*/
	xdes_t*	descr,	/*!< in: descriptor */
	ulint	bit,	/*!< in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	ulint	offset,	/*!< in: page offset within extent:
			0 ... FSP_EXTENT_SIZE - 1 */
	ibool	val,	/*!< in: bit value */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	ulint	index;
	ulint	byte_index;
	ulint	bit_index;
	ulint	descr_byte;

	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));
	ut_ad((bit == XDES_FREE_BIT) || (bit == XDES_CLEAN_BIT));
	ut_ad(offset < FSP_EXTENT_SIZE);

	index = bit + XDES_BITS_PER_PAGE * offset;

	byte_index = index / 8;
	bit_index = index % 8;

	descr_byte = mach_read_from_1(descr + XDES_BITMAP + byte_index);
	descr_byte = ut_bit_set_nth(descr_byte, bit_index, val);

	mlog_write_ulint(descr + XDES_BITMAP + byte_index, descr_byte,
			 MLOG_1BYTE, mtr);
}

/**********************************************************************//**
Looks for a descriptor bit having the desired value. Starts from hint
and scans upward; at the end of the extent the search is wrapped to
the start of the extent.
@return bit index of the bit, ULINT_UNDEFINED if not found */
UNIV_INLINE
ulint
xdes_find_bit(
/*==========*/
	xdes_t*	descr,	/*!< in: descriptor */
	ulint	bit,	/*!< in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	ibool	val,	/*!< in: desired bit value */
	ulint	hint,	/*!< in: hint of which bit position would
			be desirable */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	ulint	i;

	ut_ad(descr && mtr);
	ut_ad(val <= TRUE);
	ut_ad(hint < FSP_EXTENT_SIZE);
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));
	for (i = hint; i < FSP_EXTENT_SIZE; i++) {
		if (val == xdes_mtr_get_bit(descr, bit, i, mtr)) {

			return(i);
		}
	}

	for (i = 0; i < hint; i++) {
		if (val == xdes_mtr_get_bit(descr, bit, i, mtr)) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**********************************************************************//**
Returns the number of used pages in a descriptor.
@return number of pages used */
UNIV_INLINE
ulint
xdes_get_n_used(
/*============*/
	const xdes_t*	descr,	/*!< in: descriptor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	count	= 0;

	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));
	for (ulint i = 0; i < FSP_EXTENT_SIZE; ++i) {
		if (FALSE == xdes_mtr_get_bit(descr, XDES_FREE_BIT, i, mtr)) {
			count++;
		}
	}

	return(count);
}

/**********************************************************************//**
Returns true if extent contains no used pages.
@return TRUE if totally free */
UNIV_INLINE
ibool
xdes_is_free(
/*=========*/
	const xdes_t*	descr,	/*!< in: descriptor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	if (0 == xdes_get_n_used(descr, mtr)) {

		return(TRUE);
	}

	return(FALSE);
}

/**********************************************************************//**
Returns true if extent contains no free pages.
@return TRUE if full */
UNIV_INLINE
ibool
xdes_is_full(
/*=========*/
	const xdes_t*	descr,	/*!< in: descriptor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	if (FSP_EXTENT_SIZE == xdes_get_n_used(descr, mtr)) {

		return(TRUE);
	}

	return(FALSE);
}

/**********************************************************************//**
Sets the state of an xdes. */
UNIV_INLINE
void
xdes_set_state(
/*===========*/
	xdes_t*	descr,	/*!< in/out: descriptor */
	ulint	state,	/*!< in: state to set */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	ut_ad(descr && mtr);
	ut_ad(state >= XDES_FREE);
	ut_ad(state <= XDES_FSEG);
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));

	mlog_write_ulint(descr + XDES_STATE, state, MLOG_4BYTES, mtr);
}

/**********************************************************************//**
Gets the state of an xdes.
@return state */
UNIV_INLINE
ulint
xdes_get_state(
/*===========*/
	const xdes_t*	descr,	/*!< in: descriptor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	state;

	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));

	state = mach_read_from_4(descr + XDES_STATE);
	ut_ad(state - 1 < XDES_FSEG);
	return(state);
}

/**********************************************************************//**
Inits an extent descriptor to the free and clean state. */
UNIV_INLINE
void
xdes_init(
/*======*/
	xdes_t*	descr,	/*!< in: descriptor */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	ulint	i;

	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_SX_FIX));
	ut_ad((XDES_SIZE - XDES_BITMAP) % 4 == 0);

	for (i = XDES_BITMAP; i < XDES_SIZE; i += 4) {
		mlog_write_ulint(descr + i, 0xFFFFFFFFUL, MLOG_4BYTES, mtr);
	}

	xdes_set_state(descr, XDES_FREE, mtr);
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
UNIV_INLINE MY_ATTRIBUTE((warn_unused_result))
xdes_t*
xdes_get_descriptor_with_space_hdr(
	fsp_header_t*	sp_header,
	ulint		space,
	ulint		offset,
	mtr_t*		mtr,
	bool		init_space = false,
	buf_block_t**	desc_block = NULL)
{
	ulint	limit;
	ulint	size;
	ulint	descr_page_no;
	ulint	flags;
	page_t*	descr_page;
#ifdef UNIV_DEBUG
	const fil_space_t*	fspace = fil_space_get(space);
	ut_ad(fspace != NULL);
#endif /* UNIV_DEBUG */
	ut_ad(mtr_memo_contains(mtr, &fspace->latch, MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains_page(mtr, sp_header, MTR_MEMO_PAGE_SX_FIX));
	ut_ad(page_offset(sp_header) == FSP_HEADER_OFFSET);
	/* Read free limit and space size */
	limit = mach_read_from_4(sp_header + FSP_FREE_LIMIT);
	size  = mach_read_from_4(sp_header + FSP_SIZE);
	flags = mach_read_from_4(sp_header + FSP_SPACE_FLAGS);
	ut_ad(limit == fspace->free_limit
	      || (fspace->free_limit == 0
		  && (init_space
		      || fspace->purpose == FIL_TYPE_TEMPORARY
		      || (srv_startup_is_before_trx_rollback_phase
			  && fspace->id <= srv_undo_tablespaces))));
	ut_ad(size == fspace->size_in_header);
	ut_ad(flags == fspace->flags);

	if ((offset >= size) || (offset >= limit)) {
		return(NULL);
	}

	const page_size_t	page_size(flags);

	descr_page_no = xdes_calc_descriptor_page(page_size, offset);

	buf_block_t*		block;

	if (descr_page_no == 0) {
		/* It is on the space header page */

		descr_page = page_align(sp_header);
		block = NULL;
	} else {
		block = buf_page_get(
			page_id_t(space, descr_page_no), page_size,
			RW_SX_LATCH, mtr);

		buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

		descr_page = buf_block_get_frame(block);
	}

	if (desc_block != NULL) {
		*desc_block = block;
	}

	return(descr_page + XDES_ARR_OFFSET
	       + XDES_SIZE * xdes_calc_descriptor_index(page_size, offset));
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
xdes_t*
xdes_get_descriptor(
	ulint			space_id,
	ulint			offset,
	const page_size_t&	page_size,
	mtr_t*			mtr)
{
	buf_block_t*	block;
	fsp_header_t*	sp_header;

	block = buf_page_get(page_id_t(space_id, 0), page_size,
			     RW_SX_LATCH, mtr);

	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

	sp_header = FSP_HEADER_OFFSET + buf_block_get_frame(block);
	return(xdes_get_descriptor_with_space_hdr(sp_header, space_id, offset,
						  mtr));
}

/********************************************************************//**
Gets pointer to a the extent descriptor if the file address
of the descriptor list node is known. The page where the
extent descriptor resides is x-locked.
@return pointer to the extent descriptor */
UNIV_INLINE
xdes_t*
xdes_lst_get_descriptor(
/*====================*/
	ulint		space,	/*!< in: space id */
	const page_size_t&	page_size,
	fil_addr_t	lst_node,/*!< in: file address of the list node
				contained in the descriptor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	xdes_t*	descr;

	ut_ad(mtr);
	ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space, NULL),
				MTR_MEMO_X_LOCK));
	descr = fut_get_ptr(space, page_size, lst_node, RW_SX_LATCH, mtr)
		- XDES_FLST_NODE;

	return(descr);
}

/********************************************************************//**
Returns page offset of the first page in extent described by a descriptor.
@return offset of the first page in extent */
UNIV_INLINE
ulint
xdes_get_offset(
/*============*/
	const xdes_t*	descr)	/*!< in: extent descriptor */
{
	ut_ad(descr);

	return(page_get_page_no(page_align(descr))
	       + ((page_offset(descr) - XDES_ARR_OFFSET) / XDES_SIZE)
	       * FSP_EXTENT_SIZE);
}
#endif /* !UNIV_HOTBACKUP */

/***********************************************************//**
Inits a file page whose prior contents should be ignored. */
static
void
fsp_init_file_page_low(
/*===================*/
	buf_block_t*	block)	/*!< in: pointer to a page */
{
	page_t*		page	= buf_block_get_frame(block);
	page_zip_des_t*	page_zip= buf_block_get_page_zip(block);

	if (!fsp_is_system_temporary(block->page.id.space())) {
		memset(page, 0, UNIV_PAGE_SIZE);
	}

	mach_write_to_4(page + FIL_PAGE_OFFSET, block->page.id.page_no());
	mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
			block->page.id.space());

	if (page_zip) {
		memset(page_zip->data, 0, page_zip_get_size(page_zip));
		memcpy(page_zip->data + FIL_PAGE_OFFSET,
		       page + FIL_PAGE_OFFSET, 4);
		memcpy(page_zip->data + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
		       page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, 4);
	}
}

#ifndef UNIV_HOTBACKUP
# ifdef UNIV_DEBUG
/** Assert that the mini-transaction is compatible with
updating an allocation bitmap page.
@param[in]	id	tablespace identifier
@param[in]	mtr	mini-transaction */
static
void
fsp_space_modify_check(
	ulint		id,
	const mtr_t*	mtr)
{
	switch (mtr->get_log_mode()) {
	case MTR_LOG_SHORT_INSERTS:
	case MTR_LOG_NONE:
		/* These modes are only allowed within a non-bitmap page
		when there is a higher-level redo log record written. */
		break;
	case MTR_LOG_NO_REDO:
#ifdef UNIV_DEBUG
		{
			const fil_type_t	type = fil_space_get_type(id);
			ut_a(id == srv_tmp_space.space_id()
			     || srv_is_tablespace_truncated(id)
			     || fil_space_is_being_truncated(id)
			     || fil_space_get_flags(id) == ULINT_UNDEFINED
			     || type == FIL_TYPE_TEMPORARY
			     || type == FIL_TYPE_IMPORT
			     || fil_space_is_redo_skipped(id));
		}
#endif /* UNIV_DEBUG */
		return;
	case MTR_LOG_ALL:
		/* We must not write redo log for the shared temporary
		tablespace. */
		ut_ad(id != srv_tmp_space.space_id());
		/* If we write redo log, the tablespace must exist. */
		ut_ad(fil_space_get_type(id) == FIL_TYPE_TABLESPACE);
		ut_ad(mtr->is_named_space(id));
		return;
	}

	ut_ad(0);
}
# endif /* UNIV_DEBUG */

/** Initialize a file page.
@param[in,out]	block	file page
@param[in,out]	mtr	mini-transaction */
static
void
fsp_init_file_page(
	buf_block_t*	block,
	mtr_t*		mtr)
{
	fsp_init_file_page_low(block);

	ut_d(fsp_space_modify_check(block->page.id.space(), mtr));
	mlog_write_initial_log_record(buf_block_get_frame(block),
				      MLOG_INIT_FILE_PAGE2, mtr);
}
#endif /* !UNIV_HOTBACKUP */

/***********************************************************//**
Parses a redo log record of a file page init.
@return end of log record or NULL */
byte*
fsp_parse_init_file_page(
/*=====================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr MY_ATTRIBUTE((unused)), /*!< in: buffer end */
	buf_block_t*	block)	/*!< in: block or NULL */
{
	ut_ad(ptr != NULL);
	ut_ad(end_ptr != NULL);

	if (block) {
		fsp_init_file_page_low(block);
	}

	return(ptr);
}

/**********************************************************************//**
Initializes the fsp system. */
void
fsp_init(void)
/*==========*/
{
	/* FSP_EXTENT_SIZE must be a multiple of page & zip size */
	ut_a(0 == (UNIV_PAGE_SIZE % FSP_EXTENT_SIZE));
	ut_a(UNIV_PAGE_SIZE);

#if UNIV_PAGE_SIZE_MAX % FSP_EXTENT_SIZE_MAX
# error "UNIV_PAGE_SIZE_MAX % FSP_EXTENT_SIZE_MAX != 0"
#endif
#if UNIV_ZIP_SIZE_MIN % FSP_EXTENT_SIZE_MIN
# error "UNIV_ZIP_SIZE_MIN % FSP_EXTENT_SIZE_MIN != 0"
#endif

	/* Does nothing at the moment */
}

/**********************************************************************//**
Writes the space id and flags to a tablespace header.  The flags contain
row type, physical/compressed page size, and logical/uncompressed page
size of the tablespace. */
void
fsp_header_init_fields(
/*===================*/
	page_t*	page,		/*!< in/out: first page in the space */
	ulint	space_id,	/*!< in: space id */
	ulint	flags)		/*!< in: tablespace flags (FSP_SPACE_FLAGS) */
{
	ut_a(fsp_flags_is_valid(flags));

	mach_write_to_4(FSP_HEADER_OFFSET + FSP_SPACE_ID + page,
			space_id);
	mach_write_to_4(FSP_HEADER_OFFSET + FSP_SPACE_FLAGS + page,
			flags);
}

#ifndef UNIV_HOTBACKUP
/** Get the offset of encrytion information in page 0.
@param[in]	page_size	page size.
@return	offset on success, otherwise 0. */
static
ulint
fsp_header_get_encryption_offset(
	const page_size_t&	page_size)
{
	ulint	offset;
#ifdef UNIV_DEBUG
	ulint	left_size;
#endif

	offset = XDES_ARR_OFFSET + XDES_SIZE * xdes_arr_size(page_size);
#ifdef UNIV_DEBUG
	left_size = page_size.physical() - FSP_HEADER_OFFSET - offset
		- FIL_PAGE_DATA_END;

	ut_ad(left_size >= ENCRYPTION_INFO_SIZE_V2);
#endif

	return offset;
}

/** Fill the encryption info.
@param[in]	space		tablespace
@param[in,out]	encrypt_info	buffer for encrypt key.
@return true if success. */
bool
fsp_header_fill_encryption_info(
	fil_space_t*		space,
	byte*			encrypt_info)
{
	byte*			ptr;
	lint			elen;
	ulint			master_key_id;
	byte*			master_key;
	byte			key_info[ENCRYPTION_KEY_LEN * 2];
	ulint			crc;
	Encryption::Version	version;
#ifdef	UNIV_ENCRYPT_DEBUG
	const byte*		data;
	ulint			i;
#endif

	/* Get master key from key ring */
	Encryption::get_master_key(&master_key_id, &master_key, &version);
	if (master_key == NULL) {
		return(false);
	}

	memset(encrypt_info, 0, ENCRYPTION_INFO_SIZE_V2);
	memset(key_info, 0, ENCRYPTION_KEY_LEN * 2);

	/* Use the new master key to encrypt the tablespace
	key. */
	ut_ad(encrypt_info != NULL);
	ptr = encrypt_info;

	/* Write magic header. */
	if (version == Encryption::ENCRYPTION_VERSION_1) {
		memcpy(ptr, ENCRYPTION_KEY_MAGIC_V1, ENCRYPTION_MAGIC_SIZE);
	} else {
		memcpy(ptr, ENCRYPTION_KEY_MAGIC_V2, ENCRYPTION_MAGIC_SIZE);
	}
	ptr += ENCRYPTION_MAGIC_SIZE;

	/* Write master key id. */
	mach_write_to_4(ptr, master_key_id);
	ptr += sizeof(ulint);

	/* Write server uuid. */
	if (version == Encryption::ENCRYPTION_VERSION_2) {
		memcpy(ptr, Encryption::uuid, ENCRYPTION_SERVER_UUID_LEN);
		ptr += ENCRYPTION_SERVER_UUID_LEN;
	}

	/* Write tablespace key to temp space. */
	memcpy(key_info,
	       space->encryption_key,
	       ENCRYPTION_KEY_LEN);

	/* Write tablespace iv to temp space. */
	memcpy(key_info + ENCRYPTION_KEY_LEN,
	       space->encryption_iv,
	       ENCRYPTION_KEY_LEN);

#ifdef	UNIV_ENCRYPT_DEBUG
	fprintf(stderr, "Set %lu:%lu ",space->id,
		Encryption::master_key_id);
	for (data = (const byte*) master_key, i = 0;
	     i < ENCRYPTION_KEY_LEN; i++)
		fprintf(stderr, "%02lx", (ulong)*data++);
	fprintf(stderr, " ");
	for (data = (const byte*) space->encryption_key,
	     i = 0; i < ENCRYPTION_KEY_LEN; i++)
		fprintf(stderr, "%02lx", (ulong)*data++);
	fprintf(stderr, " ");
	for (data = (const byte*) space->encryption_iv,
	     i = 0; i < ENCRYPTION_KEY_LEN; i++)
		fprintf(stderr, "%02lx", (ulong)*data++);
	fprintf(stderr, "\n");
#endif
	/* Encrypt tablespace key and iv. */
	elen = my_aes_encrypt(
		key_info,
		ENCRYPTION_KEY_LEN * 2,
		ptr,
		master_key,
		ENCRYPTION_KEY_LEN,
		my_aes_256_ecb,
		NULL, false);

	if (elen == MY_AES_BAD_DATA) {
		my_free(master_key);
		return(false);
	}

	ptr += ENCRYPTION_KEY_LEN * 2;

	/* Write checksum bytes. */
	crc = ut_crc32(key_info, ENCRYPTION_KEY_LEN * 2);
	mach_write_to_4(ptr, crc);

	my_free(master_key);
	return(true);
}

/** Rotate the encryption info in the space header.
@param[in]	space		tablespace
@param[in]      encrypt_info	buffer for re-encrypt key.
@param[in,out]	mtr		mini-transaction
@return true if success. */
bool
fsp_header_rotate_encryption(
	fil_space_t*		space,
	byte*			encrypt_info,
	mtr_t*			mtr)
{
	buf_block_t*	block;
	ulint		offset;
	page_t*		page;
	ulint		master_key_id;

	ut_ad(mtr);
	ut_ad(space->encryption_type != Encryption::NONE);

	const page_size_t	page_size(space->flags);

	DBUG_EXECUTE_IF("fsp_header_rotate_encryption_failure",
			return(false););

	/* Fill encryption info. */
	if (!fsp_header_fill_encryption_info(space,
					     encrypt_info)) {
		return(false);
	}

	/* Save the encryption info to the page 0. */
	block = buf_page_get(page_id_t(space->id, 0),
			     page_size,
			     RW_SX_LATCH, mtr);
	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);
	ut_ad(space->id == page_get_space_id(buf_block_get_frame(block)));

	offset = fsp_header_get_encryption_offset(page_size);
	ut_ad(offset != 0 && offset < UNIV_PAGE_SIZE);

	page = buf_block_get_frame(block);

	/* If is in recovering, skip all master key id is rotated
	tablespaces. */
	master_key_id = mach_read_from_4(
		page + offset + ENCRYPTION_MAGIC_SIZE);
	if (recv_recovery_is_on()
	    && master_key_id == Encryption::master_key_id) {
		ut_ad(memcmp(page + offset,
			     ENCRYPTION_KEY_MAGIC_V1,
			     ENCRYPTION_MAGIC_SIZE) == 0
		      || memcmp(page + offset,
				ENCRYPTION_KEY_MAGIC_V2,
				ENCRYPTION_MAGIC_SIZE) == 0);
		return(true);
	}

	mlog_write_string(page + offset,
			  encrypt_info,
			  ENCRYPTION_INFO_SIZE_V2,
			  mtr);

	return(true);
}

/** Initializes the space header of a new created space and creates also the
insert buffer tree root if space == 0.
@param[in]	space_id	space id
@param[in]	size		current size in blocks
@param[in,out]	mtr		min-transaction
@return	true on success, otherwise false. */
bool
fsp_header_init(
	ulint	space_id,
	ulint	size,
	mtr_t*	mtr)
{
	fsp_header_t*	header;
	buf_block_t*	block;
	page_t*		page;

	ut_ad(mtr);

	fil_space_t*		space	= mtr_x_lock_space(space_id, mtr);

	const page_id_t		page_id(space_id, 0);
	const page_size_t	page_size(space->flags);

	block = buf_page_create(page_id, page_size, mtr);
	buf_page_get(page_id, page_size, RW_SX_LATCH, mtr);
	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

	space->size_in_header = size;
	space->free_len = 0;
	space->free_limit = 0;

	/* The prior contents of the file page should be ignored */

	fsp_init_file_page(block, mtr);
	page = buf_block_get_frame(block);

	mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_TYPE_FSP_HDR,
			 MLOG_2BYTES, mtr);

	header = FSP_HEADER_OFFSET + page;

	mlog_write_ulint(header + FSP_SPACE_ID, space_id, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_NOT_USED, 0, MLOG_4BYTES, mtr);

	mlog_write_ulint(header + FSP_SIZE, size, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_FREE_LIMIT, 0, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_SPACE_FLAGS, space->flags,
			 MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_FRAG_N_USED, 0, MLOG_4BYTES, mtr);

	flst_init(header + FSP_FREE, mtr);
	flst_init(header + FSP_FREE_FRAG, mtr);
	flst_init(header + FSP_FULL_FRAG, mtr);
	flst_init(header + FSP_SEG_INODES_FULL, mtr);
	flst_init(header + FSP_SEG_INODES_FREE, mtr);

	mlog_write_ull(header + FSP_SEG_ID, 1, mtr);

	fsp_fill_free_list(!is_system_tablespace(space_id),
			   space, header, mtr);

	/* For encryption tablespace, we need to save the encryption
	info to the page 0. */
	if (FSP_FLAGS_GET_ENCRYPTION(space->flags)) {
		ulint	offset = fsp_header_get_encryption_offset(page_size);
		byte	encryption_info[ENCRYPTION_INFO_SIZE_V2];

		if (offset == 0)
			return(false);

		if (!fsp_header_fill_encryption_info(space,
						     encryption_info)) {
			space->encryption_type = Encryption::NONE;
			memset(space->encryption_key, 0, ENCRYPTION_KEY_LEN);
			memset(space->encryption_iv, 0, ENCRYPTION_KEY_LEN);
			return(false);
		}

		mlog_write_string(page + offset,
				  encryption_info,
				  ENCRYPTION_INFO_SIZE_V2,
				  mtr);
	}

	if (space_id == srv_sys_space.space_id()) {
		if (btr_create(DICT_CLUSTERED | DICT_IBUF,
			       0, univ_page_size, DICT_IBUF_ID_MIN + space_id,
			       dict_ind_redundant, NULL, mtr) == FIL_NULL) {
			return(false);
		}
	}

	return(true);
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Reads the space id from the first page of a tablespace.
@return space id, ULINT UNDEFINED if error */
ulint
fsp_header_get_space_id(
/*====================*/
	const page_t*	page)	/*!< in: first page of a tablespace */
{
	ulint	fsp_id;
	ulint	id;

	fsp_id = mach_read_from_4(FSP_HEADER_OFFSET + page + FSP_SPACE_ID);

	id = mach_read_from_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

	DBUG_EXECUTE_IF("fsp_header_get_space_id_failure",
			id = ULINT_UNDEFINED;);

	if (id != fsp_id) {
		ib::error() << "Space ID in fsp header is " << fsp_id
			<< ", but in the page header it is " << id << ".";
		return(ULINT_UNDEFINED);
	}

	return(id);
}

/** Reads the page size from the first page of a tablespace.
@param[in]	page	first page of a tablespace
@return page size */
page_size_t
fsp_header_get_page_size(
	const page_t*	page)
{
	return(page_size_t(fsp_header_get_flags(page)));
}

/** Decoding the encryption info
from the first page of a tablespace.
@param[in/out]	key		key
@param[in/out]	iv		iv
@param[in]	encryption_info	encrytion info.
@return true if success */
bool
fsp_header_decode_encryption_info(
	byte*		key,
	byte*		iv,
	byte*		encryption_info)
{
	byte*			ptr;
	ulint			master_key_id;
	byte*			master_key = NULL;
	lint			elen;
	byte			key_info[ENCRYPTION_KEY_LEN * 2];
	ulint			crc1;
	ulint			crc2;
	char			srv_uuid[ENCRYPTION_SERVER_UUID_LEN + 1];
	Encryption::Version	version;
#ifdef	UNIV_ENCRYPT_DEBUG
	const byte*		data;
	ulint			i;
#endif

	ptr = encryption_info;

	/* For compatibility with 5.7.11, we need to handle the
	encryption information which created in this old version. */
	if (memcmp(ptr, ENCRYPTION_KEY_MAGIC_V1,
		     ENCRYPTION_MAGIC_SIZE) == 0) {
		version = Encryption::ENCRYPTION_VERSION_1;
	} else {
		version = Encryption::ENCRYPTION_VERSION_2;
	}

	/* Check magic. */
	if (version == Encryption::ENCRYPTION_VERSION_2
	    && memcmp(ptr, ENCRYPTION_KEY_MAGIC_V2, ENCRYPTION_MAGIC_SIZE) != 0) {
		/* We ignore report error for recovery,
		since the encryption info maybe hasn't writen
		into datafile when the table is newly created. */
		if (!recv_recovery_is_on()) {
			return(false);
		} else {
			return(true);
		}
	}
	ptr += ENCRYPTION_MAGIC_SIZE;

	/* Get master key id. */
	master_key_id = mach_read_from_4(ptr);
	ptr += sizeof(ulint);

	/* Get server uuid. */
	if (version == Encryption::ENCRYPTION_VERSION_2) {
		memset(srv_uuid, 0, ENCRYPTION_SERVER_UUID_LEN + 1);
		memcpy(srv_uuid, ptr, ENCRYPTION_SERVER_UUID_LEN);
		ptr += ENCRYPTION_SERVER_UUID_LEN;
	}

	/* Get master key by key id. */
	memset(key_info, 0, ENCRYPTION_KEY_LEN * 2);
	if (version == Encryption::ENCRYPTION_VERSION_1) {
		Encryption::get_master_key(master_key_id, NULL, &master_key);
	} else {
		Encryption::get_master_key(master_key_id, srv_uuid, &master_key);
	}
        if (master_key == NULL) {
                return(false);
        }

#ifdef	UNIV_ENCRYPT_DEBUG
	fprintf(stderr, "%lu ", master_key_id);
	for (data = (const byte*) master_key, i = 0;
	     i < ENCRYPTION_KEY_LEN; i++)
		fprintf(stderr, "%02lx", (ulong)*data++);
#endif

	/* Decrypt tablespace key and iv. */
	elen = my_aes_decrypt(
		ptr,
		ENCRYPTION_KEY_LEN * 2,
		key_info,
		master_key,
		ENCRYPTION_KEY_LEN,
		my_aes_256_ecb, NULL, false);

	if (elen == MY_AES_BAD_DATA) {
		my_free(master_key);
		return(NULL);
	}

	/* Check checksum bytes. */
	ptr += ENCRYPTION_KEY_LEN * 2;

	crc1 = mach_read_from_4(ptr);
	crc2 = ut_crc32(key_info, ENCRYPTION_KEY_LEN * 2);
	if (crc1 != crc2) {
		ib::error() << "Failed to decrypt encryption information,"
			<< " please confirm the master key was not changed.";
		my_free(master_key);
		return(false);
	}

	/* Get tablespace key */
	memcpy(key, key_info, ENCRYPTION_KEY_LEN);

	/* Get tablespace iv */
	memcpy(iv, key_info + ENCRYPTION_KEY_LEN,
	       ENCRYPTION_KEY_LEN);

#ifdef	UNIV_ENCRYPT_DEBUG
	fprintf(stderr, " ");
	for (data = (const byte*) key,
	     i = 0; i < ENCRYPTION_KEY_LEN; i++)
		fprintf(stderr, "%02lx", (ulong)*data++);
	fprintf(stderr, " ");
	for (data = (const byte*) iv,
	     i = 0; i < ENCRYPTION_KEY_LEN; i++)
		fprintf(stderr, "%02lx", (ulong)*data++);
	fprintf(stderr, "\n");
#endif

	my_free(master_key);

	if (Encryption::master_key_id < master_key_id) {
		Encryption::master_key_id = master_key_id;
		memcpy(Encryption::uuid, srv_uuid, ENCRYPTION_SERVER_UUID_LEN);
	}

	return(true);
}

/** Reads the encryption key from the first page of a tablespace.
@param[in]	fsp_flags	tablespace flags
@param[in/out]	key		tablespace key
@param[in/out]	iv		tablespace iv
@param[in]	page	first page of a tablespace
@return true if success */
bool
fsp_header_get_encryption_key(
	ulint		fsp_flags,
	byte*		key,
	byte*		iv,
	page_t*		page)
{
	ulint			offset;
	const page_size_t	page_size(fsp_flags);

	offset = fsp_header_get_encryption_offset(page_size);
	if (offset == 0) {
		return(false);
	}

	return(fsp_header_decode_encryption_info(key, iv, page + offset));
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Increases the space size field of a space. */
void
fsp_header_inc_size(
/*================*/
	ulint	space_id,	/*!< in: space id */
	ulint	size_inc,	/*!< in: size increment in pages */
	mtr_t*	mtr)		/*!< in/out: mini-transaction */
{
	fsp_header_t*	header;
	ulint		size;

	ut_ad(mtr);

	fil_space_t*	space = mtr_x_lock_space(space_id, mtr);
	ut_d(fsp_space_modify_check(space_id, mtr));

	header = fsp_get_space_header(
		space_id, page_size_t(space->flags), mtr);

	size = mach_read_from_4(header + FSP_SIZE);
	ut_ad(size == space->size_in_header);

	size += size_inc;

	mlog_write_ulint(header + FSP_SIZE, size, MLOG_4BYTES, mtr);
	space->size_in_header = size;
}

/**********************************************************************//**
Gets the size of the system tablespace from the tablespace header.  If
we do not have an auto-extending data file, this should be equal to
the size of the data files.  If there is an auto-extending data file,
this can be smaller.
@return size in pages */
ulint
fsp_header_get_tablespace_size(void)
/*================================*/
{
	fsp_header_t*	header;
	ulint		size;
	mtr_t		mtr;

	mtr_start(&mtr);

#ifdef UNIV_DEBUG
	fil_space_t*	space =
#endif /* UNIV_DEBUG */
	mtr_x_lock_space(TRX_SYS_SPACE, &mtr);

	header = fsp_get_space_header(TRX_SYS_SPACE, univ_page_size, &mtr);

	size = mach_read_from_4(header + FSP_SIZE);
	ut_ad(space->size_in_header == size);

	mtr_commit(&mtr);

	return(size);
}

/** Try to extend a single-table tablespace so that a page would fit in the
data file.
@param[in,out]	space	tablespace
@param[in]	page_no	page number
@param[in,out]	header	tablespace header
@param[in,out]	mtr	mini-transaction
@return true if success */
static UNIV_COLD MY_ATTRIBUTE((warn_unused_result))
bool
fsp_try_extend_data_file_with_pages(
	fil_space_t*	space,
	ulint		page_no,
	fsp_header_t*	header,
	mtr_t*		mtr)
{
	bool	success;
	ulint	size;

	ut_a(!is_system_tablespace(space->id));
	ut_d(fsp_space_modify_check(space->id, mtr));

	size = mach_read_from_4(header + FSP_SIZE);
	ut_ad(size == space->size_in_header);

	ut_a(page_no >= size);

	success = fil_space_extend(space, page_no + 1);
	/* The size may be less than we wanted if we ran out of disk space. */
	mlog_write_ulint(header + FSP_SIZE, space->size, MLOG_4BYTES, mtr);
	space->size_in_header = space->size;

	return(success);
}

/** Try to extend the last data file of a tablespace if it is auto-extending.
@param[in,out]	space	tablespace
@param[in,out]	header	tablespace header
@param[in,out]	mtr	mini-transaction
@return whether the tablespace was extended */
static UNIV_COLD
ulint
fsp_try_extend_data_file(
	fil_space_t*	space,
	fsp_header_t*	header,
	mtr_t*		mtr)
{
	ulint	size;		/* current number of pages in the datafile */
	ulint	size_increase;	/* number of pages to extend this file */
	const char* OUT_OF_SPACE_MSG =
		"ran out of space. Please add another file or use"
		" 'autoextend' for the last file in setting";

	ut_d(fsp_space_modify_check(space->id, mtr));

	if (space->id == srv_sys_space.space_id()
	    && !srv_sys_space.can_auto_extend_last_file()) {

		/* We print the error message only once to avoid
		spamming the error log. Note that we don't need
		to reset the flag to false as dealing with this
		error requires server restart. */
		if (!srv_sys_space.get_tablespace_full_status()) {
			ib::error() << "Tablespace " << srv_sys_space.name()
				<< " " << OUT_OF_SPACE_MSG
				<< " innodb_data_file_path.";
			srv_sys_space.set_tablespace_full_status(true);
		}
		return(false);
	} else if (fsp_is_system_temporary(space->id)
		   && !srv_tmp_space.can_auto_extend_last_file()) {

		/* We print the error message only once to avoid
		spamming the error log. Note that we don't need
		to reset the flag to false as dealing with this
		error requires server restart. */
		if (!srv_tmp_space.get_tablespace_full_status()) {
			ib::error() << "Tablespace " << srv_tmp_space.name()
				<< " " << OUT_OF_SPACE_MSG
				<< " innodb_temp_data_file_path.";
			srv_tmp_space.set_tablespace_full_status(true);
		}
		return(false);
	}

	size = mach_read_from_4(header + FSP_SIZE);
	ut_ad(size == space->size_in_header);

	const page_size_t	page_size(
		mach_read_from_4(header + FSP_SPACE_FLAGS));

	if (space->id == srv_sys_space.space_id()) {

		size_increase = srv_sys_space.get_increment();

	} else if (space->id == srv_tmp_space.space_id()) {

		size_increase = srv_tmp_space.get_increment();

	} else {
		ulint	extent_pages
			= fsp_get_extent_size_in_pages(page_size);
		if (size < extent_pages) {
			/* Let us first extend the file to extent_size */
			if (!fsp_try_extend_data_file_with_pages(
				    space, extent_pages - 1, header, mtr)) {
				return(false);
			}

			size = extent_pages;
		}

		size_increase = fsp_get_pages_to_extend_ibd(page_size, size);
	}

	if (size_increase == 0) {

		return(false);
	}

	if (!fil_space_extend(space, size + size_increase)) {
		return(false);
	}

	/* We ignore any fragments of a full megabyte when storing the size
	to the space header */

	space->size_in_header = ut_calc_align_down(
		space->size, (1024 * 1024) / page_size.physical());

	mlog_write_ulint(
		header + FSP_SIZE, space->size_in_header, MLOG_4BYTES, mtr);

	return(true);
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
ulint
fsp_get_pages_to_extend_ibd(
	const page_size_t&	page_size,
	ulint			size)
{
	ulint	size_increase;	/* number of pages to extend this file */
	ulint	extent_size;	/* one megabyte, in pages */
	ulint	threshold;	/* The size of the tablespace (in number
				of pages) where we start allocating more
				than one extent at a time. */

	extent_size = fsp_get_extent_size_in_pages(page_size);

	/* The threshold is set at 32MiB except when the physical page
	size is small enough that it must be done sooner. */
	threshold = ut_min(32 * extent_size, page_size.physical());

	if (size < threshold) {
		size_increase = extent_size;
	} else {
		/* Below in fsp_fill_free_list() we assume
		that we add at most FSP_FREE_ADD extents at
		a time */
		size_increase = FSP_FREE_ADD * extent_size;
	}

	return(size_increase);
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
static
void
fsp_fill_free_list(
	bool		init_space,
	fil_space_t*	space,
	fsp_header_t*	header,
	mtr_t*		mtr)
{
	ulint	limit;
	ulint	size;
	ulint	flags;
	xdes_t*	descr;
	ulint	count		= 0;
	ulint	frag_n_used;
	ulint	i;

	ut_ad(page_offset(header) == FSP_HEADER_OFFSET);
	ut_d(fsp_space_modify_check(space->id, mtr));

	/* Check if we can fill free list from above the free list limit */
	size = mach_read_from_4(header + FSP_SIZE);
	limit = mach_read_from_4(header + FSP_FREE_LIMIT);
	flags = mach_read_from_4(header + FSP_SPACE_FLAGS);

	ut_ad(size == space->size_in_header);
	ut_ad(limit == space->free_limit);
	ut_ad(flags == space->flags);

	const page_size_t	page_size(flags);

	if (size < limit + FSP_EXTENT_SIZE * FSP_FREE_ADD) {
		if ((!init_space && !is_system_tablespace(space->id))
		    || (space->id == srv_sys_space.space_id()
			&& srv_sys_space.can_auto_extend_last_file())
		    || (space->id == srv_tmp_space.space_id()
			&& srv_tmp_space.can_auto_extend_last_file())) {
			fsp_try_extend_data_file(space, header, mtr);
			size = space->size_in_header;
		}
	}

	i = limit;

	while ((init_space && i < 1)
	       || ((i + FSP_EXTENT_SIZE <= size) && (count < FSP_FREE_ADD))) {

		bool	init_xdes
			= (ut_2pow_remainder(i, page_size.physical()) == 0);

		space->free_limit = i + FSP_EXTENT_SIZE;
		mlog_write_ulint(header + FSP_FREE_LIMIT, i + FSP_EXTENT_SIZE,
				 MLOG_4BYTES, mtr);

		if (init_xdes) {

			buf_block_t*	block;

			/* We are going to initialize a new descriptor page
			and a new ibuf bitmap page: the prior contents of the
			pages should be ignored. */

			if (i > 0) {
				const page_id_t	page_id(space->id, i);

				block = buf_page_create(
					page_id, page_size, mtr);

				buf_page_get(
					page_id, page_size, RW_SX_LATCH, mtr);

				buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

				fsp_init_file_page(block, mtr);
				mlog_write_ulint(buf_block_get_frame(block)
						 + FIL_PAGE_TYPE,
						 FIL_PAGE_TYPE_XDES,
						 MLOG_2BYTES, mtr);
			}

			/* Initialize the ibuf bitmap page in a separate
			mini-transaction because it is low in the latching
			order, and we must be able to release its latch.
			Note: Insert-Buffering is disabled for tables that
			reside in the temp-tablespace. */
			if (space->id != srv_tmp_space.space_id()) {
				mtr_t	ibuf_mtr;

				mtr_start(&ibuf_mtr);
				ibuf_mtr.set_named_space(space);

				/* Avoid logging while truncate table
				fix-up is active. */
				if (space->purpose == FIL_TYPE_TEMPORARY
				    || srv_is_tablespace_truncated(
					    space->id)) {
					mtr_set_log_mode(
						&ibuf_mtr, MTR_LOG_NO_REDO);
				}

				const page_id_t	page_id(
					space->id,
					i + FSP_IBUF_BITMAP_OFFSET);

				block = buf_page_create(
					page_id, page_size, &ibuf_mtr);

				buf_page_get(
					page_id, page_size, RW_SX_LATCH,
					&ibuf_mtr);

				buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

				fsp_init_file_page(block, &ibuf_mtr);

				ibuf_bitmap_page_init(block, &ibuf_mtr);

				mtr_commit(&ibuf_mtr);
			}
		}

		buf_block_t*	desc_block = NULL;
		descr = xdes_get_descriptor_with_space_hdr(
			header, space->id, i, mtr, init_space, &desc_block);
		if (desc_block != NULL) {
			fil_block_check_type(
				desc_block, FIL_PAGE_TYPE_XDES, mtr);
		}
		xdes_init(descr, mtr);

		if (UNIV_UNLIKELY(init_xdes)) {

			/* The first page in the extent is a descriptor page
			and the second is an ibuf bitmap page: mark them
			used */

			xdes_set_bit(descr, XDES_FREE_BIT, 0, FALSE, mtr);
			xdes_set_bit(descr, XDES_FREE_BIT,
				     FSP_IBUF_BITMAP_OFFSET, FALSE, mtr);
			xdes_set_state(descr, XDES_FREE_FRAG, mtr);

			flst_add_last(header + FSP_FREE_FRAG,
				      descr + XDES_FLST_NODE, mtr);
			frag_n_used = mach_read_from_4(
				header + FSP_FRAG_N_USED);
			mlog_write_ulint(header + FSP_FRAG_N_USED,
					 frag_n_used + 2, MLOG_4BYTES, mtr);
		} else {
			flst_add_last(header + FSP_FREE,
				      descr + XDES_FLST_NODE, mtr);
			count++;
		}

		i += FSP_EXTENT_SIZE;
	}

	space->free_len += count;
}

/** Allocates a new free extent.
@param[in]	space_id	tablespace identifier
@param[in]	page_size	page size
@param[in]	hint		hint of which extent would be desirable: any
page offset in the extent goes; the hint must not be > FSP_FREE_LIMIT
@param[in,out]	mtr		mini-transaction
@return extent descriptor, NULL if cannot be allocated */
static
xdes_t*
fsp_alloc_free_extent(
	ulint			space_id,
	const page_size_t&	page_size,
	ulint			hint,
	mtr_t*			mtr)
{
	fsp_header_t*	header;
	fil_addr_t	first;
	xdes_t*		descr;
	buf_block_t*	desc_block = NULL;

	header = fsp_get_space_header(space_id, page_size, mtr);

	descr = xdes_get_descriptor_with_space_hdr(
		header, space_id, hint, mtr, false, &desc_block);

	fil_space_t*	space = fil_space_get(space_id);
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

			return(NULL);	/* No free extents left */
		}

		descr = xdes_lst_get_descriptor(
			space_id, page_size, first, mtr);
	}

	flst_remove(header + FSP_FREE, descr + XDES_FLST_NODE, mtr);
	space->free_len--;

	return(descr);
}

/**********************************************************************//**
Allocates a single free page from a space. */
static
void
fsp_alloc_from_free_frag(
/*=====================*/
	fsp_header_t*	header,	/*!< in/out: tablespace header */
	xdes_t*		descr,	/*!< in/out: extent descriptor */
	ulint		bit,	/*!< in: slot to allocate in the extent */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint		frag_n_used;

	ut_ad(xdes_get_state(descr, mtr) == XDES_FREE_FRAG);
	ut_a(xdes_mtr_get_bit(descr, XDES_FREE_BIT, bit, mtr));
	xdes_set_bit(descr, XDES_FREE_BIT, bit, FALSE, mtr);

	/* Update the FRAG_N_USED field */
	frag_n_used = mach_read_from_4(header + FSP_FRAG_N_USED);
	frag_n_used++;
	mlog_write_ulint(header + FSP_FRAG_N_USED, frag_n_used, MLOG_4BYTES,
			 mtr);
	if (xdes_is_full(descr, mtr)) {
		/* The fragment is full: move it to another list */
		flst_remove(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE,
			    mtr);
		xdes_set_state(descr, XDES_FULL_FRAG, mtr);

		flst_add_last(header + FSP_FULL_FRAG, descr + XDES_FLST_NODE,
			      mtr);
		mlog_write_ulint(header + FSP_FRAG_N_USED,
				 frag_n_used - FSP_EXTENT_SIZE, MLOG_4BYTES,
				 mtr);
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
static
buf_block_t*
fsp_page_create(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	rw_lock_type_t		rw_latch,
	mtr_t*			mtr,
	mtr_t*			init_mtr)
{
	buf_block_t*	block = buf_page_create(page_id, page_size, init_mtr);

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX)
	      == rw_lock_own(&block->lock, RW_LOCK_X));

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_SX_FIX)
	      == rw_lock_own(&block->lock, RW_LOCK_SX));

	ut_ad(rw_latch == RW_X_LATCH || rw_latch == RW_SX_LATCH);

	/* Mimic buf_page_get(), but avoid the buf_pool->page_hash lookup. */
	if (rw_latch == RW_X_LATCH) {
		rw_lock_x_lock(&block->lock);
	} else {
		rw_lock_sx_lock(&block->lock);
	}
	mutex_enter(&block->mutex);

	buf_block_buf_fix_inc(block, __FILE__, __LINE__);

	mutex_exit(&block->mutex);
	mtr_memo_push(init_mtr, block, rw_latch == RW_X_LATCH
		      ? MTR_MEMO_PAGE_X_FIX : MTR_MEMO_PAGE_SX_FIX);

	if (init_mtr == mtr
	    || (rw_latch == RW_X_LATCH
		? rw_lock_get_x_lock_count(&block->lock) == 1
		: rw_lock_get_sx_lock_count(&block->lock) == 1)) {

		/* Initialize the page, unless it was already
		SX-latched in mtr. (In this case, we would want to
		allocate another page that has not been freed in mtr.) */
		ut_ad(init_mtr == mtr
		      || !mtr_memo_contains_flagged(mtr, block,
						    MTR_MEMO_PAGE_X_FIX
						    | MTR_MEMO_PAGE_SX_FIX));

		fsp_init_file_page(block, init_mtr);
	}

	return(block);
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
static MY_ATTRIBUTE((warn_unused_result))
buf_block_t*
fsp_alloc_free_page(
	ulint			space,
	const page_size_t&	page_size,
	ulint			hint,
	rw_lock_type_t		rw_latch,
	mtr_t*			mtr,
	mtr_t*			init_mtr)
{
	fsp_header_t*	header;
	fil_addr_t	first;
	xdes_t*		descr;
	ulint		free;
	ulint		page_no;
	ulint		space_size;

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

			descr = fsp_alloc_free_extent(space, page_size,
						      hint, mtr);

			if (descr == NULL) {
				/* No free space left */

				return(NULL);
			}

			xdes_set_state(descr, XDES_FREE_FRAG, mtr);
			flst_add_last(header + FSP_FREE_FRAG,
				      descr + XDES_FLST_NODE, mtr);
		} else {
			descr = xdes_lst_get_descriptor(space, page_size,
							first, mtr);
		}

		/* Reset the hint */
		hint = 0;
	}

	/* Now we have in descr an extent with at least one free page. Look
	for a free page in the extent. */

	free = xdes_find_bit(descr, XDES_FREE_BIT, TRUE,
			     hint % FSP_EXTENT_SIZE, mtr);
	if (free == ULINT_UNDEFINED) {

		ut_print_buf(stderr, ((byte*) descr) - 500, 1000);
		putc('\n', stderr);

		ut_error;
	}

	page_no = xdes_get_offset(descr) + free;

	space_size = mach_read_from_4(header + FSP_SIZE);
	ut_ad(space_size == fil_space_get(space)->size_in_header
	      || (space == TRX_SYS_SPACE
		  && srv_startup_is_before_trx_rollback_phase));

	if (space_size <= page_no) {
		/* It must be that we are extending a single-table tablespace
		whose size is still < 64 pages */

		ut_a(!is_system_tablespace(space));
		if (page_no >= FSP_EXTENT_SIZE) {
			ib::error() << "Trying to extend a single-table"
				" tablespace " << space << " , by single"
				" page(s) though the space size " << space_size
				<< ". Page no " << page_no << ".";
			return(NULL);
		}

		fil_space_t*	fspace = fil_space_get(space);

		if (!fsp_try_extend_data_file_with_pages(fspace, page_no,
							 header, mtr)) {
			/* No disk space left */
			return(NULL);
		}
	}

	fsp_alloc_from_free_frag(header, descr, free, mtr);
	return(fsp_page_create(page_id_t(space, page_no), page_size,
			       rw_latch, mtr, init_mtr));
}

/** Frees a single page of a space.
The page is marked as free and clean.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction */
static
void
fsp_free_page(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	mtr_t*			mtr)
{
	fsp_header_t*	header;
	xdes_t*		descr;
	ulint		state;
	ulint		frag_n_used;

	ut_ad(mtr);
	ut_d(fsp_space_modify_check(page_id.space(), mtr));

	/* fprintf(stderr, "Freeing page %lu in space %lu\n", page, space); */

	header = fsp_get_space_header(
		page_id.space(), page_size, mtr);

	descr = xdes_get_descriptor_with_space_hdr(
		header, page_id.space(), page_id.page_no(), mtr);

	state = xdes_get_state(descr, mtr);

	if (state != XDES_FREE_FRAG && state != XDES_FULL_FRAG) {
		ib::error() << "File space extent descriptor of page "
			<< page_id << " has state " << state;
		fputs("InnoDB: Dump of descriptor: ", stderr);
		ut_print_buf(stderr, ((byte*) descr) - 50, 200);
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

		ib::error() << "File space extent descriptor of page "
			<< page_id << " says it is free. Dump of descriptor: ";
		ut_print_buf(stderr, ((byte*) descr) - 50, 200);
		putc('\n', stderr);
		/* Crash in debug version, so that we get a core dump
		of this corruption. */
		ut_ad(0);

		/* We put here some fault tolerance: if the page
		is already free, return without doing anything! */

		return;
	}

	const ulint	bit = page_id.page_no() % FSP_EXTENT_SIZE;

	xdes_set_bit(descr, XDES_FREE_BIT, bit, TRUE, mtr);
	xdes_set_bit(descr, XDES_CLEAN_BIT, bit, TRUE, mtr);

	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED, MLOG_4BYTES,
				     mtr);
	if (state == XDES_FULL_FRAG) {
		/* The fragment was full: move it to another list */
		flst_remove(header + FSP_FULL_FRAG, descr + XDES_FLST_NODE,
			    mtr);
		xdes_set_state(descr, XDES_FREE_FRAG, mtr);
		flst_add_last(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE,
			      mtr);
		mlog_write_ulint(header + FSP_FRAG_N_USED,
				 frag_n_used + FSP_EXTENT_SIZE - 1,
				 MLOG_4BYTES, mtr);
	} else {
		ut_a(frag_n_used > 0);
		mlog_write_ulint(header + FSP_FRAG_N_USED, frag_n_used - 1,
				 MLOG_4BYTES, mtr);
	}

	if (xdes_is_free(descr, mtr)) {
		/* The extent has become free: move it to another list */
		flst_remove(header + FSP_FREE_FRAG, descr + XDES_FLST_NODE,
			    mtr);
		fsp_free_extent(page_id, page_size, mtr);
	}
}

/** Returns an extent to the free list of a space.
@param[in]	page_id		page id in the extent
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction */
static
void
fsp_free_extent(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	mtr_t*			mtr)
{
	fsp_header_t*	header;
	xdes_t*		descr;

	ut_ad(mtr);

	header = fsp_get_space_header(page_id.space(), page_size, mtr);

	descr = xdes_get_descriptor_with_space_hdr(
		header, page_id.space(), page_id.page_no(), mtr);

	ut_a(xdes_get_state(descr, mtr) != XDES_FREE);

	xdes_init(descr, mtr);

	flst_add_last(header + FSP_FREE, descr + XDES_FLST_NODE, mtr);
	fil_space_get(page_id.space())->free_len++;
}

/** Returns the nth inode slot on an inode page.
@param[in]	page		segment inode page
@param[in]	i		inode index on page
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return segment inode */
UNIV_INLINE
fseg_inode_t*
fsp_seg_inode_page_get_nth_inode(
	page_t*			page,
	ulint			i,
	const page_size_t&	page_size,
	mtr_t*			mtr)
{
	ut_ad(i < FSP_SEG_INODES_PER_PAGE(page_size));
	ut_ad(mtr_memo_contains_page(mtr, page, MTR_MEMO_PAGE_SX_FIX));

	return(page + FSEG_ARR_OFFSET + FSEG_INODE_SIZE * i);
}

/** Looks for a used segment inode on a segment inode page.
@param[in]	page		segment inode page
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return segment inode index, or ULINT_UNDEFINED if not found */
static
ulint
fsp_seg_inode_page_find_used(
	page_t*			page,
	const page_size_t&	page_size,
	mtr_t*			mtr)
{
	ulint		i;
	fseg_inode_t*	inode;

	for (i = 0; i < FSP_SEG_INODES_PER_PAGE(page_size); i++) {

		inode = fsp_seg_inode_page_get_nth_inode(
			page, i, page_size, mtr);

		if (mach_read_from_8(inode + FSEG_ID)) {
			/* This is used */

			ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N)
			      == FSEG_MAGIC_N_VALUE);
			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/** Looks for an unused segment inode on a segment inode page.
@param[in]	page		segment inode page
@param[in]	i		search forward starting from this index
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return segment inode index, or ULINT_UNDEFINED if not found */
static
ulint
fsp_seg_inode_page_find_free(
	page_t*			page,
	ulint			i,
	const page_size_t&	page_size,
	mtr_t*			mtr)
{
	for (; i < FSP_SEG_INODES_PER_PAGE(page_size); i++) {

		fseg_inode_t*	inode;

		inode = fsp_seg_inode_page_get_nth_inode(
			page, i, page_size, mtr);

		if (!mach_read_from_8(inode + FSEG_ID)) {
			/* This is unused */
			return(i);
		}

		ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N)
		      == FSEG_MAGIC_N_VALUE);
	}

	return(ULINT_UNDEFINED);
}

/**********************************************************************//**
Allocates a new file segment inode page.
@return TRUE if could be allocated */
static
ibool
fsp_alloc_seg_inode_page(
/*=====================*/
	fsp_header_t*	space_header,	/*!< in: space header */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	fseg_inode_t*	inode;
	buf_block_t*	block;
	page_t*		page;
	ulint		space;

	ut_ad(page_offset(space_header) == FSP_HEADER_OFFSET);

	space = page_get_space_id(page_align(space_header));

	const page_size_t	page_size(mach_read_from_4(FSP_SPACE_FLAGS
							   + space_header));

	block = fsp_alloc_free_page(space, page_size, 0, RW_SX_LATCH, mtr, mtr);

	if (block == NULL) {

		return(FALSE);
	}

	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);
	ut_ad(rw_lock_get_sx_lock_count(&block->lock) == 1);

	page = buf_block_get_frame(block);

	mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_INODE,
			 MLOG_2BYTES, mtr);

	for (ulint i = 0; i < FSP_SEG_INODES_PER_PAGE(page_size); i++) {

		inode = fsp_seg_inode_page_get_nth_inode(
			page, i, page_size, mtr);

		mlog_write_ull(inode + FSEG_ID, 0, mtr);
	}

	flst_add_last(
		space_header + FSP_SEG_INODES_FREE,
		page + FSEG_INODE_PAGE_NODE, mtr);

	return(TRUE);
}

/**********************************************************************//**
Allocates a new file segment inode.
@return segment inode, or NULL if not enough space */
static
fseg_inode_t*
fsp_alloc_seg_inode(
/*================*/
	fsp_header_t*	space_header,	/*!< in: space header */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	buf_block_t*	block;
	page_t*		page;
	fseg_inode_t*	inode;
	ulint		n;

	ut_ad(page_offset(space_header) == FSP_HEADER_OFFSET);

	/* Allocate a new segment inode page if needed. */
	if (flst_get_len(space_header + FSP_SEG_INODES_FREE) == 0
	    && !fsp_alloc_seg_inode_page(space_header, mtr)) {
		return(NULL);
	}

	const page_size_t	page_size(
		mach_read_from_4(FSP_SPACE_FLAGS + space_header));

	const page_id_t		page_id(
		page_get_space_id(page_align(space_header)),
		flst_get_first(space_header + FSP_SEG_INODES_FREE, mtr).page);

	block = buf_page_get(page_id, page_size, RW_SX_LATCH, mtr);
	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);
	fil_block_check_type(block, FIL_PAGE_INODE, mtr);

	page = buf_block_get_frame(block);

	n = fsp_seg_inode_page_find_free(page, 0, page_size, mtr);

	ut_a(n != ULINT_UNDEFINED);

	inode = fsp_seg_inode_page_get_nth_inode(page, n, page_size, mtr);

	if (ULINT_UNDEFINED == fsp_seg_inode_page_find_free(page, n + 1,
							    page_size, mtr)) {
		/* There are no other unused headers left on the page: move it
		to another list */

		flst_remove(space_header + FSP_SEG_INODES_FREE,
			    page + FSEG_INODE_PAGE_NODE, mtr);

		flst_add_last(space_header + FSP_SEG_INODES_FULL,
			      page + FSEG_INODE_PAGE_NODE, mtr);
	}

	ut_ad(!mach_read_from_8(inode + FSEG_ID)
	      || mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
	return(inode);
}

/** Frees a file segment inode.
@param[in]	space		space id
@param[in]	page_size	page size
@param[in,out]	inode		segment inode
@param[in,out]	mtr		mini-transaction */
static
void
fsp_free_seg_inode(
	ulint			space,
	const page_size_t&	page_size,
	fseg_inode_t*		inode,
	mtr_t*			mtr)
{
	page_t*		page;
	fsp_header_t*	space_header;

	ut_d(fsp_space_modify_check(space, mtr));

	page = page_align(inode);

	space_header = fsp_get_space_header(space, page_size, mtr);

	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	if (ULINT_UNDEFINED
	    == fsp_seg_inode_page_find_free(page, 0, page_size, mtr)) {

		/* Move the page to another list */

		flst_remove(space_header + FSP_SEG_INODES_FULL,
			    page + FSEG_INODE_PAGE_NODE, mtr);

		flst_add_last(space_header + FSP_SEG_INODES_FREE,
			      page + FSEG_INODE_PAGE_NODE, mtr);
	}

	mlog_write_ull(inode + FSEG_ID, 0, mtr);
	mlog_write_ulint(inode + FSEG_MAGIC_N, 0xfa051ce3, MLOG_4BYTES, mtr);

	if (ULINT_UNDEFINED
	    == fsp_seg_inode_page_find_used(page, page_size, mtr)) {

		/* There are no other used headers left on the page: free it */

		flst_remove(space_header + FSP_SEG_INODES_FREE,
			    page + FSEG_INODE_PAGE_NODE, mtr);

		fsp_free_page(page_id_t(space, page_get_page_no(page)),
			      page_size, mtr);
	}
}

/** Returns the file segment inode, page x-latched.
@param[in]	header		segment header
@param[in]	space		space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@param[out]	block		inode block, or NULL to ignore
@return segment inode, page x-latched; NULL if the inode is free */
static
fseg_inode_t*
fseg_inode_try_get(
	fseg_header_t*		header,
	ulint			space,
	const page_size_t&	page_size,
	mtr_t*			mtr,
	buf_block_t**		block)
{
	fil_addr_t	inode_addr;
	fseg_inode_t*	inode;

	inode_addr.page = mach_read_from_4(header + FSEG_HDR_PAGE_NO);
	inode_addr.boffset = mach_read_from_2(header + FSEG_HDR_OFFSET);
	ut_ad(space == mach_read_from_4(header + FSEG_HDR_SPACE));

	inode = fut_get_ptr(space, page_size, inode_addr, RW_SX_LATCH, mtr,
			    block);

	if (UNIV_UNLIKELY(!mach_read_from_8(inode + FSEG_ID))) {

		inode = NULL;
	} else {
		ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N)
		      == FSEG_MAGIC_N_VALUE);
	}

	return(inode);
}

/** Returns the file segment inode, page x-latched.
@param[in]	header		segment header
@param[in]	space		space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@param[out]	block		inode block
@return segment inode, page x-latched */
static
fseg_inode_t*
fseg_inode_get(
	fseg_header_t*		header,
	ulint			space,
	const page_size_t&	page_size,
	mtr_t*			mtr,
	buf_block_t**		block = NULL)
{
	fseg_inode_t*	inode
		= fseg_inode_try_get(header, space, page_size, mtr, block);
	ut_a(inode);
	return(inode);
}

/**********************************************************************//**
Gets the page number from the nth fragment page slot.
@return page number, FIL_NULL if not in use */
UNIV_INLINE
ulint
fseg_get_nth_frag_page_no(
/*======================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	ulint		n,	/*!< in: slot index */
	mtr_t*		mtr MY_ATTRIBUTE((unused)))
				/*!< in/out: mini-transaction */
{
	ut_ad(inode && mtr);
	ut_ad(n < FSEG_FRAG_ARR_N_SLOTS);
	ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_SX_FIX));
	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
	return(mach_read_from_4(inode + FSEG_FRAG_ARR
				+ n * FSEG_FRAG_SLOT_SIZE));
}

/**********************************************************************//**
Sets the page number in the nth fragment page slot. */
UNIV_INLINE
void
fseg_set_nth_frag_page_no(
/*======================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	ulint		n,	/*!< in: slot index */
	ulint		page_no,/*!< in: page number to set */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ut_ad(inode && mtr);
	ut_ad(n < FSEG_FRAG_ARR_N_SLOTS);
	ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_SX_FIX));
	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	mlog_write_ulint(inode + FSEG_FRAG_ARR + n * FSEG_FRAG_SLOT_SIZE,
			 page_no, MLOG_4BYTES, mtr);
}

/**********************************************************************//**
Finds a fragment page slot which is free.
@return slot index; ULINT_UNDEFINED if none found */
static
ulint
fseg_find_free_frag_page_slot(
/*==========================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	i;
	ulint	page_no;

	ut_ad(inode && mtr);

	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		page_no = fseg_get_nth_frag_page_no(inode, i, mtr);

		if (page_no == FIL_NULL) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**********************************************************************//**
Finds a fragment page slot which is used and last in the array.
@return slot index; ULINT_UNDEFINED if none found */
static
ulint
fseg_find_last_used_frag_page_slot(
/*===============================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	i;
	ulint	page_no;

	ut_ad(inode && mtr);

	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		page_no = fseg_get_nth_frag_page_no(
			inode, FSEG_FRAG_ARR_N_SLOTS - i - 1, mtr);

		if (page_no != FIL_NULL) {

			return(FSEG_FRAG_ARR_N_SLOTS - i - 1);
		}
	}

	return(ULINT_UNDEFINED);
}

/**********************************************************************//**
Calculates reserved fragment page slots.
@return number of fragment pages */
static
ulint
fseg_get_n_frag_pages(
/*==================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	i;
	ulint	count	= 0;

	ut_ad(inode && mtr);

	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		if (FIL_NULL != fseg_get_nth_frag_page_no(inode, i, mtr)) {
			count++;
		}
	}

	return(count);
}

/**********************************************************************//**
Creates a new segment.
@return the block where the segment header is placed, x-latched, NULL
if could not create segment because of lack of space */
buf_block_t*
fseg_create_general(
/*================*/
	ulint	space_id,/*!< in: space id */
	ulint	page,	/*!< in: page where the segment header is placed: if
			this is != 0, the page must belong to another segment,
			if this is 0, a new page will be allocated and it
			will belong to the created segment */
	ulint	byte_offset, /*!< in: byte offset of the created segment header
			on the page */
	ibool	has_done_reservation, /*!< in: TRUE if the caller has already
			done the reservation for the pages with
			fsp_reserve_free_extents (at least 2 extents: one for
			the inode and the other for the segment) then there is
			no need to do the check for this individual
			operation */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	fsp_header_t*	space_header;
	fseg_inode_t*	inode;
	ib_id_t		seg_id;
	buf_block_t*	block	= 0; /* remove warning */
	fseg_header_t*	header	= 0; /* remove warning */
	ulint		n_reserved;
	ulint		i;

	DBUG_ENTER("fseg_create_general");

	ut_ad(mtr);
	ut_ad(byte_offset + FSEG_HEADER_SIZE
	      <= UNIV_PAGE_SIZE - FIL_PAGE_DATA_END);
	ut_d(fsp_space_modify_check(space_id, mtr));

	fil_space_t*		space = mtr_x_lock_space(space_id, mtr);
	const page_size_t	page_size(space->flags);

	if (page != 0) {
		block = buf_page_get(page_id_t(space_id, page), page_size,
				     RW_SX_LATCH, mtr);

		header = byte_offset + buf_block_get_frame(block);

		const ulint	type = space_id == TRX_SYS_SPACE
			&& page == TRX_SYS_PAGE_NO
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

	if (!has_done_reservation
	    && !fsp_reserve_free_extents(&n_reserved, space_id, 2,
					 FSP_NORMAL, mtr)) {
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

	mlog_write_ulint(inode + FSEG_MAGIC_N, FSEG_MAGIC_N_VALUE,
			 MLOG_4BYTES, mtr);
	for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
		fseg_set_nth_frag_page_no(inode, i, FIL_NULL, mtr);
	}

	if (page == 0) {
		block = fseg_alloc_free_page_low(space, page_size,
						 inode, 0, FSP_UP, RW_SX_LATCH,
						 mtr, mtr
#ifdef UNIV_DEBUG
						 , has_done_reservation
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

	mlog_write_ulint(header + FSEG_HDR_OFFSET,
			 page_offset(inode), MLOG_2BYTES, mtr);

	mlog_write_ulint(header + FSEG_HDR_PAGE_NO,
			 page_get_page_no(page_align(inode)),
			 MLOG_4BYTES, mtr);

	mlog_write_ulint(header + FSEG_HDR_SPACE, space_id, MLOG_4BYTES, mtr);

funct_exit:
	if (!has_done_reservation) {

		fil_space_release_free_extents(space_id, n_reserved);
	}

	DBUG_RETURN(block);
}

/**********************************************************************//**
Creates a new segment.
@return the block where the segment header is placed, x-latched, NULL
if could not create segment because of lack of space */
buf_block_t*
fseg_create(
/*========*/
	ulint	space,	/*!< in: space id */
	ulint	page,	/*!< in: page where the segment header is placed: if
			this is != 0, the page must belong to another segment,
			if this is 0, a new page will be allocated and it
			will belong to the created segment */
	ulint	byte_offset, /*!< in: byte offset of the created segment header
			on the page */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	return(fseg_create_general(space, page, byte_offset, FALSE, mtr));
}

/**********************************************************************//**
Calculates the number of pages reserved by a segment, and how many pages are
currently used.
@return number of reserved pages */
static
ulint
fseg_n_reserved_pages_low(
/*======================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	ulint*		used,	/*!< out: number of pages used (not
				more than reserved) */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	ret;

	ut_ad(inode && used && mtr);
	ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_SX_FIX));

	*used = mach_read_from_4(inode + FSEG_NOT_FULL_N_USED)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FULL)
		+ fseg_get_n_frag_pages(inode, mtr);

	ret = fseg_get_n_frag_pages(inode, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FREE)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_NOT_FULL)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FULL);

	return(ret);
}

/**********************************************************************//**
Calculates the number of pages reserved by a segment, and how many pages are
currently used.
@return number of reserved pages */
ulint
fseg_n_reserved_pages(
/*==================*/
	fseg_header_t*	header,	/*!< in: segment header */
	ulint*		used,	/*!< out: number of pages used (<= reserved) */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint		ret;
	fseg_inode_t*	inode;
	ulint		space_id;
	fil_space_t*	space;

	space_id = page_get_space_id(page_align(header));
	space = mtr_x_lock_space(space_id, mtr);

	const page_size_t	page_size(space->flags);

	inode = fseg_inode_get(header, space_id, page_size, mtr);

	ret = fseg_n_reserved_pages_low(inode, used, mtr);

	return(ret);
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
static
void
fseg_fill_free_list(
	fseg_inode_t*		inode,
	ulint			space,
	const page_size_t&	page_size,
	ulint			hint,
	mtr_t*			mtr)
{
	xdes_t*	descr;
	ulint	i;
	ib_id_t	seg_id;
	ulint	reserved;
	ulint	used;

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

		if ((descr == NULL)
		    || (XDES_FREE != xdes_get_state(descr, mtr))) {

			/* We cannot allocate the desired extent: stop */

			return;
		}

		descr = fsp_alloc_free_extent(space, page_size, hint, mtr);

		xdes_set_state(descr, XDES_FSEG, mtr);

		seg_id = mach_read_from_8(inode + FSEG_ID);
		ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N)
		      == FSEG_MAGIC_N_VALUE);
		mlog_write_ull(descr + XDES_ID, seg_id, mtr);

		flst_add_last(inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);
		hint += FSP_EXTENT_SIZE;
	}
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
static
xdes_t*
fseg_alloc_free_extent(
	fseg_inode_t*		inode,
	ulint			space,
	const page_size_t&	page_size,
	mtr_t*			mtr)
{
	xdes_t*		descr;
	ib_id_t		seg_id;
	fil_addr_t	first;

	ut_ad(!((page_offset(inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
	ut_d(fsp_space_modify_check(space, mtr));

	if (flst_get_len(inode + FSEG_FREE) > 0) {
		/* Segment free list is not empty, allocate from it */

		first = flst_get_first(inode + FSEG_FREE, mtr);

		descr = xdes_lst_get_descriptor(space, page_size, first, mtr);
	} else {
		/* Segment free list was empty, allocate from space */
		descr = fsp_alloc_free_extent(space, page_size, 0, mtr);

		if (descr == NULL) {

			return(NULL);
		}

		seg_id = mach_read_from_8(inode + FSEG_ID);

		xdes_set_state(descr, XDES_FSEG, mtr);
		mlog_write_ull(descr + XDES_ID, seg_id, mtr);
		flst_add_last(inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);

		/* Try to fill the segment free list */
		fseg_fill_free_list(inode, space, page_size,
				    xdes_get_offset(descr) + FSP_EXTENT_SIZE,
				    mtr);
	}

	return(descr);
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
static
buf_block_t*
fseg_alloc_free_page_low(
	fil_space_t*		space,
	const page_size_t&	page_size,
	fseg_inode_t*		seg_inode,
	ulint			hint,
	byte			direction,
	rw_lock_type_t		rw_latch,
	mtr_t*			mtr,
	mtr_t*			init_mtr
#ifdef UNIV_DEBUG
	, ibool			has_done_reservation
#endif /* UNIV_DEBUG */
)
{
	fsp_header_t*	space_header;
	ib_id_t		seg_id;
	ulint		used;
	ulint		reserved;
	xdes_t*		descr;		/*!< extent of the hinted page */
	ulint		ret_page;	/*!< the allocated page offset, FIL_NULL
					if could not be allocated */
	xdes_t*		ret_descr;	/*!< the extent of the allocated page */
	ulint		n;
	const ulint	space_id	= space->id;

	ut_ad(mtr);
	ut_ad((direction >= FSP_UP) && (direction <= FSP_NO_DIR));
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N)
	      == FSEG_MAGIC_N_VALUE);
	ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
	ut_ad(space->purpose == FIL_TYPE_TEMPORARY
	      || space->purpose == FIL_TYPE_TABLESPACE);
	seg_id = mach_read_from_8(seg_inode + FSEG_ID);

	ut_ad(seg_id);
	ut_d(fsp_space_modify_check(space_id, mtr));
	ut_ad(fil_page_get_type(page_align(seg_inode)) == FIL_PAGE_INODE);

	reserved = fseg_n_reserved_pages_low(seg_inode, &used, mtr);

	space_header = fsp_get_space_header(space_id, page_size, mtr);

	descr = xdes_get_descriptor_with_space_hdr(space_header, space_id,
						   hint, mtr);
	if (descr == NULL) {
		/* Hint outside space or too high above free limit: reset
		hint */
		/* The file space header page is always allocated. */
		hint = 0;
		descr = xdes_get_descriptor(space_id, hint, page_size, mtr);
	}

	/* In the big if-else below we look for ret_page and ret_descr */
	/*-------------------------------------------------------------*/
	if ((xdes_get_state(descr, mtr) == XDES_FSEG)
	    && mach_read_from_8(descr + XDES_ID) == seg_id
	    && (xdes_mtr_get_bit(descr, XDES_FREE_BIT,
				 hint % FSP_EXTENT_SIZE, mtr) == TRUE)) {
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
	} else if (xdes_get_state(descr, mtr) == XDES_FREE
		   && reserved - used < reserved / FSEG_FILLFACTOR
		   && used >= FSEG_FRAG_LIMIT) {

		/* 2. We allocate the free extent from space and can take
		=========================================================
		the hinted page
		===============*/
		ret_descr = fsp_alloc_free_extent(
			space_id, page_size, hint, mtr);

		ut_a(ret_descr == descr);

		xdes_set_state(ret_descr, XDES_FSEG, mtr);
		mlog_write_ull(ret_descr + XDES_ID, seg_id, mtr);
		flst_add_last(seg_inode + FSEG_FREE,
			      ret_descr + XDES_FLST_NODE, mtr);

		/* Try to fill the segment free list */
		fseg_fill_free_list(seg_inode, space_id, page_size,
				    hint + FSP_EXTENT_SIZE, mtr);
		goto take_hinted_page;
		/*-----------------------------------------------------------*/
	} else if ((direction != FSP_NO_DIR)
		   && ((reserved - used) < reserved / FSEG_FILLFACTOR)
		   && (used >= FSEG_FRAG_LIMIT)
		   && (!!(ret_descr
			  = fseg_alloc_free_extent(
				  seg_inode, space_id, page_size, mtr)))) {

		/* 3. We take any free extent (which was already assigned above
		===============================================================
		in the if-condition to ret_descr) and take the lowest or
		========================================================
		highest page in it, depending on the direction
		==============================================*/
		ret_page = xdes_get_offset(ret_descr);

		if (direction == FSP_DOWN) {
			ret_page += FSP_EXTENT_SIZE - 1;
		}
		ut_ad(!has_done_reservation || ret_page != FIL_NULL);
		/*-----------------------------------------------------------*/
	} else if ((xdes_get_state(descr, mtr) == XDES_FSEG)
		   && mach_read_from_8(descr + XDES_ID) == seg_id
		   && (!xdes_is_full(descr, mtr))) {

		/* 4. We can take the page from the same extent as the
		======================================================
		hinted page (and the extent already belongs to the
		==================================================
		segment)
		========*/
		ret_descr = descr;
		ret_page = xdes_get_offset(ret_descr)
			+ xdes_find_bit(ret_descr, XDES_FREE_BIT, TRUE,
					hint % FSP_EXTENT_SIZE, mtr);
		ut_ad(!has_done_reservation || ret_page != FIL_NULL);
		/*-----------------------------------------------------------*/
	} else if (reserved - used > 0) {
		/* 5. We take any unused page from the segment
		==============================================*/
		fil_addr_t	first;

		if (flst_get_len(seg_inode + FSEG_NOT_FULL) > 0) {
			first = flst_get_first(seg_inode + FSEG_NOT_FULL,
					       mtr);
		} else if (flst_get_len(seg_inode + FSEG_FREE) > 0) {
			first = flst_get_first(seg_inode + FSEG_FREE, mtr);
		} else {
			ut_ad(!has_done_reservation);
			return(NULL);
		}

		ret_descr = xdes_lst_get_descriptor(space_id, page_size,
						    first, mtr);
		ret_page = xdes_get_offset(ret_descr)
			+ xdes_find_bit(ret_descr, XDES_FREE_BIT, TRUE,
					0, mtr);
		ut_ad(!has_done_reservation || ret_page != FIL_NULL);
		/*-----------------------------------------------------------*/
	} else if (used < FSEG_FRAG_LIMIT) {
		/* 6. We allocate an individual page from the space
		===================================================*/
		buf_block_t* block = fsp_alloc_free_page(
			space_id, page_size, hint, rw_latch, mtr, init_mtr);

		ut_ad(!has_done_reservation || block != NULL);

		if (block != NULL) {
			/* Put the page in the fragment page array of the
			segment */
			n = fseg_find_free_frag_page_slot(seg_inode, mtr);
			ut_a(n != ULINT_UNDEFINED);

			fseg_set_nth_frag_page_no(
				seg_inode, n, block->page.id.page_no(),
				mtr);
		}

		/* fsp_alloc_free_page() invoked fsp_init_file_page()
		already. */
		return(block);
		/*-----------------------------------------------------------*/
	} else {
		/* 7. We allocate a new extent and take its first page
		======================================================*/
		ret_descr = fseg_alloc_free_extent(seg_inode,
						   space_id, page_size, mtr);

		if (ret_descr == NULL) {
			ret_page = FIL_NULL;
			ut_ad(!has_done_reservation);
		} else {
			ret_page = xdes_get_offset(ret_descr);
			ut_ad(!has_done_reservation || ret_page != FIL_NULL);
		}
	}

	if (ret_page == FIL_NULL) {
		/* Page could not be allocated */

		ut_ad(!has_done_reservation);
		return(NULL);
	}

	if (space->size <= ret_page && !is_system_tablespace(space_id)) {
		/* It must be that we are extending a single-table
		tablespace whose size is still < 64 pages */

		if (ret_page >= FSP_EXTENT_SIZE) {
			ib::error() << "Error (2): trying to extend"
			" a single-table tablespace " << space_id
			<< " by single page(s) though the"
			<< " space size " << space->size
			<< ". Page no " << ret_page << ".";
			ut_ad(!has_done_reservation);
			return(NULL);
		}

		if (!fsp_try_extend_data_file_with_pages(
			    space, ret_page, space_header, mtr)) {
			/* No disk space left */
			ut_ad(!has_done_reservation);
			return(NULL);
		}
	}

got_hinted_page:
	/* ret_descr == NULL if the block was allocated from free_frag
	(XDES_FREE_FRAG) */
	if (ret_descr != NULL) {
		/* At this point we know the extent and the page offset.
		The extent is still in the appropriate list (FSEG_NOT_FULL
		or FSEG_FREE), and the page is not yet marked as used. */

		ut_ad(xdes_get_descriptor(space_id, ret_page, page_size, mtr)
		      == ret_descr);

		ut_ad(xdes_mtr_get_bit(
				ret_descr, XDES_FREE_BIT,
				ret_page % FSP_EXTENT_SIZE, mtr));

		fseg_mark_page_used(seg_inode, ret_page, ret_descr, mtr);
	}

	ut_ad(space->flags
	      == mach_read_from_4(FSP_SPACE_FLAGS + space_header));
	return(fsp_page_create(page_id_t(space_id, ret_page), page_size,
			       rw_latch, mtr, init_mtr));
}

/**********************************************************************//**
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation.
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block (not allocated or initialized) otherwise */
buf_block_t*
fseg_alloc_free_page_general(
/*=========================*/
	fseg_header_t*	seg_header,/*!< in/out: segment header */
	ulint		hint,	/*!< in: hint of which page would be
				desirable */
	byte		direction,/*!< in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	ibool		has_done_reservation, /*!< in: TRUE if the caller has
				already done the reservation for the page
				with fsp_reserve_free_extents, then there
				is no need to do the check for this individual
				page */
	mtr_t*		mtr,	/*!< in/out: mini-transaction */
	mtr_t*		init_mtr)/*!< in/out: mtr or another mini-transaction
				in which the page should be initialized.
				If init_mtr!=mtr, but the page is already
				latched in mtr, do not initialize the page. */
{
	fseg_inode_t*	inode;
	ulint		space_id;
	fil_space_t*	space;
	buf_block_t*	iblock;
	buf_block_t*	block;
	ulint		n_reserved;

	space_id = page_get_space_id(page_align(seg_header));
	space = mtr_x_lock_space(space_id, mtr);
	const page_size_t	page_size(space->flags);

	if (rw_lock_get_x_lock_count(&space->latch) == 1) {
		/* This thread did not own the latch before this call: free
		excess pages from the insert buffer free list */

		if (space_id == IBUF_SPACE_ID) {
			ibuf_free_excess_pages();
		}
	}

	inode = fseg_inode_get(seg_header, space_id, page_size, mtr, &iblock);
	fil_block_check_type(iblock, FIL_PAGE_INODE, mtr);

	if (!has_done_reservation
	    && !fsp_reserve_free_extents(&n_reserved, space_id, 2,
					 FSP_NORMAL, mtr)) {
		return(NULL);
	}

	block = fseg_alloc_free_page_low(space, page_size,
					 inode, hint, direction,
					 RW_X_LATCH, mtr, init_mtr
#ifdef UNIV_DEBUG
					 , has_done_reservation
#endif /* UNIV_DEBUG */
					 );

	/* The allocation cannot fail if we have already reserved a
	space for the page. */
	ut_ad(!has_done_reservation || block != NULL);

	if (!has_done_reservation) {
		fil_space_release_free_extents(space_id, n_reserved);
	}

	return(block);
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
static
bool
fsp_reserve_free_pages(
	fil_space_t*	space,
	fsp_header_t*	space_header,
	ulint		size,
	mtr_t*		mtr,
	ulint		n_pages)
{
	xdes_t*	descr;
	ulint	n_used;

	ut_a(!is_system_tablespace(space->id));
	ut_a(size < FSP_EXTENT_SIZE);

	descr = xdes_get_descriptor_with_space_hdr(
		space_header, space->id, 0, mtr);
	n_used = xdes_get_n_used(descr, mtr);

	ut_a(n_used <= size);

	return(size >= n_used + n_pages
	       || fsp_try_extend_data_file_with_pages(
		       space, n_used + n_pages - 1, space_header, mtr));
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
bool
fsp_reserve_free_extents(
	ulint*		n_reserved,
	ulint		space_id,
	ulint		n_ext,
	fsp_reserve_t	alloc_type,
	mtr_t*		mtr,
	ulint		n_pages)
{
	fsp_header_t*	space_header;
	ulint		n_free_list_ext;
	ulint		free_limit;
	ulint		size;
	ulint		n_free;
	ulint		n_free_up;
	ulint		reserve;

	ut_ad(mtr);
	*n_reserved = n_ext;

	fil_space_t*		space = mtr_x_lock_space(space_id, mtr);
	const page_size_t	page_size(space->flags);

	space_header = fsp_get_space_header(space_id, page_size, mtr);
try_again:
	size = mach_read_from_4(space_header + FSP_SIZE);
	ut_ad(size == space->size_in_header);

	if (size < FSP_EXTENT_SIZE && n_pages < FSP_EXTENT_SIZE / 2) {
		/* Use different rules for small single-table tablespaces */
		*n_reserved = 0;
		return(fsp_reserve_free_pages(space, space_header, size,
					      mtr, n_pages));
	}

	n_free_list_ext = flst_get_len(space_header + FSP_FREE);
	ut_ad(space->free_len == n_free_list_ext);

	free_limit = mtr_read_ulint(space_header + FSP_FREE_LIMIT,
				    MLOG_4BYTES, mtr);
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
		n_free_up -= n_free_up / (page_size.physical()
					  / FSP_EXTENT_SIZE);
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
		return(true);
	}
try_to_extend:
	if (fsp_try_extend_data_file(space, space_header, mtr)) {
		goto try_again;
	}

	return(false);
}

/** Calculate how many KiB of new data we will be able to insert to the
tablespace without running out of space.
@param[in]	space_id	tablespace ID
@return available space in KiB
@retval UINTMAX_MAX if unknown */
uintmax_t
fsp_get_available_space_in_free_extents(
	ulint	space_id)
{
	FilSpace	space(space_id);
	if (space() == NULL) {
		return(UINTMAX_MAX);
	}

	return(fsp_get_available_space_in_free_extents(space));
}

/** Calculate how many KiB of new data we will be able to insert to the
tablespace without running out of space. Start with a space object that has
been acquired by the caller who holds it for the calculation,
@param[in]	space		tablespace object from fil_space_acquire()
@return available space in KiB */
uintmax_t
fsp_get_available_space_in_free_extents(
	const fil_space_t*	space)
{
	ut_ad(space->n_pending_ops > 0);

	ulint	size_in_header = space->size_in_header;
	if (size_in_header < FSP_EXTENT_SIZE) {
		return(0);		/* TODO: count free frag pages and
					return a value based on that */
	}

	/* Below we play safe when counting free extents above the free limit:
	some of them will contain extent descriptor pages, and therefore
	will not be free extents */
	ut_ad(size_in_header >= space->free_limit);
	ulint	n_free_up =
		(size_in_header - space->free_limit) / FSP_EXTENT_SIZE;

	page_size_t	page_size(space->flags);
	if (n_free_up > 0) {
		n_free_up--;
		n_free_up -= n_free_up / (page_size.physical()
					  / FSP_EXTENT_SIZE);
	}

	/* We reserve 1 extent + 0.5 % of the space size to undo logs
	and 1 extent + 0.5 % to cleaning operations; NOTE: this source
	code is duplicated in the function above! */

	ulint	reserve = 2 + ((size_in_header / FSP_EXTENT_SIZE) * 2) / 200;
	ulint	n_free = space->free_len + n_free_up;

	if (reserve > n_free) {
		return(0);
	}

	return(static_cast<uintmax_t>(n_free - reserve)
	       * FSP_EXTENT_SIZE * (page_size.physical() / 1024));
}

/********************************************************************//**
Marks a page used. The page must reside within the extents of the given
segment. */
static
void
fseg_mark_page_used(
/*================*/
	fseg_inode_t*	seg_inode,/*!< in: segment inode */
	ulint		page,	/*!< in: page offset */
	xdes_t*		descr,  /*!< in: extent descriptor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	not_full_n_used;

	ut_ad(fil_page_get_type(page_align(seg_inode)) == FIL_PAGE_INODE);
	ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N)
	      == FSEG_MAGIC_N_VALUE);

	ut_ad(mtr_read_ulint(seg_inode + FSEG_ID, MLOG_4BYTES, mtr)
	      == mtr_read_ulint(descr + XDES_ID, MLOG_4BYTES, mtr));

	if (xdes_is_free(descr, mtr)) {
		/* We move the extent from the free list to the
		NOT_FULL list */
		flst_remove(seg_inode + FSEG_FREE, descr + XDES_FLST_NODE,
			    mtr);
		flst_add_last(seg_inode + FSEG_NOT_FULL,
			      descr + XDES_FLST_NODE, mtr);
	}

	ut_ad(xdes_mtr_get_bit(
			descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, mtr));

	/* We mark the page as used */
	xdes_set_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, FALSE, mtr);

	not_full_n_used = mtr_read_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
					 MLOG_4BYTES, mtr);
	not_full_n_used++;
	mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED, not_full_n_used,
			 MLOG_4BYTES, mtr);
	if (xdes_is_full(descr, mtr)) {
		/* We move the extent from the NOT_FULL list to the
		FULL list */
		flst_remove(seg_inode + FSEG_NOT_FULL,
			    descr + XDES_FLST_NODE, mtr);
		flst_add_last(seg_inode + FSEG_FULL,
			      descr + XDES_FLST_NODE, mtr);

		mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
				 not_full_n_used - FSP_EXTENT_SIZE,
				 MLOG_4BYTES, mtr);
	}
}

/** Frees a single page of a segment.
@param[in]	seg_inode	segment inode
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	ahi		whether we may need to drop the adaptive
hash index
@param[in,out]	mtr		mini-transaction */
static
void
fseg_free_page_low(
	fseg_inode_t*		seg_inode,
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	bool			ahi,
	mtr_t*			mtr)
{
	xdes_t*	descr;
	ulint	not_full_n_used;
	ulint	state;
	ib_id_t	descr_id;
	ib_id_t	seg_id;
	ulint	i;

	ut_ad(seg_inode != NULL);
	ut_ad(mtr != NULL);
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N)
	      == FSEG_MAGIC_N_VALUE);
	ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
	ut_d(fsp_space_modify_check(page_id.space(), mtr));

	/* Drop search system page hash index if the page is found in
	the pool and is hashed */

	if (ahi) {
		btr_search_drop_page_hash_when_freed(page_id, page_size);
	}

	descr = xdes_get_descriptor(page_id.space(), page_id.page_no(),
				    page_size, mtr);

	if (xdes_mtr_get_bit(descr, XDES_FREE_BIT,
			     page_id.page_no() % FSP_EXTENT_SIZE, mtr)) {
		fputs("InnoDB: Dump of the tablespace extent descriptor: ",
		      stderr);
		ut_print_buf(stderr, descr, 40);
		ib::error() << "InnoDB is trying to free page " << page_id
			<< " though it is already marked as free in the"
			" tablespace! The tablespace free space info is"
			" corrupt. You may need to dump your tables and"
			" recreate the whole database!";
crash:
		ib::fatal() << FORCE_RECOVERY_MSG;
	}

	state = xdes_get_state(descr, mtr);

	if (state != XDES_FSEG) {
		/* The page is in the fragment pages of the segment */

		for (i = 0;; i++) {
			if (fseg_get_nth_frag_page_no(seg_inode, i, mtr)
			    == page_id.page_no()) {

				fseg_set_nth_frag_page_no(seg_inode, i,
							  FIL_NULL, mtr);
				break;
			}
		}

		fsp_free_page(page_id, page_size, mtr);

		return;
	}

	/* If we get here, the page is in some extent of the segment */

	descr_id = mach_read_from_8(descr + XDES_ID);
	seg_id = mach_read_from_8(seg_inode + FSEG_ID);

	if (UNIV_UNLIKELY(descr_id != seg_id)) {
		fputs("InnoDB: Dump of the tablespace extent descriptor: ",
		      stderr);
		ut_print_buf(stderr, descr, 40);
		fputs("\nInnoDB: Dump of the segment inode: ", stderr);
		ut_print_buf(stderr, seg_inode, 40);
		putc('\n', stderr);

		ib::error() << "InnoDB is trying to free page " << page_id
			<< ", which does not belong to segment " << descr_id
			<< " but belongs to segment " << seg_id << ".";
		goto crash;
	}

	not_full_n_used = mtr_read_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
					 MLOG_4BYTES, mtr);
	if (xdes_is_full(descr, mtr)) {
		/* The fragment is full: move it to another list */
		flst_remove(seg_inode + FSEG_FULL,
			    descr + XDES_FLST_NODE, mtr);
		flst_add_last(seg_inode + FSEG_NOT_FULL,
			      descr + XDES_FLST_NODE, mtr);
		mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
				 not_full_n_used + FSP_EXTENT_SIZE - 1,
				 MLOG_4BYTES, mtr);
	} else {
		ut_a(not_full_n_used > 0);
		mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
				 not_full_n_used - 1, MLOG_4BYTES, mtr);
	}

	const ulint	bit = page_id.page_no() % FSP_EXTENT_SIZE;

	xdes_set_bit(descr, XDES_FREE_BIT, bit, TRUE, mtr);
	xdes_set_bit(descr, XDES_CLEAN_BIT, bit, TRUE, mtr);

	if (xdes_is_free(descr, mtr)) {
		/* The extent has become free: free it to space */
		flst_remove(seg_inode + FSEG_NOT_FULL,
			    descr + XDES_FLST_NODE, mtr);
		fsp_free_extent(page_id, page_size, mtr);
	}
}

/**********************************************************************//**
Frees a single page of a segment. */
void
fseg_free_page(
/*===========*/
	fseg_header_t*	seg_header, /*!< in: segment header */
	ulint		space_id,/*!< in: space id */
	ulint		page,	/*!< in: page offset */
	bool		ahi,	/*!< in: whether we may need to drop
				the adaptive hash index */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	fseg_inode_t*		seg_inode;
	buf_block_t*		iblock;
	const fil_space_t*	space = mtr_x_lock_space(space_id, mtr);
	const page_size_t	page_size(space->flags);

	seg_inode = fseg_inode_get(seg_header, space_id, page_size, mtr,
				   &iblock);
	fil_block_check_type(iblock, FIL_PAGE_INODE, mtr);

	const page_id_t	page_id(space_id, page);

	fseg_free_page_low(seg_inode, page_id, page_size, ahi, mtr);

	ut_d(buf_page_set_file_page_was_freed(page_id));
}

/**********************************************************************//**
Checks if a single page of a segment is free.
@return true if free */
bool
fseg_page_is_free(
/*==============*/
	fseg_header_t*	seg_header,	/*!< in: segment header */
	ulint		space_id,	/*!< in: space id */
	ulint		page)		/*!< in: page offset */
{
	mtr_t		mtr;
	ibool		is_free;
	xdes_t*		descr;
	fseg_inode_t*	seg_inode;

	mtr_start(&mtr);
	const fil_space_t*	space = mtr_x_lock_space(space_id, &mtr);
	const page_size_t	page_size(space->flags);

	seg_inode = fseg_inode_get(seg_header, space_id, page_size, &mtr);

	ut_a(seg_inode);
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N)
	      == FSEG_MAGIC_N_VALUE);
	ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));

	descr = xdes_get_descriptor(space_id, page, page_size, &mtr);
	ut_a(descr);

	is_free = xdes_mtr_get_bit(
		descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, &mtr);

	mtr_commit(&mtr);

	return(is_free);
}

/**********************************************************************//**
Frees an extent of a segment to the space free list. */
static
void
fseg_free_extent(
/*=============*/
	fseg_inode_t*	seg_inode, /*!< in: segment inode */
	ulint		space,	/*!< in: space id */
	const page_size_t&	page_size,
	ulint		page,	/*!< in: a page in the extent */
	bool		ahi,	/*!< in: whether we may need to drop
				the adaptive hash index */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	first_page_in_extent;
	xdes_t*	descr;
	ulint	not_full_n_used;
	ulint	descr_n_used;
	ulint	i;

	ut_ad(seg_inode != NULL);
	ut_ad(mtr != NULL);

	descr = xdes_get_descriptor(space, page, page_size, mtr);

	ut_a(xdes_get_state(descr, mtr) == XDES_FSEG);
	ut_a(!memcmp(descr + XDES_ID, seg_inode + FSEG_ID, 8));
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N)
	      == FSEG_MAGIC_N_VALUE);
	ut_d(fsp_space_modify_check(space, mtr));

	first_page_in_extent = page - (page % FSP_EXTENT_SIZE);

	if (ahi) {
		for (i = 0; i < FSP_EXTENT_SIZE; i++) {
			if (!xdes_mtr_get_bit(descr, XDES_FREE_BIT, i, mtr)) {

				/* Drop search system page hash index
				if the page is found in the pool and
				is hashed */

				btr_search_drop_page_hash_when_freed(
					page_id_t(space,
						  first_page_in_extent + i),
					page_size);
			}
		}
	}

	if (xdes_is_full(descr, mtr)) {
		flst_remove(seg_inode + FSEG_FULL,
			    descr + XDES_FLST_NODE, mtr);
	} else if (xdes_is_free(descr, mtr)) {
		flst_remove(seg_inode + FSEG_FREE,
			    descr + XDES_FLST_NODE, mtr);
	} else {
		flst_remove(seg_inode + FSEG_NOT_FULL,
			    descr + XDES_FLST_NODE, mtr);

		not_full_n_used = mtr_read_ulint(
			seg_inode + FSEG_NOT_FULL_N_USED, MLOG_4BYTES, mtr);

		descr_n_used = xdes_get_n_used(descr, mtr);
		ut_a(not_full_n_used >= descr_n_used);
		mlog_write_ulint(seg_inode + FSEG_NOT_FULL_N_USED,
				 not_full_n_used - descr_n_used,
				 MLOG_4BYTES, mtr);
	}

	fsp_free_extent(page_id_t(space, page), page_size, mtr);

#ifdef UNIV_DEBUG
	for (i = 0; i < FSP_EXTENT_SIZE; i++) {

		buf_page_set_file_page_was_freed(
			page_id_t(space, first_page_in_extent + i));
	}
#endif /* UNIV_DEBUG */
}

/**********************************************************************//**
Frees part of a segment. This function can be used to free a segment by
repeatedly calling this function in different mini-transactions. Doing
the freeing in a single mini-transaction might result in too big a
mini-transaction.
@return TRUE if freeing completed */
ibool
fseg_free_step(
/*===========*/
	fseg_header_t*	header,	/*!< in, own: segment header; NOTE: if the header
				resides on the first page of the frag list
				of the segment, this pointer becomes obsolete
				after the last freeing step */
	bool		ahi,	/*!< in: whether we may need to drop
				the adaptive hash index */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint		n;
	ulint		page;
	xdes_t*		descr;
	fseg_inode_t*	inode;
	ulint		space_id;
	ulint		header_page;

	DBUG_ENTER("fseg_free_step");

	space_id = page_get_space_id(page_align(header));
	header_page = page_get_page_no(page_align(header));

	const fil_space_t*	space = mtr_x_lock_space(space_id, mtr);
	const page_size_t	page_size(space->flags);

	descr = xdes_get_descriptor(space_id, header_page, page_size, mtr);

	/* Check that the header resides on a page which has not been
	freed yet */

	ut_a(xdes_mtr_get_bit(descr, XDES_FREE_BIT,
			      header_page % FSP_EXTENT_SIZE, mtr) == FALSE);
	buf_block_t*		iblock;

	inode = fseg_inode_try_get(header, space_id, page_size, mtr, &iblock);

	if (inode == NULL) {
		ib::info() << "Double free of inode from "
			<< page_id_t(space_id, header_page);
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
		inode,
		page_id_t(space_id, fseg_get_nth_frag_page_no(inode, n, mtr)),
		page_size, ahi, mtr);

	n = fseg_find_last_used_frag_page_slot(inode, mtr);

	if (n == ULINT_UNDEFINED) {
		/* Freeing completed: free the segment inode */
		fsp_free_seg_inode(space_id, page_size, inode, mtr);

		DBUG_RETURN(TRUE);
	}

	DBUG_RETURN(FALSE);
}

/**********************************************************************//**
Frees part of a segment. Differs from fseg_free_step because this function
leaves the header page unfreed.
@return TRUE if freeing completed, except the header page */
ibool
fseg_free_step_not_header(
/*======================*/
	fseg_header_t*	header,	/*!< in: segment header which must reside on
				the first fragment page of the segment */
	bool		ahi,	/*!< in: whether we may need to drop
				the adaptive hash index */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint		n;
	ulint		page;
	xdes_t*		descr;
	fseg_inode_t*	inode;
	ulint		space_id;
	ulint		page_no;

	space_id = page_get_space_id(page_align(header));
	ut_ad(mtr->is_named_space(space_id));

	const fil_space_t*	space = mtr_x_lock_space(space_id, mtr);
	const page_size_t	page_size(space->flags);
	buf_block_t*		iblock;

	inode = fseg_inode_get(header, space_id, page_size, mtr, &iblock);
	fil_block_check_type(iblock, FIL_PAGE_INODE, mtr);

	descr = fseg_get_first_extent(inode, space_id, page_size, mtr);

	if (descr != NULL) {
		/* Free the extent held by the segment */
		page = xdes_get_offset(descr);

		fseg_free_extent(inode, space_id, page_size, page, ahi, mtr);

		return(FALSE);
	}

	/* Free a frag page */

	n = fseg_find_last_used_frag_page_slot(inode, mtr);

	if (n == ULINT_UNDEFINED) {
		ut_error;
	}

	page_no = fseg_get_nth_frag_page_no(inode, n, mtr);

	if (page_no == page_get_page_no(page_align(header))) {

		return(TRUE);
	}

	fseg_free_page_low(inode, page_id_t(space_id, page_no), page_size, ahi,
			   mtr);

	return(FALSE);
}

/** Returns the first extent descriptor for a segment.
We think of the extent lists of the segment catenated in the order
FSEG_FULL -> FSEG_NOT_FULL -> FSEG_FREE.
@param[in]	inode		segment inode
@param[in]	space_id	space id
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return the first extent descriptor, or NULL if none */
static
xdes_t*
fseg_get_first_extent(
	fseg_inode_t*		inode,
	ulint			space_id,
	const page_size_t&	page_size,
	mtr_t*			mtr)
{
	fil_addr_t	first;
	xdes_t*		descr;

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

		return(NULL);
	}
	descr = xdes_lst_get_descriptor(space_id, page_size, first, mtr);

	return(descr);
}

#ifdef UNIV_DEBUG
/*******************************************************************//**
Validates a segment.
@return TRUE if ok */
static
ibool
fseg_validate_low(
/*==============*/
	fseg_inode_t*	inode, /*!< in: segment inode */
	mtr_t*		mtr2)	/*!< in/out: mini-transaction */
{
	ulint		space_id;
	ib_id_t		seg_id;
	mtr_t		mtr;
	xdes_t*		descr;
	fil_addr_t	node_addr;
	ulint		n_used		= 0;
	ulint		n_used2		= 0;

	ut_ad(mtr_memo_contains_page(mtr2, inode, MTR_MEMO_PAGE_SX_FIX));
	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	space_id = page_get_space_id(page_align(inode));

	seg_id = mach_read_from_8(inode + FSEG_ID);
	n_used = mtr_read_ulint(inode + FSEG_NOT_FULL_N_USED,
				MLOG_4BYTES, mtr2);
	flst_validate(inode + FSEG_FREE, mtr2);
	flst_validate(inode + FSEG_NOT_FULL, mtr2);
	flst_validate(inode + FSEG_FULL, mtr2);

	/* Validate FSEG_FREE list */
	node_addr = flst_get_first(inode + FSEG_FREE, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		const fil_space_t*	space = mtr_x_lock_space(
			space_id, &mtr);

		const page_size_t	page_size(space->flags);

		descr = xdes_lst_get_descriptor(space_id, page_size,
						node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == 0);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FSEG);
		ut_a(mach_read_from_8(descr + XDES_ID) == seg_id);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate FSEG_NOT_FULL list */

	node_addr = flst_get_first(inode + FSEG_NOT_FULL, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		const fil_space_t*	space = mtr_x_lock_space(
			space_id, &mtr);
		const page_size_t	page_size(space->flags);

		descr = xdes_lst_get_descriptor(space_id, page_size,
						node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) > 0);
		ut_a(xdes_get_n_used(descr, &mtr) < FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FSEG);
		ut_a(mach_read_from_8(descr + XDES_ID) == seg_id);

		n_used2 += xdes_get_n_used(descr, &mtr);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate FSEG_FULL list */

	node_addr = flst_get_first(inode + FSEG_FULL, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		const fil_space_t*	space = mtr_x_lock_space(
			space_id, &mtr);
		const page_size_t	page_size(space->flags);

		descr = xdes_lst_get_descriptor(space_id, page_size,
						node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FSEG);
		ut_a(mach_read_from_8(descr + XDES_ID) == seg_id);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	ut_a(n_used == n_used2);

	return(TRUE);
}

/*******************************************************************//**
Validates a segment.
@return TRUE if ok */
ibool
fseg_validate(
/*==========*/
	fseg_header_t*	header, /*!< in: segment header */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	fseg_inode_t*	inode;
	ibool		ret;
	ulint		space_id;

	space_id = page_get_space_id(page_align(header));

	const fil_space_t*	space = mtr_x_lock_space(space_id, mtr);
	const page_size_t	page_size(space->flags);

	inode = fseg_inode_get(header, space_id, page_size, mtr);

	ret = fseg_validate_low(inode, mtr);

	return(ret);
}
#endif /* UNIV_DEBUG */

#ifdef UNIV_BTR_PRINT
/*******************************************************************//**
Writes info of a segment. */
static
void
fseg_print_low(
/*===========*/
	fseg_inode_t*	inode, /*!< in: segment inode */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	space;
	ulint	n_used;
	ulint	n_frag;
	ulint	n_free;
	ulint	n_not_full;
	ulint	n_full;
	ulint	reserved;
	ulint	used;
	ulint	page_no;
	ib_id_t	seg_id;

	ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_SX_FIX));
	space = page_get_space_id(page_align(inode));
	page_no = page_get_page_no(page_align(inode));

	reserved = fseg_n_reserved_pages_low(inode, &used, mtr);

	seg_id = mach_read_from_8(inode + FSEG_ID);

	n_used = mtr_read_ulint(inode + FSEG_NOT_FULL_N_USED,
				MLOG_4BYTES, mtr);
	n_frag = fseg_get_n_frag_pages(inode, mtr);
	n_free = flst_get_len(inode + FSEG_FREE);
	n_not_full = flst_get_len(inode + FSEG_NOT_FULL);
	n_full = flst_get_len(inode + FSEG_FULL);

	ib::info() << "SEGMENT id " << seg_id
		<< " space " << space << ";"
		<< " page " << page_no << ";"
		<< " res " << reserved << " used " << used << ";"
		<< " full ext " << n_full << ";"
		<< " fragm pages " << n_frag << ";"
		<< " free extents " << n_free << ";"
		<< " not full extents " << n_not_full << ": pages " << n_used;

	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
}

/*******************************************************************//**
Writes info of a segment. */
void
fseg_print(
/*=======*/
	fseg_header_t*	header, /*!< in: segment header */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	fseg_inode_t*	inode;
	ulint		space_id;

	space_id = page_get_space_id(page_align(header));
	const fil_space_t*	space = mtr_x_lock_space(space_id, mtr);
	const page_size_t	page_size(space->flags);

	inode = fseg_inode_get(header, space_id, page_size, mtr);

	fseg_print_low(inode, mtr);
}
#endif /* UNIV_BTR_PRINT */
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
/** Print the file segment header to the given output stream.
@param[in]	out	the output stream into which the object is printed.
@retval	the output stream into which the object was printed. */
std::ostream&
fseg_header::to_stream(std::ostream&	out) const
{
	const ulint	space = mtr_read_ulint(m_header + FSEG_HDR_SPACE,
					       MLOG_4BYTES, m_mtr);

	const ulint	page_no = mtr_read_ulint(m_header + FSEG_HDR_PAGE_NO,
						 MLOG_4BYTES, m_mtr);

	const ulint	offset = mtr_read_ulint(m_header + FSEG_HDR_OFFSET,
						 MLOG_2BYTES, m_mtr);

	out << "[fseg_header_t: space=" << space << ", page="
		<< page_no << ", offset=" << offset << "]";

	return(out);
}
#endif /* UNIV_DEBUG */
