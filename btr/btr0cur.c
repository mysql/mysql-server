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

(c) 1994-2001 Innobase Oy

Created 10/16/1994 Heikki Tuuri
*******************************************************/

#include "btr0cur.h"

#ifdef UNIV_NONINL
#include "btr0cur.ic"
#endif

#include "page0page.h"
#include "rem0rec.h"
#include "rem0cmp.h"
#include "btr0btr.h"
#include "btr0sea.h"
#include "row0upd.h"
#include "trx0rec.h"
#include "que0que.h"
#include "row0row.h"
#include "srv0srv.h"
#include "ibuf0ibuf.h"
#include "lock0lock.h"

#ifdef UNIV_DEBUG
/* If the following is set to TRUE, this module prints a lot of
trace information of individual record operations */
ibool	btr_cur_print_record_ops = FALSE;
#endif /* UNIV_DEBUG */

ulint	btr_cur_n_non_sea	= 0;
ulint	btr_cur_n_sea		= 0;
ulint	btr_cur_n_non_sea_old	= 0;
ulint	btr_cur_n_sea_old	= 0;

/* In the optimistic insert, if the insert does not fit, but this much space
can be released by page reorganize, then it is reorganized */

#define BTR_CUR_PAGE_REORGANIZE_LIMIT	(UNIV_PAGE_SIZE / 32)

/* When estimating number of different kay values in an index sample
this many index pages */
#define BTR_KEY_VAL_ESTIMATE_N_PAGES	8

/* The structure of a BLOB part header */
/*--------------------------------------*/
#define BTR_BLOB_HDR_PART_LEN		0	/* BLOB part len on this
						page */
#define BTR_BLOB_HDR_NEXT_PAGE_NO	4	/* next BLOB part page no,
						FIL_NULL if none */
/*--------------------------------------*/
#define BTR_BLOB_HDR_SIZE		8

/***********************************************************************
Marks all extern fields in a record as owned by the record. This function
should be called if the delete mark of a record is removed: a not delete
marked record always owns all its extern fields. */
static
void
btr_cur_unmark_extern_fields(
/*=========================*/
	rec_t*		rec,	/* in: record in a clustered index */
	mtr_t*		mtr,	/* in: mtr */
	const ulint*	offsets);/* in: array returned by rec_get_offsets() */
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
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	upd_t*		update,	/* in: update vector */
	ibool		do_not_free_inherited,/* in: TRUE if called in a
				rollback and we do not want to free
				inherited fields */
	mtr_t*		mtr);	/* in: mini-transaction handle which contains
				an X-latch to record page and to the tree */
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
	ulint		page_no,	/* in: page number of the leaf */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor,		/* in: cursor */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint	left_page_no;
	ulint	right_page_no;
	page_t*	get_page;

	ut_ad(page && mtr);

	if (latch_mode == BTR_SEARCH_LEAF) {

		get_page = btr_page_get(space, page_no, RW_S_LATCH, mtr);
		ut_a(page_is_comp(get_page) == page_is_comp(page));
		buf_block_align(get_page)->check_index_page_at_flush = TRUE;

	} else if (latch_mode == BTR_MODIFY_LEAF) {

		get_page = btr_page_get(space, page_no, RW_X_LATCH, mtr);
		ut_a(page_is_comp(get_page) == page_is_comp(page));
		buf_block_align(get_page)->check_index_page_at_flush = TRUE;

	} else if (latch_mode == BTR_MODIFY_TREE) {

		/* x-latch also brothers from left to right */
		left_page_no = btr_page_get_prev(page, mtr);

		if (left_page_no != FIL_NULL) {
			get_page = btr_page_get(space, left_page_no,
							RW_X_LATCH, mtr);
#ifdef UNIV_BTR_DEBUG
			ut_a(btr_page_get_next(get_page, mtr)
					== buf_frame_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */
			ut_a(page_is_comp(get_page) == page_is_comp(page));
			buf_block_align(get_page)->check_index_page_at_flush =
									TRUE;
		}

		get_page = btr_page_get(space, page_no, RW_X_LATCH, mtr);
		ut_a(page_is_comp(get_page) == page_is_comp(page));
		buf_block_align(get_page)->check_index_page_at_flush = TRUE;

		right_page_no = btr_page_get_next(page, mtr);

		if (right_page_no != FIL_NULL) {
			get_page = btr_page_get(space, right_page_no,
							RW_X_LATCH, mtr);
#ifdef UNIV_BTR_DEBUG
			ut_a(btr_page_get_prev(get_page, mtr)
					== buf_frame_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */
			buf_block_align(get_page)->check_index_page_at_flush =
									TRUE;
		}

	} else if (latch_mode == BTR_SEARCH_PREV) {

		/* s-latch also left brother */
		left_page_no = btr_page_get_prev(page, mtr);

		if (left_page_no != FIL_NULL) {
			cursor->left_page = btr_page_get(space, left_page_no,
							RW_S_LATCH, mtr);
#ifdef UNIV_BTR_DEBUG
			ut_a(btr_page_get_next(cursor->left_page, mtr)
					== buf_frame_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */
			ut_a(page_is_comp(cursor->left_page) ==
					page_is_comp(page));
			buf_block_align(
			cursor->left_page)->check_index_page_at_flush = TRUE;
		}

		get_page = btr_page_get(space, page_no, RW_S_LATCH, mtr);
		ut_a(page_is_comp(get_page) == page_is_comp(page));
		buf_block_align(get_page)->check_index_page_at_flush = TRUE;

	} else if (latch_mode == BTR_MODIFY_PREV) {

		/* x-latch also left brother */
		left_page_no = btr_page_get_prev(page, mtr);

		if (left_page_no != FIL_NULL) {
			cursor->left_page = btr_page_get(space, left_page_no,
							RW_X_LATCH, mtr);
#ifdef UNIV_BTR_DEBUG
			ut_a(btr_page_get_next(cursor->left_page, mtr)
					== buf_frame_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */
			ut_a(page_is_comp(cursor->left_page) ==
					page_is_comp(page));
			buf_block_align(
			cursor->left_page)->check_index_page_at_flush = TRUE;
		}

		get_page = btr_page_get(space, page_no, RW_X_LATCH, mtr);
		ut_a(page_is_comp(get_page) == page_is_comp(page));
		buf_block_align(get_page)->check_index_page_at_flush = TRUE;
	} else {
		ut_error;
	}
}

/************************************************************************
Searches an index tree and positions a tree cursor on a given level.
NOTE: n_fields_cmp in tuple must be set so that it cannot be compared
to node pointer page number fields on the upper levels of the tree!
Note that if mode is PAGE_CUR_LE, which is used in inserts, then
cursor->up_match and cursor->low_match both will have sensible values.
If mode is PAGE_CUR_GE, then up_match will a have a sensible value. */

void
btr_cur_search_to_nth_level(
/*========================*/
	dict_index_t*	index,	/* in: index */
	ulint		level,	/* in: the tree level of search */
	dtuple_t*	tuple,	/* in: data tuple; NOTE: n_fields_cmp in
				tuple must be set so that it cannot get
				compared to the node ptr page number field! */
	ulint		mode,	/* in: PAGE_CUR_L, ...;
				Inserts should always be made using
				PAGE_CUR_LE to search the position! */
	ulint		latch_mode, /* in: BTR_SEARCH_LEAF, ..., ORed with
				BTR_INSERT and BTR_ESTIMATE;
				cursor->left_page is used to store a pointer
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
	dict_tree_t*	tree;
	page_cur_t*	page_cursor;
	page_t*		page;
	page_t*		guess;
	rec_t*		node_ptr;
	ulint		page_no;
	ulint		space;
	ulint		up_match;
	ulint		up_bytes;
	ulint		low_match;
	ulint		low_bytes;
	ulint		height;
	ulint		savepoint;
	ulint		rw_latch;
	ulint		page_mode;
	ulint		insert_planned;
	ulint		buf_mode;
	ulint		estimate;
	ulint		ignore_sec_unique;
	ulint		root_height = 0; /* remove warning */
#ifdef BTR_CUR_ADAPT
	btr_search_t*	info;
#endif
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;
	/* Currently, PAGE_CUR_LE is the only search mode used for searches
	ending to upper levels */

	ut_ad(level == 0 || mode == PAGE_CUR_LE);
	ut_ad(dict_tree_check_search_tuple(index->tree, tuple));
	ut_ad(!(index->type & DICT_IBUF) || ibuf_inside());
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
	if (btr_search_latch.writer == RW_LOCK_NOT_LOCKED
		&& latch_mode <= BTR_MODIFY_LEAF && info->last_hash_succ
		&& !estimate
#ifdef PAGE_CUR_LE_OR_EXTENDS
		&& mode != PAGE_CUR_LE_OR_EXTENDS
#endif /* PAGE_CUR_LE_OR_EXTENDS */
		&& srv_use_adaptive_hash_indexes
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
#endif
#endif
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

	tree = index->tree;

	if (latch_mode == BTR_MODIFY_TREE) {
		mtr_x_lock(dict_tree_get_lock(tree), mtr);

	} else if (latch_mode == BTR_CONT_MODIFY_TREE) {
		/* Do nothing */
		ut_ad(mtr_memo_contains(mtr, dict_tree_get_lock(tree),
							MTR_MEMO_X_LOCK));
	} else {
		mtr_s_lock(dict_tree_get_lock(tree), mtr);
	}

	page_cursor = btr_cur_get_page_cur(cursor);

	space = dict_tree_get_space(tree);
	page_no = dict_tree_get_page(tree);

	up_match = 0;
	up_bytes = 0;
	low_match = 0;
	low_bytes = 0;

	height = ULINT_UNDEFINED;
	rw_latch = RW_NO_LATCH;
	buf_mode = BUF_GET;

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
		if ((height == 0) && (latch_mode <= BTR_MODIFY_LEAF)) {

			rw_latch = latch_mode;

			if (insert_planned && ibuf_should_try(index,
							ignore_sec_unique)) {

				/* Try insert to the insert buffer if the
				page is not in the buffer pool */

				buf_mode = BUF_GET_IF_IN_POOL;
			}
		}
retry_page_get:
		page = buf_page_get_gen(space, page_no, rw_latch, guess,
					buf_mode,
					__FILE__, __LINE__,
					mtr);
		if (page == NULL) {
			/* This must be a search to perform an insert;
			try insert to the insert buffer */

			ut_ad(buf_mode == BUF_GET_IF_IN_POOL);
			ut_ad(insert_planned);
			ut_ad(cursor->thr);

			if (ibuf_should_try(index, ignore_sec_unique) &&
				ibuf_insert(tuple, index, space, page_no,
							cursor->thr)) {
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

		buf_block_align(page)->check_index_page_at_flush = TRUE;

#ifdef UNIV_SYNC_DEBUG
		if (rw_latch != RW_NO_LATCH) {
			buf_page_dbg_add_level(page, SYNC_TREE_NODE);
		}
#endif
		ut_ad(0 == ut_dulint_cmp(tree->id,
					btr_page_get_index_id(page)));

		if (height == ULINT_UNDEFINED) {
			/* We are in the root node */

			height = btr_page_get_level(page, mtr);
			root_height = height;
			cursor->tree_height = root_height + 1;
#ifdef BTR_CUR_ADAPT
			if (page != guess) {
				info->root_guess = page;
			}
#endif
		}

		if (height == 0) {
			if (rw_latch == RW_NO_LATCH) {

				btr_cur_latch_leaves(page, space,
						page_no, latch_mode, cursor,
						mtr);
			}

			if ((latch_mode != BTR_MODIFY_TREE)
				&& (latch_mode != BTR_CONT_MODIFY_TREE)) {

				/* Release the tree s-latch */

				mtr_release_s_latch_at_savepoint(
						mtr, savepoint,
						dict_tree_get_lock(tree));
			}

			page_mode = mode;
		}

		page_cur_search_with_match(page, index, tuple, page_mode,
					&up_match, &up_bytes,
					&low_match, &low_bytes, page_cursor);
		if (estimate) {
			btr_cur_add_path_info(cursor, height, root_height);
		}

		/* If this is the desired level, leave the loop */

		ut_ad(height
		== btr_page_get_level(page_cur_get_page(page_cursor), mtr));

		if (level == height) {

			if (level > 0) {
				/* x-latch the page */
				page = btr_page_get(space,
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
		if (srv_use_adaptive_hash_indexes) {

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
	dict_tree_t*	tree;
	page_t*		page;
	ulint		page_no;
	ulint		space;
	ulint		height;
	ulint		root_height = 0; /* remove warning */
	rec_t*		node_ptr;
	ulint		estimate;
	ulint		savepoint;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	estimate = latch_mode & BTR_ESTIMATE;
	latch_mode = latch_mode & ~BTR_ESTIMATE;

	tree = index->tree;

	/* Store the position of the tree latch we push to mtr so that we
	know how to release it when we have latched the leaf node */

	savepoint = mtr_set_savepoint(mtr);

	if (latch_mode == BTR_MODIFY_TREE) {
		mtr_x_lock(dict_tree_get_lock(tree), mtr);
	} else {
		mtr_s_lock(dict_tree_get_lock(tree), mtr);
	}

	page_cursor = btr_cur_get_page_cur(cursor);
	cursor->index = index;

	space = dict_tree_get_space(tree);
	page_no = dict_tree_get_page(tree);

	height = ULINT_UNDEFINED;

	for (;;) {
		page = buf_page_get_gen(space, page_no, RW_NO_LATCH, NULL,
					BUF_GET,
					__FILE__, __LINE__,
					mtr);
		ut_ad(0 == ut_dulint_cmp(tree->id,
						btr_page_get_index_id(page)));

		buf_block_align(page)->check_index_page_at_flush = TRUE;

		if (height == ULINT_UNDEFINED) {
			/* We are in the root node */

			height = btr_page_get_level(page, mtr);
			root_height = height;
		}

		if (height == 0) {
			btr_cur_latch_leaves(page, space, page_no,
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
						dict_tree_get_lock(tree));
			}
		}

		if (from_left) {
			page_cur_set_before_first(page, page_cursor);
		} else {
			page_cur_set_after_last(page, page_cursor);
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

void
btr_cur_open_at_rnd_pos(
/*====================*/
	dict_index_t*	index,		/* in: index */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor,		/* in/out: B-tree cursor */
	mtr_t*		mtr)		/* in: mtr */
{
	page_cur_t*	page_cursor;
	dict_tree_t*	tree;
	page_t*		page;
	ulint		page_no;
	ulint		space;
	ulint		height;
	rec_t*		node_ptr;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	tree = index->tree;

	if (latch_mode == BTR_MODIFY_TREE) {
		mtr_x_lock(dict_tree_get_lock(tree), mtr);
	} else {
		mtr_s_lock(dict_tree_get_lock(tree), mtr);
	}

	page_cursor = btr_cur_get_page_cur(cursor);
	cursor->index = index;

	space = dict_tree_get_space(tree);
	page_no = dict_tree_get_page(tree);

	height = ULINT_UNDEFINED;

	for (;;) {
		page = buf_page_get_gen(space, page_no, RW_NO_LATCH, NULL,
					BUF_GET,
					__FILE__, __LINE__,
					mtr);
		ut_ad(0 == ut_dulint_cmp(tree->id,
						btr_page_get_index_id(page)));

		if (height == ULINT_UNDEFINED) {
			/* We are in the root node */

			height = btr_page_get_level(page, mtr);
		}

		if (height == 0) {
			btr_cur_latch_leaves(page, space, page_no,
						latch_mode, cursor, mtr);
		}

		page_cur_open_on_rnd_user_rec(page, page_cursor);

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
be freed by reorganizing. Differs from _optimistic_insert because
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
	dtuple_t*	tuple,	/* in: tuple to insert; the size info need not
				have been stored to tuple */
	ibool*		reorg,	/* out: TRUE if reorganization occurred */
	mtr_t*		mtr)	/* in: mtr */
{
	page_cur_t*	page_cursor;
	page_t*		page;
	rec_t*		rec;

	ut_ad(dtuple_check_typed(tuple));

	*reorg = FALSE;

	page = btr_cur_get_page(cursor);

	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
							MTR_MEMO_PAGE_X_FIX));
	page_cursor = btr_cur_get_page_cur(cursor);

	/* Now, try the insert */
	rec = page_cur_tuple_insert(page_cursor, tuple, cursor->index, mtr);

	if (!rec) {
		/* If record did not fit, reorganize */

		btr_page_reorganize(page, cursor->index, mtr);

		*reorg = TRUE;

		page_cur_search(page, cursor->index, tuple,
						PAGE_CUR_LE, page_cursor);

		rec = page_cur_tuple_insert(page_cursor, tuple,
						cursor->index, mtr);
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
	dtuple_t*	entry,	/* in: entry to insert */
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

	err = lock_rec_insert_check_and_lock(flags, rec, index, thr, inherit);

	if (err != DB_SUCCESS) {

		return(err);
	}

	if ((index->type & DICT_CLUSTERED) && !(index->type & DICT_IBUF)) {

		err = trx_undo_report_row_operation(flags, TRX_UNDO_INSERT_OP,
					thr, index, entry, NULL, 0, NULL,
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
	fprintf(stderr, "Trx with id %lu %lu going to ",
		ut_dulint_get_high(trx->id),
		ut_dulint_get_low(trx->id));
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
	dtuple_t*	entry,	/* in: entry to insert */
	rec_t**		rec,	/* out: pointer to inserted record if
				succeed */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or
				NULL */
	que_thr_t*	thr,	/* in: query thread or NULL */
	mtr_t*		mtr)	/* in: mtr */
{
	big_rec_t*	big_rec_vec	= NULL;
	dict_index_t*	index;
	page_cur_t*	page_cursor;
	page_t*		page;
	ulint		max_size;
	rec_t*		dummy_rec;
	ulint		level;
	ibool		reorg;
	ibool		inherit;
	ulint		rec_size;
	ulint		type;
	ulint		err;

	*big_rec = NULL;

	page = btr_cur_get_page(cursor);
	index = cursor->index;

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

	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
							MTR_MEMO_PAGE_X_FIX));
	max_size = page_get_max_insert_size_after_reorganize(page, 1);
	level = btr_page_get_level(page, mtr);

calculate_sizes_again:
	/* Calculate the record size when entry is converted to a record */
	rec_size = rec_get_converted_size(index, entry);

	if (rec_size >=
		ut_min(page_get_free_space_of_empty(page_is_comp(page)) / 2,
		REC_MAX_DATA_SIZE)) {

		/* The record is so big that we have to store some fields
		externally on separate database pages */

		big_rec_vec = dtuple_convert_big_rec(index, entry, NULL, 0);

		if (big_rec_vec == NULL) {

			return(DB_TOO_BIG_RECORD);
		}

		goto calculate_sizes_again;
	}

	/* If there have been many consecutive inserts, and we are on the leaf
	level, check if we have to split the page to reserve enough free space
	for future updates of records. */

	type = index->type;

	if ((type & DICT_CLUSTERED)
		&& (dict_tree_get_space_reserve(index->tree) + rec_size > max_size)
		&& (page_get_n_recs(page) >= 2)
		&& (0 == level)
		&& (btr_page_get_split_rec_to_right(cursor, &dummy_rec)
			|| btr_page_get_split_rec_to_left(cursor, &dummy_rec))) {

		if (big_rec_vec) {
			dtuple_convert_back_big_rec(index, entry, big_rec_vec);
		}

		return(DB_FAIL);
	}

	if (!(((max_size >= rec_size)
				&& (max_size >= BTR_CUR_PAGE_REORGANIZE_LIMIT))
			|| (page_get_max_insert_size(page, 1) >= rec_size)
			|| (page_get_n_recs(page) <= 1))) {

		if (big_rec_vec) {
			dtuple_convert_back_big_rec(index, entry, big_rec_vec);
		}
		return(DB_FAIL);
	}

	/* Check locks and write to the undo log, if specified */
	err = btr_cur_ins_lock_and_undo(flags, cursor, entry, thr, &inherit);

	if (err != DB_SUCCESS) {

		if (big_rec_vec) {
			dtuple_convert_back_big_rec(index, entry, big_rec_vec);
		}
		return(err);
	}

	page_cursor = btr_cur_get_page_cur(cursor);

	reorg = FALSE;

	/* Now, try the insert */

	*rec = page_cur_insert_rec_low(page_cursor, entry, index,
							NULL, NULL, mtr);
	if (UNIV_UNLIKELY(!(*rec))) {
		/* If the record did not fit, reorganize */
		btr_page_reorganize(page, index, mtr);

		ut_ad(page_get_max_insert_size(page, 1) == max_size);

		reorg = TRUE;

		page_cur_search(page, index, entry, PAGE_CUR_LE, page_cursor);

		*rec = page_cur_tuple_insert(page_cursor, entry, index, mtr);

		if (UNIV_UNLIKELY(!*rec)) {
			fputs("InnoDB: Error: cannot insert tuple ", stderr);
			dtuple_print(stderr, entry);
			fputs(" into ", stderr);
			dict_index_name_print(stderr, thr_get_trx(thr), index);
			fprintf(stderr, "\nInnoDB: max insert size %lu\n",
				(ulong) max_size);
			ut_error;
		}
	}

#ifdef BTR_CUR_HASH_ADAPT
	if (!reorg && (0 == level) && (cursor->flag == BTR_CUR_HASH)) {
		btr_search_update_hash_node_on_insert(cursor);
	} else {
		btr_search_update_hash_on_insert(cursor);
	}
#endif

	if (!(flags & BTR_NO_LOCKING_FLAG) && inherit) {

		lock_update_insert(*rec);
	}

/*	fprintf(stderr, "Insert into page %lu, max ins size %lu,"
		" rec %lu ind type %lu\n",
			buf_frame_get_page_no(page), max_size,
					rec_size + PAGE_DIR_SLOT_SIZE, type);
*/
	if (!(type & DICT_CLUSTERED)) {
		/* We have added a record to page: update its free bits */
		ibuf_update_free_bits_if_full(cursor->index, page, max_size,
					rec_size + PAGE_DIR_SLOT_SIZE);
	}

	*big_rec = big_rec_vec;

	return(DB_SUCCESS);
}

/*****************************************************************
Performs an insert on a page of an index tree. It is assumed that mtr
holds an x-latch on the tree and on the cursor page. If the insert is
made on the leaf level, to avoid deadlocks, mtr must also own x-latches
to brothers of page, if those brothers exist. */

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
	dtuple_t*	entry,	/* in: entry to insert */
	rec_t**		rec,	/* out: pointer to inserted record if
				succeed */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or
				NULL */
	que_thr_t*	thr,	/* in: query thread or NULL */
	mtr_t*		mtr)	/* in: mtr */
{
	dict_index_t*	index		= cursor->index;
	big_rec_t*	big_rec_vec	= NULL;
	page_t*		page;
	ulint		err;
	ibool		dummy_inh;
	ibool		success;
	ulint		n_extents	= 0;
	ulint		n_reserved;

	ut_ad(dtuple_check_typed(entry));

	*big_rec = NULL;

	page = btr_cur_get_page(cursor);

	ut_ad(mtr_memo_contains(mtr,
				dict_tree_get_lock(btr_cur_get_tree(cursor)),
							MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
							MTR_MEMO_PAGE_X_FIX));

	/* Try first an optimistic insert; reset the cursor flag: we do not
	assume anything of how it was positioned */

	cursor->flag = BTR_CUR_BINARY;

	err = btr_cur_optimistic_insert(flags, cursor, entry, rec, big_rec,
								thr, mtr);
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
			err = DB_OUT_OF_FILE_SPACE;

			return(err);
		}
	}

	if (rec_get_converted_size(index, entry) >=
		ut_min(page_get_free_space_of_empty(page_is_comp(page)) / 2,
		REC_MAX_DATA_SIZE)) {

		/* The record is so big that we have to store some fields
		externally on separate database pages */

		big_rec_vec = dtuple_convert_big_rec(index, entry, NULL, 0);

		if (big_rec_vec == NULL) {

			if (n_extents > 0) {
				fil_space_release_free_extents(index->space,
								n_reserved);
			}
			return(DB_TOO_BIG_RECORD);
		}
	}

	if (dict_tree_get_page(index->tree)
					== buf_frame_get_page_no(page)) {

		/* The page is the root page */
		*rec = btr_root_raise_and_insert(cursor, entry, mtr);
	} else {
		*rec = btr_page_split_and_insert(cursor, entry, mtr);
	}

	btr_cur_position(index, page_rec_get_prev(*rec), cursor);

#ifdef BTR_CUR_ADAPT
	btr_search_update_hash_on_insert(cursor);
#endif
	if (!(flags & BTR_NO_LOCKING_FLAG)) {

		lock_update_insert(*rec);
	}

	err = DB_SUCCESS;

	if (n_extents > 0) {
		fil_space_release_free_extents(index->space, n_reserved);
	}

	*big_rec = big_rec_vec;

	return(err);
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
	upd_t*		update,	/* in: update vector */
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

	if (!(index->type & DICT_CLUSTERED)) {
		/* We do undo logging only when we update a clustered index
		record */
		return(lock_sec_rec_modify_check_and_lock(flags, rec, index,
									thr));
	}

	/* Check if we have to wait for a lock: enqueue an explicit lock
	request if yes */

	err = DB_SUCCESS;

	if (!(flags & BTR_NO_LOCKING_FLAG)) {
		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		*offsets_ = (sizeof offsets_) / sizeof *offsets_;

		err = lock_clust_rec_modify_check_and_lock(flags, rec, index,
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
	upd_t*		update,		/* in: update vector */
	trx_t*		trx,		/* in: transaction */
	dulint		roll_ptr,	/* in: roll ptr */
	mtr_t*		mtr)		/* in: mtr */
{
	byte*	log_ptr;
	page_t*	page	= ut_align_down(rec, UNIV_PAGE_SIZE);
	ut_ad(flags < 256);
	ut_ad(!!page_is_comp(page) == dict_table_is_comp(index->table));

	log_ptr = mlog_open_and_write_index(mtr, rec, index, page_is_comp(page)
			? MLOG_COMP_REC_UPDATE_IN_PLACE
			: MLOG_REC_UPDATE_IN_PLACE,
			1 + DATA_ROLL_PTR_LEN + 14 + 2 + MLOG_BUF_MARGIN);

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
	mach_write_to_2(log_ptr, ut_align_offset(rec, UNIV_PAGE_SIZE));
	log_ptr += 2;

	row_upd_index_write_log(update, log_ptr, mtr);
}

/***************************************************************
Parses a redo log record of updating a record in-place. */

byte*
btr_cur_parse_update_in_place(
/*==========================*/
				/* out: end of log record or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	page_t*		page,	/* in: page or NULL */
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
		row_upd_rec_sys_fields_in_recovery(rec, offsets,
			pos, trx_id, roll_ptr);
	}

	row_upd_rec_in_place(rec, offsets, update);

func_exit:
	mem_heap_free(heap);

	return(ptr);
}

/*****************************************************************
Updates a record when the update causes no size changes in its fields.
We assume here that the ordering fields of the record do not change. */

ulint
btr_cur_update_in_place(
/*====================*/
				/* out: DB_SUCCESS or error number */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	upd_t*		update,	/* in: update vector */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	dict_index_t*	index;
	buf_block_t*	block;
	ulint		err;
	rec_t*		rec;
	dulint		roll_ptr	= ut_dulint_zero;
	trx_t*		trx;
	ulint		was_delete_marked;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));
	trx = thr_get_trx(thr);
	offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);
#ifdef UNIV_DEBUG
	if (btr_cur_print_record_ops && thr) {
		btr_cur_trx_report(trx, index, "update ");
		rec_print_new(stderr, rec, offsets);
	}
#endif /* UNIV_DEBUG */

	/* Do lock checking and undo logging */
	err = btr_cur_upd_lock_and_undo(flags, cursor, update, cmpl_info,
							thr, &roll_ptr);
	if (UNIV_UNLIKELY(err != DB_SUCCESS)) {

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
		return(err);
	}

	block = buf_block_align(rec);
	ut_ad(!!page_is_comp(buf_block_get_frame(block))
		== dict_table_is_comp(index->table));

	if (block->is_hashed) {
		/* The function row_upd_changes_ord_field_binary works only
		if the update vector was built for a clustered index, we must
		NOT call it if index is secondary */

		if (!(index->type & DICT_CLUSTERED)
			|| row_upd_changes_ord_field_binary(NULL, index, update)) {

			/* Remove possible hash index pointer to this record */
			btr_search_update_hash_on_delete(cursor);
		}

		rw_lock_x_lock(&btr_search_latch);
	}

	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_rec_sys_fields(rec, index, offsets, trx, roll_ptr);
	}

	/* FIXME: in a mixed tree, all records may not have enough ordering
	fields for btr search: */

	was_delete_marked = rec_get_deleted_flag(rec,
				page_is_comp(buf_block_get_frame(block)));

	row_upd_rec_in_place(rec, offsets, update);

	if (block->is_hashed) {
		rw_lock_x_unlock(&btr_search_latch);
	}

	btr_cur_update_in_place_log(flags, rec, index, update, trx, roll_ptr,
									mtr);
	if (was_delete_marked && !rec_get_deleted_flag(rec,
				page_is_comp(buf_block_get_frame(block)))) {
		/* The new updated record owns its possible externally
		stored fields */

		btr_cur_unmark_extern_fields(rec, mtr, offsets);
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

ulint
btr_cur_optimistic_update(
/*======================*/
				/* out: DB_SUCCESS, or DB_OVERFLOW if the
				updated record does not fit, DB_UNDERFLOW
				if the page would become too empty */
	ulint		flags,	/* in: undo logging and locking flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update;
				cursor stays valid and positioned on the
				same record */
	upd_t*		update,	/* in: update vector; this must also
				contain trx id and roll ptr fields */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	dict_index_t*	index;
	page_cur_t*	page_cursor;
	ulint		err;
	page_t*		page;
	rec_t*		rec;
	ulint		max_size;
	ulint		new_rec_size;
	ulint		old_rec_size;
	dtuple_t*	new_entry;
	dulint		roll_ptr;
	trx_t*		trx;
	mem_heap_t*	heap;
	ibool		reorganized	= FALSE;
	ulint		i;
	ulint*		offsets;

	page = btr_cur_get_page(cursor);
	rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	ut_ad(!!page_rec_is_comp(rec) == dict_table_is_comp(index->table));

	heap = mem_heap_create(1024);
	offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, &heap);

#ifdef UNIV_DEBUG
	if (btr_cur_print_record_ops && thr) {
		btr_cur_trx_report(thr_get_trx(thr), index, "update ");
		rec_print_new(stderr, rec, offsets);
	}
#endif /* UNIV_DEBUG */

	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
							MTR_MEMO_PAGE_X_FIX));
	if (!row_upd_changes_field_size_or_external(index, offsets, update)) {

		/* The simplest and the most common case: the update does not
		change the size of any field and none of the updated fields is
		externally stored in rec or update */
		mem_heap_free(heap);
		return(btr_cur_update_in_place(flags, cursor, update,
							cmpl_info, thr, mtr));
	}

	for (i = 0; i < upd_get_n_fields(update); i++) {
		if (upd_get_nth_field(update, i)->extern_storage) {

			/* Externally stored fields are treated in pessimistic
			update */

			mem_heap_free(heap);
			return(DB_OVERFLOW);
		}
	}

	if (rec_offs_any_extern(offsets)) {
		/* Externally stored fields are treated in pessimistic
		update */

		mem_heap_free(heap);
		return(DB_OVERFLOW);
	}

	page_cursor = btr_cur_get_page_cur(cursor);

	new_entry = row_rec_to_index_entry(ROW_COPY_DATA, index, rec, heap);

	row_upd_index_replace_new_col_vals_index_pos(new_entry, index, update,
		FALSE, NULL);
	old_rec_size = rec_offs_size(offsets);
	new_rec_size = rec_get_converted_size(index, new_entry);

	if (UNIV_UNLIKELY(new_rec_size >= page_get_free_space_of_empty(
				page_is_comp(page)) / 2)) {

		mem_heap_free(heap);

		return(DB_OVERFLOW);
	}

	max_size = old_rec_size
			+ page_get_max_insert_size_after_reorganize(page, 1);

	if (UNIV_UNLIKELY(page_get_data_size(page)
					- old_rec_size + new_rec_size
					< BTR_CUR_PAGE_COMPRESS_LIMIT)) {

		/* The page would become too empty */

		mem_heap_free(heap);

		return(DB_UNDERFLOW);
	}

	if (!(((max_size >= BTR_CUR_PAGE_REORGANIZE_LIMIT)
					&& (max_size >= new_rec_size))
			|| (page_get_n_recs(page) <= 1))) {

		/* There was not enough space, or it did not pay to
		reorganize: for simplicity, we decide what to do assuming a
		reorganization is needed, though it might not be necessary */

		mem_heap_free(heap);

		return(DB_OVERFLOW);
	}

	/* Do lock checking and undo logging */
	err = btr_cur_upd_lock_and_undo(flags, cursor, update, cmpl_info, thr,
								&roll_ptr);
	if (err != DB_SUCCESS) {

		mem_heap_free(heap);

		return(err);
	}

	/* Ok, we may do the replacement. Store on the page infimum the
	explicit locks on rec, before deleting rec (see the comment in
	.._pessimistic_update). */

	lock_rec_store_on_page_infimum(page, rec);

	btr_search_update_hash_on_delete(cursor);

	page_cur_delete_rec(page_cursor, index, offsets, mtr);

	page_cur_move_to_prev(page_cursor);

	trx = thr_get_trx(thr);

	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_index_entry_sys_field(new_entry, index, DATA_ROLL_PTR,
								roll_ptr);
		row_upd_index_entry_sys_field(new_entry, index, DATA_TRX_ID,
								trx->id);
	}

	rec = btr_cur_insert_if_possible(cursor, new_entry, &reorganized, mtr);

	ut_a(rec); /* <- We calculated above the insert would fit */

	if (!rec_get_deleted_flag(rec, page_is_comp(page))) {
		/* The new inserted record owns its possible externally
		stored fields */

		offsets = rec_get_offsets(rec, index, offsets,
						ULINT_UNDEFINED, &heap);
		btr_cur_unmark_extern_fields(rec, mtr, offsets);
	}

	/* Restore the old explicit lock state on the record */

	lock_rec_restore_from_page_infimum(rec, page);

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
	rec_t*	rec,	/* in: updated record */
	mtr_t*	mtr)	/* in: mtr */
{
	page_t*	page;
	page_t*	prev_page;
	ulint	space;
	ulint	prev_page_no;

	page = buf_frame_align(rec);

	if (page_rec_get_next(page_get_infimum_rec(page)) != rec) {
		/* Updated record is not the first user record on its page */

		return;
	}

	space = buf_frame_get_space_id(page);
	prev_page_no = btr_page_get_prev(page, mtr);

	ut_ad(prev_page_no != FIL_NULL);
	prev_page = buf_page_get_with_no_latch(space, prev_page_no, mtr);
#ifdef UNIV_BTR_DEBUG
	ut_a(btr_page_get_next(prev_page, mtr)
			== buf_frame_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

	/* We must already have an x-latch to prev_page! */
	ut_ad(mtr_memo_contains(mtr, buf_block_align(prev_page),
						MTR_MEMO_PAGE_X_FIX));

	lock_rec_reset_and_inherit_gap_locks(page_get_supremum_rec(prev_page),
									rec);
}

/*****************************************************************
Performs an update of a record on a page of a tree. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. If the
update is made on the leaf level, to avoid deadlocks, mtr must also
own x-latches to brothers of page, if those brothers exist. We assume
here that the ordering fields of the record do not change. */

ulint
btr_cur_pessimistic_update(
/*=======================*/
				/* out: DB_SUCCESS or error code */
	ulint		flags,	/* in: undo logging, locking, and rollback
				flags */
	btr_cur_t*	cursor,	/* in: cursor on the record to update */
	big_rec_t**	big_rec,/* out: big rec vector whose fields have to
				be stored externally by the caller, or NULL */
	upd_t*		update,	/* in: update vector; this is allowed also
				contain trx id and roll ptr fields, but
				the values in update vector have no effect */
	ulint		cmpl_info,/* in: compiler info on secondary index
				updates */
	que_thr_t*	thr,	/* in: query thread */
	mtr_t*		mtr)	/* in: mtr */
{
	big_rec_t*	big_rec_vec	= NULL;
	big_rec_t*	dummy_big_rec;
	dict_index_t*	index;
	page_t*		page;
	dict_tree_t*	tree;
	rec_t*		rec;
	page_cur_t*	page_cursor;
	dtuple_t*	new_entry;
	mem_heap_t*	heap;
	ulint		err;
	ulint		optim_err;
	ibool		dummy_reorganized;
	dulint		roll_ptr;
	trx_t*		trx;
	ibool		was_first;
	ibool		success;
	ulint		n_extents	= 0;
	ulint		n_reserved;
	ulint*		ext_vect;
	ulint		n_ext_vect;
	ulint		reserve_flag;
	ulint*		offsets		= NULL;

	*big_rec = NULL;

	page = btr_cur_get_page(cursor);
	rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	tree = index->tree;

	ut_ad(mtr_memo_contains(mtr, dict_tree_get_lock(tree),
							MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
							MTR_MEMO_PAGE_X_FIX));

	optim_err = btr_cur_optimistic_update(flags, cursor, update,
							cmpl_info, thr, mtr);

	if (optim_err != DB_UNDERFLOW && optim_err != DB_OVERFLOW) {

		return(optim_err);
	}

	/* Do lock checking and undo logging */
	err = btr_cur_upd_lock_and_undo(flags, cursor, update, cmpl_info,
							thr, &roll_ptr);
	if (err != DB_SUCCESS) {

		return(err);
	}

	if (optim_err == DB_OVERFLOW) {
		/* First reserve enough free space for the file segments
		of the index tree, so that the update will not fail because
		of lack of space */

		n_extents = cursor->tree_height / 16 + 3;

		if (flags & BTR_NO_UNDO_LOG_FLAG) {
			reserve_flag = FSP_CLEANING;
		} else {
			reserve_flag = FSP_NORMAL;
		}

		success = fsp_reserve_free_extents(&n_reserved,
						index->space,
						n_extents, reserve_flag, mtr);
		if (!success) {
			err = DB_OUT_OF_FILE_SPACE;

			return(err);
		}
	}

	heap = mem_heap_create(1024);
	offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED, &heap);

	trx = thr_get_trx(thr);

	new_entry = row_rec_to_index_entry(ROW_COPY_DATA, index, rec, heap);

	row_upd_index_replace_new_col_vals_index_pos(new_entry, index, update,
		FALSE, heap);
	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_index_entry_sys_field(new_entry, index, DATA_ROLL_PTR,
								roll_ptr);
		row_upd_index_entry_sys_field(new_entry, index, DATA_TRX_ID,
								trx->id);
	}

	if (flags & BTR_NO_UNDO_LOG_FLAG) {
		/* We are in a transaction rollback undoing a row
		update: we must free possible externally stored fields
		which got new values in the update, if they are not
		inherited values. They can be inherited if we have
		updated the primary key to another value, and then
		update it back again. */

		ut_a(big_rec_vec == NULL);

		btr_rec_free_updated_extern_fields(index, rec, offsets,
			update, TRUE, mtr);
	}

	/* We have to set appropriate extern storage bits in the new
	record to be inserted: we have to remember which fields were such */

	ext_vect = mem_heap_alloc(heap, sizeof(ulint)
					* dict_index_get_n_fields(index));
	ut_ad(!page_is_comp(page) || !rec_get_node_ptr_flag(rec));
	offsets = rec_get_offsets(rec, index, offsets,
					ULINT_UNDEFINED, &heap);
	n_ext_vect = btr_push_update_extern_fields(ext_vect, offsets, update);

	if (UNIV_UNLIKELY(rec_get_converted_size(index, new_entry) >=
		ut_min(page_get_free_space_of_empty(page_is_comp(page)) / 2,
		REC_MAX_DATA_SIZE))) {

		big_rec_vec = dtuple_convert_big_rec(index, new_entry,
							ext_vect, n_ext_vect);
		if (big_rec_vec == NULL) {

			err = DB_TOO_BIG_RECORD;
			goto return_after_reservations;
		}
	}

	page_cursor = btr_cur_get_page_cur(cursor);

	/* Store state of explicit locks on rec on the page infimum record,
	before deleting rec. The page infimum acts as a dummy carrier of the
	locks, taking care also of lock releases, before we can move the locks
	back on the actual record. There is a special case: if we are
	inserting on the root page and the insert causes a call of
	btr_root_raise_and_insert. Therefore we cannot in the lock system
	delete the lock structs set on the root page even if the root
	page carries just node pointers. */

	lock_rec_store_on_page_infimum(buf_frame_align(rec), rec);

	btr_search_update_hash_on_delete(cursor);

	page_cur_delete_rec(page_cursor, index, offsets, mtr);

	page_cur_move_to_prev(page_cursor);

	rec = btr_cur_insert_if_possible(cursor, new_entry,
						&dummy_reorganized, mtr);
	ut_a(rec || optim_err != DB_UNDERFLOW);

	if (rec) {
		lock_rec_restore_from_page_infimum(rec, page);
		rec_set_field_extern_bits(rec, index,
						ext_vect, n_ext_vect, mtr);

		offsets = rec_get_offsets(rec, index, offsets,
					ULINT_UNDEFINED, &heap);

		if (!rec_get_deleted_flag(rec, rec_offs_comp(offsets))) {
			/* The new inserted record owns its possible externally
			stored fields */
			btr_cur_unmark_extern_fields(rec, mtr, offsets);
		}

		btr_cur_compress_if_useful(cursor, mtr);

		err = DB_SUCCESS;
		goto return_after_reservations;
	}

	if (page_cur_is_before_first(page_cursor)) {
		/* The record to be updated was positioned as the first user
		record on its page */

		was_first = TRUE;
	} else {
		was_first = FALSE;
	}

	/* The first parameter means that no lock checking and undo logging
	is made in the insert */

	err = btr_cur_pessimistic_insert(BTR_NO_UNDO_LOG_FLAG
					| BTR_NO_LOCKING_FLAG
					| BTR_KEEP_SYS_FLAG,
					cursor, new_entry, &rec,
					&dummy_big_rec, NULL, mtr);
	ut_a(rec);
	ut_a(err == DB_SUCCESS);
	ut_a(dummy_big_rec == NULL);

	rec_set_field_extern_bits(rec, index, ext_vect, n_ext_vect, mtr);
	offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);

	if (!rec_get_deleted_flag(rec, rec_offs_comp(offsets))) {
		/* The new inserted record owns its possible externally
		stored fields */

		btr_cur_unmark_extern_fields(rec, mtr, offsets);
	}

	lock_rec_restore_from_page_infimum(rec, page);

	/* If necessary, restore also the correct lock state for a new,
	preceding supremum record created in a page split. While the old
	record was nonexistent, the supremum might have inherited its locks
	from a wrong record. */

	if (!was_first) {
		btr_cur_pess_upd_restore_supremum(rec, mtr);
	}

return_after_reservations:
	mem_heap_free(heap);

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
			1 + 1 + DATA_ROLL_PTR_LEN + 14 + 2);

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
	mach_write_to_2(log_ptr, ut_align_offset(rec, UNIV_PAGE_SIZE));
	log_ptr += 2;

	mlog_close(mtr, log_ptr);
}

/********************************************************************
Parses the redo log record for delete marking or unmarking of a clustered
index record. */

byte*
btr_cur_parse_del_mark_set_clust_rec(
/*=================================*/
				/* out: end of log record or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	dict_index_t*	index,	/* in: index corresponding to page */
	page_t*		page)	/* in: page or NULL */
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

		if (!(flags & BTR_KEEP_SYS_FLAG)) {
			mem_heap_t*	heap		= NULL;
			ulint		offsets_[REC_OFFS_NORMAL_SIZE];
			*offsets_ = (sizeof offsets_) / sizeof *offsets_;

			row_upd_rec_sys_fields_in_recovery(rec,
					rec_get_offsets(rec, index, offsets_,
					ULINT_UNDEFINED, &heap),
					pos, trx_id, roll_ptr);
			if (UNIV_LIKELY_NULL(heap)) {
				mem_heap_free(heap);
			}
		}

		/* We do not need to reserve btr_search_latch, as the page
		is only being recovered, and there cannot be a hash index to
		it. */

		rec_set_deleted_flag(rec, page_is_comp(page), val);
	}

	return(ptr);
}

/***************************************************************
Marks a clustered index record deleted. Writes an undo log record to
undo log on this delete marking. Writes in the trx id field the id
of the deleting transaction, and in the roll ptr field pointer to the
undo log record created. */

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
	trx_t*		trx;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

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

	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(!rec_get_deleted_flag(rec, rec_offs_comp(offsets)));

	err = lock_clust_rec_modify_check_and_lock(flags,
						rec, index, offsets, thr);

	if (err != DB_SUCCESS) {

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
		return(err);
	}

	err = trx_undo_report_row_operation(flags, TRX_UNDO_MODIFY_OP, thr,
						index, NULL, NULL, 0, rec,
						&roll_ptr);
	if (err != DB_SUCCESS) {

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
		return(err);
	}

	block = buf_block_align(rec);

	if (block->is_hashed) {
		rw_lock_x_lock(&btr_search_latch);
	}

	rec_set_deleted_flag(rec, rec_offs_comp(offsets), val);

	trx = thr_get_trx(thr);

	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_rec_sys_fields(rec, index, offsets, trx, roll_ptr);
	}

	if (block->is_hashed) {
		rw_lock_x_unlock(&btr_search_latch);
	}

	btr_cur_del_mark_set_clust_rec_log(flags, rec, index, val, trx,
							roll_ptr, mtr);
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(DB_SUCCESS);
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

	mach_write_to_2(log_ptr, ut_align_offset(rec, UNIV_PAGE_SIZE));
	log_ptr += 2;

	mlog_close(mtr, log_ptr);
}

