/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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

/********************************************************************//**
@file btr/btr0sea.cc
The index tree adaptive search

Created 2/17/1996 Heikki Tuuri
*************************************************************************/

#include "btr0sea.h"

#include <sys/types.h>

#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0pcur.h"
#include "buf0buf.h"
#include "ha0ha.h"
#include "my_compiler.h"
#include "my_inttypes.h"
#include "page0cur.h"
#include "page0page.h"
#include "srv0mon.h"
#include "sync0sync.h"

/** Is search system enabled.
Search system is protected by array of latches. */
bool		btr_search_enabled	= true;

/** Number of adaptive hash index partition. */
ulong		btr_ahi_parts		= 8;

#ifdef UNIV_SEARCH_PERF_STAT
/** Number of successful adaptive hash index lookups */
ulint		btr_search_n_succ	= 0;
/** Number of failed adaptive hash index lookups */
ulint		btr_search_n_hash_fail	= 0;
#endif /* UNIV_SEARCH_PERF_STAT */

/** padding to prevent other memory update
hotspots from residing on the same memory
cache line as btr_search_latches */
byte		btr_sea_pad1[64];

/** The latches protecting the adaptive search system: this latches protects the
(1) positions of records on those pages where a hash index has been built.
NOTE: It does not protect values of non-ordering fields within a record from
being updated in-place! We can use fact (1) to perform unique searches to
indexes. We will allocate the latches from dynamic memory to get it to the
same DRAM page as other hotspot semaphores */
rw_lock_t**	btr_search_latches;

/** padding to prevent other memory update hotspots from residing on
the same memory cache line */
byte		btr_sea_pad2[64];

/** The adaptive hash index */
btr_search_sys_t*	btr_search_sys;

/** If the number of records on the page divided by this parameter
would have been successfully accessed using a hash index, the index
is then built on the page, assuming the global limit has been reached */
#define BTR_SEARCH_PAGE_BUILD_LIMIT	16

/** The global limit for consecutive potentially successful hash searches,
before hash index building is started */
#define BTR_SEARCH_BUILD_LIMIT		100

/** Compute the hash value of an index identifier.
@param[in]	space_id	tablespace identifier
@param[in]	index_id	index identifier
@return hash value */
static
ulint
btr_search_fold_index_id(
	uint32_t	space_id,
	space_index_t	index_id)
{
	return(ut_fold_ulint_pair(ut_fold_ull(index_id), space_id));
}

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/** Compute the hash value of an index identifier.
@param[in]	id	index identifier
@return hash value */
static
ulint
btr_search_fold_index_id(
	const index_id_t&	id)
{
	return(btr_search_fold_index_id(id.m_space_id, id.m_index_id));
}
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

/** Determine the number of accessed key fields.
@param[in]	n_fields	number of complete fields
@param[in]	n_bytes		number of bytes in an incomplete last field
@return	number of complete or incomplete fields */
inline MY_ATTRIBUTE((warn_unused_result))
ulint
btr_search_get_n_fields(
	ulint	n_fields,
	ulint	n_bytes)
{
	return(n_fields + (n_bytes > 0 ? 1 : 0));
}

/** Determine the number of accessed key fields.
@param[in]	cursor		b-tree cursor
@return	number of complete or incomplete fields */
inline MY_ATTRIBUTE((warn_unused_result))
ulint
btr_search_get_n_fields(
	const btr_cur_t*	cursor)
{
	return(btr_search_get_n_fields(cursor->n_fields, cursor->n_bytes));
}

/** Builds a hash index on a page with the given parameters. If the page
already has a hash index with different parameters, the old hash index is
removed. If index is non-NULL, this function checks if n_fields and n_bytes
are sensible values, and does not build a hash index if not.
@param[in]	index		index for which to build, or NULL if not known
@param[in]	block		index page, s- or x-latched
@param[in]	n_fields	hash this many full fields
@param[in]	n_bytes		hash this many bytes from the next field
@param[in]	left_side	hash for searches from left side */
static
void
btr_search_build_page_hash_index(
	dict_index_t*	index,
	buf_block_t*	block,
	ulint		n_fields,
	ulint		n_bytes,
	ibool		left_side);

/** This function should be called before reserving any btr search mutex, if
the intended operation might add nodes to the search system hash table.
Because of the latching order, once we have reserved the btr search system
latch, we cannot allocate a free frame from the buffer pool. Checks that
there is a free buffer frame allocated for hash table heap in the btr search
system. If not, allocates a free frames for the heap. This check makes it
probable that, when have reserved the btr search system latch and we need to
allocate a new node to the hash table, it will succeed. However, the check
will not guarantee success.
@param[in]	index	index handler */
static
void
btr_search_check_free_space_in_heap(dict_index_t* index)
{
	hash_table_t*	table;
	mem_heap_t*	heap;

	ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_S));
	ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));

	table = btr_get_search_table(index);

	heap = table->heap;

	/* Note that we peek the value of heap->free_block without reserving
	the latch: this is ok, because we will not guarantee that there will
	be enough free space in the hash table. */

	if (heap->free_block == NULL) {
		buf_block_t*	block = buf_block_alloc(NULL);

		btr_search_x_lock(index);

		if (btr_search_enabled
		    && heap->free_block == NULL) {
			heap->free_block = block;
		} else {
			buf_block_free(block);
		}

		btr_search_x_unlock(index);
	}
}

/** Creates and initializes the adaptive search system at a database start.
@param[in]	hash_size	hash table size. */
void
btr_search_sys_create(ulint hash_size)
{
	/* Search System is divided into n parts.
	Each part controls access to distinct set of hash buckets from
	hash table through its own latch. */

	/* Step-1: Allocate latches (1 per part). */
	btr_search_latches = reinterpret_cast<rw_lock_t**>(
		ut_malloc(sizeof(rw_lock_t*) * btr_ahi_parts, mem_key_ahi));

	for (ulint i = 0; i < btr_ahi_parts; ++i) {

		btr_search_latches[i] = reinterpret_cast<rw_lock_t*>(
			ut_malloc(sizeof(rw_lock_t), mem_key_ahi));

		rw_lock_create(btr_search_latch_key,
			       btr_search_latches[i], SYNC_SEARCH_SYS);
	}

	/* Step-2: Allocate hash tablees. */
	btr_search_sys = reinterpret_cast<btr_search_sys_t*>(
		ut_malloc(sizeof(btr_search_sys_t), mem_key_ahi));

	btr_search_sys->hash_tables = reinterpret_cast<hash_table_t**>(
		ut_malloc(sizeof(hash_table_t*) * btr_ahi_parts, mem_key_ahi));

	for (ulint i = 0; i < btr_ahi_parts; ++i) {

		btr_search_sys->hash_tables[i] =
			ib_create((hash_size / btr_ahi_parts),
				  LATCH_ID_HASH_TABLE_MUTEX,
				  0, MEM_HEAP_FOR_BTR_SEARCH);

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
		btr_search_sys->hash_tables[i]->adaptive = TRUE;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	}
}

