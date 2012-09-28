/*****************************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.

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

#include "fsp0fsp.h"

#ifdef UNIV_NONINL
#include "fsp0fsp.ic"
#endif

#include "buf0buf.h"
#include "fil0fil.h"
#include "mtr0log.h"
#include "ut0byte.h"
#include "page0page.h"
#include "page0zip.h"
#ifdef UNIV_HOTBACKUP
# include "fut0lst.h"
#else /* UNIV_HOTBACKUP */
# include "sync0sync.h"
# include "fut0fut.h"
# include "srv0srv.h"
# include "ibuf0ibuf.h"
# include "btr0btr.h"
# include "btr0sea.h"
# include "dict0boot.h"
# include "log0log.h"
#endif /* UNIV_HOTBACKUP */
#include "dict0mem.h"
#include "srv0start.h"


#ifndef UNIV_HOTBACKUP
/** Flag to indicate if we have printed the tablespace full error. */
static ibool fsp_tbs_full_error_printed = FALSE;

/**********************************************************************//**
Returns an extent to the free list of a space. */
static
void
fsp_free_extent(
/*============*/
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint		page,	/*!< in: page offset in the extent */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */
/**********************************************************************//**
Frees an extent of a segment to the space free list. */
static
void
fseg_free_extent(
/*=============*/
	fseg_inode_t*	seg_inode, /*!< in: segment inode */
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint		page,	/*!< in: page offset in the extent */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */
/**********************************************************************//**
Calculates the number of pages reserved by a segment, and how
many pages are currently used.
@return	number of reserved pages */
static
ulint
fseg_n_reserved_pages_low(
/*======================*/
	fseg_inode_t*	header,	/*!< in: segment inode */
	ulint*		used,	/*!< out: number of pages used (not
				more than reserved) */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */
/********************************************************************//**
Marks a page used. The page must reside within the extents of the given
segment. */
static __attribute__((nonnull))
void
fseg_mark_page_used(
/*================*/
	fseg_inode_t*	seg_inode,/*!< in: segment inode */
	ulint		page,	/*!< in: page offset */
	xdes_t*		descr,  /*!< in: extent descriptor */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */
/**********************************************************************//**
Returns the first extent descriptor for a segment. We think of the extent
lists of the segment catenated in the order FSEG_FULL -> FSEG_NOT_FULL
-> FSEG_FREE.
@return	the first extent descriptor, or NULL if none */
static
xdes_t*
fseg_get_first_extent(
/*==================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */
/**********************************************************************//**
Puts new extents to the free list if
there are free extents above the free limit. If an extent happens
to contain an extent descriptor page, the extent is put to
the FSP_FREE_FRAG list with the page marked as used. */
static
void
fsp_fill_free_list(
/*===============*/
	ibool		init_space,	/*!< in: TRUE if this is a single-table
					tablespace and we are only initing
					the tablespace's first extent
					descriptor page and ibuf bitmap page;
					then we do not allocate more extents */
	ulint		space,		/*!< in: space */
	fsp_header_t*	header,		/*!< in/out: space header */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
	UNIV_COLD __attribute__((nonnull));
/**********************************************************************//**
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation.
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block (not allocated or initialized) otherwise */
static
buf_block_t*
fseg_alloc_free_page_low(
/*=====================*/
	ulint		space,	/*!< in: space */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	fseg_inode_t*	seg_inode, /*!< in/out: segment inode */
	ulint		hint,	/*!< in: hint of which page would be
				desirable */
	byte		direction, /*!< in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	mtr_t*		mtr,	/*!< in/out: mini-transaction */
	mtr_t*		init_mtr)/*!< in/out: mtr or another mini-transaction
				in which the page should be initialized.
				If init_mtr!=mtr, but the page is already
				latched in mtr, do not initialize the page. */
	__attribute__((warn_unused_result, nonnull));
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Reads the file space size stored in the header page.
@return	tablespace size stored in the space header */
UNIV_INTERN
ulint
fsp_get_size_low(
/*=============*/
	page_t*	page)	/*!< in: header page (page 0 in the tablespace) */
{
	return(mach_read_from_4(page + FSP_HEADER_OFFSET + FSP_SIZE));
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Gets a pointer to the space header and x-locks its page.
@return	pointer to the space header, page x-locked */
UNIV_INLINE
fsp_header_t*
fsp_get_space_header(
/*=================*/
	ulint	id,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size in bytes
			or 0 for uncompressed pages */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	buf_block_t*	block;
	fsp_header_t*	header;

	ut_ad(ut_is_2pow(zip_size));
	ut_ad(zip_size <= UNIV_ZIP_SIZE_MAX);
	ut_ad(!zip_size || zip_size >= UNIV_ZIP_SIZE_MIN);
	ut_ad(id || !zip_size);

	block = buf_page_get(id, zip_size, 0, RW_X_LATCH, mtr);
	header = FSP_HEADER_OFFSET + buf_block_get_frame(block);
	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

	ut_ad(id == mach_read_from_4(FSP_SPACE_ID + header));
	ut_ad(zip_size == fsp_flags_get_zip_size(
		      mach_read_from_4(FSP_SPACE_FLAGS + header)));
	return(header);
}

/**********************************************************************//**
Gets a descriptor bit of a page.
@return	TRUE if free */
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
	ut_ad(mtr->state == MTR_ACTIVE);
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_X_FIX));

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

	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_X_FIX));
	ut_ad((bit == XDES_FREE_BIT) || (bit == XDES_CLEAN_BIT));
	ut_ad(offset < FSP_EXTENT_SIZE);

	index = bit + XDES_BITS_PER_PAGE * offset;

	byte_index = index / 8;
	bit_index = index % 8;

	descr_byte = mtr_read_ulint(descr + XDES_BITMAP + byte_index,
				    MLOG_1BYTE, mtr);
	descr_byte = ut_bit_set_nth(descr_byte, bit_index, val);

	mlog_write_ulint(descr + XDES_BITMAP + byte_index, descr_byte,
			 MLOG_1BYTE, mtr);
}

/**********************************************************************//**
Looks for a descriptor bit having the desired value. Starts from hint
and scans upward; at the end of the extent the search is wrapped to
the start of the extent.
@return	bit index of the bit, ULINT_UNDEFINED if not found */
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
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_X_FIX));
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
Looks for a descriptor bit having the desired value. Scans the extent in
a direction opposite to xdes_find_bit.
@return	bit index of the bit, ULINT_UNDEFINED if not found */
UNIV_INLINE
ulint
xdes_find_bit_downward(
/*===================*/
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
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_X_FIX));
	for (i = hint + 1; i > 0; i--) {
		if (val == xdes_mtr_get_bit(descr, bit, i - 1, mtr)) {

			return(i - 1);
		}
	}

	for (i = FSP_EXTENT_SIZE - 1; i > hint; i--) {
		if (val == xdes_mtr_get_bit(descr, bit, i, mtr)) {

			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**********************************************************************//**
Returns the number of used pages in a descriptor.
@return	number of pages used */
UNIV_INLINE
ulint
xdes_get_n_used(
/*============*/
	const xdes_t*	descr,	/*!< in: descriptor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	count	= 0;

	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_X_FIX));
	for (ulint i = 0; i < FSP_EXTENT_SIZE; ++i) {
		if (FALSE == xdes_mtr_get_bit(descr, XDES_FREE_BIT, i, mtr)) {
			count++;
		}
	}

	return(count);
}

/**********************************************************************//**
Returns true if extent contains no used pages.
@return	TRUE if totally free */
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
@return	TRUE if full */
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
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_X_FIX));

	mlog_write_ulint(descr + XDES_STATE, state, MLOG_4BYTES, mtr);
}

/**********************************************************************//**
Gets the state of an xdes.
@return	state */
UNIV_INLINE
ulint
xdes_get_state(
/*===========*/
	const xdes_t*	descr,	/*!< in: descriptor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	state;

	ut_ad(descr && mtr);
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_X_FIX));

	state = mtr_read_ulint(descr + XDES_STATE, MLOG_4BYTES, mtr);
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
	ut_ad(mtr_memo_contains_page(mtr, descr, MTR_MEMO_PAGE_X_FIX));
	ut_ad((XDES_SIZE - XDES_BITMAP) % 4 == 0);

	for (i = XDES_BITMAP; i < XDES_SIZE; i += 4) {
		mlog_write_ulint(descr + i, 0xFFFFFFFFUL, MLOG_4BYTES, mtr);
	}

	xdes_set_state(descr, XDES_FREE, mtr);
}

/********************************************************************//**
Gets pointer to a the extent descriptor of a page. The page where the extent
descriptor resides is x-locked. This function no longer extends the data
file.
@return pointer to the extent descriptor, NULL if the page does not
exist in the space or if the offset is >= the free limit */
UNIV_INLINE __attribute__((nonnull, warn_unused_result))
xdes_t*
xdes_get_descriptor_with_space_hdr(
/*===============================*/
	fsp_header_t*	sp_header,	/*!< in/out: space header, x-latched
					in mtr */
	ulint		space,		/*!< in: space id */
	ulint		offset,		/*!< in: page offset; if equal
					to the free limit, we try to
					add new extents to the space
					free list */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	ulint	limit;
	ulint	size;
	ulint	zip_size;
	ulint	descr_page_no;
	page_t*	descr_page;

	ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space, NULL),
				MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains_page(mtr, sp_header, MTR_MEMO_PAGE_X_FIX));
	ut_ad(page_offset(sp_header) == FSP_HEADER_OFFSET);
	/* Read free limit and space size */
	limit = mach_read_from_4(sp_header + FSP_FREE_LIMIT);
	size  = mach_read_from_4(sp_header + FSP_SIZE);
	zip_size = fsp_flags_get_zip_size(
		mach_read_from_4(sp_header + FSP_SPACE_FLAGS));

	if ((offset >= size) || (offset >= limit)) {
		return(NULL);
	}

	descr_page_no = xdes_calc_descriptor_page(zip_size, offset);

	if (descr_page_no == 0) {
		/* It is on the space header page */

		descr_page = page_align(sp_header);
	} else {
		buf_block_t*	block;

		block = buf_page_get(space, zip_size, descr_page_no,
				     RW_X_LATCH, mtr);
		buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

		descr_page = buf_block_get_frame(block);
	}

	return(descr_page + XDES_ARR_OFFSET
	       + XDES_SIZE * xdes_calc_descriptor_index(zip_size, offset));
}

/********************************************************************//**
Gets pointer to a the extent descriptor of a page. The page where the
extent descriptor resides is x-locked. If the page offset is equal to
the free limit of the space, adds new extents from above the free limit
to the space free list, if not free limit == space size. This adding
is necessary to make the descriptor defined, as they are uninitialized
above the free limit.
@return pointer to the extent descriptor, NULL if the page does not
exist in the space or if the offset exceeds the free limit */
static __attribute__((nonnull, warn_unused_result))
xdes_t*
xdes_get_descriptor(
/*================*/
	ulint	space,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size in bytes
			or 0 for uncompressed pages */
	ulint	offset,	/*!< in: page offset; if equal to the free limit,
			we try to add new extents to the space free list */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	buf_block_t*	block;
	fsp_header_t*	sp_header;

	block = buf_page_get(space, zip_size, 0, RW_X_LATCH, mtr);
	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

	sp_header = FSP_HEADER_OFFSET + buf_block_get_frame(block);
	return(xdes_get_descriptor_with_space_hdr(sp_header, space, offset,
						  mtr));
}

/********************************************************************//**
Gets pointer to a the extent descriptor if the file address
of the descriptor list node is known. The page where the
extent descriptor resides is x-locked.
@return	pointer to the extent descriptor */
UNIV_INLINE
xdes_t*
xdes_lst_get_descriptor(
/*====================*/
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	fil_addr_t	lst_node,/*!< in: file address of the list node
				contained in the descriptor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	xdes_t*	descr;

	ut_ad(mtr);
	ut_ad(mtr_memo_contains(mtr, fil_space_get_latch(space, NULL),
				MTR_MEMO_X_LOCK));
	descr = fut_get_ptr(space, zip_size, lst_node, RW_X_LATCH, mtr)
		- XDES_FLST_NODE;

	return(descr);
}

/********************************************************************//**
Returns page offset of the first page in extent described by a descriptor.
@return	offset of the first page in extent */
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

#ifndef UNIV_HOTBACKUP
	block->check_index_page_at_flush = FALSE;
#endif /* !UNIV_HOTBACKUP */

	if (page_zip) {
		memset(page, 0, UNIV_PAGE_SIZE);
		memset(page_zip->data, 0, page_zip_get_size(page_zip));
		mach_write_to_4(page + FIL_PAGE_OFFSET,
				buf_block_get_page_no(block));
		mach_write_to_4(page
				+ FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
				buf_block_get_space(block));
		memcpy(page_zip->data + FIL_PAGE_OFFSET,
		       page + FIL_PAGE_OFFSET, 4);
		memcpy(page_zip->data + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
		       page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, 4);
		return;
	}

	memset(page, 0, UNIV_PAGE_SIZE);
	mach_write_to_4(page + FIL_PAGE_OFFSET, buf_block_get_page_no(block));
	mach_write_to_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
			buf_block_get_space(block));
}

#ifndef UNIV_HOTBACKUP
/***********************************************************//**
Inits a file page whose prior contents should be ignored. */
static
void
fsp_init_file_page(
/*===============*/
	buf_block_t*	block,	/*!< in: pointer to a page */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	fsp_init_file_page_low(block);

	mlog_write_initial_log_record(buf_block_get_frame(block),
				      MLOG_INIT_FILE_PAGE, mtr);
}
#endif /* !UNIV_HOTBACKUP */

/***********************************************************//**
Parses a redo log record of a file page init.
@return	end of log record or NULL */
UNIV_INTERN
byte*
fsp_parse_init_file_page(
/*=====================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr __attribute__((unused)), /*!< in: buffer end */
	buf_block_t*	block)	/*!< in: block or NULL */
{
	ut_ad(ptr && end_ptr);

	if (block) {
		fsp_init_file_page_low(block);
	}

	return(ptr);
}

/**********************************************************************//**
Initializes the fsp system. */
UNIV_INTERN
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
UNIV_INTERN
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
/**********************************************************************//**
Initializes the space header of a new created space and creates also the
insert buffer tree root if space == 0. */
UNIV_INTERN
void
fsp_header_init(
/*============*/
	ulint	space,		/*!< in: space id */
	ulint	size,		/*!< in: current size in blocks */
	mtr_t*	mtr)		/*!< in/out: mini-transaction */
{
	fsp_header_t*	header;
	buf_block_t*	block;
	page_t*		page;
	ulint		flags;
	ulint		zip_size;

	ut_ad(mtr);

	mtr_x_lock(fil_space_get_latch(space, &flags), mtr);

	zip_size = fsp_flags_get_zip_size(flags);
	block = buf_page_create(space, 0, zip_size, mtr);
	buf_page_get(space, zip_size, 0, RW_X_LATCH, mtr);
	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

	/* The prior contents of the file page should be ignored */

	fsp_init_file_page(block, mtr);
	page = buf_block_get_frame(block);

	mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_TYPE_FSP_HDR,
			 MLOG_2BYTES, mtr);

	header = FSP_HEADER_OFFSET + page;

	mlog_write_ulint(header + FSP_SPACE_ID, space, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_NOT_USED, 0, MLOG_4BYTES, mtr);

	mlog_write_ulint(header + FSP_SIZE, size, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_FREE_LIMIT, 0, MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_SPACE_FLAGS, flags,
			 MLOG_4BYTES, mtr);
	mlog_write_ulint(header + FSP_FRAG_N_USED, 0, MLOG_4BYTES, mtr);

	flst_init(header + FSP_FREE, mtr);
	flst_init(header + FSP_FREE_FRAG, mtr);
	flst_init(header + FSP_FULL_FRAG, mtr);
	flst_init(header + FSP_SEG_INODES_FULL, mtr);
	flst_init(header + FSP_SEG_INODES_FREE, mtr);

	mlog_write_ull(header + FSP_SEG_ID, 1, mtr);
	if (space == 0) {
		fsp_fill_free_list(FALSE, space, header, mtr);
		btr_create(DICT_CLUSTERED | DICT_UNIVERSAL | DICT_IBUF,
			   0, 0, DICT_IBUF_ID_MIN + space,
			   dict_ind_redundant, mtr);
	} else {
		fsp_fill_free_list(TRUE, space, header, mtr);
	}
}
#endif /* !UNIV_HOTBACKUP */

/**********************************************************************//**
Reads the space id from the first page of a tablespace.
@return	space id, ULINT UNDEFINED if error */
UNIV_INTERN
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
		ib_logf(IB_LOG_LEVEL_ERROR,
			"Space id in fsp header %lu,but in the page header "
			"%lu", fsp_id, id);

		return(ULINT_UNDEFINED);
	}

	return(id);
}

/**********************************************************************//**
Reads the space flags from the first page of a tablespace.
@return	flags */
UNIV_INTERN
ulint
fsp_header_get_flags(
/*=================*/
	const page_t*	page)	/*!< in: first page of a tablespace */
{
	ut_ad(!page_offset(page));

	return(mach_read_from_4(FSP_HEADER_OFFSET + FSP_SPACE_FLAGS + page));
}

/**********************************************************************//**
Reads the compressed page size from the first page of a tablespace.
@return	compressed page size in bytes, or 0 if uncompressed */
UNIV_INTERN
ulint
fsp_header_get_zip_size(
/*====================*/
	const page_t*	page)	/*!< in: first page of a tablespace */
{
	ulint	flags = fsp_header_get_flags(page);

	return(fsp_flags_get_zip_size(flags));
}

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Increases the space size field of a space. */
UNIV_INTERN
void
fsp_header_inc_size(
/*================*/
	ulint	space,		/*!< in: space id */
	ulint	size_inc,	/*!< in: size increment in pages */
	mtr_t*	mtr)		/*!< in/out: mini-transaction */
{
	fsp_header_t*	header;
	ulint		size;
	ulint		flags;

	ut_ad(mtr);

	mtr_x_lock(fil_space_get_latch(space, &flags), mtr);

	header = fsp_get_space_header(space,
				      fsp_flags_get_zip_size(flags),
				      mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);

	mlog_write_ulint(header + FSP_SIZE, size + size_inc, MLOG_4BYTES,
			 mtr);
}

/**********************************************************************//**
Gets the size of the system tablespace from the tablespace header.  If
we do not have an auto-extending data file, this should be equal to
the size of the data files.  If there is an auto-extending data file,
this can be smaller.
@return	size in pages */
UNIV_INTERN
ulint
fsp_header_get_tablespace_size(void)
/*================================*/
{
	fsp_header_t*	header;
	ulint		size;
	mtr_t		mtr;

	mtr_start(&mtr);

	mtr_x_lock(fil_space_get_latch(0, NULL), &mtr);

	header = fsp_get_space_header(0, 0, &mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, &mtr);

	mtr_commit(&mtr);

	return(size);
}

/***********************************************************************//**
Tries to extend a single-table tablespace so that a page would fit in the
data file.
@return	TRUE if success */
static UNIV_COLD __attribute__((nonnull, warn_unused_result))
ibool
fsp_try_extend_data_file_with_pages(
/*================================*/
	ulint		space,		/*!< in: space */
	ulint		page_no,	/*!< in: page number */
	fsp_header_t*	header,		/*!< in/out: space header */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	ibool	success;
	ulint	actual_size;
	ulint	size;

	ut_a(space != 0);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);

	ut_a(page_no >= size);

	success = fil_extend_space_to_desired_size(&actual_size, space,
						   page_no + 1);
	/* actual_size now has the space size in pages; it may be less than
	we wanted if we ran out of disk space */

	mlog_write_ulint(header + FSP_SIZE, actual_size, MLOG_4BYTES, mtr);

	return(success);
}

/***********************************************************************//**
Tries to extend the last data file of a tablespace if it is auto-extending.
@return	FALSE if not auto-extending */
static UNIV_COLD __attribute__((nonnull))
ibool
fsp_try_extend_data_file(
/*=====================*/
	ulint*		actual_increase,/*!< out: actual increase in pages, where
					we measure the tablespace size from
					what the header field says; it may be
					the actual file size rounded down to
					megabyte */
	ulint		space,		/*!< in: space */
	fsp_header_t*	header,		/*!< in/out: space header */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	ulint	size;
	ulint	zip_size;
	ulint	new_size;
	ulint	old_size;
	ulint	size_increase;
	ulint	actual_size;
	ibool	success;

	*actual_increase = 0;

	if (space == 0 && !srv_auto_extend_last_data_file) {

		/* We print the error message only once to avoid
		spamming the error log. Note that we don't need
		to reset the flag to FALSE as dealing with this
		error requires server restart. */
		if (fsp_tbs_full_error_printed == FALSE) {
			fprintf(stderr,
				"InnoDB: Error: Data file(s) ran"
				" out of space.\n"
				"Please add another data file or"
				" use \'autoextend\' for the last"
				" data file.\n");
			fsp_tbs_full_error_printed = TRUE;
		}
		return(FALSE);
	}

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);
	zip_size = fsp_flags_get_zip_size(
		mach_read_from_4(header + FSP_SPACE_FLAGS));

	old_size = size;

	if (space == 0) {
		if (!srv_last_file_size_max) {
			size_increase = SRV_AUTO_EXTEND_INCREMENT;
		} else {
			if (srv_last_file_size_max
			    < srv_data_file_sizes[srv_n_data_files - 1]) {

				fprintf(stderr,
					"InnoDB: Error: Last data file size"
					" is %lu, max size allowed %lu\n",
					(ulong) srv_data_file_sizes[
						srv_n_data_files - 1],
					(ulong) srv_last_file_size_max);
			}

			size_increase = srv_last_file_size_max
				- srv_data_file_sizes[srv_n_data_files - 1];
			if (size_increase > SRV_AUTO_EXTEND_INCREMENT) {
				size_increase = SRV_AUTO_EXTEND_INCREMENT;
			}
		}
	} else {
		/* We extend single-table tablespaces first one extent
		at a time, but for bigger tablespaces more. It is not
		enough to extend always by one extent, because some
		extents are frag page extents. */
		ulint	extent_size;	/*!< one megabyte, in pages */

		if (!zip_size) {
			extent_size = FSP_EXTENT_SIZE;
		} else {
			extent_size = FSP_EXTENT_SIZE
				* UNIV_PAGE_SIZE / zip_size;
		}

		if (size < extent_size) {
			/* Let us first extend the file to extent_size */
			success = fsp_try_extend_data_file_with_pages(
				space, extent_size - 1, header, mtr);
			if (!success) {
				new_size = mtr_read_ulint(header + FSP_SIZE,
							  MLOG_4BYTES, mtr);

				*actual_increase = new_size - old_size;

				return(FALSE);
			}

			size = extent_size;
		}

		if (size < 32 * extent_size) {
			size_increase = extent_size;
		} else {
			/* Below in fsp_fill_free_list() we assume
			that we add at most FSP_FREE_ADD extents at
			a time */
			size_increase = FSP_FREE_ADD * extent_size;
		}
	}

	if (size_increase == 0) {

		return(TRUE);
	}

	success = fil_extend_space_to_desired_size(&actual_size, space,
						   size + size_increase);
	/* We ignore any fragments of a full megabyte when storing the size
	to the space header */

	if (!zip_size) {
		new_size = ut_calc_align_down(actual_size,
					      (1024 * 1024) / UNIV_PAGE_SIZE);
	} else {
		new_size = ut_calc_align_down(actual_size,
					      (1024 * 1024) / zip_size);
	}
	mlog_write_ulint(header + FSP_SIZE, new_size, MLOG_4BYTES, mtr);

	*actual_increase = new_size - old_size;

	return(TRUE);
}

/**********************************************************************//**
Puts new extents to the free list if there are free extents above the free
limit. If an extent happens to contain an extent descriptor page, the extent
is put to the FSP_FREE_FRAG list with the page marked as used. */
static
void
fsp_fill_free_list(
/*===============*/
	ibool		init_space,	/*!< in: TRUE if this is a single-table
					tablespace and we are only initing
					the tablespace's first extent
					descriptor page and ibuf bitmap page;
					then we do not allocate more extents */
	ulint		space,		/*!< in: space */
	fsp_header_t*	header,		/*!< in/out: space header */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	ulint	limit;
	ulint	size;
	ulint	zip_size;
	xdes_t*	descr;
	ulint	count		= 0;
	ulint	frag_n_used;
	ulint	actual_increase;
	ulint	i;
	mtr_t	ibuf_mtr;

	ut_ad(header && mtr);
	ut_ad(page_offset(header) == FSP_HEADER_OFFSET);

	/* Check if we can fill free list from above the free list limit */
	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);
	limit = mtr_read_ulint(header + FSP_FREE_LIMIT, MLOG_4BYTES, mtr);

	zip_size = fsp_flags_get_zip_size(
		mach_read_from_4(FSP_SPACE_FLAGS + header));
	ut_a(ut_is_2pow(zip_size));
	ut_a(zip_size <= UNIV_ZIP_SIZE_MAX);
	ut_a(!zip_size || zip_size >= UNIV_ZIP_SIZE_MIN);

	if (space == 0 && srv_auto_extend_last_data_file
	    && size < limit + FSP_EXTENT_SIZE * FSP_FREE_ADD) {

		/* Try to increase the last data file size */
		fsp_try_extend_data_file(&actual_increase, space, header, mtr);
		size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);
	}

	if (space != 0 && !init_space
	    && size < limit + FSP_EXTENT_SIZE * FSP_FREE_ADD) {

		/* Try to increase the .ibd file size */
		fsp_try_extend_data_file(&actual_increase, space, header, mtr);
		size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);
	}

	i = limit;

	while ((init_space && i < 1)
	       || ((i + FSP_EXTENT_SIZE <= size) && (count < FSP_FREE_ADD))) {

		ibool	init_xdes;
		if (zip_size) {
			init_xdes = ut_2pow_remainder(i, zip_size) == 0;
		} else {
			init_xdes = ut_2pow_remainder(i, UNIV_PAGE_SIZE) == 0;
		}

		mlog_write_ulint(header + FSP_FREE_LIMIT, i + FSP_EXTENT_SIZE,
				 MLOG_4BYTES, mtr);

		if (UNIV_UNLIKELY(init_xdes)) {

			buf_block_t*	block;

			/* We are going to initialize a new descriptor page
			and a new ibuf bitmap page: the prior contents of the
			pages should be ignored. */

			if (i > 0) {
				block = buf_page_create(
					space, i, zip_size, mtr);
				buf_page_get(space, zip_size, i,
					     RW_X_LATCH, mtr);
				buf_block_dbg_add_level(block,
							SYNC_FSP_PAGE);

				fsp_init_file_page(block, mtr);
				mlog_write_ulint(buf_block_get_frame(block)
						 + FIL_PAGE_TYPE,
						 FIL_PAGE_TYPE_XDES,
						 MLOG_2BYTES, mtr);
			}

			/* Initialize the ibuf bitmap page in a separate
			mini-transaction because it is low in the latching
			order, and we must be able to release its latch
			before returning from the fsp routine */

			mtr_start(&ibuf_mtr);

			block = buf_page_create(space,
						    i + FSP_IBUF_BITMAP_OFFSET,
						    zip_size, &ibuf_mtr);
			buf_page_get(space, zip_size,
				     i + FSP_IBUF_BITMAP_OFFSET,
				     RW_X_LATCH, &ibuf_mtr);
			buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

			fsp_init_file_page(block, &ibuf_mtr);

			ibuf_bitmap_page_init(block, &ibuf_mtr);

			mtr_commit(&ibuf_mtr);
		}

		descr = xdes_get_descriptor_with_space_hdr(header, space, i,
							   mtr);
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
			frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED,
						     MLOG_4BYTES, mtr);
			mlog_write_ulint(header + FSP_FRAG_N_USED,
					 frag_n_used + 2, MLOG_4BYTES, mtr);
		} else {
			flst_add_last(header + FSP_FREE,
				      descr + XDES_FLST_NODE, mtr);
			count++;
		}

		i += FSP_EXTENT_SIZE;
	}
}

/**********************************************************************//**
Allocates a new free extent.
@return	extent descriptor, NULL if cannot be allocated */
static
xdes_t*
fsp_alloc_free_extent(
/*==================*/
	ulint	space,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size in bytes
			or 0 for uncompressed pages */
	ulint	hint,	/*!< in: hint of which extent would be desirable: any
			page offset in the extent goes; the hint must not
			be > FSP_FREE_LIMIT */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	fsp_header_t*	header;
	fil_addr_t	first;
	xdes_t*		descr;

	ut_ad(mtr);

	header = fsp_get_space_header(space, zip_size, mtr);

	descr = xdes_get_descriptor_with_space_hdr(header, space, hint, mtr);

	if (descr && (xdes_get_state(descr, mtr) == XDES_FREE)) {
		/* Ok, we can take this extent */
	} else {
		/* Take the first extent in the free list */
		first = flst_get_first(header + FSP_FREE, mtr);

		if (fil_addr_is_null(first)) {
			fsp_fill_free_list(FALSE, space, header, mtr);

			first = flst_get_first(header + FSP_FREE, mtr);
		}

		if (fil_addr_is_null(first)) {

			return(NULL);	/* No free extents left */
		}

		descr = xdes_lst_get_descriptor(space, zip_size, first, mtr);
	}

	flst_remove(header + FSP_FREE, descr + XDES_FLST_NODE, mtr);

	return(descr);
}

/**********************************************************************//**
Allocates a single free page from a space. */
static __attribute__((nonnull))
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
	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED, MLOG_4BYTES,
				     mtr);
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

/**********************************************************************//**
Gets a buffer block for an allocated page.

NOTE: If init_mtr != mtr, the block will only be initialized if it was
not previously x-latched. It is assumed that the block has been
x-latched only by mtr, and freed in mtr in that case.

@return block, initialized if init_mtr==mtr
or rw_lock_x_lock_count(&block->lock) == 1 */
static
buf_block_t*
fsp_page_create(
/*============*/
	ulint	space,		/*!< in: space id of the allocated page */
	ulint	zip_size,	/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint	page_no,	/*!< in: page number of the allocated page */
	mtr_t*	mtr,		/*!< in: mini-transaction of the allocation */
	mtr_t*	init_mtr)	/*!< in: mini-transaction for initializing
				the page */
{
	buf_block_t*	block
		= buf_page_create(space, page_no, zip_size, init_mtr);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX)
	      == rw_lock_own(&block->lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	/* Mimic buf_page_get(), but avoid the buf_pool->page_hash lookup. */
	rw_lock_x_lock(&block->lock);
	mutex_enter(&block->mutex);
	buf_block_buf_fix_inc(block, __FILE__, __LINE__);
	mutex_exit(&block->mutex);
	mtr_memo_push(init_mtr, block, MTR_MEMO_PAGE_X_FIX);

	if (init_mtr == mtr
	    || rw_lock_get_x_lock_count(&block->lock) == 1) {

		/* Initialize the page, unless it was already
		X-latched in mtr. (In this case, we would want to
		allocate another page that has not been freed in mtr.) */
		ut_ad(init_mtr == mtr
		      || !mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));

		fsp_init_file_page(block, init_mtr);
	}

	return(block);
}

/**********************************************************************//**
Allocates a single free page from a space. The page is marked as used.
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block (not allocated or initialized) otherwise */
static __attribute__((nonnull, warn_unused_result))
buf_block_t*
fsp_alloc_free_page(
/*================*/
	ulint	space,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size in bytes
			or 0 for uncompressed pages */
	ulint	hint,	/*!< in: hint of which page would be desirable */
	mtr_t*	mtr,	/*!< in/out: mini-transaction */
	mtr_t*	init_mtr)/*!< in/out: mini-transaction in which the
			page should be initialized
			(may be the same as mtr) */
{
	fsp_header_t*	header;
	fil_addr_t	first;
	xdes_t*		descr;
	ulint		free;
	ulint		page_no;
	ulint		space_size;

	ut_ad(mtr);
	ut_ad(init_mtr);

	header = fsp_get_space_header(space, zip_size, mtr);

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

			descr = fsp_alloc_free_extent(space, zip_size,
						      hint, mtr);

			if (descr == NULL) {
				/* No free space left */

				return(NULL);
			}

			xdes_set_state(descr, XDES_FREE_FRAG, mtr);
			flst_add_last(header + FSP_FREE_FRAG,
				      descr + XDES_FLST_NODE, mtr);
		} else {
			descr = xdes_lst_get_descriptor(space, zip_size,
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

	space_size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, mtr);

	if (space_size <= page_no) {
		/* It must be that we are extending a single-table tablespace
		whose size is still < 64 pages */

		ut_a(space != 0);
		if (page_no >= FSP_EXTENT_SIZE) {
			fprintf(stderr,
				"InnoDB: Error: trying to extend a"
				" single-table tablespace %lu\n"
				"InnoDB: by single page(s) though the"
				" space size %lu. Page no %lu.\n",
				(ulong) space, (ulong) space_size,
				(ulong) page_no);
			return(NULL);
		}
		if (!fsp_try_extend_data_file_with_pages(space, page_no,
							 header, mtr)) {
			/* No disk space left */
			return(NULL);
		}
	}

	fsp_alloc_from_free_frag(header, descr, free, mtr);
	return(fsp_page_create(space, zip_size, page_no, mtr, init_mtr));
}

/**********************************************************************//**
Frees a single page of a space. The page is marked as free and clean. */
static
void
fsp_free_page(
/*==========*/
	ulint	space,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size in bytes
			or 0 for uncompressed pages */
	ulint	page,	/*!< in: page offset */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	fsp_header_t*	header;
	xdes_t*		descr;
	ulint		state;
	ulint		frag_n_used;

	ut_ad(mtr);

	/* fprintf(stderr, "Freeing page %lu in space %lu\n", page, space); */

	header = fsp_get_space_header(space, zip_size, mtr);

	descr = xdes_get_descriptor_with_space_hdr(header, space, page, mtr);

	state = xdes_get_state(descr, mtr);

	if (state != XDES_FREE_FRAG && state != XDES_FULL_FRAG) {
		fprintf(stderr,
			"InnoDB: Error: File space extent descriptor"
			" of page %lu has state %lu\n",
			(ulong) page,
			(ulong) state);
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
			     page % FSP_EXTENT_SIZE, mtr)) {

		fprintf(stderr,
			"InnoDB: Error: File space extent descriptor"
			" of page %lu says it is free\n"
			"InnoDB: Dump of descriptor: ", (ulong) page);
		ut_print_buf(stderr, ((byte*) descr) - 50, 200);
		putc('\n', stderr);
		/* Crash in debug version, so that we get a core dump
		of this corruption. */
		ut_ad(0);

		/* We put here some fault tolerance: if the page
		is already free, return without doing anything! */

		return;
	}

	xdes_set_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);
	xdes_set_bit(descr, XDES_CLEAN_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);

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
		fsp_free_extent(space, zip_size, page, mtr);
	}

	mtr->n_freed_pages++;
}

/**********************************************************************//**
Returns an extent to the free list of a space. */
static
void
fsp_free_extent(
/*============*/
	ulint	space,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size in bytes
			or 0 for uncompressed pages */
	ulint	page,	/*!< in: page offset in the extent */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	fsp_header_t*	header;
	xdes_t*		descr;

	ut_ad(mtr);

	header = fsp_get_space_header(space, zip_size, mtr);

	descr = xdes_get_descriptor_with_space_hdr(header, space, page, mtr);

	if (xdes_get_state(descr, mtr) == XDES_FREE) {

		ut_print_buf(stderr, (byte*) descr - 500, 1000);
		putc('\n', stderr);

		ut_error;
	}

	xdes_init(descr, mtr);

	flst_add_last(header + FSP_FREE, descr + XDES_FLST_NODE, mtr);
}

/**********************************************************************//**
Returns the nth inode slot on an inode page.
@return	segment inode */
UNIV_INLINE
fseg_inode_t*
fsp_seg_inode_page_get_nth_inode(
/*=============================*/
	page_t*	page,	/*!< in: segment inode page */
	ulint	i,	/*!< in: inode index on page */
	ulint	zip_size __attribute__((unused)),
			/*!< in: compressed page size, or 0 */
	mtr_t*	mtr __attribute__((unused)))
			/*!< in/out: mini-transaction */
{
	ut_ad(i < FSP_SEG_INODES_PER_PAGE(zip_size));
	ut_ad(mtr_memo_contains_page(mtr, page, MTR_MEMO_PAGE_X_FIX));

	return(page + FSEG_ARR_OFFSET + FSEG_INODE_SIZE * i);
}

/**********************************************************************//**
Looks for a used segment inode on a segment inode page.
@return	segment inode index, or ULINT_UNDEFINED if not found */
static
ulint
fsp_seg_inode_page_find_used(
/*=========================*/
	page_t*	page,	/*!< in: segment inode page */
	ulint	zip_size,/*!< in: compressed page size, or 0 */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	ulint		i;
	fseg_inode_t*	inode;

	for (i = 0; i < FSP_SEG_INODES_PER_PAGE(zip_size); i++) {

		inode = fsp_seg_inode_page_get_nth_inode(
			page, i, zip_size, mtr);

		if (mach_read_from_8(inode + FSEG_ID)) {
			/* This is used */

			ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N)
			      == FSEG_MAGIC_N_VALUE);
			return(i);
		}
	}

	return(ULINT_UNDEFINED);
}

/**********************************************************************//**
Looks for an unused segment inode on a segment inode page.
@return	segment inode index, or ULINT_UNDEFINED if not found */
static
ulint
fsp_seg_inode_page_find_free(
/*=========================*/
	page_t*	page,	/*!< in: segment inode page */
	ulint	i,	/*!< in: search forward starting from this index */
	ulint	zip_size,/*!< in: compressed page size, or 0 */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	for (; i < FSP_SEG_INODES_PER_PAGE(zip_size); i++) {

		fseg_inode_t*	inode;

		inode = fsp_seg_inode_page_get_nth_inode(
			page, i, zip_size, mtr);

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
@return	TRUE if could be allocated */
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
	ulint		zip_size;

	ut_ad(page_offset(space_header) == FSP_HEADER_OFFSET);

	space = page_get_space_id(page_align(space_header));

	zip_size = fsp_flags_get_zip_size(
		mach_read_from_4(FSP_SPACE_FLAGS + space_header));

	block = fsp_alloc_free_page(space, zip_size, 0, mtr, mtr);

	if (block == NULL) {

		return(FALSE);
	}

	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);
	ut_ad(rw_lock_get_x_lock_count(&block->lock) == 1);

	block->check_index_page_at_flush = FALSE;

	page = buf_block_get_frame(block);

	mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_INODE,
			 MLOG_2BYTES, mtr);

	for (ulint i = 0; i < FSP_SEG_INODES_PER_PAGE(zip_size); i++) {

		inode = fsp_seg_inode_page_get_nth_inode(
			page, i, zip_size, mtr);

		mlog_write_ull(inode + FSEG_ID, 0, mtr);
	}

	flst_add_last(
		space_header + FSP_SEG_INODES_FREE,
		page + FSEG_INODE_PAGE_NODE, mtr);

	return(TRUE);
}

/**********************************************************************//**
Allocates a new file segment inode.
@return	segment inode, or NULL if not enough space */
static
fseg_inode_t*
fsp_alloc_seg_inode(
/*================*/
	fsp_header_t*	space_header,	/*!< in: space header */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	ulint		page_no;
	buf_block_t*	block;
	page_t*		page;
	fseg_inode_t*	inode;
	ibool		success;
	ulint		zip_size;
	ulint		n;

	ut_ad(page_offset(space_header) == FSP_HEADER_OFFSET);

	if (flst_get_len(space_header + FSP_SEG_INODES_FREE, mtr) == 0) {
		/* Allocate a new segment inode page */

		success = fsp_alloc_seg_inode_page(space_header, mtr);

		if (!success) {

			return(NULL);
		}
	}

	page_no = flst_get_first(space_header + FSP_SEG_INODES_FREE, mtr).page;

	zip_size = fsp_flags_get_zip_size(
		mach_read_from_4(FSP_SPACE_FLAGS + space_header));
	block = buf_page_get(page_get_space_id(page_align(space_header)),
			     zip_size, page_no, RW_X_LATCH, mtr);
	buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

	page = buf_block_get_frame(block);

	n = fsp_seg_inode_page_find_free(page, 0, zip_size, mtr);

	ut_a(n != ULINT_UNDEFINED);

	inode = fsp_seg_inode_page_get_nth_inode(page, n, zip_size, mtr);

	if (ULINT_UNDEFINED == fsp_seg_inode_page_find_free(page, n + 1,
							    zip_size, mtr)) {
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

/**********************************************************************//**
Frees a file segment inode. */
static
void
fsp_free_seg_inode(
/*===============*/
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	fseg_inode_t*	inode,	/*!< in: segment inode */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	page_t*		page;
	fsp_header_t*	space_header;

	page = page_align(inode);

	space_header = fsp_get_space_header(space, zip_size, mtr);

	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	if (ULINT_UNDEFINED
	    == fsp_seg_inode_page_find_free(page, 0, zip_size, mtr)) {

		/* Move the page to another list */

		flst_remove(space_header + FSP_SEG_INODES_FULL,
			    page + FSEG_INODE_PAGE_NODE, mtr);

		flst_add_last(space_header + FSP_SEG_INODES_FREE,
			      page + FSEG_INODE_PAGE_NODE, mtr);
	}

	mlog_write_ull(inode + FSEG_ID, 0, mtr);
	mlog_write_ulint(inode + FSEG_MAGIC_N, 0xfa051ce3, MLOG_4BYTES, mtr);

	if (ULINT_UNDEFINED
	    == fsp_seg_inode_page_find_used(page, zip_size, mtr)) {

		/* There are no other used headers left on the page: free it */

		flst_remove(space_header + FSP_SEG_INODES_FREE,
			    page + FSEG_INODE_PAGE_NODE, mtr);

		fsp_free_page(space, zip_size, page_get_page_no(page), mtr);
	}
}

/**********************************************************************//**
Returns the file segment inode, page x-latched.
@return	segment inode, page x-latched; NULL if the inode is free */
static
fseg_inode_t*
fseg_inode_try_get(
/*===============*/
	fseg_header_t*	header,	/*!< in: segment header */
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	fil_addr_t	inode_addr;
	fseg_inode_t*	inode;

	inode_addr.page = mach_read_from_4(header + FSEG_HDR_PAGE_NO);
	inode_addr.boffset = mach_read_from_2(header + FSEG_HDR_OFFSET);
	ut_ad(space == mach_read_from_4(header + FSEG_HDR_SPACE));

	inode = fut_get_ptr(space, zip_size, inode_addr, RW_X_LATCH, mtr);

	if (UNIV_UNLIKELY(!mach_read_from_8(inode + FSEG_ID))) {

		inode = NULL;
	} else {
		ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N)
		      == FSEG_MAGIC_N_VALUE);
	}

	return(inode);
}

/**********************************************************************//**
Returns the file segment inode, page x-latched.
@return	segment inode, page x-latched */
static
fseg_inode_t*
fseg_inode_get(
/*===========*/
	fseg_header_t*	header,	/*!< in: segment header */
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	fseg_inode_t*	inode
		= fseg_inode_try_get(header, space, zip_size, mtr);
	ut_a(inode);
	return(inode);
}

/**********************************************************************//**
Gets the page number from the nth fragment page slot.
@return	page number, FIL_NULL if not in use */
UNIV_INLINE
ulint
fseg_get_nth_frag_page_no(
/*======================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	ulint		n,	/*!< in: slot index */
	mtr_t*		mtr __attribute__((unused)))
				/*!< in/out: mini-transaction */
{
	ut_ad(inode && mtr);
	ut_ad(n < FSEG_FRAG_ARR_N_SLOTS);
	ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_X_FIX));
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
	ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_X_FIX));
	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	mlog_write_ulint(inode + FSEG_FRAG_ARR + n * FSEG_FRAG_SLOT_SIZE,
			 page_no, MLOG_4BYTES, mtr);
}

/**********************************************************************//**
Finds a fragment page slot which is free.
@return	slot index; ULINT_UNDEFINED if none found */
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
@return	slot index; ULINT_UNDEFINED if none found */
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
@return	number of fragment pages */
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
UNIV_INTERN
buf_block_t*
fseg_create_general(
/*================*/
	ulint	space,	/*!< in: space id */
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
	ulint		flags;
	ulint		zip_size;
	fsp_header_t*	space_header;
	fseg_inode_t*	inode;
	ib_id_t		seg_id;
	buf_block_t*	block	= 0; /* remove warning */
	fseg_header_t*	header	= 0; /* remove warning */
	rw_lock_t*	latch;
	ibool		success;
	ulint		n_reserved;
	ulint		i;

	ut_ad(mtr);
	ut_ad(byte_offset + FSEG_HEADER_SIZE
	      <= UNIV_PAGE_SIZE - FIL_PAGE_DATA_END);

	latch = fil_space_get_latch(space, &flags);
	zip_size = fsp_flags_get_zip_size(flags);

	if (page != 0) {
		block = buf_page_get(space, zip_size, page, RW_X_LATCH, mtr);
		header = byte_offset + buf_block_get_frame(block);
	}

	mtr_x_lock(latch, mtr);

	if (rw_lock_get_x_lock_count(latch) == 1) {
		/* This thread did not own the latch before this call: free
		excess pages from the insert buffer free list */

		if (space == IBUF_SPACE_ID) {
			ibuf_free_excess_pages();
		}
	}

	if (!has_done_reservation) {
		success = fsp_reserve_free_extents(&n_reserved, space, 2,
						   FSP_NORMAL, mtr);
		if (!success) {
			return(NULL);
		}
	}

	space_header = fsp_get_space_header(space, zip_size, mtr);

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
		block = fseg_alloc_free_page_low(space, zip_size,
						 inode, 0, FSP_UP, mtr, mtr);

		if (block == NULL) {

			fsp_free_seg_inode(space, zip_size, inode, mtr);

			goto funct_exit;
		}

		ut_ad(rw_lock_get_x_lock_count(&block->lock) == 1);

		header = byte_offset + buf_block_get_frame(block);
		mlog_write_ulint(buf_block_get_frame(block) + FIL_PAGE_TYPE,
				 FIL_PAGE_TYPE_SYS, MLOG_2BYTES, mtr);
	}

	mlog_write_ulint(header + FSEG_HDR_OFFSET,
			 page_offset(inode), MLOG_2BYTES, mtr);

	mlog_write_ulint(header + FSEG_HDR_PAGE_NO,
			 page_get_page_no(page_align(inode)),
			 MLOG_4BYTES, mtr);

	mlog_write_ulint(header + FSEG_HDR_SPACE, space, MLOG_4BYTES, mtr);

funct_exit:
	if (!has_done_reservation) {

		fil_space_release_free_extents(space, n_reserved);
	}

	return(block);
}

/**********************************************************************//**
Creates a new segment.
@return the block where the segment header is placed, x-latched, NULL
if could not create segment because of lack of space */
UNIV_INTERN
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
@return	number of reserved pages */
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
	ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_X_FIX));

	*used = mtr_read_ulint(inode + FSEG_NOT_FULL_N_USED, MLOG_4BYTES, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FULL, mtr)
		+ fseg_get_n_frag_pages(inode, mtr);

	ret = fseg_get_n_frag_pages(inode, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FREE, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_NOT_FULL, mtr)
		+ FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FULL, mtr);

	return(ret);
}

/**********************************************************************//**
Calculates the number of pages reserved by a segment, and how many pages are
currently used.
@return	number of reserved pages */
UNIV_INTERN
ulint
fseg_n_reserved_pages(
/*==================*/
	fseg_header_t*	header,	/*!< in: segment header */
	ulint*		used,	/*!< out: number of pages used (<= reserved) */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint		ret;
	fseg_inode_t*	inode;
	ulint		space;
	ulint		flags;
	ulint		zip_size;
	rw_lock_t*	latch;

	space = page_get_space_id(page_align(header));
	latch = fil_space_get_latch(space, &flags);
	zip_size = fsp_flags_get_zip_size(flags);

	mtr_x_lock(latch, mtr);

	inode = fseg_inode_get(header, space, zip_size, mtr);

	ret = fseg_n_reserved_pages_low(inode, used, mtr);

	return(ret);
}

