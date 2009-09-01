/*****************************************************************************

Copyright (c) 1994, 2009, Innobase Oy. All Rights Reserved.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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

/******************************************************
The index tree cursor

All changes that row operations make to a B-tree or the records
there must go through this module! Undo log records are written here
of every modify or insert of a clustered index record.

			NOTE!!!
To make sure we do not run out of disk space during a pessimistic
insert or update, we have to reserve 2 x the height of the index tree
many pages in the tablespace before we start the operation, because
if leaf splitting has been started, it is difficult to undo, except
by crashing the database and doing a roll-forward.

Created 10/16/1994 Heikki Tuuri
*******************************************************/

#include "btr0cur.h"

#ifdef UNIV_NONINL
#include "btr0cur.ic"
#endif

#include "page0page.h"
#include "page0zip.h"
#include "rem0rec.h"
#include "rem0cmp.h"
#include "buf0lru.h"
#include "btr0btr.h"
#include "btr0sea.h"
#include "row0upd.h"
#include "trx0rec.h"
#include "trx0roll.h" /* trx_is_recv() */
#include "que0que.h"
#include "row0row.h"
#include "srv0srv.h"
#include "ibuf0ibuf.h"
#include "lock0lock.h"
#include "zlib.h"

#ifdef UNIV_DEBUG
/* If the following is set to TRUE, this module prints a lot of
trace information of individual record operations */
UNIV_INTERN ibool	btr_cur_print_record_ops = FALSE;
#endif /* UNIV_DEBUG */

UNIV_INTERN ulint	btr_cur_n_non_sea	= 0;
UNIV_INTERN ulint	btr_cur_n_sea		= 0;
UNIV_INTERN ulint	btr_cur_n_non_sea_old	= 0;
UNIV_INTERN ulint	btr_cur_n_sea_old	= 0;

/* In the optimistic insert, if the insert does not fit, but this much space
can be released by page reorganize, then it is reorganized */

#define BTR_CUR_PAGE_REORGANIZE_LIMIT	(UNIV_PAGE_SIZE / 32)

/* The structure of a BLOB part header */
/*--------------------------------------*/
#define BTR_BLOB_HDR_PART_LEN		0	/* BLOB part len on this
						page */
#define BTR_BLOB_HDR_NEXT_PAGE_NO	4	/* next BLOB part page no,
						FIL_NULL if none */
/*--------------------------------------*/
#define BTR_BLOB_HDR_SIZE		8

/* A BLOB field reference full of zero, for use in assertions and tests.
Initially, BLOB field references are set to zero, in
dtuple_convert_big_rec(). */
UNIV_INTERN const byte field_ref_zero[BTR_EXTERN_FIELD_REF_SIZE];

/***********************************************************************
Marks all extern fields in a record as owned by the record. This function
should be called if the delete mark of a record is removed: a not delete
marked record always owns all its extern fields. */
static
void
btr_cur_unmark_extern_fields(
/*=========================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page whose uncompressed
				part will be updated, or NULL */
	rec_t*		rec,	/* in/out: record in a clustered index */
	dict_index_t*	index,	/* in: index of the page */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	mtr_t*		mtr);	/* in: mtr, or NULL if not logged */
/***********************************************************************
Adds path information to the cursor for the current page, for which
the binary search has been performed. */
static
void
btr_cur_add_path_info(
/*==================*/
	btr_cur_t*	cursor,		/* in: cursor positioned on a page */
	ulint		height,		/* in: height of the page in tree;
					0 means leaf node */
	ulint		root_height);	/* in: root node height in tree */
/***************************************************************
Frees the externally stored fields for a record, if the field is mentioned
in the update vector. */
static
void
btr_rec_free_updated_extern_fields(
/*===============================*/
	dict_index_t*	index,	/* in: index of rec; the index tree MUST be
				X-latched */
	rec_t*		rec,	/* in: record */
	page_zip_des_t*	page_zip,/* in: compressed page whose uncompressed
				part will be updated, or NULL */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	const upd_t*	update,	/* in: update vector */
	enum trx_rb_ctx	rb_ctx,	/* in: rollback context */
	mtr_t*		mtr);	/* in: mini-transaction handle which contains
				an X-latch to record page and to the tree */
/***************************************************************
Frees the externally stored fields for a record. */
static
void
btr_rec_free_externally_stored_fields(
/*==================================*/
	dict_index_t*	index,	/* in: index of the data, the index
				tree MUST be X-latched */
	rec_t*		rec,	/* in: record */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	page_zip_des_t*	page_zip,/* in: compressed page whose uncompressed
				part will be updated, or NULL */
	enum trx_rb_ctx	rb_ctx,	/* in: rollback context */
	mtr_t*		mtr);	/* in: mini-transaction handle which contains
				an X-latch to record page and to the index
				tree */