/** Resize hash index hash table.
@param[in]	hash_size	hash index hash table size */
void
btr_search_sys_resize(ulint hash_size)
{
	/* Step-1: Lock all search latches in exclusive mode. */
	btr_search_x_lock_all();

	if (btr_search_enabled) {

		btr_search_x_unlock_all();

		ib::error() << "btr_search_sys_resize failed because"
			" hash index hash table is not empty.";
		ut_ad(0);
		return;
	}

	/* Step-2: Recreate hash tables with new size. */
	for (ulint i = 0; i < btr_ahi_parts; ++i) {

		mem_heap_free(btr_search_sys->hash_tables[i]->heap);
		hash_table_free(btr_search_sys->hash_tables[i]);

		btr_search_sys->hash_tables[i] =
			ib_create((hash_size / btr_ahi_parts),
				  LATCH_ID_HASH_TABLE_MUTEX,
				  0, MEM_HEAP_FOR_BTR_SEARCH);

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
		btr_search_sys->hash_tables[i]->adaptive = TRUE;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	}

	/* Step-3: Unlock all search latches from exclusive mode. */
	btr_search_x_unlock_all();
}

/** Frees the adaptive search system at a database shutdown. */
void
btr_search_sys_free()
{
	if (btr_search_sys == NULL) {
		ut_ad(btr_search_latches == NULL);
		return;
	}

	ut_ad(btr_search_latches != NULL);

	/* Step-1: Release the hash tables. */
	for (ulint i = 0; i < btr_ahi_parts; ++i) {

		mem_heap_free(btr_search_sys->hash_tables[i]->heap);
		hash_table_free(btr_search_sys->hash_tables[i]);

	}

	ut_free(btr_search_sys->hash_tables);
	ut_free(btr_search_sys);
	btr_search_sys = NULL;

	/* Step-2: Release all allocates latches. */
	for (ulint i = 0; i < btr_ahi_parts; ++i) {

		rw_lock_free(btr_search_latches[i]);
		ut_free(btr_search_latches[i]);
	}

	ut_free(btr_search_latches);
	btr_search_latches = NULL;
}

/** Set index->ref_count = 0 on all indexes of a table.
@param[in,out]	table	table handler */
static
void
btr_search_disable_ref_count(
	dict_table_t*	table)
{
	dict_index_t*	index;

	ut_ad(mutex_own(&dict_sys->mutex));

	for (index = table->first_index();
	     index != NULL;
	     index = index->next()) {

		ut_ad(rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));

		index->search_info->ref_count = 0;
	}
}

/** Disable the adaptive hash search system and empty the index.
@param[in]	need_mutex	need to acquire dict_sys->mutex */
void
btr_search_disable(
	bool	need_mutex)
{
	dict_table_t*	table;

	if (need_mutex) {
		mutex_enter(&dict_sys->mutex);
	}

	ut_ad(mutex_own(&dict_sys->mutex));
	btr_search_x_lock_all();

	if (!btr_search_enabled) {
		if (need_mutex) {
			mutex_exit(&dict_sys->mutex);
		}

		btr_search_x_unlock_all();
		return;
	}

	btr_search_enabled = false;

	/* Clear the index->search_info->ref_count of every index in
	the data dictionary cache. */
	for (table = UT_LIST_GET_FIRST(dict_sys->table_LRU); table;
	     table = UT_LIST_GET_NEXT(table_LRU, table)) {

		btr_search_disable_ref_count(table);
	}

	for (table = UT_LIST_GET_FIRST(dict_sys->table_non_LRU); table;
	     table = UT_LIST_GET_NEXT(table_LRU, table)) {

		btr_search_disable_ref_count(table);
	}

	if (need_mutex) {
		mutex_exit(&dict_sys->mutex);
	}

	/* Set all block->index = NULL. */
	buf_pool_clear_hash_index();

	/* Clear the adaptive hash index. */
	for (ulint i = 0; i < btr_ahi_parts; ++i) {
		hash_table_clear(btr_search_sys->hash_tables[i]);
		mem_heap_empty(btr_search_sys->hash_tables[i]->heap);
	}

	btr_search_x_unlock_all();
}

/** Enable the adaptive hash search system. */
void
btr_search_enable()
{
	os_rmb;
	/* Don't allow enabling AHI if buffer pool resize is hapenning.
	Ignore it sliently.  */
	if (srv_buf_pool_old_size != srv_buf_pool_size)
		return;

	btr_search_x_lock_all();
	btr_search_enabled = true;
	btr_search_x_unlock_all();
}

/** Creates and initializes a search info struct.
@param[in]	heap		heap where created.
@return own: search info struct */
btr_search_t*
btr_search_info_create(mem_heap_t* heap)
{
	btr_search_t*	info;

	info = (btr_search_t*) mem_heap_alloc(heap, sizeof(btr_search_t));

	ut_d(info->magic_n = BTR_SEARCH_MAGIC_N);

	info->ref_count = 0;
	info->root_guess = NULL;
	info->withdraw_clock = 0;

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

/** Returns the value of ref_count. The value is protected by latch.
@param[in]	info		search info
@param[in]	index		index identifier
@return ref_count value. */
ulint
btr_search_info_get_ref_count(
	const btr_search_t*	info,
	const dict_index_t*	index)
{
	ulint ret = 0;

	if (!btr_search_enabled) {
		return(ret);
	}

	ut_ad(info);

	ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_S));
	ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));

	btr_search_s_lock(index);
	ret = info->ref_count;
	btr_search_s_unlock(index);

	return(ret);
}

/** Updates the search info of an index about hash successes. NOTE that info
is NOT protected by any semaphore, to save CPU time! Do not assume its fields
are consistent.
@param[in,out]	info	search info
@param[in]	cursor	cursor which was just positioned */
static
void
btr_search_info_update_hash(
	btr_search_t*	info,
	btr_cur_t*	cursor)
{
	dict_index_t*	index = cursor->index;
	ulint		n_unique;
	int		cmp;

	ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_S));
	ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));

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

/** Update the block search info on hash successes. NOTE that info and
block->n_hash_helps, n_fields, n_bytes, left_side are NOT protected by any
semaphore, to save CPU time! Do not assume the fields are consistent.
@return TRUE if building a (new) hash index on the block is recommended
@param[in,out]	info	search info
@param[in,out]	block	buffer block
@param[in]	cursor	cursor */
static
ibool
btr_search_update_block_hash_info(
	btr_search_t*		info,
	buf_block_t*		block,
	const btr_cur_t*	cursor)
{
	ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_S));
	ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_X));
	ut_ad(rw_lock_own(&block->lock, RW_LOCK_S)
	      || rw_lock_own(&block->lock, RW_LOCK_X));

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