/*********************************************************************//**
Tries to fill the free list of a segment with consecutive free extents.
This happens if the segment is big enough to allow extents in the free list,
the free list is empty, and the extents can be allocated consecutively from
the hint onward. */
static
void
fseg_fill_free_list(
/*================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint		hint,	/*!< in: hint which extent would be good as
				the first extent */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	xdes_t*	descr;
	ulint	i;
	ib_id_t	seg_id;
	ulint	reserved;
	ulint	used;

	ut_ad(inode && mtr);
	ut_ad(!((page_offset(inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));

	reserved = fseg_n_reserved_pages_low(inode, &used, mtr);

	if (reserved < FSEG_FREE_LIST_LIMIT * FSP_EXTENT_SIZE) {

		/* The segment is too small to allow extents in free list */

		return;
	}

	if (flst_get_len(inode + FSEG_FREE, mtr) > 0) {
		/* Free list is not empty */

		return;
	}

	for (i = 0; i < FSEG_FREE_LIST_MAX_LEN; i++) {
		descr = xdes_get_descriptor(space, zip_size, hint, mtr);

		if ((descr == NULL)
		    || (XDES_FREE != xdes_get_state(descr, mtr))) {

			/* We cannot allocate the desired extent: stop */

			return;
		}

		descr = fsp_alloc_free_extent(space, zip_size, hint, mtr);

		xdes_set_state(descr, XDES_FSEG, mtr);

		seg_id = mach_read_from_8(inode + FSEG_ID);
		ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N)
		      == FSEG_MAGIC_N_VALUE);
		mlog_write_ull(descr + XDES_ID, seg_id, mtr);

		flst_add_last(inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);
		hint += FSP_EXTENT_SIZE;
	}
}

/*********************************************************************//**
Allocates a free extent for the segment: looks first in the free list of the
segment, then tries to allocate from the space free list. NOTE that the extent
returned still resides in the segment free list, it is not yet taken off it!
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block (not allocated or initialized) otherwise */
static
xdes_t*
fseg_alloc_free_extent(
/*===================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	xdes_t*		descr;
	ib_id_t		seg_id;
	fil_addr_t	first;

	ut_ad(!((page_offset(inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	if (flst_get_len(inode + FSEG_FREE, mtr) > 0) {
		/* Segment free list is not empty, allocate from it */

		first = flst_get_first(inode + FSEG_FREE, mtr);

		descr = xdes_lst_get_descriptor(space, zip_size, first, mtr);
	} else {
		/* Segment free list was empty, allocate from space */
		descr = fsp_alloc_free_extent(space, zip_size, 0, mtr);

		if (descr == NULL) {

			return(NULL);
		}

		seg_id = mach_read_from_8(inode + FSEG_ID);

		xdes_set_state(descr, XDES_FSEG, mtr);
		mlog_write_ull(descr + XDES_ID, seg_id, mtr);
		flst_add_last(inode + FSEG_FREE, descr + XDES_FLST_NODE, mtr);

		/* Try to fill the segment free list */
		fseg_fill_free_list(inode, space, zip_size,
				    xdes_get_offset(descr) + FSP_EXTENT_SIZE,
				    mtr);
	}

	return(descr);
}

/**********************************************************************//**
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation.
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block (not allocated or initialized) otherwise */
static
buf_block_t*
fseg_alloc_free_page_low(
/*=====================*/
	ulint		space,	/*!< in: space */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	fseg_inode_t*	seg_inode, /*!< in/out: segment inode */
	ulint		hint,	/*!< in: hint of which page would be
				desirable */
	byte		direction, /*!< in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	mtr_t*		mtr,	/*!< in/out: mini-transaction */
	mtr_t*		init_mtr)/*!< in/out: mtr or another mini-transaction
				in which the page should be initialized.
				If init_mtr!=mtr, but the page is already
				latched in mtr, do not initialize the page. */
{
	fsp_header_t*	space_header;
	ulint		space_size;
	ib_id_t		seg_id;
	ulint		used;
	ulint		reserved;
	xdes_t*		descr;		/*!< extent of the hinted page */
	ulint		ret_page;	/*!< the allocated page offset, FIL_NULL
					if could not be allocated */
	xdes_t*		ret_descr;	/*!< the extent of the allocated page */
	ibool		success;
	ulint		n;

	ut_ad(mtr);
	ut_ad((direction >= FSP_UP) && (direction <= FSP_NO_DIR));
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N)
	      == FSEG_MAGIC_N_VALUE);
	ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));
	seg_id = mach_read_from_8(seg_inode + FSEG_ID);

	ut_ad(seg_id);

	reserved = fseg_n_reserved_pages_low(seg_inode, &used, mtr);

	space_header = fsp_get_space_header(space, zip_size, mtr);

	descr = xdes_get_descriptor_with_space_hdr(space_header, space,
						   hint, mtr);
	if (descr == NULL) {
		/* Hint outside space or too high above free limit: reset
		hint */
		/* The file space header page is always allocated. */
		hint = 0;
		descr = xdes_get_descriptor(space, zip_size, hint, mtr);
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
		ret_descr = fsp_alloc_free_extent(space, zip_size, hint, mtr);

		ut_a(ret_descr == descr);

		xdes_set_state(ret_descr, XDES_FSEG, mtr);
		mlog_write_ull(ret_descr + XDES_ID, seg_id, mtr);
		flst_add_last(seg_inode + FSEG_FREE,
			      ret_descr + XDES_FLST_NODE, mtr);

		/* Try to fill the segment free list */
		fseg_fill_free_list(seg_inode, space, zip_size,
				    hint + FSP_EXTENT_SIZE, mtr);
		goto take_hinted_page;
		/*-----------------------------------------------------------*/
	} else if ((direction != FSP_NO_DIR)
		   && ((reserved - used) < reserved / FSEG_FILLFACTOR)
		   && (used >= FSEG_FRAG_LIMIT)
		   && (!!(ret_descr
			  = fseg_alloc_free_extent(seg_inode,
						   space, zip_size, mtr)))) {

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
		/*-----------------------------------------------------------*/
	} else if (reserved - used > 0) {
		/* 5. We take any unused page from the segment
		==============================================*/
		fil_addr_t	first;

		if (flst_get_len(seg_inode + FSEG_NOT_FULL, mtr) > 0) {
			first = flst_get_first(seg_inode + FSEG_NOT_FULL,
					       mtr);
		} else if (flst_get_len(seg_inode + FSEG_FREE, mtr) > 0) {
			first = flst_get_first(seg_inode + FSEG_FREE, mtr);
		} else {
			ut_error;
			return(NULL);
		}

		ret_descr = xdes_lst_get_descriptor(space, zip_size,
						    first, mtr);
		ret_page = xdes_get_offset(ret_descr)
			+ xdes_find_bit(ret_descr, XDES_FREE_BIT, TRUE,
					0, mtr);
		/*-----------------------------------------------------------*/
	} else if (used < FSEG_FRAG_LIMIT) {
		/* 6. We allocate an individual page from the space
		===================================================*/
		buf_block_t* block = fsp_alloc_free_page(
			space, zip_size, hint, mtr, init_mtr);

		if (block != NULL) {
			/* Put the page in the fragment page array of the
			segment */
			n = fseg_find_free_frag_page_slot(seg_inode, mtr);
			ut_a(n != ULINT_UNDEFINED);

			fseg_set_nth_frag_page_no(
				seg_inode, n, buf_block_get_page_no(block),
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
						   space, zip_size, mtr);

		if (ret_descr == NULL) {
			ret_page = FIL_NULL;
		} else {
			ret_page = xdes_get_offset(ret_descr);
		}
	}

	if (ret_page == FIL_NULL) {
		/* Page could not be allocated */

		return(NULL);
	}

	if (space != 0) {
		space_size = fil_space_get_size(space);

		if (space_size <= ret_page) {
			/* It must be that we are extending a single-table
			tablespace whose size is still < 64 pages */

			if (ret_page >= FSP_EXTENT_SIZE) {
				fprintf(stderr,
					"InnoDB: Error (2): trying to extend"
					" a single-table tablespace %lu\n"
					"InnoDB: by single page(s) though"
					" the space size %lu. Page no %lu.\n",
					(ulong) space, (ulong) space_size,
					(ulong) ret_page);
				return(NULL);
			}

			success = fsp_try_extend_data_file_with_pages(
				space, ret_page, space_header, mtr);
			if (!success) {
				/* No disk space left */
				return(NULL);
			}
		}
	}

got_hinted_page:
	/* ret_descr == NULL if the block was allocated from free_frag
	(XDES_FREE_FRAG) */
	if (ret_descr != NULL) {
		/* At this point we know the extent and the page offset.
		The extent is still in the appropriate list (FSEG_NOT_FULL
		or FSEG_FREE), and the page is not yet marked as used. */

		ut_ad(xdes_get_descriptor(space, zip_size, ret_page, mtr)
		      == ret_descr);

		ut_ad(xdes_mtr_get_bit(
				ret_descr, XDES_FREE_BIT,
				ret_page % FSP_EXTENT_SIZE, mtr));

		fseg_mark_page_used(seg_inode, ret_page, ret_descr, mtr);
	}

	return(fsp_page_create(
		       space, fsp_flags_get_zip_size(
			       mach_read_from_4(FSP_SPACE_FLAGS
						+ space_header)),
		       ret_page, mtr, init_mtr));
}

/**********************************************************************//**
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation.
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block (not allocated or initialized) otherwise */
UNIV_INTERN
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
	ulint		space;
	ulint		flags;
	ulint		zip_size;
	rw_lock_t*	latch;
	buf_block_t*	block;
	ulint		n_reserved;

	space = page_get_space_id(page_align(seg_header));

	latch = fil_space_get_latch(space, &flags);

	zip_size = fsp_flags_get_zip_size(flags);

	mtr_x_lock(latch, mtr);

	if (rw_lock_get_x_lock_count(latch) == 1) {
		/* This thread did not own the latch before this call: free
		excess pages from the insert buffer free list */

		if (space == IBUF_SPACE_ID) {
			ibuf_free_excess_pages();
		}
	}

	inode = fseg_inode_get(seg_header, space, zip_size, mtr);

	if (!has_done_reservation
	    && !fsp_reserve_free_extents(&n_reserved, space, 2,
					 FSP_NORMAL, mtr)) {
		return(NULL);
	}

	block = fseg_alloc_free_page_low(space, zip_size,
					 inode, hint, direction,
					 mtr, init_mtr);
	if (!has_done_reservation) {
		fil_space_release_free_extents(space, n_reserved);
	}

	return(block);
}

/**********************************************************************//**
Checks that we have at least 2 frag pages free in the first extent of a
single-table tablespace, and they are also physically initialized to the data
file. That is we have already extended the data file so that those pages are
inside the data file. If not, this function extends the tablespace with
pages.
@return	TRUE if there were >= 3 free pages, or we were able to extend */
static
ibool
fsp_reserve_free_pages(
/*===================*/
	ulint		space,		/*!< in: space id, must be != 0 */
	fsp_header_t*	space_header,	/*!< in: header of that space,
					x-latched */
	ulint		size,		/*!< in: size of the tablespace in
					pages, must be < FSP_EXTENT_SIZE/2 */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
{
	xdes_t*	descr;
	ulint	n_used;

	ut_a(space != 0);
	ut_a(size < FSP_EXTENT_SIZE / 2);

	descr = xdes_get_descriptor_with_space_hdr(space_header, space, 0,
						   mtr);
	n_used = xdes_get_n_used(descr, mtr);

	ut_a(n_used <= size);

	if (size >= n_used + 2) {

		return(TRUE);
	}

	return(fsp_try_extend_data_file_with_pages(space, n_used + 1,
						   space_header, mtr));
}

/**********************************************************************//**
Reserves free pages from a tablespace. All mini-transactions which may
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

Single-table tablespaces whose size is < 32 pages are a special case. In this
function we would liberally reserve several 64 page extents for every page
split or merge in a B-tree. But we do not want to waste disk space if the table
only occupies < 32 pages. That is why we apply different rules in that special
case, just ensuring that there are 3 free pages available.
@return	TRUE if we were able to make the reservation */
UNIV_INTERN
ibool
fsp_reserve_free_extents(
/*=====================*/
	ulint*	n_reserved,/*!< out: number of extents actually reserved; if we
			return TRUE and the tablespace size is < 64 pages,
			then this can be 0, otherwise it is n_ext */
	ulint	space,	/*!< in: space id */
	ulint	n_ext,	/*!< in: number of extents to reserve */
	ulint	alloc_type,/*!< in: FSP_NORMAL, FSP_UNDO, or FSP_CLEANING */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	fsp_header_t*	space_header;
	rw_lock_t*	latch;
	ulint		n_free_list_ext;
	ulint		free_limit;
	ulint		size;
	ulint		flags;
	ulint		zip_size;
	ulint		n_free;
	ulint		n_free_up;
	ulint		reserve;
	ibool		success;
	ulint		n_pages_added;

	ut_ad(mtr);
	*n_reserved = n_ext;

	latch = fil_space_get_latch(space, &flags);
	zip_size = fsp_flags_get_zip_size(flags);

	mtr_x_lock(latch, mtr);

	space_header = fsp_get_space_header(space, zip_size, mtr);
try_again:
	size = mtr_read_ulint(space_header + FSP_SIZE, MLOG_4BYTES, mtr);

	if (size < FSP_EXTENT_SIZE / 2) {
		/* Use different rules for small single-table tablespaces */
		*n_reserved = 0;
		return(fsp_reserve_free_pages(space, space_header, size, mtr));
	}

	n_free_list_ext = flst_get_len(space_header + FSP_FREE, mtr);

	free_limit = mtr_read_ulint(space_header + FSP_FREE_LIMIT,
				    MLOG_4BYTES, mtr);

	/* Below we play safe when counting free extents above the free limit:
	some of them will contain extent descriptor pages, and therefore
	will not be free extents */

	n_free_up = (size - free_limit) / FSP_EXTENT_SIZE;

	if (n_free_up > 0) {
		n_free_up--;
		if (!zip_size) {
			n_free_up -= n_free_up
				/ (UNIV_PAGE_SIZE / FSP_EXTENT_SIZE);
		} else {
			n_free_up -= n_free_up
				/ (zip_size / FSP_EXTENT_SIZE);
		}
	}

	n_free = n_free_list_ext + n_free_up;

	if (alloc_type == FSP_NORMAL) {
		/* We reserve 1 extent + 0.5 % of the space size to undo logs
		and 1 extent + 0.5 % to cleaning operations; NOTE: this source
		code is duplicated in the function below! */

		reserve = 2 + ((size / FSP_EXTENT_SIZE) * 2) / 200;

		if (n_free <= reserve + n_ext) {

			goto try_to_extend;
		}
	} else if (alloc_type == FSP_UNDO) {
		/* We reserve 0.5 % of the space size to cleaning operations */

		reserve = 1 + ((size / FSP_EXTENT_SIZE) * 1) / 200;

		if (n_free <= reserve + n_ext) {

			goto try_to_extend;
		}
	} else {
		ut_a(alloc_type == FSP_CLEANING);
	}

	success = fil_space_reserve_free_extents(space, n_free, n_ext);

	if (success) {
		return(TRUE);
	}
try_to_extend:
	success = fsp_try_extend_data_file(&n_pages_added, space,
					   space_header, mtr);
	if (success && n_pages_added > 0) {

		goto try_again;
	}

	return(FALSE);
}

/**********************************************************************//**
This function should be used to get information on how much we still
will be able to insert new data to the database without running out the
tablespace. Only free extents are taken into account and we also subtract
the safety margin required by the above function fsp_reserve_free_extents.
@return	available space in kB */
UNIV_INTERN
ullint
fsp_get_available_space_in_free_extents(
/*====================================*/
	ulint	space)	/*!< in: space id */
{
	fsp_header_t*	space_header;
	ulint		n_free_list_ext;
	ulint		free_limit;
	ulint		size;
	ulint		flags;
	ulint		zip_size;
	ulint		n_free;
	ulint		n_free_up;
	ulint		reserve;
	rw_lock_t*	latch;
	mtr_t		mtr;

	/* The convoluted mutex acquire is to overcome latching order
	issues: The problem is that the fil_mutex is at a lower level
	than the tablespace latch and the buffer pool mutex. We have to
	first prevent any operations on the file system by acquiring the
	dictionary mutex. Then acquire the tablespace latch to obey the
	latching order and then release the dictionary mutex. That way we
	ensure that the tablespace instance can't be freed while we are
	examining its contents (see fil_space_free()).

	However, there is one further complication, we release the fil_mutex
	when we need to invalidate the the pages in the buffer pool and we
	reacquire the fil_mutex when deleting and freeing the tablespace
	instance in fil0fil.cc. Here we need to account for that situation
	too. */

	mutex_enter(&dict_sys->mutex);

	/* At this stage there is no guarantee that the tablespace even
	exists in the cache. */

	if (fil_tablespace_deleted_or_being_deleted_in_mem(space, -1)) {

		mutex_exit(&dict_sys->mutex);

		return(ULLINT_UNDEFINED);
	}

	mtr_start(&mtr);

	latch = fil_space_get_latch(space, &flags);

	/* This should ensure that the tablespace instance can't be freed
	by another thread. However, the tablespace pages can still be freed
	from the buffer pool. We need to check for that again. */

	zip_size = fsp_flags_get_zip_size(flags);

	mtr_x_lock(latch, &mtr);

	mutex_exit(&dict_sys->mutex);

	/* At this point it is possible for the tablespace to be deleted and
	its pages removed from the buffer pool. We need to check for that
	situation. However, the tablespace instance can't be deleted because
	our latching above should ensure that. */

	if (fil_tablespace_is_being_deleted(space)) {

		mtr_commit(&mtr);

		return(ULLINT_UNDEFINED);
	}

	/* From here on even if the user has dropped the tablespace, the
	pages _must_ still exist in the buffer pool and the tablespace
	instance _must_ be in the file system hash table. */

	space_header = fsp_get_space_header(space, zip_size, &mtr);

	size = mtr_read_ulint(space_header + FSP_SIZE, MLOG_4BYTES, &mtr);

	n_free_list_ext = flst_get_len(space_header + FSP_FREE, &mtr);

	free_limit = mtr_read_ulint(space_header + FSP_FREE_LIMIT,
				    MLOG_4BYTES, &mtr);
	mtr_commit(&mtr);

	if (size < FSP_EXTENT_SIZE) {
		ut_a(space != 0);	/* This must be a single-table
					tablespace */

		return(0);		/* TODO: count free frag pages and
					return a value based on that */
	}

	/* Below we play safe when counting free extents above the free limit:
	some of them will contain extent descriptor pages, and therefore
	will not be free extents */

	n_free_up = (size - free_limit) / FSP_EXTENT_SIZE;

	if (n_free_up > 0) {
		n_free_up--;
		if (!zip_size) {
			n_free_up -= n_free_up
				/ (UNIV_PAGE_SIZE / FSP_EXTENT_SIZE);
		} else {
			n_free_up -= n_free_up
				/ (zip_size / FSP_EXTENT_SIZE);
		}
	}

	n_free = n_free_list_ext + n_free_up;

	/* We reserve 1 extent + 0.5 % of the space size to undo logs
	and 1 extent + 0.5 % to cleaning operations; NOTE: this source
	code is duplicated in the function above! */

	reserve = 2 + ((size / FSP_EXTENT_SIZE) * 2) / 200;

	if (reserve > n_free) {
		return(0);
	}

	if (!zip_size) {
		return((ullint) (n_free - reserve)
		       * FSP_EXTENT_SIZE
		       * (UNIV_PAGE_SIZE / 1024));
	} else {
		return((ullint) (n_free - reserve)
		       * FSP_EXTENT_SIZE
		       * (zip_size / 1024));
	}
}

/********************************************************************//**
Marks a page used. The page must reside within the extents of the given
segment. */
static __attribute__((nonnull))
void
fseg_mark_page_used(
/*================*/
	fseg_inode_t*	seg_inode,/*!< in: segment inode */
	ulint		page,	/*!< in: page offset */
	xdes_t*		descr,  /*!< in: extent descriptor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	not_full_n_used;

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

/**********************************************************************//**
Frees a single page of a segment. */
static
void
fseg_free_page_low(
/*===============*/
	fseg_inode_t*	seg_inode, /*!< in: segment inode */
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint		page,	/*!< in: page offset */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	xdes_t*	descr;
	ulint	not_full_n_used;
	ulint	state;
	ib_id_t	descr_id;
	ib_id_t	seg_id;
	ulint	i;

	ut_ad(seg_inode && mtr);
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N)
	      == FSEG_MAGIC_N_VALUE);
	ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));

	/* Drop search system page hash index if the page is found in
	the pool and is hashed */

	btr_search_drop_page_hash_when_freed(space, zip_size, page);

	descr = xdes_get_descriptor(space, zip_size, page, mtr);

	if (xdes_mtr_get_bit(descr, XDES_FREE_BIT,
			     page % FSP_EXTENT_SIZE, mtr)) {
		fputs("InnoDB: Dump of the tablespace extent descriptor: ",
		      stderr);
		ut_print_buf(stderr, descr, 40);

		fprintf(stderr, "\n"
			"InnoDB: Serious error! InnoDB is trying to"
			" free page %lu\n"
			"InnoDB: though it is already marked as free"
			" in the tablespace!\n"
			"InnoDB: The tablespace free space info is corrupt.\n"
			"InnoDB: You may need to dump your"
			" InnoDB tables and recreate the whole\n"
			"InnoDB: database!\n", (ulong) page);
crash:
		fputs("InnoDB: Please refer to\n"
		      "InnoDB: " REFMAN "forcing-innodb-recovery.html\n"
		      "InnoDB: about forcing recovery.\n", stderr);
		ut_error;
	}

	state = xdes_get_state(descr, mtr);

	if (state != XDES_FSEG) {
		/* The page is in the fragment pages of the segment */

		for (i = 0;; i++) {
			if (fseg_get_nth_frag_page_no(seg_inode, i, mtr)
			    == page) {

				fseg_set_nth_frag_page_no(seg_inode, i,
							  FIL_NULL, mtr);
				break;
			}
		}

		fsp_free_page(space, zip_size, page, mtr);

		return;
	}

	/* If we get here, the page is in some extent of the segment */

	descr_id = mach_read_from_8(descr + XDES_ID);
	seg_id = mach_read_from_8(seg_inode + FSEG_ID);
#if 0
	fprintf(stderr,
		"InnoDB: InnoDB is freeing space %lu page %lu,\n"
		"InnoDB: which belongs to descr seg %llu\n"
		"InnoDB: segment %llu.\n",
		(ulong) space, (ulong) page,
		(ullint) descr_id,
		(ullint) seg_id);
#endif /* 0 */
	if (UNIV_UNLIKELY(descr_id != seg_id)) {
		fputs("InnoDB: Dump of the tablespace extent descriptor: ",
		      stderr);
		ut_print_buf(stderr, descr, 40);
		fputs("\nInnoDB: Dump of the segment inode: ", stderr);
		ut_print_buf(stderr, seg_inode, 40);
		putc('\n', stderr);

		fprintf(stderr,
			"InnoDB: Serious error: InnoDB is trying to"
			" free space %lu page %lu,\n"
			"InnoDB: which does not belong to"
			" segment %llu but belongs\n"
			"InnoDB: to segment %llu.\n",
			(ulong) space, (ulong) page,
			(ullint) descr_id,
			(ullint) seg_id);
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

	xdes_set_bit(descr, XDES_FREE_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);
	xdes_set_bit(descr, XDES_CLEAN_BIT, page % FSP_EXTENT_SIZE, TRUE, mtr);

	if (xdes_is_free(descr, mtr)) {
		/* The extent has become free: free it to space */
		flst_remove(seg_inode + FSEG_NOT_FULL,
			    descr + XDES_FLST_NODE, mtr);
		fsp_free_extent(space, zip_size, page, mtr);
	}

	mtr->n_freed_pages++;
}

/**********************************************************************//**
Frees a single page of a segment. */
UNIV_INTERN
void
fseg_free_page(
/*===========*/
	fseg_header_t*	seg_header, /*!< in: segment header */
	ulint		space,	/*!< in: space id */
	ulint		page,	/*!< in: page offset */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint		flags;
	ulint		zip_size;
	fseg_inode_t*	seg_inode;
	rw_lock_t*	latch;

	latch = fil_space_get_latch(space, &flags);
	zip_size = fsp_flags_get_zip_size(flags);

	mtr_x_lock(latch, mtr);

	seg_inode = fseg_inode_get(seg_header, space, zip_size, mtr);

	fseg_free_page_low(seg_inode, space, zip_size, page, mtr);

#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	buf_page_set_file_page_was_freed(space, page);
#endif /* UNIV_DEBUG_FILE_ACCESSES || UNIV_DEBUG */
}

