/*****************************************************************************

Copyright (c) 1994, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/page0page.h
Index page routines

Created 2/2/1994 Heikki Tuuri
*******************************************************/

#ifndef page0page_h
#define page0page_h

#include "univ.i"

#include "page0types.h"
#include "fil0fil.h"
#include "buf0buf.h"
#include "data0data.h"
#include "dict0dict.h"
#include "rem0rec.h"
#include "fsp0fsp.h"
#include "mtr0mtr.h"

#ifdef UNIV_MATERIALIZE
#undef UNIV_INLINE
#define UNIV_INLINE
#endif

/*			PAGE HEADER
			===========

Index page header starts at the first offset left free by the FIL-module */

typedef	byte		page_header_t;

#define	PAGE_HEADER	FSEG_PAGE_DATA	/* index page header starts at this
				offset */
/*-----------------------------*/
#define PAGE_N_DIR_SLOTS 0	/* number of slots in page directory */
#define	PAGE_HEAP_TOP	 2	/* pointer to record heap top */
#define	PAGE_N_HEAP	 4	/* number of records in the heap,
				bit 15=flag: new-style compact page format */
#define	PAGE_FREE	 6	/* pointer to start of page free record list */
#define	PAGE_GARBAGE	 8	/* number of bytes in deleted records */
#define	PAGE_LAST_INSERT 10	/* pointer to the last inserted record, or
				NULL if this info has been reset by a delete,
				for example */
#define	PAGE_DIRECTION	 12	/* last insert direction: PAGE_LEFT, ... */
#define	PAGE_N_DIRECTION 14	/* number of consecutive inserts to the same
				direction */
#define	PAGE_N_RECS	 16	/* number of user records on the page */
#define PAGE_MAX_TRX_ID	 18	/* highest id of a trx which may have modified
				a record on the page; trx_id_t; defined only
				in secondary indexes and in the insert buffer
				tree */
#define PAGE_HEADER_PRIV_END 26	/* end of private data structure of the page
				header which are set in a page create */
/*----*/
#define	PAGE_LEVEL	 26	/* level of the node in an index tree; the
				leaf level is the level 0.  This field should
				not be written to after page creation. */
#define	PAGE_INDEX_ID	 28	/* index id where the page belongs.
				This field should not be written to after
				page creation. */
#define PAGE_BTR_SEG_LEAF 36	/* file segment header for the leaf pages in
				a B-tree: defined only on the root page of a
				B-tree, but not in the root of an ibuf tree */
#define PAGE_BTR_IBUF_FREE_LIST	PAGE_BTR_SEG_LEAF
#define PAGE_BTR_IBUF_FREE_LIST_NODE PAGE_BTR_SEG_LEAF
				/* in the place of PAGE_BTR_SEG_LEAF and _TOP
				there is a free list base node if the page is
				the root page of an ibuf tree, and at the same
				place is the free list node if the page is in
				a free list */
#define PAGE_BTR_SEG_TOP (36 + FSEG_HEADER_SIZE)
				/* file segment header for the non-leaf pages
				in a B-tree: defined only on the root page of
				a B-tree, but not in the root of an ibuf
				tree */
/*----*/
#define PAGE_DATA	(PAGE_HEADER + 36 + 2 * FSEG_HEADER_SIZE)
				/* start of data on the page */

#define PAGE_OLD_INFIMUM	(PAGE_DATA + 1 + REC_N_OLD_EXTRA_BYTES)
				/* offset of the page infimum record on an
				old-style page */
#define PAGE_OLD_SUPREMUM	(PAGE_DATA + 2 + 2 * REC_N_OLD_EXTRA_BYTES + 8)
				/* offset of the page supremum record on an
				old-style page */
#define PAGE_OLD_SUPREMUM_END (PAGE_OLD_SUPREMUM + 9)
				/* offset of the page supremum record end on
				an old-style page */
#define PAGE_NEW_INFIMUM	(PAGE_DATA + REC_N_NEW_EXTRA_BYTES)
				/* offset of the page infimum record on a
				new-style compact page */
#define PAGE_NEW_SUPREMUM	(PAGE_DATA + 2 * REC_N_NEW_EXTRA_BYTES + 8)
				/* offset of the page supremum record on a
				new-style compact page */
#define PAGE_NEW_SUPREMUM_END (PAGE_NEW_SUPREMUM + 8)
				/* offset of the page supremum record end on
				a new-style compact page */
/*-----------------------------*/

/* Heap numbers */
#define PAGE_HEAP_NO_INFIMUM	0	/* page infimum */
#define PAGE_HEAP_NO_SUPREMUM	1	/* page supremum */
#define PAGE_HEAP_NO_USER_LOW	2	/* first user record in
					creation (insertion) order,
					not necessarily collation order;
					this record may have been deleted */

/* Directions of cursor movement */
#define	PAGE_LEFT		1
#define	PAGE_RIGHT		2
#define	PAGE_SAME_REC		3
#define	PAGE_SAME_PAGE		4
#define	PAGE_NO_DIRECTION	5

/*			PAGE DIRECTORY
			==============
*/

typedef	byte			page_dir_slot_t;
typedef page_dir_slot_t		page_dir_t;

/* Offset of the directory start down from the page end. We call the
slot with the highest file address directory start, as it points to
the first record in the list of records. */
#define	PAGE_DIR		FIL_PAGE_DATA_END

/* We define a slot in the page directory as two bytes */
#define	PAGE_DIR_SLOT_SIZE	2

/* The offset of the physically lower end of the directory, counted from
page end, when the page is empty */
#define PAGE_EMPTY_DIR_START	(PAGE_DIR + 2 * PAGE_DIR_SLOT_SIZE)

/* The maximum and minimum number of records owned by a directory slot. The
number may drop below the minimum in the first and the last slot in the
directory. */
#define PAGE_DIR_SLOT_MAX_N_OWNED	8
#define	PAGE_DIR_SLOT_MIN_N_OWNED	4

/************************************************************//**
Gets the start of a page.
@return	start of the page */
UNIV_INLINE
page_t*
page_align(
/*=======*/
	const void*	ptr)	/*!< in: pointer to page frame */
		__attribute__((const));
/************************************************************//**
Gets the offset within a page.
@return	offset from the start of the page */
UNIV_INLINE
ulint
page_offset(
/*========*/
	const void*	ptr)	/*!< in: pointer to page frame */
		__attribute__((const));
/*************************************************************//**
Returns the max trx id field value. */
UNIV_INLINE
trx_id_t
page_get_max_trx_id(
/*================*/
	const page_t*	page);	/*!< in: page */
/*************************************************************//**
Sets the max trx id field value. */
UNIV_INTERN
void
page_set_max_trx_id(
/*================*/
	buf_block_t*	block,	/*!< in/out: page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	trx_id_t	trx_id,	/*!< in: transaction id */
	mtr_t*		mtr);	/*!< in/out: mini-transaction, or NULL */