/***************************************************************
Gets the externally stored size of a record, in units of a database page. */
static
ulint
btr_rec_get_externally_stored_len(
/*==============================*/
				/* out: externally stored part,
				in units of a database page */
	rec_t*		rec,	/* in: record */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */

/**********************************************************
The following function is used to set the deleted bit of a record. */
UNIV_INLINE
void
btr_rec_set_deleted_flag(
/*=====================*/
				/* out: TRUE on success;
				FALSE on page_zip overflow */
	rec_t*		rec,	/* in/out: physical record */
	page_zip_des_t*	page_zip,/* in/out: compressed page (or NULL) */
	ulint		flag)	/* in: nonzero if delete marked */
{
	if (page_rec_is_comp(rec)) {
		rec_set_deleted_flag_new(rec, page_zip, flag);
	} else {
		ut_ad(!page_zip);
		rec_set_deleted_flag_old(rec, flag);
	}
}

/*==================== B-TREE SEARCH =========================*/

/************************************************************************
Latches the leaf page or pages requested. */
static
void
btr_cur_latch_leaves(
/*=================*/
	page_t*		page,		/* in: leaf page where the search
					converged */
	ulint		space,		/* in: space id */
	ulint		zip_size,	/* in: compressed page size in bytes
					or 0 for uncompressed pages */
	ulint		page_no,	/* in: page number of the leaf */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor,		/* in: cursor */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint		mode;
	ulint		left_page_no;
	ulint		right_page_no;
	buf_block_t*	get_block;

	ut_ad(page && mtr);

	switch (latch_mode) {
	case BTR_SEARCH_LEAF:
	case BTR_MODIFY_LEAF:
		mode = latch_mode == BTR_SEARCH_LEAF ? RW_S_LATCH : RW_X_LATCH;
		get_block = btr_block_get(space, zip_size, page_no, mode, mtr);
#ifdef UNIV_BTR_DEBUG
		ut_a(page_is_comp(get_block->frame) == page_is_comp(page));
#endif /* UNIV_BTR_DEBUG */
		get_block->check_index_page_at_flush = TRUE;
		return;
	case BTR_MODIFY_TREE:
		/* x-latch also brothers from left to right */
		left_page_no = btr_page_get_prev(page, mtr);

		if (left_page_no != FIL_NULL) {
			get_block = btr_block_get(space, zip_size,
						  left_page_no,
						  RW_X_LATCH, mtr);
#ifdef UNIV_BTR_DEBUG
			ut_a(page_is_comp(get_block->frame)
			     == page_is_comp(page));
			ut_a(btr_page_get_next(get_block->frame, mtr)
			     == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */
			get_block->check_index_page_at_flush = TRUE;
		}

		get_block = btr_block_get(space, zip_size, page_no,
					  RW_X_LATCH, mtr);
#ifdef UNIV_BTR_DEBUG
		ut_a(page_is_comp(get_block->frame) == page_is_comp(page));
#endif /* UNIV_BTR_DEBUG */
		get_block->check_index_page_at_flush = TRUE;

		right_page_no = btr_page_get_next(page, mtr);

		if (right_page_no != FIL_NULL) {
			get_block = btr_block_get(space, zip_size,
						  right_page_no,
						  RW_X_LATCH, mtr);
#ifdef UNIV_BTR_DEBUG
			ut_a(page_is_comp(get_block->frame)
			     == page_is_comp(page));
			ut_a(btr_page_get_prev(get_block->frame, mtr)
			     == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */
			get_block->check_index_page_at_flush = TRUE;
		}

		return;

	case BTR_SEARCH_PREV:
	case BTR_MODIFY_PREV:
		mode = latch_mode == BTR_SEARCH_PREV ? RW_S_LATCH : RW_X_LATCH;
		/* latch also left brother */
		left_page_no = btr_page_get_prev(page, mtr);

		if (left_page_no != FIL_NULL) {
			get_block = btr_block_get(space, zip_size,
						  left_page_no, mode, mtr);
			cursor->left_block = get_block;
#ifdef UNIV_BTR_DEBUG
			ut_a(page_is_comp(get_block->frame)
			     == page_is_comp(page));
			ut_a(btr_page_get_next(get_block->frame, mtr)
			     == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */
			get_block->check_index_page_at_flush = TRUE;
		}

		get_block = btr_block_get(space, zip_size, page_no, mode, mtr);
#ifdef UNIV_BTR_DEBUG
		ut_a(page_is_comp(get_block->frame) == page_is_comp(page));
#endif /* UNIV_BTR_DEBUG */
		get_block->check_index_page_at_flush = TRUE;
		return;
	}

	ut_error;
}

/************************************************************************
Searches an index tree and positions a tree cursor on a given level.
NOTE: n_fields_cmp in tuple must be set so that it cannot be compared
to node pointer page number fields on the upper levels of the tree!
Note that if mode is PAGE_CUR_LE, which is used in inserts, then
cursor->up_match and cursor->low_match both will have sensible values.
If mode is PAGE_CUR_GE, then up_match will a have a sensible value.

If mode is PAGE_CUR_LE , cursor is left at the place where an insert of the
search tuple should be performed in the B-tree. InnoDB does an insert
immediately after the cursor. Thus, the cursor may end up on a user record,
or on a page infimum record. */
UNIV_INTERN
void
btr_cur_search_to_nth_level(
/*========================*/
	dict_index_t*	index,	/* in: index */
	ulint		level,	/* in: the tree level of search */
	const dtuple_t*	tuple,	/* in: data tuple; NOTE: n_fields_cmp in
				tuple must be set so that it cannot get
				compared to the node ptr page number field! */
	ulint		mode,	/* in: PAGE_CUR_L, ...;
				Inserts should always be made using
				PAGE_CUR_LE to search the position! */
	ulint		latch_mode, /* in: BTR_SEARCH_LEAF, ..., ORed with
				BTR_INSERT and BTR_ESTIMATE;
				cursor->left_block is used to store a pointer
				to the left neighbor page, in the cases
				BTR_SEARCH_PREV and BTR_MODIFY_PREV;
				NOTE that if has_search_latch
				is != 0, we maybe do not have a latch set
				on the cursor page, we assume
				the caller uses his search latch
				to protect the record! */
	btr_cur_t*	cursor, /* in/out: tree cursor; the cursor page is
				s- or x-latched, but see also above! */
	ulint		has_search_latch,/* in: info on the latch mode the
				caller currently has on btr_search_latch:
				RW_S_LATCH, or 0 */
	mtr_t*		mtr)	/* in: mtr */
{
	page_cur_t*	page_cursor;
	page_t*		page;
	buf_block_t*	guess;
	rec_t*		node_ptr;
	ulint		page_no;
	ulint		space;
	ulint		up_match;
	ulint		up_bytes;
	ulint		low_match;
	ulint		low_bytes;
	ulint		height;
	ulint		savepoint;
	ulint		page_mode;
	ulint		insert_planned;
	ulint		estimate;
	ulint		ignore_sec_unique;
	ulint		root_height = 0; /* remove warning */
#ifdef BTR_CUR_ADAPT
	btr_search_t*	info;
#endif
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);
	/* Currently, PAGE_CUR_LE is the only search mode used for searches
	ending to upper levels */

	ut_ad(level == 0 || mode == PAGE_CUR_LE);
	ut_ad(dict_index_check_search_tuple(index, tuple));
	ut_ad(!dict_index_is_ibuf(index) || ibuf_inside());
	ut_ad(dtuple_check_typed(tuple));

#ifdef UNIV_DEBUG
	cursor->up_match = ULINT_UNDEFINED;
	cursor->low_match = ULINT_UNDEFINED;
#endif
	insert_planned = latch_mode & BTR_INSERT;
	estimate = latch_mode & BTR_ESTIMATE;
	ignore_sec_unique = latch_mode & BTR_IGNORE_SEC_UNIQUE;
	latch_mode = latch_mode & ~(BTR_INSERT | BTR_ESTIMATE
				    | BTR_IGNORE_SEC_UNIQUE);

	ut_ad(!insert_planned || (mode == PAGE_CUR_LE));

	cursor->flag = BTR_CUR_BINARY;
	cursor->index = index;

#ifndef BTR_CUR_ADAPT
	guess = NULL;
#else
	info = btr_search_get_info(index);

	guess = info->root_guess;

#ifdef BTR_CUR_HASH_ADAPT

#ifdef UNIV_SEARCH_PERF_STAT
	info->n_searches++;
#endif
	if (rw_lock_get_writer(&btr_search_latch) == RW_LOCK_NOT_LOCKED
	    && latch_mode <= BTR_MODIFY_LEAF && info->last_hash_succ
	    && !estimate
#ifdef PAGE_CUR_LE_OR_EXTENDS
	    && mode != PAGE_CUR_LE_OR_EXTENDS
#endif /* PAGE_CUR_LE_OR_EXTENDS */
	    /* If !has_search_latch, we do a dirty read of
	    btr_search_enabled below, and btr_search_guess_on_hash()
	    will have to check it again. */
	    && UNIV_LIKELY(btr_search_enabled)
	    && btr_search_guess_on_hash(index, info, tuple, mode,
					latch_mode, cursor,
					has_search_latch, mtr)) {

		/* Search using the hash index succeeded */

		ut_ad(cursor->up_match != ULINT_UNDEFINED
		      || mode != PAGE_CUR_GE);
		ut_ad(cursor->up_match != ULINT_UNDEFINED
		      || mode != PAGE_CUR_LE);
		ut_ad(cursor->low_match != ULINT_UNDEFINED
		      || mode != PAGE_CUR_LE);
		btr_cur_n_sea++;

		return;
	}
#endif /* BTR_CUR_HASH_ADAPT */
#endif /* BTR_CUR_ADAPT */
	btr_cur_n_non_sea++;

	/* If the hash search did not succeed, do binary search down the
	tree */

	if (has_search_latch) {
		/* Release possible search latch to obey latching order */
		rw_lock_s_unlock(&btr_search_latch);
	}

	/* Store the position of the tree latch we push to mtr so that we
	know how to release it when we have latched leaf node(s) */

	savepoint = mtr_set_savepoint(mtr);

	if (latch_mode == BTR_MODIFY_TREE) {
		mtr_x_lock(dict_index_get_lock(index), mtr);

	} else if (latch_mode == BTR_CONT_MODIFY_TREE) {
		/* Do nothing */
		ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
					MTR_MEMO_X_LOCK));
	} else {
		mtr_s_lock(dict_index_get_lock(index), mtr);
	}

	page_cursor = btr_cur_get_page_cur(cursor);

	space = dict_index_get_space(index);
	page_no = dict_index_get_page(index);

	up_match = 0;
	up_bytes = 0;
	low_match = 0;
	low_bytes = 0;

	height = ULINT_UNDEFINED;

	/* We use these modified search modes on non-leaf levels of the
	B-tree. These let us end up in the right B-tree leaf. In that leaf
	we use the original search mode. */

	switch (mode) {
	case PAGE_CUR_GE:
		page_mode = PAGE_CUR_L;
		break;
	case PAGE_CUR_G:
		page_mode = PAGE_CUR_LE;
		break;
	default:
#ifdef PAGE_CUR_LE_OR_EXTENDS
		ut_ad(mode == PAGE_CUR_L || mode == PAGE_CUR_LE
		      || mode == PAGE_CUR_LE_OR_EXTENDS);
#else /* PAGE_CUR_LE_OR_EXTENDS */
		ut_ad(mode == PAGE_CUR_L || mode == PAGE_CUR_LE);
#endif /* PAGE_CUR_LE_OR_EXTENDS */
		page_mode = mode;
		break;
	}

	/* Loop and search until we arrive at the desired level */

	for (;;) {
		ulint		zip_size;
		buf_block_t*	block;
		ulint		rw_latch;
		ulint		buf_mode;

		zip_size = dict_table_zip_size(index->table);
		rw_latch = RW_NO_LATCH;
		buf_mode = BUF_GET;

		if (height == 0 && latch_mode <= BTR_MODIFY_LEAF) {

			rw_latch = latch_mode;

			if (insert_planned
			    && ibuf_should_try(index, ignore_sec_unique)) {

				/* Try insert to the insert buffer if the
				page is not in the buffer pool */

				buf_mode = BUF_GET_IF_IN_POOL;
			}
		}

retry_page_get:
		block = buf_page_get_gen(space, zip_size, page_no,
					 rw_latch, guess, buf_mode,
					 __FILE__, __LINE__, mtr);
		if (block == NULL) {
			/* This must be a search to perform an insert;
			try insert to the insert buffer */

			ut_ad(buf_mode == BUF_GET_IF_IN_POOL);
			ut_ad(insert_planned);
			ut_ad(cursor->thr);

			if (ibuf_insert(tuple, index, space, zip_size,
					page_no, cursor->thr)) {
				/* Insertion to the insert buffer succeeded */
				cursor->flag = BTR_CUR_INSERT_TO_IBUF;
				if (UNIV_LIKELY_NULL(heap)) {
					mem_heap_free(heap);
				}
				goto func_exit;
			}

			/* Insert to the insert buffer did not succeed:
			retry page get */

			buf_mode = BUF_GET;

			goto retry_page_get;
		}

		page = buf_block_get_frame(block);

		block->check_index_page_at_flush = TRUE;

		if (rw_latch != RW_NO_LATCH) {
#ifdef UNIV_ZIP_DEBUG
			const page_zip_des_t*	page_zip
				= buf_block_get_page_zip(block);
			ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */

			buf_block_dbg_add_level(block, SYNC_TREE_NODE);
		}

		ut_ad(0 == ut_dulint_cmp(index->id,
					 btr_page_get_index_id(page)));

		if (UNIV_UNLIKELY(height == ULINT_UNDEFINED)) {
			/* We are in the root node */

			height = btr_page_get_level(page, mtr);
			root_height = height;
			cursor->tree_height = root_height + 1;
#ifdef BTR_CUR_ADAPT
			if (block != guess) {
				info->root_guess = block;
			}
#endif
		}

		if (height == 0) {
			if (rw_latch == RW_NO_LATCH) {

				btr_cur_latch_leaves(page, space, zip_size,
						     page_no, latch_mode,
						     cursor, mtr);
			}

			if ((latch_mode != BTR_MODIFY_TREE)
			    && (latch_mode != BTR_CONT_MODIFY_TREE)) {

				/* Release the tree s-latch */

				mtr_release_s_latch_at_savepoint(
					mtr, savepoint,
					dict_index_get_lock(index));
			}

			page_mode = mode;
		}

		page_cur_search_with_match(block, index, tuple, page_mode,
					   &up_match, &up_bytes,
					   &low_match, &low_bytes,
					   page_cursor);

		if (estimate) {
			btr_cur_add_path_info(cursor, height, root_height);
		}

		/* If this is the desired level, leave the loop */

		ut_ad(height == btr_page_get_level(
			      page_cur_get_page(page_cursor), mtr));

		if (level == height) {

			if (level > 0) {
				/* x-latch the page */
				page = btr_page_get(space, zip_size,
						    page_no, RW_X_LATCH, mtr);
				ut_a((ibool)!!page_is_comp(page)
				     == dict_table_is_comp(index->table));
			}

			break;
		}

		ut_ad(height > 0);

		height--;

		guess = NULL;

		node_ptr = page_cur_get_rec(page_cursor);
		offsets = rec_get_offsets(node_ptr, cursor->index, offsets,
					  ULINT_UNDEFINED, &heap);
		/* Go to the child node */
		page_no = btr_node_ptr_get_child_page_no(node_ptr, offsets);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	if (level == 0) {
		cursor->low_match = low_match;
		cursor->low_bytes = low_bytes;
		cursor->up_match = up_match;
		cursor->up_bytes = up_bytes;

#ifdef BTR_CUR_ADAPT
		/* We do a dirty read of btr_search_enabled here.  We
		will properly check btr_search_enabled again in
		btr_search_build_page_hash_index() before building a
		page hash index, while holding btr_search_latch. */
		if (UNIV_LIKELY(btr_search_enabled)) {

			btr_search_info_update(index, cursor);
		}
#endif
		ut_ad(cursor->up_match != ULINT_UNDEFINED
		      || mode != PAGE_CUR_GE);
		ut_ad(cursor->up_match != ULINT_UNDEFINED
		      || mode != PAGE_CUR_LE);
		ut_ad(cursor->low_match != ULINT_UNDEFINED
		      || mode != PAGE_CUR_LE);
	}

func_exit:
	if (has_search_latch) {

		rw_lock_s_lock(&btr_search_latch);
	}
}

/*********************************************************************
Opens a cursor at either end of an index. */
UNIV_INTERN
void
btr_cur_open_at_index_side(
/*=======================*/
	ibool		from_left,	/* in: TRUE if open to the low end,
					FALSE if to the high end */
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: latch mode */
	btr_cur_t*	cursor,		/* in: cursor */
	mtr_t*		mtr)		/* in: mtr */
{
	page_cur_t*	page_cursor;
	ulint		page_no;
	ulint		space;
	ulint		zip_size;
	ulint		height;
	ulint		root_height = 0; /* remove warning */
	rec_t*		node_ptr;
	ulint		estimate;
	ulint		savepoint;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	estimate = latch_mode & BTR_ESTIMATE;
	latch_mode = latch_mode & ~BTR_ESTIMATE;

	/* Store the position of the tree latch we push to mtr so that we
	know how to release it when we have latched the leaf node */

	savepoint = mtr_set_savepoint(mtr);

	if (latch_mode == BTR_MODIFY_TREE) {
		mtr_x_lock(dict_index_get_lock(index), mtr);
	} else {
		mtr_s_lock(dict_index_get_lock(index), mtr);
	}

	page_cursor = btr_cur_get_page_cur(cursor);
	cursor->index = index;

	space = dict_index_get_space(index);
	zip_size = dict_table_zip_size(index->table);
	page_no = dict_index_get_page(index);

	height = ULINT_UNDEFINED;

	for (;;) {
		buf_block_t*	block;
		page_t*		page;
		block = buf_page_get_gen(space, zip_size, page_no,
					 RW_NO_LATCH, NULL, BUF_GET,
					 __FILE__, __LINE__, mtr);
		page = buf_block_get_frame(block);
		ut_ad(0 == ut_dulint_cmp(index->id,
					 btr_page_get_index_id(page)));

		block->check_index_page_at_flush = TRUE;

		if (height == ULINT_UNDEFINED) {
			/* We are in the root node */

			height = btr_page_get_level(page, mtr);
			root_height = height;
		}

		if (height == 0) {
			btr_cur_latch_leaves(page, space, zip_size, page_no,
					     latch_mode, cursor, mtr);

			/* In versions <= 3.23.52 we had forgotten to
			release the tree latch here. If in an index scan
			we had to scan far to find a record visible to the
			current transaction, that could starve others
			waiting for the tree latch. */

			if ((latch_mode != BTR_MODIFY_TREE)
			    && (latch_mode != BTR_CONT_MODIFY_TREE)) {

				/* Release the tree s-latch */

				mtr_release_s_latch_at_savepoint(
					mtr, savepoint,
					dict_index_get_lock(index));
			}
		}

		if (from_left) {
			page_cur_set_before_first(block, page_cursor);
		} else {
			page_cur_set_after_last(block, page_cursor);
		}

		if (height == 0) {
			if (estimate) {
				btr_cur_add_path_info(cursor, height,
						      root_height);
			}

			break;
		}

		ut_ad(height > 0);

		if (from_left) {
			page_cur_move_to_next(page_cursor);
		} else {
			page_cur_move_to_prev(page_cursor);
		}

		if (estimate) {
			btr_cur_add_path_info(cursor, height, root_height);
		}

		height--;

		node_ptr = page_cur_get_rec(page_cursor);
		offsets = rec_get_offsets(node_ptr, cursor->index, offsets,
					  ULINT_UNDEFINED, &heap);
		/* Go to the child node */
		page_no = btr_node_ptr_get_child_page_no(node_ptr, offsets);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/**************************************************************************
Positions a cursor at a randomly chosen position within a B-tree. */
UNIV_INTERN
void
btr_cur_open_at_rnd_pos(
/*====================*/
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor,		/* in/out: B-tree cursor */
	mtr_t*		mtr)		/* in: mtr */
{
	page_cur_t*	page_cursor;
	ulint		page_no;
	ulint		space;
	ulint		zip_size;
	ulint		height;
	rec_t*		node_ptr;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	if (latch_mode == BTR_MODIFY_TREE) {
		mtr_x_lock(dict_index_get_lock(index), mtr);
	} else {
		mtr_s_lock(dict_index_get_lock(index), mtr);
	}

	page_cursor = btr_cur_get_page_cur(cursor);
	cursor->index = index;

	space = dict_index_get_space(index);
	zip_size = dict_table_zip_size(index->table);
	page_no = dict_index_get_page(index);

	height = ULINT_UNDEFINED;

	for (;;) {
		buf_block_t*	block;
		page_t*		page;

		block = buf_page_get_gen(space, zip_size, page_no,
					 RW_NO_LATCH, NULL, BUF_GET,
					 __FILE__, __LINE__, mtr);
		page = buf_block_get_frame(block);
		ut_ad(0 == ut_dulint_cmp(index->id,
					 btr_page_get_index_id(page)));

		if (height == ULINT_UNDEFINED) {
			/* We are in the root node */

			height = btr_page_get_level(page, mtr);
		}

		if (height == 0) {
			btr_cur_latch_leaves(page, space, zip_size, page_no,
					     latch_mode, cursor, mtr);
		}

		page_cur_open_on_rnd_user_rec(block, page_cursor);

		if (height == 0) {

			break;
		}

		ut_ad(height > 0);

		height--;

		node_ptr = page_cur_get_rec(page_cursor);
		offsets = rec_get_offsets(node_ptr, cursor->index, offsets,
					  ULINT_UNDEFINED, &heap);
		/* Go to the child node */
		page_no = btr_node_ptr_get_child_page_no(node_ptr, offsets);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/*==================== B-TREE INSERT =========================*/

/*****************************************************************
Inserts a record if there is enough space, or if enough space can
be freed by reorganizing. Differs from btr_cur_optimistic_insert because
no heuristics is applied to whether it pays to use CPU time for
reorganizing the page or not. */
static
rec_t*
btr_cur_insert_if_possible(
/*=======================*/
				/* out: pointer to inserted record if succeed,
				else NULL */
	btr_cur_t*	cursor,	/* in: cursor on page after which to insert;
				cursor stays valid */
	const dtuple_t*	tuple,	/* in: tuple to insert; the size info need not
				have been stored to tuple */
	ulint		n_ext,	/* in: number of externally stored columns */
	mtr_t*		mtr)	/* in: mtr */
{
	page_cur_t*	page_cursor;
	buf_block_t*	block;
	rec_t*		rec;

	ut_ad(dtuple_check_typed(tuple));

	block = btr_cur_get_block(cursor);

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	page_cursor = btr_cur_get_page_cur(cursor);

	/* Now, try the insert */
	rec = page_cur_tuple_insert(page_cursor, tuple,
				    cursor->index, n_ext, mtr);

	if (UNIV_UNLIKELY(!rec)) {
		/* If record did not fit, reorganize */

		if (btr_page_reorganize(block, cursor->index, mtr)) {

			page_cur_search(block, cursor->index, tuple,
					PAGE_CUR_LE, page_cursor);

			rec = page_cur_tuple_insert(page_cursor, tuple,
						    cursor->index, n_ext, mtr);
		}
	}

	return(rec);
}

/*****************************************************************
For an insert, checks the locks and does the undo logging if desired. */
UNIV_INLINE
ulint
btr_cur_ins_lock_and_undo(
/*======================*/
				/* out: DB_SUCCESS, DB_WAIT_LOCK,
				DB_FAIL, or error number */
	ulint		flags,	/* in: undo logging and locking flags: if
				not zero, the parameters index and thr
				should be specified */
	btr_cur_t*	cursor,	/* in: cursor on page after which to insert */
	const dtuple_t*	entry,	/* in: entry to insert */
	que_thr_t*	thr,	/* in: query thread or NULL */
	ibool*		inherit)/* out: TRUE if the inserted new record maybe
				should inherit LOCK_GAP type locks from the
				successor record */
{
	dict_index_t*	index;
	ulint		err;
	rec_t*		rec;
	dulint		roll_ptr;

	/* Check if we have to wait for a lock: enqueue an explicit lock
	request if yes */

	rec = btr_cur_get_rec(cursor);
	index = cursor->index;

	err = lock_rec_insert_check_and_lock(flags, rec,
					     btr_cur_get_block(cursor),
					     index, thr, inherit);

	if (err != DB_SUCCESS) {

		return(err);
	}

	if (dict_index_is_clust(index) && !dict_index_is_ibuf(index)) {

		err = trx_undo_report_row_operation(flags, TRX_UNDO_INSERT_OP,
						    thr, index, entry,
						    NULL, 0, NULL,
						    &roll_ptr);
		if (err != DB_SUCCESS) {

			return(err);
		}

		/* Now we can fill in the roll ptr field in entry */

		if (!(flags & BTR_KEEP_SYS_FLAG)) {

			row_upd_index_entry_sys_field(entry, index,
						      DATA_ROLL_PTR, roll_ptr);
		}
	}

	return(DB_SUCCESS);
}

#ifdef UNIV_DEBUG
/*****************************************************************
Report information about a transaction. */
static
void
btr_cur_trx_report(
/*===============*/
	trx_t*			trx,	/* in: transaction */
	const dict_index_t*	index,	/* in: index */
	const char*		op)	/* in: operation */
{
	fprintf(stderr, "Trx with id " TRX_ID_FMT " going to ",
		TRX_ID_PREP_PRINTF(trx->id));
	fputs(op, stderr);
	dict_index_name_print(stderr, trx, index);
	putc('\n', stderr);
}
#endif /* UNIV_DEBUG */

/*****************************************************************
Tries to perform an insert to a page in an index tree, next to cursor.
It is assumed that mtr holds an x-latch on the page. The operation does
not succeed if there is too little space on the page. If there is just
one record on the page, the insert will always succeed; this is to
prevent trying to split a page with just one record. */
UNIV_INTERN
ulint
btr_cur_optimistic_insert(
/*======================*/
				/* out: DB_SUCCESS, DB_WAIT_LOCK,
				DB_FAIL, or error number */
	ulint		flags,	/* in: undo logging and locking flags: if not
				zero, the parameters index and thr should be
				specified */
	btr_cur_t*	cursor,	/* in: cursor on page after which to insert;
				cursor stays valid */
	dtuple_t*	entry,	/* in/out: entry to insert */
	rec_t**		rec,	/* out: pointer to inserted record if
				succeed */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or
				NULL */
	ulint		n_ext,	/* in: number of externally stored columns */
	que_thr_t*	thr,	/* in: query thread or NULL */
	mtr_t*		mtr)	/* in: mtr; if this function returns
				DB_SUCCESS on a leaf page of a secondary
				index in a compressed tablespace, the
				mtr must be committed before latching
				any further pages */
{
	big_rec_t*	big_rec_vec	= NULL;
	dict_index_t*	index;
	page_cur_t*	page_cursor;
	buf_block_t*	block;
	page_t*		page;
	ulint		max_size;
	rec_t*		dummy_rec;
	ibool		leaf;
	ibool		reorg;
	ibool		inherit;
	ulint		zip_size;
	ulint		rec_size;
	mem_heap_t*	heap		= NULL;
	ulint		err;

	*big_rec = NULL;

	block = btr_cur_get_block(cursor);
	page = buf_block_get_frame(block);
	index = cursor->index;
	zip_size = buf_block_get_zip_size(block);
#ifdef UNIV_DEBUG_VALGRIND
	if (zip_size) {
		UNIV_MEM_ASSERT_RW(page, UNIV_PAGE_SIZE);
		UNIV_MEM_ASSERT_RW(block->page.zip.data, zip_size);
	}
#endif /* UNIV_DEBUG_VALGRIND */

	if (!dtuple_check_typed_no_assert(entry)) {
		fputs("InnoDB: Error in a tuple to insert into ", stderr);
		dict_index_name_print(stderr, thr_get_trx(thr), index);
	}
#ifdef UNIV_DEBUG
	if (btr_cur_print_record_ops && thr) {
		btr_cur_trx_report(thr_get_trx(thr), index, "insert into ");
		dtuple_print(stderr, entry);
	}
#endif /* UNIV_DEBUG */

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	max_size = page_get_max_insert_size_after_reorganize(page, 1);
	leaf = page_is_leaf(page);

	/* Calculate the record size when entry is converted to a record */
	rec_size = rec_get_converted_size(index, entry, n_ext);

	if (page_zip_rec_needs_ext(rec_size, page_is_comp(page),
				   dtuple_get_n_fields(entry), zip_size)) {

		/* The record is so big that we have to store some fields
		externally on separate database pages */
		big_rec_vec = dtuple_convert_big_rec(index, entry, &n_ext);

		if (UNIV_UNLIKELY(big_rec_vec == NULL)) {

			return(DB_TOO_BIG_RECORD);
		}

		rec_size = rec_get_converted_size(index, entry, n_ext);
	}

	if (UNIV_UNLIKELY(zip_size)) {
		/* Estimate the free space of an empty compressed page.
		Subtract one byte for the encoded heap_no in the
		modification log. */
		ulint	free_space_zip = page_zip_empty_size(
			cursor->index->n_fields, zip_size) - 1;
		ulint	n_uniq = dict_index_get_n_unique_in_tree(index);

		ut_ad(dict_table_is_comp(index->table));

		/* There should be enough room for two node pointer
		records on an empty non-leaf page.  This prevents
		infinite page splits. */

		if (UNIV_LIKELY(entry->n_fields >= n_uniq)
		    && UNIV_UNLIKELY(REC_NODE_PTR_SIZE
				     + rec_get_converted_size_comp_prefix(
					     index, entry->fields, n_uniq,
					     NULL)
				     /* On a compressed page, there is
				     a two-byte entry in the dense
				     page directory for every record.
				     But there is no record header. */
				     - (REC_N_NEW_EXTRA_BYTES - 2)
				     > free_space_zip / 2)) {

			if (big_rec_vec) {
				dtuple_convert_back_big_rec(
					index, entry, big_rec_vec);
			}

			if (heap) {
				mem_heap_free(heap);
			}

			return(DB_TOO_BIG_RECORD);
		}
	}

	/* If there have been many consecutive inserts, and we are on the leaf
	level, check if we have to split the page to reserve enough free space
	for future updates of records. */

	if (dict_index_is_clust(index)
	    && (page_get_n_recs(page) >= 2)
	    && UNIV_LIKELY(leaf)
	    && (dict_index_get_space_reserve() + rec_size > max_size)
	    && (btr_page_get_split_rec_to_right(cursor, &dummy_rec)
		|| btr_page_get_split_rec_to_left(cursor, &dummy_rec))) {
fail:
		err = DB_FAIL;
fail_err:

		if (big_rec_vec) {
			dtuple_convert_back_big_rec(index, entry, big_rec_vec);
		}

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}

		return(err);
	}

	if (UNIV_UNLIKELY(max_size < BTR_CUR_PAGE_REORGANIZE_LIMIT
	     || max_size < rec_size)
	    && UNIV_LIKELY(page_get_n_recs(page) > 1)
	    && page_get_max_insert_size(page, 1) < rec_size) {

		goto fail;
	}

	/* Check locks and write to the undo log, if specified */
	err = btr_cur_ins_lock_and_undo(flags, cursor, entry, thr, &inherit);

	if (UNIV_UNLIKELY(err != DB_SUCCESS)) {

		goto fail_err;
	}

	page_cursor = btr_cur_get_page_cur(cursor);

	/* Now, try the insert */

	{
		const rec_t* page_cursor_rec = page_cur_get_rec(page_cursor);
		*rec = page_cur_tuple_insert(page_cursor, entry, index,
					     n_ext, mtr);
		reorg = page_cursor_rec != page_cur_get_rec(page_cursor);

		if (UNIV_UNLIKELY(reorg)) {
			ut_a(zip_size);
			ut_a(*rec);
		}
	}

	if (UNIV_UNLIKELY(!*rec) && UNIV_LIKELY(!reorg)) {
		/* If the record did not fit, reorganize */
		if (UNIV_UNLIKELY(!btr_page_reorganize(block, index, mtr))) {
			ut_a(zip_size);

			goto fail;
		}

		ut_ad(zip_size
		      || page_get_max_insert_size(page, 1) == max_size);

		reorg = TRUE;

		page_cur_search(block, index, entry, PAGE_CUR_LE, page_cursor);

		*rec = page_cur_tuple_insert(page_cursor, entry, index,
					     n_ext, mtr);

		if (UNIV_UNLIKELY(!*rec)) {
			if (UNIV_LIKELY(zip_size != 0)) {

				goto fail;
			}

			fputs("InnoDB: Error: cannot insert tuple ", stderr);
			dtuple_print(stderr, entry);
			fputs(" into ", stderr);
			dict_index_name_print(stderr, thr_get_trx(thr), index);
			fprintf(stderr, "\nInnoDB: max insert size %lu\n",
				(ulong) max_size);
			ut_error;
		}
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

#ifdef BTR_CUR_HASH_ADAPT
	if (!reorg && leaf && (cursor->flag == BTR_CUR_HASH)) {
		btr_search_update_hash_node_on_insert(cursor);
	} else {
		btr_search_update_hash_on_insert(cursor);
	}
#endif

	if (!(flags & BTR_NO_LOCKING_FLAG) && inherit) {

		lock_update_insert(block, *rec);
	}

#if 0
	fprintf(stderr, "Insert into page %lu, max ins size %lu,"
		" rec %lu ind type %lu\n",
		buf_block_get_page_no(block), max_size,
		rec_size + PAGE_DIR_SLOT_SIZE, index->type);
#endif
	if (leaf
	    && !dict_index_is_clust(index)
	    && !dict_index_is_ibuf(index)) {
		/* Update the free bits of the B-tree page in the
		insert buffer bitmap. */

		/* The free bits in the insert buffer bitmap must
		never exceed the free space on a page.  It is safe to
		decrement or reset the bits in the bitmap in a
		mini-transaction that is committed before the
		mini-transaction that affects the free space. */

		/* It is unsafe to increment the bits in a separately
		committed mini-transaction, because in crash recovery,
		the free bits could momentarily be set too high. */

		if (zip_size) {
			/* Update the bits in the same mini-transaction. */
			ibuf_update_free_bits_zip(block, mtr);
		} else {
			/* Decrement the bits in a separate
			mini-transaction. */
			ibuf_update_free_bits_if_full(
				block, max_size,
				rec_size + PAGE_DIR_SLOT_SIZE);
		}
	}

	*big_rec = big_rec_vec;

	return(DB_SUCCESS);
}

/*****************************************************************
Performs an insert on a page of an index tree. It is assumed that mtr
holds an x-latch on the tree and on the cursor page. If the insert is
made on the leaf level, to avoid deadlocks, mtr must also own x-latches
to brothers of page, if those brothers exist. */
UNIV_INTERN
ulint
btr_cur_pessimistic_insert(
/*=======================*/
				/* out: DB_SUCCESS or error number */
	ulint		flags,	/* in: undo logging and locking flags: if not
				zero, the parameter thr should be
				specified; if no undo logging is specified,
				then the caller must have reserved enough
				free extents in the file space so that the
				insertion will certainly succeed */
	btr_cur_t*	cursor,	/* in: cursor after which to insert;
				cursor stays valid */
	dtuple_t*	entry,	/* in/out: entry to insert */
	rec_t**		rec,	/* out: pointer to inserted record if
				succeed */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or
				NULL */
	ulint		n_ext,	/* in: number of externally stored columns */
	que_thr_t*	thr,	/* in: query thread or NULL */
	mtr_t*		mtr)	/* in: mtr */
{
	dict_index_t*	index		= cursor->index;
	ulint		zip_size	= dict_table_zip_size(index->table);
	big_rec_t*	big_rec_vec	= NULL;
	mem_heap_t*	heap		= NULL;
	ulint		err;
	ibool		dummy_inh;
	ibool		success;
	ulint		n_extents	= 0;
	ulint		n_reserved;

	ut_ad(dtuple_check_typed(entry));

	*big_rec = NULL;

	ut_ad(mtr_memo_contains(mtr,
				dict_index_get_lock(btr_cur_get_index(cursor)),
				MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, btr_cur_get_block(cursor),
				MTR_MEMO_PAGE_X_FIX));

	/* Try first an optimistic insert; reset the cursor flag: we do not
	assume anything of how it was positioned */

	cursor->flag = BTR_CUR_BINARY;

	err = btr_cur_optimistic_insert(flags, cursor, entry, rec,
					big_rec, n_ext, thr, mtr);
	if (err != DB_FAIL) {

		return(err);
	}

	/* Retry with a pessimistic insert. Check locks and write to undo log,
	if specified */

	err = btr_cur_ins_lock_and_undo(flags, cursor, entry, thr, &dummy_inh);

	if (err != DB_SUCCESS) {

		return(err);
	}

	if (!(flags & BTR_NO_UNDO_LOG_FLAG)) {
		/* First reserve enough free space for the file segments
		of the index tree, so that the insert will not fail because
		of lack of space */

		n_extents = cursor->tree_height / 16 + 3;

		success = fsp_reserve_free_extents(&n_reserved, index->space,
						   n_extents, FSP_NORMAL, mtr);
		if (!success) {
			return(DB_OUT_OF_FILE_SPACE);
		}
	}

	if (page_zip_rec_needs_ext(rec_get_converted_size(index, entry, n_ext),
				   dict_table_is_comp(index->table),
				   dict_index_get_n_fields(index),
				   zip_size)) {
		/* The record is so big that we have to store some fields
		externally on separate database pages */

		if (UNIV_LIKELY_NULL(big_rec_vec)) {
			/* This should never happen, but we handle
			the situation in a robust manner. */
			ut_ad(0);
			dtuple_convert_back_big_rec(index, entry, big_rec_vec);
		}

		big_rec_vec = dtuple_convert_big_rec(index, entry, &n_ext);

		if (big_rec_vec == NULL) {

			if (n_extents > 0) {
				fil_space_release_free_extents(index->space,
							       n_reserved);
			}
			return(DB_TOO_BIG_RECORD);
		}
	}

	if (dict_index_get_page(index)
	    == buf_block_get_page_no(btr_cur_get_block(cursor))) {

		/* The page is the root page */
		*rec = btr_root_raise_and_insert(cursor, entry, n_ext, mtr);
	} else {
		*rec = btr_page_split_and_insert(cursor, entry, n_ext, mtr);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	ut_ad(page_rec_get_next(btr_cur_get_rec(cursor)) == *rec);

#ifdef BTR_CUR_ADAPT
	btr_search_update_hash_on_insert(cursor);
#endif
	if (!(flags & BTR_NO_LOCKING_FLAG)) {

		lock_update_insert(btr_cur_get_block(cursor), *rec);
	}

	if (n_extents > 0) {
		fil_space_release_free_extents(index->space, n_reserved);
	}

	*big_rec = big_rec_vec;

	return(DB_SUCCESS);
}

/*==================== B-TREE UPDATE =========================*/

/*****************************************************************
For an update, checks the locks and does the undo logging. */
UNIV_INLINE
ulint
btr_cur_upd_lock_and_undo(
/*======================*/
				/* out: DB_SUCCESS, DB_WAIT_LOCK, or error
				number */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor on record to update */
	const upd_t*	update,	/* in: update vector */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	dulint*		roll_ptr)/* out: roll pointer */
{
	dict_index_t*	index;
	rec_t*		rec;
	ulint		err;

	ut_ad(cursor && update && thr && roll_ptr);

	rec = btr_cur_get_rec(cursor);
	index = cursor->index;

	if (!dict_index_is_clust(index)) {
		/* We do undo logging only when we update a clustered index
		record */
		return(lock_sec_rec_modify_check_and_lock(
			       flags, btr_cur_get_block(cursor), rec,
			       index, thr));
	}

	/* Check if we have to wait for a lock: enqueue an explicit lock
	request if yes */

	err = DB_SUCCESS;

	if (!(flags & BTR_NO_LOCKING_FLAG)) {
		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		rec_offs_init(offsets_);

		err = lock_clust_rec_modify_check_and_lock(
			flags, btr_cur_get_block(cursor), rec, index,
			rec_get_offsets(rec, index, offsets_,
					ULINT_UNDEFINED, &heap), thr);
		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
		if (err != DB_SUCCESS) {

			return(err);
		}
	}

	/* Append the info about the update in the undo log */

	err = trx_undo_report_row_operation(flags, TRX_UNDO_MODIFY_OP, thr,
					    index, NULL, update,
					    cmpl_info, rec, roll_ptr);
	return(err);
}

/***************************************************************
Writes a redo log record of updating a record in-place. */
UNIV_INLINE
void
btr_cur_update_in_place_log(
/*========================*/
	ulint		flags,		/* in: flags */
	rec_t*		rec,		/* in: record */
	dict_index_t*	index,		/* in: index where cursor positioned */
	const upd_t*	update,		/* in: update vector */
	trx_t*		trx,		/* in: transaction */
	dulint		roll_ptr,	/* in: roll ptr */
	mtr_t*		mtr)		/* in: mtr */
{
	byte*	log_ptr;
	page_t*	page	= page_align(rec);
	ut_ad(flags < 256);
	ut_ad(!!page_is_comp(page) == dict_table_is_comp(index->table));

	log_ptr = mlog_open_and_write_index(mtr, rec, index, page_is_comp(page)
					    ? MLOG_COMP_REC_UPDATE_IN_PLACE
					    : MLOG_REC_UPDATE_IN_PLACE,
					    1 + DATA_ROLL_PTR_LEN + 14 + 2
					    + MLOG_BUF_MARGIN);

	if (!log_ptr) {
		/* Logging in mtr is switched off during crash recovery */
		return;
	}

	/* The code below assumes index is a clustered index: change index to
	the clustered index if we are updating a secondary index record (or we
	could as well skip writing the sys col values to the log in this case
	because they are not needed for a secondary index record update) */

	index = dict_table_get_first_index(index->table);

	mach_write_to_1(log_ptr, flags);
	log_ptr++;

	log_ptr = row_upd_write_sys_vals_to_log(index, trx, roll_ptr, log_ptr,
						mtr);
	mach_write_to_2(log_ptr, page_offset(rec));
	log_ptr += 2;

	row_upd_index_write_log(update, log_ptr, mtr);
}

/***************************************************************
Parses a redo log record of updating a record in-place. */
UNIV_INTERN
byte*
btr_cur_parse_update_in_place(
/*==========================*/
				/* out: end of log record or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	page_t*		page,	/* in/out: page or NULL */
	page_zip_des_t*	page_zip,/* in/out: compressed page, or NULL */
	dict_index_t*	index)	/* in: index corresponding to page */
{
	ulint	flags;
	rec_t*	rec;
	upd_t*	update;
	ulint	pos;
	dulint	trx_id;
	dulint	roll_ptr;
	ulint	rec_offset;
	mem_heap_t* heap;
	ulint*	offsets;

	if (end_ptr < ptr + 1) {

		return(NULL);
	}

	flags = mach_read_from_1(ptr);
	ptr++;

	ptr = row_upd_parse_sys_vals(ptr, end_ptr, &pos, &trx_id, &roll_ptr);

	if (ptr == NULL) {

		return(NULL);
	}

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	rec_offset = mach_read_from_2(ptr);
	ptr += 2;

	ut_a(rec_offset <= UNIV_PAGE_SIZE);

	heap = mem_heap_create(256);

	ptr = row_upd_index_parse(ptr, end_ptr, heap, &update);

	if (!ptr || !page) {

		goto func_exit;
	}

	ut_a((ibool)!!page_is_comp(page) == dict_table_is_comp(index->table));
	rec = page + rec_offset;

	/* We do not need to reserve btr_search_latch, as the page is only
	being recovered, and there cannot be a hash index to it. */

	offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, &heap);

	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_rec_sys_fields_in_recovery(rec, page_zip, offsets,
						   pos, trx_id, roll_ptr);
	}

	row_upd_rec_in_place(rec, index, offsets, update, page_zip);

func_exit:
	mem_heap_free(heap);

	return(ptr);
}

/*****************************************************************
See if there is enough place in the page modification log to log
an update-in-place. */
static
ibool
btr_cur_update_alloc_zip(
/*=====================*/
				/* out: TRUE if enough place */
	page_zip_des_t*	page_zip,/* in/out: compressed page */
	buf_block_t*	block,	/* in/out: buffer page */
	dict_index_t*	index,	/* in: the index corresponding to the block */
	ulint		length,	/* in: size needed */
	mtr_t*		mtr)	/* in: mini-transaction */
{
	ut_a(page_zip == buf_block_get_page_zip(block));
	ut_ad(page_zip);
	ut_ad(!dict_index_is_ibuf(index));

	if (page_zip_available(page_zip, dict_index_is_clust(index),
			       length, 0)) {
		return(TRUE);
	}

	if (!page_zip->m_nonempty) {
		/* The page has been freshly compressed, so
		recompressing it will not help. */
		return(FALSE);
	}

	if (!page_zip_compress(page_zip, buf_block_get_frame(block),
			       index, mtr)) {
		/* Unable to compress the page */
		return(FALSE);
	}

	/* After recompressing a page, we must make sure that the free
	bits in the insert buffer bitmap will not exceed the free
	space on the page.  Because this function will not attempt
	recompression unless page_zip_available() fails above, it is
	safe to reset the free bits if page_zip_available() fails
	again, below.  The free bits can safely be reset in a separate
	mini-transaction.  If page_zip_available() succeeds below, we
	can be sure that the page_zip_compress() above did not reduce
	the free space available on the page. */

	if (!page_zip_available(page_zip, dict_index_is_clust(index),
				length, 0)) {
		/* Out of space: reset the free bits. */
		if (!dict_index_is_clust(index)
		    && page_is_leaf(buf_block_get_frame(block))) {
			ibuf_reset_free_bits(block);
		}
		return(FALSE);
	}

	return(TRUE);
}

/*****************************************************************
Updates a record when the update causes no size changes in its fields.
We assume here that the ordering fields of the record do not change. */
UNIV_INTERN
ulint
btr_cur_update_in_place(
/*====================*/
				/* out: DB_SUCCESS or error number */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	const upd_t*	update,	/* in: update vector */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr; must be committed before
				latching any further pages */
{
	dict_index_t*	index;
	buf_block_t*	block;
	page_zip_des_t*	page_zip;
	ulint		err;
	rec_t*		rec;
	dulint		roll_ptr	= ut_dulint_zero;
	trx_t*		trx;
	ulint		was_delete_marked;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));
	/* The insert buffer tree should never be updated in place. */
	ut_ad(!dict_index_is_ibuf(index));

	trx = thr_get_trx(thr);
	offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);
#ifdef UNIV_DEBUG
	if (btr_cur_print_record_ops && thr) {
		btr_cur_trx_report(trx, index, "update ");
		rec_print_new(stderr, rec, offsets);
	}
#endif /* UNIV_DEBUG */

	block = btr_cur_get_block(cursor);
	page_zip = buf_block_get_page_zip(block);

	/* Check that enough space is available on the compressed page. */
	if (UNIV_LIKELY_NULL(page_zip)
	    && !btr_cur_update_alloc_zip(page_zip, block, index,
					 rec_offs_size(offsets), mtr)) {
		return(DB_ZIP_OVERFLOW);
	}

	/* Do lock checking and undo logging */
	err = btr_cur_upd_lock_and_undo(flags, cursor, update, cmpl_info,
					thr, &roll_ptr);
	if (UNIV_UNLIKELY(err != DB_SUCCESS)) {

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
		return(err);
	}

	if (block->is_hashed) {
		/* The function row_upd_changes_ord_field_binary works only
		if the update vector was built for a clustered index, we must
		NOT call it if index is secondary */

		if (!dict_index_is_clust(index)
		    || row_upd_changes_ord_field_binary(NULL, index, update)) {

			/* Remove possible hash index pointer to this record */
			btr_search_update_hash_on_delete(cursor);
		}

		rw_lock_x_lock(&btr_search_latch);
	}

	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_rec_sys_fields(rec, NULL,
				       index, offsets, trx, roll_ptr);
	}

	was_delete_marked = rec_get_deleted_flag(
		rec, page_is_comp(buf_block_get_frame(block)));

	row_upd_rec_in_place(rec, index, offsets, update, page_zip);

	if (block->is_hashed) {
		rw_lock_x_unlock(&btr_search_latch);
	}

	if (page_zip && !dict_index_is_clust(index)
	    && page_is_leaf(buf_block_get_frame(block))) {
		/* Update the free bits in the insert buffer. */
		ibuf_update_free_bits_zip(block, mtr);
	}

	btr_cur_update_in_place_log(flags, rec, index, update,
				    trx, roll_ptr, mtr);

	if (was_delete_marked
	    && !rec_get_deleted_flag(rec, page_is_comp(
					     buf_block_get_frame(block)))) {
		/* The new updated record owns its possible externally
		stored fields */

		btr_cur_unmark_extern_fields(page_zip,
					     rec, index, offsets, mtr);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(DB_SUCCESS);
}

/*****************************************************************
Tries to update a record on a page in an index tree. It is assumed that mtr
holds an x-latch on the page. The operation does not succeed if there is too
little space on the page or if the update would result in too empty a page,
so that tree compression is recommended. We assume here that the ordering
fields of the record do not change. */
UNIV_INTERN
ulint
btr_cur_optimistic_update(
/*======================*/
				/* out: DB_SUCCESS, or DB_OVERFLOW if the
				updated record does not fit, DB_UNDERFLOW
				if the page would become too empty, or
				DB_ZIP_OVERFLOW if there is not enough
				space left on the compressed page */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	const upd_t*	update,	/* in: update vector; this must also
				contain trx id and roll ptr fields */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr; must be committed before
				latching any further pages */
{
	dict_index_t*	index;
	page_cur_t*	page_cursor;
	ulint		err;
	buf_block_t*	block;
	page_t*		page;
	page_zip_des_t*	page_zip;
	rec_t*		rec;
	rec_t*		orig_rec;
	ulint		max_size;
	ulint		new_rec_size;
	ulint		old_rec_size;
	dtuple_t*	new_entry;
	dulint		roll_ptr;
	trx_t*		trx;
	mem_heap_t*	heap;
	ulint		i;
	ulint		n_ext;
	ulint*		offsets;

	block = btr_cur_get_block(cursor);
	page = buf_block_get_frame(block);
	orig_rec = rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));
	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	/* The insert buffer tree should never be updated in place. */
	ut_ad(!dict_index_is_ibuf(index));

	heap = mem_heap_create(1024);
	offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, &heap);

#ifdef UNIV_DEBUG
	if (btr_cur_print_record_ops && thr) {
		btr_cur_trx_report(thr_get_trx(thr), index, "update ");
		rec_print_new(stderr, rec, offsets);
	}
#endif /* UNIV_DEBUG */

	if (!row_upd_changes_field_size_or_external(index, offsets, update)) {

		/* The simplest and the most common case: the update does not
		change the size of any field and none of the updated fields is
		externally stored in rec or update, and there is enough space
		on the compressed page to log the update. */

		mem_heap_free(heap);
		return(btr_cur_update_in_place(flags, cursor, update,
					       cmpl_info, thr, mtr));
	}

	if (rec_offs_any_extern(offsets)) {
any_extern:
		/* Externally stored fields are treated in pessimistic
		update */

		mem_heap_free(heap);
		return(DB_OVERFLOW);
	}

	for (i = 0; i < upd_get_n_fields(update); i++) {
		if (dfield_is_ext(&upd_get_nth_field(update, i)->new_val)) {

			goto any_extern;
		}
	}

	page_cursor = btr_cur_get_page_cur(cursor);

	new_entry = row_rec_to_index_entry(ROW_COPY_DATA, rec, index, offsets,
					   &n_ext, heap);
	/* We checked above that there are no externally stored fields. */
	ut_a(!n_ext);

	/* The page containing the clustered index record
	corresponding to new_entry is latched in mtr.
	Thus the following call is safe. */
	row_upd_index_replace_new_col_vals_index_pos(new_entry, index, update,
						     FALSE, heap);
	old_rec_size = rec_offs_size(offsets);
	new_rec_size = rec_get_converted_size(index, new_entry, 0);

	page_zip = buf_block_get_page_zip(block);
#ifdef UNIV_ZIP_DEBUG
	ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */

	if (UNIV_LIKELY_NULL(page_zip)
	    && !btr_cur_update_alloc_zip(page_zip, block, index,
					 new_rec_size, mtr)) {
		err = DB_ZIP_OVERFLOW;
		goto err_exit;
	}

	if (UNIV_UNLIKELY(new_rec_size
			  >= (page_get_free_space_of_empty(page_is_comp(page))
			      / 2))) {

		err = DB_OVERFLOW;
		goto err_exit;
	}

	if (UNIV_UNLIKELY(page_get_data_size(page)
			  - old_rec_size + new_rec_size
			  < BTR_CUR_PAGE_COMPRESS_LIMIT)) {

		/* The page would become too empty */

		err = DB_UNDERFLOW;
		goto err_exit;
	}

	max_size = old_rec_size
		+ page_get_max_insert_size_after_reorganize(page, 1);

	if (!(((max_size >= BTR_CUR_PAGE_REORGANIZE_LIMIT)
	       && (max_size >= new_rec_size))
	      || (page_get_n_recs(page) <= 1))) {

		/* There was not enough space, or it did not pay to
		reorganize: for simplicity, we decide what to do assuming a
		reorganization is needed, though it might not be necessary */

		err = DB_OVERFLOW;
		goto err_exit;
	}

	/* Do lock checking and undo logging */
	err = btr_cur_upd_lock_and_undo(flags, cursor, update, cmpl_info, thr,
					&roll_ptr);
	if (err != DB_SUCCESS) {
err_exit:
		mem_heap_free(heap);
		return(err);
	}

	/* Ok, we may do the replacement. Store on the page infimum the
	explicit locks on rec, before deleting rec (see the comment in
	btr_cur_pessimistic_update). */

	lock_rec_store_on_page_infimum(block, rec);

	btr_search_update_hash_on_delete(cursor);

	/* The call to row_rec_to_index_entry(ROW_COPY_DATA, ...) above
	invokes rec_offs_make_valid() to point to the copied record that
	the fields of new_entry point to.  We have to undo it here. */
	ut_ad(rec_offs_validate(NULL, index, offsets));
	rec_offs_make_valid(page_cur_get_rec(page_cursor), index, offsets);

	page_cur_delete_rec(page_cursor, index, offsets, mtr);

	page_cur_move_to_prev(page_cursor);

	trx = thr_get_trx(thr);

	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_index_entry_sys_field(new_entry, index, DATA_ROLL_PTR,
					      roll_ptr);
		row_upd_index_entry_sys_field(new_entry, index, DATA_TRX_ID,
					      trx->id);
	}

	/* There are no externally stored columns in new_entry */
	rec = btr_cur_insert_if_possible(cursor, new_entry, 0/*n_ext*/, mtr);
	ut_a(rec); /* <- We calculated above the insert would fit */

	if (page_zip && !dict_index_is_clust(index)
	    && page_is_leaf(page)) {
		/* Update the free bits in the insert buffer. */
		ibuf_update_free_bits_zip(block, mtr);
	}

	/* Restore the old explicit lock state on the record */

	lock_rec_restore_from_page_infimum(block, rec, block);

	page_cur_move_to_next(page_cursor);

	mem_heap_free(heap);

	return(DB_SUCCESS);
}