/** Updates a hash node reference when it has been unsuccessfully used in a
search which could have succeeded with the used hash parameters. This can
happen because when building a hash index for a page, we do not check
what happens at page boundaries, and therefore there can be misleading
hash nodes. Also, collisions in the fold value can lead to misleading
references. This function lazily fixes these imperfections in the hash
index.
@param[in]	info	search info
@param[in]	block	buffer block where cursor positioned
@param[in]	cursor	cursor */
static
void
btr_search_update_hash_ref(
	const btr_search_t*	info,
	buf_block_t*		block,
	const btr_cur_t*	cursor)
{
	dict_index_t*	index;
	ulint		fold;
	const rec_t*	rec;

	ut_ad(cursor->flag == BTR_CUR_HASH_FAIL);
	ut_ad(rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_X));
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_S)
	      || rw_lock_own(&(block->lock), RW_LOCK_X));
	ut_ad(page_align(btr_cur_get_rec(cursor))
	      == buf_block_get_frame(block));
	assert_block_ahi_valid(block);

	index = block->index;

	if (!index) {

		return;
	}

	ut_ad(block->page.id.space() == index->space);
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
				block->curr_n_bytes,
				btr_search_fold_index_id(
					index->space, index->id));
		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
		ut_ad(rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));

		ha_insert_for_fold(btr_get_search_table(index), fold,
				   block, rec);

		MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_ADDED);
	}
}

/** Updates the search info.
@param[in,out]	info	search info
@param[in]	cursor	cursor which was just positioned */
void
btr_search_info_update_slow(
	btr_search_t*	info,
	btr_cur_t*	cursor)
{
	buf_block_t*	block;
	ibool		build_index;

	ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_S));
	ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_X));

	block = btr_cur_get_block(cursor);

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

		btr_search_x_lock(cursor->index);

		btr_search_update_hash_ref(info, block, cursor);

		btr_search_x_unlock(cursor->index);
	}

	if (build_index) {
		/* Note that since we did not protect block->n_fields etc.
		with any semaphore, the values can be inconsistent. We have
		to check inside the function call that they make sense. */
		btr_search_build_page_hash_index(cursor->index, block,
						 block->n_fields,
						 block->n_bytes,
						 block->left_side);
	}
}

/** Checks if a guessed position for a tree cursor is right. Note that if
mode is PAGE_CUR_LE, which is used in inserts, and the function returns
TRUE, then cursor->up_match and cursor->low_match both have sensible values.
@param[in,out]	cursor		guess cursor position
@param[in]	can_only_compare_to_cursor_rec
				if we do not have a latch on the page of cursor,
				but a latch corresponding search system, then
				ONLY the columns of the record UNDER the cursor
				are protected, not the next or previous record
				in the chain: we cannot look at the next or
				previous record to check our guess!
@param[in]	tuple		data tuple
@param[in]	mode		PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G, PAGE_CUR_GE
@param[in]	mtr		mini transaction
@return TRUE if success */
static
ibool
btr_search_check_guess(
	btr_cur_t*	cursor,
	ibool		can_only_compare_to_cursor_rec,
	const dtuple_t*	tuple,
	ulint		mode,
	mtr_t*		mtr)
{
	rec_t*		rec;
	ulint		n_unique;
	ulint		match;
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

	offsets = rec_get_offsets(rec, cursor->index, offsets,
				  n_unique, &heap);
	cmp = cmp_dtuple_rec_with_match(tuple, rec, cursor->index, offsets,
					&match);

	if (mode == PAGE_CUR_GE) {
		if (cmp > 0) {
			goto exit_func;
		}

		cursor->up_match = match;

		if (match >= n_unique) {
			success = TRUE;
			goto exit_func;
		}
	} else if (mode == PAGE_CUR_LE) {
		if (cmp < 0) {
			goto exit_func;
		}

		cursor->low_match = match;

	} else if (mode == PAGE_CUR_G) {
		if (cmp >= 0) {
			goto exit_func;
		}
	} else if (mode == PAGE_CUR_L) {
		if (cmp <= 0) {
			goto exit_func;
		}
	}

	if (can_only_compare_to_cursor_rec) {
		/* Since we could not determine if our guess is right just by
		looking at the record under the cursor, return FALSE */
		goto exit_func;
	}

	match = 0;

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
		cmp = cmp_dtuple_rec_with_match(
			tuple, prev_rec, cursor->index, offsets, &match);
		if (mode == PAGE_CUR_GE) {
			success = cmp > 0;
		} else {
			success = cmp >= 0;
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
		cmp = cmp_dtuple_rec_with_match(
			tuple, next_rec, cursor->index, offsets, &match);
		if (mode == PAGE_CUR_LE) {
			success = cmp < 0;
			cursor->up_match = match;
		} else {
			success = cmp <= 0;
		}
	}
exit_func:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	return(success);
}

static
void
btr_search_failure(btr_search_t* info, btr_cur_t* cursor)
{
	cursor->flag = BTR_CUR_HASH_FAIL;

#ifdef UNIV_SEARCH_PERF_STAT
	++info->n_hash_fail;

	if (info->n_hash_succ > 0) {
		--info->n_hash_succ;
	}
#endif /* UNIV_SEARCH_PERF_STAT */

	info->last_hash_succ = FALSE;
}

