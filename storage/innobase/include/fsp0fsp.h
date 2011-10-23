/*****************************************************************************

Copyright (c) 1995, 2011, Oracle and/or its affiliates. All Rights Reserved.

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

/**************************************************//**
@file include/fsp0fsp.h
File space management

Created 12/18/1995 Heikki Tuuri
*******************************************************/

#ifndef fsp0fsp_h
#define fsp0fsp_h

#include "univ.i"

#include "mtr0mtr.h"
#include "fut0lst.h"
#include "ut0byte.h"
#include "page0types.h"
#include "fsp0types.h"

/* @defgroup fsp_flags InnoDB Tablespace Flag Constants @{ */

/** Tablespace flags layout (FSP_SPACE_FLAGS)
All flags are zero for row formats REDUNDANT and COMPACT.  The use of the
low order bit is different from the low order bit in dict_table_t::flags
and SYS_TABLES.TYPE.  When FSP_SPACE_FLAGS != 0, it means that the file
format is post-Antelope and the flags field is being used. */
#define FSP_FLAGS_ANTELOPE		0

/** Width of the POST_ANTELOPE flag */
#define FSP_FLAGS_WIDTH_POST_ANTELOPE	1
/** Number of flag bits used to indicate the tablespace zip page size */
#define FSP_FLAGS_WIDTH_ZIP_SSIZE	4
/** Width of the ATOMIC_BLOBS flag.  The ability to break up a long
column into an in-record prefix and an externally stored part is available
to the two Barracuda row formats COMPRESSED and DYNAMIC. */
#define FSP_FLAGS_WIDTH_ATOMIC_BLOBS	1
/** Width of all the currently known tablespace flags */
#define FSP_FLAGS_BITS		(FSP_FLAGS_WIDTH_POST_ANTELOPE	\
				+ FSP_FLAGS_WIDTH_ZIP_SSIZE	\
				+ FSP_FLAGS_WIDTH_ATOMIC_BLOBS)

/** A mask of all the known/used bits in tablespace flags */
#define FSP_FLAGS_MASK		(~(~0 << FSP_FLAGS_BITS))

/** Zero relative shift position of the POST_ANTELOPE field */
#define FSP_FLAGS_POS_POST_ANTELOPE	0
/** Zero relative shift position of the ZIP_SSIZE field */
#define FSP_FLAGS_POS_ZIP_SSIZE		(FSP_FLAGS_POS_POST_ANTELOPE	\
					+ FSP_FLAGS_WIDTH_POST_ANTELOPE)
/** Zero relative shift position of the ATOMIC_BLOBS field */
#define FSP_FLAGS_POS_ATOMIC_BLOBS	(FSP_FLAGS_POS_ZIP_SSIZE	\
					+ FSP_FLAGS_WIDTH_ZIP_SSIZE)
/** Zero relative shift position of the start of the UNUSED bits */
#define FSP_FLAGS_POS_UNUSED		(FSP_FLAGS_POS_ATOMIC_BLOBS	\
					+ FSP_FLAGS_WIDTH_ATOMIC_BLOBS)

/** Bit mask of the POST_ANTELOPE field */
#define FSP_FLAGS_MASK_POST_ANTELOPE				\
		((~(~0 << FSP_FLAGS_WIDTH_POST_ANTELOPE))	\
		<< FSP_FLAGS_POS_POST_ANTELOPE)
/** Bit mask of the ZIP_SSIZE field */
#define FSP_FLAGS_MASK_ZIP_SSIZE				\
		((~(~0 << FSP_FLAGS_WIDTH_ZIP_SSIZE))		\
		<< FSP_FLAGS_POS_ZIP_SSIZE)
/** Bit mask of the ATOMIC_BLOBS field */
#define FSP_FLAGS_MASK_ATOMIC_BLOBS				\
		((~(~0 << FSP_FLAGS_WIDTH_ATOMIC_BLOBS))	\
		<< FSP_FLAGS_POS_ATOMIC_BLOBS)

/** Return the value of the POST_ANTELOPE field */
#define FSP_FLAGS_GET_POST_ANTELOPE(flags)			\
		((flags & FSP_FLAGS_MASK_POST_ANTELOPE)		\
		>> FSP_FLAGS_POS_POST_ANTELOPE)
/** Return the value of the ZIP_SSIZE field */
#define FSP_FLAGS_GET_ZIP_SSIZE(flags)				\
		((flags & FSP_FLAGS_MASK_ZIP_SSIZE)		\
		>> FSP_FLAGS_POS_ZIP_SSIZE)
/** Return the value of the ATOMIC_BLOBS field */
#define FSP_FLAGS_HAS_ATOMIC_BLOBS(flags)			\
		((flags & FSP_FLAGS_MASK_ATOMIC_BLOBS)		\
		>> FSP_FLAGS_POS_ATOMIC_BLOBS)
/** Return the contents of the UNUSED bits */
#define FSP_FLAGS_GET_UNUSED(flags)				\
		(flags >> FSP_FLAGS_POS_UNUSED)
/* @} */

/**********************************************************************//**
Initializes the file space system. */
UNIV_INTERN
void
fsp_init(void);
/*==========*/
/**********************************************************************//**
Gets the size of the system tablespace from the tablespace header.  If
we do not have an auto-extending data file, this should be equal to
the size of the data files.  If there is an auto-extending data file,
this can be smaller.
@return	size in pages */
UNIV_INTERN
ulint
fsp_header_get_tablespace_size(void);
/*================================*/
/**********************************************************************//**
Reads the file space size stored in the header page.
@return	tablespace size stored in the space header */
UNIV_INTERN
ulint
fsp_get_size_low(
/*=============*/
	page_t*	page);	/*!< in: header page (page 0 in the tablespace) */
/**********************************************************************//**
Reads the space id from the first page of a tablespace.
@return	space id, ULINT UNDEFINED if error */
UNIV_INTERN
ulint
fsp_header_get_space_id(
/*====================*/
	const page_t*	page);	/*!< in: first page of a tablespace */
/**********************************************************************//**
Reads the space flags from the first page of a tablespace.
@return	flags */
UNIV_INTERN
ulint
fsp_header_get_flags(
/*=================*/
	const page_t*	page);	/*!< in: first page of a tablespace */
/**********************************************************************//**
Reads the compressed page size from the first page of a tablespace.
@return	compressed page size in bytes, or 0 if uncompressed */
UNIV_INTERN
ulint
fsp_header_get_zip_size(
/*====================*/
	const page_t*	page);	/*!< in: first page of a tablespace */
/**********************************************************************//**
Writes the space id and compressed page size to a tablespace header.
This function is used past the buffer pool when we in fil0fil.cc create
a new single-table tablespace. */
UNIV_INTERN
void
fsp_header_init_fields(
/*===================*/
	page_t*	page,		/*!< in/out: first page in the space */
	ulint	space_id,	/*!< in: space id */
	ulint	flags);		/*!< in: tablespace flags (FSP_SPACE_FLAGS):
				0, or table->flags if newer than COMPACT */
/**********************************************************************//**
Initializes the space header of a new created space and creates also the
insert buffer tree root if space == 0. */
UNIV_INTERN
void
fsp_header_init(
/*============*/
	ulint	space,		/*!< in: space id */
	ulint	size,		/*!< in: current size in blocks */
	mtr_t*	mtr);		/*!< in: mini-transaction handle */
/**********************************************************************//**
Increases the space size field of a space. */
UNIV_INTERN
void
fsp_header_inc_size(
/*================*/
	ulint	space,	/*!< in: space id */
	ulint	size_inc,/*!< in: size increment in pages */
	mtr_t*	mtr);	/*!< in: mini-transaction handle */
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
	mtr_t*	mtr);	/*!< in: mtr */
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
	mtr_t*	mtr);	/*!< in: mtr */
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
	mtr_t*		mtr);	/*!< in: mtr handle */
/**********************************************************************//**
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize
file space fragmentation.
@param[in/out] seg_header	segment header
@param[in] hint			hint of which page would be desirable
@param[in] direction		if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR
@param[in/out] mtr		mini-transaction
@return	the allocated page offset FIL_NULL if no page could be allocated */
#define fseg_alloc_free_page(seg_header, hint, direction, mtr)		\
	fseg_alloc_free_page_general(seg_header, hint, direction,	\
				     FALSE, mtr, mtr)
