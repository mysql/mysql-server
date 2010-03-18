/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

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

/**********************************************************************//**
Initializes the file space system. */
UNIV_INTERN
void
fsp_init(void);
/*==========*/
/**********************************************************************//**
Gets the current free limit of the system tablespace.  The free limit
means the place of the first page which has never been put to the
free list for allocation.  The space above that address is initialized
to zero.  Sets also the global variable log_fsp_current_free_limit.
@return	free limit in megabytes */
UNIV_INTERN
ulint
fsp_header_get_free_limit(void);
/*===========================*/
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
This function is used past the buffer pool when we in fil0fil.c create
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
@return	the allocated page offset FIL_NULL if no page could be allocated */
UNIV_INTERN
ulint
fseg_alloc_free_page(
/*=================*/
	fseg_header_t*	seg_header, /*!< in: segment header */
	ulint		hint,	/*!< in: hint of which page would be desirable */
	byte		direction, /*!< in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	mtr_t*		mtr);	/*!< in: mtr handle */
/**********************************************************************//**
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation.
@return	allocated page offset, FIL_NULL if no page could be allocated */
UNIV_INTERN
ulint
fseg_alloc_free_page_general(
/*=========================*/
	fseg_header_t*	seg_header,/*!< in: segment header */
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
	mtr_t*		mtr);	/*!< in: mtr handle */
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

#ifndef UNIV_NONINL
#include "fsp0fsp.ic"
#endif

#endif