/** Tries to guess the right search position based on the hash search info
of the index. Note that if mode is PAGE_CUR_LE, which is used in inserts,
and the function returns TRUE, then cursor->up_match and cursor->low_match
both have sensible values.
@param[in,out]	index		index
@param[in,out]	info		index search info
@param[in]	tuple		logical record
@param[in]	mode		PAGE_CUR_L, ....
@param[in]	latch_mode	BTR_SEARCH_LEAF, ...;
				NOTE that only if has_search_latch is 0, we will
				have a latch set on the cursor page, otherwise
				we assume the caller uses his search latch
				to protect the record!
@param[out]	cursor		tree cursor
@param[in]	has_search_latch
				latch mode the caller currently has on
				search system: RW_S/X_LATCH or 0
@param[in]	mtr		mini transaction
@return TRUE if succeeded */
ibool
btr_search_guess_on_hash(
	dict_index_t*	index,
	btr_search_t*	info,
	const dtuple_t*	tuple,
	ulint		mode,
	ulint		latch_mode,
	btr_cur_t*	cursor,
	ulint		has_search_latch,
	mtr_t*		mtr)
{
	const rec_t*	rec;
	ulint		fold;
#ifdef notdefined
	btr_cur_t	cursor2;
	btr_pcur_t	pcur;
#endif

	if (!btr_search_enabled) {
		return(FALSE);
	}

	ut_ad(index && info && tuple && cursor && mtr);
	ut_ad(!dict_index_is_ibuf(index));
	ut_ad((latch_mode == BTR_SEARCH_LEAF)
	      || (latch_mode == BTR_MODIFY_LEAF));

	/* Not supported for spatial index */
	ut_ad(!dict_index_is_spatial(index));

	/* Note that, for efficiency, the struct info may not be protected by
	any latch here! */

	if (info->n_hash_potential == 0) {

		return(FALSE);
	}

	cursor->n_fields = info->n_fields;
	cursor->n_bytes = info->n_bytes;

	if (dtuple_get_n_fields(tuple) < btr_search_get_n_fields(cursor)) {

		return(FALSE);
	}

#ifdef UNIV_SEARCH_PERF_STAT
	info->n_hash_succ++;
#endif
	fold = dtuple_fold(tuple, cursor->n_fields, cursor->n_bytes,
			   btr_search_fold_index_id(index->space, index->id));

	cursor->fold = fold;
	cursor->flag = BTR_CUR_HASH;

	if (!has_search_latch) {
		btr_search_s_lock(index);

		if (!btr_search_enabled) {
			btr_search_s_unlock(index);

			btr_search_failure(info, cursor);

			return(FALSE);
		}
	}

	ut_ad(rw_lock_get_writer(btr_get_search_latch(index)) != RW_LOCK_X);
	ut_ad(rw_lock_get_reader_count(btr_get_search_latch(index)) > 0);

	rec = (rec_t*) ha_search_and_get_data(
			btr_get_search_table(index), fold);

	if (rec == NULL) {

		if (!has_search_latch) {
			btr_search_s_unlock(index);
		}

		btr_search_failure(info, cursor);

		return(FALSE);
	}

	buf_block_t*	block = buf_block_from_ahi(rec);

	if (!has_search_latch) {

		if (!buf_page_get_known_nowait(
			latch_mode, block, BUF_MAKE_YOUNG,
			__FILE__, __LINE__, mtr)) {

			if (!has_search_latch) {
				btr_search_s_unlock(index);
			}

			btr_search_failure(info, cursor);

			return(FALSE);
		}

		btr_search_s_unlock(index);

		buf_block_dbg_add_level(block, SYNC_TREE_NODE_FROM_HASH);
	}

	if (buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE) {

		ut_ad(buf_block_get_state(block) == BUF_BLOCK_REMOVE_HASH);

		if (!has_search_latch) {

			btr_leaf_page_release(block, latch_mode, mtr);
		}

		btr_search_failure(info, cursor);

		return(FALSE);
	}

	ut_ad(page_rec_is_user_rec(rec));

	btr_cur_position(index, (rec_t*) rec, block, cursor);

	/* Check the validity of the guess within the page */

	/* If we only have the latch on search system, not on the
	page, it only protects the columns of the record the cursor
	is positioned on. We cannot look at the next of the previous
	record to determine if our guess for the cursor position is
	right. */
	if (index->space != block->page.id.space()
	    || index->id != btr_page_get_index_id(block->frame)
	    || !btr_search_check_guess(cursor,
				       has_search_latch,
				       tuple, mode, mtr)) {

		if (!has_search_latch) {
			btr_leaf_page_release(block, latch_mode, mtr);
		}

		btr_search_failure(info, cursor);

		return(FALSE);
	}

	if (info->n_hash_potential < BTR_SEARCH_BUILD_LIMIT + 5) {

		info->n_hash_potential++;
	}

#ifdef notdefined
	/* These lines of code can be used in a debug version to check
	the correctness of the searched cursor position: */

	info->last_hash_succ = FALSE;

	/* Currently, does not work if the following fails: */
	ut_ad(!has_search_latch);

	btr_leaf_page_release(block, latch_mode, mtr);

	btr_cur_search_to_nth_level(
		index, 0, tuple, mode, latch_mode, &cursor2, 0, mtr);

	if (mode == PAGE_CUR_GE
	    && page_rec_is_supremum(btr_cur_get_rec(&cursor2))) {

		/* If mode is PAGE_CUR_GE, then the binary search
		in the index tree may actually take us to the supremum
		of the previous page */

		info->last_hash_succ = FALSE;

		btr_pcur_open_on_user_rec(
			index, tuple, mode, latch_mode, &pcur, mtr);

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
	if (!has_search_latch && buf_page_peek_if_too_old(&block->page)) {

		buf_page_make_young(&block->page);
	}

	/* Increment the page get statistics though we did not really
	fix the page: for user info only */

	{
		buf_pool_t*	buf_pool = buf_pool_from_bpage(&block->page);

		++buf_pool->stat.n_page_gets;
	}

	return(TRUE);
}

/** Drop any adaptive hash index entries that point to an index page.
@param[in,out]	block	block containing index page, s- or x-latched, or an
			index page for which we know that
			block->buf_fix_count == 0 or it is an index page which
			has already been removed from the buf_pool->page_hash
			i.e.: it is in state BUF_BLOCK_REMOVE_HASH */
void
btr_search_drop_page_hash_index(buf_block_t* block)
{
	ulint			n_fields;
	ulint			n_bytes;
	const page_t*		page;
	const rec_t*		rec;
	ulint			fold;
	ulint			prev_fold;
	ulint			n_cached;
	ulint			n_recs;
	ulint*			folds;
	ulint			i;
	mem_heap_t*		heap;
	const dict_index_t*	index;
	ulint*			offsets;
	rw_lock_t*		latch;
	btr_search_t*		info;

retry:
	/* Do a dirty check on block->index, return if the block is
	not in the adaptive hash index. */
	index = block->index;
	/* This debug check uses a dirty read that could theoretically cause
	false positives while buf_pool_clear_hash_index() is executing. */
	assert_block_ahi_valid(block);

	if (index == NULL) {
		return;
	}

	ut_ad(block->page.buf_fix_count == 0
	      || buf_block_get_state(block) == BUF_BLOCK_REMOVE_HASH
	      || rw_lock_own(&block->lock, RW_LOCK_S)
	      || rw_lock_own(&block->lock, RW_LOCK_X));

	/* We must not dereference index here, because it could be freed
	if (index->table->n_ref_count == 0 && !mutex_own(&dict_sys->mutex)).
	Determine the ahi_slot based on the block contents. */

	const space_index_t	index_id
		= btr_page_get_index_id(block->frame);
	const ulint		ahi_slot
		= ut_fold_ulint_pair(static_cast<ulint>(index_id),
				     static_cast<ulint>(block->page.id.space()))
		% btr_ahi_parts;
	latch = btr_search_latches[ahi_slot];

	ut_ad(!btr_search_own_any(RW_LOCK_S));
	ut_ad(!btr_search_own_any(RW_LOCK_X));

	rw_lock_s_lock(latch);
	assert_block_ahi_valid(block);

	if (block->index == NULL) {
		rw_lock_s_unlock(latch);
		return;
	}

	/* The index associated with a block must remain the
	same, because we are holding block->lock or the block is
	not accessible by other threads (BUF_BLOCK_REMOVE_HASH),
	or the index is not accessible to other threads
	(buf_fix_count == 0 when DROP TABLE or similar is executing
	buf_LRU_drop_page_hash_for_tablespace()). */
	ut_a(index == block->index);
	ut_ad(!index->disable_ahi);
	ut_ad(btr_search_enabled);

	ut_ad(block->page.id.space() == index->space);
	ut_a(index_id == index->id);
	ut_a(!dict_index_is_ibuf(index));
#ifdef UNIV_DEBUG
	switch (dict_index_get_online_status(index)) {
	case ONLINE_INDEX_CREATION:
		/* The index is being created (bulk loaded). */
	case ONLINE_INDEX_COMPLETE:
		/* The index has been published. */
	case ONLINE_INDEX_ABORTED:
		/* Either the index creation was aborted due to an
		error observed by InnoDB (in which case there should
		not be any adaptive hash index entries), or it was
		completed and then flagged aborted in
		rollback_inplace_alter_table(). */
	case ONLINE_INDEX_ABORTED_DROPPED:
		/* Since dropping the indexes are delayed to post_ddl,
		this status is similar to ONLINE_INDEX_ABORTED. */
		break;
	default:
		ut_error;
	}
#endif /* UNIV_DEBUG */

	n_fields = block->curr_n_fields;
	n_bytes = block->curr_n_bytes;

	/* NOTE: The AHI fields of block must not be accessed after
	releasing search latch, as the index page might only be s-latched! */

	rw_lock_s_unlock(latch);

	ut_a(n_fields > 0 || n_bytes > 0);

	page = block->frame;
	n_recs = page_get_n_recs(page);

	/* Calculate and cache fold values into an array for fast deletion
	from the hash index */

	folds = (ulint*) ut_malloc_nokey(n_recs * sizeof(ulint));

	n_cached = 0;

	rec = page_get_infimum_rec(page);
	rec = page_rec_get_next_low(rec, page_is_comp(page));

	const ulint	index_fold = btr_search_fold_index_id(
		block->page.id.space(), index_id);

	prev_fold = 0;

	heap = NULL;
	offsets = NULL;

	while (!page_rec_is_supremum(rec)) {
		offsets = rec_get_offsets(
			rec, index, offsets,
			btr_search_get_n_fields(n_fields, n_bytes), &heap);
		fold = rec_fold(rec, offsets, n_fields, n_bytes, index_fold);

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

	rw_lock_x_lock(latch);

	if (UNIV_UNLIKELY(!block->index)) {
		/* Someone else has meanwhile dropped the hash index */

		goto cleanup;
	}

	ut_a(block->index == index);

	if (block->curr_n_fields != n_fields
	    || block->curr_n_bytes != n_bytes) {

		/* Someone else has meanwhile built a new hash index on the
		page, with different parameters */

		rw_lock_x_unlock(latch);

		ut_free(folds);
		goto retry;
	}

	for (i = 0; i < n_cached; i++) {

		ha_remove_all_nodes_to_page(
			btr_search_sys->hash_tables[ahi_slot],
			folds[i], page);
	}

	info = btr_search_get_info(block->index);
	ut_a(info->ref_count > 0);
	info->ref_count--;

	block->index = NULL;

	MONITOR_INC(MONITOR_ADAPTIVE_HASH_PAGE_REMOVED);
	MONITOR_INC_VALUE(MONITOR_ADAPTIVE_HASH_ROW_REMOVED, n_cached);

cleanup:
	assert_block_ahi_valid(block);
	rw_lock_x_unlock(latch);

	ut_free(folds);
}

/** Drop any adaptive hash index entries that may point to an index
page that may be in the buffer pool, when a page is evicted from the
buffer pool or freed in a file segment.
@param[in]	page_id		page id
@param[in]	page_size	page size */
void
btr_search_drop_page_hash_when_freed(
	const page_id_t&	page_id,
	const page_size_t&	page_size)
{
	buf_block_t*	block;
	mtr_t		mtr;

	ut_d(export_vars.innodb_ahi_drop_lookups++);

	mtr_start(&mtr);

	/* If the caller has a latch on the page, then the caller must
	have a x-latch on the page and it must have already dropped
	the hash index for the page. Because of the x-latch that we
	are possibly holding, we cannot s-latch the page, but must
	(recursively) x-latch it, even though we are only reading. */

	block = buf_page_get_gen(page_id, page_size, RW_X_LATCH, NULL,
				 BUF_PEEK_IF_IN_POOL, __FILE__, __LINE__,
				 &mtr);

	if (block) {

		/* If AHI is still valid, page can't be in free state.
		AHI is dropped when page is freed. */
		ut_ad(!block->page.file_page_was_freed);

		buf_block_dbg_add_level(block, SYNC_TREE_NODE_FROM_HASH);

		dict_index_t*	index = block->index;
		if (index != NULL) {
			/* In all our callers, the table handle should
			be open, or we should be in the process of
			dropping the table (preventing eviction). */
			ut_ad(index->table->n_ref_count > 0
			      || mutex_own(&dict_sys->mutex));
			btr_search_drop_page_hash_index(block);
		}
	}

	mtr_commit(&mtr);
}

/** Drop any adaptive hash index entries for a table.
@param[in,out]	table	to drop indexes of this table */
void
btr_drop_ahi_for_table(dict_table_t* table)
{
	const ulint     len = UT_LIST_GET_LEN(table->indexes);

	if (len == 0) {
		return;
	}

	const dict_index_t*	indexes[MAX_INDEXES];
	static constexpr unsigned DROP_BATCH = 1024;

	page_id_t		drop[DROP_BATCH];
	const page_size_t	page_size(dict_table_page_size(table));

	for (;;) {
		ulint			ref_count	= 0;
		const dict_index_t**	end		= indexes;

		for (dict_index_t* index = table->first_index();
		     index != nullptr; index = index->next()) {
			if (ulint n_refs = index->search_info->ref_count) {
				ut_ad(!index->disable_ahi);
				ut_ad(index->is_committed());
				ref_count += n_refs;
				ut_ad(indexes + len > end);
				*end++ = index;
			}
		}

		ut_ad((indexes == end) == (ref_count == 0));

		if (ref_count == 0) {
			return;
		}

		for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
			unsigned n_drop = 0;

			buf_pool_t*     buf_pool = buf_pool_from_array(i);
			mutex_enter(&buf_pool->LRU_list_mutex);
			const buf_page_t* prev;

			for (const buf_page_t* bpage
				     = UT_LIST_GET_LAST(buf_pool->LRU);
			     bpage != nullptr; bpage = prev) {
				prev = UT_LIST_GET_PREV(LRU, bpage);

				ut_a(buf_page_in_file(bpage));

				if (buf_page_get_state(bpage)
				    != BUF_BLOCK_FILE_PAGE
				    || (bpage->io_fix != BUF_IO_NONE
					&& bpage->io_fix != BUF_IO_WRITE)
				    || bpage->buf_fix_count > 0) {
					continue;
				}

				const dict_index_t* index
					= reinterpret_cast<const buf_block_t*>(
						bpage)->index;
				if (index == nullptr) {
					continue;
				}

				if (std::search_n(indexes, end, 1, index)
				    != end) {
					drop[n_drop].copy_from(bpage->id);
					if (++n_drop == DROP_BATCH) {
						break;
					}
				}
			}

			mutex_exit(&buf_pool->LRU_list_mutex);

			for (unsigned i = 0; i < n_drop; ++i) {
				btr_search_drop_page_hash_when_freed(
					drop[i], page_size);
			}
		}

		os_thread_yield();
	}
}

/** Drop any adaptive hash index entries for a index.
@param[in,out]	index	to drop hash indexes for this index */
void
btr_drop_ahi_for_index(dict_index_t* index)
{
	ut_ad(index->is_committed());

	if (index->disable_ahi || index->search_info->ref_count == 0) {
		return;
	}

	static constexpr unsigned	DROP_BATCH = 1024;

	const dict_table_t*		table = index->table;
	page_id_t			drop[DROP_BATCH];
	const page_size_t		page_size(dict_table_page_size(table));

	while (true) {
		if (index->search_info->ref_count == 0) {
			return;
		}

		for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
			unsigned n_drop = 0;

			buf_pool_t*	buf_pool = buf_pool_from_array(i);
			mutex_enter(&buf_pool->LRU_list_mutex);
			const buf_page_t* prev;

			for (const buf_page_t* bpage
				= UT_LIST_GET_LAST(buf_pool->LRU);
			     bpage != nullptr; bpage = prev) {
				prev = UT_LIST_GET_PREV(LRU, bpage);

				ut_a(buf_page_in_file(bpage));

				if (buf_page_get_state(bpage)
				    != BUF_BLOCK_FILE_PAGE
				    || (bpage->io_fix != BUF_IO_NONE
					&& bpage->io_fix != BUF_IO_WRITE)
				    || bpage->buf_fix_count > 0) {
					continue;
				}

				const dict_index_t* block_index
					= reinterpret_cast<const buf_block_t*>(
						bpage)->index;
				if (block_index == nullptr
				    || block_index != index) {
					continue;
				}

				drop[n_drop].copy_from(bpage->id);
				if (++n_drop == DROP_BATCH) {
					break;
				}
			}

			mutex_exit(&buf_pool->LRU_list_mutex);

			for (unsigned i = 0; i < n_drop; ++i) {
				btr_search_drop_page_hash_when_freed(
					drop[i], page_size);
			}
		}

		os_thread_yield();
	}
}