/*****************************************************************
If, in a split, a new supremum record was created as the predecessor of the
updated record, the supremum record must inherit exactly the locks on the
updated record. In the split it may have inherited locks from the successor
of the updated record, which is not correct. This function restores the
right locks for the new supremum. */
static
void
btr_cur_pess_upd_restore_supremum(
/*==============================*/
	buf_block_t*	block,	/* in: buffer block of rec */
	const rec_t*	rec,	/* in: updated record */
	mtr_t*		mtr)	/* in: mtr */
{
	page_t*		page;
	buf_block_t*	prev_block;
	ulint		space;
	ulint		zip_size;
	ulint		prev_page_no;

	page = buf_block_get_frame(block);

	if (page_rec_get_next(page_get_infimum_rec(page)) != rec) {
		/* Updated record is not the first user record on its page */

		return;
	}

	space = buf_block_get_space(block);
	zip_size = buf_block_get_zip_size(block);
	prev_page_no = btr_page_get_prev(page, mtr);

	ut_ad(prev_page_no != FIL_NULL);
	prev_block = buf_page_get_with_no_latch(space, zip_size,
						prev_page_no, mtr);
#ifdef UNIV_BTR_DEBUG
	ut_a(btr_page_get_next(prev_block->frame, mtr)
	     == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

	/* We must already have an x-latch on prev_block! */
	ut_ad(mtr_memo_contains(mtr, prev_block, MTR_MEMO_PAGE_X_FIX));

	lock_rec_reset_and_inherit_gap_locks(prev_block, block,
					     PAGE_HEAP_NO_SUPREMUM,
					     page_rec_get_heap_no(rec));
}

/*****************************************************************
Performs an update of a record on a page of a tree. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. If the
update is made on the leaf level, to avoid deadlocks, mtr must also
own x-latches to brothers of page, if those brothers exist. We assume
here that the ordering fields of the record do not change. */
UNIV_INTERN
ulint
btr_cur_pessimistic_update(
/*=======================*/
				/* out: DB_SUCCESS or error code */
	ulint		flags,	/* in: undo logging, locking, and rollback
				flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update */
	mem_heap_t**	heap,	/* in/out: pointer to memory heap, or NULL */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or NULL */
	const upd_t*	update,	/* in: update vector; this is allowed also
				contain trx id and roll ptr fields, but
				the values in update vector have no effect */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr; must be committed before
				latching any further pages */
{
	big_rec_t*	big_rec_vec	= NULL;
	big_rec_t*	dummy_big_rec;
	dict_index_t*	index;
	buf_block_t*	block;
	page_t*		page;
	page_zip_des_t*	page_zip;
	rec_t*		rec;
	page_cur_t*	page_cursor;
	dtuple_t*	new_entry;
	ulint		err;
	ulint		optim_err;
	dulint		roll_ptr;
	trx_t*		trx;
	ibool		was_first;
	ulint		n_extents	= 0;
	ulint		n_reserved;
	ulint		n_ext;
	ulint*		offsets		= NULL;

	*big_rec = NULL;

	block = btr_cur_get_block(cursor);
	page = buf_block_get_frame(block);
	page_zip = buf_block_get_page_zip(block);
	rec = btr_cur_get_rec(cursor);
	index = cursor->index;

	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
#ifdef UNIV_ZIP_DEBUG
	ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */
	/* The insert buffer tree should never be updated in place. */
	ut_ad(!dict_index_is_ibuf(index));

	optim_err = btr_cur_optimistic_update(flags, cursor, update,
					      cmpl_info, thr, mtr);

	switch (optim_err) {
	case DB_UNDERFLOW:
	case DB_OVERFLOW:
	case DB_ZIP_OVERFLOW:
		break;
	default:
		return(optim_err);
	}

	/* Do lock checking and undo logging */
	err = btr_cur_upd_lock_and_undo(flags, cursor, update, cmpl_info,
					thr, &roll_ptr);
	if (err != DB_SUCCESS) {

		return(err);
	}

	if (optim_err == DB_OVERFLOW) {
		ulint	reserve_flag;

		/* First reserve enough free space for the file segments
		of the index tree, so that the update will not fail because
		of lack of space */

		n_extents = cursor->tree_height / 16 + 3;

		if (flags & BTR_NO_UNDO_LOG_FLAG) {
			reserve_flag = FSP_CLEANING;
		} else {
			reserve_flag = FSP_NORMAL;
		}

		if (!fsp_reserve_free_extents(&n_reserved, index->space,
					      n_extents, reserve_flag, mtr)) {
			return(DB_OUT_OF_FILE_SPACE);
		}
	}

	if (!*heap) {
		*heap = mem_heap_create(1024);
	}
	offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, heap);

	trx = thr_get_trx(thr);

	new_entry = row_rec_to_index_entry(ROW_COPY_DATA, rec, index, offsets,
					   &n_ext, *heap);
	/* The call to row_rec_to_index_entry(ROW_COPY_DATA, ...) above
	invokes rec_offs_make_valid() to point to the copied record that
	the fields of new_entry point to.  We have to undo it here. */
	ut_ad(rec_offs_validate(NULL, index, offsets));
	rec_offs_make_valid(rec, index, offsets);

	/* The page containing the clustered index record
	corresponding to new_entry is latched in mtr.  If the
	clustered index record is delete-marked, then its externally
	stored fields cannot have been purged yet, because then the
	purge would also have removed the clustered index record
	itself.  Thus the following call is safe. */
	row_upd_index_replace_new_col_vals_index_pos(new_entry, index, update,
						     FALSE, *heap);
	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_index_entry_sys_field(new_entry, index, DATA_ROLL_PTR,
					      roll_ptr);
		row_upd_index_entry_sys_field(new_entry, index, DATA_TRX_ID,
					      trx->id);
	}

	if ((flags & BTR_NO_UNDO_LOG_FLAG) && rec_offs_any_extern(offsets)) {
		/* We are in a transaction rollback undoing a row
		update: we must free possible externally stored fields
		which got new values in the update, if they are not
		inherited values. They can be inherited if we have
		updated the primary key to another value, and then
		update it back again. */

		ut_ad(big_rec_vec == NULL);

		btr_rec_free_updated_extern_fields(
			index, rec, page_zip, offsets, update,
			trx_is_recv(trx) ? RB_RECOVERY : RB_NORMAL, mtr);
	}

	/* We have to set appropriate extern storage bits in the new
	record to be inserted: we have to remember which fields were such */

	ut_ad(!page_is_comp(page) || !rec_get_node_ptr_flag(rec));
	offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, heap);
	n_ext += btr_push_update_extern_fields(new_entry, update, *heap);

	if (UNIV_LIKELY_NULL(page_zip)) {
		ut_ad(page_is_comp(page));
		if (page_zip_rec_needs_ext(
			    rec_get_converted_size(index, new_entry, n_ext),
			    TRUE,
			    dict_index_get_n_fields(index),
			    page_zip_get_size(page_zip))) {

			goto make_external;
		}
	} else if (page_zip_rec_needs_ext(
			   rec_get_converted_size(index, new_entry, n_ext),
			   page_is_comp(page), 0, 0)) {
make_external:
		big_rec_vec = dtuple_convert_big_rec(index, new_entry, &n_ext);
		if (UNIV_UNLIKELY(big_rec_vec == NULL)) {

			err = DB_TOO_BIG_RECORD;
			goto return_after_reservations;
		}
	}

	/* Store state of explicit locks on rec on the page infimum record,
	before deleting rec. The page infimum acts as a dummy carrier of the
	locks, taking care also of lock releases, before we can move the locks
	back on the actual record. There is a special case: if we are
	inserting on the root page and the insert causes a call of
	btr_root_raise_and_insert. Therefore we cannot in the lock system
	delete the lock structs set on the root page even if the root
	page carries just node pointers. */

	lock_rec_store_on_page_infimum(block, rec);

	btr_search_update_hash_on_delete(cursor);

