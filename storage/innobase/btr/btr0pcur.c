/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file btr/btr0pcur.c
The index tree persistent cursor

Created 2/23/1996 Heikki Tuuri
*******************************************************/

#include "btr0pcur.h"

#ifdef UNIV_NONINL
#include "btr0pcur.ic"
#endif

#include "ut0byte.h"
#include "rem0cmp.h"
#include "trx0trx.h"

/**************************************************************//**
Allocates memory for a persistent cursor object and initializes the cursor.
@return	own: persistent cursor */
UNIV_INTERN
btr_pcur_t*
btr_pcur_create_for_mysql(void)
/*============================*/
{
	btr_pcur_t*	pcur;

	pcur = mem_alloc(sizeof(btr_pcur_t));

	pcur->btr_cur.index = NULL;
	btr_pcur_init(pcur);

	return(pcur);
}

/**************************************************************//**
Resets a persistent cursor object, freeing ::old_rec_buf if it is
allocated and resetting the other members to their initial values. */
UNIV_INTERN
void
btr_pcur_reset(
/*===========*/
	btr_pcur_t*	cursor)	/*!< in, out: persistent cursor */
{
	if (cursor->old_rec_buf != NULL) {

		mem_free(cursor->old_rec_buf);

		cursor->old_rec_buf = NULL;
	}

	cursor->btr_cur.index = NULL;
	cursor->btr_cur.page_cur.rec = NULL;
	cursor->old_rec = NULL;
	cursor->old_n_fields = 0;
	cursor->old_stored = BTR_PCUR_OLD_NOT_STORED;

	cursor->latch_mode = BTR_NO_LATCHES;
	cursor->pos_state = BTR_PCUR_NOT_POSITIONED;
}

/**************************************************************//**
Frees the memory for a persistent cursor object. */
UNIV_INTERN
void
btr_pcur_free_for_mysql(
/*====================*/
	btr_pcur_t*	cursor)	/*!< in, own: persistent cursor */
{
	btr_pcur_reset(cursor);
	mem_free(cursor);
}

/**************************************************************//**
The position of the cursor is stored by taking an initial segment of the
record the cursor is positioned on, before, or after, and copying it to the
cursor data structure, or just setting a flag if the cursor id before the
first in an EMPTY tree, or after the last in an EMPTY tree. NOTE that the
page where the cursor is positioned must not be empty if the index tree is
not totally empty! */
UNIV_INTERN
void
btr_pcur_store_position(
/*====================*/
	btr_pcur_t*	cursor, /*!< in: persistent cursor */
	mtr_t*		mtr)	/*!< in: mtr */
{
	page_cur_t*	page_cursor;
	buf_block_t*	block;
	rec_t*		rec;
	dict_index_t*	index;
	page_t*		page;
	ulint		offs;

	ut_a(cursor->pos_state == BTR_PCUR_IS_POSITIONED);
	ut_ad(cursor->latch_mode != BTR_NO_LATCHES);

	block = btr_pcur_get_block(cursor);
	index = btr_cur_get_index(btr_pcur_get_btr_cur(cursor));

	page_cursor = btr_pcur_get_page_cur(cursor);

	rec = page_cur_get_rec(page_cursor);
	page = page_align(rec);
	offs = page_offset(rec);

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_S_FIX)
	      || mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	ut_a(cursor->latch_mode != BTR_NO_LATCHES);

	if (UNIV_UNLIKELY(page_get_n_recs(page) == 0)) {
		/* It must be an empty index tree; NOTE that in this case
		we do not store the modify_clock, but always do a search
		if we restore the cursor position */

		ut_a(btr_page_get_next(page, mtr) == FIL_NULL);
		ut_a(btr_page_get_prev(page, mtr) == FIL_NULL);
		ut_ad(page_is_leaf(page));
		ut_ad(page_get_page_no(page) == index->page);

		cursor->old_stored = BTR_PCUR_OLD_STORED;

		if (page_rec_is_supremum_low(offs)) {

			cursor->rel_pos = BTR_PCUR_AFTER_LAST_IN_TREE;
		} else {
			cursor->rel_pos = BTR_PCUR_BEFORE_FIRST_IN_TREE;
		}

		return;
	}

	if (page_rec_is_supremum_low(offs)) {

		rec = page_rec_get_prev(rec);

		cursor->rel_pos = BTR_PCUR_AFTER;

	} else if (page_rec_is_infimum_low(offs)) {

		rec = page_rec_get_next(rec);

		cursor->rel_pos = BTR_PCUR_BEFORE;
	} else {
		cursor->rel_pos = BTR_PCUR_ON;
	}

	cursor->old_stored = BTR_PCUR_OLD_STORED;
	cursor->old_rec = dict_index_copy_rec_order_prefix(
		index, rec, &cursor->old_n_fields,
		&cursor->old_rec_buf, &cursor->buf_size);

	cursor->block_when_stored = block;
	cursor->modify_clock = buf_block_get_modify_clock(block);
}

/**************************************************************//**
Copies the stored position of a pcur to another pcur. */
UNIV_INTERN
void
btr_pcur_copy_stored_position(
/*==========================*/
	btr_pcur_t*	pcur_receive,	/*!< in: pcur which will receive the
					position info */
	btr_pcur_t*	pcur_donate)	/*!< in: pcur from which the info is
					copied */
{
	if (pcur_receive->old_rec_buf) {
		mem_free(pcur_receive->old_rec_buf);
	}

	ut_memcpy(pcur_receive, pcur_donate, sizeof(btr_pcur_t));

	if (pcur_donate->old_rec_buf) {

		pcur_receive->old_rec_buf = mem_alloc(pcur_donate->buf_size);

		ut_memcpy(pcur_receive->old_rec_buf, pcur_donate->old_rec_buf,
			  pcur_donate->buf_size);
		pcur_receive->old_rec = pcur_receive->old_rec_buf
			+ (pcur_donate->old_rec - pcur_donate->old_rec_buf);
	}

	pcur_receive->old_n_fields = pcur_donate->old_n_fields;
}

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
UNIV_INTERN
ibool
btr_pcur_restore_position_func(
/*===========================*/
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor,		/*!< in: detached persistent cursor */
	const char*	file,		/*!< in: file name */
	ulint		line,		/*!< in: line where called */
	mtr_t*		mtr)		/*!< in: mtr */
{
	dict_index_t*	index;
	dtuple_t*	tuple;
	ulint		mode;
	ulint		old_mode;
	mem_heap_t*	heap;

	ut_ad(mtr);
	ut_ad(mtr->state == MTR_ACTIVE);

	index = btr_cur_get_index(btr_pcur_get_btr_cur(cursor));

	if (UNIV_UNLIKELY(cursor->old_stored != BTR_PCUR_OLD_STORED)
	    || UNIV_UNLIKELY(cursor->pos_state != BTR_PCUR_WAS_POSITIONED
			     && cursor->pos_state != BTR_PCUR_IS_POSITIONED)) {
		ut_print_buf(stderr, cursor, sizeof(btr_pcur_t));
		putc('\n', stderr);
		if (cursor->trx_if_known) {
			trx_print(stderr, cursor->trx_if_known, 0);
		}

		ut_error;
	}

	if (UNIV_UNLIKELY
	    (cursor->rel_pos == BTR_PCUR_AFTER_LAST_IN_TREE
	     || cursor->rel_pos == BTR_PCUR_BEFORE_FIRST_IN_TREE)) {

		/* In these cases we do not try an optimistic restoration,
		but always do a search */

		btr_cur_open_at_index_side(
			cursor->rel_pos == BTR_PCUR_BEFORE_FIRST_IN_TREE,
			index, latch_mode, btr_pcur_get_btr_cur(cursor), mtr);

		cursor->latch_mode = latch_mode;
		cursor->pos_state = BTR_PCUR_IS_POSITIONED;
		cursor->block_when_stored = btr_pcur_get_block(cursor);

		return(FALSE);
	}

	ut_a(cursor->old_rec);
	ut_a(cursor->old_n_fields);

	if (UNIV_LIKELY(latch_mode == BTR_SEARCH_LEAF)
	    || UNIV_LIKELY(latch_mode == BTR_MODIFY_LEAF)) {
		/* Try optimistic restoration */

		if (UNIV_LIKELY(buf_page_optimistic_get(
					latch_mode,
					cursor->block_when_stored,
					cursor->modify_clock,
					file, line, mtr))) {
			cursor->pos_state = BTR_PCUR_IS_POSITIONED;

			buf_block_dbg_add_level(
				btr_pcur_get_block(cursor),
				dict_index_is_ibuf(index)
				? SYNC_IBUF_TREE_NODE : SYNC_TREE_NODE);

			if (cursor->rel_pos == BTR_PCUR_ON) {
#ifdef UNIV_DEBUG
				const rec_t*	rec;
				const ulint*	offsets1;
				const ulint*	offsets2;
#endif /* UNIV_DEBUG */
				cursor->latch_mode = latch_mode;
#ifdef UNIV_DEBUG
				rec = btr_pcur_get_rec(cursor);

				heap = mem_heap_create(256);
				offsets1 = rec_get_offsets(
					cursor->old_rec, index, NULL,
					cursor->old_n_fields, &heap);
				offsets2 = rec_get_offsets(
					rec, index, NULL,
					cursor->old_n_fields, &heap);

				ut_ad(!cmp_rec_rec(cursor->old_rec,
						   rec, offsets1, offsets2,
						   index));
				mem_heap_free(heap);
#endif /* UNIV_DEBUG */
				return(TRUE);
			}

			return(FALSE);
		}
	}

	/* If optimistic restoration did not succeed, open the cursor anew */

	heap = mem_heap_create(256);

	tuple = dict_index_build_data_tuple(index, cursor->old_rec,
					    cursor->old_n_fields, heap);

	/* Save the old search mode of the cursor */
	old_mode = cursor->search_mode;

	switch (cursor->rel_pos) {
	case BTR_PCUR_ON:
		mode = PAGE_CUR_LE;
		break;
	case BTR_PCUR_AFTER:
		mode = PAGE_CUR_G;
		break;
	case BTR_PCUR_BEFORE:
		mode = PAGE_CUR_L;
		break;
	default:
		ut_error;
		mode = 0;
	}

	btr_pcur_open_with_no_init_func(index, tuple, mode, latch_mode,
					cursor, 0, file, line, mtr);

	/* Restore the old search mode */
	cursor->search_mode = old_mode;

	if (btr_pcur_is_on_user_rec(cursor)) {
		switch (cursor->rel_pos) {
		case BTR_PCUR_ON:
			if (!cmp_dtuple_rec(
				    tuple, btr_pcur_get_rec(cursor),
				    rec_get_offsets(btr_pcur_get_rec(cursor),
						    index, NULL,
						    ULINT_UNDEFINED, &heap))) {

				/* We have to store the NEW value for
				the modify clock, since the cursor can
				now be on a different page! But we can
				retain the value of old_rec */

				cursor->block_when_stored =
					btr_pcur_get_block(cursor);
				cursor->modify_clock =
					buf_block_get_modify_clock(
						cursor->block_when_stored);
				cursor->old_stored = BTR_PCUR_OLD_STORED;

				mem_heap_free(heap);

				return(TRUE);
			}

			break;
		case BTR_PCUR_BEFORE:
			page_cur_move_to_next(btr_pcur_get_page_cur(cursor));
			break;
		case BTR_PCUR_AFTER:
			page_cur_move_to_prev(btr_pcur_get_page_cur(cursor));
			break;
#ifdef UNIV_DEBUG
		default:
			ut_error;
#endif /* UNIV_DEBUG */
		}
	}

	mem_heap_free(heap);

	/* We have to store new position information, modify_clock etc.,
	to the cursor because it can now be on a different page, the record
	under it may have been removed, etc. */

	btr_pcur_store_position(cursor, mtr);

	return(FALSE);
}