/**********************************************************************//**
Checks if a single page of a segment is free.
@return	true if free */
UNIV_INTERN
bool
fseg_page_is_free(
/*==============*/
	fseg_header_t*	seg_header,	/*!< in: segment header */
	ulint		space,		/*!< in: space id */
	ulint		page)		/*!< in: page offset */
{
	mtr_t		mtr;
	ibool		is_free;
	ulint		flags;
	rw_lock_t*	latch;
	xdes_t*		descr;
	ulint		zip_size;
	fseg_inode_t*	seg_inode;

	latch = fil_space_get_latch(space, &flags);
	zip_size = dict_tf_get_zip_size(flags);

	mtr_start(&mtr);
	mtr_x_lock(latch, &mtr);

	seg_inode = fseg_inode_get(seg_header, space, zip_size, &mtr);

	ut_a(seg_inode);
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N)
	      == FSEG_MAGIC_N_VALUE);
	ut_ad(!((page_offset(seg_inode) - FSEG_ARR_OFFSET) % FSEG_INODE_SIZE));

	descr = xdes_get_descriptor(space, zip_size, page, &mtr);
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
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint		page,	/*!< in: a page in the extent */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint	first_page_in_extent;
	xdes_t*	descr;
	ulint	not_full_n_used;
	ulint	descr_n_used;
	ulint	i;

	ut_ad(seg_inode && mtr);

	descr = xdes_get_descriptor(space, zip_size, page, mtr);

	ut_a(xdes_get_state(descr, mtr) == XDES_FSEG);
	ut_a(!memcmp(descr + XDES_ID, seg_inode + FSEG_ID, 8));
	ut_ad(mach_read_from_4(seg_inode + FSEG_MAGIC_N)
	      == FSEG_MAGIC_N_VALUE);

	first_page_in_extent = page - (page % FSP_EXTENT_SIZE);

	for (i = 0; i < FSP_EXTENT_SIZE; i++) {
		if (!xdes_mtr_get_bit(descr, XDES_FREE_BIT, i, mtr)) {

			/* Drop search system page hash index if the page is
			found in the pool and is hashed */

			btr_search_drop_page_hash_when_freed(
				space, zip_size, first_page_in_extent + i);
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

	fsp_free_extent(space, zip_size, page, mtr);

#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	for (i = 0; i < FSP_EXTENT_SIZE; i++) {

		buf_page_set_file_page_was_freed(space,
						 first_page_in_extent + i);
	}
#endif /* UNIV_DEBUG_FILE_ACCESSES || UNIV_DEBUG */
}

/**********************************************************************//**
Frees part of a segment. This function can be used to free a segment by
repeatedly calling this function in different mini-transactions. Doing
the freeing in a single mini-transaction might result in too big a
mini-transaction.
@return	TRUE if freeing completed */
UNIV_INTERN
ibool
fseg_free_step(
/*===========*/
	fseg_header_t*	header,	/*!< in, own: segment header; NOTE: if the header
				resides on the first page of the frag list
				of the segment, this pointer becomes obsolete
				after the last freeing step */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint		n;
	ulint		page;
	xdes_t*		descr;
	fseg_inode_t*	inode;
	ulint		space;
	ulint		flags;
	ulint		zip_size;
	ulint		header_page;
	rw_lock_t*	latch;

	space = page_get_space_id(page_align(header));
	header_page = page_get_page_no(page_align(header));

	latch = fil_space_get_latch(space, &flags);
	zip_size = fsp_flags_get_zip_size(flags);

	mtr_x_lock(latch, mtr);

	descr = xdes_get_descriptor(space, zip_size, header_page, mtr);

	/* Check that the header resides on a page which has not been
	freed yet */

	ut_a(xdes_mtr_get_bit(descr, XDES_FREE_BIT,
			      header_page % FSP_EXTENT_SIZE, mtr) == FALSE);

	inode = fseg_inode_try_get(header, space, zip_size, mtr);

	if (UNIV_UNLIKELY(inode == NULL)) {
		fprintf(stderr, "double free of inode from %u:%u\n",
			(unsigned) space, (unsigned) header_page);
		return(TRUE);
	}

	descr = fseg_get_first_extent(inode, space, zip_size, mtr);

	if (descr != NULL) {
		/* Free the extent held by the segment */
		page = xdes_get_offset(descr);

		fseg_free_extent(inode, space, zip_size, page, mtr);

		return(FALSE);
	}

	/* Free a frag page */
	n = fseg_find_last_used_frag_page_slot(inode, mtr);

	if (n == ULINT_UNDEFINED) {
		/* Freeing completed: free the segment inode */
		fsp_free_seg_inode(space, zip_size, inode, mtr);

		return(TRUE);
	}

	fseg_free_page_low(inode, space, zip_size,
			   fseg_get_nth_frag_page_no(inode, n, mtr), mtr);

	n = fseg_find_last_used_frag_page_slot(inode, mtr);

	if (n == ULINT_UNDEFINED) {
		/* Freeing completed: free the segment inode */
		fsp_free_seg_inode(space, zip_size, inode, mtr);

		return(TRUE);
	}

	return(FALSE);
}