#ifdef UNIV_ZIP_DEBUG
	ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */
	page_cursor = btr_cur_get_page_cur(cursor);

	page_cur_delete_rec(page_cursor, index, offsets, mtr);

	page_cur_move_to_prev(page_cursor);

	rec = btr_cur_insert_if_possible(cursor, new_entry, n_ext, mtr);

	if (rec) {
		lock_rec_restore_from_page_infimum(btr_cur_get_block(cursor),
						   rec, block);

		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, heap);

		if (!rec_get_deleted_flag(rec, rec_offs_comp(offsets))) {
			/* The new inserted record owns its possible externally
			stored fields */
			btr_cur_unmark_extern_fields(page_zip,
						     rec, index, offsets, mtr);
		}

		btr_cur_compress_if_useful(cursor, mtr);

		if (page_zip && !dict_index_is_clust(index)
		    && page_is_leaf(page)) {
			/* Update the free bits in the insert buffer. */
			ibuf_update_free_bits_zip(block, mtr);
		}

		err = DB_SUCCESS;
		goto return_after_reservations;
	} else {
		ut_a(optim_err != DB_UNDERFLOW);

		/* Out of space: reset the free bits. */
		if (!dict_index_is_clust(index)
		    && page_is_leaf(page)) {
			ibuf_reset_free_bits(block);
		}
	}

	/* Was the record to be updated positioned as the first user
	record on its page? */
	was_first = page_cur_is_before_first(page_cursor);

	/* The first parameter means that no lock checking and undo logging
	is made in the insert */

	err = btr_cur_pessimistic_insert(BTR_NO_UNDO_LOG_FLAG
					 | BTR_NO_LOCKING_FLAG
					 | BTR_KEEP_SYS_FLAG,
					 cursor, new_entry, &rec,
					 &dummy_big_rec, n_ext, NULL, mtr);
	ut_a(rec);
	ut_a(err == DB_SUCCESS);
	ut_a(dummy_big_rec == NULL);

	if (!rec_get_deleted_flag(rec, rec_offs_comp(offsets))) {
		/* The new inserted record owns its possible externally
		stored fields */
		buf_block_t*	rec_block = btr_cur_get_block(cursor);

#ifdef UNIV_ZIP_DEBUG
		ut_a(!page_zip || page_zip_validate(page_zip, page));
		page = buf_block_get_frame(rec_block);
#endif /* UNIV_ZIP_DEBUG */
		page_zip = buf_block_get_page_zip(rec_block);

		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, heap);
		btr_cur_unmark_extern_fields(page_zip,
					     rec, index, offsets, mtr);
	}

	lock_rec_restore_from_page_infimum(btr_cur_get_block(cursor),
					   rec, block);

	/* If necessary, restore also the correct lock state for a new,
	preceding supremum record created in a page split. While the old
	record was nonexistent, the supremum might have inherited its locks
	from a wrong record. */

	if (!was_first) {
		btr_cur_pess_upd_restore_supremum(btr_cur_get_block(cursor),
						  rec, mtr);
	}

return_after_reservations:
#ifdef UNIV_ZIP_DEBUG
	ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */

	if (n_extents > 0) {
		fil_space_release_free_extents(index->space, n_reserved);
	}

	*big_rec = big_rec_vec;

	return(err);
}

/*==================== B-TREE DELETE MARK AND UNMARK ===============*/

/********************************************************************
Writes the redo log record for delete marking or unmarking of an index
record. */
UNIV_INLINE
void
btr_cur_del_mark_set_clust_rec_log(
/*===============================*/
	ulint		flags,	/* in: flags */
	rec_t*		rec,	/* in: record */
	dict_index_t*	index,	/* in: index of the record */
	ibool		val,	/* in: value to set */
	trx_t*		trx,	/* in: deleting transaction */
	dulint		roll_ptr,/* in: roll ptr to the undo log record */
	mtr_t*		mtr)	/* in: mtr */
{
	byte*	log_ptr;
	ut_ad(flags < 256);
	ut_ad(val <= 1);

	ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));

	log_ptr = mlog_open_and_write_index(mtr, rec, index,
					    page_rec_is_comp(rec)
					    ? MLOG_COMP_REC_CLUST_DELETE_MARK
					    : MLOG_REC_CLUST_DELETE_MARK,
					    1 + 1 + DATA_ROLL_PTR_LEN
					    + 14 + 2);

	if (!log_ptr) {
		/* Logging in mtr is switched off during crash recovery */
		return;
	}

	mach_write_to_1(log_ptr, flags);
	log_ptr++;
	mach_write_to_1(log_ptr, val);
	log_ptr++;

	log_ptr = row_upd_write_sys_vals_to_log(index, trx, roll_ptr, log_ptr,
						mtr);
	mach_write_to_2(log_ptr, page_offset(rec));
	log_ptr += 2;

	mlog_close(mtr, log_ptr);
}