/** Build a hash index on a page with the given parameters. If the page already
has a hash index with different parameters, the old hash index is removed.
If index is non-NULL, this function checks if n_fields and n_bytes are
sensible, and does not build a hash index if not.
@param[in,out]	index		index for which to build.
@param[in,out]	block		index page, s-/x- latched.
@param[in]	n_fields	hash this many full fields
@param[in]	n_bytes		hash this many bytes of the next field
@param[in]	left_side	hash for searches from left side */
static
void
btr_search_build_page_hash_index(
	dict_index_t*	index,
	buf_block_t*	block,
	ulint		n_fields,
	ulint		n_bytes,
	ibool		left_side)
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

	if (index->disable_ahi || !btr_search_enabled) {
		return;
	}

	rec_offs_init(offsets_);
	ut_ad(index);
	ut_ad(block->page.id.space() == index->space);
	ut_a(!dict_index_is_ibuf(index));

	ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_S)
	      || rw_lock_own(&(block->lock), RW_LOCK_X));

	btr_search_s_lock(index);

	table = btr_get_search_table(index);
	page = buf_block_get_frame(block);

	if (block->index && ((block->curr_n_fields != n_fields)
			     || (block->curr_n_bytes != n_bytes)
			     || (block->curr_left_side != left_side))) {

		btr_search_s_unlock(index);

		btr_search_drop_page_hash_index(block);
	} else {
		btr_search_s_unlock(index);
	}

	/* Check that the values for hash index build are sensible */

	if (n_fields == 0 && n_bytes == 0) {

		return;
	}

	if (dict_index_get_n_unique_in_tree(index)
	    < btr_search_get_n_fields(n_fields, n_bytes)) {
		return;
	}

	n_recs = page_get_n_recs(page);

	if (n_recs == 0) {

		return;
	}

	/* Calculate and cache fold values and corresponding records into
	an array for fast insertion to the hash index */

	folds = (ulint*) ut_malloc_nokey(n_recs * sizeof(ulint));
	recs = (rec_t**) ut_malloc_nokey(n_recs * sizeof(rec_t*));

	n_cached = 0;

	ut_a(index->id == btr_page_get_index_id(page));

	rec = page_rec_get_next(page_get_infimum_rec(page));

	offsets = rec_get_offsets(
		rec, index, offsets,
		btr_search_get_n_fields(n_fields, n_bytes),
		&heap);
	ut_ad(page_rec_is_supremum(rec)
	      || n_fields + (n_bytes > 0) == rec_offs_n_fields(offsets));

	const ulint	index_fold = btr_search_fold_index_id(
		block->page.id.space(), index->id);

	fold = rec_fold(rec, offsets, n_fields, n_bytes, index_fold);

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

		offsets = rec_get_offsets(
			next_rec, index, offsets,
			btr_search_get_n_fields(n_fields, n_bytes), &heap);
		next_fold = rec_fold(next_rec, offsets, n_fields, n_bytes,
				     index_fold);

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

	btr_search_x_lock(index);

	if (!btr_search_enabled) {
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
		assert_block_ahi_empty(block);
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

	MONITOR_INC(MONITOR_ADAPTIVE_HASH_PAGE_ADDED);
	MONITOR_INC_VALUE(MONITOR_ADAPTIVE_HASH_ROW_ADDED, n_cached);
exit_func:
	assert_block_ahi_valid(block);
	btr_search_x_unlock(index);

	ut_free(folds);
	ut_free(recs);
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/** Moves or deletes hash entries for moved records. If new_page is already
hashed, then the hash index for page, if any, is dropped. If new_page is not
hashed, and page is hashed, then a new hash index is built to new_page with the
same parameters as page (this often happens when a page is split).
@param[in,out]	new_block	records are copied to this page.
@param[in,out]	block		index page from which record are copied, and the
				copied records will be deleted from this page.
@param[in,out]	index		record descriptor */
void
btr_search_move_or_delete_hash_entries(
	buf_block_t*	new_block,
	buf_block_t*	block,
	dict_index_t*	index)
{
	/* AHI is disabled for intrinsic table as it depends on index-id
	which is dynamically assigned for intrinsic table indexes and not
	through a centralized index generator. */
	if (index->disable_ahi || !btr_search_enabled) {
		return;
	}

	ut_ad(!index->table->is_intrinsic());

	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));
	ut_ad(rw_lock_own(&(new_block->lock), RW_LOCK_X));

	btr_search_s_lock(index);

	ut_a(!new_block->index || new_block->index == index);
	ut_a(!block->index || block->index == index);
	ut_a(!(new_block->index || block->index)
	     || !dict_index_is_ibuf(index));
	assert_block_ahi_valid(block);
	assert_block_ahi_valid(new_block);

	if (new_block->index) {

		btr_search_s_unlock(index);

		btr_search_drop_page_hash_index(block);

		return;
	}

	if (block->index) {
		ulint	n_fields = block->curr_n_fields;
		ulint	n_bytes = block->curr_n_bytes;
		ibool	left_side = block->curr_left_side;

		new_block->n_fields = block->curr_n_fields;
		new_block->n_bytes = block->curr_n_bytes;
		new_block->left_side = left_side;

		btr_search_s_unlock(index);

		ut_a(n_fields > 0 || n_bytes > 0);

		btr_search_build_page_hash_index(
			index, new_block, n_fields, n_bytes, left_side);
		ut_ad(n_fields == block->curr_n_fields);
		ut_ad(n_bytes == block->curr_n_bytes);
		ut_ad(left_side == block->curr_left_side);
		return;
	}

	btr_search_s_unlock(index);
}

