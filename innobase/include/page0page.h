/******************************************************
Index page routines

(c) 1994-1996 Innobase Oy

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
				a record on the page; a dulint; defined only
				in secondary indexes; specifically, not in an
				ibuf tree; NOTE: this may be modified only
				when the thread has an x-latch to the page,
				and ALSO an x-latch to btr_search_latch
				if there is a hash index to the page! */
#define PAGE_HEADER_PRIV_END 26	/* end of private data structure of the page
				header which are set in a page create */
/*----*/
#define	PAGE_LEVEL	 26	/* level of the node in an index tree; the
				leaf level is the level 0 */
#define	PAGE_INDEX_ID	 28	/* index id where the page belongs */
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

/*****************************************************************
Returns the max trx id field value. */
UNIV_INLINE
dulint
page_get_max_trx_id(
/*================*/
	page_t*	page);	/* in: page */
/*****************************************************************
Sets the max trx id field value. */

void
page_set_max_trx_id(
/*================*/
	page_t*	page,	/* in: page */
	dulint	trx_id);/* in: transaction id */
/*****************************************************************
Sets the max trx id field value if trx_id is bigger than the previous
value. */
UNIV_INLINE
void
page_update_max_trx_id(
/*===================*/
	page_t*	page,	/* in: page */
	dulint	trx_id);	/* in: transaction id */
/*****************************************************************
Reads the given header field. */
UNIV_INLINE
ulint
page_header_get_field(
/*==================*/
	page_t*	page,	/* in: page */
	ulint	field);	/* in: PAGE_N_DIR_SLOTS, ... */
/*****************************************************************
Sets the given header field. */
UNIV_INLINE
void
page_header_set_field(
/*==================*/
	page_t*	page,	/* in: page */
	ulint	field,	/* in: PAGE_N_DIR_SLOTS, ... */
	ulint	val);	/* in: value */
/*****************************************************************
Returns the pointer stored in the given header field. */
UNIV_INLINE
byte*
page_header_get_ptr(
/*================*/
			/* out: pointer or NULL */
	page_t*	page,	/* in: page */
	ulint	field);	/* in: PAGE_FREE, ... */
/*****************************************************************
Sets the pointer stored in the given header field. */
UNIV_INLINE
void
page_header_set_ptr(
/*================*/
	page_t*	page,	/* in: page */
	ulint	field,	/* in: PAGE_FREE, ... */
	byte*	ptr);	/* in: pointer or NULL*/
/*****************************************************************
Resets the last insert info field in the page header. Writes to mlog
about this operation. */
UNIV_INLINE
void
page_header_reset_last_insert(
/*==========================*/
	page_t*	page,	/* in: page */
	mtr_t*	mtr);	/* in: mtr */
/****************************************************************
Gets the first record on the page. */
UNIV_INLINE
rec_t*
page_get_infimum_rec(
/*=================*/
			/* out: the first record in record list */
	page_t*	page);	/* in: page which must have record(s) */
/****************************************************************
Gets the last record on the page. */
UNIV_INLINE
rec_t*
page_get_supremum_rec(
/*==================*/
			/* out: the last record in record list */
	page_t*	page);	/* in: page which must have record(s) */
/****************************************************************
Returns the middle record of record list. If there are an even number
of records in the list, returns the first record of upper half-list. */

rec_t*
page_get_middle_rec(
/*================*/
			/* out: middle record */
	page_t*	page);	/* in: page */
/*****************************************************************
Compares a data tuple to a physical record. Differs from the function
cmp_dtuple_rec_with_match in the way that the record must reside on an
index page, and also page infimum and supremum records can be given in
the parameter rec. These are considered as the negative infinity and
the positive infinity in the alphabetical order. */
UNIV_INLINE
int
page_cmp_dtuple_rec_with_match(
/*===========================*/	
				/* out: 1, 0, -1, if dtuple is greater, equal, 
				less than rec, respectively, when only the 
				common first fields are compared */
	dtuple_t*	dtuple,	/* in: data tuple */
	rec_t*		rec,	/* in: physical record on a page; may also 
				be page infimum or supremum, in which case 
				matched-parameter values below are not 
				affected */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint*	 	matched_fields, /* in/out: number of already completely 
				matched fields; when function returns
				contains the value for current comparison */
	ulint*	  	matched_bytes); /* in/out: number of already matched 
				bytes within the first field not completely
				matched; when function returns contains the
				value for current comparison */