/*************************************************************//**
Sets the max trx id field value if trx_id is bigger than the previous
value. */
UNIV_INLINE
void
page_update_max_trx_id(
/*===================*/
	buf_block_t*	block,	/*!< in/out: page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose
				uncompressed part will be updated, or NULL */
	trx_id_t	trx_id,	/*!< in: transaction id */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */
/*************************************************************//**
Reads the given header field. */
UNIV_INLINE
ulint
page_header_get_field(
/*==================*/
	const page_t*	page,	/*!< in: page */
	ulint		field);	/*!< in: PAGE_N_DIR_SLOTS, ... */
/*************************************************************//**
Sets the given header field. */
UNIV_INLINE
void
page_header_set_field(
/*==================*/
	page_t*		page,	/*!< in/out: page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose
				uncompressed part will be updated, or NULL */
	ulint		field,	/*!< in: PAGE_N_DIR_SLOTS, ... */
	ulint		val);	/*!< in: value */
/*************************************************************//**
Returns the offset stored in the given header field.
@return	offset from the start of the page, or 0 */
UNIV_INLINE
ulint
page_header_get_offs(
/*=================*/
	const page_t*	page,	/*!< in: page */
	ulint		field)	/*!< in: PAGE_FREE, ... */
	__attribute__((nonnull, pure));

/*************************************************************//**
Returns the pointer stored in the given header field, or NULL. */
#define page_header_get_ptr(page, field)			\
	(page_header_get_offs(page, field)			\
	 ? page + page_header_get_offs(page, field) : NULL)
/*************************************************************//**
Sets the pointer stored in the given header field. */
UNIV_INLINE
void
page_header_set_ptr(
/*================*/
	page_t*		page,	/*!< in/out: page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose
				uncompressed part will be updated, or NULL */
	ulint		field,	/*!< in/out: PAGE_FREE, ... */
	const byte*	ptr);	/*!< in: pointer or NULL*/
#ifndef UNIV_HOTBACKUP
/*************************************************************//**
Resets the last insert info field in the page header. Writes to mlog
about this operation. */
UNIV_INLINE
void
page_header_reset_last_insert(
/*==========================*/
	page_t*		page,	/*!< in: page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose
				uncompressed part will be updated, or NULL */
	mtr_t*		mtr);	/*!< in: mtr */
#endif /* !UNIV_HOTBACKUP */
/************************************************************//**
Gets the offset of the first record on the page.
@return	offset of the first record in record list, relative from page */
UNIV_INLINE
ulint
page_get_infimum_offset(
/*====================*/
	const page_t*	page);	/*!< in: page which must have record(s) */
/************************************************************//**
Gets the offset of the last record on the page.
@return	offset of the last record in record list, relative from page */
UNIV_INLINE
ulint
page_get_supremum_offset(
/*=====================*/
	const page_t*	page);	/*!< in: page which must have record(s) */
#define page_get_infimum_rec(page) ((page) + page_get_infimum_offset(page))
#define page_get_supremum_rec(page) ((page) + page_get_supremum_offset(page))

/************************************************************//**
Returns the nth record of the record list.
This is the inverse function of page_rec_get_n_recs_before().
@return	nth record */
UNIV_INTERN
const rec_t*
page_rec_get_nth_const(
/*===================*/
	const page_t*	page,	/*!< in: page */
	ulint		nth)	/*!< in: nth record */
	__attribute__((nonnull, warn_unused_result));
/************************************************************//**
Returns the nth record of the record list.
This is the inverse function of page_rec_get_n_recs_before().
@return	nth record */
UNIV_INLINE
rec_t*
page_rec_get_nth(
/*=============*/
	page_t*	page,	/*< in: page */
	ulint	nth)	/*!< in: nth record */
	__attribute__((nonnull, warn_unused_result));

#ifndef UNIV_HOTBACKUP
/************************************************************//**
Returns the middle record of the records on the page. If there is an
even number of records in the list, returns the first record of the
upper half-list.
@return	middle record */
UNIV_INLINE
rec_t*
page_get_middle_rec(
/*================*/
	page_t*	page)	/*!< in: page */
	__attribute__((nonnull, warn_unused_result));
/*************************************************************//**
Compares a data tuple to a physical record. Differs from the function
cmp_dtuple_rec_with_match in the way that the record must reside on an
index page, and also page infimum and supremum records can be given in
the parameter rec. These are considered as the negative infinity and
the positive infinity in the alphabetical order.
@return 1, 0, -1, if dtuple is greater, equal, less than rec,
respectively, when only the common first fields are compared */
UNIV_INLINE
int
page_cmp_dtuple_rec_with_match(
/*===========================*/
	const dtuple_t*	dtuple,	/*!< in: data tuple */
	const rec_t*	rec,	/*!< in: physical record on a page; may also
				be page infimum or supremum, in which case
				matched-parameter values below are not
				affected */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint*		matched_fields, /*!< in/out: number of already completely
				matched fields; when function returns
				contains the value for current comparison */
	ulint*		matched_bytes); /*!< in/out: number of already matched
				bytes within the first field not completely
				matched; when function returns contains the
				value for current comparison */
#endif /* !UNIV_HOTBACKUP */
/*************************************************************//**
Gets the page number.
@return	page number */
UNIV_INLINE
ulint
page_get_page_no(
/*=============*/
	const page_t*	page);	/*!< in: page */
/*************************************************************//**
Gets the tablespace identifier.
@return	space id */
UNIV_INLINE
ulint
page_get_space_id(
/*==============*/
	const page_t*	page);	/*!< in: page */
/*************************************************************//**
Gets the number of user records on page (the infimum and supremum records
are not user records).
@return	number of user records */
UNIV_INLINE
ulint
page_get_n_recs(
/*============*/
	const page_t*	page);	/*!< in: index page */
/***************************************************************//**
Returns the number of records before the given record in chain.
The number includes infimum and supremum records.
This is the inverse function of page_rec_get_nth().
@return	number of records */
UNIV_INTERN
ulint
page_rec_get_n_recs_before(
/*=======================*/
	const rec_t*	rec);	/*!< in: the physical record */
/*************************************************************//**
Gets the number of records in the heap.
@return	number of user records */
UNIV_INLINE
ulint
page_dir_get_n_heap(
/*================*/
	const page_t*	page);	/*!< in: index page */
/*************************************************************//**
Sets the number of records in the heap. */
UNIV_INLINE
void
page_dir_set_n_heap(
/*================*/
	page_t*		page,	/*!< in/out: index page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose
				uncompressed part will be updated, or NULL.
				Note that the size of the dense page directory
				in the compressed page trailer is
				n_heap * PAGE_ZIP_DIR_SLOT_SIZE. */
	ulint		n_heap);/*!< in: number of records */
/*************************************************************//**
Gets the number of dir slots in directory.
@return	number of slots */
UNIV_INLINE
ulint
page_dir_get_n_slots(
/*=================*/
	const page_t*	page);	/*!< in: index page */
/*************************************************************//**
Sets the number of dir slots in directory. */
UNIV_INLINE
void
page_dir_set_n_slots(
/*=================*/
	page_t*		page,	/*!< in/out: page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose
				uncompressed part will be updated, or NULL */
	ulint		n_slots);/*!< in: number of slots */
#ifdef UNIV_DEBUG
/*************************************************************//**
Gets pointer to nth directory slot.
@return	pointer to dir slot */
UNIV_INLINE
page_dir_slot_t*
page_dir_get_nth_slot(
/*==================*/
	const page_t*	page,	/*!< in: index page */
	ulint		n);	/*!< in: position */
#else /* UNIV_DEBUG */
# define page_dir_get_nth_slot(page, n)		\
	((page) + UNIV_PAGE_SIZE - PAGE_DIR	\
	 - (n + 1) * PAGE_DIR_SLOT_SIZE)
#endif /* UNIV_DEBUG */
/**************************************************************//**
Used to check the consistency of a record on a page.
@return	TRUE if succeed */
UNIV_INLINE
ibool
page_rec_check(
/*===========*/
	const rec_t*	rec);	/*!< in: record */
/***************************************************************//**
Gets the record pointed to by a directory slot.
@return	pointer to record */
UNIV_INLINE
const rec_t*
page_dir_slot_get_rec(
/*==================*/
	const page_dir_slot_t*	slot);	/*!< in: directory slot */
/***************************************************************//**
This is used to set the record offset in a directory slot. */
UNIV_INLINE
void
page_dir_slot_set_rec(
/*==================*/
	page_dir_slot_t* slot,	/*!< in: directory slot */
	rec_t*		 rec);	/*!< in: record on the page */
/***************************************************************//**
Gets the number of records owned by a directory slot.
@return	number of records */
UNIV_INLINE
ulint
page_dir_slot_get_n_owned(
/*======================*/
	const page_dir_slot_t*	slot);	/*!< in: page directory slot */
/***************************************************************//**
This is used to set the owned records field of a directory slot. */
UNIV_INLINE
void
page_dir_slot_set_n_owned(
/*======================*/
	page_dir_slot_t*slot,	/*!< in/out: directory slot */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	ulint		n);	/*!< in: number of records owned by the slot */
/************************************************************//**
Calculates the space reserved for directory slots of a given
number of records. The exact value is a fraction number
n * PAGE_DIR_SLOT_SIZE / PAGE_DIR_SLOT_MIN_N_OWNED, and it is
rounded upwards to an integer. */
UNIV_INLINE
ulint
page_dir_calc_reserved_space(
/*=========================*/
	ulint	n_recs);	/*!< in: number of records */
/***************************************************************//**
Looks for the directory slot which owns the given record.
@return	the directory slot number */
UNIV_INTERN
ulint
page_dir_find_owner_slot(
/*=====================*/
	const rec_t*	rec);	/*!< in: the physical record */
/************************************************************//**
Determine whether the page is in new-style compact format.
@return nonzero if the page is in compact format, zero if it is in
old-style format */
UNIV_INLINE
ulint
page_is_comp(
/*=========*/
	const page_t*	page);	/*!< in: index page */
/************************************************************//**
TRUE if the record is on a page in compact format.
@return	nonzero if in compact format */
UNIV_INLINE
ulint
page_rec_is_comp(
/*=============*/
	const rec_t*	rec);	/*!< in: record */
/***************************************************************//**
Returns the heap number of a record.
@return	heap number */
UNIV_INLINE
ulint
page_rec_get_heap_no(
/*=================*/
	const rec_t*	rec);	/*!< in: the physical record */
/************************************************************//**
Determine whether the page is a B-tree leaf.
@return	TRUE if the page is a B-tree leaf */
UNIV_INLINE
ibool
page_is_leaf(
/*=========*/
	const page_t*	page)	/*!< in: page */
	__attribute__((nonnull, pure));
/************************************************************//**
Gets the pointer to the next record on the page.
@return	pointer to next record */
UNIV_INLINE
const rec_t*
page_rec_get_next_low(
/*==================*/
	const rec_t*	rec,	/*!< in: pointer to record */
	ulint		comp);	/*!< in: nonzero=compact page layout */
/************************************************************//**
Gets the pointer to the next record on the page.
@return	pointer to next record */
UNIV_INLINE
rec_t*
page_rec_get_next(
/*==============*/
	rec_t*	rec);	/*!< in: pointer to record */
/************************************************************//**
Gets the pointer to the next record on the page.
@return	pointer to next record */
UNIV_INLINE
const rec_t*
page_rec_get_next_const(
/*====================*/
	const rec_t*	rec);	/*!< in: pointer to record */
/************************************************************//**
Gets the pointer to the next non delete-marked record on the page.
If all subsequent records are delete-marked, then this function
will return the supremum record.
@return	pointer to next non delete-marked record or pointer to supremum */
UNIV_INLINE
const rec_t*
page_rec_get_next_non_del_marked(
/*=============================*/
	const rec_t*	rec);	/*!< in: pointer to record */
/************************************************************//**
Sets the pointer to the next record on the page. */
UNIV_INLINE
void
page_rec_set_next(
/*==============*/
	rec_t*	rec,	/*!< in: pointer to record,
			must not be page supremum */
	rec_t*	next);	/*!< in: pointer to next record,
			must not be page infimum */
/************************************************************//**
Gets the pointer to the previous record.
@return	pointer to previous record */
UNIV_INLINE
const rec_t*
page_rec_get_prev_const(
/*====================*/
	const rec_t*	rec);	/*!< in: pointer to record, must not be page
				infimum */
/************************************************************//**
Gets the pointer to the previous record.
@return	pointer to previous record */
UNIV_INLINE
rec_t*
page_rec_get_prev(
/*==============*/
	rec_t*		rec);	/*!< in: pointer to record,
				must not be page infimum */
/************************************************************//**
TRUE if the record is a user record on the page.
@return	TRUE if a user record */
UNIV_INLINE
ibool
page_rec_is_user_rec_low(
/*=====================*/
	ulint	offset)	/*!< in: record offset on page */
	__attribute__((const));
/************************************************************//**
TRUE if the record is the supremum record on a page.
@return	TRUE if the supremum record */
UNIV_INLINE
ibool
page_rec_is_supremum_low(
/*=====================*/
	ulint	offset)	/*!< in: record offset on page */
	__attribute__((const));
/************************************************************//**
TRUE if the record is the infimum record on a page.
@return	TRUE if the infimum record */
UNIV_INLINE
ibool
page_rec_is_infimum_low(
/*====================*/
	ulint	offset)	/*!< in: record offset on page */
	__attribute__((const));

/************************************************************//**
TRUE if the record is a user record on the page.
@return	TRUE if a user record */
UNIV_INLINE
ibool
page_rec_is_user_rec(
/*=================*/
	const rec_t*	rec)	/*!< in: record */
	__attribute__((const));
/************************************************************//**
TRUE if the record is the supremum record on a page.
@return	TRUE if the supremum record */
UNIV_INLINE
ibool
page_rec_is_supremum(
/*=================*/
	const rec_t*	rec)	/*!< in: record */
	__attribute__((const));

/************************************************************//**
TRUE if the record is the infimum record on a page.
@return	TRUE if the infimum record */
UNIV_INLINE
ibool
page_rec_is_infimum(
/*================*/
	const rec_t*	rec)	/*!< in: record */
	__attribute__((const));
/***************************************************************//**
Looks for the record which owns the given record.
@return	the owner record */
UNIV_INLINE
rec_t*
page_rec_find_owner_rec(
/*====================*/
	rec_t*	rec);	/*!< in: the physical record */
#ifndef UNIV_HOTBACKUP
/***********************************************************************//**
Write a 32-bit field in a data dictionary record. */
UNIV_INLINE
void
page_rec_write_field(
/*=================*/
	rec_t*	rec,	/*!< in/out: record to update */
	ulint	i,	/*!< in: index of the field to update */
	ulint	val,	/*!< in: value to write */
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
	__attribute__((nonnull));
#endif /* !UNIV_HOTBACKUP */
/************************************************************//**
Returns the maximum combined size of records which can be inserted on top
of record heap.
@return	maximum combined size for inserted records */
UNIV_INLINE
ulint
page_get_max_insert_size(
/*=====================*/
	const page_t*	page,	/*!< in: index page */
	ulint		n_recs);/*!< in: number of records */
/************************************************************//**
Returns the maximum combined size of records which can be inserted on top
of record heap if page is first reorganized.
@return	maximum combined size for inserted records */
UNIV_INLINE
ulint
page_get_max_insert_size_after_reorganize(
/*======================================*/
	const page_t*	page,	/*!< in: index page */
	ulint		n_recs);/*!< in: number of records */
/*************************************************************//**
Calculates free space if a page is emptied.
@return	free space */
UNIV_INLINE
ulint
page_get_free_space_of_empty(
/*=========================*/
	ulint	comp)	/*!< in: nonzero=compact page format */
		__attribute__((const));
/**********************************************************//**
Returns the base extra size of a physical record.  This is the
size of the fixed header, independent of the record size.
@return	REC_N_NEW_EXTRA_BYTES or REC_N_OLD_EXTRA_BYTES */
UNIV_INLINE
ulint
page_rec_get_base_extra_size(
/*=========================*/
	const rec_t*	rec);	/*!< in: physical record */
/************************************************************//**
Returns the sum of the sizes of the records in the record list
excluding the infimum and supremum records.
@return	data in bytes */
UNIV_INLINE
ulint
page_get_data_size(
/*===============*/
	const page_t*	page);	/*!< in: index page */
/************************************************************//**
Allocates a block of memory from the head of the free list
of an index page. */
UNIV_INLINE
void
page_mem_alloc_free(
/*================*/
	page_t*		page,	/*!< in/out: index page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page with enough
				space available for inserting the record,
				or NULL */
	rec_t*		next_rec,/*!< in: pointer to the new head of the
				free record list */
	ulint		need);	/*!< in: number of bytes allocated */
/************************************************************//**
Allocates a block of memory from the heap of an index page.
@return	pointer to start of allocated buffer, or NULL if allocation fails */
UNIV_INTERN
byte*
page_mem_alloc_heap(
/*================*/
	page_t*		page,	/*!< in/out: index page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page with enough
				space available for inserting the record,
				or NULL */
	ulint		need,	/*!< in: total number of bytes needed */
	ulint*		heap_no);/*!< out: this contains the heap number
				of the allocated record
				if allocation succeeds */
/************************************************************//**
Puts a record to free list. */
UNIV_INLINE
void
page_mem_free(
/*==========*/
	page_t*			page,	/*!< in/out: index page */
	page_zip_des_t*		page_zip,/*!< in/out: compressed page,
					 or NULL */
	rec_t*			rec,	/*!< in: pointer to the (origin of)
					record */
	const dict_index_t*	index,	/*!< in: index of rec */
	const ulint*		offsets);/*!< in: array returned by
					 rec_get_offsets() */
/**********************************************************//**
Create an uncompressed B-tree index page.
@return	pointer to the page */
UNIV_INTERN
page_t*
page_create(
/*========*/
	buf_block_t*	block,		/*!< in: a buffer block where the
					page is created */
	mtr_t*		mtr,		/*!< in: mini-transaction handle */
	ulint		comp);		/*!< in: nonzero=compact page format */
/**********************************************************//**
Create a compressed B-tree index page.
@return	pointer to the page */
UNIV_INTERN
page_t*
page_create_zip(
/*============*/
	buf_block_t*		block,		/*!< in/out: a buffer frame
						where the page is created */
	dict_index_t*		index,		/*!< in: the index of the
						page */
	ulint			level,		/*!< in: the B-tree level of
						the page */
	const redo_page_compress_t* page_comp_info,
						/*!< in: used for applying
						MLOG_FILE_TRUNCATE redo log
						record during recovery */
	mtr_t*			mtr);		/*!< in/out: mini-transaction
						handle */
/*************************************************************//**
Differs from page_copy_rec_list_end, because this function does not
touch the lock table and max trx id on page or compress the page. */
UNIV_INTERN
void
page_copy_rec_list_end_no_locks(
/*============================*/
	buf_block_t*	new_block,	/*!< in: index page to copy to */
	buf_block_t*	block,		/*!< in: index page of rec */
	rec_t*		rec,		/*!< in: record on page */
	dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*		mtr);		/*!< in: mtr */
/*************************************************************//**
Copies records from page to new_page, from the given record onward,
including that record. Infimum and supremum records are not copied.
The records are copied to the start of the record list on new_page.
@return pointer to the original successor of the infimum record on
new_page, or NULL on zip overflow (new_block will be decompressed) */
UNIV_INTERN
rec_t*
page_copy_rec_list_end(
/*===================*/
	buf_block_t*	new_block,	/*!< in/out: index page to copy to */
	buf_block_t*	block,		/*!< in: index page containing rec */
	rec_t*		rec,		/*!< in: record on page */
	dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*		mtr)		/*!< in: mtr */
	__attribute__((nonnull));
/*************************************************************//**
Copies records from page to new_page, up to the given record, NOT
including that record. Infimum and supremum records are not copied.
The records are copied to the end of the record list on new_page.
@return pointer to the original predecessor of the supremum record on
new_page, or NULL on zip overflow (new_block will be decompressed) */
UNIV_INTERN
rec_t*
page_copy_rec_list_start(
/*=====================*/
	buf_block_t*	new_block,	/*!< in/out: index page to copy to */
	buf_block_t*	block,		/*!< in: index page containing rec */
	rec_t*		rec,		/*!< in: record on page */
	dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*		mtr)		/*!< in: mtr */
	__attribute__((nonnull));
/*************************************************************//**
Deletes records from a page from a given record onward, including that record.
The infimum and supremum records are not deleted. */
UNIV_INTERN
void
page_delete_rec_list_end(
/*=====================*/
	rec_t*		rec,	/*!< in: pointer to record on page */
	buf_block_t*	block,	/*!< in: buffer block of the page */
	dict_index_t*	index,	/*!< in: record descriptor */
	ulint		n_recs,	/*!< in: number of records to delete,
				or ULINT_UNDEFINED if not known */
	ulint		size,	/*!< in: the sum of the sizes of the
				records in the end of the chain to
				delete, or ULINT_UNDEFINED if not known */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull));
/*************************************************************//**
Deletes records from page, up to the given record, NOT including
that record. Infimum and supremum records are not deleted. */
UNIV_INTERN
void
page_delete_rec_list_start(
/*=======================*/
	rec_t*		rec,	/*!< in: record on page */
	buf_block_t*	block,	/*!< in: buffer block of the page */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr)	/*!< in: mtr */
	__attribute__((nonnull));
/*************************************************************//**
Moves record list end to another page. Moved records include
split_rec.
@return TRUE on success; FALSE on compression failure (new_block will
be decompressed) */
UNIV_INTERN
ibool
page_move_rec_list_end(
/*===================*/
	buf_block_t*	new_block,	/*!< in/out: index page where to move */
	buf_block_t*	block,		/*!< in: index page from where to move */
	rec_t*		split_rec,	/*!< in: first record to move */
	dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*		mtr)		/*!< in: mtr */
	__attribute__((nonnull(1, 2, 4, 5)));
/*************************************************************//**
Moves record list start to another page. Moved records do not include
split_rec.
@return	TRUE on success; FALSE on compression failure */
UNIV_INTERN
ibool
page_move_rec_list_start(
/*=====================*/
	buf_block_t*	new_block,	/*!< in/out: index page where to move */
	buf_block_t*	block,		/*!< in/out: page containing split_rec */
	rec_t*		split_rec,	/*!< in: first record not to move */
	dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*		mtr)		/*!< in: mtr */
	__attribute__((nonnull(1, 2, 4, 5)));
/****************************************************************//**
Splits a directory slot which owns too many records. */
UNIV_INTERN
void
page_dir_split_slot(
/*================*/
	page_t*		page,	/*!< in: index page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose
				uncompressed part will be written, or NULL */
	ulint		slot_no)/*!< in: the directory slot */
	__attribute__((nonnull(1)));
/*************************************************************//**
Tries to balance the given directory slot with too few records
with the upper neighbor, so that there are at least the minimum number
of records owned by the slot; this may result in the merging of
two slots. */
UNIV_INTERN
void
page_dir_balance_slot(
/*==================*/
	page_t*		page,	/*!< in/out: index page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	ulint		slot_no)/*!< in: the directory slot */
	__attribute__((nonnull(1)));
/**********************************************************//**
Parses a log record of a record list end or start deletion.
@return	end of log record or NULL */
UNIV_INTERN
byte*
page_parse_delete_rec_list(
/*=======================*/
	byte		type,	/*!< in: MLOG_LIST_END_DELETE,
				MLOG_LIST_START_DELETE,
				MLOG_COMP_LIST_END_DELETE or
				MLOG_COMP_LIST_START_DELETE */
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	buf_block_t*	block,	/*!< in/out: buffer block or NULL */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr);	/*!< in: mtr or NULL */
/***********************************************************//**
Parses a redo log record of creating a page.
@return	end of log record or NULL */
UNIV_INTERN
byte*
page_parse_create(
/*==============*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	ulint		comp,	/*!< in: nonzero=compact page format */
	buf_block_t*	block,	/*!< in: block or NULL */
	mtr_t*		mtr);	/*!< in: mtr or NULL */
#ifndef UNIV_HOTBACKUP
/************************************************************//**
Prints record contents including the data relevant only in
the index page context. */
UNIV_INTERN
void
page_rec_print(
/*===========*/
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets);/*!< in: record descriptor */
# ifdef UNIV_BTR_PRINT
/***************************************************************//**
This is used to print the contents of the directory for
debugging purposes. */
UNIV_INTERN
void
page_dir_print(
/*===========*/
	page_t*	page,	/*!< in: index page */
	ulint	pr_n);	/*!< in: print n first and n last entries */
/***************************************************************//**
This is used to print the contents of the page record list for
debugging purposes. */
UNIV_INTERN
void
page_print_list(
/*============*/
	buf_block_t*	block,	/*!< in: index page */
	dict_index_t*	index,	/*!< in: dictionary index of the page */
	ulint		pr_n);	/*!< in: print n first and n last entries */
/***************************************************************//**
Prints the info in a page header. */
UNIV_INTERN
void
page_header_print(
/*==============*/
	const page_t*	page);	/*!< in: index page */
/***************************************************************//**
This is used to print the contents of the page for
debugging purposes. */
UNIV_INTERN
void
page_print(
/*=======*/
	buf_block_t*	block,	/*!< in: index page */
	dict_index_t*	index,	/*!< in: dictionary index of the page */
	ulint		dn,	/*!< in: print dn first and last entries
				in directory */
	ulint		rn);	/*!< in: print rn first and last records
				in directory */
# endif /* UNIV_BTR_PRINT */
#endif /* !UNIV_HOTBACKUP */
/***************************************************************//**
The following is used to validate a record on a page. This function
differs from rec_validate as it can also check the n_owned field and
the heap_no field.
@return	TRUE if ok */
UNIV_INTERN
ibool
page_rec_validate(
/*==============*/
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets);/*!< in: array returned by rec_get_offsets() */
/***************************************************************//**
Checks that the first directory slot points to the infimum record and
the last to the supremum. This function is intended to track if the
bug fixed in 4.0.14 has caused corruption to users' databases. */
UNIV_INTERN
void
page_check_dir(
/*===========*/
	const page_t*	page);	/*!< in: index page */
/***************************************************************//**
This function checks the consistency of an index page when we do not
know the index. This is also resilient so that this should never crash
even if the page is total garbage.
@return	TRUE if ok */
UNIV_INTERN
ibool
page_simple_validate_old(
/*=====================*/
	const page_t*	page);	/*!< in: index page in ROW_FORMAT=REDUNDANT */
/***************************************************************//**
This function checks the consistency of an index page when we do not
know the index. This is also resilient so that this should never crash
even if the page is total garbage.
@return	TRUE if ok */
UNIV_INTERN
ibool
page_simple_validate_new(
/*=====================*/
	const page_t*	page);	/*!< in: index page in ROW_FORMAT!=REDUNDANT */
/***************************************************************//**
This function checks the consistency of an index page.
@return	TRUE if ok */
UNIV_INTERN
ibool
page_validate(
/*==========*/
	const page_t*	page,	/*!< in: index page */
	dict_index_t*	index);	/*!< in: data dictionary index containing
				the page record type definition */
/***************************************************************//**
Looks in the page record list for a record with the given heap number.
@return	record, NULL if not found */

const rec_t*
page_find_rec_with_heap_no(
/*=======================*/
	const page_t*	page,	/*!< in: index page */
	ulint		heap_no);/*!< in: heap number */
#ifdef UNIV_MATERIALIZE
#undef UNIV_INLINE
#define UNIV_INLINE  UNIV_INLINE_ORIGINAL
#endif

#ifndef UNIV_NONINL
#include "page0page.ic"
#endif

#endif