/**********************************************************************//**
Frees part of a segment. Differs from fseg_free_step because this function
leaves the header page unfreed.
@return	TRUE if freeing completed, except the header page */
UNIV_INTERN
ibool
fseg_free_step_not_header(
/*======================*/
	fseg_header_t*	header,	/*!< in: segment header which must reside on
				the first fragment page of the segment */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ulint		n;
	ulint		page;
	xdes_t*		descr;
	fseg_inode_t*	inode;
	ulint		space;
	ulint		flags;
	ulint		zip_size;
	ulint		page_no;
	rw_lock_t*	latch;

	space = page_get_space_id(page_align(header));

	latch = fil_space_get_latch(space, &flags);
	zip_size = fsp_flags_get_zip_size(flags);

	mtr_x_lock(latch, mtr);

	inode = fseg_inode_get(header, space, zip_size, mtr);

	descr = fseg_get_first_extent(inode, space, zip_size, mtr);

	if (descr != NULL) {
		/* Free the extent held by the segment */
		page = xdes_get_offset(descr);

		fseg_free_extent(inode, space, zip_size, page, mtr);

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

	fseg_free_page_low(inode, space, zip_size, page_no, mtr);

	return(FALSE);
}

/**********************************************************************//**
Returns the first extent descriptor for a segment. We think of the extent
lists of the segment catenated in the order FSEG_FULL -> FSEG_NOT_FULL
-> FSEG_FREE.
@return	the first extent descriptor, or NULL if none */
static
xdes_t*
fseg_get_first_extent(
/*==================*/
	fseg_inode_t*	inode,	/*!< in: segment inode */
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	fil_addr_t	first;
	xdes_t*		descr;

	ut_ad(inode && mtr);

	ut_ad(space == page_get_space_id(page_align(inode)));
	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	first = fil_addr_null;

	if (flst_get_len(inode + FSEG_FULL, mtr) > 0) {

		first = flst_get_first(inode + FSEG_FULL, mtr);

	} else if (flst_get_len(inode + FSEG_NOT_FULL, mtr) > 0) {

		first = flst_get_first(inode + FSEG_NOT_FULL, mtr);

	} else if (flst_get_len(inode + FSEG_FREE, mtr) > 0) {

		first = flst_get_first(inode + FSEG_FREE, mtr);
	}

	if (first.page == FIL_NULL) {

		return(NULL);
	}
	descr = xdes_lst_get_descriptor(space, zip_size, first, mtr);

	return(descr);
}

/*******************************************************************//**
Validates a segment.
@return	TRUE if ok */
static
ibool
fseg_validate_low(
/*==============*/
	fseg_inode_t*	inode, /*!< in: segment inode */
	mtr_t*		mtr2)	/*!< in/out: mini-transaction */
{
	ulint		space;
	ib_id_t		seg_id;
	mtr_t		mtr;
	xdes_t*		descr;
	fil_addr_t	node_addr;
	ulint		n_used		= 0;
	ulint		n_used2		= 0;

	ut_ad(mtr_memo_contains_page(mtr2, inode, MTR_MEMO_PAGE_X_FIX));
	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);

	space = page_get_space_id(page_align(inode));

	seg_id = mach_read_from_8(inode + FSEG_ID);
	n_used = mtr_read_ulint(inode + FSEG_NOT_FULL_N_USED,
				MLOG_4BYTES, mtr2);
	flst_validate(inode + FSEG_FREE, mtr2);
	flst_validate(inode + FSEG_NOT_FULL, mtr2);
	flst_validate(inode + FSEG_FULL, mtr2);

	/* Validate FSEG_FREE list */
	node_addr = flst_get_first(inode + FSEG_FREE, mtr2);

	while (!fil_addr_is_null(node_addr)) {
		ulint	flags;
		ulint	zip_size;

		mtr_start(&mtr);
		mtr_x_lock(fil_space_get_latch(space, &flags), &mtr);
		zip_size = fsp_flags_get_zip_size(flags);

		descr = xdes_lst_get_descriptor(space, zip_size,
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
		ulint	flags;
		ulint	zip_size;

		mtr_start(&mtr);
		mtr_x_lock(fil_space_get_latch(space, &flags), &mtr);
		zip_size = fsp_flags_get_zip_size(flags);

		descr = xdes_lst_get_descriptor(space, zip_size,
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
		ulint	flags;
		ulint	zip_size;

		mtr_start(&mtr);
		mtr_x_lock(fil_space_get_latch(space, &flags), &mtr);
		zip_size = fsp_flags_get_zip_size(flags);

		descr = xdes_lst_get_descriptor(space, zip_size,
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

#ifdef UNIV_DEBUG
/*******************************************************************//**
Validates a segment.
@return	TRUE if ok */
UNIV_INTERN
ibool
fseg_validate(
/*==========*/
	fseg_header_t*	header, /*!< in: segment header */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	fseg_inode_t*	inode;
	ibool		ret;
	ulint		space;
	ulint		flags;
	ulint		zip_size;

	space = page_get_space_id(page_align(header));

	mtr_x_lock(fil_space_get_latch(space, &flags), mtr);
	zip_size = fsp_flags_get_zip_size(flags);

	inode = fseg_inode_get(header, space, zip_size, mtr);

	ret = fseg_validate_low(inode, mtr);

	return(ret);
}
#endif /* UNIV_DEBUG */

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

	ut_ad(mtr_memo_contains_page(mtr, inode, MTR_MEMO_PAGE_X_FIX));
	space = page_get_space_id(page_align(inode));
	page_no = page_get_page_no(page_align(inode));

	reserved = fseg_n_reserved_pages_low(inode, &used, mtr);

	seg_id = mach_read_from_8(inode + FSEG_ID);

	n_used = mtr_read_ulint(inode + FSEG_NOT_FULL_N_USED,
				MLOG_4BYTES, mtr);
	n_frag = fseg_get_n_frag_pages(inode, mtr);
	n_free = flst_get_len(inode + FSEG_FREE, mtr);
	n_not_full = flst_get_len(inode + FSEG_NOT_FULL, mtr);
	n_full = flst_get_len(inode + FSEG_FULL, mtr);

	fprintf(stderr,
		"SEGMENT id %llu space %lu; page %lu;"
		" res %lu used %lu; full ext %lu\n"
		"fragm pages %lu; free extents %lu;"
		" not full extents %lu: pages %lu\n",
		(ullint) seg_id,
		(ulong) space, (ulong) page_no,
		(ulong) reserved, (ulong) used, (ulong) n_full,
		(ulong) n_frag, (ulong) n_free, (ulong) n_not_full,
		(ulong) n_used);
	ut_ad(mach_read_from_4(inode + FSEG_MAGIC_N) == FSEG_MAGIC_N_VALUE);
}

#ifdef UNIV_BTR_PRINT
/*******************************************************************//**
Writes info of a segment. */
UNIV_INTERN
void
fseg_print(
/*=======*/
	fseg_header_t*	header, /*!< in: segment header */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	fseg_inode_t*	inode;
	ulint		space;
	ulint		flags;
	ulint		zip_size;

	space = page_get_space_id(page_align(header));

	mtr_x_lock(fil_space_get_latch(space, &flags), mtr);
	zip_size = fsp_flags_get_zip_size(flags);

	inode = fseg_inode_get(header, space, zip_size, mtr);

	fseg_print_low(inode, mtr);
}
#endif /* UNIV_BTR_PRINT */

/*******************************************************************//**
Validates the file space system and its segments.
@return	TRUE if ok */
UNIV_INTERN
ibool
fsp_validate(
/*=========*/
	ulint	space)	/*!< in: space id */
{
	fsp_header_t*	header;
	fseg_inode_t*	seg_inode;
	page_t*		seg_inode_page;
	rw_lock_t*	latch;
	ulint		size;
	ulint		flags;
	ulint		zip_size;
	ulint		free_limit;
	ulint		frag_n_used;
	mtr_t		mtr;
	mtr_t		mtr2;
	xdes_t*		descr;
	fil_addr_t	node_addr;
	fil_addr_t	next_node_addr;
	ulint		descr_count	= 0;
	ulint		n_used		= 0;
	ulint		n_used2		= 0;
	ulint		n_full_frag_pages;
	ulint		n;
	ulint		seg_inode_len_free;
	ulint		seg_inode_len_full;

	latch = fil_space_get_latch(space, &flags);
	zip_size = fsp_flags_get_zip_size(flags);
	ut_a(ut_is_2pow(zip_size));
	ut_a(zip_size <= UNIV_ZIP_SIZE_MAX);
	ut_a(!zip_size || zip_size >= UNIV_ZIP_SIZE_MIN);

	/* Start first a mini-transaction mtr2 to lock out all other threads
	from the fsp system */
	mtr_start(&mtr2);
	mtr_x_lock(latch, &mtr2);

	mtr_start(&mtr);
	mtr_x_lock(latch, &mtr);

	header = fsp_get_space_header(space, zip_size, &mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, &mtr);
	free_limit = mtr_read_ulint(header + FSP_FREE_LIMIT,
				    MLOG_4BYTES, &mtr);
	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED,
				     MLOG_4BYTES, &mtr);

	n_full_frag_pages = FSP_EXTENT_SIZE
		* flst_get_len(header + FSP_FULL_FRAG, &mtr);

	if (UNIV_UNLIKELY(free_limit > size)) {

		ut_a(space != 0);
		ut_a(size < FSP_EXTENT_SIZE);
	}

	flst_validate(header + FSP_FREE, &mtr);
	flst_validate(header + FSP_FREE_FRAG, &mtr);
	flst_validate(header + FSP_FULL_FRAG, &mtr);

	mtr_commit(&mtr);

	/* Validate FSP_FREE list */
	mtr_start(&mtr);
	mtr_x_lock(latch, &mtr);

	header = fsp_get_space_header(space, zip_size, &mtr);
	node_addr = flst_get_first(header + FSP_FREE, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(latch, &mtr);

		descr_count++;
		descr = xdes_lst_get_descriptor(space, zip_size,
						node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == 0);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FREE);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate FSP_FREE_FRAG list */
	mtr_start(&mtr);
	mtr_x_lock(latch, &mtr);

	header = fsp_get_space_header(space, zip_size, &mtr);
	node_addr = flst_get_first(header + FSP_FREE_FRAG, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(latch, &mtr);

		descr_count++;
		descr = xdes_lst_get_descriptor(space, zip_size,
						node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) > 0);
		ut_a(xdes_get_n_used(descr, &mtr) < FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FREE_FRAG);

		n_used += xdes_get_n_used(descr, &mtr);
		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);

		mtr_commit(&mtr);
	}

	/* Validate FSP_FULL_FRAG list */
	mtr_start(&mtr);
	mtr_x_lock(latch, &mtr);

	header = fsp_get_space_header(space, zip_size, &mtr);
	node_addr = flst_get_first(header + FSP_FULL_FRAG, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {
		mtr_start(&mtr);
		mtr_x_lock(latch, &mtr);

		descr_count++;
		descr = xdes_lst_get_descriptor(space, zip_size,
						node_addr, &mtr);

		ut_a(xdes_get_n_used(descr, &mtr) == FSP_EXTENT_SIZE);
		ut_a(xdes_get_state(descr, &mtr) == XDES_FULL_FRAG);

		node_addr = flst_get_next_addr(descr + XDES_FLST_NODE, &mtr);
		mtr_commit(&mtr);
	}

	/* Validate segments */
	mtr_start(&mtr);
	mtr_x_lock(latch, &mtr);

	header = fsp_get_space_header(space, zip_size, &mtr);

	node_addr = flst_get_first(header + FSP_SEG_INODES_FULL, &mtr);

	seg_inode_len_full = flst_get_len(header + FSP_SEG_INODES_FULL, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {

		n = 0;
		do {
			mtr_start(&mtr);
			mtr_x_lock(latch, &mtr);

			seg_inode_page = fut_get_ptr(
				space, zip_size, node_addr, RW_X_LATCH, &mtr)
				- FSEG_INODE_PAGE_NODE;

			seg_inode = fsp_seg_inode_page_get_nth_inode(
				seg_inode_page, n, zip_size, &mtr);
			ut_a(mach_read_from_8(seg_inode + FSEG_ID) != 0);
			fseg_validate_low(seg_inode, &mtr);

			descr_count += flst_get_len(seg_inode + FSEG_FREE,
						    &mtr);
			descr_count += flst_get_len(seg_inode + FSEG_FULL,
						    &mtr);
			descr_count += flst_get_len(seg_inode + FSEG_NOT_FULL,
						    &mtr);

			n_used2 += fseg_get_n_frag_pages(seg_inode, &mtr);

			next_node_addr = flst_get_next_addr(
				seg_inode_page + FSEG_INODE_PAGE_NODE, &mtr);
			mtr_commit(&mtr);
		} while (++n < FSP_SEG_INODES_PER_PAGE(zip_size));

		node_addr = next_node_addr;
	}

	mtr_start(&mtr);
	mtr_x_lock(latch, &mtr);

	header = fsp_get_space_header(space, zip_size, &mtr);

	node_addr = flst_get_first(header + FSP_SEG_INODES_FREE, &mtr);

	seg_inode_len_free = flst_get_len(header + FSP_SEG_INODES_FREE, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {

		n = 0;

		do {
			mtr_start(&mtr);
			mtr_x_lock(latch, &mtr);

			seg_inode_page = fut_get_ptr(
				space, zip_size, node_addr, RW_X_LATCH, &mtr)
				- FSEG_INODE_PAGE_NODE;

			seg_inode = fsp_seg_inode_page_get_nth_inode(
				seg_inode_page, n, zip_size, &mtr);
			if (mach_read_from_8(seg_inode + FSEG_ID)) {
				fseg_validate_low(seg_inode, &mtr);

				descr_count += flst_get_len(
					seg_inode + FSEG_FREE, &mtr);
				descr_count += flst_get_len(
					seg_inode + FSEG_FULL, &mtr);
				descr_count += flst_get_len(
					seg_inode + FSEG_NOT_FULL, &mtr);
				n_used2 += fseg_get_n_frag_pages(
					seg_inode, &mtr);
			}

			next_node_addr = flst_get_next_addr(
				seg_inode_page + FSEG_INODE_PAGE_NODE, &mtr);
			mtr_commit(&mtr);
		} while (++n < FSP_SEG_INODES_PER_PAGE(zip_size));

		node_addr = next_node_addr;
	}

	ut_a(descr_count * FSP_EXTENT_SIZE == free_limit);
	if (!zip_size) {
		ut_a(n_used + n_full_frag_pages
		     == n_used2 + 2 * ((free_limit + (UNIV_PAGE_SIZE - 1))
				       / UNIV_PAGE_SIZE)
		     + seg_inode_len_full + seg_inode_len_free);
	} else {
		ut_a(n_used + n_full_frag_pages
		     == n_used2 + 2 * ((free_limit + (zip_size - 1))
				       / zip_size)
		     + seg_inode_len_full + seg_inode_len_free);
	}
	ut_a(frag_n_used == n_used);

	mtr_commit(&mtr2);

	return(TRUE);
}

/*******************************************************************//**
Prints info of a file space. */
UNIV_INTERN
void
fsp_print(
/*======*/
	ulint	space)	/*!< in: space id */
{
	fsp_header_t*	header;
	fseg_inode_t*	seg_inode;
	page_t*		seg_inode_page;
	rw_lock_t*	latch;
	ulint		flags;
	ulint		zip_size;
	ulint		size;
	ulint		free_limit;
	ulint		frag_n_used;
	fil_addr_t	node_addr;
	fil_addr_t	next_node_addr;
	ulint		n_free;
	ulint		n_free_frag;
	ulint		n_full_frag;
	ib_id_t		seg_id;
	ulint		n;
	ulint		n_segs		= 0;
	mtr_t		mtr;
	mtr_t		mtr2;

	latch = fil_space_get_latch(space, &flags);
	zip_size = fsp_flags_get_zip_size(flags);

	/* Start first a mini-transaction mtr2 to lock out all other threads
	from the fsp system */

	mtr_start(&mtr2);

	mtr_x_lock(latch, &mtr2);

	mtr_start(&mtr);

	mtr_x_lock(latch, &mtr);

	header = fsp_get_space_header(space, zip_size, &mtr);

	size = mtr_read_ulint(header + FSP_SIZE, MLOG_4BYTES, &mtr);

	free_limit = mtr_read_ulint(header + FSP_FREE_LIMIT, MLOG_4BYTES,
				    &mtr);
	frag_n_used = mtr_read_ulint(header + FSP_FRAG_N_USED, MLOG_4BYTES,
				     &mtr);
	n_free = flst_get_len(header + FSP_FREE, &mtr);
	n_free_frag = flst_get_len(header + FSP_FREE_FRAG, &mtr);
	n_full_frag = flst_get_len(header + FSP_FULL_FRAG, &mtr);

	seg_id = mach_read_from_8(header + FSP_SEG_ID);

	fprintf(stderr,
		"FILE SPACE INFO: id %lu\n"
		"size %lu, free limit %lu, free extents %lu\n"
		"not full frag extents %lu: used pages %lu,"
		" full frag extents %lu\n"
		"first seg id not used %llu\n",
		(ulong) space,
		(ulong) size, (ulong) free_limit, (ulong) n_free,
		(ulong) n_free_frag, (ulong) frag_n_used, (ulong) n_full_frag,
		(ullint) seg_id);

	mtr_commit(&mtr);

	/* Print segments */

	mtr_start(&mtr);
	mtr_x_lock(latch, &mtr);

	header = fsp_get_space_header(space, zip_size, &mtr);

	node_addr = flst_get_first(header + FSP_SEG_INODES_FULL, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {

		n = 0;

		do {

			mtr_start(&mtr);
			mtr_x_lock(latch, &mtr);

			seg_inode_page = fut_get_ptr(
				space, zip_size, node_addr, RW_X_LATCH, &mtr)
				- FSEG_INODE_PAGE_NODE;

			seg_inode = fsp_seg_inode_page_get_nth_inode(
				seg_inode_page, n, zip_size, &mtr);
			ut_a(mach_read_from_8(seg_inode + FSEG_ID) != 0);
			fseg_print_low(seg_inode, &mtr);

			n_segs++;

			next_node_addr = flst_get_next_addr(
				seg_inode_page + FSEG_INODE_PAGE_NODE, &mtr);
			mtr_commit(&mtr);
		} while (++n < FSP_SEG_INODES_PER_PAGE(zip_size));

		node_addr = next_node_addr;
	}

	mtr_start(&mtr);
	mtr_x_lock(latch, &mtr);

	header = fsp_get_space_header(space, zip_size, &mtr);

	node_addr = flst_get_first(header + FSP_SEG_INODES_FREE, &mtr);

	mtr_commit(&mtr);

	while (!fil_addr_is_null(node_addr)) {

		n = 0;

		do {

			mtr_start(&mtr);
			mtr_x_lock(latch, &mtr);

			seg_inode_page = fut_get_ptr(
				space, zip_size, node_addr, RW_X_LATCH, &mtr)
				- FSEG_INODE_PAGE_NODE;

			seg_inode = fsp_seg_inode_page_get_nth_inode(
				seg_inode_page, n, zip_size, &mtr);
			if (mach_read_from_8(seg_inode + FSEG_ID)) {

				fseg_print_low(seg_inode, &mtr);
				n_segs++;
			}

			next_node_addr = flst_get_next_addr(
				seg_inode_page + FSEG_INODE_PAGE_NODE, &mtr);
			mtr_commit(&mtr);
		} while (++n < FSP_SEG_INODES_PER_PAGE(zip_size));

		node_addr = next_node_addr;
	}

	mtr_commit(&mtr2);

	fprintf(stderr, "NUMBER of file segments: %lu\n", (ulong) n_segs);
}
#endif /* !UNIV_HOTBACKUP */
