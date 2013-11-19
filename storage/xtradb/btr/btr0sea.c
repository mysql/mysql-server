/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.
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
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/********************************************************************//**
@file btr/btr0sea.c
The index tree adaptive search

Created 2/17/1996 Heikki Tuuri
*************************************************************************/

#include "btr0sea.h"
#ifdef UNIV_NONINL
#include "btr0sea.ic"
#endif

#include "buf0buf.h"
#include "page0page.h"
#include "page0cur.h"
#include "btr0cur.h"
#include "btr0pcur.h"
#include "btr0btr.h"
#include "ha0ha.h"
#include "srv0srv.h"
/** Flag: has the search system been enabled?
Protected by btr_search_latch. */
UNIV_INTERN char		btr_search_enabled	= TRUE;

UNIV_INTERN ulint		btr_search_index_num	= 1;

#ifdef UNIV_PFS_MUTEX
/* Key to register btr_search_enabled_mutex with performance schema */
UNIV_INTERN mysql_pfs_key_t	btr_search_enabled_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/** A dummy variable to fool the compiler */
UNIV_INTERN ulint		btr_search_this_is_zero = 0;

#ifdef UNIV_SEARCH_PERF_STAT
/** Number of successful adaptive hash index lookups */
UNIV_INTERN ulint		btr_search_n_succ	= 0;
/** Number of failed adaptive hash index lookups */
UNIV_INTERN ulint		btr_search_n_hash_fail	= 0;
#endif /* UNIV_SEARCH_PERF_STAT */

/** padding to prevent other memory update
hotspots from residing on the same memory
cache line as btr_search_latch */
UNIV_INTERN byte		btr_sea_pad1[64];

/** Array of latches protecting individual AHI partitions. The latches
protect: (1) positions of records on those pages where a hash index from the
corresponding AHI partition has been built.
NOTE: They do not protect values of non-ordering fields within a record from
being updated in-place! We can use fact (1) to perform unique searches to
indexes. */

UNIV_INTERN rw_lock_t*		btr_search_latch_arr;

/** padding to prevent other memory update hotspots from residing on
the same memory cache line */
UNIV_INTERN byte		btr_sea_pad2[64];

/** The adaptive hash index */
UNIV_INTERN btr_search_sys_t*	btr_search_sys;

#ifdef UNIV_PFS_RWLOCK
/* Key to register btr_search_sys with performance schema */
UNIV_INTERN mysql_pfs_key_t	btr_search_latch_key;
#endif /* UNIV_PFS_RWLOCK */

/** If the number of records on the page divided by this parameter
would have been successfully accessed using a hash index, the index
is then built on the page, assuming the global limit has been reached */
#define BTR_SEARCH_PAGE_BUILD_LIMIT	16

/** The global limit for consecutive potentially successful hash searches,
before hash index building is started */
#define BTR_SEARCH_BUILD_LIMIT		100

/********************************************************************//**
Builds a hash index on a page with the given parameters. If the page already
has a hash index with different parameters, the old hash index is removed.
If index is non-NULL, this function checks if n_fields and n_bytes are
sensible values, and does not build a hash index if not. */
static
void
btr_search_build_page_hash_index(
/*=============================*/
	dict_index_t*	index,	/*!< in: index for which to build, or NULL if
				not known */
	buf_block_t*	block,	/*!< in: index page, s- or x-latched */
	ulint		n_fields,/*!< in: hash this many full fields */
	ulint		n_bytes,/*!< in: hash this many bytes from the next
				field */
	ibool		left_side);/*!< in: hash for searches from left side? */

/*****************************************************************//**
This function should be called before reserving any btr search mutex, if
the intended operation might add nodes to the search system hash table.
Because of the latching order, once we have reserved the btr search system
latch, we cannot allocate a free frame from the buffer pool. Checks that
there is a free buffer frame allocated for hash table heap in the btr search
system. If not, allocates a free frames for the heap. This check makes it
probable that, when have reserved the btr search system latch and we need to
allocate a new node to the hash table, it will succeed. However, the check
will not guarantee success. */
static
void
btr_search_check_free_space_in_heap(
/*=====================================*/
	dict_index_t*	index)
{
	hash_table_t*	table;
	mem_heap_t*	heap;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(btr_search_get_latch(index), RW_LOCK_SHARED));
	ut_ad(!rw_lock_own(btr_search_get_latch(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	table = btr_search_get_hash_table(index);

	heap = table->heap;

	/* Note that we peek the value of heap->free_block without reserving
	the latch: this is ok, because we will not guarantee that there will
	be enough free space in the hash table. */

	if (heap->free_block == NULL) {
		buf_block_t*	block = buf_block_alloc(NULL);

		rw_lock_x_lock(btr_search_get_latch(index));

		if (heap->free_block == NULL) {
			heap->free_block = block;
		} else {
			buf_block_free(block);
		}

		rw_lock_x_unlock(btr_search_get_latch(index));
	}
}

/*****************************************************************//**
Creates and initializes the adaptive search system at a database start. */
UNIV_INTERN
void
btr_search_sys_create(
/*==================*/
	ulint	hash_size)	/*!< in: hash index hash table size */
{
	ulint i;
	/* We allocate the search latch from dynamic memory:
	see above at the global variable definition */

	//btr_search_latch_temp = mem_alloc(sizeof(rw_lock_t));

	//rw_lock_create(btr_search_latch_key, &btr_search_latch,
	//	       SYNC_SEARCH_SYS);

	/* PS bug lp:1018264 - Multiple hash index partitions causes overly
	   large hash index: When multiple adaptive hash index partitions are
	   specified, _each_ partition was being created with hash_size which
	   should be 1/64 of the total size of all buffer pools which is
	   incorrect and can cause overly high memory usage. hash_size
	   should be representing the _total_ size of all partitions, not the
	   individual size of each partition. */
	hash_size /= btr_search_index_num;

	btr_search_sys = mem_alloc(sizeof(btr_search_sys_t));

	/* btr_search_index_num is constrained to machine word size for
	historical reasons. This limitation can be easily removed later. */

	btr_search_latch_arr = mem_alloc(sizeof(rw_lock_t) *
					 btr_search_index_num);
	btr_search_sys->hash_tables = mem_alloc(sizeof(hash_table_t *) *
						btr_search_index_num);
	for (i = 0; i < btr_search_index_num; i++) {

		rw_lock_create(btr_search_latch_key,
				&btr_search_latch_arr[i], SYNC_SEARCH_SYS);

		btr_search_sys->hash_tables[i] = ha_create(hash_size, 0, 0);
	}
}

/*****************************************************************//**
Frees the adaptive search system at a database shutdown. */
UNIV_INTERN
void
btr_search_sys_free(void)
/*=====================*/
{
	ulint i;

	for (i = 0; i < btr_search_index_num; i++) {
		mem_heap_free(btr_search_sys->hash_tables[i]->heap);
		hash_table_free(btr_search_sys->hash_tables[i]);

		rw_lock_free(&btr_search_latch_arr[i]);
	}
	mem_free(btr_search_sys->hash_tables);
	mem_free(btr_search_latch_arr);

	//rw_lock_free(&btr_search_latch);
	//mem_free(btr_search_latch_temp);
	//btr_search_latch_temp = NULL;
	mem_free(btr_search_sys);
	btr_search_sys = NULL;
}

/********************************************************************//**
Disable the adaptive hash search system and empty the index. */
UNIV_INTERN
void
btr_search_disable(void)
/*====================*/
{
	dict_table_t*	table;
	ulint i;

	mutex_enter(&dict_sys->mutex);
	btr_search_x_lock_all();

	btr_search_enabled = FALSE;

	/* Clear the index->search_info->ref_count of every index in
	the data dictionary cache. */
	for (table = UT_LIST_GET_FIRST(dict_sys->table_LRU); table;
	     table = UT_LIST_GET_NEXT(table_LRU, table)) {

		dict_index_t*	index;

		for (index = dict_table_get_first_index(table); index;
		     index = dict_table_get_next_index(index)) {

			index->search_info->ref_count = 0;
		}
	}

	mutex_exit(&dict_sys->mutex);

	/* Set all block->index = NULL. */
	buf_pool_clear_hash_index();

	/* Clear the adaptive hash index. */
	for (i = 0; i < btr_search_index_num; i++) {
		hash_table_clear(btr_search_sys->hash_tables[i]);
		mem_heap_empty(btr_search_sys->hash_tables[i]->heap);
	}

	btr_search_x_unlock_all();
}

/********************************************************************//**
Enable the adaptive hash search system. */
UNIV_INTERN
void
btr_search_enable(void)
/*====================*/
{
	btr_search_x_lock_all();

	btr_search_enabled = TRUE;

	btr_search_x_unlock_all();
}

/*****************************************************************//**
Creates and initializes a search info struct.
@return	own: search info struct */
UNIV_INTERN
btr_search_t*
btr_search_info_create(
/*===================*/
	mem_heap_t*	heap)	/*!< in: heap where created */
{
	btr_search_t*	info;

	info = mem_heap_alloc(heap, sizeof(btr_search_t));

#ifdef UNIV_DEBUG
	info->magic_n = BTR_SEARCH_MAGIC_N;
#endif /* UNIV_DEBUG */

	info->ref_count = 0;
	info->root_guess = NULL;

	info->hash_analysis = 0;
	info->n_hash_potential = 0;

	info->last_hash_succ = FALSE;

#ifdef UNIV_SEARCH_PERF_STAT
	info->n_hash_succ = 0;
	info->n_hash_fail = 0;
	info->n_patt_succ = 0;
	info->n_searches = 0;
#endif /* UNIV_SEARCH_PERF_STAT */

	/* Set some sensible values */
	info->n_fields = 1;
	info->n_bytes = 0;

	info->left_side = TRUE;

	return(info);
}

/*****************************************************************//**
Returns the value of ref_count. The value is protected by
the latch of the AHI partition corresponding to this index.
@return	ref_count value. */
UNIV_INTERN
ulint
btr_search_info_get_ref_count(
/*==========================*/
	btr_search_t*   info,	/*!< in: search info. */
	dict_index_t*	index)	/*!< in: index */
{
	ulint ret;

	ut_ad(info);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(btr_search_get_latch(index), RW_LOCK_SHARED));
	ut_ad(!rw_lock_own(btr_search_get_latch(index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	rw_lock_s_lock(btr_search_get_latch(index));
	ret = info->ref_count;
	rw_lock_s_unlock(btr_search_get_latch(index));

	return(ret);
}

/*********************************************************************//**
Updates the search info of an index about hash successes. NOTE that info
is NOT protected by any semaphore, to save CPU time! Do not assume its fields
are consistent. */
static
void
btr_search_info_update_hash(
/*========================*/
	btr_search_t*	info,	/*!< in/out: search info */
	btr_cur_t*	cursor)	/*!< in: cursor which was just positioned */
{
	dict_index_t*	index;
	ulint		n_unique;
	int		cmp;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(btr_search_get_latch(cursor->index), RW_LOCK_SHARED));
	ut_ad(!rw_lock_own(btr_search_get_latch(cursor->index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	index = cursor->index;

	if (dict_index_is_ibuf(index)) {
		/* So many deletes are performed on an insert buffer tree
		that we do not consider a hash index useful on it: */

		return;
	}

	n_unique = dict_index_get_n_unique_in_tree(index);

	if (info->n_hash_potential == 0) {

		goto set_new_recomm;
	}

	/* Test if the search would have succeeded using the recommended
	hash prefix */

	if (info->n_fields >= n_unique && cursor->up_match >= n_unique) {
increment_potential:
		info->n_hash_potential++;

		return;
	}

	cmp = ut_pair_cmp(info->n_fields, info->n_bytes,
			  cursor->low_match, cursor->low_bytes);

	if (info->left_side ? cmp <= 0 : cmp > 0) {

		goto set_new_recomm;
	}

	cmp = ut_pair_cmp(info->n_fields, info->n_bytes,
			  cursor->up_match, cursor->up_bytes);

	if (info->left_side ? cmp <= 0 : cmp > 0) {

		goto increment_potential;
	}

set_new_recomm:
	/* We have to set a new recommendation; skip the hash analysis
	for a while to avoid unnecessary CPU time usage when there is no
	chance for success */

	info->hash_analysis = 0;

	cmp = ut_pair_cmp(cursor->up_match, cursor->up_bytes,
			  cursor->low_match, cursor->low_bytes);
	if (cmp == 0) {
		info->n_hash_potential = 0;

		/* For extra safety, we set some sensible values here */

		info->n_fields = 1;
		info->n_bytes = 0;

		info->left_side = TRUE;

	} else if (cmp > 0) {
		info->n_hash_potential = 1;

		if (cursor->up_match >= n_unique) {

			info->n_fields = n_unique;
			info->n_bytes = 0;

		} else if (cursor->low_match < cursor->up_match) {

			info->n_fields = cursor->low_match + 1;
			info->n_bytes = 0;
		} else {
			info->n_fields = cursor->low_match;
			info->n_bytes = cursor->low_bytes + 1;
		}

		info->left_side = TRUE;
	} else {
		info->n_hash_potential = 1;

		if (cursor->low_match >= n_unique) {

			info->n_fields = n_unique;
			info->n_bytes = 0;

		} else if (cursor->low_match > cursor->up_match) {

			info->n_fields = cursor->up_match + 1;
			info->n_bytes = 0;
		} else {
			info->n_fields = cursor->up_match;
			info->n_bytes = cursor->up_bytes + 1;
		}

		info->left_side = FALSE;
	}
}

/*********************************************************************//**
Updates the block search info on hash successes. NOTE that info and
block->n_hash_helps, n_fields, n_bytes, side are NOT protected by any
semaphore, to save CPU time! Do not assume the fields are consistent.
@return	TRUE if building a (new) hash index on the block is recommended */
static
ibool
btr_search_update_block_hash_info(
/*==============================*/
	btr_search_t*	info,	/*!< in: search info */
	buf_block_t*	block,	/*!< in: buffer block */
	btr_cur_t*	cursor __attribute__((unused)))
				/*!< in: cursor */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(btr_search_get_latch(cursor->index), RW_LOCK_SHARED));
	ut_ad(!rw_lock_own(btr_search_get_latch(cursor->index), RW_LOCK_EX));
	ut_ad(rw_lock_own(&block->lock, RW_LOCK_SHARED)
	      || rw_lock_own(&block->lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(cursor);

	info->last_hash_succ = FALSE;

	ut_a(buf_block_state_valid(block));
	ut_ad(info->magic_n == BTR_SEARCH_MAGIC_N);

	if ((block->n_hash_helps > 0)
	    && (info->n_hash_potential > 0)
	    && (block->n_fields == info->n_fields)
	    && (block->n_bytes == info->n_bytes)
	    && (block->left_side == info->left_side)) {

		if ((block->index)
		    && (block->curr_n_fields == info->n_fields)
		    && (block->curr_n_bytes == info->n_bytes)
		    && (block->curr_left_side == info->left_side)) {

			/* The search would presumably have succeeded using
			the hash index */

			info->last_hash_succ = TRUE;
		}

		block->n_hash_helps++;
	} else {
		block->n_hash_helps = 1;
		block->n_fields = info->n_fields;
		block->n_bytes = info->n_bytes;
		block->left_side = info->left_side;
	}

#ifdef UNIV_DEBUG
	if (cursor->index->table->does_not_fit_in_memory) {
		block->n_hash_helps = 0;
	}
#endif /* UNIV_DEBUG */

	if ((block->n_hash_helps > page_get_n_recs(block->frame)
	     / BTR_SEARCH_PAGE_BUILD_LIMIT)
	    && (info->n_hash_potential >= BTR_SEARCH_BUILD_LIMIT)) {

		if ((!block->index)
		    || (block->n_hash_helps
			> 2 * page_get_n_recs(block->frame))
		    || (block->n_fields != block->curr_n_fields)
		    || (block->n_bytes != block->curr_n_bytes)
		    || (block->left_side != block->curr_left_side)) {

			/* Build a new hash index on the page */

			return(TRUE);
		}
	}

	return(FALSE);
}

/*********************************************************************//**
Updates a hash node reference when it has been unsuccessfully used in a
search which could have succeeded with the used hash parameters. This can
happen because when building a hash index for a page, we do not check
what happens at page boundaries, and therefore there can be misleading
hash nodes. Also, collisions in the fold value can lead to misleading
references. This function lazily fixes these imperfections in the hash
index. */
static
void
btr_search_update_hash_ref(
/*=======================*/
	btr_search_t*	info,	/*!< in: search info */
	buf_block_t*	block,	/*!< in: buffer block where cursor positioned */
	btr_cur_t*	cursor)	/*!< in: cursor */
{
	dict_index_t*	index;
	ulint		fold;
	const rec_t*	rec;

	ut_ad(cursor->flag == BTR_CUR_HASH_FAIL);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(btr_search_get_latch(cursor->index), RW_LOCK_EX));
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_SHARED)
	      || rw_lock_own(&(block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(page_align(btr_cur_get_rec(cursor))
	      == buf_block_get_frame(block));

	index = block->index;

	if (!index) {

		return;
	}

	ut_a(index == cursor->index);
	ut_a(!dict_index_is_ibuf(index));

	if ((info->n_hash_potential > 0)
	    && (block->curr_n_fields == info->n_fields)
	    && (block->curr_n_bytes == info->n_bytes)
	    && (block->curr_left_side == info->left_side)) {
		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		rec_offs_init(offsets_);

		rec = btr_cur_get_rec(cursor);

		if (!page_rec_is_user_rec(rec)) {

			return;
		}

		fold = rec_fold(rec,
				rec_get_offsets(rec, index, offsets_,
						ULINT_UNDEFINED, &heap),
				block->curr_n_fields,
				block->curr_n_bytes, index->id);
		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
#ifdef UNIV_SYNC_DEBUG
		ut_ad(rw_lock_own(btr_search_get_latch(cursor->index),
				  RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

		ha_insert_for_fold(btr_search_get_hash_table(cursor->index),
				   fold, block, rec);
	}
}

/*********************************************************************//**
Updates the search info. */
UNIV_INTERN
void
btr_search_info_update_slow(
/*========================*/
	btr_search_t*	info,	/*!< in/out: search info */
	btr_cur_t*	cursor)	/*!< in: cursor which was just positioned */
{
	buf_block_t*	block;
	ibool		build_index;
	ulint*		params;
	ulint*		params2;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(btr_search_get_latch(cursor->index), RW_LOCK_SHARED));
	ut_ad(!rw_lock_own(btr_search_get_latch(cursor->index), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	block = btr_cur_get_block(cursor);

	SRV_CORRUPT_TABLE_CHECK(block, return;);

	/* NOTE that the following two function calls do NOT protect
	info or block->n_fields etc. with any semaphore, to save CPU time!
	We cannot assume the fields are consistent when we return from
	those functions! */

	btr_search_info_update_hash(info, cursor);

	build_index = btr_search_update_block_hash_info(info, block, cursor);

	if (build_index || (cursor->flag == BTR_CUR_HASH_FAIL)) {

		btr_search_check_free_space_in_heap(cursor->index);
	}

	if (cursor->flag == BTR_CUR_HASH_FAIL) {
		/* Update the hash node reference, if appropriate */

#ifdef UNIV_SEARCH_PERF_STAT
		btr_search_n_hash_fail++;
#endif /* UNIV_SEARCH_PERF_STAT */

		rw_lock_x_lock(btr_search_get_latch(cursor->index));

		btr_search_update_hash_ref(info, block, cursor);

		rw_lock_x_unlock(btr_search_get_latch(cursor->index));
	}

	if (build_index) {
		/* Note that since we did not protect block->n_fields etc.
		with any semaphore, the values can be inconsistent. We have
		to check inside the function call that they make sense. We
		also malloc an array and store the values there to make sure
		the compiler does not let the function call parameters change
		inside the called function. It might be that the compiler
		would optimize the call just to pass pointers to block. */

		params = mem_alloc(3 * sizeof(ulint));
		params[0] = block->n_fields;
		params[1] = block->n_bytes;
		params[2] = block->left_side;

		/* Make sure the compiler cannot deduce the values and do
		optimizations */

		params2 = params + btr_search_this_is_zero;

		btr_search_build_page_hash_index(cursor->index,
						 block,
						 params2[0],
						 params2[1],
						 params2[2]);
		mem_free(params);
	}
}

/******************************************************************//**
Checks if a guessed position for a tree cursor is right. Note that if
mode is PAGE_CUR_LE, which is used in inserts, and the function returns
TRUE, then cursor->up_match and cursor->low_match both have sensible values.
@return	TRUE if success */
static
ibool
btr_search_check_guess(
/*===================*/
	btr_cur_t*	cursor,	/*!< in: guessed cursor position */
	ibool		can_only_compare_to_cursor_rec,
				/*!< in: if we do not have a latch on the page
				of cursor, but only a latch on
				btr_search_latch, then ONLY the columns
				of the record UNDER the cursor are
				protected, not the next or previous record
				in the chain: we cannot look at the next or
				previous record to check our guess! */
	const dtuple_t*	tuple,	/*!< in: data tuple */
	ulint		mode,	/*!< in: PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G,
				or PAGE_CUR_GE */
	mtr_t*		mtr)	/*!< in: mtr */
{
	rec_t*		rec;
	ulint		n_unique;
	ulint		match;
	ulint		bytes;
	int		cmp;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	ibool		success		= FALSE;
	rec_offs_init(offsets_);

	n_unique = dict_index_get_n_unique_in_tree(cursor->index);

	rec = btr_cur_get_rec(cursor);

	ut_ad(page_rec_is_user_rec(rec));

	match = 0;
	bytes = 0;

	offsets = rec_get_offsets(rec, cursor->index, offsets,
				  n_unique, &heap);
	cmp = page_cmp_dtuple_rec_with_match(tuple, rec,
					     offsets, &match, &bytes);

	if (mode == PAGE_CUR_GE) {
		if (cmp == 1) {
			goto exit_func;
		}

		cursor->up_match = match;

		if (match >= n_unique) {
			success = TRUE;
			goto exit_func;
		}
	} else if (mode == PAGE_CUR_LE) {
		if (cmp == -1) {
			goto exit_func;
		}

		cursor->low_match = match;

	} else if (mode == PAGE_CUR_G) {
		if (cmp != -1) {
			goto exit_func;
		}
	} else if (mode == PAGE_CUR_L) {
		if (cmp != 1) {
			goto exit_func;
		}
	}

	if (can_only_compare_to_cursor_rec) {
		/* Since we could not determine if our guess is right just by
		looking at the record under the cursor, return FALSE */
		goto exit_func;
	}

	match = 0;
	bytes = 0;

	if ((mode == PAGE_CUR_G) || (mode == PAGE_CUR_GE)) {
		rec_t*	prev_rec;

		ut_ad(!page_rec_is_infimum(rec));

		prev_rec = page_rec_get_prev(rec);

		if (page_rec_is_infimum(prev_rec)) {
			success = btr_page_get_prev(page_align(prev_rec), mtr)
				== FIL_NULL;

			goto exit_func;
		}

		offsets = rec_get_offsets(prev_rec, cursor->index, offsets,
					  n_unique, &heap);
		cmp = page_cmp_dtuple_rec_with_match(tuple, prev_rec,
						     offsets, &match, &bytes);
		if (mode == PAGE_CUR_GE) {
			success = cmp == 1;
		} else {
			success = cmp != -1;
		}

		goto exit_func;
	} else {
		rec_t*	next_rec;

		ut_ad(!page_rec_is_supremum(rec));

		next_rec = page_rec_get_next(rec);

		if (page_rec_is_supremum(next_rec)) {
			if (btr_page_get_next(page_align(next_rec), mtr)
			    == FIL_NULL) {

				cursor->up_match = 0;
				success = TRUE;
			}

			goto exit_func;
		}

		offsets = rec_get_offsets(next_rec, cursor->index, offsets,
					  n_unique, &heap);
		cmp = page_cmp_dtuple_rec_with_match(tuple, next_rec,
						     offsets, &match, &bytes);
		if (mode == PAGE_CUR_LE) {
			success = cmp == -1;
			cursor->up_match = match;
		} else {
			success = cmp != 1;
		}
	}
exit_func:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(success);
}

/******************************************************************//**
Tries to guess the right search position based on the hash search info
of the index. Note that if mode is PAGE_CUR_LE, which is used in inserts,
and the function returns TRUE, then cursor->up_match and cursor->low_match
both have sensible values.
@return	TRUE if succeeded */
UNIV_INTERN
ibool
btr_search_guess_on_hash(
/*=====================*/
	dict_index_t*	index,		/*!< in: index */
	btr_search_t*	info,		/*!< in: index search info */
	const dtuple_t*	tuple,		/*!< in: logical record */
	ulint		mode,		/*!< in: PAGE_CUR_L, ... */
	ulint		latch_mode,	/*!< in: BTR_SEARCH_LEAF, ...;
					NOTE that only if has_search_latch
					is 0, we will have a latch set on
					the cursor page, otherwise we assume
					the caller uses his search latch
					to protect the record! */
	btr_cur_t*	cursor,		/*!< out: tree cursor */
	ulint		has_search_latch,/*!< in: latch mode the caller
					currently has on btr_search_latch:
					RW_S_LATCH, RW_X_LATCH, or 0 */
	mtr_t*		mtr)		/*!< in: mtr */
{
	buf_pool_t*	buf_pool;
	buf_block_t*	block;
	const rec_t*	rec;
	ulint		fold;
	index_id_t	index_id;
#ifdef notdefined
	btr_cur_t	cursor2;
	btr_pcur_t	pcur;
#endif
	ut_ad(index && info && tuple && cursor && mtr);
	ut_ad(!dict_index_is_ibuf(index));
	ut_ad((latch_mode == BTR_SEARCH_LEAF)
	      || (latch_mode == BTR_MODIFY_LEAF));

	/* Note that, for efficiency, the struct info may not be protected by
	any latch here! */

	if (UNIV_UNLIKELY(info->n_hash_potential == 0)) {

		return(FALSE);
	}

	cursor->n_fields = info->n_fields;
	cursor->n_bytes = info->n_bytes;

	if (UNIV_UNLIKELY(dtuple_get_n_fields(tuple)
			  < cursor->n_fields + (cursor->n_bytes > 0))) {

		return(FALSE);
	}

	index_id = index->id;

#ifdef UNIV_SEARCH_PERF_STAT
	info->n_hash_succ++;
#endif
	fold = dtuple_fold(tuple, cursor->n_fields, cursor->n_bytes, index_id);

	cursor->fold = fold;
	cursor->flag = BTR_CUR_HASH;

	if (UNIV_LIKELY(!has_search_latch)) {
		rw_lock_s_lock(btr_search_get_latch(index));

		if (UNIV_UNLIKELY(!btr_search_enabled)) {
			goto failure_unlock;
		}
	}

	ut_ad(rw_lock_get_writer(btr_search_get_latch(index)) != RW_LOCK_EX);
	ut_ad(rw_lock_get_reader_count(btr_search_get_latch(index)) > 0);

	rec = ha_search_and_get_data(btr_search_get_hash_table(index), fold);

	if (UNIV_UNLIKELY(!rec)) {
		goto failure_unlock;
	}

	block = buf_block_align(rec);

	if (UNIV_LIKELY(!has_search_latch)) {

		if (UNIV_UNLIKELY(
			    !buf_page_get_known_nowait(latch_mode, block,
						       BUF_MAKE_YOUNG,
						       __FILE__, __LINE__,
						       mtr))) {
			goto failure_unlock;
		}

		rw_lock_s_unlock(btr_search_get_latch(index));

		buf_block_dbg_add_level(block, SYNC_TREE_NODE_FROM_HASH);
	}

	if (UNIV_UNLIKELY(buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE)) {
		ut_ad(buf_block_get_state(block) == BUF_BLOCK_REMOVE_HASH);

		if (UNIV_LIKELY(!has_search_latch)) {

			btr_leaf_page_release(block, latch_mode, mtr);
		}

		goto failure;
	}

	ut_ad(page_rec_is_user_rec(rec));

	btr_cur_position(index, (rec_t*) rec, block, cursor);

	/* Check the validity of the guess within the page */

	/* If we only have the latch on btr_search_latch, not on the
	page, it only protects the columns of the record the cursor
	is positioned on. We cannot look at the next of the previous
	record to determine if our guess for the cursor position is
	right. */
	if (UNIV_UNLIKELY(index_id != btr_page_get_index_id(block->frame))
	    || !btr_search_check_guess(cursor,
				       has_search_latch,
				       tuple, mode, mtr)) {
		if (UNIV_LIKELY(!has_search_latch)) {
			btr_leaf_page_release(block, latch_mode, mtr);
		}

		goto failure;
	}

	if (UNIV_LIKELY(info->n_hash_potential < BTR_SEARCH_BUILD_LIMIT + 5)) {

		info->n_hash_potential++;
	}

#ifdef notdefined
	/* These lines of code can be used in a debug version to check
	the correctness of the searched cursor position: */

	info->last_hash_succ = FALSE;

	/* Currently, does not work if the following fails: */
	ut_ad(!has_search_latch);

	btr_leaf_page_release(block, latch_mode, mtr);

	btr_cur_search_to_nth_level(index, 0, tuple, mode, latch_mode,
				    &cursor2, 0, mtr);
	if (mode == PAGE_CUR_GE
	    && page_rec_is_supremum(btr_cur_get_rec(&cursor2))) {

		/* If mode is PAGE_CUR_GE, then the binary search
		in the index tree may actually take us to the supremum
		of the previous page */

		info->last_hash_succ = FALSE;

		btr_pcur_open_on_user_rec(index, tuple, mode, latch_mode,
					  &pcur, mtr);
		ut_ad(btr_pcur_get_rec(&pcur) == btr_cur_get_rec(cursor));
	} else {
		ut_ad(btr_cur_get_rec(&cursor2) == btr_cur_get_rec(cursor));
	}

	/* NOTE that it is theoretically possible that the above assertions
	fail if the page of the cursor gets removed from the buffer pool
	meanwhile! Thus it might not be a bug. */
#endif
	info->last_hash_succ = TRUE;

#ifdef UNIV_SEARCH_PERF_STAT
	btr_search_n_succ++;
#endif
	if (UNIV_LIKELY(!has_search_latch)
	    && buf_page_peek_if_too_old(&block->page)) {

		buf_page_make_young(&block->page);
	}

	/* Increment the page get statistics though we did not really
	fix the page: for user info only */
	buf_pool = buf_pool_from_bpage(&block->page);
	buf_pool->stat.n_page_gets++;

	return(TRUE);

	/*-------------------------------------------*/
failure_unlock:
	if (UNIV_LIKELY(!has_search_latch)) {
		rw_lock_s_unlock(btr_search_get_latch(index));
	}
failure:
	cursor->flag = BTR_CUR_HASH_FAIL;

#ifdef UNIV_SEARCH_PERF_STAT
	info->n_hash_fail++;

	if (info->n_hash_succ > 0) {
		info->n_hash_succ--;
	}
#endif
	info->last_hash_succ = FALSE;

	return(FALSE);
}

/********************************************************************//**
Drops a page hash index. */
UNIV_INTERN
void
btr_search_drop_page_hash_index(
/*============================*/
	buf_block_t*	block)	/*!< in: block containing index page,
				s- or x-latched, or an index page
				for which we know that
				block->buf_fix_count == 0 */
{
	hash_table_t*		table;
	ulint			n_fields;
	ulint			n_bytes;
	const page_t*		page;
	const rec_t*		rec;
	ulint			fold;
	ulint			prev_fold;
	index_id_t		index_id;
	ulint			n_cached;
	ulint			n_recs;
	ulint*			folds;
	ulint			i;
	mem_heap_t*		heap;
	const dict_index_t*	index;
	ulint*			offsets;

retry:
	/* Do a dirty check on block->index, return if the block is not in the
	adaptive hash index. This is to avoid acquiring an AHI latch for
	performance considerations. */

	index = block->index;
	if (!index) {

		return;
	}

	rw_lock_s_lock(btr_search_get_latch(index));

	if (UNIV_UNLIKELY(index != block->index)) {

		rw_lock_s_unlock(btr_search_get_latch(index));

		goto retry;
	}

	table = btr_search_get_hash_table(index);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_SHARED)
	      || rw_lock_own(&(block->lock), RW_LOCK_EX)
	      || (block->page.buf_fix_count == 0));
#endif /* UNIV_SYNC_DEBUG */

	n_fields = block->curr_n_fields;
	n_bytes = block->curr_n_bytes;
	ut_a(index == block->index);
	ut_a(!dict_index_is_ibuf(index));

	/* NOTE: The fields of block must not be accessed after
	releasing btr_search_latch, as the index page might only
	be s-latched! */

	rw_lock_s_unlock(btr_search_get_latch(index));

	ut_a(n_fields + n_bytes > 0);

	page = block->frame;
	n_recs = page_get_n_recs(page);

	/* Calculate and cache fold values into an array for fast deletion
	from the hash index */

	folds = mem_alloc(n_recs * sizeof(ulint));

	n_cached = 0;

	rec = page_get_infimum_rec(page);
	rec = page_rec_get_next_low(rec, page_is_comp(page));

	index_id = btr_page_get_index_id(page);

	ut_a(index_id == index->id);

	prev_fold = 0;

	heap = NULL;
	offsets = NULL;

	while (!page_rec_is_supremum(rec)) {
		offsets = rec_get_offsets(rec, index, offsets,
					  n_fields + (n_bytes > 0), &heap);
		ut_a(rec_offs_n_fields(offsets) == n_fields + (n_bytes > 0));
		fold = rec_fold(rec, offsets, n_fields, n_bytes, index_id);

		if (fold == prev_fold && prev_fold != 0) {

			goto next_rec;
		}

		/* Remove all hash nodes pointing to this page from the
		hash chain */

		folds[n_cached] = fold;
		n_cached++;
next_rec:
		rec = page_rec_get_next_low(rec, page_rec_is_comp(rec));
		prev_fold = fold;
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	rw_lock_x_lock(btr_search_get_latch(index));

	if (UNIV_UNLIKELY(!block->index)) {
		/* Someone else has meanwhile dropped the hash index */

		goto cleanup;
	}

	ut_a(block->index == index);

	if (UNIV_UNLIKELY(block->curr_n_fields != n_fields)
	    || UNIV_UNLIKELY(block->curr_n_bytes != n_bytes)) {

		/* Someone else has meanwhile built a new hash index on the
		page, with different parameters */

		rw_lock_x_unlock(btr_search_get_latch(index));

		mem_free(folds);
		goto retry;
	}

	for (i = 0; i < n_cached; i++) {

		ha_remove_all_nodes_to_page(table, folds[i], page);
	}

	ut_a(index->search_info->ref_count > 0);
	index->search_info->ref_count--;

	block->index = NULL;

cleanup:
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	if (UNIV_UNLIKELY(block->n_pointers)) {
		/* Corruption */
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Corruption of adaptive hash index."
			" After dropping\n"
			"InnoDB: the hash index to a page of %s,"
			" still %lu hash nodes remain.\n",
			index->name, (ulong) block->n_pointers);
		rw_lock_x_unlock(btr_search_get_latch(index));

		ut_ad(btr_search_validate());
	} else {
		rw_lock_x_unlock(btr_search_get_latch(index));
	}
#else /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	rw_lock_x_unlock(btr_search_get_latch(index));
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

	mem_free(folds);
}

/************************************************************************
Drops a page hash index based on index */
UNIV_INTERN
void
btr_search_drop_page_hash_index_on_index(
/*=====================================*/
	dict_index_t*	index)		/* in: record descriptor */
{

	hash_table_t*	table;
	buf_block_t*	block;
	ulint		n_fields;
	ulint		n_bytes;
	const page_t*		page;
	const rec_t*		rec;
	ulint		fold;
	ulint		prev_fold;
	index_id_t	index_id;
	ulint		n_cached;
	ulint		n_recs;
	ulint*		folds;
	ulint		i, j;
	mem_heap_t*	heap	= NULL;
	ulint*		offsets;
	ibool		released_search_latch;

	rw_lock_s_lock(btr_search_get_latch(index));

	table = btr_search_get_hash_table(index);

	for (j = 0; j < srv_buf_pool_instances; j++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(j);

		do {
			buf_chunk_t*	chunks	= buf_pool->chunks;
			buf_chunk_t*	chunk	= chunks + buf_pool->n_chunks;

			released_search_latch = FALSE;

			while (--chunk >= chunks) {
				block	= chunk->blocks;
				i	= chunk->size;

retry:
				for (; i--; block++) {
					if (buf_block_get_state(block)
					    != BUF_BLOCK_FILE_PAGE
					    || block->index != index
					    || !block->index) {
						continue;
					}

					page = block->frame;

					/* from btr_search_drop_page_hash_index() */
					n_fields = block->curr_n_fields;
					n_bytes = block->curr_n_bytes;


					/* keeping latch order */
					rw_lock_s_unlock(
						btr_search_get_latch(index));
					released_search_latch = TRUE;
					rw_lock_x_lock(&block->lock);


					ut_a(n_fields + n_bytes > 0);

					n_recs = page_get_n_recs(page);

					/* Calculate and cache fold values into an array for fast deletion
					from the hash index */

					folds = mem_alloc(n_recs * sizeof(ulint));

					n_cached = 0;

					rec = page_get_infimum_rec(page);
					rec = page_rec_get_next_low(rec, page_is_comp(page));

					index_id = btr_page_get_index_id(page);
	
					ut_a(index_id == index->id);

					prev_fold = 0;

					offsets = NULL;

					while (!page_rec_is_supremum(rec)) {
						offsets = rec_get_offsets(rec, index, offsets,
									n_fields + (n_bytes > 0), &heap);
						ut_a(rec_offs_n_fields(offsets) == n_fields + (n_bytes > 0));
						fold = rec_fold(rec, offsets, n_fields, n_bytes, index_id);

						if (fold == prev_fold && prev_fold != 0) {

							goto next_rec;
						}

						/* Remove all hash nodes pointing to this page from the
						hash chain */

						folds[n_cached] = fold;
						n_cached++;
next_rec:
						rec = page_rec_get_next_low(rec, page_rec_is_comp(rec));
						prev_fold = fold;
					}

					if (UNIV_LIKELY_NULL(heap)) {
						mem_heap_empty(heap);
					}

					rw_lock_x_lock(
						btr_search_get_latch(index));

					if (UNIV_UNLIKELY(!block->index)) {
						goto cleanup;
					}

					ut_a(block->index == index);

					if (UNIV_UNLIKELY(block->curr_n_fields != n_fields)
					    || UNIV_UNLIKELY(block->curr_n_bytes != n_bytes)) {
						rw_lock_x_unlock(
							btr_search_get_latch(index));
						rw_lock_x_unlock(&block->lock);

						mem_free(folds);

						rw_lock_s_lock(
							btr_search_get_latch(index));
						goto retry;
					}

					for (i = 0; i < n_cached; i++) {

						ha_remove_all_nodes_to_page(table, folds[i], page);
					}

					ut_a(index->search_info->ref_count > 0);
					index->search_info->ref_count--;

					block->index = NULL;

cleanup:
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
					if (UNIV_UNLIKELY(block->n_pointers)) {
						/* Corruption */
						ut_print_timestamp(stderr);
						fprintf(stderr,
"InnoDB: The adaptive hash index is corrupted. After dropping\n"
"InnoDB: the hash index to a page of %s, %lu hash nodes still remain.\n",
							index->name, (ulong) block->n_pointers);
					}
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
					rw_lock_x_unlock(
						btr_search_get_latch(index));
					rw_lock_x_unlock(&block->lock);

					mem_free(folds);

					rw_lock_s_lock(
						btr_search_get_latch(index));
				}
			}
		} while (released_search_latch);
	}

	rw_lock_s_unlock(btr_search_get_latch(index));

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/********************************************************************//**
Drops a possible page hash index when a page is evicted from the buffer pool
or freed in a file segment. */
UNIV_INTERN
void
btr_search_drop_page_hash_when_freed(
/*=================================*/
	ulint	space,		/*!< in: space id */
	ulint	zip_size,	/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint	page_no)	/*!< in: page number */
{
	buf_block_t*	block;
	mtr_t		mtr;

	mtr_start(&mtr);

	/* If the caller has a latch on the page, then the caller must
	have a x-latch on the page and it must have already dropped
	the hash index for the page. Because of the x-latch that we
	are possibly holding, we cannot s-latch the page, but must
	(recursively) x-latch it, even though we are only reading. */

	block = buf_page_get_gen(space, zip_size, page_no, RW_X_LATCH, NULL,
				 BUF_PEEK_IF_IN_POOL, __FILE__, __LINE__,
				 &mtr);

	if (block && block->index) {

		buf_block_dbg_add_level(block, SYNC_TREE_NODE_FROM_HASH);

		btr_search_drop_page_hash_index(block);
	}

	mtr_commit(&mtr);
}

/********************************************************************//**
Builds a hash index on a page with the given parameters. If the page already
has a hash index with different parameters, the old hash index is removed.
If index is non-NULL, this function checks if n_fields and n_bytes are
sensible values, and does not build a hash index if not. */
static
void
btr_search_build_page_hash_index(
/*=============================*/
	dict_index_t*	index,	/*!< in: index for which to build */
	buf_block_t*	block,	/*!< in: index page, s- or x-latched */
	ulint		n_fields,/*!< in: hash this many full fields */
	ulint		n_bytes,/*!< in: hash this many bytes from the next
				field */
	ibool		left_side)/*!< in: hash for searches from left side? */
{
	hash_table_t*	table;
	page_t*		page;
	rec_t*		rec;
	rec_t*		next_rec;
	ulint		fold;
	ulint		next_fold;
	ulint		n_cached;
	ulint		n_recs;
	ulint*		folds;
	rec_t**		recs;
	ulint		i;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	ut_ad(index);
	ut_a(!dict_index_is_ibuf(index));

	table = btr_search_get_hash_table(index);
	page = buf_block_get_frame(block);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(btr_search_get_latch(index->id), RW_LOCK_EX));
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_SHARED)
	      || rw_lock_own(&(block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	rw_lock_s_lock(btr_search_get_latch(index));

	if (block->index && ((block->curr_n_fields != n_fields)
				 || (block->curr_n_bytes != n_bytes)
				 || (block->curr_left_side != left_side))) {

		rw_lock_s_unlock(btr_search_get_latch(index));

		btr_search_drop_page_hash_index(block);
	} else {
		rw_lock_s_unlock(btr_search_get_latch(index));
	}

	n_recs = page_get_n_recs(page);

	if (n_recs == 0) {

		return;
	}

	/* Check that the values for hash index build are sensible */

	if (n_fields + n_bytes == 0) {

		return;
	}

	if (dict_index_get_n_unique_in_tree(index) < n_fields
	    || (dict_index_get_n_unique_in_tree(index) == n_fields
		&& n_bytes > 0)) {
		return;
	}

	/* Calculate and cache fold values and corresponding records into
	an array for fast insertion to the hash index */

	folds = mem_alloc(n_recs * sizeof(ulint));
	recs = mem_alloc(n_recs * sizeof(rec_t*));

	n_cached = 0;

	ut_a(index->id == btr_page_get_index_id(page));

	rec = page_rec_get_next(page_get_infimum_rec(page));

	offsets = rec_get_offsets(rec, index, offsets,
				  n_fields + (n_bytes > 0), &heap);

	if (!page_rec_is_supremum(rec)) {
		ut_a(n_fields <= rec_offs_n_fields(offsets));

		if (n_bytes > 0) {
			ut_a(n_fields < rec_offs_n_fields(offsets));
		}
	}

	fold = rec_fold(rec, offsets, n_fields, n_bytes, index->id);

	if (left_side) {

		folds[n_cached] = fold;
		recs[n_cached] = rec;
		n_cached++;
	}

	for (;;) {
		next_rec = page_rec_get_next(rec);

		if (page_rec_is_supremum(next_rec)) {

			if (!left_side) {

				folds[n_cached] = fold;
				recs[n_cached] = rec;
				n_cached++;
			}

			break;
		}

		offsets = rec_get_offsets(next_rec, index, offsets,
					  n_fields + (n_bytes > 0), &heap);
		next_fold = rec_fold(next_rec, offsets, n_fields,
				     n_bytes, index->id);

		if (fold != next_fold) {
			/* Insert an entry into the hash index */

			if (left_side) {

				folds[n_cached] = next_fold;
				recs[n_cached] = next_rec;
				n_cached++;
			} else {
				folds[n_cached] = fold;
				recs[n_cached] = rec;
				n_cached++;
			}
		}

		rec = next_rec;
		fold = next_fold;
	}

	btr_search_check_free_space_in_heap(index);

	rw_lock_x_lock(btr_search_get_latch(index));

	if (UNIV_UNLIKELY(!btr_search_enabled)) {
		goto exit_func;
	}

	if (block->index && ((block->curr_n_fields != n_fields)
			     || (block->curr_n_bytes != n_bytes)
			     || (block->curr_left_side != left_side))) {
		goto exit_func;
	}

	/* This counter is decremented every time we drop page
	hash index entries and is incremented here. Since we can
	rebuild hash index for a page that is already hashed, we
	have to take care not to increment the counter in that
	case. */
	if (!block->index) {
		index->search_info->ref_count++;
	}

	block->n_hash_helps = 0;

	block->curr_n_fields = n_fields;
	block->curr_n_bytes = n_bytes;
	block->curr_left_side = left_side;
	block->index = index;

	for (i = 0; i < n_cached; i++) {

		ha_insert_for_fold(table, folds[i], block, recs[i]);
	}

exit_func:
	rw_lock_x_unlock(btr_search_get_latch(index));

	mem_free(folds);
	mem_free(recs);
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/********************************************************************//**
Moves or deletes hash entries for moved records. If new_page is already hashed,
then the hash index for page, if any, is dropped. If new_page is not hashed,
and page is hashed, then a new hash index is built to new_page with the same
parameters as page (this often happens when a page is split). */
UNIV_INTERN
void
btr_search_move_or_delete_hash_entries(
/*===================================*/
	buf_block_t*	new_block,	/*!< in: records are copied
					to this page */
	buf_block_t*	block,		/*!< in: index page from which
					records were copied, and the
					copied records will be deleted
					from this page */
	dict_index_t*	index)		/*!< in: record descriptor */
{
	ulint	n_fields;
	ulint	n_bytes;
	ibool	left_side;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_EX));
	ut_ad(rw_lock_own(&(new_block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	rw_lock_s_lock(btr_search_get_latch(index));

	ut_a(!new_block->index || new_block->index == index);
	ut_a(!block->index || block->index == index);
	ut_a(!(new_block->index || block->index)
	     || !dict_index_is_ibuf(index));

	if (new_block->index) {

		rw_lock_s_unlock(btr_search_get_latch(index));

		btr_search_drop_page_hash_index(block);

		return;
	}

	if (block->index) {

		n_fields = block->curr_n_fields;
		n_bytes = block->curr_n_bytes;
		left_side = block->curr_left_side;

		new_block->n_fields = block->curr_n_fields;
		new_block->n_bytes = block->curr_n_bytes;
		new_block->left_side = left_side;

		rw_lock_s_unlock(btr_search_get_latch(index));

		ut_a(n_fields + n_bytes > 0);

		btr_search_build_page_hash_index(index, new_block, n_fields,
						 n_bytes, left_side);
		ut_ad(n_fields == block->curr_n_fields);
		ut_ad(n_bytes == block->curr_n_bytes);
		ut_ad(left_side == block->curr_left_side);
		return;
	}

	rw_lock_s_unlock(btr_search_get_latch(index));
}

/********************************************************************//**
Updates the page hash index when a single record is deleted from a page. */
UNIV_INTERN
void
btr_search_update_hash_on_delete(
/*=============================*/
	btr_cur_t*	cursor)	/*!< in: cursor which was positioned on the
				record to delete using btr_cur_search_...,
				the record is not yet deleted */
{
	hash_table_t*	table;
	buf_block_t*	block;
	const rec_t*	rec;
	ulint		fold;
	dict_index_t*	index;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	mem_heap_t*	heap		= NULL;
	rec_offs_init(offsets_);

	block = btr_cur_get_block(cursor);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	index = block->index;

	if (!index) {

		return;
	}

	ut_a(index == cursor->index);
	ut_a(block->curr_n_fields + block->curr_n_bytes > 0);
	ut_a(!dict_index_is_ibuf(index));

	table = btr_search_get_hash_table(cursor->index);

	rec = btr_cur_get_rec(cursor);

	fold = rec_fold(rec, rec_get_offsets(rec, index, offsets_,
					     ULINT_UNDEFINED, &heap),
			block->curr_n_fields, block->curr_n_bytes, index->id);
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	rw_lock_x_lock(btr_search_get_latch(cursor->index));

	if (block->index) {
		ut_a(block->index == index);

		ha_search_and_delete_if_found(table, fold, rec);
	}

	rw_lock_x_unlock(btr_search_get_latch(cursor->index));
}

/********************************************************************//**
Updates the page hash index when a single record is inserted on a page. */
UNIV_INTERN
void
btr_search_update_hash_node_on_insert(
/*==================================*/
	btr_cur_t*	cursor)	/*!< in: cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
{
	hash_table_t*	table;
	buf_block_t*	block;
	dict_index_t*	index;
	rec_t*		rec;

	rec = btr_cur_get_rec(cursor);

	block = btr_cur_get_block(cursor);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	index = block->index;

	if (!index) {

		return;
	}

	ut_a(cursor->index == index);
	ut_a(!dict_index_is_ibuf(index));

	rw_lock_x_lock(btr_search_get_latch(cursor->index));

	if (!block->index) {

		goto func_exit;
	}

	ut_a(block->index == index);

	if ((cursor->flag == BTR_CUR_HASH)
	    && (cursor->n_fields == block->curr_n_fields)
	    && (cursor->n_bytes == block->curr_n_bytes)
	    && !block->curr_left_side) {

		table = btr_search_get_hash_table(cursor->index);

		ha_search_and_update_if_found(table, cursor->fold, rec,
					      block, page_rec_get_next(rec));

func_exit:
		rw_lock_x_unlock(btr_search_get_latch(cursor->index));
	} else {
		rw_lock_x_unlock(btr_search_get_latch(cursor->index));

		btr_search_update_hash_on_insert(cursor);
	}
}

/********************************************************************//**
Updates the page hash index when a single record is inserted on a page. */
UNIV_INTERN
void
btr_search_update_hash_on_insert(
/*=============================*/
	btr_cur_t*	cursor)	/*!< in: cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
{
	hash_table_t*	table;
	buf_block_t*	block;
	dict_index_t*	index;
	rec_t*		rec;
	rec_t*		ins_rec;
	rec_t*		next_rec;
	ulint		fold;
	ulint		ins_fold;
	ulint		next_fold = 0; /* remove warning (??? bug ???) */
	ulint		n_fields;
	ulint		n_bytes;
	ibool		left_side;
	ibool		locked		= FALSE;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	table = btr_search_get_hash_table(cursor->index);

	btr_search_check_free_space_in_heap(cursor->index);

	rec = btr_cur_get_rec(cursor);

	block = btr_cur_get_block(cursor);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	index = block->index;

	if (!index) {

		return;
	}

	ut_a(index == cursor->index);
	ut_a(!dict_index_is_ibuf(index));

	n_fields = block->curr_n_fields;
	n_bytes = block->curr_n_bytes;
	left_side = block->curr_left_side;

	ins_rec = page_rec_get_next(rec);
	next_rec = page_rec_get_next(ins_rec);

	offsets = rec_get_offsets(ins_rec, index, offsets,
				  ULINT_UNDEFINED, &heap);
	ins_fold = rec_fold(ins_rec, offsets, n_fields, n_bytes, index->id);

	if (!page_rec_is_supremum(next_rec)) {
		offsets = rec_get_offsets(next_rec, index, offsets,
					  n_fields + (n_bytes > 0), &heap);
		next_fold = rec_fold(next_rec, offsets, n_fields,
				     n_bytes, index->id);
	}

	if (!page_rec_is_infimum(rec)) {
		offsets = rec_get_offsets(rec, index, offsets,
					  n_fields + (n_bytes > 0), &heap);
		fold = rec_fold(rec, offsets, n_fields, n_bytes, index->id);
	} else {
		if (left_side) {

			rw_lock_x_lock(btr_search_get_latch(index));

			locked = TRUE;

			if (!btr_search_enabled) {
				goto function_exit;
			}

			ha_insert_for_fold(table, ins_fold, block, ins_rec);
		}

		goto check_next_rec;
	}

	if (fold != ins_fold) {

		if (!locked) {

			rw_lock_x_lock(btr_search_get_latch(index));

			locked = TRUE;

			if (!btr_search_enabled) {
				goto function_exit;
			}
		}

		if (!left_side) {
			ha_insert_for_fold(table, fold, block, rec);
		} else {
			ha_insert_for_fold(table, ins_fold, block, ins_rec);
		}
	}

check_next_rec:
	if (page_rec_is_supremum(next_rec)) {

		if (!left_side) {

			if (!locked) {
				rw_lock_x_lock(btr_search_get_latch(index));

				locked = TRUE;

				if (!btr_search_enabled) {
					goto function_exit;
				}
			}

			ha_insert_for_fold(table, ins_fold, block, ins_rec);
		}

		goto function_exit;
	}

	if (ins_fold != next_fold) {

		if (!locked) {

			rw_lock_x_lock(btr_search_get_latch(index));

			locked = TRUE;

			if (!btr_search_enabled) {
				goto function_exit;
			}
		}

		if (!left_side) {

			ha_insert_for_fold(table, ins_fold, block, ins_rec);
			/*
			fputs("Hash insert for ", stderr);
			dict_index_name_print(stderr, index);
			fprintf(stderr, " fold %lu\n", ins_fold);
			*/
		} else {
			ha_insert_for_fold(table, next_fold, block, next_rec);
		}
	}

function_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	if (locked) {
		rw_lock_x_unlock(btr_search_get_latch(index));
	}
}

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/********************************************************************//**
Validates the search system.
@return	TRUE if ok */
UNIV_INTERN
ibool
btr_search_validate(void)
/*=====================*/
{
	ha_node_t*	node;
	ulint		n_page_dumps	= 0;
	ibool		ok		= TRUE;
	ulint		i,j;
	ulint		cell_count;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;

	/* How many cells to check before temporarily releasing
	btr_search_latch. */
	ulint		chunk_size = 10000;

	rec_offs_init(offsets_);

	btr_search_x_lock_all();
	buf_pool_page_hash_x_lock_all();

	for (j = 0; j < btr_search_index_num; j++) {

	cell_count = hash_get_n_cells(btr_search_sys->hash_tables[j]);

	for (i = 0; i < cell_count; i++) {
		/* We release btr_search_latch every once in a while to
		give other queries a chance to run. */
		if ((i != 0) && ((i % chunk_size) == 0)) {
			buf_pool_page_hash_x_unlock_all();
			btr_search_x_unlock_all();
			os_thread_yield();
			btr_search_x_lock_all();
			buf_pool_page_hash_x_lock_all();
		}

		node = hash_get_nth_cell(btr_search_sys->hash_tables[j], i)->node;

		for (; node != NULL; node = node->next) {
			const buf_block_t*	block
				= buf_block_align(node->data);
			const buf_block_t*	hash_block;
			buf_pool_t*		buf_pool;
			index_id_t		page_index_id;

			buf_pool = buf_pool_from_bpage((buf_page_t*) block);

			if (UNIV_LIKELY(buf_block_get_state(block)
					== BUF_BLOCK_FILE_PAGE)) {

				/* The space and offset are only valid
				for file blocks.  It is possible that
				the block is being freed
				(BUF_BLOCK_REMOVE_HASH, see the
				assertion and the comment below) */
				hash_block = buf_block_hash_get(
					buf_pool,
					buf_block_get_space(block),
					buf_block_get_page_no(block));
			} else {
				hash_block = NULL;
			}

			if (hash_block) {
				ut_a(hash_block == block);
			} else {
				/* When a block is being freed,
				buf_LRU_search_and_free_block() first
				removes the block from
				buf_pool->page_hash by calling
				buf_LRU_block_remove_hashed_page().
				After that, it invokes
				btr_search_drop_page_hash_index() to
				remove the block from
				btr_search_sys->hash_index. */

				ut_a(buf_block_get_state(block)
				     == BUF_BLOCK_REMOVE_HASH);
			}

			ut_a(!dict_index_is_ibuf(block->index));

			page_index_id = btr_page_get_index_id(block->frame);

			offsets = rec_get_offsets(node->data,
						  block->index, offsets,
						  block->curr_n_fields
						  + (block->curr_n_bytes > 0),
						  &heap);

			if (!block->index || node->fold
			    != rec_fold(node->data,
					offsets,
					block->curr_n_fields,
					block->curr_n_bytes,
					page_index_id)) {
				const page_t*	page = block->frame;

				ok = FALSE;
				ut_print_timestamp(stderr);

				fprintf(stderr,
					"  InnoDB: Error in an adaptive hash"
					" index pointer to page %lu\n"
					"InnoDB: ptr mem address %p"
					" index id %llu,"
					" node fold %lu, rec fold %lu\n",
					(ulong) page_get_page_no(page),
					node->data,
					(ullint) page_index_id,
					(ulong) node->fold,
					(ulong) rec_fold(node->data,
							 offsets,
							 block->curr_n_fields,
							 block->curr_n_bytes,
							 page_index_id));

				fputs("InnoDB: Record ", stderr);
				rec_print_new(stderr, node->data, offsets);
				fprintf(stderr, "\nInnoDB: on that page."
					" Page mem address %p, is hashed %p,"
					" n fields %lu, n bytes %lu\n"
					"InnoDB: side %lu\n",
					(void*) page, (void*) block->index,
					(ulong) block->curr_n_fields,
					(ulong) block->curr_n_bytes,
					(ulong) block->curr_left_side);

				if (n_page_dumps < 20) {
					buf_page_print(
						page, 0,
						BUF_PAGE_PRINT_NO_CRASH);
					n_page_dumps++;
				}
			}
		}
	}

	for (i = 0; i < cell_count; i += chunk_size) {
		ulint end_index = ut_min(i + chunk_size - 1, cell_count - 1);

		/* We release btr_search_latch every once in a while to
		give other queries a chance to run. */
		if (i != 0) {
			buf_pool_page_hash_x_unlock_all();
			btr_search_x_unlock_all();
			os_thread_yield();
			btr_search_x_lock_all();
			buf_pool_page_hash_x_lock_all();
		}

		if (!ha_validate(btr_search_sys->hash_tables[j], i, end_index)) {
			ok = FALSE;
		}
	}

	} /*for (j = 0; j < btr_search_index_num; j++)*/

	buf_pool_page_hash_x_unlock_all();
	btr_search_x_unlock_all();
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	return(ok);
}
#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */
