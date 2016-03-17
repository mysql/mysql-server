/*****************************************************************************

Copyright (c) 1996, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/btr0pcur.h
The index tree persistent cursor

Created 2/23/1996 Heikki Tuuri
*******************************************************/

#ifndef btr0pcur_h
#define btr0pcur_h

#include "univ.i"
#include "dict0dict.h"
#include "data0data.h"
#include "mtr0mtr.h"
#include "page0cur.h"
#include "btr0cur.h"
#include "btr0btr.h"
#include "btr0types.h"
#include "gis0rtree.h"

/* Relative positions for a stored cursor position */
enum btr_pcur_pos_t {
	BTR_PCUR_ON		= 1,
	BTR_PCUR_BEFORE		= 2,
	BTR_PCUR_AFTER		= 3,
/* Note that if the tree is not empty, btr_pcur_store_position does not
use the following, but only uses the above three alternatives, where the
position is stored relative to a specific record: this makes implementation
of a scroll cursor easier */
	BTR_PCUR_BEFORE_FIRST_IN_TREE	= 4,	/* in an empty tree */
	BTR_PCUR_AFTER_LAST_IN_TREE	= 5	/* in an empty tree */
};

/**************************************************************//**
Allocates memory for a persistent cursor object and initializes the cursor.
@return own: persistent cursor */
btr_pcur_t*
btr_pcur_create_for_mysql(void);
/*============================*/

/**************************************************************//**
Resets a persistent cursor object, freeing ::old_rec_buf if it is
allocated and resetting the other members to their initial values. */
void
btr_pcur_reset(
/*===========*/
	btr_pcur_t*	cursor);/*!< in, out: persistent cursor */

/**************************************************************//**
Frees the memory for a persistent cursor object. */
void
btr_pcur_free_for_mysql(
/*====================*/
	btr_pcur_t*	cursor);	/*!< in, own: persistent cursor */
/**************************************************************//**
Copies the stored position of a pcur to another pcur. */
void
btr_pcur_copy_stored_position(
/*==========================*/
	btr_pcur_t*	pcur_receive,	/*!< in: pcur which will receive the
					position info */
	btr_pcur_t*	pcur_donate);	/*!< in: pcur from which the info is
					copied */
/**************************************************************//**
Sets the old_rec_buf field to NULL. */
UNIV_INLINE
void
btr_pcur_init(
/*==========*/
	btr_pcur_t*	pcur);	/*!< in: persistent cursor */

/** Free old_rec_buf.
@param[in]	pcur	Persistent cursor holding old_rec to be freed. */
UNIV_INLINE
void
btr_pcur_free(
	btr_pcur_t*	pcur);

/**************************************************************//**
Initializes and opens a persistent cursor to an index tree. It should be
closed with btr_pcur_close. */
UNIV_INLINE
void
btr_pcur_open_low(
/*==============*/
	dict_index_t*	index,	/*!< in: index */
	ulint		level,	/*!< in: level in the btree */
	const dtuple_t*	tuple,	/*!< in: tuple on which search done */
	page_cur_mode_t	mode,	/*!< in: PAGE_CUR_L, ...;
				NOTE that if the search is made using a unique
				prefix of a record, mode should be
				PAGE_CUR_LE, not PAGE_CUR_GE, as the latter
				may end up on the previous page from the
				record! */
	ulint		latch_mode,/*!< in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor, /*!< in: memory buffer for persistent cursor */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr);	/*!< in: mtr */
#define btr_pcur_open(i,t,md,l,c,m)				\
	btr_pcur_open_low(i,0,t,md,l,c,__FILE__,__LINE__,m)
/**************************************************************//**
Opens an persistent cursor to an index tree without initializing the
cursor. */
UNIV_INLINE
void
btr_pcur_open_with_no_init_func(
/*============================*/
	dict_index_t*	index,	/*!< in: index */
	const dtuple_t*	tuple,	/*!< in: tuple on which search done */
	page_cur_mode_t	mode,	/*!< in: PAGE_CUR_L, ...;
				NOTE that if the search is made using a unique
				prefix of a record, mode should be
				PAGE_CUR_LE, not PAGE_CUR_GE, as the latter
				may end up on the previous page of the
				record! */
	ulint		latch_mode,/*!< in: BTR_SEARCH_LEAF, ...;
				NOTE that if has_search_latch != 0 then
				we maybe do not acquire a latch on the cursor
				page, but assume that the caller uses his
				btr search latch to protect the record! */
	btr_pcur_t*	cursor, /*!< in: memory buffer for persistent cursor */
	ulint		has_search_latch,
				/*!< in: latch mode the caller
				currently has on search system:
				RW_S_LATCH, or 0 */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr);	/*!< in: mtr */
#define btr_pcur_open_with_no_init(ix,t,md,l,cur,has,m)			\
	btr_pcur_open_with_no_init_func(ix,t,md,l,cur,has,__FILE__,__LINE__,m)

/*****************************************************************//**
Opens a persistent cursor at either end of an index. */
UNIV_INLINE
void
btr_pcur_open_at_index_side(
/*========================*/
	bool		from_left,	/*!< in: true if open to the low end,
					false if to the high end */
	dict_index_t*	index,		/*!< in: index */
	ulint		latch_mode,	/*!< in: latch mode */
	btr_pcur_t*	pcur,		/*!< in/out: cursor */
	bool		init_pcur,	/*!< in: whether to initialize pcur */
	ulint		level,		/*!< in: level to search for
					(0=leaf) */
	mtr_t*		mtr)		/*!< in/out: mini-transaction */
	MY_ATTRIBUTE((nonnull));
/**************************************************************//**
Gets the up_match value for a pcur after a search.
@return number of matched fields at the cursor or to the right if
search mode was PAGE_CUR_GE, otherwise undefined */
UNIV_INLINE
ulint
btr_pcur_get_up_match(
/*==================*/
	const btr_pcur_t*	cursor); /*!< in: persistent cursor */
/**************************************************************//**
Gets the low_match value for a pcur after a search.
@return number of matched fields at the cursor or to the right if
search mode was PAGE_CUR_LE, otherwise undefined */
UNIV_INLINE
ulint
btr_pcur_get_low_match(
/*===================*/
	const btr_pcur_t*	cursor); /*!< in: persistent cursor */
/**************************************************************//**
If mode is PAGE_CUR_G or PAGE_CUR_GE, opens a persistent cursor on the first
user record satisfying the search condition, in the case PAGE_CUR_L or
PAGE_CUR_LE, on the last user record. If no such user record exists, then
in the first case sets the cursor after last in tree, and in the latter case
before first in tree. The latching mode must be BTR_SEARCH_LEAF or
BTR_MODIFY_LEAF. */
void
btr_pcur_open_on_user_rec_func(
/*===========================*/
	dict_index_t*	index,		/*!< in: index */
	const dtuple_t*	tuple,		/*!< in: tuple on which search done */
	page_cur_mode_t	mode,		/*!< in: PAGE_CUR_L, ... */
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF or
					BTR_MODIFY_LEAF */
	btr_pcur_t*	cursor,		/*!< in: memory buffer for persistent
					cursor */
	const char*	file,		/*!< in: file name */
	ulint		line,		/*!< in: line where called */
	mtr_t*		mtr);		/*!< in: mtr */
#define btr_pcur_open_on_user_rec(i,t,md,l,c,m)				\
	btr_pcur_open_on_user_rec_func(i,t,md,l,c,__FILE__,__LINE__,m)
/**********************************************************************//**
Positions a cursor at a randomly chosen position within a B-tree.
@return true if the index is available and we have put the cursor, false
if the index is unavailable */
UNIV_INLINE
bool
btr_pcur_open_at_rnd_pos_func(
/*==========================*/
	dict_index_t*	index,		/*!< in: index */
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor,		/*!< in/out: B-tree pcur */
	const char*	file,		/*!< in: file name */
	ulint		line,		/*!< in: line where called */
	mtr_t*		mtr);		/*!< in: mtr */
#define btr_pcur_open_at_rnd_pos(i,l,c,m)				\
	btr_pcur_open_at_rnd_pos_func(i,l,c,__FILE__,__LINE__,m)
/**************************************************************//**
Frees the possible memory heap of a persistent cursor and sets the latch
mode of the persistent cursor to BTR_NO_LATCHES.
WARNING: this function does not release the latch on the page where the
cursor is currently positioned. The latch is acquired by the
"move to next/previous" family of functions. Since recursive shared locks
are not allowed, you must take care (if using the cursor in S-mode) to
manually release the latch by either calling
btr_leaf_page_release(btr_pcur_get_block(&pcur), pcur.latch_mode, mtr)
or by committing the mini-transaction right after btr_pcur_close().
A subsequent attempt to crawl the same page in the same mtr would cause
an assertion failure. */
UNIV_INLINE
void
btr_pcur_close(
/*===========*/
	btr_pcur_t*	cursor);	/*!< in: persistent cursor */
/**************************************************************//**
The position of the cursor is stored by taking an initial segment of the
record the cursor is positioned on, before, or after, and copying it to the
cursor data structure, or just setting a flag if the cursor id before the
first in an EMPTY tree, or after the last in an EMPTY tree. NOTE that the
page where the cursor is positioned must not be empty if the index tree is
not totally empty! */
void
btr_pcur_store_position(
/*====================*/
	btr_pcur_t*	cursor, /*!< in: persistent cursor */
	mtr_t*		mtr);	/*!< in: mtr */
/**************************************************************//**
Restores the stored position of a persistent cursor bufferfixing the page and
obtaining the specified latches. If the cursor position was saved when the
(1) cursor was positioned on a user record: this function restores the position
to the last record LESS OR EQUAL to the stored record;
(2) cursor was positioned on a page infimum record: restores the position to
the last record LESS than the user record which was the successor of the page
infimum;
(3) cursor was positioned on the page supremum: restores to the first record
GREATER than the user record which was the predecessor of the supremum.
(4) cursor was positioned before the first or after the last in an empty tree:
restores to before first or after the last in the tree.
@return TRUE if the cursor position was stored when it was on a user
record and it can be restored on a user record whose ordering fields
are identical to the ones of the original user record */
ibool
btr_pcur_restore_position_func(
/*===========================*/
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor,		/*!< in: detached persistent cursor */
	const char*	file,		/*!< in: file name */
	ulint		line,		/*!< in: line where called */
	mtr_t*		mtr);		/*!< in: mtr */
#define btr_pcur_restore_position(l,cur,mtr)				\
	btr_pcur_restore_position_func(l,cur,__FILE__,__LINE__,mtr)
/*********************************************************//**
Gets the rel_pos field for a cursor whose position has been stored.
@return BTR_PCUR_ON, ... */
UNIV_INLINE
ulint
btr_pcur_get_rel_pos(
/*=================*/
	const btr_pcur_t*	cursor);/*!< in: persistent cursor */
/**************************************************************//**
Commits the mtr and sets the pcur latch mode to BTR_NO_LATCHES,
that is, the cursor becomes detached.
Function btr_pcur_store_position should be used before calling this,
if restoration of cursor is wanted later. */
UNIV_INLINE
void
btr_pcur_commit_specify_mtr(
/*========================*/
	btr_pcur_t*	pcur,	/*!< in: persistent cursor */
	mtr_t*		mtr);	/*!< in: mtr to commit */
/*********************************************************//**
Moves the persistent cursor to the next record in the tree. If no records are
left, the cursor stays 'after last in tree'.
@return TRUE if the cursor was not after last in tree */
UNIV_INLINE
ibool
btr_pcur_move_to_next(
/*==================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor; NOTE that the
				function may release the page latch */
	mtr_t*		mtr);	/*!< in: mtr */
/*********************************************************//**
Moves the persistent cursor to the previous record in the tree. If no records
are left, the cursor stays 'before first in tree'.
@return TRUE if the cursor was not before first in tree */
ibool
btr_pcur_move_to_prev(
/*==================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor; NOTE that the
				function may release the page latch */
	mtr_t*		mtr);	/*!< in: mtr */
/*********************************************************//**
Moves the persistent cursor to the last record on the same page. */
UNIV_INLINE
void
btr_pcur_move_to_last_on_page(
/*==========================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor */
	mtr_t*		mtr);	/*!< in: mtr */
/*********************************************************//**
Moves the persistent cursor to the next user record in the tree. If no user
records are left, the cursor ends up 'after last in tree'.
@return TRUE if the cursor moved forward, ending on a user record */
UNIV_INLINE
ibool
btr_pcur_move_to_next_user_rec(
/*===========================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor; NOTE that the
				function may release the page latch */
	mtr_t*		mtr);	/*!< in: mtr */
/*********************************************************//**
Moves the persistent cursor to the first record on the next page.
Releases the latch on the current page, and bufferunfixes it.
Note that there must not be modifications on the current page,
as then the x-latch can be released only in mtr_commit. */
void
btr_pcur_move_to_next_page(
/*=======================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor; must be on the
				last record of the current page */
	mtr_t*		mtr);	/*!< in: mtr */
/*********************************************************//**
Moves the persistent cursor backward if it is on the first record
of the page. Releases the latch on the current page, and bufferunfixes
it. Note that to prevent a possible deadlock, the operation first
stores the position of the cursor, releases the leaf latch, acquires
necessary latches and restores the cursor position again before returning.
The alphabetical position of the cursor is guaranteed to be sensible
on return, but it may happen that the cursor is not positioned on the
last record of any page, because the structure of the tree may have
changed while the cursor had no latches. */
void
btr_pcur_move_backward_from_page(
/*=============================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor, must be on the
				first record of the current page */
	mtr_t*		mtr);	/*!< in: mtr */
#ifdef UNIV_DEBUG
/*********************************************************//**
Returns the btr cursor component of a persistent cursor.
@return pointer to btr cursor component */
UNIV_INLINE
btr_cur_t*
btr_pcur_get_btr_cur(
/*=================*/
	const btr_pcur_t*	cursor);	/*!< in: persistent cursor */
/*********************************************************//**
Returns the page cursor component of a persistent cursor.
@return pointer to page cursor component */
UNIV_INLINE
page_cur_t*
btr_pcur_get_page_cur(
/*==================*/
	const btr_pcur_t*	cursor);	/*!< in: persistent cursor */
/*********************************************************//**
Returns the page of a persistent cursor.
@return pointer to the page */
UNIV_INLINE
page_t*
btr_pcur_get_page(
/*==============*/
	const btr_pcur_t*	cursor);/*!< in: persistent cursor */
/*********************************************************//**
Returns the buffer block of a persistent cursor.
@return pointer to the block */
UNIV_INLINE
buf_block_t*
btr_pcur_get_block(
/*===============*/
	const btr_pcur_t*	cursor);/*!< in: persistent cursor */
/*********************************************************//**
Returns the record of a persistent cursor.
@return pointer to the record */
UNIV_INLINE
rec_t*
btr_pcur_get_rec(
/*=============*/
	const btr_pcur_t*	cursor);/*!< in: persistent cursor */
#else /* UNIV_DEBUG */
# define btr_pcur_get_btr_cur(cursor) (&(cursor)->btr_cur)
# define btr_pcur_get_page_cur(cursor) (&(cursor)->btr_cur.page_cur)
# define btr_pcur_get_page(cursor) ((cursor)->btr_cur.page_cur.block->frame)
# define btr_pcur_get_block(cursor) ((cursor)->btr_cur.page_cur.block)
# define btr_pcur_get_rec(cursor) ((cursor)->btr_cur.page_cur.rec)
#endif /* UNIV_DEBUG */
/*********************************************************//**
Checks if the persistent cursor is on a user record. */
UNIV_INLINE
ibool
btr_pcur_is_on_user_rec(
/*====================*/
	const btr_pcur_t*	cursor);/*!< in: persistent cursor */
/*********************************************************//**
Checks if the persistent cursor is after the last user record on
a page. */
UNIV_INLINE
ibool
btr_pcur_is_after_last_on_page(
/*===========================*/
	const btr_pcur_t*	cursor);/*!< in: persistent cursor */
/*********************************************************//**
Checks if the persistent cursor is before the first user record on
a page. */
UNIV_INLINE
ibool
btr_pcur_is_before_first_on_page(
/*=============================*/
	const btr_pcur_t*	cursor);/*!< in: persistent cursor */
/*********************************************************//**
Checks if the persistent cursor is before the first user record in
the index tree. */
UNIV_INLINE
ibool
btr_pcur_is_before_first_in_tree(
/*=============================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor */
	mtr_t*		mtr);	/*!< in: mtr */
/*********************************************************//**
Checks if the persistent cursor is after the last user record in
the index tree. */
UNIV_INLINE
ibool
btr_pcur_is_after_last_in_tree(
/*===========================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor */
	mtr_t*		mtr);	/*!< in: mtr */
/*********************************************************//**
Moves the persistent cursor to the next record on the same page. */
UNIV_INLINE
void
btr_pcur_move_to_next_on_page(
/*==========================*/
	btr_pcur_t*	cursor);/*!< in/out: persistent cursor */
/*********************************************************//**
Moves the persistent cursor to the previous record on the same page. */
UNIV_INLINE
void
btr_pcur_move_to_prev_on_page(
/*==========================*/
	btr_pcur_t*	cursor);/*!< in/out: persistent cursor */
/*********************************************************//**
Moves the persistent cursor to the infimum record on the same page. */
UNIV_INLINE
void
btr_pcur_move_before_first_on_page(
/*===============================*/
	btr_pcur_t*	cursor); /*!< in/out: persistent cursor */

/** Position state of persistent B-tree cursor. */
enum pcur_pos_t {
	/** The persistent cursor is not positioned. */
	BTR_PCUR_NOT_POSITIONED = 0,
	/** The persistent cursor was previously positioned.
	TODO: currently, the state can be BTR_PCUR_IS_POSITIONED,
	though it really should be BTR_PCUR_WAS_POSITIONED,
	because we have no obligation to commit the cursor with
	mtr; similarly latch_mode may be out of date. This can
	lead to problems if btr_pcur is not used the right way;
	all current code should be ok. */
	BTR_PCUR_WAS_POSITIONED,
	/** The persistent cursor is positioned by optimistic get to the same
	record as it was positioned at. Not used for rel_pos == BTR_PCUR_ON.
	It may need adjustment depending on previous/current search direction
	and rel_pos. */
	BTR_PCUR_IS_POSITIONED_OPTIMISTIC,
	/** The persistent cursor is positioned by index search.
	Or optimistic get for rel_pos == BTR_PCUR_ON. */
	BTR_PCUR_IS_POSITIONED
};

/* The persistent B-tree cursor structure. This is used mainly for SQL
selects, updates, and deletes. */

struct btr_pcur_t{
	/** a B-tree cursor */
	btr_cur_t	btr_cur;
	/** see TODO note below!
	BTR_SEARCH_LEAF, BTR_MODIFY_LEAF, BTR_MODIFY_TREE or BTR_NO_LATCHES,
	depending on the latching state of the page and tree where the cursor
	is positioned; BTR_NO_LATCHES means that the cursor is not currently
	positioned:
	we say then that the cursor is detached; it can be restored to
	attached if the old position was stored in old_rec */
	ulint		latch_mode;
	/** true if old_rec is stored */
	bool		old_stored;
	/** if cursor position is stored, contains an initial segment of the
	latest record cursor was positioned either on, before or after */
	rec_t*		old_rec;
	/** number of fields in old_rec */
	ulint		old_n_fields;
	/** BTR_PCUR_ON, BTR_PCUR_BEFORE, or BTR_PCUR_AFTER, depending on
	whether cursor was on, before, or after the old_rec record */
	enum btr_pcur_pos_t	rel_pos;
	/** buffer block when the position was stored */
	buf_block_t*	block_when_stored;
	/** the modify clock value of the buffer block when the cursor position
	was stored */
	ib_uint64_t	modify_clock;
	/** the withdraw clock value of the buffer pool when the cursor
	position was stored */
	ulint		withdraw_clock;
	/** btr_pcur_store_position() and btr_pcur_restore_position() state. */
	enum pcur_pos_t	pos_state;
	/** PAGE_CUR_G, ... */
	page_cur_mode_t	search_mode;
	/** the transaction, if we know it; otherwise this field is not defined;
	can ONLY BE USED in error prints in fatal assertion failures! */
	trx_t*		trx_if_known;
	/*-----------------------------*/
	/* NOTE that the following fields may possess dynamically allocated
	memory which should be freed if not needed anymore! */

	/** NULL, or a dynamically allocated buffer for old_rec */
	byte*		old_rec_buf;
	/** old_rec_buf size if old_rec_buf is not NULL */
	ulint		buf_size;

	/** Return the index of this persistent cursor */
	dict_index_t*	index() const { return(btr_cur.index); }
};

#ifndef UNIV_NONINL
#include "btr0pcur.ic"
#endif

#endif
