/************************************************************************
The page cursor

(c) 1994-1996 Innobase Oy

Created 10/4/1994 Heikki Tuuri
*************************************************************************/

#ifndef page0cur_h
#define page0cur_h

#include "univ.i"

#include "page0types.h"
#include "page0page.h"
#include "rem0rec.h"
#include "data0data.h"
#include "mtr0mtr.h"


#define PAGE_CUR_ADAPT

/* Page cursor search modes; the values must be in this order! */

#define	PAGE_CUR_G	1
#define	PAGE_CUR_GE	2
#define	PAGE_CUR_L	3
#define	PAGE_CUR_LE	4
/*#define PAGE_CUR_LE_OR_EXTENDS 5*/ /* This is a search mode used in
				 "column LIKE 'abc%' ORDER BY column DESC";
				 we have to find strings which are <= 'abc' or
				 which extend it */
#ifdef UNIV_SEARCH_DEBUG
# define PAGE_CUR_DBG	6	/* As PAGE_CUR_LE, but skips search shortcut */
#endif /* UNIV_SEARCH_DEBUG */

#ifdef PAGE_CUR_ADAPT
# ifdef UNIV_SEARCH_PERF_STAT
extern ulint	page_cur_short_succ;
# endif /* UNIV_SEARCH_PERF_STAT */
#endif /* PAGE_CUR_ADAPT */

/*************************************************************
Gets pointer to the page frame where the cursor is positioned. */
UNIV_INLINE
page_t*
page_cur_get_page(
/*==============*/
				/* out: page */
	page_cur_t*	cur);	/* in: page cursor */
/*************************************************************
Gets the record where the cursor is positioned. */
UNIV_INLINE
rec_t*
page_cur_get_rec(
/*=============*/
				/* out: record */
	page_cur_t*	cur);	/* in: page cursor */
/*************************************************************
Sets the cursor object to point before the first user record 
on the page. */
UNIV_INLINE
void
page_cur_set_before_first(
/*======================*/
	page_t*		page,	/* in: index page */
	page_cur_t*	cur);	/* in: cursor */
/*************************************************************
Sets the cursor object to point after the last user record on 
the page. */
UNIV_INLINE
void
page_cur_set_after_last(
/*====================*/
	page_t*		page,	/* in: index page */
	page_cur_t*	cur);	/* in: cursor */
/*************************************************************
Returns TRUE if the cursor is before first user record on page. */
UNIV_INLINE
ibool
page_cur_is_before_first(
/*=====================*/
					/* out: TRUE if at start */
	const page_cur_t*	cur);	/* in: cursor */
/*************************************************************
Returns TRUE if the cursor is after last user record. */
UNIV_INLINE
ibool
page_cur_is_after_last(
/*===================*/
					/* out: TRUE if at end */
	const page_cur_t*	cur);	/* in: cursor */
/**************************************************************
Positions the cursor on the given record. */
UNIV_INLINE
void
page_cur_position(
/*==============*/
	rec_t*		rec,	/* in: record on a page */
	page_cur_t*	cur);	/* in: page cursor */
/**************************************************************
Invalidates a page cursor by setting the record pointer NULL. */
UNIV_INLINE
void
page_cur_invalidate(
/*================*/
	page_cur_t*	cur);	/* in: page cursor */
/**************************************************************
Moves the cursor to the next record on page. */
UNIV_INLINE
void
page_cur_move_to_next(
/*==================*/
	page_cur_t*	cur);	/* in: cursor; must not be after last */
/**************************************************************
Moves the cursor to the previous record on page. */
UNIV_INLINE
void
page_cur_move_to_prev(
/*==================*/
	page_cur_t*	cur);	/* in: cursor; must not before first */
/***************************************************************
Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The cursor stays at
the same position. */
UNIV_INLINE
rec_t*
page_cur_tuple_insert(
/*==================*/
				/* out: pointer to record if succeed, NULL
				otherwise */
	page_cur_t*	cursor,	/* in: a page cursor */
	page_zip_des_t*	page_zip,/* in/out: compressed page with at least
				25 + rec_size bytes available, or NULL */
	dtuple_t*	tuple,	/* in: pointer to a data tuple */
	dict_index_t*	index,	/* in: record descriptor */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/***************************************************************
Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The cursor stays at
the same position. */
UNIV_INLINE
rec_t*
page_cur_rec_insert(
/*================*/
				/* out: pointer to record if succeed, NULL
				otherwise */
	page_cur_t*	cursor,	/* in: a page cursor */
	page_zip_des_t*	page_zip,/* in/out: compressed page with at least
				25 + rec_size bytes available, or NULL */
	rec_t*		rec,	/* in: record to insert */
	dict_index_t*	index,	/* in: record descriptor */
	ulint*		offsets,/* in: rec_get_offsets(rec, index) */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/***************************************************************
Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The record to be
inserted can be in a data tuple or as a physical record. The other parameter
must then be NULL. The cursor stays at the same position. */

rec_t*
page_cur_insert_rec_low(
/*====================*/
				/* out: pointer to record if succeed, NULL
				otherwise */
	page_cur_t*	cursor,	/* in: a page cursor */
	page_zip_des_t*	page_zip,/* in/out: compressed page with at least
				25 + rec_size bytes available, or NULL */
	dtuple_t*	tuple,	/* in: pointer to a data tuple or NULL */
	dict_index_t*	index,	/* in: record descriptor */
	rec_t*		rec,	/* in: pointer to a physical record or NULL */
	ulint*		offsets,/* in: rec_get_offsets(rec, index) or NULL */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/*****************************************************************
Copies records from page to a newly created page, from a given record onward,
including that record. Infimum and supremum records are not copied. */

void
page_copy_rec_list_end_to_created_page(
/*===================================*/
	page_t*		new_page,	/* in: index page to copy to */
	rec_t*		rec,		/* in: first record to copy */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr);		/* in: mtr */
/***************************************************************
Deletes a record at the page cursor. The cursor is moved to the 
next record after the deleted one. */

void
page_cur_delete_rec(
/*================*/
	page_cur_t*  	cursor,	/* in/out: a page cursor */
	dict_index_t*	index,	/* in: record descriptor */
	const ulint*	offsets,/* in: rec_get_offsets(cursor->rec, index) */
	page_zip_des_t*	page_zip,/* in/out: compressed page with at least
				32 bytes available, or NULL */
	mtr_t*		mtr);	/* in: mini-transaction handle */
/********************************************************************
Searches the right position for a page cursor. */
UNIV_INLINE
ulint
page_cur_search(
/*============*/
				/* out: number of matched fields on the left */
	page_t*		page,	/* in: index page */
	dict_index_t*	index,	/* in: record descriptor */
	dtuple_t*	tuple,	/* in: data tuple */
	ulint		mode,	/* in: PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G,
				or PAGE_CUR_GE */
	page_cur_t*	cursor);/* out: page cursor */
/********************************************************************
Searches the right position for a page cursor. */

void
page_cur_search_with_match(
/*=======================*/
	page_t*		page,	/* in: index page */
	dict_index_t*	index,	/* in: record descriptor */
	dtuple_t*	tuple,	/* in: data tuple */
	ulint		mode,	/* in: PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G,
				or PAGE_CUR_GE */
	ulint*		iup_matched_fields,
				/* in/out: already matched fields in upper
				limit record */
	ulint*		iup_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */
	ulint*		ilow_matched_fields,
				/* in/out: already matched fields in lower
				limit record */
	ulint*		ilow_matched_bytes,
				/* in/out: already matched bytes in a field
				not yet completely matched */
	page_cur_t*	cursor); /* out: page cursor */ 
/***************************************************************
Positions a page cursor on a randomly chosen user record on a page. If there
are no user records, sets the cursor on the infimum record. */

void
page_cur_open_on_rnd_user_rec(
/*==========================*/
	page_t*		page,	/* in: page */
	page_cur_t*	cursor);/* in/out: page cursor */
/***************************************************************
Parses a log record of a record insert on a page. */

byte*
page_cur_parse_insert_rec(
/*======================*/
				/* out: end of log record or NULL */
	ibool		is_short,/* in: TRUE if short inserts */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	dict_index_t*	index,	/* in: record descriptor */
	page_t*		page,	/* in/out: page or NULL */
	page_zip_des_t*	page_zip,/* in/out: compressed page with at least
				25 + rec_size bytes available, or NULL */
	mtr_t*		mtr);	/* in: mtr or NULL */
/**************************************************************
Parses a log record of copying a record list end to a new created page. */

byte*
page_parse_copy_rec_list_to_created_page(
/*=====================================*/
				/* out: end of log record or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	dict_index_t*	index,	/* in: record descriptor */
	page_t*		page,	/* in/out: page or NULL */
	page_zip_des_t*	page_zip,/* in/out: compressed page or NULL */
	mtr_t*		mtr);	/* in: mtr or NULL */
/***************************************************************
Parses log record of a record delete on a page. */

byte*
page_cur_parse_delete_rec(
/*======================*/
				/* out: pointer to record end or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	dict_index_t*	index,	/* in: record descriptor */
	page_t*		page,	/* in/out: page or NULL */
	page_zip_des_t*	page_zip,/* in/out: compressed page with at least
				32 bytes available, or NULL */
	mtr_t*		mtr);	/* in: mtr or NULL */

/* Index page cursor */

struct page_cur_struct{
	byte*	rec;	/* pointer to a record on page */
};

#ifndef UNIV_NONINL
#include "page0cur.ic"
#endif

#endif 
