/******************************************************
File space management

(c) 1995 Innobase Oy

Created 12/18/1995 Heikki Tuuri
*******************************************************/

#ifndef fsp0fsp_h
#define fsp0fsp_h

#include "univ.i"

#include "mtr0mtr.h"
#include "fut0lst.h"
#include "ut0byte.h"
#include "page0types.h"

/* If records are inserted in order, there are the following
flags to tell this (their type is made byte for the compiler
to warn if direction and hint parameters are switched in
fseg_alloc_free_page): */
#define	FSP_UP		((byte)111)	/* alphabetically upwards */
#define	FSP_DOWN	((byte)112)	/* alphabetically downwards */
#define	FSP_NO_DIR	((byte)113)	/* no order */

/* File space extent size in pages */
#define	FSP_EXTENT_SIZE		64

/* On a page of any file segment, data may be put starting from this offset: */
#define FSEG_PAGE_DATA		FIL_PAGE_DATA

/* File segment header which points to the inode describing the file segment */
typedef	byte	fseg_header_t;

#define FSEG_HDR_SPACE		0	/* space id of the inode */
#define FSEG_HDR_PAGE_NO	4	/* page number of the inode */
#define FSEG_HDR_OFFSET		8	/* byte offset of the inode */

#define FSEG_HEADER_SIZE	10

/**************************************************************************
Initializes the file space system. */

void
fsp_init(void);
/*==========*/
/**************************************************************************
Initializes the space header of a new created space. */

void
fsp_header_init(
/*============*/
	ulint	space,	/* in: space id */
	ulint	size,	/* in: current size in blocks */
	mtr_t*	mtr);	/* in: mini-transaction handle */	
/**************************************************************************
Increases the space size field of a space. */

void
fsp_header_inc_size(
/*================*/
	ulint	space,	/* in: space id */
	ulint	size_inc,/* in: size increment in pages */
	mtr_t*	mtr);	/* in: mini-transaction handle */	
/**************************************************************************
Creates a new segment. */

page_t*
fseg_create(
/*========*/
			/* out: the page where the segment header is placed,
			x-latched, NULL if could not create segment
			because of lack of space */
	ulint	space,	/* in: space id */
	ulint	page,	/* in: page where the segment header is placed: if
			this is != 0, the page must belong to another segment,
			if this is 0, a new page will be allocated and it
			will belong to the created segment */
	ulint	byte_offset, /* in: byte offset of the created segment header
			on the page */
	mtr_t*	mtr);	/* in: mtr */
/**************************************************************************
Creates a new segment. */

page_t*
fseg_create_general(
/*================*/
			/* out: the page where the segment header is placed,
			x-latched, NULL if could not create segment
			because of lack of space */
	ulint	space,	/* in: space id */
	ulint	page,	/* in: page where the segment header is placed: if
			this is != 0, the page must belong to another segment,
			if this is 0, a new page will be allocated and it
			will belong to the created segment */
	ulint	byte_offset, /* in: byte offset of the created segment header
			on the page */
	ibool	has_done_reservation, /* in: TRUE if the caller has
			already done the reservation for the pages
			with fsp_reserve_free_extents (at least 2 extents:
			one for the inode and, then there other for the
			segment) is no need to do the check for this
			individual operation */
	mtr_t*	mtr);	/* in: mtr */
/**************************************************************************
Calculates the number of pages reserved by a segment, and how many pages are
currently used. */

ulint
fseg_n_reserved_pages(
/*==================*/
				/* out: number of reserved pages */
	fseg_header_t* 	header,	/* in: segment header */
	ulint*		used,	/* out: number of pages used (<= reserved) */
	mtr_t*		mtr);	/* in: mtr handle */
/**************************************************************************
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize
file space fragmentation. */

ulint
fseg_alloc_free_page(
/*=================*/
				/* out: the allocated page offset
				FIL_NULL if no page could be allocated */
	fseg_header_t*	seg_header, /* in: segment header */
	ulint		hint,	/* in: hint of which page would be desirable */
	byte		direction, /* in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	mtr_t*		mtr);	/* in: mtr handle */
/**************************************************************************
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation. */

ulint
fseg_alloc_free_page_general(
/*=========================*/
				/* out: allocated page offset, FIL_NULL if no
				page could be allocated */
	fseg_header_t*	seg_header,/* in: segment header */
	ulint		hint,	/* in: hint of which page would be desirable */
	byte		direction,/* in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	ibool		has_done_reservation, /* in: TRUE if the caller has
				already done the reservation for the page
				with fsp_reserve_free_extents, then there
				is no need to do the check for this individual
				page */
	mtr_t*		mtr);	/* in: mtr handle */
/**************************************************************************
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
reserve some space. */ 

ibool
fsp_reserve_free_extents(
/*=====================*/
			/* out: TRUE if we were able to make the reservation */
	ulint	space,	/* in: space id */
	ulint	n_ext,	/* in: number of extents to reserve */
	ulint	alloc_type,/* in: FSP_NORMAL, FSP_UNDO, or FSP_CLEANING */
	mtr_t*	mtr);	/* in: mtr */
/**************************************************************************
This function should be used to get information on how much we still
will be able to insert new data to the database without running out the
tablespace. Only free extents are taken into account and we also subtract
the safety margin required by the above function fsp_reserve_free_extents. */

ulint
fsp_get_available_space_in_free_extents(
/*====================================*/
			/* out: available space in kB */
	ulint	space);	/* in: space id */
/**************************************************************************
Frees a single page of a segment. */

void
fseg_free_page(
/*===========*/
	fseg_header_t*	seg_header, /* in: segment header */
	ulint		space,	/* in: space id */
	ulint		page,	/* in: page offset */
	mtr_t*		mtr);	/* in: mtr handle */
/***********************************************************************
Frees a segment. The freeing is performed in several mini-transactions,
so that there is no danger of bufferfixing too many buffer pages. */

void
fseg_free(
/*======*/
	ulint	space,	/* in: space id */
	ulint	page_no,/* in: page number where the segment header is
			placed */
	ulint	offset);/* in: byte offset of the segment header on that
			page */
/**************************************************************************
Frees part of a segment. This function can be used to free a segment
by repeatedly calling this function in different mini-transactions.
Doing the freeing in a single mini-transaction might result in
too big a mini-transaction. */

ibool
fseg_free_step(
/*===========*/
				/* out: TRUE if freeing completed */
	fseg_header_t*	header,	/* in, own: segment header; NOTE: if the header
				resides on the first page of the frag list
				of the segment, this pointer becomes obsolete
				after the last freeing step */
	mtr_t*		mtr);	/* in: mtr */
/**************************************************************************
Frees part of a segment. Differs from fseg_free_step because this function
leaves the header page unfreed. */

ibool
fseg_free_step_not_header(
/*======================*/
				/* out: TRUE if freeing completed, except the
				header page */
	fseg_header_t*	header,	/* in: segment header which must reside on
				the first fragment page of the segment */
	mtr_t*		mtr);	/* in: mtr */
/***************************************************************************
Checks if a page address is an extent descriptor page address. */
UNIV_INLINE
ibool
fsp_descr_page(
/*===========*/
			/* out: TRUE if a descriptor page */
	ulint	page_no);/* in: page number */
/***************************************************************
Parses a redo log record of a file page init. */

byte*
fsp_parse_init_file_page(
/*=====================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page);	/* in: page or NULL */
/***********************************************************************
Validates the file space system and its segments. */

ibool
fsp_validate(
/*=========*/
			/* out: TRUE if ok */
	ulint	space);	/* in: space id */
/***********************************************************************
Prints info of a file space. */

void
fsp_print(
/*======*/
	ulint	space);	/* in: space id */
/***********************************************************************
Validates a segment. */

ibool
fseg_validate(
/*==========*/
				/* out: TRUE if ok */
	fseg_header_t*	header, /* in: segment header */
	mtr_t*		mtr2);	/* in: mtr */
/***********************************************************************
Writes info of a segment. */

void
fseg_print(
/*=======*/
	fseg_header_t*	header, /* in: segment header */
	mtr_t*		mtr);	/* in: mtr */

/* Flags for fsp_reserve_free_extents */
#define FSP_NORMAL	1000000
#define	FSP_UNDO	2000000
#define FSP_CLEANING	3000000

/* Number of pages described in a single descriptor page: currently each page
description takes less than 1 byte; a descriptor page is repeated every
this many file pages */
#define XDES_DESCRIBED_PER_PAGE		UNIV_PAGE_SIZE

/* The space low address page map, and also offsets for extent descriptor and
bitmap pages which are repeated always after XDES_DESCRIBED_PER_PAGE more
pages: */
/*--------------------------------------*/
#define FSP_XDES_OFFSET			0
#define FSP_IBUF_BITMAP_OFFSET		1
				/* The ibuf bitmap pages are the ones whose
				page number is the number above plus a
				multiple of XDES_DESCRIBED_PER_PAGE */
#define FSP_FIRST_INODE_PAGE_NO		2
#define FSP_IBUF_HEADER_PAGE_NO		3
#define FSP_IBUF_TREE_ROOT_PAGE_NO	4
				/* The ibuf tree root page number in each
				tablespace; its fseg inode is on the page
				number FSP_FIRST_INODE_PAGE_NO */
#define FSP_TRX_SYS_PAGE_NO		5
#define	FSP_FIRST_RSEG_PAGE_NO		6
#define FSP_DICT_HDR_PAGE_NO		7
/*--------------------------------------*/

#ifndef UNIV_NONINL
#include "fsp0fsp.ic"
#endif

#endif
