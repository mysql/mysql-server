/*****************************************************************************

Copyright (c) 1994, 2009, Innobase Oy. All Rights Reserved.

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

/********************************************************************//**
@file include/page0cur.h
The page cursor

Created 10/4/1994 Heikki Tuuri
*************************************************************************/

#ifndef page0cur_h
#define page0cur_h

#include "univ.i"

#include "buf0types.h"
#include "page0page.h"
#include "rem0rec.h"
#include "data0data.h"
#include "mtr0mtr.h"


#define PAGE_CUR_ADAPT

/* Page cursor search modes; the values must be in this order! */

#define	PAGE_CUR_UNSUPP	0
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

#ifdef UNIV_DEBUG
/*********************************************************//**
Gets pointer to the page frame where the cursor is positioned.
@return	page */
UNIV_INLINE
page_t*
page_cur_get_page(
/*==============*/
	page_cur_t*	cur);	/*!< in: page cursor */
/*********************************************************//**
Gets pointer to the buffer block where the cursor is positioned.
@return	page */
UNIV_INLINE
buf_block_t*
page_cur_get_block(
/*===============*/
	page_cur_t*	cur);	/*!< in: page cursor */
/*********************************************************//**
Gets pointer to the page frame where the cursor is positioned.
@return	page */
UNIV_INLINE
page_zip_des_t*
page_cur_get_page_zip(
/*==================*/
	page_cur_t*	cur);	/*!< in: page cursor */
/*********************************************************//**
Gets the record where the cursor is positioned.
@return	record */
UNIV_INLINE
rec_t*
page_cur_get_rec(
/*=============*/
	page_cur_t*	cur);	/*!< in: page cursor */
#else /* UNIV_DEBUG */
# define page_cur_get_page(cur)		page_align((cur)->rec)
# define page_cur_get_block(cur)	(cur)->block
# define page_cur_get_page_zip(cur)	buf_block_get_page_zip((cur)->block)
# define page_cur_get_rec(cur)		(cur)->rec
#endif /* UNIV_DEBUG */
/*********************************************************//**
Sets the cursor object to point before the first user record
on the page. */
UNIV_INLINE
void
page_cur_set_before_first(
/*======================*/
	const buf_block_t*	block,	/*!< in: index page */
	page_cur_t*		cur);	/*!< in: cursor */
/*********************************************************//**
Sets the cursor object to point after the last user record on
the page. */
UNIV_INLINE
void
page_cur_set_after_last(
/*====================*/
	const buf_block_t*	block,	/*!< in: index page */
	page_cur_t*		cur);	/*!< in: cursor */
/*********************************************************//**
Returns TRUE if the cursor is before first user record on page.
@return	TRUE if at start */
UNIV_INLINE
ibool
page_cur_is_before_first(
/*=====================*/
	const page_cur_t*	cur);	/*!< in: cursor */
/*********************************************************//**
Returns TRUE if the cursor is after last user record.
@return	TRUE if at end */
UNIV_INLINE
ibool
page_cur_is_after_last(
/*===================*/
	const page_cur_t*	cur);	/*!< in: cursor */
/**********************************************************//**
Positions the cursor on the given record. */
UNIV_INLINE
void
page_cur_position(
/*==============*/
	const rec_t*		rec,	/*!< in: record on a page */
	const buf_block_t*	block,	/*!< in: buffer block containing
					the record */
	page_cur_t*		cur);	/*!< out: page cursor */
/**********************************************************//**
Invalidates a page cursor by setting the record pointer NULL. */
UNIV_INLINE
void
page_cur_invalidate(
/*================*/
	page_cur_t*	cur);	/*!< out: page cursor */
/**********************************************************//**
Moves the cursor to the next record on page. */
UNIV_INLINE
void
page_cur_move_to_next(
/*==================*/
	page_cur_t*	cur);	/*!< in/out: cursor; must not be after last */
/**********************************************************//**
Moves the cursor to the previous record on page. */
UNIV_INLINE
void
page_cur_move_to_prev(
/*==================*/
	page_cur_t*	cur);	/*!< in/out: cursor; not before first */
#ifndef UNIV_HOTBACKUP
/***********************************************************//**
Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The cursor stays at
the same logical position, but the physical position may change if it is
pointing to a compressed page that was reorganized.
@return	pointer to record if succeed, NULL otherwise */
UNIV_INLINE
rec_t*
page_cur_tuple_insert(
/*==================*/
	page_cur_t*	cursor,	/*!< in/out: a page cursor */
	const dtuple_t*	tuple,	/*!< in: pointer to a data tuple */
	dict_index_t*	index,	/*!< in: record descriptor */
	ulint		n_ext,	/*!< in: number of externally stored columns */
	mtr_t*		mtr);	/*!< in: mini-transaction handle, or NULL */
#endif /* !UNIV_HOTBACKUP */
/***********************************************************//**
Inserts a record next to page cursor. Returns pointer to inserted record if
succeed, i.e., enough space available, NULL otherwise. The cursor stays at
the same logical position, but the physical position may change if it is
pointing to a compressed page that was reorganized.
@return	pointer to record if succeed, NULL otherwise */
UNIV_INLINE
rec_t*
page_cur_rec_insert(
/*================*/
	page_cur_t*	cursor,	/*!< in/out: a page cursor */
	const rec_t*	rec,	/*!< in: record to insert */
	dict_index_t*	index,	/*!< in: record descriptor */
	ulint*		offsets,/*!< in/out: rec_get_offsets(rec, index) */
	mtr_t*		mtr);	/*!< in: mini-transaction handle, or NULL */
/***********************************************************//**
Inserts a record next to page cursor on an uncompressed page.
Returns pointer to inserted record if succeed, i.e., enough
space available, NULL otherwise. The cursor stays at the same position.
@return	pointer to record if succeed, NULL otherwise */
UNIV_INTERN
rec_t*
page_cur_insert_rec_low(
/*====================*/
	rec_t*		current_rec,/*!< in: pointer to current record after
				which the new record is inserted */
	dict_index_t*	index,	/*!< in: record descriptor */
	const rec_t*	rec,	/*!< in: pointer to a physical record */
	ulint*		offsets,/*!< in/out: rec_get_offsets(rec, index) */
	mtr_t*		mtr);	/*!< in: mini-transaction handle, or NULL */
/***********************************************************//**
Inserts a record next to page cursor on a compressed and uncompressed
page. Returns pointer to inserted record if succeed, i.e.,
enough space available, NULL otherwise.
The cursor stays at the same position.
@return	pointer to record if succeed, NULL otherwise */
UNIV_INTERN
rec_t*
page_cur_insert_rec_zip(
/*====================*/
	rec_t**		current_rec,/*!< in/out: pointer to current record after
				which the new record is inserted */
	buf_block_t*	block,	/*!< in: buffer block of *current_rec */
	dict_index_t*	index,	/*!< in: record descriptor */
	const rec_t*	rec,	/*!< in: pointer to a physical record */
	ulint*		offsets,/*!< in/out: rec_get_offsets(rec, index) */
	mtr_t*		mtr);	/*!< in: mini-transaction handle, or NULL */
/*************************************************************//**
Copies records from page to a newly created page, from a given record onward,
including that record. Infimum and supremum records are not copied. */
UNIV_INTERN
void
page_copy_rec_list_end_to_created_page(
/*===================================*/
	page_t*		new_page,	/*!< in/out: index page to copy to */
	rec_t*		rec,		/*!< in: first record to copy */
	dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*		mtr);		/*!< in: mtr */
/***********************************************************//**
Deletes a record at the page cursor. The cursor is moved to the
next record after the deleted one. */
UNIV_INTERN
void
page_cur_delete_rec(
/*================*/
	page_cur_t*	cursor,	/*!< in/out: a page cursor */
	dict_index_t*	index,	/*!< in: record descriptor */
	const ulint*	offsets,/*!< in: rec_get_offsets(cursor->rec, index) */
	mtr_t*		mtr);	/*!< in: mini-transaction handle */
#ifndef UNIV_HOTBACKUP
/****************************************************************//**
Searches the right position for a page cursor.
@return	number of matched fields on the left */
UNIV_INLINE
ulint
page_cur_search(
/*============*/
	const buf_block_t*	block,	/*!< in: buffer block */
	const dict_index_t*	index,	/*!< in: record descriptor */
	const dtuple_t*		tuple,	/*!< in: data tuple */
	ulint			mode,	/*!< in: PAGE_CUR_L,
					PAGE_CUR_LE, PAGE_CUR_G, or
					PAGE_CUR_GE */
	page_cur_t*		cursor);/*!< out: page cursor */
/****************************************************************//**
Searches the right position for a page cursor. */
UNIV_INTERN
void
page_cur_search_with_match(
/*=======================*/
	const buf_block_t*	block,	/*!< in: buffer block */
	const dict_index_t*	index,	/*!< in: record descriptor */
	const dtuple_t*		tuple,	/*!< in: data tuple */
	ulint			mode,	/*!< in: PAGE_CUR_L,
					PAGE_CUR_LE, PAGE_CUR_G, or
					PAGE_CUR_GE */
	ulint*			iup_matched_fields,
					/*!< in/out: already matched
					fields in upper limit record */
	ulint*			iup_matched_bytes,
					/*!< in/out: already matched
					bytes in a field not yet
					completely matched */
	ulint*			ilow_matched_fields,
					/*!< in/out: already matched
					fields in lower limit record */
	ulint*			ilow_matched_bytes,
					/*!< in/out: already matched
					bytes in a field not yet
					completely matched */
	page_cur_t*		cursor);/*!< out: page cursor */
/***********************************************************//**
Positions a page cursor on a randomly chosen user record on a page. If there
are no user records, sets the cursor on the infimum record. */
UNIV_INTERN
void
page_cur_open_on_rnd_user_rec(
/*==========================*/
	buf_block_t*	block,	/*!< in: page */
	page_cur_t*	cursor);/*!< out: page cursor */
#endif /* !UNIV_HOTBACKUP */
/***********************************************************//**
Parses a log record of a record insert on a page.
@return	end of log record or NULL */
UNIV_INTERN
byte*
page_cur_parse_insert_rec(
/*======================*/
	ibool		is_short,/*!< in: TRUE if short inserts */
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	buf_block_t*	block,	/*!< in: page or NULL */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr);	/*!< in: mtr or NULL */
/**********************************************************//**
Parses a log record of copying a record list end to a new created page.
@return	end of log record or NULL */
UNIV_INTERN
byte*
page_parse_copy_rec_list_to_created_page(
/*=====================================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	buf_block_t*	block,	/*!< in: page or NULL */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr);	/*!< in: mtr or NULL */
/***********************************************************//**
Parses log record of a record delete on a page.
@return	pointer to record end or NULL */
UNIV_INTERN
byte*
page_cur_parse_delete_rec(
/*======================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	buf_block_t*	block,	/*!< in: page or NULL */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr);	/*!< in: mtr or NULL */

/** Index page cursor */

struct page_cur_struct{
	byte*		rec;	/*!< pointer to a record on page */
	buf_block_t*	block;	/*!< pointer to the block containing rec */
};

#ifndef UNIV_NONINL
#include "page0cur.ic"
#endif

#endif
