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

ulint	btr_cur_rnd	= 0;

ulint	btr_cur_n_non_sea	= 0;

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
	upd_t*		update,	/* in: update vector */
	mtr_t*		mtr);	/* in: mini-transaction handle which contains
				an X-latch to record page and to the tree */

/*==================== B-TREE SEARCH =========================*/
	
/************************************************************************
Latches the leaf page or pages requested. */
static
void
btr_cur_latch_leaves(
/*=================*/
	dict_tree_t*	tree,		/* in: index tree */
	page_t*		page,		/* in: leaf page where the search
					converged */
	ulint		space,		/* in: space id */
	ulint		page_no,	/* in: page number of the leaf */
	ulint		latch_mode,	/* in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor, 	/* in: cursor */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint	left_page_no;
	ulint	right_page_no;

	ut_ad(tree && page && mtr);

	if (latch_mode == BTR_SEARCH_LEAF) {
	
		btr_page_get(space, page_no, RW_S_LATCH, mtr);

	} else if (latch_mode == BTR_MODIFY_LEAF) {

		btr_page_get(space, page_no, RW_X_LATCH, mtr);

	} else if (latch_mode == BTR_MODIFY_TREE) {

		/* x-latch also brothers from left to right */
		left_page_no = btr_page_get_prev(page, mtr);

		if (left_page_no != FIL_NULL) {
			btr_page_get(space, left_page_no, RW_X_LATCH, mtr);
		}
				
		btr_page_get(space, page_no, RW_X_LATCH, mtr);

		right_page_no = btr_page_get_next(page, mtr);

		if (right_page_no != FIL_NULL) {
			btr_page_get(space, right_page_no, RW_X_LATCH, mtr);
		}

	} else if (latch_mode == BTR_SEARCH_PREV) {

		/* s-latch also left brother */
		left_page_no = btr_page_get_prev(page, mtr);

		if (left_page_no != FIL_NULL) {
			cursor->left_page = btr_page_get(space, left_page_no,
							RW_S_LATCH, mtr);
		}

		btr_page_get(space, page_no, RW_S_LATCH, mtr);

	} else if (latch_mode == BTR_MODIFY_PREV) {

		/* x-latch also left brother */
		left_page_no = btr_page_get_prev(page, mtr);

		if (left_page_no != FIL_NULL) {
			cursor->left_page = btr_page_get(space, left_page_no,
							RW_X_LATCH, mtr);
		}

		btr_page_get(space, page_no, RW_X_LATCH, mtr);
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
				NOTE that if the search is made using a unique
				prefix of a record, mode should be
				PAGE_CUR_LE, not PAGE_CUR_GE, as the latter
				may end up on the previous page relative to the
				record! Inserts should always be made using
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
	ulint 		low_bytes;
	ulint		height;
	ulint		savepoint;
	ulint		rw_latch;
	ulint		page_mode;
	ulint		insert_planned;
	ulint		buf_mode;
	ulint		estimate;
	ulint		root_height;
#ifdef BTR_CUR_ADAPT
	btr_search_t*	info;
#endif
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
	latch_mode = latch_mode & ~(BTR_INSERT | BTR_ESTIMATE);

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
	if (latch_mode <= BTR_MODIFY_LEAF && info->last_hash_succ
		&& !estimate
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
	        return;
	}
#endif
#endif

#ifdef UNIV_SEARCH_PERF_STAT
	btr_cur_n_non_sea++;
#endif
	/* If the hash search did not succeed, do binary search down the
	tree */

	if (has_search_latch) {
		/* Release possible search latch to obey latching order */
		rw_lock_s_unlock(&btr_search_latch);
	}

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

	if (mode == PAGE_CUR_GE) {
		page_mode = PAGE_CUR_L;
	} else if (mode == PAGE_CUR_G) {
		page_mode = PAGE_CUR_LE;
	} else if (mode == PAGE_CUR_LE) {
		page_mode = PAGE_CUR_LE;
	} else {
		ut_ad(mode == PAGE_CUR_L);
		page_mode = PAGE_CUR_L;
	}
			
	/* Loop and search until we arrive at the desired level */

	for (;;) {
		if ((height == 0) && (latch_mode <= BTR_MODIFY_LEAF)) {

			rw_latch = latch_mode;

			if (insert_planned && ibuf_should_try(index)) {
				
				/* Try insert to the insert buffer if the
				page is not in the buffer pool */

				buf_mode = BUF_GET_IF_IN_POOL;
			}
		}
retry_page_get:		
		page = buf_page_get_gen(space, page_no, rw_latch, guess,
					buf_mode,
#ifdef UNIV_SYNC_DEBUG
					IB__FILE__, __LINE__,
#endif
					mtr);

		if (page == NULL) {
			/* This must be a search to perform an insert;
			try insert to the insert buffer */

			ut_ad(buf_mode == BUF_GET_IF_IN_POOL);
			ut_ad(insert_planned);
			ut_ad(cursor->thr);

			if (ibuf_should_try(index) &&
				ibuf_insert(tuple, index, space, page_no,
							cursor->thr)) {
				/* Insertion to the insert buffer succeeded */
				cursor->flag = BTR_CUR_INSERT_TO_IBUF;

				return;
			}

			/* Insert to the insert buffer did not succeed:
			retry page get */

			buf_mode = BUF_GET;

			goto retry_page_get;
		}
			
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

				btr_cur_latch_leaves(tree, page, space,
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

		page_cur_search_with_match(page, tuple, page_mode, &up_match,
					&up_bytes, &low_match, &low_bytes,
					page_cursor);
		if (estimate) {
			btr_cur_add_path_info(cursor, height, root_height);
		}	

		/* If this is the desired level, leave the loop */

		if (level == height) {

			if (level > 0) {
				/* x-latch the page */
				btr_page_get(space, page_no, RW_X_LATCH, mtr);
			}

			break;
		}

		ut_ad(height > 0);

		height--;
		guess = NULL;

		node_ptr = page_cur_get_rec(page_cursor);
		
		/* Go to the child node */
		page_no = btr_node_ptr_get_child_page_no(node_ptr);
	}

	if (level == 0) {
		cursor->low_match = low_match;
		cursor->low_bytes = low_bytes;
		cursor->up_match = up_match;
		cursor->up_bytes = up_bytes;

#ifdef BTR_CUR_ADAPT		
		btr_search_info_update(index, cursor);
#endif

		ut_ad(cursor->up_match != ULINT_UNDEFINED
						|| mode != PAGE_CUR_GE);
		ut_ad(cursor->up_match != ULINT_UNDEFINED
						|| mode != PAGE_CUR_LE);
		ut_ad(cursor->low_match != ULINT_UNDEFINED
						|| mode != PAGE_CUR_LE);
	}

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
	ulint		root_height;
	rec_t*		node_ptr;
	ulint		estimate;

	estimate = latch_mode & BTR_ESTIMATE;
	latch_mode = latch_mode & ~BTR_ESTIMATE;
	
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
#ifdef UNIV_SYNC_DEBUG
					IB__FILE__, __LINE__,
#endif
					mtr);
		ut_ad(0 == ut_dulint_cmp(tree->id,
						btr_page_get_index_id(page)));

		if (height == ULINT_UNDEFINED) {
			/* We are in the root node */

			height = btr_page_get_level(page, mtr);
			root_height = height;
		}

		if (height == 0) {
			btr_cur_latch_leaves(tree, page, space, page_no,
						latch_mode, cursor, mtr);
		}
		
		if (from_left) {
			page_cur_set_before_first(page, page_cursor);
		} else {
			page_cur_set_after_last(page, page_cursor);
		}

		if (estimate) {
			btr_cur_add_path_info(cursor, height, root_height);
		}

		if (height == 0) {

			break;
		}

		ut_ad(height > 0);

		if (from_left) {
			page_cur_move_to_next(page_cursor);
		} else {
			page_cur_move_to_prev(page_cursor);
		}

		height--;

		node_ptr = page_cur_get_rec(page_cursor);
		
		/* Go to the child node */
		page_no = btr_node_ptr_get_child_page_no(node_ptr);
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
#ifdef UNIV_SYNC_DEBUG
					IB__FILE__, __LINE__,
#endif
					mtr);
		ut_ad(0 == ut_dulint_cmp(tree->id,
						btr_page_get_index_id(page)));

		if (height == ULINT_UNDEFINED) {
			/* We are in the root node */

			height = btr_page_get_level(page, mtr);
		}

		if (height == 0) {
			btr_cur_latch_leaves(tree, page, space, page_no,
						latch_mode, cursor, mtr);
		}

		page_cur_open_on_rnd_user_rec(page, page_cursor);	

		if (height == 0) {

			break;
		}

		ut_ad(height > 0);

		height--;

		node_ptr = page_cur_get_rec(page_cursor);
		
		/* Go to the child node */
		page_no = btr_node_ptr_get_child_page_no(node_ptr);
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
	rec = page_cur_tuple_insert(page_cursor, tuple, mtr);	

	if (!rec) {
		/* If record did not fit, reorganize */

		btr_page_reorganize(page, mtr);

		*reorg = TRUE;

		page_cur_search(page, tuple, PAGE_CUR_LE, page_cursor);

		rec = page_cur_tuple_insert(page_cursor, tuple, mtr);
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
	ulint		data_size;
	ulint		extra_size;
	ulint		type;
	ulint		err;
	
	ut_ad(dtuple_check_typed(entry));

	*big_rec = NULL;

	page = btr_cur_get_page(cursor);
	index = cursor->index;

	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
							MTR_MEMO_PAGE_X_FIX));
	max_size = page_get_max_insert_size_after_reorganize(page, 1);
	level = btr_page_get_level(page, mtr);

calculate_sizes_again:
	/* Calculate the record size when entry is converted to a record */
	data_size = dtuple_get_data_size(entry);
	extra_size = rec_get_converted_extra_size(data_size,
						dtuple_get_n_fields(entry));
	rec_size = data_size + extra_size;

	if ((rec_size >= page_get_free_space_of_empty() / 2)
	    || (rec_size >= REC_MAX_DATA_SIZE)) {

		/* The record is so big that we have to store some fields
		externally on separate database pages */
		
                big_rec_vec = dtuple_convert_big_rec(index, entry);

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

	*rec = page_cur_insert_rec_low(page_cursor, entry, data_size,
								NULL, mtr);	
	if (!(*rec)) {
		/* If the record did not fit, reorganize */
		btr_page_reorganize(page, mtr);

		ut_ad(page_get_max_insert_size(page, 1) == max_size);
		
		reorg = TRUE;

		page_cur_search(page, entry, PAGE_CUR_LE, page_cursor);

		*rec = page_cur_tuple_insert(page_cursor, entry, mtr);

		if (!(*rec)) {
			char* err_buf = mem_alloc(1000);

			dtuple_sprintf(err_buf, 900, entry);
			
			fprintf(stderr,
	"InnoDB: Error: cannot insert tuple %s to index %s of table %s\n"
	"InnoDB: max insert size %lu\n",
			err_buf, index->name, index->table->name, max_size);

			mem_free(err_buf);
		}
		
		ut_a(*rec); /* <- We calculated above the record would fit */
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

/*	printf("Insert to page %lu, max ins size %lu, rec %lu ind type %lu\n",
			buf_frame_get_page_no(page), max_size,
					rec_size + PAGE_DIR_SLOT_SIZE, type);
*/	
	if (!(type & (DICT_CLUSTERED | DICT_UNIQUE))) {
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

		success = fsp_reserve_free_extents(index->space,
						n_extents, FSP_NORMAL, mtr);
		if (!success) {
			err = DB_OUT_OF_FILE_SPACE;

			return(err);
		}
	}

	if ((rec_get_converted_size(entry)
				>= page_get_free_space_of_empty() / 2)
	    || (rec_get_converted_size(entry) >= REC_MAX_DATA_SIZE)) {

		/* The record is so big that we have to store some fields
		externally on separate database pages */
		
                big_rec_vec = dtuple_convert_big_rec(index, entry);

		if (big_rec_vec == NULL) {
		
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
		fil_space_release_free_extents(index->space, n_extents);
	}

	*big_rec = big_rec_vec;

	return(err);
}

/*==================== B-TREE UPDATE =========================*/
/* Only clustered index records are modified using these functions */

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

	/* Only clustered index records are updated using this function */
	ut_ad((cursor->index)->type & DICT_CLUSTERED);

	rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	
	/* Check if we have to wait for a lock: enqueue an explicit lock
	request if yes */

	err = DB_SUCCESS;

	if (!(flags & BTR_NO_LOCKING_FLAG)) {
		err = lock_clust_rec_modify_check_and_lock(flags, rec, index,
									thr);
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

	log_ptr = mlog_open(mtr, 30 + MLOG_BUF_MARGIN);

	log_ptr = mlog_write_initial_log_record_fast(rec,
				MLOG_REC_UPDATE_IN_PLACE, log_ptr, mtr);

	mach_write_to_1(log_ptr, flags);
	log_ptr++;

	log_ptr = row_upd_write_sys_vals_to_log(index, trx, roll_ptr, log_ptr,
									mtr);
	mach_write_to_2(log_ptr, rec - buf_frame_align(rec));
	log_ptr += 2;

	row_upd_index_write_log(update, log_ptr, mtr);
}	

/***************************************************************
Parses a redo log record of updating a record in-place. */

byte*
btr_cur_parse_update_in_place(
/*==========================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page)	/* in: page or NULL */
{
	ulint	flags;
	rec_t*	rec;
	upd_t*	update;
	ulint	pos;
	dulint	trx_id;
	dulint	roll_ptr;
	ulint	rec_offset;
	mem_heap_t* heap;

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

	heap = mem_heap_create(256);
	
	ptr = row_upd_index_parse(ptr, end_ptr, heap, &update);

	if (ptr == NULL) {
		mem_heap_free(heap);
		
		return(NULL);
	}

	if (!page) {
		mem_heap_free(heap);

		return(ptr);
	}
	
	rec = page + rec_offset;
	
	/* We do not need to reserve btr_search_latch, as the page is only
	being recovered, and there cannot be a hash index to it. */

	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_rec_sys_fields_in_recovery(rec, pos, trx_id, roll_ptr);
	}

	row_upd_rec_in_place(rec, update);

	mem_heap_free(heap);

	return(ptr);
}

/*****************************************************************
Updates a record when the update causes no size changes in its fields. */

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
	dulint		roll_ptr;
	trx_t*		trx;

	/* Only clustered index records are updated using this function */
	ut_ad((cursor->index)->type & DICT_CLUSTERED);

	rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	trx = thr_get_trx(thr);
	
	/* Do lock checking and undo logging */
	err = btr_cur_upd_lock_and_undo(flags, cursor, update, cmpl_info,
							thr, &roll_ptr);
	if (err != DB_SUCCESS) {

		return(err);
	}

	block = buf_block_align(rec);

	if (block->is_hashed) {
		rw_lock_x_lock(&btr_search_latch);
	}

	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_rec_sys_fields(rec, index, trx, roll_ptr);
	}

	/* FIXME: in a mixed tree, all records may not have enough ordering
	fields for btr search: */
	
	row_upd_rec_in_place(rec, update);

	if (block->is_hashed) {
		rw_lock_x_unlock(&btr_search_latch);
	}

	btr_cur_update_in_place_log(flags, rec, index, update, trx, roll_ptr,
									mtr);
	return(DB_SUCCESS);
}