/*********************************************************//**
Moves the persistent cursor to the first record on the next page. Releases the
latch on the current page, and bufferunfixes it. Note that there must not be
modifications on the current page, as then the x-latch can be released only in
mtr_commit. */
UNIV_INTERN
void
btr_pcur_move_to_next_page(
/*=======================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor; must be on the
				last record of the current page */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ulint		next_page_no;
	ulint		space;
	ulint		zip_size;
	page_t*		page;
	buf_block_t*	next_block;
	page_t*		next_page;

	ut_a(cursor->pos_state == BTR_PCUR_IS_POSITIONED);
	ut_ad(cursor->latch_mode != BTR_NO_LATCHES);
	ut_ad(btr_pcur_is_after_last_on_page(cursor));

	cursor->old_stored = BTR_PCUR_OLD_NOT_STORED;

	page = btr_pcur_get_page(cursor);
	next_page_no = btr_page_get_next(page, mtr);
	space = buf_block_get_space(btr_pcur_get_block(cursor));
	zip_size = buf_block_get_zip_size(btr_pcur_get_block(cursor));

	ut_ad(next_page_no != FIL_NULL);

	next_block = btr_block_get(space, zip_size, next_page_no,
				   cursor->latch_mode,
				   btr_pcur_get_btr_cur(cursor)->index, mtr);
	next_page = buf_block_get_frame(next_block);
#ifdef UNIV_BTR_DEBUG
	ut_a(page_is_comp(next_page) == page_is_comp(page));
	ut_a(btr_page_get_prev(next_page, mtr)
	     == buf_block_get_page_no(btr_pcur_get_block(cursor)));
#endif /* UNIV_BTR_DEBUG */
	next_block->check_index_page_at_flush = TRUE;

	btr_leaf_page_release(btr_pcur_get_block(cursor),
			      cursor->latch_mode, mtr);

	page_cur_set_before_first(next_block, btr_pcur_get_page_cur(cursor));

	page_check_dir(next_page);
}