/********************************************************************
Parses the redo log record for delete marking or unmarking of a secondary
index record. */

byte*
btr_cur_parse_del_mark_set_sec_rec(
/*===============================*/
				/* out: end of log record or NULL */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	page_t*		page)	/* in: page or NULL */
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

		rec_set_deleted_flag(rec, page_is_comp(page), val);
	}

	return(ptr);
}

/***************************************************************
Sets a secondary index record delete mark to TRUE or FALSE. */

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

	rec = btr_cur_get_rec(cursor);

#ifdef UNIV_DEBUG
	if (btr_cur_print_record_ops && thr) {
		btr_cur_trx_report(thr_get_trx(thr), cursor->index,
				"del mark ");
		rec_print(stderr, rec, cursor->index);
	}
#endif /* UNIV_DEBUG */

	err = lock_sec_rec_modify_check_and_lock(flags, rec, cursor->index,
									thr);
	if (err != DB_SUCCESS) {

		return(err);
	}

	block = buf_block_align(rec);
	ut_ad(!!page_is_comp(buf_block_get_frame(block))
		== dict_table_is_comp(cursor->index->table));

	if (block->is_hashed) {
		rw_lock_x_lock(&btr_search_latch);
	}

	rec_set_deleted_flag(rec, page_is_comp(buf_block_get_frame(block)),
									val);

	if (block->is_hashed) {
		rw_lock_x_unlock(&btr_search_latch);
	}

	btr_cur_del_mark_set_sec_rec_log(rec, val, mtr);

	return(DB_SUCCESS);
}

/***************************************************************
Sets a secondary index record delete mark to FALSE. This function is only
used by the insert buffer insert merge mechanism. */

void
btr_cur_del_unmark_for_ibuf(
/*========================*/
	rec_t*		rec,	/* in: record to delete unmark */
	mtr_t*		mtr)	/* in: mtr */
{
	/* We do not need to reserve btr_search_latch, as the page has just
	been read to the buffer pool and there cannot be a hash index to it. */

	rec_set_deleted_flag(rec, page_is_comp(buf_frame_align(rec)), FALSE);

	btr_cur_del_mark_set_sec_rec_log(rec, FALSE, mtr);
}

/*==================== B-TREE RECORD REMOVE =========================*/

/*****************************************************************
Tries to compress a page of the tree on the leaf level. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. To avoid
deadlocks, mtr must also own x-latches to brothers of page, if those
brothers exist. NOTE: it is assumed that the caller has reserved enough
free extents so that the compression will always succeed if done! */

void
btr_cur_compress(
/*=============*/
	btr_cur_t*	cursor,	/* in: cursor on the page to compress;
				cursor does not stay valid */
	mtr_t*		mtr)	/* in: mtr */
{
	ut_ad(mtr_memo_contains(mtr,
				dict_tree_get_lock(btr_cur_get_tree(cursor)),
							MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(btr_cur_get_rec(cursor)),
				MTR_MEMO_PAGE_X_FIX));
	ut_ad(btr_page_get_level(btr_cur_get_page(cursor), mtr) == 0);

	btr_compress(cursor, mtr);
}

/*****************************************************************
Tries to compress a page of the tree if it seems useful. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. To avoid
deadlocks, mtr must also own x-latches to brothers of page, if those
brothers exist. NOTE: it is assumed that the caller has reserved enough
free extents so that the compression will always succeed if done! */

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
				dict_tree_get_lock(btr_cur_get_tree(cursor)),
							MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(btr_cur_get_rec(cursor)),
				MTR_MEMO_PAGE_X_FIX));

	if (btr_cur_compress_recommendation(cursor, mtr)) {

		btr_compress(cursor, mtr);

		return(TRUE);
	}

	return(FALSE);
}

/***********************************************************
Removes the record on which the tree cursor is positioned on a leaf page.
It is assumed that the mtr has an x-latch on the page where the cursor is
positioned, but no latch on the whole tree. */

ibool
btr_cur_optimistic_delete(
/*======================*/
				/* out: TRUE if success, i.e., the page
				did not become too empty */
	btr_cur_t*	cursor,	/* in: cursor on leaf page, on the record to
				delete; cursor stays valid: if deletion
				succeeds, on function exit it points to the
				successor of the deleted record */
	mtr_t*		mtr)	/* in: mtr */
{
	page_t*		page;
	ulint		max_ins_size;
	rec_t*		rec;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	ibool		no_compress_needed;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_ad(mtr_memo_contains(mtr, buf_block_align(btr_cur_get_rec(cursor)),
							MTR_MEMO_PAGE_X_FIX));
	/* This is intended only for leaf page deletions */

	page = btr_cur_get_page(cursor);

	ut_ad(btr_page_get_level(page, mtr) == 0);

	rec = btr_cur_get_rec(cursor);
	offsets = rec_get_offsets(rec, cursor->index, offsets,
					ULINT_UNDEFINED, &heap);

	no_compress_needed = !rec_offs_any_extern(offsets)
			&& btr_cur_can_delete_without_compress(
			cursor, rec_offs_size(offsets), mtr);

	if (no_compress_needed) {

		lock_update_delete(rec);

		btr_search_update_hash_on_delete(cursor);

		max_ins_size = page_get_max_insert_size_after_reorganize(page,
									1);
		page_cur_delete_rec(btr_cur_get_page_cur(cursor),
						cursor->index, offsets, mtr);

		ibuf_update_free_bits_low(cursor->index, page, max_ins_size,
									mtr);
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
	ibool		in_rollback,/* in: TRUE if called in rollback */
	mtr_t*		mtr)	/* in: mtr */
{
	page_t*		page;
	dict_tree_t*	tree;
	rec_t*		rec;
	dtuple_t*	node_ptr;
	ulint		n_extents	= 0;
	ulint		n_reserved;
	ibool		success;
	ibool		ret		= FALSE;
	ulint		level;
	mem_heap_t*	heap;
	ulint*		offsets;

	page = btr_cur_get_page(cursor);
	tree = btr_cur_get_tree(cursor);

	ut_ad(mtr_memo_contains(mtr, dict_tree_get_lock(tree),
							MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
							MTR_MEMO_PAGE_X_FIX));
	if (!has_reserved_extents) {
		/* First reserve enough free space for the file segments
		of the index tree, so that the node pointer updates will
		not fail because of lack of space */

		n_extents = cursor->tree_height / 32 + 1;

		success = fsp_reserve_free_extents(&n_reserved,
						cursor->index->space,
						n_extents, FSP_CLEANING, mtr);
		if (!success) {
			*err = DB_OUT_OF_FILE_SPACE;

			return(FALSE);
		}
	}

	heap = mem_heap_create(1024);
	rec = btr_cur_get_rec(cursor);

	offsets = rec_get_offsets(rec, cursor->index,
					NULL, ULINT_UNDEFINED, &heap);

	/* Free externally stored fields if the record is neither
	a node pointer nor in two-byte format.
	This avoids an unnecessary loop. */
	if (page_is_comp(page)
			? !rec_get_node_ptr_flag(rec)
			: !rec_get_1byte_offs_flag(rec)) {
		btr_rec_free_externally_stored_fields(cursor->index,
					rec, offsets, in_rollback, mtr);
	}

	if (UNIV_UNLIKELY(page_get_n_recs(page) < 2)
		&& UNIV_UNLIKELY(dict_tree_get_page(btr_cur_get_tree(cursor))
			!= buf_frame_get_page_no(page))) {

		/* If there is only one record, drop the whole page in
		btr_discard_page, if this is not the root page */

		btr_discard_page(cursor, mtr);

		*err = DB_SUCCESS;
		ret = TRUE;

		goto return_after_reservations;
	}

	lock_update_delete(rec);
	level = btr_page_get_level(page, mtr);

	if (level > 0
		&& UNIV_UNLIKELY(rec == page_rec_get_next(
					 page_get_infimum_rec(page)))) {

		rec_t*	next_rec = page_rec_get_next(rec);

		if (btr_page_get_prev(page, mtr) == FIL_NULL) {

			/* If we delete the leftmost node pointer on a
			non-leaf level, we must mark the new leftmost node
			pointer as the predefined minimum record */

			btr_set_min_rec_mark(next_rec, page_is_comp(page),
						mtr);
		} else {
			/* Otherwise, if we delete the leftmost node pointer
			on a page, we have to change the father node pointer
			so that it is equal to the new leftmost node pointer
			on the page */

			btr_node_ptr_delete(tree, page, mtr);

			node_ptr = dict_tree_build_node_ptr(
					tree, next_rec,
					buf_frame_get_page_no(page),
						heap, level);

			btr_insert_on_non_leaf_level(tree,
					level + 1, node_ptr, mtr);
		}
	}

	btr_search_update_hash_on_delete(cursor);

	page_cur_delete_rec(btr_cur_get_page_cur(cursor), cursor->index,
							offsets, mtr);

	ut_ad(btr_check_node_ptr(tree, page, mtr));

	*err = DB_SUCCESS;

return_after_reservations:
	mem_heap_free(heap);

	if (ret == FALSE) {
		ret = btr_cur_compress_if_useful(cursor, mtr);
	}

	if (n_extents > 0) {
		fil_space_release_free_extents(cursor->index->space,
								n_reserved);
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
	slot->n_recs = page_get_n_recs(buf_frame_align(rec));
}

/***********************************************************************
Estimates the number of rows in a given index range. */

ib_longlong
btr_estimate_n_rows_in_range(
/*=========================*/
				/* out: estimated number of rows */
	dict_index_t*	index,	/* in: index */
	dtuple_t*	tuple1,	/* in: range start, may also be empty tuple */
	ulint		mode1,	/* in: search mode for range start */
	dtuple_t*	tuple2,	/* in: range end, may also be empty tuple */
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
	ib_longlong	n_rows;
	ulint		i;
	mtr_t		mtr;

	mtr_start(&mtr);

	cursor.path_arr = path1;

	if (dtuple_get_n_fields(tuple1) > 0) {

		btr_cur_search_to_nth_level(index, 0, tuple1, mode1,
					BTR_SEARCH_LEAF	| BTR_ESTIMATE,
					&cursor, 0, &mtr);
	} else {
		btr_cur_open_at_index_side(TRUE, index,
					BTR_SEARCH_LEAF	| BTR_ESTIMATE,
					&cursor, &mtr);
	}

	mtr_commit(&mtr);

	mtr_start(&mtr);

	cursor.path_arr = path2;

	if (dtuple_get_n_fields(tuple2) > 0) {

		btr_cur_search_to_nth_level(index, 0, tuple2, mode2,
					BTR_SEARCH_LEAF	| BTR_ESTIMATE,
					&cursor, 0, &mtr);
	} else {
		btr_cur_open_at_index_side(FALSE, index,
					BTR_SEARCH_LEAF	| BTR_ESTIMATE,
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
	ib_longlong*	n_diff;
	ulint		not_empty_flag	= 0;
	ulint		total_external_size = 0;
	ulint		i;
	ulint		j;
	ulint		add_on;
	mtr_t		mtr;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_rec_[REC_OFFS_NORMAL_SIZE];
	ulint		offsets_next_rec_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets_rec	= offsets_rec_;
	ulint*		offsets_next_rec= offsets_next_rec_;
	*offsets_rec_ = (sizeof offsets_rec_) / sizeof *offsets_rec_;
	*offsets_next_rec_ =
			(sizeof offsets_next_rec_) / sizeof *offsets_next_rec_;

	n_cols = dict_index_get_n_unique(index);

	n_diff = mem_alloc((n_cols + 1) * sizeof(ib_longlong));

	memset(n_diff, 0, (n_cols + 1) * sizeof(ib_longlong));

	/* We sample some pages in the index to get an estimate */

	for (i = 0; i < BTR_KEY_VAL_ESTIMATE_N_PAGES; i++) {
		rec_t*	supremum;
		mtr_start(&mtr);

		btr_cur_open_at_rnd_pos(index, BTR_SEARCH_LEAF, &cursor, &mtr);

		/* Count the number of different key values for each prefix of
		the key on this index page. If the prefix does not determine
		the index record uniquely in te B-tree, then we subtract one
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
			rec_t*	next_rec = page_rec_get_next(rec);
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
						&matched_bytes);

			for (j = matched_fields + 1; j <= n_cols; j++) {
				/* We add one if this index record has
				a different prefix from the previous */

				n_diff[j]++;
			}

			total_external_size +=
				btr_rec_get_externally_stored_len(
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
		total_external_size +=
				btr_rec_get_externally_stored_len(rec,
								offsets_rec);
		mtr_commit(&mtr);
	}

	/* If we saw k borders between different key values on
	BTR_KEY_VAL_ESTIMATE_N_PAGES leaf pages, we can estimate how many
	there will be in index->stat_n_leaf_pages */

	/* We must take into account that our sample actually represents
	also the pages used for external storage of fields (those pages are
	included in index->stat_n_leaf_pages) */

	for (j = 0; j <= n_cols; j++) {
		index->stat_n_diff_key_vals[j] =
				(n_diff[j]
				 * (ib_longlong)index->stat_n_leaf_pages
				 + BTR_KEY_VAL_ESTIMATE_N_PAGES - 1
				 + total_external_size
				 + not_empty_flag)
					/ (BTR_KEY_VAL_ESTIMATE_N_PAGES
					   + total_external_size);

		/* If the tree is small, smaller than <
		10 * BTR_KEY_VAL_ESTIMATE_N_PAGES + total_external_size, then
		the above estimate is ok. For bigger trees it is common that we
		do not see any borders between key values in the few pages
		we pick. But still there may be BTR_KEY_VAL_ESTIMATE_N_PAGES
		different key values, or even more. Let us try to approximate
		that: */

		add_on = index->stat_n_leaf_pages /
		   (10 * (BTR_KEY_VAL_ESTIMATE_N_PAGES + total_external_size));

		if (add_on > BTR_KEY_VAL_ESTIMATE_N_PAGES) {
			add_on = BTR_KEY_VAL_ESTIMATE_N_PAGES;
		}

		index->stat_n_diff_key_vals[j] += add_on;
	}

	mem_free(n_diff);
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
	rec_t*		rec,	/* in: clustered index record */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		i,	/* in: field number */
	ibool		val,	/* in: value to set */
	mtr_t*		mtr)	/* in: mtr */
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

	mlog_write_ulint(data + local_len + BTR_EXTERN_LEN, byte_val,
							MLOG_1BYTE, mtr);
}

/***********************************************************************
Marks not updated extern fields as not-owned by this record. The ownership
is transferred to the updated record which is inserted elsewhere in the
index tree. In purge only the owner of externally stored field is allowed
to free the field. */

void
btr_cur_mark_extern_inherited_fields(
/*=================================*/
	rec_t*		rec,	/* in: record in a clustered index */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	upd_t*		update,	/* in: update vector */
	mtr_t*		mtr)	/* in: mtr */
{
	ibool	is_updated;
	ulint	n;
	ulint	j;
	ulint	i;

	ut_ad(rec_offs_validate(rec, NULL, offsets));
	ut_ad(!rec_offs_comp(offsets) || !rec_get_node_ptr_flag(rec));
	n = rec_offs_n_fields(offsets);

	for (i = 0; i < n; i++) {
		if (rec_offs_nth_extern(offsets, i)) {

			/* Check it is not in updated fields */
			is_updated = FALSE;

			if (update) {
				for (j = 0; j < upd_get_n_fields(update);
								j++) {
					if (upd_get_nth_field(update, j)
							->field_no == i) {
						is_updated = TRUE;
					}
				}
			}

			if (!is_updated) {
				btr_cur_set_ownership_of_extern_field(rec,
							offsets, i, FALSE, mtr);
			}
		}
	}
}

/***********************************************************************
The complement of the previous function: in an update entry may inherit
some externally stored fields from a record. We must mark them as inherited
in entry, so that they are not freed in a rollback. */

void
btr_cur_mark_dtuple_inherited_extern(
/*=================================*/
	dtuple_t*	entry,		/* in: updated entry to be inserted to
					clustered index */
	ulint*		ext_vec,	/* in: array of extern fields in the
					original record */
	ulint		n_ext_vec,	/* in: number of elements in ext_vec */
	upd_t*		update)		/* in: update vector */
{
	dfield_t* dfield;
	ulint	byte_val;
	byte*	data;
	ulint	len;
	ibool	is_updated;
	ulint	j;
	ulint	i;

	if (ext_vec == NULL) {

		return;
	}

	for (i = 0; i < n_ext_vec; i++) {

		/* Check ext_vec[i] is in updated fields */
		is_updated = FALSE;

		for (j = 0; j < upd_get_n_fields(update); j++) {
			if (upd_get_nth_field(update, j)->field_no
							== ext_vec[i]) {
				is_updated = TRUE;
			}
		}

		if (!is_updated) {
			dfield = dtuple_get_nth_field(entry, ext_vec[i]);

			data = (byte*) dfield_get_data(dfield);
			len = dfield_get_len(dfield);

			len -= BTR_EXTERN_FIELD_REF_SIZE;

			byte_val = mach_read_from_1(data + len
							+ BTR_EXTERN_LEN);

			byte_val = byte_val | BTR_EXTERN_INHERITED_FLAG;

			mach_write_to_1(data + len + BTR_EXTERN_LEN, byte_val);
		}
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
	rec_t*		rec,	/* in: record in a clustered index */
	mtr_t*		mtr,	/* in: mtr */
	const ulint*	offsets)/* in: array returned by rec_get_offsets() */
{
	ulint	n;
	ulint	i;

	ut_ad(!rec_offs_comp(offsets) || !rec_get_node_ptr_flag(rec));
	n = rec_offs_n_fields(offsets);

	for (i = 0; i < n; i++) {
		if (rec_offs_nth_extern(offsets, i)) {

			btr_cur_set_ownership_of_extern_field(rec, offsets, i,
								TRUE, mtr);
		}
	}
}

/***********************************************************************
Marks all extern fields in a dtuple as owned by the record. */

void
btr_cur_unmark_dtuple_extern_fields(
/*================================*/
	dtuple_t*	entry,		/* in: clustered index entry */
	ulint*		ext_vec,	/* in: array of numbers of fields
					which have been stored externally */
	ulint		n_ext_vec)	/* in: number of elements in ext_vec */
{
	dfield_t* dfield;
	ulint	byte_val;
	byte*	data;
	ulint	len;
	ulint	i;

	for (i = 0; i < n_ext_vec; i++) {
		dfield = dtuple_get_nth_field(entry, ext_vec[i]);

		data = (byte*) dfield_get_data(dfield);
		len = dfield_get_len(dfield);

		len -= BTR_EXTERN_FIELD_REF_SIZE;

		byte_val = mach_read_from_1(data + len + BTR_EXTERN_LEN);

		byte_val = byte_val & (~BTR_EXTERN_OWNER_FLAG);

		mach_write_to_1(data + len + BTR_EXTERN_LEN, byte_val);
	}
}

/***********************************************************************
Stores the positions of the fields marked as extern storage in the update
vector, and also those fields who are marked as extern storage in rec
and not mentioned in updated fields. We use this function to remember
which fields we must mark as extern storage in a record inserted for an
update. */

ulint
btr_push_update_extern_fields(
/*==========================*/
				/* out: number of values stored in ext_vect */
	ulint*		ext_vect,/* in: array of ulints, must be preallocated
				to have space for all fields in rec */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	upd_t*		update)	/* in: update vector or NULL */
{
	ulint	n_pushed	= 0;
	ibool	is_updated;
	ulint	n;
	ulint	j;
	ulint	i;

	if (update) {
		n = upd_get_n_fields(update);

		for (i = 0; i < n; i++) {

			if (upd_get_nth_field(update, i)->extern_storage) {

				ext_vect[n_pushed] =
					upd_get_nth_field(update, i)->field_no;

				n_pushed++;
			}
		}
	}

	n = rec_offs_n_fields(offsets);

	for (i = 0; i < n; i++) {
		if (rec_offs_nth_extern(offsets, i)) {

			/* Check it is not in updated fields */
			is_updated = FALSE;

			if (update) {
				for (j = 0; j < upd_get_n_fields(update);
								j++) {
					if (upd_get_nth_field(update, j)
							->field_no == i) {
						is_updated = TRUE;
					}
				}
			}

			if (!is_updated) {
				ext_vect[n_pushed] = i;
				n_pushed++;
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
	byte*	blob_header)	/* in: blob header */
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
	byte*	blob_header)	/* in: blob header */
{
	return(mach_read_from_4(blob_header + BTR_BLOB_HDR_NEXT_PAGE_NO));
}

/***********************************************************************
Stores the fields in big_rec_vec to the tablespace and puts pointers to
them in rec. The fields are stored on pages allocated from leaf node
file segment of the index tree. */

ulint
btr_store_big_rec_extern_fields(
/*============================*/
					/* out: DB_SUCCESS or error */
	dict_index_t*	index,		/* in: index of rec; the index tree
					MUST be X-latched */
	rec_t*		rec,		/* in: record */
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
	byte*	data;
	ulint	local_len;
	ulint	extern_len;
	ulint	store_len;
	ulint	page_no;
	page_t*	page;
	ulint	space_id;
	page_t*	prev_page;
	page_t*	rec_page;
	ulint	prev_page_no;
	ulint	hint_page_no;
	ulint	i;
	mtr_t	mtr;

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(mtr_memo_contains(local_mtr, dict_tree_get_lock(index->tree),
							MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(local_mtr, buf_block_align(rec),
							MTR_MEMO_PAGE_X_FIX));
	ut_a(index->type & DICT_CLUSTERED);

	space_id = buf_frame_get_space_id(rec);

	/* We have to create a file segment to the tablespace
	for each field and put the pointer to the field in rec */

	for (i = 0; i < big_rec_vec->n_fields; i++) {

		data = rec_get_nth_field(rec, offsets,
				big_rec_vec->fields[i].field_no, &local_len);
		ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);
		local_len -= BTR_EXTERN_FIELD_REF_SIZE;
		extern_len = big_rec_vec->fields[i].len;

		ut_a(extern_len > 0);

		prev_page_no = FIL_NULL;

		while (extern_len > 0) {
			mtr_start(&mtr);

			if (prev_page_no == FIL_NULL) {
				hint_page_no = buf_frame_get_page_no(rec) + 1;
			} else {
				hint_page_no = prev_page_no + 1;
			}

			page = btr_page_alloc(index->tree, hint_page_no,
						FSP_NO_DIR, 0, &mtr);
			if (page == NULL) {

				mtr_commit(&mtr);

				return(DB_OUT_OF_FILE_SPACE);
			}

			mlog_write_ulint(page + FIL_PAGE_TYPE,
					FIL_PAGE_TYPE_BLOB, MLOG_2BYTES, &mtr);

			page_no = buf_frame_get_page_no(page);

			if (prev_page_no != FIL_NULL) {
				prev_page = buf_page_get(space_id,
						prev_page_no,
						RW_X_LATCH, &mtr);

#ifdef UNIV_SYNC_DEBUG
				buf_page_dbg_add_level(prev_page,
							SYNC_EXTERN_STORAGE);
#endif /* UNIV_SYNC_DEBUG */

				mlog_write_ulint(prev_page + FIL_PAGE_DATA
						+ BTR_BLOB_HDR_NEXT_PAGE_NO,
						page_no, MLOG_4BYTES, &mtr);
			}

			if (extern_len > (UNIV_PAGE_SIZE - FIL_PAGE_DATA
						- BTR_BLOB_HDR_SIZE
						- FIL_PAGE_DATA_END)) {
				store_len = UNIV_PAGE_SIZE - FIL_PAGE_DATA
						- BTR_BLOB_HDR_SIZE
						- FIL_PAGE_DATA_END;
			} else {
				store_len = extern_len;
			}

			mlog_write_string(page + FIL_PAGE_DATA
						+ BTR_BLOB_HDR_SIZE,
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

			rec_page = buf_page_get(space_id,
						buf_frame_get_page_no(data),
							RW_X_LATCH, &mtr);
#ifdef UNIV_SYNC_DEBUG
			buf_page_dbg_add_level(rec_page, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */
			mlog_write_ulint(data + local_len + BTR_EXTERN_LEN, 0,
						MLOG_4BYTES, &mtr);
			mlog_write_ulint(data + local_len + BTR_EXTERN_LEN + 4,
					big_rec_vec->fields[i].len
								- extern_len,
					MLOG_4BYTES, &mtr);

			if (prev_page_no == FIL_NULL) {
				mlog_write_ulint(data + local_len
							+ BTR_EXTERN_SPACE_ID,
						space_id,
						MLOG_4BYTES, &mtr);

				mlog_write_ulint(data + local_len
							+ BTR_EXTERN_PAGE_NO,
						page_no,
						MLOG_4BYTES, &mtr);

				mlog_write_ulint(data + local_len
							+ BTR_EXTERN_OFFSET,
						FIL_PAGE_DATA,
						MLOG_4BYTES, &mtr);

				/* Set the bit denoting that this field
				in rec is stored externally */

				rec_set_nth_field_extern_bit(rec, index,
					big_rec_vec->fields[i].field_no,
					TRUE, &mtr);
			}

			prev_page_no = page_no;

			mtr_commit(&mtr);
		}
	}

	return(DB_SUCCESS);
}

/***********************************************************************
Frees the space in an externally stored field to the file space
management if the field in data is owned the externally stored field,
in a rollback we may have the additional condition that the field must
not be inherited. */

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
	byte*		data,		/* in: internally stored data
					+ reference to the externally
					stored part */
	ulint		local_len,	/* in: length of data */
	ibool		do_not_free_inherited,/* in: TRUE if called in a
					rollback and we do not want to free
					inherited fields */
	mtr_t*		local_mtr __attribute__((unused))) /* in: mtr
					containing the latch to data an an
					X-latch to the index tree */
{
	page_t*	page;
	page_t*	rec_page;
	ulint	space_id;
	ulint	page_no;
	ulint	offset;
	ulint	extern_len;
	ulint	next_page_no;
	ulint	part_len;
	mtr_t	mtr;

	ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);
	ut_ad(mtr_memo_contains(local_mtr, dict_tree_get_lock(index->tree),
							MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(local_mtr, buf_block_align(data),
							MTR_MEMO_PAGE_X_FIX));
	ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);
	local_len -= BTR_EXTERN_FIELD_REF_SIZE;

	for (;;) {
		mtr_start(&mtr);

		rec_page = buf_page_get(buf_frame_get_space_id(data),
				buf_frame_get_page_no(data), RW_X_LATCH, &mtr);
#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(rec_page, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */
		space_id = mach_read_from_4(data + local_len
						+ BTR_EXTERN_SPACE_ID);

		page_no = mach_read_from_4(data + local_len
						+ BTR_EXTERN_PAGE_NO);

		offset = mach_read_from_4(data + local_len
							+ BTR_EXTERN_OFFSET);
		extern_len = mach_read_from_4(data + local_len
						+ BTR_EXTERN_LEN + 4);

		/* If extern len is 0, then there is no external storage data
		at all */

		if (extern_len == 0) {

			mtr_commit(&mtr);

			return;
		}

		if (mach_read_from_1(data + local_len + BTR_EXTERN_LEN)
						& BTR_EXTERN_OWNER_FLAG) {
			/* This field does not own the externally
			stored field: do not free! */

			mtr_commit(&mtr);

			return;
		}

		if (do_not_free_inherited
			&& mach_read_from_1(data + local_len + BTR_EXTERN_LEN)
						& BTR_EXTERN_INHERITED_FLAG) {
			/* Rollback and inherited field: do not free! */

			mtr_commit(&mtr);

			return;
		}

		page = buf_page_get(space_id, page_no, RW_X_LATCH, &mtr);
#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(page, SYNC_EXTERN_STORAGE);
#endif /* UNIV_SYNC_DEBUG */
		next_page_no = mach_read_from_4(page + FIL_PAGE_DATA
						+ BTR_BLOB_HDR_NEXT_PAGE_NO);

		part_len = btr_blob_get_part_len(page + FIL_PAGE_DATA);

		ut_a(extern_len >= part_len);

		/* We must supply the page level (= 0) as an argument
		because we did not store it on the page (we save the space
		overhead from an index page header. */

		btr_page_free_low(index->tree, page, 0, &mtr);

		mlog_write_ulint(data + local_len + BTR_EXTERN_PAGE_NO,
						next_page_no,
						MLOG_4BYTES, &mtr);
		mlog_write_ulint(data + local_len + BTR_EXTERN_LEN + 4,
						extern_len - part_len,
						MLOG_4BYTES, &mtr);
		if (next_page_no == FIL_NULL) {
			ut_a(extern_len - part_len == 0);
		}

		if (extern_len - part_len == 0) {
			ut_a(next_page_no == FIL_NULL);
		}

		mtr_commit(&mtr);
	}
}

/***************************************************************
Frees the externally stored fields for a record. */

void
btr_rec_free_externally_stored_fields(
/*==================================*/
	dict_index_t*	index,	/* in: index of the data, the index
				tree MUST be X-latched */
	rec_t*		rec,	/* in: record */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	ibool		do_not_free_inherited,/* in: TRUE if called in a
				rollback and we do not want to free
				inherited fields */
	mtr_t*		mtr)	/* in: mini-transaction handle which contains
				an X-latch to record page and to the index
				tree */
{
	ulint	n_fields;
	byte*	data;
	ulint	len;
	ulint	i;

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(rec),
							MTR_MEMO_PAGE_X_FIX));
	/* Free possible externally stored fields in the record */

	ut_ad(dict_table_is_comp(index->table) == !!rec_offs_comp(offsets));
	n_fields = rec_offs_n_fields(offsets);

	for (i = 0; i < n_fields; i++) {
		if (rec_offs_nth_extern(offsets, i)) {

			data = rec_get_nth_field(rec, offsets, i, &len);
			btr_free_externally_stored_field(index, data, len,
						do_not_free_inherited, mtr);
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
	rec_t*		rec,	/* in: record */
	const ulint*	offsets,/* in: rec_get_offsets(rec, index) */
	upd_t*		update,	/* in: update vector */
	ibool		do_not_free_inherited,/* in: TRUE if called in a
				rollback and we do not want to free
				inherited fields */
	mtr_t*		mtr)	/* in: mini-transaction handle which contains
				an X-latch to record page and to the tree */
{
	upd_field_t*	ufield;
	ulint		n_fields;
	byte*		data;
	ulint		len;
	ulint		i;

	ut_ad(rec_offs_validate(rec, index, offsets));
	ut_ad(mtr_memo_contains(mtr, buf_block_align(rec),
							MTR_MEMO_PAGE_X_FIX));

	/* Free possible externally stored fields in the record */

	n_fields = upd_get_n_fields(update);

	for (i = 0; i < n_fields; i++) {
		ufield = upd_get_nth_field(update, i);

		if (rec_offs_nth_extern(offsets, ufield->field_no)) {

			data = rec_get_nth_field(rec, offsets,
						ufield->field_no, &len);
			btr_free_externally_stored_field(index, data, len,
						do_not_free_inherited, mtr);
		}
	}
}

/***********************************************************************
Copies an externally stored field of a record to mem heap. Parameter
data contains a pointer to 'internally' stored part of the field:
possibly some data, and the reference to the externally stored part in
the last 20 bytes of data. */

byte*
btr_copy_externally_stored_field(
/*=============================*/
				/* out: the whole field copied to heap */
	ulint*		len,	/* out: length of the whole field */
	byte*		data,	/* in: 'internally' stored part of the
				field containing also the reference to
				the external part */
	ulint		local_len,/* in: length of data */
	mem_heap_t*	heap)	/* in: mem heap */
{
	page_t*	page;
	ulint	space_id;
	ulint	page_no;
	ulint	offset;
	ulint	extern_len;
	byte*	blob_header;
	ulint	part_len;
	byte*	buf;
	ulint	copied_len;
	mtr_t	mtr;

	ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

	local_len -= BTR_EXTERN_FIELD_REF_SIZE;

	space_id = mach_read_from_4(data + local_len + BTR_EXTERN_SPACE_ID);

	page_no = mach_read_from_4(data + local_len + BTR_EXTERN_PAGE_NO);

	offset = mach_read_from_4(data + local_len + BTR_EXTERN_OFFSET);

	/* Currently a BLOB cannot be bigger that 4 GB; we
	leave the 4 upper bytes in the length field unused */

	extern_len = mach_read_from_4(data + local_len + BTR_EXTERN_LEN + 4);

	buf = mem_heap_alloc(heap, local_len + extern_len);

	ut_memcpy(buf, data, local_len);
	copied_len = local_len;

	if (extern_len == 0) {
		*len = copied_len;

		return(buf);
	}

	for (;;) {
		mtr_start(&mtr);

		page = buf_page_get(space_id, page_no, RW_S_LATCH, &mtr);
#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(page, SYNC_EXTERN_STORAGE);
#endif /* UNIV_SYNC_DEBUG */
		blob_header = page + offset;

		part_len = btr_blob_get_part_len(blob_header);

		ut_memcpy(buf + copied_len, blob_header + BTR_BLOB_HDR_SIZE,
							part_len);
		copied_len += part_len;

		page_no = btr_blob_get_next_page_no(blob_header);

		mtr_commit(&mtr);

		if (page_no == FIL_NULL) {
			ut_a(copied_len == local_len + extern_len);

			*len = copied_len;

			return(buf);
		}

		/* On other BLOB pages except the first the BLOB header
		always is at the page data start: */

		offset = FIL_PAGE_DATA;

		ut_a(copied_len < local_len + extern_len);
	}
}

/***********************************************************************
Copies an externally stored field of a record to mem heap. */

byte*
btr_rec_copy_externally_stored_field(
/*=================================*/
				/* out: the field copied to heap */
	rec_t*		rec,	/* in: record */
	const ulint*	offsets,/* in: array returned by rec_get_offsets() */
	ulint		no,	/* in: field number */
	ulint*		len,	/* out: length of the field */
	mem_heap_t*	heap)	/* in: mem heap */
{
	ulint	local_len;
	byte*	data;

	ut_ad(rec_offs_validate(rec, NULL, offsets));
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

	return(btr_copy_externally_stored_field(len, data, local_len, heap));
}