/*****************************************************************
Tries to update a record on a page in an index tree. It is assumed that mtr
holds an x-latch on the page. The operation does not succeed if there is too
little space on the page or if the update would result in too empty a page,
so that tree compression is recommended. */

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

	/* Only clustered index records are updated using this function */
	ut_ad((cursor->index)->type & DICT_CLUSTERED);
	
	page = btr_cur_get_page(cursor);
	rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	
	ut_ad(mtr_memo_contains(mtr, buf_block_align(page),
							MTR_MEMO_PAGE_X_FIX));
	if (!row_upd_changes_field_size(rec, index, update)) {

		/* The simplest and most common case: the update does not
		change the size of any field */

		return(btr_cur_update_in_place(flags, cursor, update,
							cmpl_info, thr, mtr));
	}

	for (i = 0; i < upd_get_n_fields(update); i++) {
		if (upd_get_nth_field(update, i)->extern_storage) {

			/* Externally stored fields are treated in pessimistic
			update */

			return(DB_OVERFLOW);
		}
	}

	if (rec_contains_externally_stored_field(btr_cur_get_rec(cursor))) {
		/* Externally stored fields are treated in pessimistic
		update */

		return(DB_OVERFLOW);
	}
	
	page_cursor = btr_cur_get_page_cur(cursor);
	
	heap = mem_heap_create(1024);
	
	new_entry = row_rec_to_index_entry(ROW_COPY_DATA, index, rec, heap);

	row_upd_clust_index_replace_new_col_vals(new_entry, update);

	old_rec_size = rec_get_size(rec);
	new_rec_size = rec_get_converted_size(new_entry);
	
	if (new_rec_size >= page_get_free_space_of_empty() / 2) {

		mem_heap_free(heap);		

		return(DB_OVERFLOW);
	}

	max_size = old_rec_size
			+ page_get_max_insert_size_after_reorganize(page, 1);

	if (page_get_data_size(page) - old_rec_size + new_rec_size
					< BTR_CUR_PAGE_COMPRESS_LIMIT) {

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

	lock_rec_store_on_page_infimum(rec);

	btr_search_update_hash_on_delete(cursor);

        page_cur_delete_rec(page_cursor, mtr);

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

	/* We must already have an x-latch to prev_page! */
	ut_ad(mtr_memo_contains(mtr, buf_block_align(prev_page),
		      				MTR_MEMO_PAGE_X_FIX));

	lock_rec_reset_and_inherit_gap_locks(page_get_supremum_rec(prev_page),
									rec);
}
		   
/***************************************************************
Replaces and copies the data in the new column values stored in the
update vector to the clustered index entry given. */
static
void
btr_cur_copy_new_col_vals(
/*======================*/
	dtuple_t*	entry,	/* in/out: index entry where replaced */
	upd_t*		update,	/* in: update vector */
	mem_heap_t*	heap)	/* in: heap where data is copied */
{
	upd_field_t*	upd_field;
	dfield_t*	dfield;
	dfield_t*	new_val;
	ulint		field_no;
	byte*		data;
	ulint		i;

	dtuple_set_info_bits(entry, update->info_bits);

	for (i = 0; i < upd_get_n_fields(update); i++) {

		upd_field = upd_get_nth_field(update, i);

		field_no = upd_field->field_no;

		dfield = dtuple_get_nth_field(entry, field_no);

		new_val = &(upd_field->new_val);

		if (new_val->len == UNIV_SQL_NULL) {
			data = NULL;
		} else {
			data = mem_heap_alloc(heap, new_val->len);

			ut_memcpy(data, new_val->data, new_val->len);
		}

		dfield_set_data(dfield, data, new_val->len);
	}
}

/*****************************************************************
Performs an update of a record on a page of a tree. It is assumed
that mtr holds an x-latch on the tree and on the cursor page. If the
update is made on the leaf level, to avoid deadlocks, mtr must also
own x-latches to brothers of page, if those brothers exist. */

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
	ulint*		ext_vect;
	ulint		n_ext_vect;
	ulint		reserve_flag;
	
	*big_rec = NULL;
	
	page = btr_cur_get_page(cursor);
	rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	tree = index->tree;

	ut_ad(index->type & DICT_CLUSTERED);
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
		
		success = fsp_reserve_free_extents(cursor->index->space,
						n_extents, reserve_flag, mtr);
		if (!success) {
			err = DB_OUT_OF_FILE_SPACE;

			return(err);
		}
	}
	
	heap = mem_heap_create(1024);

	trx = thr_get_trx(thr);
	
	new_entry = row_rec_to_index_entry(ROW_COPY_DATA, index, rec, heap);

	btr_cur_copy_new_col_vals(new_entry, update, heap);

	if (!(flags & BTR_KEEP_SYS_FLAG)) {
		row_upd_index_entry_sys_field(new_entry, index, DATA_ROLL_PTR,
								roll_ptr);
		row_upd_index_entry_sys_field(new_entry, index, DATA_TRX_ID,
								trx->id);
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

	lock_rec_store_on_page_infimum(rec);

	btr_search_update_hash_on_delete(cursor);

	if (flags & BTR_NO_UNDO_LOG_FLAG) {
		/* We are in a transaction rollback undoing a row
		update: we must free possible externally stored fields
		which got new values in the update */

		ut_a(big_rec_vec == NULL);
		
		btr_rec_free_updated_extern_fields(index, rec, update, mtr);
	}

	/* We have to set appropriate extern storage bits in the new
	record to be inserted: we have to remember which fields were such */

	ext_vect = mem_heap_alloc(heap, sizeof(ulint) * rec_get_n_fields(rec));
	n_ext_vect = btr_push_update_extern_fields(ext_vect, rec, update);
	
	page_cur_delete_rec(page_cursor, mtr);

	page_cur_move_to_prev(page_cursor);

	if ((rec_get_converted_size(new_entry) >=
				page_get_free_space_of_empty() / 2)
	    || (rec_get_converted_size(new_entry) >= REC_MAX_DATA_SIZE)) {

                big_rec_vec = dtuple_convert_big_rec(index, new_entry);

		if (big_rec_vec == NULL) {

			mem_heap_free(heap);
		
			goto return_after_reservations;
		}
	}

	rec = btr_cur_insert_if_possible(cursor, new_entry,
						&dummy_reorganized, mtr);
	ut_a(rec || optim_err != DB_UNDERFLOW);

	if (rec) {
		lock_rec_restore_from_page_infimum(rec, page);
		rec_set_field_extern_bits(rec, ext_vect, n_ext_vect, mtr);
		
		btr_cur_compress_if_useful(cursor, mtr);

		err = DB_SUCCESS;
		mem_heap_free(heap);

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

	rec_set_field_extern_bits(rec, ext_vect, n_ext_vect, mtr);

	lock_rec_restore_from_page_infimum(rec, page);

	/* If necessary, restore also the correct lock state for a new,
	preceding supremum record created in a page split. While the old
	record was nonexistent, the supremum might have inherited its locks
	from a wrong record. */

	if (!was_first) {
		btr_cur_pess_upd_restore_supremum(rec, mtr);
	}

	mem_heap_free(heap);

return_after_reservations:

	if (n_extents > 0) {
		fil_space_release_free_extents(cursor->index->space,
							n_extents);
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

	log_ptr = mlog_open(mtr, 30);

	log_ptr = mlog_write_initial_log_record_fast(rec,
				MLOG_REC_CLUST_DELETE_MARK, log_ptr, mtr);

	mach_write_to_1(log_ptr, flags);
	log_ptr++;
	mach_write_to_1(log_ptr, val);
	log_ptr++;

	log_ptr = row_upd_write_sys_vals_to_log(index, trx, roll_ptr, log_ptr,
									mtr);
	mach_write_to_2(log_ptr, rec - buf_frame_align(rec));
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
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page)	/* in: page or NULL */	
{
	ulint	flags;
	ibool	val;
	ulint	pos;
	dulint	trx_id;
	dulint	roll_ptr;
	ulint	offset;
	rec_t*	rec;

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

	if (page) {
		rec = page + offset;
	
		if (!(flags & BTR_KEEP_SYS_FLAG)) {
			row_upd_rec_sys_fields_in_recovery(rec, pos, trx_id,
								roll_ptr);
		}

		/* We do not need to reserve btr_search_latch, as the page
		is only being recovered, and there cannot be a hash index to
		it. */

		rec_set_deleted_flag(rec, val);
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
	
	rec = btr_cur_get_rec(cursor);
	index = cursor->index;
	
	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(rec_get_deleted_flag(rec) == FALSE);

	err = lock_clust_rec_modify_check_and_lock(flags, rec, index, thr);

	if (err != DB_SUCCESS) {

		return(err);
	}

	err = trx_undo_report_row_operation(flags, TRX_UNDO_MODIFY_OP, thr,
						index, NULL, NULL, 0, rec,
						&roll_ptr);
	if (err != DB_SUCCESS) {

		return(err);
	}

	block = buf_block_align(rec);

	if (block->is_hashed) {
		rw_lock_x_lock(&btr_search_latch);
	}

	rec_set_deleted_flag(rec, val);

	trx = thr_get_trx(thr);
	
	if (!(flags & BTR_KEEP_SYS_FLAG)) {

		row_upd_rec_sys_fields(rec, index, trx, roll_ptr);
	}
	
	if (block->is_hashed) {
		rw_lock_x_unlock(&btr_search_latch);
	}

	btr_cur_del_mark_set_clust_rec_log(flags, rec, index, val, trx,
							roll_ptr, mtr);
	return(DB_SUCCESS);
}

/********************************************************************
Writes the redo log record for a delete mark setting of a secondary
index record. */
UNIV_INLINE
void
btr_cur_del_mark_set_sec_rec_log(
/*=============================*/
	rec_t*	rec,	/* in: record */
	ibool	val,	/* in: value to set */
	mtr_t*	mtr)	/* in: mtr */
{
	byte*	log_ptr;

	log_ptr = mlog_open(mtr, 30);

	log_ptr = mlog_write_initial_log_record_fast(rec,
				MLOG_REC_SEC_DELETE_MARK, log_ptr, mtr);

	mach_write_to_1(log_ptr, val);
	log_ptr++;

	mach_write_to_2(log_ptr, rec - buf_frame_align(rec));
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
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page)	/* in: page or NULL */	
{
	ibool	val;
	ulint	offset;
	rec_t*	rec;

	if (end_ptr < ptr + 3) {

		return(NULL);
	}
	
	val = mach_read_from_1(ptr);
	ptr++;

	offset = mach_read_from_2(ptr);
	ptr += 2;

	if (page) {
		rec = page + offset;
	
		/* We do not need to reserve btr_search_latch, as the page
		is only being recovered, and there cannot be a hash index to
		it. */

		rec_set_deleted_flag(rec, val);
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

	err = lock_sec_rec_modify_check_and_lock(flags, rec, cursor->index,
									thr);
	if (err != DB_SUCCESS) {

		return(err);
	}

	block = buf_block_align(rec);
	
	if (block->is_hashed) {
		rw_lock_x_lock(&btr_search_latch);
	}

	rec_set_deleted_flag(rec, val);

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
	rec_t*	rec,	/* in: record to delete unmark */
	mtr_t*	mtr)	/* in: mtr */
{
	/* We do not need to reserve btr_search_latch, as the page has just
	been read to the buffer pool and there cannot be a hash index to it. */

	rec_set_deleted_flag(rec, FALSE);

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
	ut_ad(mtr_memo_contains(mtr, buf_block_align(
						btr_cur_get_page(cursor)),
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
	ut_ad(mtr_memo_contains(mtr, buf_block_align(
						btr_cur_get_page(cursor)),
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
	page_t*	page;
	ulint	max_ins_size;

	ut_ad(mtr_memo_contains(mtr, buf_block_align(btr_cur_get_page(cursor)),
							MTR_MEMO_PAGE_X_FIX));
	/* This is intended only for leaf page deletions */

	page = btr_cur_get_page(cursor);
	
	ut_ad(btr_page_get_level(page, mtr) == 0);

	if (rec_contains_externally_stored_field(btr_cur_get_rec(cursor))) {

		return(FALSE);
	}

	if (btr_cur_can_delete_without_compress(cursor, mtr)) {

		lock_update_delete(btr_cur_get_rec(cursor));

		btr_search_update_hash_on_delete(cursor);

		max_ins_size = page_get_max_insert_size_after_reorganize(page,
									1);
		page_cur_delete_rec(btr_cur_get_page_cur(cursor), mtr);

		ibuf_update_free_bits_low(cursor->index, page, max_ins_size,
									mtr);
		return(TRUE);
	}

	return(FALSE);
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
	mtr_t*		mtr)	/* in: mtr */
{
	page_t*		page;
	dict_tree_t*	tree;
	rec_t*		rec;
	dtuple_t*	node_ptr;
	ulint		n_extents	= 0;
	ibool		success;
	ibool		ret		= FALSE;
	mem_heap_t*	heap;
	
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

		success = fsp_reserve_free_extents(cursor->index->space,
						n_extents, FSP_CLEANING, mtr);
		if (!success) {
			*err = DB_OUT_OF_FILE_SPACE;

			return(FALSE);
		}
	}

	btr_rec_free_externally_stored_fields(cursor->index,
					btr_cur_get_rec(cursor), mtr);
	if ((page_get_n_recs(page) < 2)
	    && (dict_tree_get_page(btr_cur_get_tree(cursor))
					!= buf_frame_get_page_no(page))) {

		/* If there is only one record, drop the whole page in
		btr_discard_page, if this is not the root page */
	
		btr_discard_page(cursor, mtr);

		*err = DB_SUCCESS;
		ret = TRUE;

		goto return_after_reservations;	
	}

	rec = btr_cur_get_rec(cursor);
	
	lock_update_delete(rec);

	if ((btr_page_get_level(page, mtr) > 0)
	    && (page_rec_get_next(page_get_infimum_rec(page)) == rec)) {

		if (btr_page_get_prev(page, mtr) == FIL_NULL) {

			/* If we delete the leftmost node pointer on a
			non-leaf level, we must mark the new leftmost node
			pointer as the predefined minimum record */

	    		btr_set_min_rec_mark(page_rec_get_next(rec), mtr);
		} else {
			/* Otherwise, if we delete the leftmost node pointer
			on a page, we have to change the father node pointer
			so that it is equal to the new leftmost node pointer
			on the page */

			btr_node_ptr_delete(tree, page, mtr);

			heap = mem_heap_create(256);

			node_ptr = dict_tree_build_node_ptr(
						tree, page_rec_get_next(rec),
						buf_frame_get_page_no(page),
						heap);

			btr_insert_on_non_leaf_level(tree,
					btr_page_get_level(page, mtr) + 1,
					node_ptr, mtr);

			mem_heap_free(heap);
		}
	} 

	btr_search_update_hash_on_delete(cursor);

	page_cur_delete_rec(btr_cur_get_page_cur(cursor), mtr);

	ut_ad(btr_check_node_ptr(tree, page, mtr));

	*err = DB_SUCCESS;
	
return_after_reservations:

	if (ret == FALSE) {
		ret = btr_cur_compress_if_useful(cursor, mtr);
	}

	if (n_extents > 0) {
		fil_space_release_free_extents(cursor->index->space, n_extents);
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

ulint
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
	ulint		n_rows;
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
	diverged = FALSE;
	
	for (i = 0; ; i++) {
		ut_ad(i < BTR_PATH_ARRAY_N_SLOTS);
	
		slot1 = path1 + i;
		slot2 = path2 + i;

		if (slot1->nth_rec == ULINT_UNDEFINED
				|| slot2->nth_rec == ULINT_UNDEFINED) {

			return(n_rows);
		}

		if (!diverged && slot1->nth_rec != slot2->nth_rec) {

			if (slot1->nth_rec < slot2->nth_rec) {
				n_rows = slot2->nth_rec - slot1->nth_rec;
			} else {
				/* Maybe the tree has changed between
				searches */

				return(10);
			}

			diverged = TRUE;
		} else if (diverged) {
			n_rows = (n_rows * (slot1->n_recs + slot2->n_recs))
									/ 2;
		}	
	}
}

/***********************************************************************
Estimates the number of different key values in a given index. */

ulint
btr_estimate_number_of_different_key_vals(
/*======================================*/
				/* out: estimated number of key values */
	dict_index_t*	index)	/* in: index */
{
	btr_cur_t	cursor;
	page_t*		page;
	rec_t*		rec;
	ulint		total_n_recs		= 0;
	ulint		n_diff_in_page;
	ulint		n_diff			= 0;
	ulint		matched_fields;
	ulint		matched_bytes;
	ulint		i;
	mtr_t		mtr;

	if (index->type & DICT_UNIQUE) {
		return(index->table->stat_n_rows);
	}

	/* We sample some pages in the index to get an estimate */
	
	for (i = 0; i < BTR_KEY_VAL_ESTIMATE_N_PAGES; i++) {
		mtr_start(&mtr);

		btr_cur_open_at_rnd_pos(index, BTR_SEARCH_LEAF, &cursor, &mtr);
		
		/* Count the number of different key values minus one on this
		index page: we subtract one because otherwise our algorithm
		would give a wrong estimate for an index where there is
		just one key value */

		page = btr_cur_get_page(&cursor);

		rec = page_get_infimum_rec(page);
		rec = page_rec_get_next(rec);

		n_diff_in_page = 0;
		
		while (rec != page_get_supremum_rec(page)
		       && page_rec_get_next(rec)
					!= page_get_supremum_rec(page)) {
			matched_fields = 0;
			matched_bytes = 0;

			cmp_rec_rec_with_match(rec, page_rec_get_next(rec),
						index, &matched_fields,
						&matched_bytes);
			if (matched_fields <
				dict_index_get_n_ordering_defined_by_user(
								index)) {
				n_diff_in_page++;
			}

			rec = page_rec_get_next(rec);
		}

		n_diff += n_diff_in_page;
		
		total_n_recs += page_get_n_recs(page);
		
		mtr_commit(&mtr);
	}

	if (n_diff == 0) {
		/* We play safe and assume that there are just two different
		key values in the index */
		
		return(2);
	}

	return(index->table->stat_n_rows / (total_n_recs / n_diff));
}

/*================== EXTERNAL STORAGE OF BIG FIELDS ===================*/

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
	ulint*	ext_vect,	/* in: array of ulints, must be preallocated
				to have space for all fields in rec */
	rec_t*	rec,		/* in: record */
	upd_t*	update)		/* in: update vector or NULL */
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

	n = rec_get_n_fields(rec);

	for (i = 0; i < n; i++) {
		if (rec_get_nth_field_extern_bit(rec, i)) {
			
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
	big_rec_t*	big_rec_vec,	/* in: vector containing fields
					to be stored externally */
	mtr_t*		local_mtr)	/* in: mtr containing the latch to
					rec and to the tree */
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

	ut_ad(mtr_memo_contains(local_mtr, dict_tree_get_lock(index->tree),
							MTR_MEMO_X_LOCK));
	ut_ad(mtr_memo_contains(local_mtr, buf_block_align(data),
							MTR_MEMO_PAGE_X_FIX));	
	ut_a(index->type & DICT_CLUSTERED);
							
	space_id = buf_frame_get_space_id(rec);
	
	/* We have to create a file segment to the tablespace
	for each field and put the pointer to the field in rec */

	for (i = 0; i < big_rec_vec->n_fields; i++) {

		data = rec_get_nth_field(rec, big_rec_vec->fields[i].field_no,
								&local_len);
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

			page_no = buf_frame_get_page_no(page);

			if (prev_page_no != FIL_NULL) {
				prev_page = buf_page_get(space_id,
						prev_page_no,
						RW_X_LATCH, &mtr);

				buf_page_dbg_add_level(prev_page,
							SYNC_EXTERN_STORAGE);
							
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

			buf_page_dbg_add_level(rec_page, SYNC_NO_ORDER_CHECK);

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

				rec_set_nth_field_extern_bit(rec,
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
management. */

void
btr_free_externally_stored_field(
/*=============================*/
	dict_index_t*	index,		/* in: index of the data, the index
					tree MUST be X-latched */
	byte*		data,		/* in: internally stored data
					+ reference to the externally
					stored part */
	ulint		local_len,	/* in: length of data */
	mtr_t*		local_mtr)	/* in: mtr containing the latch to
					data an an X-latch to the index
					tree */
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

		buf_page_dbg_add_level(rec_page, SYNC_NO_ORDER_CHECK);

		space_id = mach_read_from_4(data + local_len
						+ BTR_EXTERN_SPACE_ID);

		page_no = mach_read_from_4(data + local_len
						+ BTR_EXTERN_PAGE_NO);

		offset = mach_read_from_4(data + local_len + BTR_EXTERN_OFFSET);

		extern_len = mach_read_from_4(data + local_len
						+ BTR_EXTERN_LEN + 4);

		/* If extern len is 0, then there is no external storage data
		at all */

		if (extern_len == 0) {

			mtr_commit(&mtr);

			return;
		}

		page = buf_page_get(space_id, page_no, RW_X_LATCH, &mtr);
		
		buf_page_dbg_add_level(page, SYNC_EXTERN_STORAGE);

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
	mtr_t*		mtr)	/* in: mini-transaction handle which contains
				an X-latch to record page and to the index
				tree */
{
	ulint	n_fields;
	byte*	data;
	ulint	len;
	ulint	i;

	ut_ad(mtr_memo_contains(mtr, buf_block_align(rec),
							MTR_MEMO_PAGE_X_FIX));
	if (rec_get_data_size(rec) <= REC_1BYTE_OFFS_LIMIT) {

		return;
	}
	
	/* Free possible externally stored fields in the record */

	n_fields = rec_get_n_fields(rec);

	for (i = 0; i < n_fields; i++) {
		if (rec_get_nth_field_extern_bit(rec, i)) {

			data = rec_get_nth_field(rec, i, &len);
			btr_free_externally_stored_field(index, data, len, mtr);
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
	upd_t*		update,	/* in: update vector */
	mtr_t*		mtr)	/* in: mini-transaction handle which contains
				an X-latch to record page and to the tree */
{
	upd_field_t*	ufield;
	ulint		n_fields;
	byte*		data;
	ulint		len;
	ulint		i;

	ut_ad(mtr_memo_contains(mtr, buf_block_align(rec),
							MTR_MEMO_PAGE_X_FIX));
	if (rec_get_data_size(rec) <= REC_1BYTE_OFFS_LIMIT) {

		return;
	}
	
	/* Free possible externally stored fields in the record */

	n_fields = upd_get_n_fields(update);

	for (i = 0; i < n_fields; i++) {
		ufield = upd_get_nth_field(update, i);
	
		if (rec_get_nth_field_extern_bit(rec, ufield->field_no)) {

			data = rec_get_nth_field(rec, ufield->field_no, &len);
			btr_free_externally_stored_field(index, data, len, mtr);
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
	
		buf_page_dbg_add_level(page, SYNC_EXTERN_STORAGE);

		blob_header = page + offset;

		part_len = btr_blob_get_part_len(blob_header);

		ut_memcpy(buf + copied_len, blob_header + BTR_BLOB_HDR_SIZE,
							part_len);
		copied_len += part_len;

		page_no = btr_blob_get_next_page_no(blob_header);

		/* On other BLOB pages except the first the BLOB header
		always is at the page data start: */

		offset = FIL_PAGE_DATA;

		mtr_commit(&mtr);

		if (page_no == FIL_NULL) {
			ut_a(copied_len == local_len + extern_len);

			*len = copied_len;
		
			return(buf);
		}

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
	ulint		no,	/* in: field number */
	ulint*		len,	/* out: length of the field */
	mem_heap_t*	heap)	/* in: mem heap */
{
	ulint	local_len;
	byte*	data;

	ut_a(rec_get_nth_field_extern_bit(rec, no));

	/* An externally stored field can contain some initial
	data from the field, and in the last 20 bytes it has the
	space id, page number, and offset where the rest of the
	field data is stored, and the data length in addition to
	the data stored locally. We may need to store some data
	locally to get the local record length above the 128 byte
	limit so that field offsets are stored in two bytes, and
	the extern bit is available in those two bytes. */

	data = rec_get_nth_field(rec, no, &local_len);

	return(btr_copy_externally_stored_field(len, data, local_len, heap));
}