/**********************************************************************//**
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation.
@return	allocated page offset, FIL_NULL if no page could be allocated */
UNIV_INTERN
ulint
fseg_alloc_free_page_general(
/*=========================*/
	fseg_header_t*	seg_header,/*!< in/out: segment header */
	ulint		hint,	/*!< in: hint of which page would be desirable */
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
				in which the page should be initialized,
				or NULL if this is a "fake allocation" of
				a page that was previously freed in mtr */
	__attribute__((warn_unused_result, nonnull(1,5)));
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
	mtr_t*	mtr);	/*!< in: mtr */
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
	ulint	space);	/*!< in: space id */
/**********************************************************************//**
Frees a single page of a segment. */
UNIV_INTERN
void
fseg_free_page(
/*===========*/
	fseg_header_t*	seg_header, /*!< in: segment header */
	ulint		space,	/*!< in: space id */
	ulint		page,	/*!< in: page offset */
	mtr_t*		mtr);	/*!< in: mtr handle */
/**********************************************************************//**
Frees part of a segment. This function can be used to free a segment
by repeatedly calling this function in different mini-transactions.
Doing the freeing in a single mini-transaction might result in
too big a mini-transaction.
@return	TRUE if freeing completed */
UNIV_INTERN
ibool
fseg_free_step(
/*===========*/
	fseg_header_t*	header,	/*!< in, own: segment header; NOTE: if the header
				resides on the first page of the frag list
				of the segment, this pointer becomes obsolete
				after the last freeing step */
	mtr_t*		mtr);	/*!< in: mtr */
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
	mtr_t*		mtr);	/*!< in: mtr */
/***********************************************************************//**
Checks if a page address is an extent descriptor page address.
@return	TRUE if a descriptor page */
UNIV_INLINE
ibool
fsp_descr_page(
/*===========*/
	ulint	zip_size,/*!< in: compressed page size in bytes;
			0 for uncompressed pages */
	ulint	page_no);/*!< in: page number */
/***********************************************************//**
Parses a redo log record of a file page init.
@return	end of log record or NULL */
UNIV_INTERN
byte*
fsp_parse_init_file_page(
/*=====================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr, /*!< in: buffer end */
	buf_block_t*	block);	/*!< in: block or NULL */
/*******************************************************************//**
Validates the file space system and its segments.
@return	TRUE if ok */
UNIV_INTERN
ibool
fsp_validate(
/*=========*/
	ulint	space);	/*!< in: space id */
/*******************************************************************//**
Prints info of a file space. */
UNIV_INTERN
void
fsp_print(
/*======*/
	ulint	space);	/*!< in: space id */
#ifdef UNIV_DEBUG
/*******************************************************************//**
Validates a segment.
@return	TRUE if ok */
UNIV_INTERN
ibool
fseg_validate(
/*==========*/
	fseg_header_t*	header, /*!< in: segment header */
	mtr_t*		mtr);	/*!< in: mtr */
#endif /* UNIV_DEBUG */
#ifdef UNIV_BTR_PRINT
/*******************************************************************//**
Writes info of a segment. */
UNIV_INTERN
void
fseg_print(
/*=======*/
	fseg_header_t*	header, /*!< in: segment header */
	mtr_t*		mtr);	/*!< in: mtr */
#endif /* UNIV_BTR_PRINT */

/********************************************************************//**
Validate and return the tablespace flags, which are stored in the
tablespace header at offset FSP_SPACE_FLAGS.  They should be 0 for
ROW_FORMAT=COMPACT and ROW_FORMAT=REDUNDANT. The newer row formats,
COMPRESSED and DYNAMIC, use a file format > Antelope so they should
have a file format number plus the DICT_TF_COMPACT bit set.
@return	ulint containing the validated tablespace flags. */
UNIV_INLINE
ulint
fsp_flags_validate(
/*===============*/
	ulint	flags);		/*!< in: tablespace flags */
/********************************************************************//**
Determine if the tablespace is compressed from dict_table_t::flags.
@return	TRUE if compressed, FALSE if not compressed */
UNIV_INLINE
ibool
fsp_flags_is_compressed(
/*====================*/
	ulint	flags);	/*!< in: tablespace flags */
/********************************************************************//**
Extract the zip size from tablespace flags.  A tablespace has only one
physical page size whether that page is compressed or not.
@return	compressed page size of the file-per-table tablespace in bytes,
or zero if the table is not compressed.  */
UNIV_INLINE
ulint
fsp_flags_get_zip_size(
/*====================*/
	ulint	flags);	/*!< in: tablespace flags */

#ifndef UNIV_NONINL
#include "fsp0fsp.ic"
#endif

#endif