/********************************************************************
Parses the redo log record for delete marking or unmarking of a clustered
index record. */
UNIV_INTERN
byte*
btr_cur_parse_del_mark_set_clust_rec(
/*=================================*/
				/* out: end of log record or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	page_t*		page,	/* in/out: page or NULL */
	page_zip_des_t*	page_zip,/* in/out: compressed page, or NULL */
	dict_index_t*	index)	/* in: index corresponding to page */
{
	ulint	flags;
	ulint	val;
	ulint	pos;
	dulint	trx_id;
	dulint	roll_ptr;
	ulint	offset;
	rec_t*	rec;

	ut_ad(!page
	      || !!page_is_comp(page) == dict_table_is_comp(index->table));

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	flags = mach_read_from_1(ptr);
	ptr++;
	val = mach_read_from_1(ptr);
	ptr++;

	ptr = row_upd_parse_sys_vals(ptr, end_ptr, &pos, &trx_id, &roll_ptr);

	if (ptr == NULL) {

		return(NULL);
	}

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	offset = mach_read_from_2(ptr);
	ptr += 2;

	ut_a(offset <= UNIV_PAGE_SIZE);

	if (page) {
		rec = page + offset;

		/* We do not need to reserve btr_search_latch, as the page
		is only being recovered, and there cannot be a hash index to
		it. */

		btr_rec_set_deleted_flag(rec, page_zip, val);

		if (!(flags & BTR_KEEP_SYS_FLAG)) {
			mem_heap_t*	heap		= NULL;
			ulint		offsets_[REC_OFFS_NORMAL_SIZE];
			rec_offs_init(offsets_);

			row_upd_rec_sys_fields_in_recovery(
				rec, page_zip,
				rec_get_offsets(rec, index, offsets_,
						ULINT_UNDEFINED, &heap),
				pos, trx_id, roll_ptr);
			if (UNIV_LIKELY_NULL(heap)) {
				mem_heap_free(heap);
			}
		}
	}

	return(ptr);
}

/***************************************************************
Marks a clustered index record deleted. Writes an undo log record to
undo log on this delete marking. Writes in the trx id field the id
of the deleting transaction, and in the roll ptr field pointer to the
undo log record created. */
UNIV_INTERN
ulint
btr_cur_del_mark_set_clust_rec(
/*===========================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, or error
				number */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor */
	ibool		val,	/* in: value to set */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	dict_index_t*	index;
	buf_block_t*	block;
	dulint		roll_ptr;
	ulint		err;
	rec_t*		rec;
	page_zip_des_t*	page_zip;
	trx_t*		trx;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));
	offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

#ifdef UNIV_DEBUG
	if (btr_cur_print_record_ops && thr) {
		btr_cur_trx_report(thr_get_trx(thr), index, "del mark ");
		rec_print_new(stderr, rec, offsets);
	}
#endif /* UNIV_DEBUG */

	ut_ad(dict_index_is_clust(index));
	ut_ad(!rec_get_deleted_flag(rec, rec_offs_comp(offsets)));

	err = lock_clust_rec_modify_check_and_lock(flags,
						   btr_cur_get_block(cursor),
						   rec, index, offsets, thr);

	if (err != DB_SUCCESS) {

		goto func_exit;
	}

	err = trx_undo_report_row_operation(flags, TRX_UNDO_MODIFY_OP, thr,
					    index, NULL, NULL, 0, rec,
					    &roll_ptr);
	if (err != DB_SUCCESS) {

		goto func_exit;
	}

	block = btr_cur_get_block(cursor);

	if (block->is_hashed) {
		rw_lock_x_lock(&btr_search_latch);
	}

	page_zip = buf_block_get_page_zip(block);

	btr_rec_set_deleted_flag(rec, page_zip, val);

	trx = thr_get_trx(thr);

	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_rec_sys_fields(rec, page_zip,
				       index, offsets, trx, roll_ptr);
	}

	if (block->is_hashed) {
		rw_lock_x_unlock(&btr_search_latch);
	}

	btr_cur_del_mark_set_clust_rec_log(flags, rec, index, val, trx,
					   roll_ptr, mtr);

func_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(err);
}

/********************************************************************
Writes the redo log record for a delete mark setting of a secondary
index record. */
UNIV_INLINE
void
btr_cur_del_mark_set_sec_rec_log(
/*=============================*/
	rec_t*		rec,	/* in: record */
	ibool		val,	/* in: value to set */
	mtr_t*		mtr)	/* in: mtr */
{
	byte*	log_ptr;
	ut_ad(val <= 1);

	log_ptr = mlog_open(mtr, 11 + 1 + 2);

	if (!log_ptr) {
		/* Logging in mtr is switched off during crash recovery:
		in that case mlog_open returns NULL */
		return;
	}

	log_ptr = mlog_write_initial_log_record_fast(
		rec, MLOG_REC_SEC_DELETE_MARK, log_ptr, mtr);
	mach_write_to_1(log_ptr, val);
	log_ptr++;

	mach_write_to_2(log_ptr, page_offset(rec));
	log_ptr += 2;

	mlog_close(mtr, log_ptr);
}

/********************************************************************
Parses the redo log record for delete marking or unmarking of a secondary
index record. */
UNIV_INTERN
byte*
btr_cur_parse_del_mark_set_sec_rec(
/*===============================*/
				/* out: end of log record or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	page_t*		page,	/* in/out: page or NULL */
	page_zip_des_t*	page_zip)/* in/out: compressed page, or NULL */
{
	ulint	val;
	ulint	offset;
	rec_t*	rec;

	if (end_ptr < ptr + 3) {

		return(NULL);
	}

	val = mach_read_from_1(ptr);
	ptr++;

	offset = mach_read_from_2(ptr);
	ptr += 2;

	ut_a(offset <= UNIV_PAGE_SIZE);

	if (page) {
		rec = page + offset;

		/* We do not need to reserve btr_search_latch, as the page
		is only being recovered, and there cannot be a hash index to
		it. */

		btr_rec_set_deleted_flag(rec, page_zip, val);
	}

	return(ptr);
}

/***************************************************************
Sets a secondary index record delete mark to TRUE or FALSE. */
UNIV_INTERN
ulint
btr_cur_del_mark_set_sec_rec(
/*=========================*/
				/* out: DB_SUCCESS, DB_LOCK_WAIT, or error
				number */
	ulint		flags,	/* in: locking flag */
	btr_cur_t*	cursor,	/* in: cursor */
	ibool		val,	/* in: value to set */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	buf_block_t*	block;
	rec_t*		rec;
	ulint		err;

	block = btr_cur_get_block(cursor);
	rec = btr_cur_get_rec(cursor);

#ifdef UNIV_DEBUG
	if (btr_cur_print_record_ops && thr) {
		btr_cur_trx_report(thr_get_trx(thr), cursor->index,
				   "del mark ");
		rec_print(stderr, rec, cursor->index);
	}
#endif /* UNIV_DEBUG */

	err = lock_sec_rec_modify_check_and_lock(flags,
						 btr_cur_get_block(cursor),
						 rec, cursor->index, thr);
	if (err != DB_SUCCESS) {

		return(err);
	}

	ut_ad(!!page_rec_is_comp(rec)
	      == dict_table_is_comp(cursor->index->table));

	if (block->is_hashed) {
		rw_lock_x_lock(&btr_search_latch);
	}

	btr_rec_set_deleted_flag(rec, buf_block_get_page_zip(block), val);

	if (block->is_hashed) {
		rw_lock_x_unlock(&btr_search_latch);
	}

	btr_cur_del_mark_set_sec_rec_log(rec, val, mtr);

	return(DB_SUCCESS);
}

/***************************************************************
Clear a secondary index record's delete mark.  This function is only
used by the insert buffer insert merge mechanism. */
UNIV_INTERN
void
btr_cur_del_unmark_for_ibuf(
/*========================*/
	rec_t*		rec,		/* in/out: record to delete unmark */
	page_zip_des_t*	page_zip,	/* in/out: compressed page
					corresponding to rec, or NULL
					when the tablespace is
					uncompressed */
	mtr_t*		mtr)		/* in: mtr */
{
	/* We do not need to reserve btr_search_latch, as the page has just
	been read to the buffer pool and there cannot be a hash index to it. */

	btr_rec_set_deleted_flag(rec, page_zip, FALSE);

	btr_cur_del_mark_set_sec_rec_log(rec, FALSE, mtr);
}

/*==================== B-TREE RECORD REMOVE =========================*/

/*****************************************************************
Tries to compress a page of the tree if it seems useful. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. To avoid
deadlocks, mtr must also own x-latches to brothers of page, if those
brothers exist. NOTE: it is assumed that the caller has reserved enough
free extents so that the compression will always succeed if done! */
UNIV_INTERN
ibool
btr_cur_compress_if_useful(
/*=======================*/
				/* out: TRUE if compression occurred */
	btr_cur_t*	cursor,	/* in: cursor on the page to compress;
				cursor does not stay valid if compression
				occurs */
	mtr_t*		mtr)	/* in: mtr */
{
	ut_ad(mtr_memo_contains(mtr,
				dict_index_get_lock(btr_cur_get_index(cursor)),
				MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, btr_cur_get_block(cursor),
				MTR_MEMO_PAGE_X_FIX));

	return(btr_cur_compress_recommendation(cursor, mtr)
	       && btr_compress(cursor, mtr));
}

/***********************************************************
Removes the record on which the tree cursor is positioned on a leaf page.
It is assumed that the mtr has an x-latch on the page where the cursor is
positioned, but no latch on the whole tree. */
UNIV_INTERN
ibool
btr_cur_optimistic_delete(
/*======================*/
				/* out: TRUE if success, i.e., the page
				did not become too empty */
	btr_cur_t*	cursor,	/* in: cursor on leaf page, on the record to
				delete; cursor stays valid: if deletion
				succeeds, on function exit it points to the
				successor of the deleted record */
	mtr_t*		mtr)	/* in: mtr; if this function returns
				TRUE on a leaf page of a secondary
				index, the mtr must be committed
				before latching any further pages */
{
	buf_block_t*	block;
	rec_t*		rec;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	ibool		no_compress_needed;
	rec_offs_init(offsets_);

	ut_ad(mtr_memo_contains(mtr, btr_cur_get_block(cursor),
				MTR_MEMO_PAGE_X_FIX));
	/* This is intended only for leaf page deletions */

	block = btr_cur_get_block(cursor);

	ut_ad(page_is_leaf(buf_block_get_frame(block)));

	rec = btr_cur_get_rec(cursor);
	offsets = rec_get_offsets(rec, cursor->index, offsets,
				  ULINT_UNDEFINED, &heap);

	no_compress_needed = !rec_offs_any_extern(offsets)
		&& btr_cur_can_delete_without_compress(
			cursor, rec_offs_size(offsets), mtr);

	if (no_compress_needed) {

		page_t*		page	= buf_block_get_frame(block);
		page_zip_des_t*	page_zip= buf_block_get_page_zip(block);
		ulint		max_ins	= 0;

		lock_update_delete(block, rec);

		btr_search_update_hash_on_delete(cursor);

		if (!page_zip) {
			max_ins = page_get_max_insert_size_after_reorganize(
				page, 1);
		}
#ifdef UNIV_ZIP_DEBUG
		ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */
		page_cur_delete_rec(btr_cur_get_page_cur(cursor),
				    cursor->index, offsets, mtr);
#ifdef UNIV_ZIP_DEBUG
		ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */

		if (dict_index_is_clust(cursor->index)
		    || dict_index_is_ibuf(cursor->index)
		    || !page_is_leaf(page)) {
			/* The insert buffer does not handle
			inserts to clustered indexes, to
			non-leaf pages of secondary index B-trees,
			or to the insert buffer. */
		} else if (page_zip) {
			ibuf_update_free_bits_zip(block, mtr);
		} else {
			ibuf_update_free_bits_low(block, max_ins, mtr);
		}
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	return(no_compress_needed);
}

/*****************************************************************
Removes the record on which the tree cursor is positioned. Tries
to compress the page if its fillfactor drops below a threshold
or if it is the only page on the level. It is assumed that mtr holds
an x-latch on the tree and on the cursor page. To avoid deadlocks,
mtr must also own x-latches to brothers of page, if those brothers
exist. */
UNIV_INTERN
ibool
btr_cur_pessimistic_delete(
/*=======================*/
				/* out: TRUE if compression occurred */
	ulint*		err,	/* out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE;
				the latter may occur because we may have
				to update node pointers on upper levels,
				and in the case of variable length keys
				these may actually grow in size */
	ibool		has_reserved_extents, /* in: TRUE if the
				caller has already reserved enough free
				extents so that he knows that the operation
				will succeed */
	btr_cur_t*	cursor,	/* in: cursor on the record to delete;
				if compression does not occur, the cursor
				stays valid: it points to successor of
				deleted record on function exit */
	enum trx_rb_ctx	rb_ctx,	/* in: rollback context */
	mtr_t*		mtr)	/* in: mtr */
{
	buf_block_t*	block;
	page_t*		page;
	page_zip_des_t*	page_zip;
	dict_index_t*	index;
	rec_t*		rec;
	dtuple_t*	node_ptr;
	ulint		n_extents	= 0;
	ulint		n_reserved;
	ibool		success;
	ibool		ret		= FALSE;
	ulint		level;
	mem_heap_t*	heap;
	ulint*		offsets;

	block = btr_cur_get_block(cursor);
	page = buf_block_get_frame(block);
	index = btr_cur_get_index(cursor);

	ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
	if (!has_reserved_extents) {
		/* First reserve enough free space for the file segments
		of the index tree, so that the node pointer updates will
		not fail because of lack of space */

		n_extents = cursor->tree_height / 32 + 1;

		success = fsp_reserve_free_extents(&n_reserved,
						   index->space,
						   n_extents,
						   FSP_CLEANING, mtr);
		if (!success) {
			*err = DB_OUT_OF_FILE_SPACE;

			return(FALSE);
		}
	}

	heap = mem_heap_create(1024);
	rec = btr_cur_get_rec(cursor);
	page_zip = buf_block_get_page_zip(block);
#ifdef UNIV_ZIP_DEBUG
	ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */

	offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, &heap);

	if (rec_offs_any_extern(offsets)) {
		btr_rec_free_externally_stored_fields(index,
						      rec, offsets, page_zip,
						      rb_ctx, mtr);
#ifdef UNIV_ZIP_DEBUG
		ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */
	}

	if (UNIV_UNLIKELY(page_get_n_recs(page) < 2)
	    && UNIV_UNLIKELY(dict_index_get_page(index)
			     != buf_block_get_page_no(block))) {

		/* If there is only one record, drop the whole page in
		btr_discard_page, if this is not the root page */

		btr_discard_page(cursor, mtr);

		*err = DB_SUCCESS;
		ret = TRUE;

		goto return_after_reservations;
	}

	lock_update_delete(block, rec);
	level = btr_page_get_level(page, mtr);

	if (level > 0
	    && UNIV_UNLIKELY(rec == page_rec_get_next(
				     page_get_infimum_rec(page)))) {

		rec_t*	next_rec = page_rec_get_next(rec);

		if (btr_page_get_prev(page, mtr) == FIL_NULL) {

			/* If we delete the leftmost node pointer on a
			non-leaf level, we must mark the new leftmost node
			pointer as the predefined minimum record */

			/* This will make page_zip_validate() fail until
			page_cur_delete_rec() completes.  This is harmless,
			because everything will take place within a single
			mini-transaction and because writing to the redo log
			is an atomic operation (performed by mtr_commit()). */
			btr_set_min_rec_mark(next_rec, mtr);
		} else {
			/* Otherwise, if we delete the leftmost node pointer
			on a page, we have to change the father node pointer
			so that it is equal to the new leftmost node pointer
			on the page */

			btr_node_ptr_delete(index, block, mtr);

			node_ptr = dict_index_build_node_ptr(
				index, next_rec, buf_block_get_page_no(block),
				heap, level);

			btr_insert_on_non_leaf_level(index,
						     level + 1, node_ptr, mtr);
		}
	}

	btr_search_update_hash_on_delete(cursor);

	page_cur_delete_rec(btr_cur_get_page_cur(cursor), index, offsets, mtr);
#ifdef UNIV_ZIP_DEBUG
	ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */

	ut_ad(btr_check_node_ptr(index, block, mtr));

	*err = DB_SUCCESS;

return_after_reservations:
	mem_heap_free(heap);

	if (ret == FALSE) {
		ret = btr_cur_compress_if_useful(cursor, mtr);
	}

	if (n_extents > 0) {
		fil_space_release_free_extents(index->space, n_reserved);
	}

	return(ret);
}

/***********************************************************************
Adds path information to the cursor for the current page, for which
the binary search has been performed. */
static
void
btr_cur_add_path_info(
/*==================*/
	btr_cur_t*	cursor,		/* in: cursor positioned on a page */
	ulint		height,		/* in: height of the page in tree;
					0 means leaf node */
	ulint		root_height)	/* in: root node height in tree */
{
	btr_path_t*	slot;
	rec_t*		rec;

	ut_a(cursor->path_arr);

	if (root_height >= BTR_PATH_ARRAY_N_SLOTS - 1) {
		/* Do nothing; return empty path */

		slot = cursor->path_arr;
		slot->nth_rec = ULINT_UNDEFINED;

		return;
	}

	if (height == 0) {
		/* Mark end of slots for path */
		slot = cursor->path_arr + root_height + 1;
		slot->nth_rec = ULINT_UNDEFINED;
	}

	rec = btr_cur_get_rec(cursor);

	slot = cursor->path_arr + (root_height - height);

	slot->nth_rec = page_rec_get_n_recs_before(rec);
	slot->n_recs = page_get_n_recs(page_align(rec));
}