/** Updates the page hash index when a single record is deleted from a page.
@param[in]	cursor	cursor which was positioned on the record to delete
			using btr_cur_search_, the record is not yet deleted.*/
void
btr_search_update_hash_on_delete(btr_cur_t* cursor)
{
	hash_table_t*	table;
	buf_block_t*	block;
	const rec_t*	rec;
	ulint		fold;
	dict_index_t*	index;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	mem_heap_t*	heap		= NULL;
	rec_offs_init(offsets_);

	if (cursor->index->disable_ahi || !btr_search_enabled) {
		return;
	}

	block = btr_cur_get_block(cursor);

	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));

	assert_block_ahi_valid(block);
	index = block->index;

	if (!index) {

		return;
	}

	ut_ad(block->page.id.space() == index->space);
	ut_a(index == cursor->index);
	ut_a(block->curr_n_fields > 0 || block->curr_n_bytes > 0);
	ut_a(!dict_index_is_ibuf(index));

	table = btr_get_search_table(index);

	rec = btr_cur_get_rec(cursor);

	fold = rec_fold(rec, rec_get_offsets(rec, index, offsets_,
					     ULINT_UNDEFINED, &heap),
			block->curr_n_fields, block->curr_n_bytes,
			btr_search_fold_index_id(index->space, index->id));
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	btr_search_x_lock(index);
	assert_block_ahi_valid(block);

	if (block->index) {
		ut_a(block->index == index);

		if (ha_search_and_delete_if_found(table, fold, rec)) {
			MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_REMOVED);
		} else {
			MONITOR_INC(
				MONITOR_ADAPTIVE_HASH_ROW_REMOVE_NOT_FOUND);
		}

		assert_block_ahi_valid(block);
	}

	btr_search_x_unlock(index);
}

