/******************************************************
The index tree persistent cursor

(c) 1996 Innobase Oy

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

/* Relative positions for a stored cursor position */
#define BTR_PCUR_ON	1
#define BTR_PCUR_BEFORE	2
#define BTR_PCUR_AFTER	3

/******************************************************************
Allocates memory for a persistent cursor object and initializes the cursor. */

btr_pcur_t*
btr_pcur_create_for_mysql(void);
/*============================*/
				/* out, own: persistent cursor */
/******************************************************************
Frees the memory for a persistent cursor object. */

void
btr_pcur_free_for_mysql(
/*====================*/
	btr_pcur_t*	cursor);	/* in, own: persistent cursor */
/******************************************************************
Copies the stored position of a pcur to another pcur. */

void
btr_pcur_copy_stored_position(
/*==========================*/
	btr_pcur_t*	pcur_receive,	/* in: pcur which will receive the
					position info */
	btr_pcur_t*	pcur_donate);	/* in: pcur from which the info is
					copied */
/******************************************************************
Sets the old_rec_buf field to NULL. */
UNIV_INLINE
void
btr_pcur_init(
/*==========*/
	btr_pcur_t*	pcur);	/* in: persistent cursor */
/******************************************************************
Initializes and opens a persistent cursor to an index tree. It should be
closed with btr_pcur_close. */
UNIV_INLINE
void
btr_pcur_open(
/*==========*/
	dict_index_t*	index,	/* in: index */
	dtuple_t*	tuple,	/* in: tuple on which search done */
	ulint		mode,	/* in: PAGE_CUR_L, ...;
				NOTE that if the search is made using a unique
				prefix of a record, mode should be
				PAGE_CUR_LE, not PAGE_CUR_GE, as the latter
				may end up on the previous page from the
				record! */
	ulint		latch_mode,/* in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor, /* in: memory buffer for persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/******************************************************************
Opens an persistent cursor to an index tree without initializing the
cursor. */
UNIV_INLINE
void
btr_pcur_open_with_no_init(
/*=======================*/
	dict_index_t*	index,	/* in: index */
	dtuple_t*	tuple,	/* in: tuple on which search done */
	ulint		mode,	/* in: PAGE_CUR_L, ...;
				NOTE that if the search is made using a unique
				prefix of a record, mode should be
				PAGE_CUR_LE, not PAGE_CUR_GE, as the latter
				may end up on the previous page of the
				record! */
	ulint		latch_mode,/* in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor, /* in: memory buffer for persistent cursor */
	ulint		has_search_latch,/* in: latch mode the caller
				currently has on btr_search_latch:
				RW_S_LATCH, or 0 */
	mtr_t*		mtr);	/* in: mtr */
/*********************************************************************
Opens a persistent cursor at either end of an index. */
UNIV_INLINE
void
btr_pcur_open_at_index_side(
/*========================*/
	ibool		from_left,	/* in: TRUE if open to the low end,
					FALSE if to the high end */
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: latch mode */
	btr_pcur_t*	pcur,		/* in: cursor */
	ibool		do_init,	/* in: TRUE if should be initialized */
	mtr_t*		mtr);		/* in: mtr */
/******************************************************************
Gets the up_match value for a pcur after a search. */
UNIV_INLINE
ulint
btr_pcur_get_up_match(
/*==================*/
				/* out: number of matched fields at the cursor
				or to the right if search mode was PAGE_CUR_GE,
				otherwise undefined */
	btr_pcur_t*	cursor); /* in: memory buffer for persistent cursor */
/******************************************************************
Gets the low_match value for a pcur after a search. */
UNIV_INLINE
ulint
btr_pcur_get_low_match(
/*===================*/
				/* out: number of matched fields at the cursor
				or to the right if search mode was PAGE_CUR_LE,
				otherwise undefined */
	btr_pcur_t*	cursor); /* in: memory buffer for persistent cursor */
/******************************************************************
If mode is PAGE_CUR_G or PAGE_CUR_GE, opens a persistent cursor on the first
user record satisfying the search condition, in the case PAGE_CUR_L or
PAGE_CUR_LE, on the last user record. If no such user record exists, then
in the first case sets the cursor after last in tree, and in the latter case
before first in tree. The latching mode must be BTR_SEARCH_LEAF or
BTR_MODIFY_LEAF. */

void
btr_pcur_open_on_user_rec(
/*======================*/
	dict_index_t*	index,		/* in: index */
	dtuple_t*	tuple,		/* in: tuple on which search done */
	ulint		mode,		/* in: PAGE_CUR_L, ... */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF or
					BTR_MODIFY_LEAF */
	btr_pcur_t*	cursor, 	/* in: memory buffer for persistent
					cursor */
	mtr_t*		mtr);		/* in: mtr */
/**************************************************************************
Positions a cursor at a randomly chosen position within a B-tree. */
UNIV_INLINE
void
btr_pcur_open_at_rnd_pos(
/*=====================*/
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor,		/* in/out: B-tree pcur */
	mtr_t*		mtr);		/* in: mtr */
/******************************************************************
Frees the possible old_rec_buf buffer of a persistent cursor and sets the
latch mode of the persistent cursor to BTR_NO_LATCHES. */
UNIV_INLINE
void
btr_pcur_close(
/*===========*/
	btr_pcur_t*	cursor);	/* in: persistent cursor */
/******************************************************************
The position of the cursor is stored by taking an initial segment of the
record the cursor is positioned on, before, or after, and copying it to the
cursor data structure. NOTE that the page where the cursor is positioned
must not be empty! */

void
btr_pcur_store_position(
/*====================*/
	btr_pcur_t*	cursor, 	/* in: persistent cursor */
	mtr_t*		mtr);		/* in: mtr */
/******************************************************************
If the latch mode of the cursor is BTR_LEAF_SEARCH or BTR_LEAF_MODIFY,
releases the page latch and bufferfix reserved by the cursor.
NOTE! In the case of BTR_LEAF_MODIFY, there should not exist changes
made by the current mini-transaction to the data protected by the
cursor latch, as then the latch must not be released until mtr_commit. */

void
btr_pcur_release_leaf(
/*==================*/
	btr_pcur_t*	cursor, /* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Gets the rel_pos field for a cursor whose position has been stored. */
UNIV_INLINE
ulint
btr_pcur_get_rel_pos(
/*=================*/
				/* out: BTR_PCUR_ON, ... */
	btr_pcur_t*	cursor);/* in: persistent cursor */
/******************************************************************
Restores the stored position of a persistent cursor bufferfixing the page and
obtaining the specified latches. If the cursor position was saved when the
(1) cursor was positioned on a user record: this function restores the position
to the last record LESS OR EQUAL to the stored record;
(2) cursor was positioned on a page infimum record: restores the position to
the last record LESS than the user record which was the successor of the page
infimum;
(3) cursor was positioned on the page supremum: restores to the first record
GREATER than the user record which was the predecessor of the supremum. */

ibool
btr_pcur_restore_position(
/*======================*/
					/* out: TRUE if the cursor position
					was stored when it was on a user record
					and it can be restored on a user record
					whose ordering fields are identical to
					the ones of the original user record */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor, 	/* in: detached persistent cursor */
	mtr_t*		mtr);		/* in: mtr */
/*************************************************************
Sets the mtr field for a pcur. */
UNIV_INLINE
void
btr_pcur_set_mtr(
/*=============*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in, own: mtr */
/*************************************************************
Gets the mtr field for a pcur. */
UNIV_INLINE
mtr_t*
btr_pcur_get_mtr(
/*=============*/
				/* out: mtr */
	btr_pcur_t*	cursor);	/* in: persistent cursor */
/******************************************************************
Commits the pcur mtr and sets the pcur latch mode to BTR_NO_LATCHES,
that is, the cursor becomes detached. If there have been modifications
to the page where pcur is positioned, this can be used instead of
btr_pcur_release_leaf. Function btr_pcur_store_position should be used
before calling this, if restoration of cursor is wanted later. */
UNIV_INLINE
void
btr_pcur_commit(
/*============*/
	btr_pcur_t*	pcur);	/* in: persistent cursor */
/******************************************************************
Differs from btr_pcur_commit in that we can specify the mtr to commit. */
UNIV_INLINE
void
btr_pcur_commit_specify_mtr(
/*========================*/
	btr_pcur_t*	pcur,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr to commit */
/******************************************************************
Tests if a cursor is detached: that is the latch mode is BTR_NO_LATCHES. */
UNIV_INLINE
ibool
btr_pcur_is_detached(
/*=================*/
				/* out: TRUE if detached */
	btr_pcur_t*	pcur);	/* in: persistent cursor */
/*************************************************************
Moves the persistent cursor to the next record in the tree. If no records are
left, the cursor stays 'after last in tree'. */
UNIV_INLINE
ibool
btr_pcur_move_to_next(
/*==================*/
				/* out: TRUE if the cursor was not after last
				in tree */
	btr_pcur_t*	cursor,	/* in: persistent cursor; NOTE that the
				function may release the page latch */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor to the previous record in the tree. If no records
are left, the cursor stays 'before first in tree'. */

ibool
btr_pcur_move_to_prev(
/*==================*/
				/* out: TRUE if the cursor was not before first
				in tree */
	btr_pcur_t*	cursor,	/* in: persistent cursor; NOTE that the
				function may release the page latch */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor to the next user record in the tree. If no user
records are left, the cursor ends up 'after last in tree'. */
UNIV_INLINE
ibool
btr_pcur_move_to_next_user_rec(
/*===========================*/
				/* out: TRUE if the cursor moved forward,
				ending on a user record */
	btr_pcur_t*	cursor,	/* in: persistent cursor; NOTE that the
				function may release the page latch */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor to the first record on the next page.
Releases the latch on the current page, and bufferunfixes it.
Note that there must not be modifications on the current page,
as then the x-latch can be released only in mtr_commit. */

void
btr_pcur_move_to_next_page(
/*=======================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor; must be on the
				last record of the current page */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
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
	btr_pcur_t*	cursor,	/* in: persistent cursor, must be on the
				first record of the current page */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Returns the btr cursor component of a persistent cursor. */
UNIV_INLINE
btr_cur_t*
btr_pcur_get_btr_cur(
/*=================*/
				/* out: pointer to btr cursor component */
	btr_pcur_t*	cursor);	/* in: persistent cursor */
/*************************************************************
Returns the page cursor component of a persistent cursor. */
UNIV_INLINE
page_cur_t*
btr_pcur_get_page_cur(
/*==================*/
				/* out: pointer to page cursor component */
	btr_pcur_t*	cursor);	/* in: persistent cursor */
/*************************************************************
Returns the page of a persistent cursor. */
UNIV_INLINE
page_t*
btr_pcur_get_page(
/*==============*/
				/* out: pointer to the page */
	btr_pcur_t*	cursor);/* in: persistent cursor */
/*************************************************************
Returns the record of a persistent cursor. */
UNIV_INLINE
rec_t*
btr_pcur_get_rec(
/*=============*/
				/* out: pointer to the record */
	btr_pcur_t*	cursor);/* in: persistent cursor */
/*************************************************************
Checks if the persistent cursor is on a user record. */
UNIV_INLINE
ibool
btr_pcur_is_on_user_rec(
/*====================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Checks if the persistent cursor is after the last user record on 
a page. */
UNIV_INLINE
ibool
btr_pcur_is_after_last_on_page(
/*===========================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Checks if the persistent cursor is before the first user record on 
a page. */
UNIV_INLINE
ibool
btr_pcur_is_before_first_on_page(
/*=============================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Checks if the persistent cursor is before the first user record in
the index tree. */
UNIV_INLINE
ibool
btr_pcur_is_before_first_in_tree(
/*=============================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Checks if the persistent cursor is after the last user record in
the index tree. */
UNIV_INLINE
ibool
btr_pcur_is_after_last_in_tree(
/*===========================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor to the next record on the same page. */
UNIV_INLINE
void
btr_pcur_move_to_next_on_page(
/*==========================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */
/*************************************************************
Moves the persistent cursor to the previous record on the same page. */
UNIV_INLINE
void
btr_pcur_move_to_prev_on_page(
/*==========================*/
	btr_pcur_t*	cursor,	/* in: persistent cursor */
	mtr_t*		mtr);	/* in: mtr */


/* The persistent B-tree cursor structure. This is used mainly for SQL
selects, updates, and deletes. */

struct btr_pcur_struct{
	btr_cur_t	btr_cur;	/* a B-tree cursor */
	ulint		latch_mode;	/* see FIXME note below!
					BTR_SEARCH_LEAF, BTR_MODIFY_LEAF,
					BTR_MODIFY_TREE, or BTR_NO_LATCHES,
					depending on the latching state of
					the page and tree where the cursor is
					positioned; the last value means that
					the cursor is not currently positioned:
					we say then that the cursor is
					detached; it can be restored to
					attached if the old position was
					stored in old_rec */
	ulint		old_stored;	/* BTR_PCUR_OLD_STORED
					or BTR_PCUR_OLD_NOT_STORED */
	rec_t*		old_rec;	/* if cursor position is stored,
					contains an initial segment of the
					latest record cursor was positioned
					either on, before, or after */
	ulint		rel_pos;	/* BTR_PCUR_ON, BTR_PCUR_BEFORE, or
					BTR_PCUR_AFTER, depending on whether
					cursor was on, before, or after the
					old_rec record */
	dulint		modify_clock;	/* the modify clock value of the
					buffer block when the cursor position
					was stored */
	ulint		pos_state;	/* see FIXME note below!
					BTR_PCUR_IS_POSITIONED,
					BTR_PCUR_WAS_POSITIONED,
					BTR_PCUR_NOT_POSITIONED */
	ulint		search_mode;	/* PAGE_CUR_G, ... */
	/*-----------------------------*/
	/* NOTE that the following fields may possess dynamically allocated
	memory, which should be freed if not needed anymore! */

	mtr_t*		mtr;		/* NULL, or this field may contain
					a mini-transaction which holds the
					latch on the cursor page */
	byte*		old_rec_buf;	/* NULL, or a dynamically allocated
					buffer for old_rec */
	ulint		buf_size;	/* old_rec_buf size if old_rec_buf
					is not NULL */
};

#define BTR_PCUR_IS_POSITIONED	1997660512	/* FIXME: currently, the state
						can be BTR_PCUR_IS_POSITIONED,
						though it really should be
						BTR_PCUR_WAS_POSITIONED,
						because we have no obligation
						to commit the cursor with
						mtr; similarly latch_mode may
						be out of date */
#define BTR_PCUR_WAS_POSITIONED	1187549791
#define BTR_PCUR_NOT_POSITIONED 1328997689

#define BTR_PCUR_OLD_STORED	908467085
#define BTR_PCUR_OLD_NOT_STORED	122766467

#ifndef UNIV_NONINL
#include "btr0pcur.ic"
#endif
				
#endif