/***********************************************************************
Estimates the number of rows in a given index range. */
UNIV_INTERN
ib_int64_t
btr_estimate_n_rows_in_range(
/*=========================*/
				/* out: estimated number of rows */
	dict_index_t*	index,	/* in: index */
	const dtuple_t*	tuple1,	/* in: range start, may also be empty tuple */
	ulint		mode1,	/* in: search mode for range start */
	const dtuple_t*	tuple2,	/* in: range end, may also be empty tuple */
	ulint		mode2)	/* in: search mode for range end */
{
	btr_path_t	path1[BTR_PATH_ARRAY_N_SLOTS];
	btr_path_t	path2[BTR_PATH_ARRAY_N_SLOTS];
	btr_cur_t	cursor;
	btr_path_t*	slot1;
	btr_path_t*	slot2;
	ibool		diverged;
	ibool		diverged_lot;
	ulint		divergence_level;
	ib_int64_t	n_rows;
	ulint		i;
	mtr_t		mtr;

	mtr_start(&mtr);

	cursor.path_arr = path1;

	if (dtuple_get_n_fields(tuple1) > 0) {

		btr_cur_search_to_nth_level(index, 0, tuple1, mode1,
					    BTR_SEARCH_LEAF | BTR_ESTIMATE,
					    &cursor, 0, &mtr);
	} else {
		btr_cur_open_at_index_side(TRUE, index,
					   BTR_SEARCH_LEAF | BTR_ESTIMATE,
					   &cursor, &mtr);
	}

	mtr_commit(&mtr);

	mtr_start(&mtr);

	cursor.path_arr = path2;

	if (dtuple_get_n_fields(tuple2) > 0) {

		btr_cur_search_to_nth_level(index, 0, tuple2, mode2,
					    BTR_SEARCH_LEAF | BTR_ESTIMATE,
					    &cursor, 0, &mtr);
	} else {
		btr_cur_open_at_index_side(FALSE, index,
					   BTR_SEARCH_LEAF | BTR_ESTIMATE,
					   &cursor, &mtr);
	}

	mtr_commit(&mtr);

	/* We have the path information for the range in path1 and path2 */

	n_rows = 1;
	diverged = FALSE;	    /* This becomes true when the path is not
				    the same any more */
	diverged_lot = FALSE;	    /* This becomes true when the paths are
				    not the same or adjacent any more */
	divergence_level = 1000000; /* This is the level where paths diverged
				    a lot */
	for (i = 0; ; i++) {
		ut_ad(i < BTR_PATH_ARRAY_N_SLOTS);

		slot1 = path1 + i;
		slot2 = path2 + i;

		if (slot1->nth_rec == ULINT_UNDEFINED
		    || slot2->nth_rec == ULINT_UNDEFINED) {

			if (i > divergence_level + 1) {
				/* In trees whose height is > 1 our algorithm
				tends to underestimate: multiply the estimate
				by 2: */

				n_rows = n_rows * 2;
			}

			/* Do not estimate the number of rows in the range
			to over 1 / 2 of the estimated rows in the whole
			table */

			if (n_rows > index->table->stat_n_rows / 2) {
				n_rows = index->table->stat_n_rows / 2;

				/* If there are just 0 or 1 rows in the table,
				then we estimate all rows are in the range */

				if (n_rows == 0) {
					n_rows = index->table->stat_n_rows;
				}
			}

			return(n_rows);
		}

		if (!diverged && slot1->nth_rec != slot2->nth_rec) {

			diverged = TRUE;

			if (slot1->nth_rec < slot2->nth_rec) {
				n_rows = slot2->nth_rec - slot1->nth_rec;

				if (n_rows > 1) {
					diverged_lot = TRUE;
					divergence_level = i;
				}
			} else {
				/* Maybe the tree has changed between
				searches */

				return(10);
			}

		} else if (diverged && !diverged_lot) {

			if (slot1->nth_rec < slot1->n_recs
			    || slot2->nth_rec > 1) {

				diverged_lot = TRUE;
				divergence_level = i;

				n_rows = 0;

				if (slot1->nth_rec < slot1->n_recs) {
					n_rows += slot1->n_recs
						- slot1->nth_rec;
				}

				if (slot2->nth_rec > 1) {
					n_rows += slot2->nth_rec - 1;
				}
			}
		} else if (diverged_lot) {

			n_rows = (n_rows * (slot1->n_recs + slot2->n_recs))
				/ 2;
		}
	}
}

/***********************************************************************
Estimates the number of different key values in a given index, for
each n-column prefix of the index where n <= dict_index_get_n_unique(index).
The estimates are stored in the array index->stat_n_diff_key_vals. */
UNIV_INTERN
void
btr_estimate_number_of_different_key_vals(
/*======================================*/
	dict_index_t*	index)	/* in: index */
{
	btr_cur_t	cursor;
	page_t*		page;
	rec_t*		rec;
	ulint		n_cols;
	ulint		matched_fields;
	ulint		matched_bytes;
	ib_int64_t	n_recs	= 0;
	ib_int64_t*	n_diff;
	ib_int64_t*	n_not_nulls;
	ullint		n_sample_pages; /* number of pages to sample */
	ulint		not_empty_flag	= 0;
	ulint		total_external_size = 0;
	ulint		i;
	ulint		j;
	ullint		add_on;
	mtr_t		mtr;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_rec_[REC_OFFS_NORMAL_SIZE];
	ulint		offsets_next_rec_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets_rec	= offsets_rec_;
	ulint*		offsets_next_rec= offsets_next_rec_;
	ulint		stats_method	= srv_stats_method;
	rec_offs_init(offsets_rec_);
	rec_offs_init(offsets_next_rec_);

	n_cols = dict_index_get_n_unique(index);

	n_diff = mem_zalloc((n_cols + 1) * sizeof(ib_int64_t));

	if (stats_method == SRV_STATS_METHOD_IGNORE_NULLS) {
		n_not_nulls = mem_zalloc((n_cols + 1) * sizeof(ib_int64_t));
	}

	/* It makes no sense to test more pages than are contained
	in the index, thus we lower the number if it is too high */
	if (srv_stats_sample_pages > index->stat_index_size) {
		if (index->stat_index_size > 0) {
			n_sample_pages = index->stat_index_size;
		} else {
			n_sample_pages = 1;
		}
	} else {
		n_sample_pages = srv_stats_sample_pages;
	}

	/* We sample some pages in the index to get an estimate */

	for (i = 0; i < n_sample_pages; i++) {
		rec_t*	supremum;
		mtr_start(&mtr);

		btr_cur_open_at_rnd_pos(index, BTR_SEARCH_LEAF, &cursor, &mtr);

		/* Count the number of different key values for each prefix of
		the key on this index page. If the prefix does not determine
		the index record uniquely in the B-tree, then we subtract one
		because otherwise our algorithm would give a wrong estimate
		for an index where there is just one key value. */

		page = btr_cur_get_page(&cursor);

		supremum = page_get_supremum_rec(page);
		rec = page_rec_get_next(page_get_infimum_rec(page));

		if (rec != supremum) {
			not_empty_flag = 1;
			offsets_rec = rec_get_offsets(rec, index, offsets_rec,
						      ULINT_UNDEFINED, &heap);
		}

		while (rec != supremum) {
                        rec_t*	next_rec;
			/* count recs */
			if (stats_method == SRV_STATS_METHOD_IGNORE_NULLS) {
				n_recs++;
				for (j = 0; j <= n_cols; j++) {
					ulint	f_len;
					rec_get_nth_field(rec, offsets_rec,
							  j, &f_len);
					if (f_len == UNIV_SQL_NULL)
						break;

					n_not_nulls[j]++;
				}
			}

			next_rec = page_rec_get_next(rec);
			if (next_rec == supremum) {
				break;
			}

			matched_fields = 0;
			matched_bytes = 0;
			offsets_next_rec = rec_get_offsets(next_rec, index,
							   offsets_next_rec,
							   n_cols, &heap);

			cmp_rec_rec_with_match(rec, next_rec,
					       offsets_rec, offsets_next_rec,
					       index, &matched_fields,
					       &matched_bytes, srv_stats_method);

			for (j = matched_fields + 1; j <= n_cols; j++) {
				/* We add one if this index record has
				a different prefix from the previous */

				n_diff[j]++;
			}

			total_external_size
				+= btr_rec_get_externally_stored_len(
					rec, offsets_rec);

			rec = next_rec;
			/* Initialize offsets_rec for the next round
			and assign the old offsets_rec buffer to
			offsets_next_rec. */
			{
				ulint*	offsets_tmp = offsets_rec;
				offsets_rec = offsets_next_rec;
				offsets_next_rec = offsets_tmp;
			}
		}


		if (n_cols == dict_index_get_n_unique_in_tree(index)) {

			/* If there is more than one leaf page in the tree,
			we add one because we know that the first record
			on the page certainly had a different prefix than the
			last record on the previous index page in the
			alphabetical order. Before this fix, if there was
			just one big record on each clustered index page, the
			algorithm grossly underestimated the number of rows
			in the table. */

			if (btr_page_get_prev(page, &mtr) != FIL_NULL
			    || btr_page_get_next(page, &mtr) != FIL_NULL) {

				n_diff[n_cols]++;
			}
		}

		offsets_rec = rec_get_offsets(rec, index, offsets_rec,
					      ULINT_UNDEFINED, &heap);
		total_external_size += btr_rec_get_externally_stored_len(
			rec, offsets_rec);
		mtr_commit(&mtr);
	}

	/* If we saw k borders between different key values on
	n_sample_pages leaf pages, we can estimate how many
	there will be in index->stat_n_leaf_pages */

	/* We must take into account that our sample actually represents
	also the pages used for external storage of fields (those pages are
	included in index->stat_n_leaf_pages) */

	for (j = 0; j <= n_cols; j++) {
		index->stat_n_diff_key_vals[j]
			= ((n_diff[j]
			    * (ib_int64_t)index->stat_n_leaf_pages
			    + n_sample_pages - 1
			    + total_external_size
			    + not_empty_flag)
			   / (n_sample_pages
			      + total_external_size));

		/* If the tree is small, smaller than
		10 * n_sample_pages + total_external_size, then
		the above estimate is ok. For bigger trees it is common that we
		do not see any borders between key values in the few pages
		we pick. But still there may be n_sample_pages
		different key values, or even more. Let us try to approximate
		that: */

		add_on = index->stat_n_leaf_pages
			/ (10 * (n_sample_pages
				 + total_external_size));

		if (add_on > n_sample_pages) {
			add_on = n_sample_pages;
		}

		index->stat_n_diff_key_vals[j] += add_on;

		/* revision for 'nulls_ignored' */
		if (stats_method == SRV_STATS_METHOD_IGNORE_NULLS) {
			if (!n_not_nulls[j])
				n_not_nulls[j] = 1;
			index->stat_n_diff_key_vals[j] =
				index->stat_n_diff_key_vals[j] * n_recs
					/ n_not_nulls[j];
		}
	}

	mem_free(n_diff);
	if (stats_method == SRV_STATS_METHOD_IGNORE_NULLS) {
		mem_free(n_not_nulls);
	}
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/*================== EXTERNAL STORAGE OF BIG FIELDS ===================*/

/***************************************************************
Gets the externally stored size of a record, in units of a database page. */
static
ulint
btr_rec_get_externally_stored_len(
/*==============================*/
				/* out: externally stored part,
				in units of a database page */
	rec_t*		rec,	/* in: record */
	const ulint*	offsets)/* in: array returned by rec_get_offsets() */
{
	ulint	n_fields;
	byte*	data;
	ulint	local_len;
	ulint	extern_len;
	ulint	total_extern_len = 0;
	ulint	i;

	ut_ad(!rec_offs_comp(offsets) || !rec_get_node_ptr_flag(rec));
	n_fields = rec_offs_n_fields(offsets);

	for (i = 0; i < n_fields; i++) {
		if (rec_offs_nth_extern(offsets, i)) {

			data = rec_get_nth_field(rec, offsets, i, &local_len);

			local_len -= BTR_EXTERN_FIELD_REF_SIZE;

			extern_len = mach_read_from_4(data + local_len
						      + BTR_EXTERN_LEN + 4);

			total_extern_len += ut_calc_align(extern_len,
							  UNIV_PAGE_SIZE);
		}
	}

	return(total_extern_len / UNIV_PAGE_SIZE);
}

/***********************************************************************
Sets the ownership bit of an externally stored field in a record. */
static
void
btr_cur_set_ownership_of_extern_field(
/*==================================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page whose uncompressed
				part will be updated, or NULL */
	rec_t*		rec,	/* in/out: clustered index record */
	dict_index_t*	index,	/* in: index of the page */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		i,	/* in: field number */
	ibool		val,	/* in: value to set */
	mtr_t*		mtr)	/* in: mtr, or NULL if not logged */
{
	byte*	data;
	ulint	local_len;
	ulint	byte_val;

	data = rec_get_nth_field(rec, offsets, i, &local_len);

	ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

	local_len -= BTR_EXTERN_FIELD_REF_SIZE;

	byte_val = mach_read_from_1(data + local_len + BTR_EXTERN_LEN);

	if (val) {
		byte_val = byte_val & (~BTR_EXTERN_OWNER_FLAG);
	} else {
		byte_val = byte_val | BTR_EXTERN_OWNER_FLAG;
	}

	if (UNIV_LIKELY_NULL(page_zip)) {
		mach_write_to_1(data + local_len + BTR_EXTERN_LEN, byte_val);
		page_zip_write_blob_ptr(page_zip, rec, index, offsets, i, mtr);
	} else if (UNIV_LIKELY(mtr != NULL)) {

		mlog_write_ulint(data + local_len + BTR_EXTERN_LEN, byte_val,
				 MLOG_1BYTE, mtr);
	} else {
		mach_write_to_1(data + local_len + BTR_EXTERN_LEN, byte_val);
	}
}

/***********************************************************************
Marks not updated extern fields as not-owned by this record. The ownership
is transferred to the updated record which is inserted elsewhere in the
index tree. In purge only the owner of externally stored field is allowed
to free the field. */
UNIV_INTERN
void
btr_cur_mark_extern_inherited_fields(
/*=================================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page whose uncompressed
				part will be updated, or NULL */
	rec_t*		rec,	/* in/out: record in a clustered index */
	dict_index_t*	index,	/* in: index of the page */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	const upd_t*	update,	/* in: update vector */
	mtr_t*		mtr)	/* in: mtr, or NULL if not logged */
{
	ulint	n;
	ulint	j;
	ulint	i;

	ut_ad(rec_offs_validate(rec, NULL, offsets));
	ut_ad(!rec_offs_comp(offsets) || !rec_get_node_ptr_flag(rec));

	if (!rec_offs_any_extern(offsets)) {

		return;
	}

	n = rec_offs_n_fields(offsets);

	for (i = 0; i < n; i++) {
		if (rec_offs_nth_extern(offsets, i)) {

			/* Check it is not in updated fields */

			if (update) {
				for (j = 0; j < upd_get_n_fields(update);
				     j++) {
					if (upd_get_nth_field(update, j)
					    ->field_no == i) {

						goto updated;
					}
				}
			}

			btr_cur_set_ownership_of_extern_field(
				page_zip, rec, index, offsets, i, FALSE, mtr);
updated:
			;
		}
	}
}

/***********************************************************************
The complement of the previous function: in an update entry may inherit
some externally stored fields from a record. We must mark them as inherited
in entry, so that they are not freed in a rollback. */
UNIV_INTERN
void
btr_cur_mark_dtuple_inherited_extern(
/*=================================*/
	dtuple_t*	entry,		/* in/out: updated entry to be
					inserted to clustered index */
	const upd_t*	update)		/* in: update vector */
{
	ulint		i;

	for (i = 0; i < dtuple_get_n_fields(entry); i++) {

		dfield_t*	dfield = dtuple_get_nth_field(entry, i);
		byte*		data;
		ulint		len;
		ulint		j;

		if (!dfield_is_ext(dfield)) {
			continue;
		}

		/* Check if it is in updated fields */

		for (j = 0; j < upd_get_n_fields(update); j++) {
			if (upd_get_nth_field(update, j)->field_no == i) {

				goto is_updated;
			}
		}

		data = dfield_get_data(dfield);
		len = dfield_get_len(dfield);
		data[len - BTR_EXTERN_FIELD_REF_SIZE + BTR_EXTERN_LEN]
			|= BTR_EXTERN_INHERITED_FLAG;

is_updated:
		;
	}
}

/***********************************************************************
Marks all extern fields in a record as owned by the record. This function
should be called if the delete mark of a record is removed: a not delete
marked record always owns all its extern fields. */
static
void
btr_cur_unmark_extern_fields(
/*=========================*/
	page_zip_des_t*	page_zip,/* in/out: compressed page whose uncompressed
				part will be updated, or NULL */
	rec_t*		rec,	/* in/out: record in a clustered index */
	dict_index_t*	index,	/* in: index of the page */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	mtr_t*		mtr)	/* in: mtr, or NULL if not logged */
{
	ulint	n;
	ulint	i;

	ut_ad(!rec_offs_comp(offsets) || !rec_get_node_ptr_flag(rec));
	n = rec_offs_n_fields(offsets);

	if (!rec_offs_any_extern(offsets)) {

		return;
	}

	for (i = 0; i < n; i++) {
		if (rec_offs_nth_extern(offsets, i)) {

			btr_cur_set_ownership_of_extern_field(
				page_zip, rec, index, offsets, i, TRUE, mtr);
		}
	}
}

/***********************************************************************
Marks all extern fields in a dtuple as owned by the record. */
UNIV_INTERN
void
btr_cur_unmark_dtuple_extern_fields(
/*================================*/
	dtuple_t*	entry)		/* in/out: clustered index entry */
{
	ulint	i;

	for (i = 0; i < dtuple_get_n_fields(entry); i++) {
		dfield_t* dfield = dtuple_get_nth_field(entry, i);

		if (dfield_is_ext(dfield)) {
			byte*	data = dfield_get_data(dfield);
			ulint	len = dfield_get_len(dfield);

			data[len - BTR_EXTERN_FIELD_REF_SIZE + BTR_EXTERN_LEN]
				&= ~BTR_EXTERN_OWNER_FLAG;
		}
	}
}

/***********************************************************************
Flags the data tuple fields that are marked as extern storage in the
update vector.  We use this function to remember which fields we must
mark as extern storage in a record inserted for an update. */
UNIV_INTERN
ulint
btr_push_update_extern_fields(
/*==========================*/
				/* out: number of flagged external columns */
	dtuple_t*	tuple,	/* in/out: data tuple */
	const upd_t*	update,	/* in: update vector */
	mem_heap_t*	heap)	/* in: memory heap */
{
	ulint			n_pushed	= 0;
	ulint			n;
	const upd_field_t*	uf;

	ut_ad(tuple);
	ut_ad(update);

	uf = update->fields;
	n = upd_get_n_fields(update);

	for (; n--; uf++) {
		if (dfield_is_ext(&uf->new_val)) {
			dfield_t*	field
				= dtuple_get_nth_field(tuple, uf->field_no);

			if (!dfield_is_ext(field)) {
				dfield_set_ext(field);
				n_pushed++;
			}

			switch (uf->orig_len) {
				byte*	data;
				ulint	len;
				byte*	buf;
			case 0:
				break;
			case BTR_EXTERN_FIELD_REF_SIZE:
				/* Restore the original locally stored
				part of the column.  In the undo log,
				InnoDB writes a longer prefix of externally
				stored columns, so that column prefixes
				in secondary indexes can be reconstructed. */
				dfield_set_data(field, (byte*) dfield_get_data(field)
						+ dfield_get_len(field)
						- BTR_EXTERN_FIELD_REF_SIZE,
						BTR_EXTERN_FIELD_REF_SIZE);
				dfield_set_ext(field);
				break;
			default:
				/* Reconstruct the original locally
				stored part of the column.  The data
				will have to be copied. */
				ut_a(uf->orig_len > BTR_EXTERN_FIELD_REF_SIZE);

				data = dfield_get_data(field);
				len = dfield_get_len(field);

				buf = mem_heap_alloc(heap, uf->orig_len);
				/* Copy the locally stored prefix. */
				memcpy(buf, data,
				       uf->orig_len
				       - BTR_EXTERN_FIELD_REF_SIZE);
				/* Copy the BLOB pointer. */
				memcpy(buf + uf->orig_len
				       - BTR_EXTERN_FIELD_REF_SIZE,
				       data + len - BTR_EXTERN_FIELD_REF_SIZE,
				       BTR_EXTERN_FIELD_REF_SIZE);

				dfield_set_data(field, buf, uf->orig_len);
				dfield_set_ext(field);
			}
		}
	}

	return(n_pushed);
}

/***********************************************************************
Returns the length of a BLOB part stored on the header page. */
static
ulint
btr_blob_get_part_len(
/*==================*/
					/* out: part length */
	const byte*	blob_header)	/* in: blob header */
{
	return(mach_read_from_4(blob_header + BTR_BLOB_HDR_PART_LEN));
}

/***********************************************************************
Returns the page number where the next BLOB part is stored. */
static
ulint
btr_blob_get_next_page_no(
/*======================*/
					/* out: page number or FIL_NULL if
					no more pages */
	const byte*	blob_header)	/* in: blob header */
{
	return(mach_read_from_4(blob_header + BTR_BLOB_HDR_NEXT_PAGE_NO));
}

/***********************************************************************
Deallocate a buffer block that was reserved for a BLOB part. */
static
void
btr_blob_free(
/*==========*/
	buf_block_t*	block,	/* in: buffer block */
	ibool		all,	/* in: TRUE=remove also the compressed page
				if there is one */
	mtr_t*		mtr)	/* in: mini-transaction to commit */
{
	ulint	space	= buf_block_get_space(block);
	ulint	page_no	= buf_block_get_page_no(block);

	ut_ad(mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));

	mtr_commit(mtr);

	//buf_pool_mutex_enter();
	mutex_enter(&LRU_list_mutex);
	mutex_enter(&block->mutex);

	/* Only free the block if it is still allocated to
	the same file page. */

	if (buf_block_get_state(block)
	    == BUF_BLOCK_FILE_PAGE
	    && buf_block_get_space(block) == space
	    && buf_block_get_page_no(block) == page_no) {

		if (buf_LRU_free_block(&block->page, all, NULL, TRUE)
		    != BUF_LRU_FREED
		    && all && block->page.zip.data
		    /* Now, buf_LRU_free_block() may release mutex temporarily */
		    && buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE
		    && buf_block_get_space(block) == space
		    && buf_block_get_page_no(block) == page_no) {
			/* Attempt to deallocate the uncompressed page
			if the whole block cannot be deallocted. */

			buf_LRU_free_block(&block->page, FALSE, NULL, TRUE);
		}
	}

	//buf_pool_mutex_exit();
	mutex_exit(&LRU_list_mutex);
	mutex_exit(&block->mutex);
}