/** Updates the page hash index when a single record is inserted on a page.
@param[in]	cursor	cursor which was positioned to the place to insert
			using btr_cur_search_, and the new record has been
			inserted next to the cursor. */
void
btr_search_update_hash_node_on_insert(btr_cur_t* cursor)
{
	hash_table_t*	table;
	buf_block_t*	block;
	dict_index_t*	index;
	rec_t*		rec;

	if (cursor->index->disable_ahi || !btr_search_enabled) {
		return;
	}

	rec = btr_cur_get_rec(cursor);

	block = btr_cur_get_block(cursor);

	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));

	index = block->index;

	if (!index) {

		return;
	}

	ut_a(cursor->index == index);
	ut_a(!dict_index_is_ibuf(index));

	btr_search_x_lock(index);

	if (!block->index) {

		goto func_exit;
	}

	ut_a(block->index == index);

	if ((cursor->flag == BTR_CUR_HASH)
	    && (cursor->n_fields == block->curr_n_fields)
	    && (cursor->n_bytes == block->curr_n_bytes)
	    && !block->curr_left_side) {

		table = btr_get_search_table(index);

		if (ha_search_and_update_if_found(
			table, cursor->fold, rec, block,
			page_rec_get_next(rec))) {
			MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_UPDATED);
		}

func_exit:
		assert_block_ahi_valid(block);
		btr_search_x_unlock(index);
	} else {
		btr_search_x_unlock(index);

		btr_search_update_hash_on_insert(cursor);
	}
}