/*********************************************************//**
Moves the persistent cursor backward if it is on the first record of the page.
Commits mtr. Note that to prevent a possible deadlock, the operation
first stores the position of the cursor, commits mtr, acquires the necessary
latches and restores the cursor position again before returning. The
alphabetical position of the cursor is guaranteed to be sensible on
return, but it may happen that the cursor is not positioned on the last
record of any page, because the structure of the tree may have changed
during the time when the cursor had no latches. */
UNIV_INTERN
void
btr_pcur_move_backward_from_page(
/*=============================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor, must be on the first
				record of the current page */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ulint		prev_page_no;
	page_t*		page;
	buf_block_t*	prev_block;
	ulint		latch_mode;
	ulint		latch_mode2;

	ut_a(cursor->pos_state == BTR_PCUR_IS_POSITIONED);
	ut_ad(cursor->latch_mode != BTR_NO_LATCHES);
	ut_ad(btr_pcur_is_before_first_on_page(cursor));
	ut_ad(!btr_pcur_is_before_first_in_tree(cursor, mtr));

	latch_mode = cursor->latch_mode;

	if (latch_mode == BTR_SEARCH_LEAF) {

		latch_mode2 = BTR_SEARCH_PREV;

	} else if (latch_mode == BTR_MODIFY_LEAF) {

		latch_mode2 = BTR_MODIFY_PREV;
	} else {
		latch_mode2 = 0; /* To eliminate compiler warning */
		ut_error;
	}

	btr_pcur_store_position(cursor, mtr);

	mtr_commit(mtr);

	mtr_start(mtr);

	btr_pcur_restore_position(latch_mode2, cursor, mtr);

	page = btr_pcur_get_page(cursor);

	prev_page_no = btr_page_get_prev(page, mtr);

	if (prev_page_no == FIL_NULL) {
	} else if (btr_pcur_is_before_first_on_page(cursor)) {

		prev_block = btr_pcur_get_btr_cur(cursor)->left_block;

		btr_leaf_page_release(btr_pcur_get_block(cursor),
				      latch_mode, mtr);

		page_cur_set_after_last(prev_block,
					btr_pcur_get_page_cur(cursor));
	} else {

		/* The repositioned cursor did not end on an infimum record on
		a page. Cursor repositioning acquired a latch also on the
		previous page, but we do not need the latch: release it. */

		prev_block = btr_pcur_get_btr_cur(cursor)->left_block;

		btr_leaf_page_release(prev_block, latch_mode, mtr);
	}

	cursor->latch_mode = latch_mode;

	cursor->old_stored = BTR_PCUR_OLD_NOT_STORED;
}

/*********************************************************//**
Moves the persistent cursor to the previous record in the tree. If no records
are left, the cursor stays 'before first in tree'.
@return	TRUE if the cursor was not before first in tree */
UNIV_INTERN
ibool
btr_pcur_move_to_prev(
/*==================*/
	btr_pcur_t*	cursor,	/*!< in: persistent cursor; NOTE that the
				function may release the page latch */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ut_ad(cursor->pos_state == BTR_PCUR_IS_POSITIONED);
	ut_ad(cursor->latch_mode != BTR_NO_LATCHES);

	cursor->old_stored = BTR_PCUR_OLD_NOT_STORED;

	if (btr_pcur_is_before_first_on_page(cursor)) {

		if (btr_pcur_is_before_first_in_tree(cursor, mtr)) {

			return(FALSE);
		}

		btr_pcur_move_backward_from_page(cursor, mtr);

		return(TRUE);
	}

	btr_pcur_move_to_prev_on_page(cursor);

	return(TRUE);
}

/**************************************************************//**
If mode is PAGE_CUR_G or PAGE_CUR_GE, opens a persistent cursor on the first
user record satisfying the search condition, in the case PAGE_CUR_L or
PAGE_CUR_LE, on the last user record. If no such user record exists, then
in the first case sets the cursor after last in tree, and in the latter case
before first in tree. The latching mode must be BTR_SEARCH_LEAF or
BTR_MODIFY_LEAF. */
UNIV_INTERN
void
btr_pcur_open_on_user_rec_func(
/*===========================*/
	dict_index_t*	index,		/*!< in: index */
	const dtuple_t*	tuple,		/*!< in: tuple on which search done */
	ulint		mode,		/*!< in: PAGE_CUR_L, ... */
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF or
					BTR_MODIFY_LEAF */
	btr_pcur_t*	cursor,		/*!< in: memory buffer for persistent
					cursor */
	const char*	file,		/*!< in: file name */
	ulint		line,		/*!< in: line where called */
	mtr_t*		mtr)		/*!< in: mtr */
{
	btr_pcur_open_func(index, tuple, mode, latch_mode, cursor,
			   file, line, mtr);

	if ((mode == PAGE_CUR_GE) || (mode == PAGE_CUR_G)) {

		if (btr_pcur_is_after_last_on_page(cursor)) {

			btr_pcur_move_to_next_user_rec(cursor, mtr);
		}
	} else {
		ut_ad((mode == PAGE_CUR_LE) || (mode == PAGE_CUR_L));

		/* Not implemented yet */

		ut_error;
	}
}