/*****************************************************************
Gets the number of user records on page (the infimum and supremum records
are not user records). */
UNIV_INLINE
ulint
page_get_n_recs(
/*============*/
			/* out: number of user records */
	page_t*	page);	/* in: index page */
/*******************************************************************
Returns the number of records before the given record in chain.
The number includes infimum and supremum records. */

ulint
page_rec_get_n_recs_before(
/*=======================*/
			/* out: number of records */
	rec_t*	rec);	/* in: the physical record */
/*****************************************************************
Gets the number of records in the heap. */
UNIV_INLINE
ulint
page_dir_get_n_heap(
/*================*/
			/* out: number of user records */
	page_t*	page);	/* in: index page */
/*****************************************************************
Sets the number of records in the heap. */
UNIV_INLINE
void
page_dir_set_n_heap(
/*================*/
	page_t*	page,	/* in: index page */
	ulint	n_heap);/* in: number of records */
/*****************************************************************
Gets the number of dir slots in directory. */
UNIV_INLINE
ulint
page_dir_get_n_slots(
/*=================*/
			/* out: number of slots */
	page_t*	page);	/* in: index page */
/*****************************************************************
Sets the number of dir slots in directory. */
UNIV_INLINE
void
page_dir_set_n_slots(
/*=================*/
			/* out: number of slots */
	page_t*	page,	/* in: index page */
	ulint	n_slots);/* in: number of slots */
/*****************************************************************
Gets pointer to nth directory slot. */
UNIV_INLINE
page_dir_slot_t*
page_dir_get_nth_slot(
/*==================*/
			/* out: pointer to dir slot */
	page_t*	page,	/* in: index page */
	ulint	n);	/* in: position */
/******************************************************************
Used to check the consistency of a record on a page. */
UNIV_INLINE
ibool
page_rec_check(
/*===========*/
			/* out: TRUE if succeed */
	rec_t*	rec);	/* in: record */
/*******************************************************************
Gets the record pointed to by a directory slot. */
UNIV_INLINE
rec_t*
page_dir_slot_get_rec(
/*==================*/
					/* out: pointer to record */
	page_dir_slot_t*	slot);	/* in: directory slot */
/*******************************************************************
This is used to set the record offset in a directory slot. */
UNIV_INLINE
void
page_dir_slot_set_rec(
/*==================*/
	page_dir_slot_t* slot,	/* in: directory slot */
	rec_t*		 rec);	/* in: record on the page */
/*******************************************************************
Gets the number of records owned by a directory slot. */
UNIV_INLINE
ulint
page_dir_slot_get_n_owned(
/*======================*/
					/* out: number of records */
	page_dir_slot_t* 	slot);	/* in: page directory slot */
/*******************************************************************
This is used to set the owned records field of a directory slot. */
UNIV_INLINE
void
page_dir_slot_set_n_owned(
/*======================*/
	page_dir_slot_t*	slot,	/* in: directory slot */
	ulint			n);	/* in: number of records owned 
					by the slot */
/****************************************************************
Calculates the space reserved for directory slots of a given
number of records. The exact value is a fraction number
n * PAGE_DIR_SLOT_SIZE / PAGE_DIR_SLOT_MIN_N_OWNED, and it is
rounded upwards to an integer. */
UNIV_INLINE
ulint
page_dir_calc_reserved_space(
/*=========================*/
	ulint	n_recs);	/* in: number of records */
/*******************************************************************
Looks for the directory slot which owns the given record. */

ulint
page_dir_find_owner_slot(
/*=====================*/
				/* out: the directory slot number */
	rec_t*		rec);	/* in: the physical record */
/****************************************************************
Determine whether the page is in new-style compact format. */
UNIV_INLINE
ibool
page_is_comp(
/*=========*/
			/* out: TRUE if the page is in compact format
			FALSE if it is in old-style format */
	page_t*	page);	/* in: index page */
/****************************************************************
Gets the pointer to the next record on the page. */
UNIV_INLINE
rec_t*
page_rec_get_next(
/*==============*/
			/* out: pointer to next record */
	rec_t*	rec);	/* in: pointer to record, must not be page
			supremum */
/****************************************************************
Sets the pointer to the next record on the page. */ 
UNIV_INLINE
void
page_rec_set_next(
/*==============*/
	rec_t*	rec,	/* in: pointer to record, must not be
			page supremum */
	rec_t*	next);	/* in: pointer to next record, must not
			be page infimum */
/****************************************************************
Gets the pointer to the previous record. */
UNIV_INLINE
rec_t*
page_rec_get_prev(
/*==============*/
				/* out: pointer to previous record */
	rec_t*		rec);	/* in: pointer to record,
				must not be page infimum */

/****************************************************************
TRUE if the record is a user record on the page. */
UNIV_INLINE
ibool
page_rec_is_user_rec(
/*=================*/
			/* out: TRUE if a user record */
	rec_t*	rec);	/* in: record */
/****************************************************************
TRUE if the record is the supremum record on a page. */
UNIV_INLINE
ibool
page_rec_is_supremum(
/*=================*/
			/* out: TRUE if the supremum record */
	rec_t*	rec);	/* in: record */
/****************************************************************
TRUE if the record is the infimum record on a page. */
UNIV_INLINE
ibool
page_rec_is_infimum(
/*================*/
			/* out: TRUE if the infimum record */
	rec_t*	rec);	/* in: record */
/****************************************************************
TRUE if the record is the first user record on the page. */
UNIV_INLINE
ibool
page_rec_is_first_user_rec(
/*=======================*/
			/* out: TRUE if first user record */
	rec_t*	rec);	/* in: record */
/****************************************************************
TRUE if the record is the last user record on the page. */
UNIV_INLINE
ibool
page_rec_is_last_user_rec(
/*======================*/
			/* out: TRUE if last user record */
	rec_t*	rec);	/* in: record */
/*******************************************************************
Looks for the record which owns the given record. */
UNIV_INLINE
rec_t*
page_rec_find_owner_rec(
/*====================*/
			/* out: the owner record */
	rec_t*	rec);	/* in: the physical record */
/***************************************************************************
This is a low-level operation which is used in a database index creation
to update the page number of a created B-tree to a data dictionary
record. */

void
page_rec_write_index_page_no(
/*=========================*/
	rec_t*	rec,	/* in: record to update */
	ulint	i,	/* in: index of the field to update */
	ulint	page_no,/* in: value to write */
	mtr_t*	mtr);	/* in: mtr */
/****************************************************************
Returns the maximum combined size of records which can be inserted on top
of record heap. */
UNIV_INLINE
ulint
page_get_max_insert_size(
/*=====================*/
			/* out: maximum combined size for inserted records */
	page_t*	page,	/* in: index page */
	ulint	n_recs);	/* in: number of records */
/****************************************************************
Returns the maximum combined size of records which can be inserted on top
of record heap if page is first reorganized. */
UNIV_INLINE
ulint
page_get_max_insert_size_after_reorganize(
/*======================================*/
			/* out: maximum combined size for inserted records */
	page_t*	page,	/* in: index page */
	ulint	n_recs);/* in: number of records */
/*****************************************************************
Calculates free space if a page is emptied. */
UNIV_INLINE
ulint
page_get_free_space_of_empty(
/*=========================*/
			/* out: free space */
	ibool	comp)	/* in: TRUE=compact page format */
		__attribute__((const));
/****************************************************************
Returns the sum of the sizes of the records in the record list
excluding the infimum and supremum records. */
UNIV_INLINE
ulint
page_get_data_size(
/*===============*/
			/* out: data in bytes */
	page_t*	page);	/* in: index page */
/****************************************************************
Allocates a block of memory from an index page. */

byte*
page_mem_alloc(
/*===========*/
				/* out: pointer to start of allocated
				buffer, or NULL if allocation fails */
	page_t*		page,	/* in: index page */
	ulint		need,	/* in: number of bytes needed */
	dict_index_t*	index,	/* in: record descriptor */
	ulint*		heap_no);/* out: this contains the heap number
				of the allocated record
				if allocation succeeds */
/****************************************************************
Puts a record to free list. */
UNIV_INLINE
void
page_mem_free(
/*==========*/
	page_t*		page,	/* in: index page */
	rec_t*		rec,	/* in: pointer to the (origin of) record */
	dict_index_t*	index);	/* in: record descriptor */
/**************************************************************
The index page creation function. */

page_t* 
page_create(
/*========*/
					/* out: pointer to the page */
	buf_frame_t*	frame,		/* in: a buffer frame where the page is
					created */
	mtr_t*		mtr,		/* in: mini-transaction handle */
	ibool		comp);		/* in: TRUE=compact page format */
/*****************************************************************
Differs from page_copy_rec_list_end, because this function does not
touch the lock table and max trx id on page. */

void
page_copy_rec_list_end_no_locks(
/*============================*/
	page_t*		new_page,	/* in: index page to copy to */
	page_t*		page,		/* in: index page */
	rec_t*		rec,		/* in: record on page */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr);		/* in: mtr */
/*****************************************************************
Copies records from page to new_page, from the given record onward,
including that record. Infimum and supremum records are not copied.
The records are copied to the start of the record list on new_page. */

void
page_copy_rec_list_end(
/*===================*/
	page_t*		new_page,	/* in: index page to copy to */
	page_t*		page,		/* in: index page */
	rec_t*		rec,		/* in: record on page */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr);		/* in: mtr */
/*****************************************************************
Copies records from page to new_page, up to the given record, NOT
including that record. Infimum and supremum records are not copied.
The records are copied to the end of the record list on new_page. */

void
page_copy_rec_list_start(
/*=====================*/
	page_t*		new_page,	/* in: index page to copy to */
	page_t*		page,		/* in: index page */
	rec_t*		rec,		/* in: record on page */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr);		/* in: mtr */
/*****************************************************************
Deletes records from a page from a given record onward, including that record.
The infimum and supremum records are not deleted. */

void
page_delete_rec_list_end(
/*=====================*/
	page_t*		page,	/* in: index page */
	rec_t*		rec,	/* in: record on page */
	dict_index_t*	index,	/* in: record descriptor */
	ulint		n_recs,	/* in: number of records to delete,
				or ULINT_UNDEFINED if not known */
	ulint		size,	/* in: the sum of the sizes of the
				records in the end of the chain to
				delete, or ULINT_UNDEFINED if not known */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Deletes records from page, up to the given record, NOT including
that record. Infimum and supremum records are not deleted. */

void
page_delete_rec_list_start(
/*=======================*/
	page_t*		page,	/* in: index page */
	rec_t*		rec,	/* in: record on page */
	dict_index_t*	index,	/* in: record descriptor */
	mtr_t*		mtr);	/* in: mtr */
/*****************************************************************
Moves record list end to another page. Moved records include
split_rec. */

void
page_move_rec_list_end(
/*===================*/
	page_t*		new_page,	/* in: index page where to move */
	page_t*		page,		/* in: index page */
	rec_t*		split_rec,	/* in: first record to move */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr);		/* in: mtr */
/*****************************************************************
Moves record list start to another page. Moved records do not include
split_rec. */

void
page_move_rec_list_start(
/*=====================*/
	page_t*		new_page,	/* in: index page where to move */
	page_t*		page,		/* in: index page */
	rec_t*		split_rec,	/* in: first record not to move */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr);		/* in: mtr */
/********************************************************************
Splits a directory slot which owns too many records. */

void
page_dir_split_slot(
/*================*/
	page_t*	page, 		/* in: the index page in question */
	ulint	slot_no); 	/* in: the directory slot */
/*****************************************************************
Tries to balance the given directory slot with too few records
with the upper neighbor, so that there are at least the minimum number 
of records owned by the slot; this may result in the merging of 
two slots. */

void
page_dir_balance_slot(
/*==================*/
	page_t*	page,		/* in: index page */
	ulint	slot_no); 	/* in: the directory slot */
/**************************************************************
Parses a log record of a record list end or start deletion. */

byte*
page_parse_delete_rec_list(
/*=======================*/
				/* out: end of log record or NULL */
	byte		type,	/* in: MLOG_LIST_END_DELETE,
				MLOG_LIST_START_DELETE,
				MLOG_COMP_LIST_END_DELETE or
				MLOG_COMP_LIST_START_DELETE */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	dict_index_t*	index,	/* in: record descriptor */
	page_t*		page,	/* in: page or NULL */
	mtr_t*		mtr);	/* in: mtr or NULL */
/***************************************************************
Parses a redo log record of creating a page. */

byte*
page_parse_create(
/*==============*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	ibool	comp,	/* in: TRUE=compact page format */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr);	/* in: mtr or NULL */
/****************************************************************
Prints record contents including the data relevant only in
the index page context. */
 
void
page_rec_print(
/*===========*/
	rec_t*		rec,	/* in: physical record */
	const ulint*	offsets);/* in: record descriptor */
/*******************************************************************
This is used to print the contents of the directory for
debugging purposes. */

void
page_dir_print(
/*===========*/
	page_t*	page,	/* in: index page */
	ulint	pr_n);	/* in: print n first and n last entries */
/*******************************************************************
This is used to print the contents of the page record list for
debugging purposes. */

void
page_print_list(
/*============*/
	page_t*		page,	/* in: index page */
	dict_index_t*	index,	/* in: dictionary index of the page */
	ulint		pr_n);	/* in: print n first and n last entries */
/*******************************************************************
Prints the info in a page header. */

void
page_header_print(
/*==============*/
	page_t*	page);
/*******************************************************************
This is used to print the contents of the page for
debugging purposes. */

void
page_print(
/*======*/
	page_t*		page,	/* in: index page */
	dict_index_t*	index,	/* in: dictionary index of the page */
	ulint		dn,	/* in: print dn first and last entries
				in directory */
	ulint		rn);	/* in: print rn first and last records
				in directory */
/*******************************************************************
The following is used to validate a record on a page. This function
differs from rec_validate as it can also check the n_owned field and
the heap_no field. */

ibool
page_rec_validate(
/*==============*/
				/* out: TRUE if ok */
	rec_t*		rec,	/* in: physical record */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
/*******************************************************************
Checks that the first directory slot points to the infimum record and
the last to the supremum. This function is intended to track if the
bug fixed in 4.0.14 has caused corruption to users' databases. */

void
page_check_dir(
/*===========*/
	page_t*	page);	/* in: index page */
/*******************************************************************
This function checks the consistency of an index page when we do not
know the index. This is also resilient so that this should never crash
even if the page is total garbage. */

ibool
page_simple_validate(
/*=================*/
			/* out: TRUE if ok */
	page_t*	page);	/* in: index page */
/*******************************************************************
This function checks the consistency of an index page. */

ibool
page_validate(
/*==========*/
				/* out: TRUE if ok */
	page_t*		page,	/* in: index page */
	dict_index_t*	index);	/* in: data dictionary index containing
				the page record type definition */
/*******************************************************************
Looks in the page record list for a record with the given heap number. */

rec_t*
page_find_rec_with_heap_no(
/*=======================*/
			/* out: record, NULL if not found */
	page_t*	page,	/* in: index page */
	ulint	heap_no);/* in: heap number */

#ifdef UNIV_MATERIALIZE
#undef UNIV_INLINE
#define UNIV_INLINE  UNIV_INLINE_ORIGINAL
#endif

#ifndef UNIV_NONINL
#include "page0page.ic"
#endif

#endif 