/** Updates the page hash index when a single record is inserted on a page.
@param[in,out]	cursor		cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
void
btr_search_update_hash_on_insert(btr_cur_t* cursor)
{
	hash_table_t*	table;
	buf_block_t*	block;
	dict_index_t*	index;
	const rec_t*	rec;
	const rec_t*	ins_rec;
	const rec_t*	next_rec;
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

	if (cursor->index->disable_ahi || !btr_search_enabled) {
		return;
	}

	block = btr_cur_get_block(cursor);

	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));
	assert_block_ahi_valid(block);

	index = block->index;

	if (!index) {

		return;
	}

	ut_ad(block->page.id.space() == index->space);
	btr_search_check_free_space_in_heap(index);

	table = btr_get_search_table(index);

	rec = btr_cur_get_rec(cursor);

	ut_a(!index->disable_ahi);
	ut_a(index == cursor->index);
	ut_a(!dict_index_is_ibuf(index));

	n_fields = block->curr_n_fields;
	n_bytes = block->curr_n_bytes;
	left_side = block->curr_left_side;

	ins_rec = page_rec_get_next_const(rec);
	next_rec = page_rec_get_next_const(ins_rec);

	const ulint	index_fold	= btr_search_fold_index_id(
		index->space, index->id);
	const ulint	n_offs		= btr_search_get_n_fields(
		n_fields, n_bytes);

	offsets = rec_get_offsets(ins_rec, index, offsets,
				  n_offs, &heap);
	ins_fold = rec_fold(ins_rec, offsets, n_fields, n_bytes, index_fold);

	if (!page_rec_is_supremum(next_rec)) {
		offsets = rec_get_offsets(
			next_rec, index, offsets, n_offs, &heap);
		next_fold = rec_fold(next_rec, offsets, n_fields, n_bytes,
				     index_fold);
	}

	if (!page_rec_is_infimum(rec)) {
		offsets = rec_get_offsets(rec, index, offsets, n_offs, &heap);
		fold = rec_fold(rec, offsets, n_fields, n_bytes, index_fold);
	} else {
		if (left_side) {

			btr_search_x_lock(index);

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

			btr_search_x_lock(index);

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
				btr_search_x_lock(index);

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

			btr_search_x_lock(index);

			locked = TRUE;

			if (!btr_search_enabled) {
				goto function_exit;
			}
		}

		if (!left_side) {
			ha_insert_for_fold(table, ins_fold, block, ins_rec);
		} else {
			ha_insert_for_fold(table, next_fold, block, next_rec);
		}
	}

function_exit:
	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
	if (locked) {
		btr_search_x_unlock(index);
	}
}

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG

/** Validates the search system for given hash table.
@param[in]	hash_table_id	hash table to validate
@return TRUE if ok */
static
ibool
btr_search_hash_table_validate(ulint hash_table_id)
{
	ha_node_t*	node;
	ibool		ok		= TRUE;
	ulint		i;
	ulint		cell_count;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;

	if (!btr_search_enabled) {
		return(TRUE);
	}

	/* How many cells to check before temporarily releasing
	search latches. */
	ulint		chunk_size = 10000;

	rec_offs_init(offsets_);

	btr_search_x_lock_all();

	cell_count = hash_get_n_cells(
			btr_search_sys->hash_tables[hash_table_id]);

	for (i = 0; i < cell_count; i++) {
		/* We release search latches every once in a while to
		give other queries a chance to run. */
		if ((i != 0) && ((i % chunk_size) == 0)) {

			btr_search_x_unlock_all();
			os_thread_yield();
			btr_search_x_lock_all();

			ulint	curr_cell_count = hash_get_n_cells(
				btr_search_sys->hash_tables[hash_table_id]);

			if (cell_count != curr_cell_count) {

				cell_count = curr_cell_count;

				if (i >= cell_count) {
					break;
				}
			}
		}

		node = (ha_node_t*) hash_get_nth_cell(
			btr_search_sys->hash_tables[hash_table_id], i)->node;

		for (; node != NULL; node = node->next) {
			buf_block_t*		block
				= buf_block_from_ahi((byte*) node->data);
			const buf_block_t*	hash_block;
			buf_pool_t*		buf_pool;

			buf_pool = buf_pool_from_bpage((buf_page_t*) block);
			/* Prevent BUF_BLOCK_FILE_PAGE -> BUF_BLOCK_REMOVE_HASH
			transition until we lock the block mutex */
			mutex_enter(&buf_pool->LRU_list_mutex);

			if (UNIV_LIKELY(buf_block_get_state(block)
					== BUF_BLOCK_FILE_PAGE)) {

				/* The space and offset are only valid
				for file blocks.  It is possible that
				the block is being freed
				(BUF_BLOCK_REMOVE_HASH, see the
				assertion and the comment below) */
				hash_block = buf_block_hash_get(
					buf_pool,
					block->page.id);
			} else {
				hash_block = NULL;
			}

			if (hash_block) {
				ut_a(hash_block == block);
			} else {
				/* When a block is being freed,
				buf_LRU_free_page() first
				removes the block from
				buf_pool->page_hash by calling
				buf_LRU_block_remove_hashed_page().
				After that, it invokes
				buf_LRU_block_remove_hashed() to
				remove the block from
				btr_search_sys->hash_tables[i]. */

				ut_a(buf_block_get_state(block)
				     == BUF_BLOCK_REMOVE_HASH);
			}

			mutex_enter(&block->mutex);
			mutex_exit(&buf_pool->LRU_list_mutex);

			ut_a(!dict_index_is_ibuf(block->index));
			ut_ad(block->page.id.space() == block->index->space);

			index_id_t	page_index_id(
				block->page.id.space(),
				btr_page_get_index_id(block->frame));

			offsets = rec_get_offsets(
				node->data, block->index, offsets,
				btr_search_get_n_fields(block->curr_n_fields,
							block->curr_n_bytes),
				&heap);

			const ulint	fold = rec_fold(
				node->data, offsets,
				block->curr_n_fields,
				block->curr_n_bytes,
				btr_search_fold_index_id(page_index_id));

			if (node->fold != fold) {
				const page_t*	page = block->frame;

				ok = FALSE;

				ib::error() << "Error in an adaptive hash"
					<< " index pointer to page "
					<< page_id_t(page_get_space_id(page),
						     page_get_page_no(page))
					<< ", ptr mem address "
					<< reinterpret_cast<const void*>(
						node->data)
					<< ", index id " << page_index_id
					<< ", node fold " << node->fold
					<< ", rec fold " << fold;

				fputs("InnoDB: Record ", stderr);
				rec_print_new(stderr, node->data, offsets);
				fprintf(stderr, "\nInnoDB: on that page."
					" Page mem address %p, is hashed %p,"
					" n fields %lu\n"
					"InnoDB: side %lu\n",
					(void*) page, (void*) block->index,
					(ulong) block->curr_n_fields,
					(ulong) block->curr_left_side);
				ut_ad(0);
			}

			mutex_exit(&block->mutex);
		}
	}

	for (i = 0; i < cell_count; i += chunk_size) {
		/* We release search latches every once in a while to
		give other queries a chance to run. */
		if (i != 0) {

			btr_search_x_unlock_all();
			os_thread_yield();
			btr_search_x_lock_all();

			ulint	curr_cell_count = hash_get_n_cells(
				btr_search_sys->hash_tables[hash_table_id]);

			if (cell_count != curr_cell_count) {

				cell_count = curr_cell_count;

				if (i >= cell_count) {
					break;
				}
			}
		}

		ulint end_index = ut_min(i + chunk_size - 1, cell_count - 1);

		if (!ha_validate(btr_search_sys->hash_tables[hash_table_id],
				 i, end_index)) {
			ok = FALSE;
		}
	}

	btr_search_x_unlock_all();

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	return(ok);
}

/** Validate the search system.
@return true if ok. */
bool
btr_search_validate()
{
	for (ulint i = 0; i < btr_ahi_parts; ++i) {
		if (!btr_search_hash_table_validate(i)) {
			return(false);
		}
	}

	return(true);
}

#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */
