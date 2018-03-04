/*****************************************************************************

Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file gis/gis0sea.cc
InnoDB R-tree search interfaces

Created 2014/01/16 Jimmy Yang
***********************************************************************/

#include <new>

#include "fsp0fsp.h"
#include "gis0rtree.h"
#include "my_dbug.h"
#include "page0cur.h"
#include "page0page.h"
#include "page0zip.h"

#include "btr0cur.h"
#include "btr0pcur.h"
#include "btr0sea.h"
#include "ibuf0ibuf.h"
#include "lock0lock.h"
#include "rem0cmp.h"
#include "srv0mon.h"
#include "trx0trx.h"
#include "sync0sync.h"

/** Restore the stored position of a persistent cursor bufferfixing the page */
static
bool
rtr_cur_restore_position(
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	cursor,		/*!< in: detached persistent cursor */
	ulint		level,		/*!< in: index level */
	mtr_t*		mtr);		/*!< in: mtr */

/*************************************************************//**
Pop out used parent path entry, until we find the parent with matching
page number */
static
void
rtr_adjust_parent_path(
/*===================*/
	rtr_info_t*	rtr_info,	/* R-Tree info struct */
	page_no_t	page_no)	/* page number to look for */
{
	while (!rtr_info->parent_path->empty()) {
		if (rtr_info->parent_path->back().child_no == page_no) {
			break;
		} else {
			if (rtr_info->parent_path->back().cursor) {
				btr_pcur_close(
					rtr_info->parent_path->back().cursor);
				ut_free(rtr_info->parent_path->back().cursor);
			}

			rtr_info->parent_path->pop_back();
		}
	}
}

/*************************************************************//**
Find the next matching record. This function is used by search
or record locating during index delete/update.
@return true if there is suitable record found, otherwise false */
static
bool
rtr_pcur_getnext_from_path(
/*=======================*/
	const dtuple_t* tuple,	/*!< in: data tuple */
	page_cur_mode_t	mode,	/*!< in: cursor search mode */
	btr_cur_t*	btr_cur,/*!< in: persistent cursor; NOTE that the
				function may release the page latch */
	ulint		target_level,
				/*!< in: target level */
	ulint		latch_mode,
				/*!< in: latch_mode */
	bool		index_locked,
				/*!< in: index tree locked */
	mtr_t*		mtr)	/*!< in: mtr */
{
	dict_index_t*	index = btr_cur->index;
	bool		found = false;
	space_id_t	space = dict_index_get_space(index);
	page_cur_t*	page_cursor;
	ulint		level = 0;
	node_visit_t	next_rec;
	rtr_info_t*	rtr_info = btr_cur->rtr_info;
	node_seq_t	page_ssn;
	ulint		my_latch_mode;
	ulint		skip_parent = false;
	bool		new_split = false;
	bool		need_parent;
	bool		for_delete = false;
	bool		for_undo_ins = false;

	/* exhausted all the pages to be searched */
	if (rtr_info->path->empty()) {
		return(false);
	}

	ut_ad(dtuple_get_n_fields_cmp(tuple));

	my_latch_mode = BTR_LATCH_MODE_WITHOUT_FLAGS(latch_mode);

	for_delete = latch_mode & BTR_RTREE_DELETE_MARK;
	for_undo_ins = latch_mode & BTR_RTREE_UNDO_INS;

	/* There should be no insert coming to this function. Only
	mode with BTR_MODIFY_* should be delete */
	ut_ad(mode != PAGE_CUR_RTREE_INSERT);
	ut_ad(my_latch_mode == BTR_SEARCH_LEAF
	      || my_latch_mode == BTR_MODIFY_LEAF
	      || my_latch_mode == BTR_MODIFY_TREE
	      || my_latch_mode == BTR_CONT_MODIFY_TREE);

	/* Whether need to track parent information. Only need so
	when we do tree altering operations (such as index page merge) */
	need_parent = ((my_latch_mode == BTR_MODIFY_TREE
		        || my_latch_mode == BTR_CONT_MODIFY_TREE)
		       && mode == PAGE_CUR_RTREE_LOCATE);

	if (!index_locked) {
		ut_ad(latch_mode & BTR_SEARCH_LEAF
		      || latch_mode & BTR_MODIFY_LEAF);
		mtr_s_lock(dict_index_get_lock(index), mtr);
	} else {
		ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
					MTR_MEMO_SX_LOCK)
		      || mtr_memo_contains(mtr, dict_index_get_lock(index),
					MTR_MEMO_S_LOCK)
		      || mtr_memo_contains(mtr, dict_index_get_lock(index),
					   MTR_MEMO_X_LOCK));
	}

	const page_size_t&	page_size = dict_table_page_size(index->table);

	/* Pop each node/page to be searched from "path" structure
	and do a search on it. Please note, any pages that are in
	the "path" structure are protected by "page" lock, so tey
	cannot be shrunk away */
	do {
		buf_block_t*	block;
		node_seq_t	path_ssn;
		const page_t*	page;
		ulint		rw_latch = RW_X_LATCH;
		ulint		tree_idx;

		mutex_enter(&rtr_info->rtr_path_mutex);
		next_rec = rtr_info->path->back();
		rtr_info->path->pop_back();
		level = next_rec.level;
		path_ssn = next_rec.seq_no;
		tree_idx = btr_cur->tree_height - level - 1;

		/* Maintain the parent path info as well, if needed */
		if (need_parent && !skip_parent && !new_split) {
			ulint		old_level;
			ulint		new_level;

			ut_ad(!rtr_info->parent_path->empty());

			/* Cleanup unused parent info */
			if (rtr_info->parent_path->back().cursor) {
				btr_pcur_close(
					rtr_info->parent_path->back().cursor);
				ut_free(rtr_info->parent_path->back().cursor);
			}

			old_level = rtr_info->parent_path->back().level;

			rtr_info->parent_path->pop_back();

			ut_ad(!rtr_info->parent_path->empty());

			/* check whether there is a level change. If so,
			the current parent path needs to pop enough
			nodes to adjust to the new search page */
			new_level = rtr_info->parent_path->back().level;

			if (old_level < new_level) {
				rtr_adjust_parent_path(
					rtr_info, next_rec.page_no);
			}

			ut_ad(!rtr_info->parent_path->empty());

			ut_ad(next_rec.page_no
			      == rtr_info->parent_path->back().child_no);
		}

		mutex_exit(&rtr_info->rtr_path_mutex);

		skip_parent = false;
		new_split = false;

		/* Once we have pages in "path", these pages are
		predicate page locked, so they can't be shrunk away.
		They also have SSN (split sequence number) to detect
		splits, so we can directly latch single page while
		getting them. They can be unlatched if not qualified.
		One reason for pre-latch is that we might need to position
		some parent position (requires latch) during search */
		if (level == 0) {
			/* S latched for SEARCH_LEAF, and X latched
			for MODIFY_LEAF */
			if (my_latch_mode <= BTR_MODIFY_LEAF) {
				rw_latch = my_latch_mode;
			}

			if (my_latch_mode == BTR_CONT_MODIFY_TREE
			    || my_latch_mode == BTR_MODIFY_TREE) {
				rw_latch = RW_NO_LATCH;
			}

		} else if (level == target_level) {
			rw_latch = RW_X_LATCH;
		}

		/* Release previous locked blocks */
		if (my_latch_mode != BTR_SEARCH_LEAF) {
			for (ulint idx = 0; idx < btr_cur->tree_height;
			     idx++) {
				if (rtr_info->tree_blocks[idx]) {
					mtr_release_block_at_savepoint(
						mtr,
						rtr_info->tree_savepoints[idx],
						rtr_info->tree_blocks[idx]);
					rtr_info->tree_blocks[idx] = NULL;
				}
			}
			for (ulint idx = RTR_MAX_LEVELS; idx < RTR_MAX_LEVELS + 3;
			     idx++) {
				if (rtr_info->tree_blocks[idx]) {
					mtr_release_block_at_savepoint(
						mtr,
						rtr_info->tree_savepoints[idx],
						rtr_info->tree_blocks[idx]);
					rtr_info->tree_blocks[idx] = NULL;
				}
			}
		}

		/* set up savepoint to record any locks to be taken */
		rtr_info->tree_savepoints[tree_idx] = mtr_set_savepoint(mtr);

#ifdef UNIV_RTR_DEBUG
		ut_ad(!(rw_lock_own(&btr_cur->page_cur.block->lock, RW_LOCK_X)
			||
			rw_lock_own(&btr_cur->page_cur.block->lock, RW_LOCK_S))
			|| my_latch_mode == BTR_MODIFY_TREE
			|| my_latch_mode == BTR_CONT_MODIFY_TREE
			|| !page_is_leaf(buf_block_get_frame(
					btr_cur->page_cur.block)));
#endif /* UNIV_RTR_DEBUG */

		page_id_t	page_id(space, next_rec.page_no);

		block = buf_page_get_gen(
			page_id, page_size,
			rw_latch, NULL, BUF_GET, __FILE__, __LINE__, mtr);

		if (block == NULL) {
			continue;
		} else if (rw_latch != RW_NO_LATCH) {
			ut_ad(!dict_index_is_ibuf(index));
			buf_block_dbg_add_level(block, SYNC_TREE_NODE);
		}

		rtr_info->tree_blocks[tree_idx] = block;

		page = buf_block_get_frame(block);
		page_ssn = page_get_ssn_id(page);

		/* If there are splits, push the splitted page.
		Note that we have SX lock on index->lock, there
		should not be any split/shrink happening here */
		if (page_ssn > path_ssn) {
			page_no_t next_page_no = btr_page_get_next(page, mtr);
			rtr_non_leaf_stack_push(
				rtr_info->path, next_page_no, path_ssn,
				level, 0, NULL, 0);

			if (!srv_read_only_mode
			    && mode != PAGE_CUR_RTREE_INSERT
			    && mode != PAGE_CUR_RTREE_LOCATE) {
				ut_ad(rtr_info->thr);
				lock_place_prdt_page_lock(
					space, next_page_no, index,
					rtr_info->thr);
			}
			new_split = true;
#ifdef UNIV_GIS_DEBUG
			fprintf(stderr,
				"GIS_DIAG: Splitted page found: %d, %ld\n",
				static_cast<int>(need_parent), next_page_no);
#endif
		}

		page_cursor = btr_cur_get_page_cur(btr_cur);
		page_cursor->rec = NULL;

		if (mode == PAGE_CUR_RTREE_LOCATE) {
			if (level == target_level && level == 0) {
				ulint	low_match;

				found = false;

				low_match = page_cur_search(
					block, index, tuple,
					PAGE_CUR_LE,
					btr_cur_get_page_cur(btr_cur));

				if (low_match == dtuple_get_n_fields_cmp(
							tuple)) {
					rec_t*	rec = btr_cur_get_rec(btr_cur);

					if (!rec_get_deleted_flag(rec,
					    dict_table_is_comp(index->table))
					    || (!for_delete && !for_undo_ins)) {
						found = true;
						btr_cur->low_match = low_match;
					} else {
						/* mark we found deleted row */
						btr_cur->rtr_info->fd_del
							= true;
					}
				}
			} else {
				page_cur_mode_t	page_mode = mode;

				if (level == target_level
				    && target_level != 0) {
					page_mode = PAGE_CUR_RTREE_GET_FATHER;
				}
				found = rtr_cur_search_with_match(
					block, index, tuple, page_mode,
					page_cursor, btr_cur->rtr_info);

				/* Save the position of parent if needed */
				if (found && need_parent) {
					btr_pcur_t*     r_cursor =
						rtr_get_parent_cursor(
							btr_cur, level, false);

					rec_t*	rec = page_cur_get_rec(
						page_cursor);
					page_cur_position(
						rec, block,
						btr_pcur_get_page_cur(r_cursor));
					r_cursor->pos_state =
						 BTR_PCUR_IS_POSITIONED;
					r_cursor->latch_mode = my_latch_mode;
					btr_pcur_store_position(r_cursor, mtr);
#ifdef UNIV_DEBUG
					ulint num_stored =
						rtr_store_parent_path(
							block, btr_cur,
							rw_latch, level, mtr);
					ut_ad(num_stored > 0);
#else
					rtr_store_parent_path(
						block, btr_cur, rw_latch,
						level, mtr);
#endif /* UNIV_DEBUG */
				}
			}
		} else {
			found = rtr_cur_search_with_match(
				block, index, tuple, mode, page_cursor,
				btr_cur->rtr_info);
		}

		/* Attach predicate lock if needed, no matter whether
		there are matched records */
		if (mode != PAGE_CUR_RTREE_INSERT
		    && mode != PAGE_CUR_RTREE_LOCATE
		    && mode >= PAGE_CUR_CONTAIN
		    && btr_cur->rtr_info->need_prdt_lock
		    && found) {
			lock_prdt_t	prdt;

			trx_t*		trx = thr_get_trx(
						btr_cur->rtr_info->thr);
			lock_mutex_enter();
			lock_init_prdt_from_mbr(
				&prdt, &btr_cur->rtr_info->mbr,
				mode, trx->lock.lock_heap);
			lock_mutex_exit();

			if (rw_latch == RW_NO_LATCH) {
				rw_lock_s_lock(&(block->lock));
			}

			lock_prdt_lock(block, &prdt, index, LOCK_S,
				       LOCK_PREDICATE, btr_cur->rtr_info->thr,
				       mtr);

			if (rw_latch == RW_NO_LATCH) {
				rw_lock_s_unlock(&(block->lock));
			}
		}

		if (found) {
			if (level == target_level) {
				page_cur_t*	r_cur;;

				if (my_latch_mode == BTR_MODIFY_TREE
				    && level == 0) {
					ut_ad(rw_latch == RW_NO_LATCH);
					page_id_t	my_page_id(
						space, block->page.id.page_no());

					btr_cur_latch_leaves(
						block, my_page_id,
						page_size, BTR_MODIFY_TREE,
						btr_cur, mtr);
				}

				r_cur = btr_cur_get_page_cur(btr_cur);

				page_cur_position(
					page_cur_get_rec(page_cursor),
					page_cur_get_block(page_cursor),
					r_cur);

				btr_cur->low_match = level != 0 ?
					DICT_INDEX_SPATIAL_NODEPTR_SIZE + 1
					: btr_cur->low_match;
				break;
			}

			/* Keep the parent path node, which points to
			last node just located */
			skip_parent = true;
		} else {
			/* Release latch on the current page */
			ut_ad(rtr_info->tree_blocks[tree_idx]);

			mtr_release_block_at_savepoint(
				mtr, rtr_info->tree_savepoints[tree_idx],
				rtr_info->tree_blocks[tree_idx]);
			rtr_info->tree_blocks[tree_idx] = NULL;
		}

	} while (!rtr_info->path->empty());

	const rec_t* rec = btr_cur_get_rec(btr_cur);

	if (page_rec_is_infimum(rec) || page_rec_is_supremum(rec)) {
		mtr_commit(mtr);
		mtr_start(mtr);
	} else if (!index_locked) {
		mtr_memo_release(mtr, dict_index_get_lock(index),
				 MTR_MEMO_X_LOCK);
	}

	return(found);
}