/***********************************************************************
Stores the fields in big_rec_vec to the tablespace and puts pointers to
them in rec.  The extern flags in rec will have to be set beforehand.
The fields are stored on pages allocated from leaf node
file segment of the index tree. */
UNIV_INTERN
ulint
btr_store_big_rec_extern_fields(
/*============================*/
					/* out: DB_SUCCESS or error */
	dict_index_t*	index,		/* in: index of rec; the index tree
					MUST be X-latched */
	buf_block_t*	rec_block,	/* in/out: block containing rec */
	rec_t*		rec,		/* in/out: record */
	const ulint*	offsets,	/* in: rec_get_offsets(rec, index);
					the "external storage" flags in offsets
					will not correspond to rec when
					this function returns */
	big_rec_t*	big_rec_vec,	/* in: vector containing fields
					to be stored externally */
	mtr_t*		local_mtr __attribute__((unused))) /* in: mtr
					containing the latch to rec and to the
					tree */
{
	ulint	rec_page_no;
	byte*	field_ref;
	ulint	extern_len;
	ulint	store_len;
	ulint	page_no;
	ulint	space_id;
	ulint	zip_size;
	ulint	prev_page_no;
	ulint	hint_page_no;
	ulint	i;
	mtr_t	mtr;
	mem_heap_t* heap = NULL;
	page_zip_des_t*	page_zip;
	z_stream c_stream;

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(mtr_memo_contains(local_mtr, dict_index_get_lock(index),
				MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(local_mtr, rec_block, MTR_MEMO_PAGE_X_FIX));
	ut_ad(buf_block_get_frame(rec_block) == page_align(rec));
	ut_a(dict_index_is_clust(index));

	page_zip = buf_block_get_page_zip(rec_block);
	ut_a(dict_table_zip_size(index->table)
	     == buf_block_get_zip_size(rec_block));

	space_id = buf_block_get_space(rec_block);
	zip_size = buf_block_get_zip_size(rec_block);
	rec_page_no = buf_block_get_page_no(rec_block);
	ut_a(fil_page_get_type(page_align(rec)) == FIL_PAGE_INDEX);

	if (UNIV_LIKELY_NULL(page_zip)) {
		int	err;

		/* Zlib deflate needs 128 kilobytes for the default
		window size, plus 512 << memLevel, plus a few
		kilobytes for small objects.  We use reduced memLevel
		to limit the memory consumption, and preallocate the
		heap, hoping to avoid memory fragmentation. */
		heap = mem_heap_create(250000);
		page_zip_set_alloc(&c_stream, heap);

		err = deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION,
				   Z_DEFLATED, 15, 7, Z_DEFAULT_STRATEGY);
		ut_a(err == Z_OK);
	}

	/* We have to create a file segment to the tablespace
	for each field and put the pointer to the field in rec */

	for (i = 0; i < big_rec_vec->n_fields; i++) {
		ut_ad(rec_offs_nth_extern(offsets,
					  big_rec_vec->fields[i].field_no));
		{
			ulint	local_len;
			field_ref = rec_get_nth_field(
				rec, offsets, big_rec_vec->fields[i].field_no,
				&local_len);
			ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);
			local_len -= BTR_EXTERN_FIELD_REF_SIZE;
			field_ref += local_len;
		}
		extern_len = big_rec_vec->fields[i].len;

		ut_a(extern_len > 0);

		prev_page_no = FIL_NULL;

		if (UNIV_LIKELY_NULL(page_zip)) {
			int	err = deflateReset(&c_stream);
			ut_a(err == Z_OK);

			c_stream.next_in = (void*) big_rec_vec->fields[i].data;
			c_stream.avail_in = extern_len;
		}

		for (;;) {
			buf_block_t*	block;
			page_t*		page;

			mtr_start(&mtr);

			if (prev_page_no == FIL_NULL) {
				hint_page_no = 1 + rec_page_no;
			} else {
				hint_page_no = prev_page_no + 1;
			}

			block = btr_page_alloc(index, hint_page_no,
					       FSP_NO_DIR, 0, &mtr);
			if (UNIV_UNLIKELY(block == NULL)) {

				mtr_commit(&mtr);

				if (UNIV_LIKELY_NULL(page_zip)) {
					deflateEnd(&c_stream);
					mem_heap_free(heap);
				}

				return(DB_OUT_OF_FILE_SPACE);
			}

			page_no = buf_block_get_page_no(block);
			page = buf_block_get_frame(block);

			if (prev_page_no != FIL_NULL) {
				buf_block_t*	prev_block;
				page_t*		prev_page;

				prev_block = buf_page_get(space_id, zip_size,
							  prev_page_no,
							  RW_X_LATCH, &mtr);
				buf_block_dbg_add_level(prev_block,
							SYNC_EXTERN_STORAGE);
				prev_page = buf_block_get_frame(prev_block);

				if (UNIV_LIKELY_NULL(page_zip)) {
					mlog_write_ulint(
						prev_page + FIL_PAGE_NEXT,
						page_no, MLOG_4BYTES, &mtr);
					memcpy(buf_block_get_page_zip(
						       prev_block)
					       ->data + FIL_PAGE_NEXT,
					       prev_page + FIL_PAGE_NEXT, 4);
				} else {
					mlog_write_ulint(
						prev_page + FIL_PAGE_DATA
						+ BTR_BLOB_HDR_NEXT_PAGE_NO,
						page_no, MLOG_4BYTES, &mtr);
				}

			}

			if (UNIV_LIKELY_NULL(page_zip)) {
				int		err;
				page_zip_des_t*	blob_page_zip;

				mach_write_to_2(page + FIL_PAGE_TYPE,
						prev_page_no == FIL_NULL
						? FIL_PAGE_TYPE_ZBLOB
						: FIL_PAGE_TYPE_ZBLOB2);

				c_stream.next_out = page
					+ FIL_PAGE_DATA;
				c_stream.avail_out
					= page_zip_get_size(page_zip)
					- FIL_PAGE_DATA;

				err = deflate(&c_stream, Z_FINISH);
				ut_a(err == Z_OK || err == Z_STREAM_END);
				ut_a(err == Z_STREAM_END
				     || c_stream.avail_out == 0);

				/* Write the "next BLOB page" pointer */
				mlog_write_ulint(page + FIL_PAGE_NEXT,
						 FIL_NULL, MLOG_4BYTES, &mtr);
				/* Initialize the unused "prev page" pointer */
				mlog_write_ulint(page + FIL_PAGE_PREV,
						 FIL_NULL, MLOG_4BYTES, &mtr);
				/* Write a back pointer to the record
				into the otherwise unused area.  This
				information could be useful in
				debugging.  Later, we might want to
				implement the possibility to relocate
				BLOB pages.  Then, we would need to be
				able to adjust the BLOB pointer in the
				record.  We do not store the heap
				number of the record, because it can
				change in page_zip_reorganize() or
				btr_page_reorganize().  However, also
				the page number of the record may
				change when B-tree nodes are split or
				merged. */
				mlog_write_ulint(page
						 + FIL_PAGE_FILE_FLUSH_LSN,
						 space_id,
						 MLOG_4BYTES, &mtr);
				mlog_write_ulint(page
						 + FIL_PAGE_FILE_FLUSH_LSN + 4,
						 rec_page_no,
						 MLOG_4BYTES, &mtr);

				/* Zero out the unused part of the page. */
				memset(page + page_zip_get_size(page_zip)
				       - c_stream.avail_out,
				       0, c_stream.avail_out);
				mlog_log_string(page + FIL_PAGE_TYPE,
						page_zip_get_size(page_zip)
						- FIL_PAGE_TYPE,
						&mtr);
				/* Copy the page to compressed storage,
				because it will be flushed to disk
				from there. */
				blob_page_zip = buf_block_get_page_zip(block);
				ut_ad(blob_page_zip);
				ut_ad(page_zip_get_size(blob_page_zip)
				      == page_zip_get_size(page_zip));
				memcpy(blob_page_zip->data, page,
				       page_zip_get_size(page_zip));

				if (err == Z_OK && prev_page_no != FIL_NULL) {

					goto next_zip_page;
				}

				rec_block = buf_page_get(space_id, zip_size,
							 rec_page_no,
							 RW_X_LATCH, &mtr);
				buf_block_dbg_add_level(rec_block,
							SYNC_NO_ORDER_CHECK);

				if (err == Z_STREAM_END) {
					mach_write_to_4(field_ref
							+ BTR_EXTERN_LEN, 0);
					mach_write_to_4(field_ref
							+ BTR_EXTERN_LEN + 4,
							c_stream.total_in);
				} else {
					memset(field_ref + BTR_EXTERN_LEN,
					       0, 8);
				}

				if (prev_page_no == FIL_NULL) {
					mach_write_to_4(field_ref
							+ BTR_EXTERN_SPACE_ID,
							space_id);

					mach_write_to_4(field_ref
							+ BTR_EXTERN_PAGE_NO,
							page_no);

					mach_write_to_4(field_ref
							+ BTR_EXTERN_OFFSET,
							FIL_PAGE_NEXT);
				}

				page_zip_write_blob_ptr(
					page_zip, rec, index, offsets,
					big_rec_vec->fields[i].field_no, &mtr);

next_zip_page:
				prev_page_no = page_no;

				/* Commit mtr and release the
				uncompressed page frame to save memory. */
				btr_blob_free(block, FALSE, &mtr);

				if (err == Z_STREAM_END) {
					break;
				}
			} else {
				mlog_write_ulint(page + FIL_PAGE_TYPE,
						 FIL_PAGE_TYPE_BLOB,
						 MLOG_2BYTES, &mtr);

				if (extern_len > (UNIV_PAGE_SIZE
						  - FIL_PAGE_DATA
						  - BTR_BLOB_HDR_SIZE
						  - FIL_PAGE_DATA_END)) {
					store_len = UNIV_PAGE_SIZE
						- FIL_PAGE_DATA
						- BTR_BLOB_HDR_SIZE
						- FIL_PAGE_DATA_END;
				} else {
					store_len = extern_len;
				}

				mlog_write_string(page + FIL_PAGE_DATA
						  + BTR_BLOB_HDR_SIZE,
						  (const byte*)
						  big_rec_vec->fields[i].data
						  + big_rec_vec->fields[i].len
						  - extern_len,
						  store_len, &mtr);
				mlog_write_ulint(page + FIL_PAGE_DATA
						 + BTR_BLOB_HDR_PART_LEN,
						 store_len, MLOG_4BYTES, &mtr);
				mlog_write_ulint(page + FIL_PAGE_DATA
						 + BTR_BLOB_HDR_NEXT_PAGE_NO,
						 FIL_NULL, MLOG_4BYTES, &mtr);

				extern_len -= store_len;

				rec_block = buf_page_get(space_id, zip_size,
							 rec_page_no,
							 RW_X_LATCH, &mtr);
				buf_block_dbg_add_level(rec_block,
							SYNC_NO_ORDER_CHECK);

				mlog_write_ulint(field_ref + BTR_EXTERN_LEN, 0,
						 MLOG_4BYTES, &mtr);
				mlog_write_ulint(field_ref
						 + BTR_EXTERN_LEN + 4,
						 big_rec_vec->fields[i].len
						 - extern_len,
						 MLOG_4BYTES, &mtr);

				if (prev_page_no == FIL_NULL) {
					mlog_write_ulint(field_ref
							 + BTR_EXTERN_SPACE_ID,
							 space_id,
							 MLOG_4BYTES, &mtr);

					mlog_write_ulint(field_ref
							 + BTR_EXTERN_PAGE_NO,
							 page_no,
							 MLOG_4BYTES, &mtr);

					mlog_write_ulint(field_ref
							 + BTR_EXTERN_OFFSET,
							 FIL_PAGE_DATA,
							 MLOG_4BYTES, &mtr);
				}

				prev_page_no = page_no;

				mtr_commit(&mtr);

				if (extern_len == 0) {
					break;
				}
			}
		}
	}

	if (UNIV_LIKELY_NULL(page_zip)) {
		deflateEnd(&c_stream);
		mem_heap_free(heap);
	}

	return(DB_SUCCESS);
}

/***********************************************************************
Check the FIL_PAGE_TYPE on an uncompressed BLOB page. */
static
void
btr_check_blob_fil_page_type(
/*=========================*/
	ulint		space_id,	/* in: space id */
	ulint		page_no,	/* in: page number */
	const page_t*	page,		/* in: page */
	ibool		read)		/* in: TRUE=read, FALSE=purge */
{
	ulint	type = fil_page_get_type(page);

	ut_a(space_id == page_get_space_id(page));
	ut_a(page_no == page_get_page_no(page));

	if (UNIV_UNLIKELY(type != FIL_PAGE_TYPE_BLOB)) {
		ulint	flags = fil_space_get_flags(space_id);

		if (UNIV_LIKELY
		    ((flags & DICT_TF_FORMAT_MASK) == DICT_TF_FORMAT_51)) {
			/* Old versions of InnoDB did not initialize
			FIL_PAGE_TYPE on BLOB pages.  Do not print
			anything about the type mismatch when reading
			a BLOB page that is in Antelope format.*/
			return;
		}

		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: FIL_PAGE_TYPE=%lu"
			" on BLOB %s space %lu page %lu flags %lx\n",
			(ulong) type, read ? "read" : "purge",
			(ulong) space_id, (ulong) page_no, (ulong) flags);
		ut_error;
	}
}

/***********************************************************************
Frees the space in an externally stored field to the file space
management if the field in data is owned by the externally stored field,
in a rollback we may have the additional condition that the field must
not be inherited. */
UNIV_INTERN
void
btr_free_externally_stored_field(
/*=============================*/
	dict_index_t*	index,		/* in: index of the data, the index
					tree MUST be X-latched; if the tree
					height is 1, then also the root page
					must be X-latched! (this is relevant
					in the case this function is called
					from purge where 'data' is located on
					an undo log page, not an index
					page) */
	byte*		field_ref,	/* in/out: field reference */
	const rec_t*	rec,		/* in: record containing field_ref, for
					page_zip_write_blob_ptr(), or NULL */
	const ulint*	offsets,	/* in: rec_get_offsets(rec, index),
					or NULL */
	page_zip_des_t*	page_zip,	/* in: compressed page corresponding
					to rec, or NULL if rec == NULL */
	ulint		i,		/* in: field number of field_ref;
					ignored if rec == NULL */
	enum trx_rb_ctx	rb_ctx,		/* in: rollback context */
	mtr_t*		local_mtr __attribute__((unused))) /* in: mtr
					containing the latch to data an an
					X-latch to the index tree */
{
	page_t*		page;
	ulint		space_id;
	ulint		rec_zip_size = dict_table_zip_size(index->table);
	ulint		ext_zip_size;
	ulint		page_no;
	ulint		next_page_no;
	mtr_t		mtr;
#ifdef UNIV_DEBUG
	ut_ad(mtr_memo_contains(local_mtr, dict_index_get_lock(index),
				MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains_page(local_mtr, field_ref,
				     MTR_MEMO_PAGE_X_FIX));
	ut_ad(!rec || rec_offs_validate(rec, index, offsets));

	if (rec) {
		ulint	local_len;
		const byte*	f = rec_get_nth_field(rec, offsets,
						      i, &local_len);
		ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);
		local_len -= BTR_EXTERN_FIELD_REF_SIZE;
		f += local_len;
		ut_ad(f == field_ref);
	}
#endif /* UNIV_DEBUG */

	if (UNIV_UNLIKELY(!memcmp(field_ref, field_ref_zero,
				  BTR_EXTERN_FIELD_REF_SIZE))) {
		/* In the rollback of uncommitted transactions, we may
		encounter a clustered index record whose BLOBs have
		not been written.  There is nothing to free then. */
		ut_a(rb_ctx == RB_RECOVERY);
		return;
	}

	space_id = mach_read_from_4(field_ref + BTR_EXTERN_SPACE_ID);

	if (UNIV_UNLIKELY(space_id != dict_index_get_space(index))) {
		ext_zip_size = fil_space_get_zip_size(space_id);
		/* This must be an undo log record in the system tablespace,
		that is, in row_purge_upd_exist_or_extern().
		Currently, externally stored records are stored in the
		same tablespace as the referring records. */
		ut_ad(!page_get_space_id(page_align(field_ref)));
		ut_ad(!rec);
		ut_ad(!page_zip);
	} else {
		ext_zip_size = rec_zip_size;
	}

	if (!rec) {
		/* This is a call from row_purge_upd_exist_or_extern(). */
		ut_ad(!page_zip);
		rec_zip_size = 0;
	}

	for (;;) {
		buf_block_t*	rec_block;
		buf_block_t*	ext_block;

		mtr_start(&mtr);

		rec_block = buf_page_get(page_get_space_id(
						 page_align(field_ref)),
					 rec_zip_size,
					 page_get_page_no(
						 page_align(field_ref)),
					 RW_X_LATCH, &mtr);
		buf_block_dbg_add_level(rec_block, SYNC_NO_ORDER_CHECK);
		page_no = mach_read_from_4(field_ref + BTR_EXTERN_PAGE_NO);

		if (/* There is no external storage data */
		    page_no == FIL_NULL
		    /* This field does not own the externally stored field */
		    || (mach_read_from_1(field_ref + BTR_EXTERN_LEN)
			& BTR_EXTERN_OWNER_FLAG)
		    /* Rollback and inherited field */
		    || (rb_ctx != RB_NONE
			&& (mach_read_from_1(field_ref + BTR_EXTERN_LEN)
			    & BTR_EXTERN_INHERITED_FLAG))) {

			/* Do not free */
			mtr_commit(&mtr);

			return;
		}

		ext_block = buf_page_get(space_id, ext_zip_size, page_no,
					 RW_X_LATCH, &mtr);
		buf_block_dbg_add_level(ext_block, SYNC_EXTERN_STORAGE);
		page = buf_block_get_frame(ext_block);

		if (ext_zip_size) {
			/* Note that page_zip will be NULL
			in row_purge_upd_exist_or_extern(). */
			switch (fil_page_get_type(page)) {
			case FIL_PAGE_TYPE_ZBLOB:
			case FIL_PAGE_TYPE_ZBLOB2:
				break;
			default:
				ut_error;
			}
			next_page_no = mach_read_from_4(page + FIL_PAGE_NEXT);

			btr_page_free_low(index, ext_block, 0, &mtr);

			if (UNIV_LIKELY(page_zip != NULL)) {
				mach_write_to_4(field_ref + BTR_EXTERN_PAGE_NO,
						next_page_no);
				mach_write_to_4(field_ref + BTR_EXTERN_LEN + 4,
						0);
				page_zip_write_blob_ptr(page_zip, rec, index,
							offsets, i, &mtr);
			} else {
				mlog_write_ulint(field_ref
						 + BTR_EXTERN_PAGE_NO,
						 next_page_no,
						 MLOG_4BYTES, &mtr);
				mlog_write_ulint(field_ref
						 + BTR_EXTERN_LEN + 4, 0,
						 MLOG_4BYTES, &mtr);
			}
		} else {
			ut_a(!page_zip);
			btr_check_blob_fil_page_type(space_id, page_no, page,
						     FALSE);

			next_page_no = mach_read_from_4(
				page + FIL_PAGE_DATA
				+ BTR_BLOB_HDR_NEXT_PAGE_NO);

			/* We must supply the page level (= 0) as an argument
			because we did not store it on the page (we save the
			space overhead from an index page header. */

			btr_page_free_low(index, ext_block, 0, &mtr);

			mlog_write_ulint(field_ref + BTR_EXTERN_PAGE_NO,
					 next_page_no,
					 MLOG_4BYTES, &mtr);
			/* Zero out the BLOB length.  If the server
			crashes during the execution of this function,
			trx_rollback_or_clean_all_recovered() could
			dereference the half-deleted BLOB, fetching a
			wrong prefix for the BLOB. */
			mlog_write_ulint(field_ref + BTR_EXTERN_LEN + 4,
					 0,
					 MLOG_4BYTES, &mtr);
		}

		/* Commit mtr and release the BLOB block to save memory. */
		btr_blob_free(ext_block, TRUE, &mtr);
	}
}

/***************************************************************
Frees the externally stored fields for a record. */
static
void
btr_rec_free_externally_stored_fields(
/*==================================*/
	dict_index_t*	index,	/* in: index of the data, the index
				tree MUST be X-latched */
	rec_t*		rec,	/* in/out: record */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	page_zip_des_t*	page_zip,/* in: compressed page whose uncompressed
				part will be updated, or NULL */
	enum trx_rb_ctx	rb_ctx,	/* in: rollback context */
	mtr_t*		mtr)	/* in: mini-transaction handle which contains
				an X-latch to record page and to the index
				tree */
{
	ulint	n_fields;
	ulint	i;

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX));
	/* Free possible externally stored fields in the record */

	ut_ad(dict_table_is_comp(index->table) == !!rec_offs_comp(offsets));
	n_fields = rec_offs_n_fields(offsets);

	for (i = 0; i < n_fields; i++) {
		if (rec_offs_nth_extern(offsets, i)) {
			ulint	len;
			byte*	data
				= rec_get_nth_field(rec, offsets, i, &len);
			ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);

			btr_free_externally_stored_field(
				index, data + len - BTR_EXTERN_FIELD_REF_SIZE,
				rec, offsets, page_zip, i, rb_ctx, mtr);
		}
	}
}

/***************************************************************
Frees the externally stored fields for a record, if the field is mentioned
in the update vector. */
static
void
btr_rec_free_updated_extern_fields(
/*===============================*/
	dict_index_t*	index,	/* in: index of rec; the index tree MUST be
				X-latched */
	rec_t*		rec,	/* in/out: record */
	page_zip_des_t*	page_zip,/* in: compressed page whose uncompressed
				part will be updated, or NULL */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	const upd_t*	update,	/* in: update vector */
	enum trx_rb_ctx	rb_ctx,	/* in: rollback context */
	mtr_t*		mtr)	/* in: mini-transaction handle which contains
				an X-latch to record page and to the tree */
{
	ulint	n_fields;
	ulint	i;

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX));

	/* Free possible externally stored fields in the record */

	n_fields = upd_get_n_fields(update);

	for (i = 0; i < n_fields; i++) {
		const upd_field_t* ufield = upd_get_nth_field(update, i);

		if (rec_offs_nth_extern(offsets, ufield->field_no)) {
			ulint	len;
			byte*	data = rec_get_nth_field(
				rec, offsets, ufield->field_no, &len);
			ut_a(len >= BTR_EXTERN_FIELD_REF_SIZE);

			btr_free_externally_stored_field(
				index, data + len - BTR_EXTERN_FIELD_REF_SIZE,
				rec, offsets, page_zip,
				ufield->field_no, rb_ctx, mtr);
		}
	}
}

/***********************************************************************
Copies the prefix of an uncompressed BLOB.  The clustered index record
that points to this BLOB must be protected by a lock or a page latch. */
static
ulint
btr_copy_blob_prefix(
/*=================*/
				/* out: number of bytes written to buf */
	byte*		buf,	/* out: the externally stored part of
				the field, or a prefix of it */
	ulint		len,	/* in: length of buf, in bytes */
	ulint		space_id,/* in: space id of the BLOB pages */
	ulint		page_no,/* in: page number of the first BLOB page */
	ulint		offset)	/* in: offset on the first BLOB page */
{
	ulint	copied_len	= 0;

	for (;;) {
		mtr_t		mtr;
		buf_block_t*	block;
		const page_t*	page;
		const byte*	blob_header;
		ulint		part_len;
		ulint		copy_len;

		mtr_start(&mtr);

		block = buf_page_get(space_id, 0, page_no, RW_S_LATCH, &mtr);
		buf_block_dbg_add_level(block, SYNC_EXTERN_STORAGE);
		page = buf_block_get_frame(block);

		btr_check_blob_fil_page_type(space_id, page_no, page, TRUE);

		blob_header = page + offset;
		part_len = btr_blob_get_part_len(blob_header);
		copy_len = ut_min(part_len, len - copied_len);

		memcpy(buf + copied_len,
		       blob_header + BTR_BLOB_HDR_SIZE, copy_len);
		copied_len += copy_len;

		page_no = btr_blob_get_next_page_no(blob_header);

		mtr_commit(&mtr);

		if (page_no == FIL_NULL || copy_len != part_len) {
			return(copied_len);
		}

		/* On other BLOB pages except the first the BLOB header
		always is at the page data start: */

		offset = FIL_PAGE_DATA;

		ut_ad(copied_len <= len);
	}
}

/***********************************************************************
Copies the prefix of a compressed BLOB.  The clustered index record
that points to this BLOB must be protected by a lock or a page latch. */
static
void
btr_copy_zblob_prefix(
/*==================*/
	z_stream*	d_stream,/* in/out: the decompressing stream */
	ulint		zip_size,/* in: compressed BLOB page size */
	ulint		space_id,/* in: space id of the BLOB pages */
	ulint		page_no,/* in: page number of the first BLOB page */
	ulint		offset)	/* in: offset on the first BLOB page */
{
	ulint	page_type = FIL_PAGE_TYPE_ZBLOB;

	ut_ad(ut_is_2pow(zip_size));
	ut_ad(zip_size >= PAGE_ZIP_MIN_SIZE);
	ut_ad(zip_size <= UNIV_PAGE_SIZE);
	ut_ad(space_id);

	for (;;) {
		buf_page_t*	bpage;
		int		err;
		ulint		next_page_no;

		/* There is no latch on bpage directly.  Instead,
		bpage is protected by the B-tree page latch that
		is being held on the clustered index record, or,
		in row_merge_copy_blobs(), by an exclusive table lock. */
		bpage = buf_page_get_zip(space_id, zip_size, page_no);

		if (UNIV_UNLIKELY(!bpage)) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Cannot load"
				" compressed BLOB"
				" page %lu space %lu\n",
				(ulong) page_no, (ulong) space_id);
			return;
		}

		if (UNIV_UNLIKELY
		    (fil_page_get_type(bpage->zip.data) != page_type)) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Unexpected type %lu of"
				" compressed BLOB"
				" page %lu space %lu\n",
				(ulong) fil_page_get_type(bpage->zip.data),
				(ulong) page_no, (ulong) space_id);
			goto end_of_blob;
		}

		next_page_no = mach_read_from_4(bpage->zip.data + offset);

		if (UNIV_LIKELY(offset == FIL_PAGE_NEXT)) {
			/* When the BLOB begins at page header,
			the compressed data payload does not
			immediately follow the next page pointer. */
			offset = FIL_PAGE_DATA;
		} else {
			offset += 4;
		}

		d_stream->next_in = bpage->zip.data + offset;
		d_stream->avail_in = zip_size - offset;

		err = inflate(d_stream, Z_NO_FLUSH);
		switch (err) {
		case Z_OK:
			if (!d_stream->avail_out) {
				goto end_of_blob;
			}
			break;
		case Z_STREAM_END:
			if (next_page_no == FIL_NULL) {
				goto end_of_blob;
			}
			/* fall through */
		default:
inflate_error:
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: inflate() of"
				" compressed BLOB"
				" page %lu space %lu returned %d (%s)\n",
				(ulong) page_no, (ulong) space_id,
				err, d_stream->msg);
		case Z_BUF_ERROR:
			goto end_of_blob;
		}

		if (next_page_no == FIL_NULL) {
			if (!d_stream->avail_in) {
				ut_print_timestamp(stderr);
				fprintf(stderr,
					"  InnoDB: unexpected end of"
					" compressed BLOB"
					" page %lu space %lu\n",
					(ulong) page_no,
					(ulong) space_id);
			} else {
				err = inflate(d_stream, Z_FINISH);
				switch (err) {
				case Z_STREAM_END:
				case Z_BUF_ERROR:
					break;
				default:
					goto inflate_error;
				}
			}

end_of_blob:
			buf_page_release_zip(bpage);
			return;
		}

		buf_page_release_zip(bpage);

		/* On other BLOB pages except the first
		the BLOB header always is at the page header: */

		page_no = next_page_no;
		offset = FIL_PAGE_NEXT;
		page_type = FIL_PAGE_TYPE_ZBLOB2;
	}
}

/***********************************************************************
Copies the prefix of an externally stored field of a record.  The
clustered index record that points to this BLOB must be protected by a
lock or a page latch. */
static
ulint
btr_copy_externally_stored_field_prefix_low(
/*========================================*/
				/* out: number of bytes written to buf */
	byte*		buf,	/* out: the externally stored part of
				the field, or a prefix of it */
	ulint		len,	/* in: length of buf, in bytes */
	ulint		zip_size,/* in: nonzero=compressed BLOB page size,
				zero for uncompressed BLOBs */
	ulint		space_id,/* in: space id of the first BLOB page */
	ulint		page_no,/* in: page number of the first BLOB page */
	ulint		offset)	/* in: offset on the first BLOB page */
{
	if (UNIV_UNLIKELY(len == 0)) {
		return(0);
	}

	if (UNIV_UNLIKELY(zip_size)) {
		int		err;
		z_stream	d_stream;
		mem_heap_t*	heap;

		/* Zlib inflate needs 32 kilobytes for the default
		window size, plus a few kilobytes for small objects. */
		heap = mem_heap_create(40000);
		page_zip_set_alloc(&d_stream, heap);

		err = inflateInit(&d_stream);
		ut_a(err == Z_OK);

		d_stream.next_out = buf;
		d_stream.avail_out = len;
		d_stream.avail_in = 0;

		btr_copy_zblob_prefix(&d_stream, zip_size,
				      space_id, page_no, offset);
		inflateEnd(&d_stream);
		mem_heap_free(heap);
		return(d_stream.total_out);
	} else {
		return(btr_copy_blob_prefix(buf, len, space_id,
					    page_no, offset));
	}
}

/***********************************************************************
Copies the prefix of an externally stored field of a record.  The
clustered index record must be protected by a lock or a page latch. */
UNIV_INTERN
ulint
btr_copy_externally_stored_field_prefix(
/*====================================*/
				/* out: the length of the copied field,
				or 0 if the column was being or has been
				deleted */
	byte*		buf,	/* out: the field, or a prefix of it */
	ulint		len,	/* in: length of buf, in bytes */
	ulint		zip_size,/* in: nonzero=compressed BLOB page size,
				zero for uncompressed BLOBs */
	const byte*	data,	/* in: 'internally' stored part of the
				field containing also the reference to
				the external part; must be protected by
				a lock or a page latch */
	ulint		local_len)/* in: length of data, in bytes */
{
	ulint	space_id;
	ulint	page_no;
	ulint	offset;

	ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

	local_len -= BTR_EXTERN_FIELD_REF_SIZE;

	if (UNIV_UNLIKELY(local_len >= len)) {
		memcpy(buf, data, len);
		return(len);
	}

	memcpy(buf, data, local_len);
	data += local_len;

	ut_a(memcmp(data, field_ref_zero, BTR_EXTERN_FIELD_REF_SIZE));

	if (!mach_read_from_4(data + BTR_EXTERN_LEN + 4)) {
		/* The externally stored part of the column has been
		(partially) deleted.  Signal the half-deleted BLOB
		to the caller. */

		return(0);
	}

	space_id = mach_read_from_4(data + BTR_EXTERN_SPACE_ID);

	page_no = mach_read_from_4(data + BTR_EXTERN_PAGE_NO);

	offset = mach_read_from_4(data + BTR_EXTERN_OFFSET);

	return(local_len
	       + btr_copy_externally_stored_field_prefix_low(buf + local_len,
							     len - local_len,
							     zip_size,
							     space_id, page_no,
							     offset));
}

/***********************************************************************
Copies an externally stored field of a record to mem heap.  The
clustered index record must be protected by a lock or a page latch. */
static
byte*
btr_copy_externally_stored_field(
/*=============================*/
				/* out: the whole field copied to heap */
	ulint*		len,	/* out: length of the whole field */
	const byte*	data,	/* in: 'internally' stored part of the
				field containing also the reference to
				the external part; must be protected by
				a lock or a page latch */
	ulint		zip_size,/* in: nonzero=compressed BLOB page size,
				zero for uncompressed BLOBs */
	ulint		local_len,/* in: length of data */
	mem_heap_t*	heap)	/* in: mem heap */
{
	ulint	space_id;
	ulint	page_no;
	ulint	offset;
	ulint	extern_len;
	byte*	buf;

	ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

	local_len -= BTR_EXTERN_FIELD_REF_SIZE;

	space_id = mach_read_from_4(data + local_len + BTR_EXTERN_SPACE_ID);

	page_no = mach_read_from_4(data + local_len + BTR_EXTERN_PAGE_NO);

	offset = mach_read_from_4(data + local_len + BTR_EXTERN_OFFSET);

	/* Currently a BLOB cannot be bigger than 4 GB; we
	leave the 4 upper bytes in the length field unused */

	extern_len = mach_read_from_4(data + local_len + BTR_EXTERN_LEN + 4);

	buf = mem_heap_alloc(heap, local_len + extern_len);

	memcpy(buf, data, local_len);
	*len = local_len
		+ btr_copy_externally_stored_field_prefix_low(buf + local_len,
							      extern_len,
							      zip_size,
							      space_id,
							      page_no, offset);

	return(buf);
}

/***********************************************************************
Copies an externally stored field of a record to mem heap. */
UNIV_INTERN
byte*
btr_rec_copy_externally_stored_field(
/*=================================*/
				/* out: the field copied to heap */
	const rec_t*	rec,	/* in: record in a clustered index;
				must be protected by a lock or a page latch */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		zip_size,/* in: nonzero=compressed BLOB page size,
				zero for uncompressed BLOBs */
	ulint		no,	/* in: field number */
	ulint*		len,	/* out: length of the field */
	mem_heap_t*	heap)	/* in: mem heap */
{
	ulint		local_len;
	const byte*	data;

	ut_a(rec_offs_nth_extern(offsets, no));

	/* An externally stored field can contain some initial
	data from the field, and in the last 20 bytes it has the
	space id, page number, and offset where the rest of the
	field data is stored, and the data length in addition to
	the data stored locally. We may need to store some data
	locally to get the local record length above the 128 byte
	limit so that field offsets are stored in two bytes, and
	the extern bit is available in those two bytes. */

	data = rec_get_nth_field(rec, offsets, no, &local_len);

	return(btr_copy_externally_stored_field(len, data,
						zip_size, local_len, heap));
}