/** Find the next matching record. This function will first exhaust
the copied record listed in the rtr_info->matches vector before
moving to next page
@param[in]	tuple		data tuple; NOTE: n_fields_cmp in tuple
				must be set so that it cannot get compared
				to the node ptr page number field!
@param[in]	mode		cursor search mode
@param[in]	sel_mode	select mode: SELECT_ORDINARY,
				SELECT_SKIP_LOKCED, or SELECT_NO_WAIT
@param[in]	cursor		persistent cursor; NOTE that the function
				may release the page latch
@param[in]	cur_level	current level
@param[in]	mtr		mini-transaction
@return true if there is next qualified record found, otherwise(if
exhausted) false */
bool
rtr_pcur_move_to_next(
	const dtuple_t*	tuple,
	page_cur_mode_t	mode,
	select_mode	sel_mode,
	btr_pcur_t*	cursor,
	ulint		cur_level,
	mtr_t*		mtr)
{
	rtr_info_t*	rtr_info = cursor->btr_cur.rtr_info;

	ut_a(cursor->pos_state == BTR_PCUR_IS_POSITIONED);

	mutex_enter(&rtr_info->matches->rtr_match_mutex);
	/* First retrieve the next record on the current page */
	while (!rtr_info->matches->matched_recs->empty()) {
		rtr_rec_t	rec;
		rec = rtr_info->matches->matched_recs->back();
		rtr_info->matches->matched_recs->pop_back();

		/* Skip unlocked record
		Note: CHECK TABLE doesn't hold record locks. */
		if (sel_mode != SELECT_ORDINARY && !rec.locked) {
			continue;
		}

		mutex_exit(&rtr_info->matches->rtr_match_mutex);

		cursor->btr_cur.page_cur.rec = rec.r_rec;
		cursor->btr_cur.page_cur.block = &rtr_info->matches->block;

		DEBUG_SYNC_C("rtr_pcur_move_to_next_return");
		return(true);
	}

	mutex_exit(&rtr_info->matches->rtr_match_mutex);

	/* Fetch the next page */
	return(rtr_pcur_getnext_from_path(tuple, mode, &cursor->btr_cur,
					 cur_level, cursor->latch_mode,
					 false, mtr));
}

/*************************************************************//**
Check if the cursor holds record pointing to the specified child page
@return	true if it is (pointing to the child page) false otherwise */
static
bool
rtr_compare_cursor_rec(
/*===================*/
	dict_index_t*	index,		/*!< in: index */
	btr_cur_t*	cursor,		/*!< in: Cursor to check */
	page_no_t	page_no,	/*!< in: desired child page number */
	mem_heap_t**	heap)		/*!< in: memory heap */
{
	const rec_t*	rec;
	ulint*		offsets;

	rec = btr_cur_get_rec(cursor);

	offsets = rec_get_offsets(
		rec, index, NULL, ULINT_UNDEFINED, heap);

	return(btr_node_ptr_get_child_page_no(rec, offsets) == page_no);
}

/**************************************************************//**
Initializes and opens a persistent cursor to an index tree. It should be
closed with btr_pcur_close. Mainly called by row_search_index_entry() */
void
rtr_pcur_open_low(
/*==============*/
	dict_index_t*	index,	/*!< in: index */
	ulint		level,	/*!< in: level in the rtree */
	const dtuple_t*	tuple,	/*!< in: tuple on which search done */
	page_cur_mode_t	mode,	/*!< in: PAGE_CUR_RTREE_LOCATE, ... */
	ulint		latch_mode,/*!< in: BTR_SEARCH_LEAF, ... */
	btr_pcur_t*	cursor, /*!< in: memory buffer for persistent cursor */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mtr */
{
	btr_cur_t*	btr_cursor;
	ulint		n_fields;
	ulint		low_match;
	rec_t*		rec;
	bool		tree_latched = false;
	bool		for_delete = false;
	bool		for_undo_ins = false;

	ut_ad(level == 0);

	ut_ad(latch_mode & BTR_MODIFY_LEAF || latch_mode & BTR_MODIFY_TREE);
	ut_ad(mode == PAGE_CUR_RTREE_LOCATE);

	/* Initialize the cursor */

	btr_pcur_init(cursor);

	for_delete = latch_mode & BTR_RTREE_DELETE_MARK;
	for_undo_ins = latch_mode & BTR_RTREE_UNDO_INS;

	cursor->latch_mode = BTR_LATCH_MODE_WITHOUT_FLAGS(latch_mode);
	cursor->search_mode = mode;

	/* Search with the tree cursor */

	btr_cursor = btr_pcur_get_btr_cur(cursor);

	btr_cursor->rtr_info = rtr_create_rtr_info(false, false,
						   btr_cursor, index);

	/* Purge will SX lock the tree instead of take Page Locks */
	if (btr_cursor->thr) {
		btr_cursor->rtr_info->need_page_lock = true;
		btr_cursor->rtr_info->thr = btr_cursor->thr;
	}

	btr_cur_search_to_nth_level(index, level, tuple, mode, latch_mode,
				    btr_cursor, 0, file, line, mtr);
	cursor->pos_state = BTR_PCUR_IS_POSITIONED;

	cursor->trx_if_known = NULL;

	low_match = btr_pcur_get_low_match(cursor);

	rec = btr_pcur_get_rec(cursor);

	n_fields = dtuple_get_n_fields(tuple);

	if (latch_mode & BTR_ALREADY_S_LATCHED) {
		ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
				  MTR_MEMO_S_LOCK));
		tree_latched = true;
	}

	if (latch_mode & BTR_MODIFY_TREE) {
		ut_ad(mtr_memo_contains(mtr, dict_index_get_lock(index),
					MTR_MEMO_X_LOCK)
		      || mtr_memo_contains(mtr, dict_index_get_lock(index),
					   MTR_MEMO_SX_LOCK));
		tree_latched = true;
	}

	if (page_rec_is_infimum(rec) || low_match != n_fields
	    || (rec_get_deleted_flag(rec, dict_table_is_comp(index->table))
		&& (for_delete || for_undo_ins))) {

		if (rec_get_deleted_flag(rec, dict_table_is_comp(index->table))
		    && for_delete) {
			btr_cursor->rtr_info->fd_del = true;
			btr_cursor->low_match = 0;
		}
		/* Did not find matched row in first dive. Release
		latched block if any before search more pages */
		if (latch_mode & BTR_MODIFY_LEAF) {
			ulint		tree_idx = btr_cursor->tree_height - 1;
			rtr_info_t*	rtr_info = btr_cursor->rtr_info;

			ut_ad(level == 0);

			if (rtr_info->tree_blocks[tree_idx]) {
				mtr_release_block_at_savepoint(
					mtr,
					rtr_info->tree_savepoints[tree_idx],
					rtr_info->tree_blocks[tree_idx]);
				rtr_info->tree_blocks[tree_idx] = NULL;
			}
		}

		bool	ret = rtr_pcur_getnext_from_path(
			tuple, mode, btr_cursor, level, latch_mode,
			tree_latched, mtr);

		if (ret) {
			low_match = btr_pcur_get_low_match(cursor);
			ut_ad(low_match == n_fields);
		}
	}
}

/** Returns the upper level node pointer to a R-Tree page. It is assumed
that mtr holds an SX-latch or X-latch on the tree.
@return	rec_get_offsets() of the node pointer record */
static
ulint*
rtr_page_get_father_node_ptr(
	ulint*		offsets,/*!< in: work area for the return value */
	mem_heap_t*	heap,	/*!< in: memory heap to use */
	btr_cur_t*	sea_cur,/*!< in: search cursor */
	btr_cur_t*	cursor,	/*!< in: cursor pointing to user record,
				out: cursor on node pointer record,
				its page x-latched */
	mtr_t*		mtr)	/*!< in: mtr */
{
	dtuple_t*	tuple;
	rec_t*		user_rec;
	rec_t*		node_ptr;
	ulint		level;
	page_no_t	page_no;
	dict_index_t*	index;
	rtr_mbr_t	mbr;

	page_no = btr_cur_get_block(cursor)->page.id.page_no();
	index = btr_cur_get_index(cursor);

	ut_ad(srv_read_only_mode
	      || mtr_memo_contains_flagged(mtr, dict_index_get_lock(index),
					   MTR_MEMO_X_LOCK | MTR_MEMO_SX_LOCK));

	ut_ad(dict_index_get_page(index) != page_no);

	level = btr_page_get_level(btr_cur_get_page(cursor), mtr);

	user_rec = btr_cur_get_rec(cursor);
	ut_a(page_rec_is_user_rec(user_rec));

	offsets = rec_get_offsets(user_rec, index, offsets,
				  ULINT_UNDEFINED, &heap);
	rtr_get_mbr_from_rec(user_rec, offsets, &mbr);

	tuple = rtr_index_build_node_ptr(
			index, &mbr, user_rec, page_no, heap, level);

	if (sea_cur && !sea_cur->rtr_info) {
		sea_cur = NULL;
	}

	rtr_get_father_node(index, level + 1, tuple, sea_cur, cursor,
			    page_no, mtr);

	node_ptr = btr_cur_get_rec(cursor);
	ut_ad(!page_rec_is_comp(node_ptr)
	      || rec_get_status(node_ptr) == REC_STATUS_NODE_PTR);
	offsets = rec_get_offsets(node_ptr, index, offsets,
				  ULINT_UNDEFINED, &heap);

	page_no_t	child_page = btr_node_ptr_get_child_page_no(node_ptr, offsets);

	if (child_page != page_no) {
		const rec_t*	print_rec;

		ib::fatal	error;

		error << "Corruption of index " << index->name
			<< " of table " << index->table->name
			<< " parent page " << page_no
			<< " child page " << child_page;

		print_rec = page_rec_get_next(
			page_get_infimum_rec(page_align(user_rec)));
		offsets = rec_get_offsets(print_rec, index,
					  offsets, ULINT_UNDEFINED, &heap);
		error << "; child ";
		rec_print(error.m_oss, print_rec,
			  rec_get_info_bits(print_rec, rec_offs_comp(offsets)),
			  offsets);
		offsets = rec_get_offsets(node_ptr, index, offsets,
					  ULINT_UNDEFINED, &heap);
		error << "; parent ";
		rec_print(error.m_oss, print_rec,
			  rec_get_info_bits(print_rec, rec_offs_comp(offsets)),
			  offsets);

		error << ". You should dump + drop + reimport the table to"
			" fix the corruption. If the crash happens at"
			" database startup, see " REFMAN
			"forcing-innodb-recovery.html about forcing"
			" recovery. Then dump + drop + reimport.";
	}

	return(offsets);
}

/* Get the rtree page father.
@param[in]	index		rtree index
@param[in]	block		child page in the index
@param[in]	mtr		mtr
@param[in]	sea_cur		search cursor, contains information
				about parent nodes in search
@param[in]	cursor		cursor on node pointer record,
				its page x-latched */
void
rtr_page_get_father(
	dict_index_t*	index,
	buf_block_t*	block,
	mtr_t*		mtr,
	btr_cur_t*	sea_cur,
	btr_cur_t*	cursor)
{
	mem_heap_t*	heap = mem_heap_create(100);
#ifdef UNIV_DEBUG
	ulint*		offsets;

	offsets = rtr_page_get_father_block(
		NULL, heap, index, block, mtr, sea_cur, cursor);

	ulint	page_no = btr_node_ptr_get_child_page_no(cursor->page_cur.rec,
							 offsets);

	ut_ad(page_no == block->page.id.page_no());
#else
	rtr_page_get_father_block(
		NULL, heap, index, block, mtr, sea_cur, cursor);
#endif

	mem_heap_free(heap);
}

/************************************************************//**
Returns the father block to a page. It is assumed that mtr holds
an X or SX latch on the tree.
@return rec_get_offsets() of the node pointer record */
ulint*
rtr_page_get_father_block(
/*======================*/
	ulint*		offsets,/*!< in: work area for the return value */
	mem_heap_t*	heap,	/*!< in: memory heap to use */
	dict_index_t*	index,	/*!< in: b-tree index */
	buf_block_t*	block,	/*!< in: child page in the index */
	mtr_t*		mtr,	/*!< in: mtr */
	btr_cur_t*	sea_cur,/*!< in: search cursor, contains information
				about parent nodes in search */
	btr_cur_t*	cursor)	/*!< out: cursor on node pointer record,
				its page x-latched */
{

	rec_t*  rec = page_rec_get_next(
		page_get_infimum_rec(buf_block_get_frame(block)));
	btr_cur_position(index, rec, block, cursor);

	return(rtr_page_get_father_node_ptr(offsets, heap, sea_cur,
					    cursor, mtr));
}

/********************************************************************//**
Returns the upper level node pointer to a R-Tree page. It is assumed
that mtr holds an x-latch on the tree. */
void
rtr_get_father_node(
/*================*/
	dict_index_t*	index,	/*!< in: index */
	ulint		level,	/*!< in: the tree level of search */
	const dtuple_t*	tuple,	/*!< in: data tuple; NOTE: n_fields_cmp in
				tuple must be set so that it cannot get
				compared to the node ptr page number field! */
	btr_cur_t*	sea_cur,/*!< in: search cursor */
	btr_cur_t*	btr_cur,/*!< in/out: tree cursor; the cursor page is
				s- or x-latched, but see also above! */
	page_no_t	page_no,/*!< Current page no */
	mtr_t*		mtr)	/*!< in: mtr */
{
	mem_heap_t*	heap = NULL;
	bool		ret = false;
	const rec_t*	rec;
	ulint		n_fields;
	bool		new_rtr = false;

	/* Try to optimally locate the parent node. Level should always
	less than sea_cur->tree_height unless the root is splitting */
	if (sea_cur && sea_cur->tree_height > level) {

		ut_ad(mtr_memo_contains_flagged(mtr,
						dict_index_get_lock(index),
						MTR_MEMO_X_LOCK
						| MTR_MEMO_SX_LOCK));
		ret = rtr_cur_restore_position(
			BTR_CONT_MODIFY_TREE, sea_cur, level, mtr);

		/* Once we block shrink tree nodes while there are
		active search on it, this optimal locating should always
		succeeds */
		ut_ad(ret);

		if (ret) {
			btr_pcur_t*	r_cursor = rtr_get_parent_cursor(
						sea_cur, level, false);

			rec = btr_pcur_get_rec(r_cursor);

			ut_ad(r_cursor->rel_pos == BTR_PCUR_ON);
			page_cur_position(rec,
					  btr_pcur_get_block(r_cursor),
					  btr_cur_get_page_cur(btr_cur));
			btr_cur->rtr_info = sea_cur->rtr_info;
			btr_cur->tree_height = sea_cur->tree_height;
			ut_ad(rtr_compare_cursor_rec(
				index, btr_cur, page_no, &heap));
			goto func_exit;
		}
	}

	/* We arrive here in one of two scenario
	1) check table and btr_valide
	2) index root page being raised */
	ut_ad(!sea_cur || sea_cur->tree_height == level);

	if (btr_cur->rtr_info) {
		rtr_clean_rtr_info(btr_cur->rtr_info, true);
	} else {
		new_rtr = true;
	}

	btr_cur->rtr_info = rtr_create_rtr_info(false, false, btr_cur, index);

	if (sea_cur && sea_cur->tree_height == level) {
		/* root split, and search the new root */
		btr_cur_search_to_nth_level(
			index, level, tuple, PAGE_CUR_RTREE_LOCATE,
			BTR_CONT_MODIFY_TREE, btr_cur, 0,
			__FILE__, __LINE__, mtr);

	} else {
		/* btr_validate */
		ut_ad(level >= 1);
		ut_ad(!sea_cur);

		btr_cur_search_to_nth_level(
			index, level, tuple, PAGE_CUR_RTREE_LOCATE,
			BTR_CONT_MODIFY_TREE, btr_cur, 0,
			__FILE__, __LINE__, mtr);

		rec = btr_cur_get_rec(btr_cur);
		n_fields = dtuple_get_n_fields_cmp(tuple);

		if (page_rec_is_infimum(rec)
		    || (btr_cur->low_match != n_fields)) {
			ret = rtr_pcur_getnext_from_path(
				tuple, PAGE_CUR_RTREE_LOCATE, btr_cur,
				level, BTR_CONT_MODIFY_TREE,
				true, mtr);

			ut_ad(ret && btr_cur->low_match == n_fields);
		}
	}

	ret = rtr_compare_cursor_rec(
		index, btr_cur, page_no, &heap);

	ut_ad(ret);

func_exit:
	if (heap) {
		mem_heap_free(heap);
	}

	if (new_rtr && btr_cur->rtr_info) {
		rtr_clean_rtr_info(btr_cur->rtr_info, true);
		btr_cur->rtr_info = NULL;
	}
}

/*******************************************************************//**
Create a RTree search info structure */
rtr_info_t*
rtr_create_rtr_info(
/******************/
	bool		need_prdt,	/*!< in: Whether predicate lock
					is needed */
	bool		init_matches,	/*!< in: Whether to initiate the
					"matches" structure for collecting
					matched leaf records */
	btr_cur_t*	cursor,		/*!< in: tree search cursor */
	dict_index_t*	index)		/*!< in: index struct */
{
	rtr_info_t*	rtr_info;

	index = index ? index : cursor->index;
	ut_ad(index);

	rtr_info = static_cast<rtr_info_t*>(ut_zalloc_nokey(sizeof(*rtr_info)));

	rtr_info->allocated = true;
	rtr_info->cursor = cursor;
	rtr_info->index = index;

	if (init_matches) {
		rtr_info->heap = mem_heap_create(sizeof(*(rtr_info->matches)));
		rtr_info->matches = static_cast<matched_rec_t*>(
					mem_heap_zalloc(
						rtr_info->heap,
						sizeof(*rtr_info->matches)));

		rtr_info->matches->matched_recs
			= UT_NEW_NOKEY(rtr_rec_vector());

		rtr_info->matches->bufp = page_align(rtr_info->matches->rec_buf
						     + UNIV_PAGE_SIZE_MAX + 1);
		mutex_create(LATCH_ID_RTR_MATCH_MUTEX,
			     &rtr_info->matches->rtr_match_mutex);
		rw_lock_create(PFS_NOT_INSTRUMENTED,
			       &(rtr_info->matches->block.lock),
			      SYNC_LEVEL_VARYING);
	}

	rtr_info->path = UT_NEW_NOKEY(rtr_node_path_t());
	rtr_info->parent_path = UT_NEW_NOKEY(rtr_node_path_t());
	rtr_info->need_prdt_lock = need_prdt;
	mutex_create(LATCH_ID_RTR_PATH_MUTEX,
		     &rtr_info->rtr_path_mutex);

	mutex_enter(&index->rtr_track->rtr_active_mutex);
	index->rtr_track->rtr_active->push_back(rtr_info);
	mutex_exit(&index->rtr_track->rtr_active_mutex);
	return(rtr_info);
}

/*******************************************************************//**
Update a btr_cur_t with rtr_info */
void
rtr_info_update_btr(
/******************/
	btr_cur_t*	cursor,		/*!< in/out: tree cursor */
	rtr_info_t*	rtr_info)	/*!< in: rtr_info to set to the
					cursor */
{
	ut_ad(rtr_info);

	cursor->rtr_info = rtr_info;
}

/*******************************************************************//**
Initialize a R-Tree Search structure */
void
rtr_init_rtr_info(
/****************/
	rtr_info_t*	rtr_info,	/*!< in: rtr_info to set to the
					cursor */
	bool		need_prdt,	/*!< in: Whether predicate lock is
					needed */
	btr_cur_t*	cursor,		/*!< in: tree search cursor */
	dict_index_t*	index,		/*!< in: index structure */
	bool		reinit)		/*!< in: Whether this is a reinit */
{
	ut_ad(rtr_info);

	if (!reinit) {
		/* Reset all members. */
		rtr_info->path = NULL;
		rtr_info->parent_path = NULL;
		rtr_info->matches = NULL;

		mutex_create(LATCH_ID_RTR_PATH_MUTEX,
			     &rtr_info->rtr_path_mutex);

		memset(rtr_info->tree_blocks, 0x0,
		       sizeof(rtr_info->tree_blocks));
		memset(rtr_info->tree_savepoints, 0x0,
		       sizeof(rtr_info->tree_savepoints));
		rtr_info->mbr.xmin = 0.0;
		rtr_info->mbr.xmax = 0.0;
		rtr_info->mbr.ymin = 0.0;
		rtr_info->mbr.ymax = 0.0;
		rtr_info->thr = NULL;
		rtr_info->heap = NULL;
		rtr_info->cursor = NULL;
		rtr_info->index = NULL;
		rtr_info->need_prdt_lock = false;
		rtr_info->need_page_lock = false;
		rtr_info->allocated = false;
		rtr_info->mbr_adj = false;
		rtr_info->fd_del = false;
		rtr_info->search_tuple = NULL;
		rtr_info->search_mode = PAGE_CUR_UNSUPP;
	}

	ut_ad(!rtr_info->matches || rtr_info->matches->matched_recs->empty());

	rtr_info->path = UT_NEW_NOKEY(rtr_node_path_t());
	rtr_info->parent_path = UT_NEW_NOKEY(rtr_node_path_t());
	rtr_info->need_prdt_lock = need_prdt;
	rtr_info->cursor = cursor;
	rtr_info->index = index;

	mutex_enter(&index->rtr_track->rtr_active_mutex);
	index->rtr_track->rtr_active->push_back(rtr_info);
	mutex_exit(&index->rtr_track->rtr_active_mutex);
}

/**************************************************************//**
Clean up R-Tree search structure */
void
rtr_clean_rtr_info(
/*===============*/
	rtr_info_t*	rtr_info,	/*!< in: RTree search info */
	bool		free_all)	/*!< in: need to free rtr_info itself */
{
	dict_index_t*	index;
	bool		initialized = false;

	if (!rtr_info) {
		return;
	}

	index = rtr_info->index;

	if (index) {
		mutex_enter(&index->rtr_track->rtr_active_mutex);
	}

	while (rtr_info->parent_path && !rtr_info->parent_path->empty()) {
		btr_pcur_t*	cur = rtr_info->parent_path->back().cursor;
		rtr_info->parent_path->pop_back();

		if (cur) {
			btr_pcur_close(cur);
			ut_free(cur);
		}
	}

	UT_DELETE(rtr_info->parent_path);
	rtr_info->parent_path = NULL;

	if (rtr_info->path != NULL) {
		UT_DELETE(rtr_info->path);
		rtr_info->path = NULL;
		initialized = true;
	}

	if (rtr_info->matches) {
		rtr_info->matches->used = false;
		rtr_info->matches->locked = false;
		rtr_info->matches->valid = false;
		rtr_info->matches->matched_recs->clear();
	}

	if (index) {
		index->rtr_track->rtr_active->remove(rtr_info);
		mutex_exit(&index->rtr_track->rtr_active_mutex);
	}

	if (free_all) {
		if (rtr_info->matches) {
			if (rtr_info->matches->matched_recs != NULL) {
				UT_DELETE(rtr_info->matches->matched_recs);
			}

			rw_lock_free(&(rtr_info->matches->block.lock));

			mutex_destroy(&rtr_info->matches->rtr_match_mutex);
		}

		if (rtr_info->heap) {
			mem_heap_free(rtr_info->heap);
		}

		if (initialized) {
			mutex_destroy(&rtr_info->rtr_path_mutex);
		}

		if (rtr_info->allocated) {
			ut_free(rtr_info);
		}
	}
}

/**************************************************************//**
Rebuilt the "path" to exclude the removing page no */
static
void
rtr_rebuild_path(
/*=============*/
	rtr_info_t*	rtr_info,	/*!< in: RTree search info */
	page_no_t	page_no)	/*!< in: need to free rtr_info itself */
{
	rtr_node_path_t*		new_path
		= UT_NEW_NOKEY(rtr_node_path_t());

	rtr_node_path_t::iterator	rit;
#ifdef UNIV_DEBUG
	ulint	before_size = rtr_info->path->size();
#endif /* UNIV_DEBUG */

	for (rit = rtr_info->path->begin();
	     rit != rtr_info->path->end(); ++rit) {
		node_visit_t	next_rec = *rit;

		if (next_rec.page_no == page_no) {
			continue;
		}

		new_path->push_back(next_rec);
#ifdef UNIV_DEBUG
		node_visit_t	rec = new_path->back();
		ut_ad(rec.level < rtr_info->cursor->tree_height
		      && rec.page_no > 0);
#endif /* UNIV_DEBUG */
	}

	UT_DELETE(rtr_info->path);

	ut_ad(new_path->size() == before_size - 1);

	rtr_info->path = new_path;

	if (!rtr_info->parent_path->empty()) {
		rtr_node_path_t*	new_parent_path = UT_NEW_NOKEY(
			rtr_node_path_t());

		for (rit = rtr_info->parent_path->begin();
		     rit != rtr_info->parent_path->end(); ++rit) {
			node_visit_t	next_rec = *rit;

			if (next_rec.child_no == page_no) {
				btr_pcur_t*	cur = next_rec.cursor;

				if (cur) {
					btr_pcur_close(cur);
					ut_free(cur);
				}

				continue;
			}

			new_parent_path->push_back(next_rec);
		}
		UT_DELETE(rtr_info->parent_path);
		rtr_info->parent_path = new_parent_path;
	}

}

/**************************************************************//**
Check whether a discarding page is in anyone's search path */
void
rtr_check_discard_page(
/*===================*/
	dict_index_t*	index,	/*!< in: index */
	btr_cur_t*	cursor, /*!< in: cursor on the page to discard: not on
				the root page */
	buf_block_t*	block)	/*!< in: block of page to be discarded */
{
	page_no_t		pageno = block->page.id.page_no();
	rtr_info_t*		rtr_info;
	rtr_info_active::iterator	it;

	mutex_enter(&index->rtr_track->rtr_active_mutex);

	for (it = index->rtr_track->rtr_active->begin();
	     it != index->rtr_track->rtr_active->end(); ++it) {
		rtr_info = *it;
		rtr_node_path_t::iterator	rit;
		bool	found = false;

		if (cursor && rtr_info == cursor->rtr_info) {
			continue;
		}

		mutex_enter(&rtr_info->rtr_path_mutex);
		for (rit = rtr_info->path->begin();
		     rit != rtr_info->path->end(); ++rit) {
			node_visit_t	node = *rit;

			if (node.page_no == pageno) {
				found = true;
				break;
			}
		}

		if (found) {
			rtr_rebuild_path(rtr_info, pageno);
		}
		mutex_exit(&rtr_info->rtr_path_mutex);

		if (rtr_info->matches) {
			mutex_enter(&rtr_info->matches->rtr_match_mutex);

			if ((&rtr_info->matches->block)->page.id.page_no()
			     == pageno) {
				if (!rtr_info->matches->matched_recs->empty()) {
					rtr_info->matches->matched_recs->clear();
				}
				ut_ad(rtr_info->matches->matched_recs->empty());
				rtr_info->matches->valid = false;
			}

			mutex_exit(&rtr_info->matches->rtr_match_mutex);
		}
	}

	mutex_exit(&index->rtr_track->rtr_active_mutex);

	lock_mutex_enter();
	lock_prdt_page_free_from_discard(block, lock_sys->prdt_hash);
	lock_prdt_page_free_from_discard(block, lock_sys->prdt_page_hash);
	lock_mutex_exit();
}

/** Restore the stored position of a persistent cursor bufferfixing the page */
static
bool
rtr_cur_restore_position(
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF, ... */
	btr_cur_t*	btr_cur,	/*!< in: detached persistent cursor */
	ulint		level,		/*!< in: index level */
	mtr_t*		mtr)		/*!< in: mtr */
{
	dict_index_t*	index;
	mem_heap_t*	heap;
	btr_pcur_t*	r_cursor = rtr_get_parent_cursor(btr_cur, level, false);
	dtuple_t*	tuple;
	bool		ret = false;

	ut_ad(mtr);
	ut_ad(r_cursor);
	ut_ad(mtr->is_active());

	index = btr_cur_get_index(btr_cur);

	if (r_cursor->rel_pos == BTR_PCUR_AFTER_LAST_IN_TREE
	    || r_cursor->rel_pos == BTR_PCUR_BEFORE_FIRST_IN_TREE) {
		return(false);
	}

	DBUG_EXECUTE_IF(
		"rtr_pessimistic_position",
		r_cursor->modify_clock = 100;
	);

	ut_ad(latch_mode == BTR_CONT_MODIFY_TREE);

	if (!buf_pool_is_obsolete(r_cursor->withdraw_clock)
	    && buf_page_optimistic_get(RW_X_LATCH,
				       r_cursor->block_when_stored,
				       r_cursor->modify_clock,
				       __FILE__, __LINE__, mtr)) {
		ut_ad(r_cursor->pos_state == BTR_PCUR_IS_POSITIONED);

		ut_ad(r_cursor->rel_pos == BTR_PCUR_ON);
#ifdef UNIV_DEBUG
		do {
			const rec_t*	rec;
			const ulint*	offsets1;
			const ulint*	offsets2;
			ulint		comp;

			rec = btr_pcur_get_rec(r_cursor);

			heap = mem_heap_create(256);
			offsets1 = rec_get_offsets(
				r_cursor->old_rec, index, NULL,
				r_cursor->old_n_fields, &heap);
			offsets2 = rec_get_offsets(
				rec, index, NULL,
				r_cursor->old_n_fields, &heap);

			comp = rec_offs_comp(offsets1);

			if (rec_get_info_bits(r_cursor->old_rec, comp)
			    & REC_INFO_MIN_REC_FLAG) {
				ut_ad(rec_get_info_bits(rec, comp)
					& REC_INFO_MIN_REC_FLAG);
			} else {

				ut_ad(!cmp_rec_rec(r_cursor->old_rec,
						   rec, offsets1, offsets2,
						   index));
			}

			mem_heap_free(heap);
		} while (0);
#endif /* UNIV_DEBUG */

		return(true);
	}

	/* Page has changed, for R-Tree, the page cannot be shrunk away,
	so we search the page and its right siblings */
	buf_block_t*	block;
	node_seq_t	page_ssn;
	const page_t*	page;
	page_cur_t*	page_cursor;
	node_visit_t*	node = rtr_get_parent_node(btr_cur, level, false);
	space_id_t	space = dict_index_get_space(index);
	node_seq_t	path_ssn = node->seq_no;
	page_size_t	page_size = dict_table_page_size(index->table);

	page_no_t	page_no = node->page_no;

	heap = mem_heap_create(256);

	tuple = dict_index_build_data_tuple(index, r_cursor->old_rec,
					    r_cursor->old_n_fields, heap);

	page_cursor = btr_pcur_get_page_cur(r_cursor);
	ut_ad(r_cursor == node->cursor);

search_again:
	page_id_t	page_id(space, page_no);

	block = buf_page_get_gen(
		page_id, page_size, RW_X_LATCH, NULL,
		BUF_GET, __FILE__, __LINE__, mtr);

	ut_ad(block);

	/* Get the page SSN */
	page = buf_block_get_frame(block);
	page_ssn = page_get_ssn_id(page);

	ulint low_match = page_cur_search(
				block, index, tuple, PAGE_CUR_LE, page_cursor);

	if (low_match == r_cursor->old_n_fields) {
		const rec_t*	rec;
		const ulint*	offsets1;
		const ulint*	offsets2;
		ulint		comp;

		rec = btr_pcur_get_rec(r_cursor);

		offsets1 = rec_get_offsets(
			r_cursor->old_rec, index, NULL,
			r_cursor->old_n_fields, &heap);
		offsets2 = rec_get_offsets(
			rec, index, NULL,
			r_cursor->old_n_fields, &heap);

		comp = rec_offs_comp(offsets1);

		if ((rec_get_info_bits(r_cursor->old_rec, comp)
		     & REC_INFO_MIN_REC_FLAG)
		    && (rec_get_info_bits(rec, comp) & REC_INFO_MIN_REC_FLAG)) {
			r_cursor->pos_state = BTR_PCUR_IS_POSITIONED;
			ret = true;
		} else if (!cmp_rec_rec(r_cursor->old_rec, rec, offsets1, offsets2,
				 index)) {
			r_cursor->pos_state = BTR_PCUR_IS_POSITIONED;
			ret = true;
		}
	}

	/* Check the page SSN to see if it has been splitted, if so, search
	the right page */
	if (!ret && page_ssn > path_ssn) {
		page_no = btr_page_get_next(page, mtr);
		goto search_again;
	}

	mem_heap_free(heap);

	return(ret);
}

/****************************************************************//**
Copy the leaf level R-tree record, and push it to matched_rec in rtr_info */
static
void
rtr_leaf_push_match_rec(
/*====================*/
	const rec_t*	rec,		/*!< in: record to copy */
	rtr_info_t*	rtr_info,	/*!< in/out: search stack */
	ulint*		offsets,	/*!< in: offsets */
	bool		is_comp)	/*!< in: is compact format */
{
	byte*		buf;
	matched_rec_t*	match_rec = rtr_info->matches;
	rec_t*		copy;
	ulint		data_len;
	rtr_rec_t	rtr_rec;

	buf = match_rec->block.frame + match_rec->used;

	copy = rec_copy(buf, rec, offsets);

	if (is_comp) {
		rec_set_next_offs_new(copy, PAGE_NEW_SUPREMUM);
	} else {
		rec_set_next_offs_old(copy, PAGE_OLD_SUPREMUM);
	}

	rtr_rec.r_rec = copy;
	rtr_rec.locked = false;

	match_rec->matched_recs->push_back(rtr_rec);
	match_rec->valid = true;

	data_len = rec_offs_data_size(offsets) + rec_offs_extra_size(offsets);
	match_rec->used += data_len;

	ut_ad(match_rec->used < UNIV_PAGE_SIZE);
}

/**************************************************************//**
Store the parent path cursor
@return number of cursor stored */
ulint
rtr_store_parent_path(
/*==================*/
	const buf_block_t*	block,	/*!< in: block of the page */
	btr_cur_t*		btr_cur,/*!< in/out: persistent cursor */
	ulint			latch_mode,
					/*!< in: latch_mode */
	ulint			level,	/*!< in: index level */
	mtr_t*			mtr)	/*!< in: mtr */
{
	ulint	num = btr_cur->rtr_info->parent_path->size();
	ulint	num_stored = 0;

	while (num >= 1) {
		node_visit_t*	node = &(*btr_cur->rtr_info->parent_path)[
					num - 1];
		btr_pcur_t*	r_cursor = node->cursor;
		buf_block_t*	cur_block;

		if (node->level > level) {
			break;
		}

		r_cursor->pos_state = BTR_PCUR_IS_POSITIONED;
		r_cursor->latch_mode = latch_mode;

		cur_block = btr_pcur_get_block(r_cursor);

		if (cur_block == block) {
			btr_pcur_store_position(r_cursor, mtr);
			num_stored++;
		} else {
			break;
		}

		num--;
	}

	return(num_stored);
}
/**************************************************************//**
push a nonleaf index node to the search path for insertion */
static
void
rtr_non_leaf_insert_stack_push(
/*===========================*/
	dict_index_t*		index,	/*!< in: index descriptor */
	rtr_node_path_t*	path,	/*!< in/out: search path */
	ulint			level,	/*!< in: index page level */
	page_no_t		child_no,/*!< in: child page no */
	const buf_block_t*	block,	/*!< in: block of the page */
	const rec_t*		rec,	/*!< in: positioned record */
	double			mbr_inc)/*!< in: MBR needs to be enlarged */
{
	node_seq_t	new_seq;
	btr_pcur_t*	my_cursor;
	page_no_t	page_no = block->page.id.page_no();

	my_cursor = static_cast<btr_pcur_t*>(
		ut_malloc_nokey(sizeof(*my_cursor)));

	btr_pcur_init(my_cursor);

	page_cur_position(rec, block, btr_pcur_get_page_cur(my_cursor));

	(btr_pcur_get_btr_cur(my_cursor))->index = index;

	new_seq = rtr_get_current_ssn_id(index);
	rtr_non_leaf_stack_push(path, page_no, new_seq, level, child_no,
				my_cursor, mbr_inc);
}

/** Copy a buf_block_t strcuture, except "block->lock" and "block->mutex".
@param[in,out]	matches	copy to match->block
@param[in]	block	block to copy */
static
void
rtr_copy_buf(
	matched_rec_t*		matches,
	const buf_block_t*	block)
{
	/* Copy all members of "block" to "matches->block" except "mutex"
	and "lock". We skip "mutex" and "lock" because they are not used
	from the dummy buf_block_t we create here and because memcpy()ing
	them generates (valid) compiler warnings that the vtable pointer
	will be copied. It is also undefined what will happen with the
	newly memcpy()ed mutex if the source mutex was acquired by
	(another) thread while it was copied. */
	memcpy(&matches->block.page, &block->page, sizeof(buf_page_t));
	matches->block.frame = block->frame;
	matches->block.unzip_LRU = block->unzip_LRU;

	ut_d(matches->block.in_unzip_LRU_list = block->in_unzip_LRU_list);
	ut_d(matches->block.in_withdraw_list = block->in_withdraw_list);

	/* Skip buf_block_t::mutex */
	/* Skip buf_block_t::lock */
	matches->block.lock_hash_val = block->lock_hash_val;
	matches->block.modify_clock = block->modify_clock;
	matches->block.n_hash_helps = block->n_hash_helps;
	matches->block.n_fields = block->n_fields;
	matches->block.left_side = block->left_side;
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	matches->block.n_pointers = block->n_pointers;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	matches->block.curr_n_fields = block->curr_n_fields;
	matches->block.curr_left_side = block->curr_left_side;
	matches->block.index = block->index;
	matches->block.made_dirty_with_no_latch
		= block->made_dirty_with_no_latch;

	/* Use copy CTOR rather than assignment operator */
	ut_d(new (&matches->block.debug_latch) rw_lock_t(block->debug_latch));
}

/****************************************************************//**
Generate a shadow copy of the page block header to save the
matched records */
static
void
rtr_init_match(
/*===========*/
	matched_rec_t*		matches,/*!< in/out: match to initialize */
	const buf_block_t*	block,	/*!< in: buffer block */
	const page_t*		page)	/*!< in: buffer page */
{
	ut_ad(matches->matched_recs->empty());
	matches->locked = false;
	rtr_copy_buf(matches, block);
	matches->block.frame = matches->bufp;
	matches->valid = false;
	/* We have to copy PAGE_W*_SUPREMUM_END bytes so that we can
	use infimum/supremum of this page as normal btr page for search. */
	memcpy(matches->block.frame, page, page_is_comp(page)
						? PAGE_NEW_SUPREMUM_END
						: PAGE_OLD_SUPREMUM_END);
	matches->used = page_is_comp(page)
				? PAGE_NEW_SUPREMUM_END
				: PAGE_OLD_SUPREMUM_END;
#ifdef RTR_SEARCH_DIAGNOSTIC
	ulint pageno = page_get_page_no(page);
	fprintf(stderr, "INNODB_RTR: Searching leaf page %d\n",
		static_cast<int>(pageno));
#endif /* RTR_SEARCH_DIAGNOSTIC */
}

/****************************************************************//**
Get the bounding box content from an index record */
void
rtr_get_mbr_from_rec(
/*=================*/
	const rec_t*	rec,	/*!< in: data tuple */
	const ulint*	offsets,/*!< in: offsets array */
	rtr_mbr_t*	mbr)	/*!< out MBR */
{
	ulint		rec_f_len;
	const byte*	data;

	data = rec_get_nth_field(rec, offsets, 0, &rec_f_len);

	rtr_read_mbr(data, mbr);
}

/****************************************************************//**
Get the bounding box content from a MBR data record */
void
rtr_get_mbr_from_tuple(
/*===================*/
	const dtuple_t* dtuple, /*!< in: data tuple */
	rtr_mbr*	mbr)	/*!< out: mbr to fill */
{
	const dfield_t* dtuple_field;
        ulint           dtuple_f_len;
	byte*		data;

	dtuple_field = dtuple_get_nth_field(dtuple, 0);
	dtuple_f_len = dfield_get_len(dtuple_field);
	ut_a(dtuple_f_len >= 4 * sizeof(double));

	data = static_cast<byte*>(dfield_get_data(dtuple_field));

	rtr_read_mbr(data, mbr);
}

/****************************************************************//**
Searches the right position in rtree for a page cursor. */
bool
rtr_cur_search_with_match(
/*======================*/
	const buf_block_t*	block,	/*!< in: buffer block */
	dict_index_t*		index,	/*!< in: index descriptor */
	const dtuple_t*		tuple,	/*!< in: data tuple */
	page_cur_mode_t		mode,	/*!< in: PAGE_CUR_RTREE_INSERT,
					PAGE_CUR_RTREE_LOCATE etc. */
	page_cur_t*		cursor,	/*!< in/out: page cursor */
	rtr_info_t*		rtr_info)/*!< in/out: search stack */
{
	bool		found = false;
	const page_t*	page;
	const rec_t*	rec;
	const rec_t*	last_rec;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	mem_heap_t*	heap = NULL;
	int		cmp = 1;
	bool		is_leaf;
	double		least_inc = DBL_MAX;
	const rec_t*	best_rec;
	const rec_t*	last_match_rec = NULL;
	ulint		level;
	bool		match_init = false;
	space_id_t	space = block->page.id.space();
	page_cur_mode_t	orig_mode = mode;
	const rec_t*	first_rec = NULL;

	rec_offs_init(offsets_);

	ut_ad(RTREE_SEARCH_MODE(mode));

	ut_ad(dict_index_is_spatial(index));

	page = buf_block_get_frame(block);

	is_leaf = page_is_leaf(page);
	level = btr_page_get_level(page, mtr);

	if (mode == PAGE_CUR_RTREE_LOCATE) {
		ut_ad(level != 0);
		mode = PAGE_CUR_WITHIN;
	}

	rec = page_dir_slot_get_rec(page_dir_get_nth_slot(page, 0));

	last_rec = rec;
	best_rec = rec;

	if (page_rec_is_infimum(rec)) {
		rec = page_rec_get_next_const(rec);
	}

	/* Check insert tuple size is larger than first rec, and try to
	avoid it if possible */
	if (mode == PAGE_CUR_RTREE_INSERT && !page_rec_is_supremum(rec)) {

		ulint	new_rec_size = rec_get_converted_size(index, tuple, 0);

		offsets = rec_get_offsets(rec, index, offsets,
					  dtuple_get_n_fields_cmp(tuple),
					  &heap);

		if (rec_offs_size(offsets) < new_rec_size) {
			first_rec = rec;
		}

		/* If this is the left-most page of this index level
		and the table is a compressed table, try to avoid
		first page as much as possible, as there will be problem
		when update MIN_REC rec in compress table */
		if (buf_block_get_page_zip(block)
		    && mach_read_from_4(page + FIL_PAGE_PREV) == FIL_NULL
		    && page_get_n_recs(page) >= 2) {

			rec = page_rec_get_next_const(rec);
		}
	}

	while (!page_rec_is_supremum(rec)) {
		offsets = rec_get_offsets(rec, index, offsets,
					  dtuple_get_n_fields_cmp(tuple),
					  &heap);
		if (!is_leaf) {
			switch (mode) {
			case PAGE_CUR_CONTAIN:
			case PAGE_CUR_INTERSECT:
			case PAGE_CUR_MBR_EQUAL:
				/* At non-leaf level, we will need to check
				both CONTAIN and INTERSECT for either of
				the search mode */
				cmp = cmp_dtuple_rec_with_gis(
					tuple, rec, offsets, PAGE_CUR_CONTAIN,
					index->rtr_srs.get());

				if (cmp != 0) {
					cmp = cmp_dtuple_rec_with_gis(
						tuple, rec, offsets,
						PAGE_CUR_INTERSECT,
						index->rtr_srs.get());
				}
				break;
			case PAGE_CUR_DISJOINT:
				cmp = cmp_dtuple_rec_with_gis(
					tuple, rec, offsets, mode,
					index->rtr_srs.get());

				if (cmp != 0) {
					cmp = cmp_dtuple_rec_with_gis(
						tuple, rec, offsets,
						PAGE_CUR_INTERSECT,
						index->rtr_srs.get());
				}
				break;
			case PAGE_CUR_RTREE_INSERT:
				double	increase;
				double	area;

				cmp = cmp_dtuple_rec_with_gis(
					tuple, rec, offsets, PAGE_CUR_WITHIN,
					index->rtr_srs.get());

				if (cmp != 0) {
					increase = rtr_rec_cal_increase(
						tuple, rec, offsets, &area,
						index->rtr_srs.get());
					/* Once it goes beyond DBL_MAX,
					it would not make sense to record
					such value, just make it
					DBL_MAX / 2  */
					if (increase >= DBL_MAX) {
						increase = DBL_MAX / 2;
					}

					if (increase < least_inc) {
						least_inc = increase;
						best_rec = rec;
					} else if (best_rec
						   && best_rec == first_rec) {
						/* if first_rec is set,
						we will try to avoid it */
						least_inc = increase;
						best_rec = rec;
					}
				}
				break;
			case PAGE_CUR_RTREE_GET_FATHER:
				cmp = cmp_dtuple_rec_with_gis_internal(
					tuple, rec, offsets,
					index->rtr_srs.get());
				break;
			default:
				/* WITHIN etc. */
				cmp = cmp_dtuple_rec_with_gis(
					tuple, rec, offsets, mode,
					index->rtr_srs.get());
			}
		} else {
			/* At leaf level, INSERT should translate to LE */
			ut_ad(mode != PAGE_CUR_RTREE_INSERT);

			cmp = cmp_dtuple_rec_with_gis(
				tuple, rec, offsets, mode,
				index->rtr_srs.get());
		}

		if (cmp == 0) {
			found = true;

			/* If located, the matching node/rec will be pushed
			to rtr_info->path for non-leaf nodes, or
			rtr_info->matches for leaf nodes */
			if (rtr_info && mode != PAGE_CUR_RTREE_INSERT) {
				if (!is_leaf) {
					page_no_t	page_no;
					node_seq_t	new_seq;
					bool		is_loc;

					is_loc = (orig_mode
						  == PAGE_CUR_RTREE_LOCATE
						  || orig_mode
						  == PAGE_CUR_RTREE_GET_FATHER);

					offsets = rec_get_offsets(
						rec, index, offsets,
						ULINT_UNDEFINED, &heap);

					page_no = btr_node_ptr_get_child_page_no(
						rec, offsets);

					ut_ad(level >= 1);

					/* Get current SSN, before we insert
					it into the path stack */
					new_seq = rtr_get_current_ssn_id(index);

					rtr_non_leaf_stack_push(
						rtr_info->path,
						page_no,
						new_seq, level - 1, 0,
						NULL, 0);

					if (is_loc) {
						rtr_non_leaf_insert_stack_push(
							index,
							rtr_info->parent_path,
							level, page_no, block,
							rec, 0);
					}

					if (!srv_read_only_mode
					    && (rtr_info->need_page_lock
						|| !is_loc)) {

						/* Lock the page, preventing it
						from being shrunk */
						lock_place_prdt_page_lock(
							space, page_no, index,
							rtr_info->thr);
					}
				} else {
					ut_ad(orig_mode
					      != PAGE_CUR_RTREE_LOCATE);

					if (!match_init) {
						rtr_init_match(
							rtr_info->matches,
							block, page);
						match_init = true;
					}

					/* Collect matched records on page */
					offsets = rec_get_offsets(
						rec, index, offsets,
						ULINT_UNDEFINED, &heap);
					rtr_leaf_push_match_rec(
						rec, rtr_info, offsets,
						page_is_comp(page));
				}

				last_match_rec = rec;
			} else {
				/* This is the insertion case, it will break
				once it finds the first MBR that can accomodate
				the inserting rec */
				break;
			}
		}

		last_rec = rec;

		rec = page_rec_get_next_const(rec);
	}

	/* All records on page are searched */
	if (page_rec_is_supremum(rec)) {
		if (!is_leaf) {
			if (!found) {
				/* No match case, if it is for insertion,
				then we select the record that result in
				least increased area */
				if (mode == PAGE_CUR_RTREE_INSERT) {
					page_no_t child_no;
					ut_ad(least_inc < DBL_MAX);
					offsets = rec_get_offsets(
						best_rec, index,
						offsets, ULINT_UNDEFINED,
						&heap);
					child_no =
					btr_node_ptr_get_child_page_no(
						best_rec, offsets);

					rtr_non_leaf_insert_stack_push(
						index, rtr_info->parent_path,
						level, child_no, block,
						best_rec, least_inc);

					page_cur_position(best_rec, block,
							  cursor);
					rtr_info->mbr_adj = true;
				} else {
					/* Position at the last rec of the
					page, if it is not the leaf page */
					page_cur_position(last_rec, block,
							  cursor);
				}
			} else {
				/* There are matching records, position
				in the last matching records */
				if (rtr_info) {
					rec = last_match_rec;
					page_cur_position(
						rec, block, cursor);
				}
			}
		} else if (rtr_info) {
			/* Leaf level, no match, position at the
			last (supremum) rec */
			if (!last_match_rec) {
				page_cur_position(rec, block, cursor);
				goto func_exit;
			}

			/* There are matched records */
			matched_rec_t*	match_rec = rtr_info->matches;

			rtr_rec_t	test_rec;

			test_rec = match_rec->matched_recs->back();
#ifdef UNIV_DEBUG
			ulint		offsets_2[REC_OFFS_NORMAL_SIZE];
			ulint*		offsets2	= offsets_2;
			rec_offs_init(offsets_2);

			ut_ad(found);

			/* Verify the record to be positioned is the same
			as the last record in matched_rec vector */
			offsets2 = rec_get_offsets(test_rec.r_rec, index,
						   offsets2, ULINT_UNDEFINED,
						   &heap);

			offsets = rec_get_offsets(last_match_rec, index,
						  offsets, ULINT_UNDEFINED,
						  &heap);

			ut_ad(cmp_rec_rec(test_rec.r_rec, last_match_rec,
					  offsets2, offsets, index) == 0);
#endif /* UNIV_DEBUG */
			/* Pop the last match record and position on it */
			match_rec->matched_recs->pop_back();
			page_cur_position(test_rec.r_rec, &match_rec->block,
					  cursor);
		}
	} else {

		if (mode == PAGE_CUR_RTREE_INSERT) {
			page_no_t child_no;
			ut_ad(!last_match_rec && rec);

			offsets = rec_get_offsets(
				rec, index, offsets, ULINT_UNDEFINED, &heap);

			child_no = btr_node_ptr_get_child_page_no(rec, offsets);

			rtr_non_leaf_insert_stack_push(
				index, rtr_info->parent_path, level, child_no,
				block, rec, 0);

		} else if (rtr_info && found && !is_leaf) {
			rec = last_match_rec;
		}

		page_cur_position(rec, block, cursor);
	}

#ifdef UNIV_DEBUG
	/* Verify that we are positioned at the same child page as pushed in
	the path stack */
	if (!is_leaf && (!page_rec_is_supremum(rec) || found)
	    && mode != PAGE_CUR_RTREE_INSERT) {
		page_no_t	page_no;

		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);
		page_no = btr_node_ptr_get_child_page_no(rec, offsets);

		if (rtr_info && found) {
			rtr_node_path_t*	path = rtr_info->path;
			node_visit_t		last_visit = path->back();

			ut_ad(last_visit.page_no == page_no);
		}
	}
#endif /* UNIV_DEBUG */

func_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	return(found);
}
