/************************************************************************
The index tree adaptive search

(c) 1996 Innobase Oy

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

ulint	btr_search_this_is_zero = 0;	/* A dummy variable to fool the
					compiler */

#ifdef UNIV_SEARCH_PERF_STAT
ulint	btr_search_n_succ	= 0;
#endif /* UNIV_SEARCH_PERF_STAT */
ulint	btr_search_n_hash_fail	= 0;

byte	btr_sea_pad1[64];	/* padding to prevent other memory update
				hotspots from residing on the same memory
				cache line as btr_search_latch */

/* The latch protecting the adaptive search system: this latch protects the
(1) positions of records on those pages where a hash index has been built.
NOTE: It does not protect values of non-ordering fields within a record from
being updated in-place! We can use fact (1) to perform unique searches to
indexes. */

rw_lock_t*	btr_search_latch_temp; /* We will allocate the latch from
					dynamic memory to get it to the
					same DRAM page as other hotspot
					semaphores */

byte	btr_sea_pad2[64];	/* padding to prevent other memory update
				hotspots from residing on the same memory
				cache line */

btr_search_sys_t*	btr_search_sys;

/* If the number of records on the page divided by this parameter
would have been successfully accessed using a hash index, the index
is then built on the page, assuming the global limit has been reached */

#define BTR_SEARCH_PAGE_BUILD_LIMIT	16

/* The global limit for consecutive potentially successful hash searches,
before hash index building is started */

#define BTR_SEARCH_BUILD_LIMIT		100

/************************************************************************
Builds a hash index on a page with the given parameters. If the page already
has a hash index with different parameters, the old hash index is removed.
If index is non-NULL, this function checks if n_fields and n_bytes are
sensible values, and does not build a hash index if not. */
static
void
btr_search_build_page_hash_index(
/*=============================*/
	dict_index_t*	index,	/* in: index for which to build, or NULL if
				not known */
	page_t*		page,	/* in: index page, s- or x-latched */
	ulint		n_fields,/* in: hash this many full fields */
	ulint		n_bytes,/* in: hash this many bytes from the next
				field */
	ulint		side);	/* in: hash for searches from this side */

/*********************************************************************
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
btr_search_check_free_space_in_heap(void)
/*=====================================*/
{
	buf_frame_t*	frame;
	hash_table_t*	table;
	mem_heap_t*	heap;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_SHARED));
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	table = btr_search_sys->hash_index;

	heap = table->heap;
			
	/* Note that we peek the value of heap->free_block without reserving
	the latch: this is ok, because we will not guarantee that there will
	be enough free space in the hash table. */

	if (heap->free_block == NULL) {
		frame = buf_frame_alloc();

		rw_lock_x_lock(&btr_search_latch);

		if (heap->free_block == NULL) {
			heap->free_block = frame;
		} else {
			buf_frame_free(frame);
		}

		rw_lock_x_unlock(&btr_search_latch);
	}
}

/*********************************************************************
Creates and initializes the adaptive search system at a database start. */

void
btr_search_sys_create(
/*==================*/
	ulint	hash_size)	/* in: hash index hash table size */
{
	/* We allocate the search latch from dynamic memory:
	see above at the global variable definition */
	
	btr_search_latch_temp = mem_alloc(sizeof(rw_lock_t));
	
	rw_lock_create(&btr_search_latch);

	btr_search_sys = mem_alloc(sizeof(btr_search_sys_t));

	btr_search_sys->hash_index = ha_create(TRUE, hash_size, 0, 0);

	rw_lock_set_level(&btr_search_latch, SYNC_SEARCH_SYS);
}

/*********************************************************************
Creates and initializes a search info struct. */

btr_search_t*
btr_search_info_create(
/*===================*/
				/* out, own: search info struct */
	mem_heap_t*	heap)	/* in: heap where created */
{
	btr_search_t*	info;

	info = mem_heap_alloc(heap, sizeof(btr_search_t));

	info->magic_n = BTR_SEARCH_MAGIC_N;

	info->last_search = NULL;
	info->n_direction = 0;
	info->root_guess = NULL;

	info->hash_analysis = 0;
	info->n_hash_potential = 0;

	info->last_hash_succ = FALSE;

	info->n_hash_succ = 0;	
	info->n_hash_fail = 0;	
	info->n_patt_succ = 0;	
	info->n_searches = 0;	

	/* Set some sensible values */
	info->n_fields = 1;
	info->n_bytes = 0;

	info->side = BTR_SEARCH_LEFT_SIDE;

	return(info);
}

/*************************************************************************
Updates the search info of an index about hash successes. NOTE that info
is NOT protected by any semaphore, to save CPU time! Do not assume its fields
are consistent. */
static
void
btr_search_info_update_hash(
/*========================*/
	btr_search_t*	info,	/* in: search info */
	btr_cur_t*	cursor)	/* in: cursor which was just positioned */
{
	dict_index_t*	index;
	ulint		n_unique;
	int		cmp;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_SHARED));
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	index = cursor->index;

	if (index->type & DICT_IBUF) {
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
			
		info->n_hash_potential++;

		return;
	}

	cmp = ut_pair_cmp(info->n_fields, info->n_bytes,
					cursor->low_match, cursor->low_bytes);

	if ((info->side == BTR_SEARCH_LEFT_SIDE && cmp <= 0)
		|| (info->side == BTR_SEARCH_RIGHT_SIDE && cmp > 0)) {

		goto set_new_recomm;
	}

	cmp = ut_pair_cmp(info->n_fields, info->n_bytes,
					cursor->up_match, cursor->up_bytes);

	if ((info->side == BTR_SEARCH_LEFT_SIDE && cmp > 0)
		|| (info->side == BTR_SEARCH_RIGHT_SIDE && cmp <= 0)) {

	    	goto set_new_recomm;
	}

	info->n_hash_potential++;

	return;
	
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

		info->side = BTR_SEARCH_LEFT_SIDE;

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

		info->side = BTR_SEARCH_LEFT_SIDE;
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

		info->side = BTR_SEARCH_RIGHT_SIDE;
	}
}
	
/*************************************************************************
Updates the block search info on hash successes. NOTE that info and
block->n_hash_helps, n_fields, n_bytes, side are NOT protected by any
semaphore, to save CPU time! Do not assume the fields are consistent. */
static
ibool
btr_search_update_block_hash_info(
/*==============================*/
				/* out: TRUE if building a (new) hash index on
				the block is recommended */
	btr_search_t*	info,	/* in: search info */
	buf_block_t*	block,	/* in: buffer block */
	btr_cur_t*	cursor)	/* in: cursor */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_SHARED));
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_EX));
	ut_ad(rw_lock_own(&((buf_block_t*) block)->lock, RW_LOCK_SHARED)
		|| rw_lock_own(&((buf_block_t*) block)->lock, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(cursor);

	info->last_hash_succ = FALSE;

	ut_a(block->magic_n == BUF_BLOCK_MAGIC_N);
	ut_a(info->magic_n == BTR_SEARCH_MAGIC_N);

	if ((block->n_hash_helps > 0)
	    && (info->n_hash_potential > 0)
	    && (block->n_fields == info->n_fields)
	    && (block->n_bytes == info->n_bytes)
	    && (block->side == info->side)) {
	
		if ((block->is_hashed)
		    && (block->curr_n_fields == info->n_fields)
		    && (block->curr_n_bytes == info->n_bytes)
		    && (block->curr_side == info->side)) {

			/* The search would presumably have succeeded using
			the hash index */
		    
			info->last_hash_succ = TRUE;
		}

		block->n_hash_helps++;
	} else {
		block->n_hash_helps = 1;
		block->n_fields = info->n_fields;
		block->n_bytes = info->n_bytes;
		block->side = info->side;
	}

	if (cursor->index->table->does_not_fit_in_memory) {
		block->n_hash_helps = 0;
	}

	if ((block->n_hash_helps > page_get_n_recs(block->frame)
	    				/ BTR_SEARCH_PAGE_BUILD_LIMIT)
	    && (info->n_hash_potential >= BTR_SEARCH_BUILD_LIMIT)) {

	    	if ((!block->is_hashed)
		    || (block->n_hash_helps
					> 2 * page_get_n_recs(block->frame))
		    || (block->n_fields != block->curr_n_fields)
		    || (block->n_bytes != block->curr_n_bytes)
		    || (block->side != block->curr_side)) {

	    		/* Build a new hash index on the page */

	    		return(TRUE);
		}
	}

	return(FALSE);
}

/*************************************************************************
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
	btr_search_t*	info,	/* in: search info */
	buf_block_t*	block,	/* in: buffer block where cursor positioned */
	btr_cur_t*	cursor)	/* in: cursor */
{
	ulint	fold;
	rec_t*	rec;
	dulint	tree_id;

	ut_ad(cursor->flag == BTR_CUR_HASH_FAIL);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&btr_search_latch, RW_LOCK_EX));
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_SHARED)
				|| rw_lock_own(&(block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	if (block->is_hashed
	    && (info->n_hash_potential > 0)
	    && (block->curr_n_fields == info->n_fields)
	    && (block->curr_n_bytes == info->n_bytes)
	    && (block->curr_side == info->side)) {
		mem_heap_t*	heap;
	    	rec = btr_cur_get_rec(cursor);

	    	if (!page_rec_is_user_rec(rec)) {

	    		return;
	    	}
	    
		tree_id = ((cursor->index)->tree)->id;
		heap = mem_heap_create(100);
		fold = rec_fold(rec, rec_get_offsets(rec, cursor->index,
				ULINT_UNDEFINED, heap), block->curr_n_fields,
				block->curr_n_bytes, tree_id);
		mem_heap_free(heap);
#ifdef UNIV_SYNC_DEBUG
		ut_ad(rw_lock_own(&btr_search_latch, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

		ha_insert_for_fold(btr_search_sys->hash_index, fold, rec);
	}
}

/*************************************************************************
Updates the search info. */

void
btr_search_info_update_slow(
/*========================*/
	btr_search_t*	info,	/* in: search info */
	btr_cur_t*	cursor)	/* in: cursor which was just positioned */
{
	buf_block_t*	block;
	ibool		build_index;
	ulint*		params;
	ulint*		params2;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_SHARED));
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	block = buf_block_align(btr_cur_get_rec(cursor));

	/* NOTE that the following two function calls do NOT protect
	info or block->n_fields etc. with any semaphore, to save CPU time!
	We cannot assume the fields are consistent when we return from
	those functions! */

	btr_search_info_update_hash(info, cursor);

	build_index = btr_search_update_block_hash_info(info, block, cursor);

	if (build_index || (cursor->flag == BTR_CUR_HASH_FAIL)) {

		btr_search_check_free_space_in_heap();
	}
	
	if (cursor->flag == BTR_CUR_HASH_FAIL) {
		/* Update the hash node reference, if appropriate */

		btr_search_n_hash_fail++;

		rw_lock_x_lock(&btr_search_latch);

		btr_search_update_hash_ref(info, block, cursor);

		rw_lock_x_unlock(&btr_search_latch);
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
		params[2] = block->side;

		/* Make sure the compiler cannot deduce the values and do
		optimizations */

		params2 = params + btr_search_this_is_zero;
		
		btr_search_build_page_hash_index(cursor->index,
						block->frame,
						params2[0],
						params2[1],
						params2[2]);
		mem_free(params);
	}
}

/**********************************************************************
Checks if a guessed position for a tree cursor is right. Note that if
mode is PAGE_CUR_LE, which is used in inserts, and the function returns
TRUE, then cursor->up_match and cursor->low_match both have sensible values. */
static
ibool
btr_search_check_guess(
/*===================*/
				/* out: TRUE if success */
	btr_cur_t*	cursor,	/* in: guessed cursor position */
	ibool           can_only_compare_to_cursor_rec,
	                        /* in: if we do not have a latch on the page
				of cursor, but only a latch on
			        btr_search_latch, then ONLY the columns
				of the record UNDER the cursor are
				protected, not the next or previous record
				in the chain: we cannot look at the next or
				previous record to check our guess! */
	dtuple_t* 	tuple,	/* in: data tuple */
	ulint		mode,	/* in: PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G,
				or PAGE_CUR_GE */
	mtr_t*		mtr)	/* in: mtr */
{
	page_t*		page;
	rec_t*		rec;
	rec_t*		prev_rec;
	rec_t*		next_rec;
	ulint		n_unique;
	ulint		match;
	ulint		bytes;
	int		cmp;
	mem_heap_t*	heap	= mem_heap_create(100);
	ulint*		offsets	= NULL;

	n_unique = dict_index_get_n_unique_in_tree(cursor->index);
	
	rec = btr_cur_get_rec(cursor);
	page = buf_frame_align(rec);

	ut_ad(page_rec_is_user_rec(rec));

	match = 0;
	bytes = 0;

	offsets = rec_get_offsets(rec, cursor->index, n_unique, heap);
	cmp = page_cmp_dtuple_rec_with_match(tuple, rec,
						offsets, &match, &bytes);

	if (mode == PAGE_CUR_GE) {
		if (cmp == 1) {
			mem_heap_free(heap);
			return(FALSE);
		}

		cursor->up_match = match;

		if (match >= n_unique) {
			mem_heap_free(heap);
			return(TRUE);
		}	
	} else if (mode == PAGE_CUR_LE) {
		if (cmp == -1) {
			mem_heap_free(heap);
			return(FALSE);
		}

		cursor->low_match = match;

	} else if (mode == PAGE_CUR_G) {
		if (cmp != -1) {
			mem_heap_free(heap);
			return(FALSE);
		}
	} else if (mode == PAGE_CUR_L) {
		if (cmp != 1) {
			mem_heap_free(heap);
			return(FALSE);
		}
	}

	if (can_only_compare_to_cursor_rec) {
	        /* Since we could not determine if our guess is right just by
	        looking at the record under the cursor, return FALSE */
		mem_heap_free(heap);
	        return(FALSE);
	}

	match = 0;
	bytes = 0;

	if ((mode == PAGE_CUR_G) || (mode == PAGE_CUR_GE)) {

		ut_ad(rec != page_get_infimum_rec(page));
		
		prev_rec = page_rec_get_prev(rec);

		if (prev_rec == page_get_infimum_rec(page)) {
			mem_heap_free(heap);
	    		return(btr_page_get_prev(page, mtr) == FIL_NULL);
		}

		offsets = rec_reget_offsets(prev_rec, cursor->index,
						offsets, n_unique, heap);
		cmp = page_cmp_dtuple_rec_with_match(tuple, prev_rec,
					offsets, &match, &bytes);
		mem_heap_free(heap);
		if (mode == PAGE_CUR_GE) {
			if (cmp != 1) {

				return(FALSE);
			}
		} else {
			if (cmp == -1) {

				return(FALSE);
			}
		}

		return(TRUE);
	}
		
	ut_ad(rec != page_get_supremum_rec(page));
	
	next_rec = page_rec_get_next(rec);

	if (next_rec == page_get_supremum_rec(page)) {
		mem_heap_free(heap);

    		if (btr_page_get_next(page, mtr) == FIL_NULL) {

			cursor->up_match = 0;

			return(TRUE);
		}

		return(FALSE);
	}

	offsets = rec_reget_offsets(next_rec, cursor->index,
						offsets, n_unique, heap);
	cmp = page_cmp_dtuple_rec_with_match(tuple, next_rec,
					offsets, &match, &bytes);
	mem_heap_free(heap);

	if (mode == PAGE_CUR_LE) {
		if (cmp != -1) {

			return(FALSE);
		}

		cursor->up_match = match;
	} else {
		if (cmp == 1) {

			return(FALSE);
		}
	}

	return(TRUE);
}

/**********************************************************************
Tries to guess the right search position based on the hash search info
of the index. Note that if mode is PAGE_CUR_LE, which is used in inserts,
and the function returns TRUE, then cursor->up_match and cursor->low_match
both have sensible values. */

ibool
btr_search_guess_on_hash(
/*=====================*/
					/* out: TRUE if succeeded */	
	dict_index_t*	index,		/* in: index */
	btr_search_t*	info,		/* in: index search info */
	dtuple_t*	tuple,		/* in: logical record */
	ulint		mode,		/* in: PAGE_CUR_L, ... */
	ulint		latch_mode, 	/* in: BTR_SEARCH_LEAF, ...;
					NOTE that only if has_search_latch
					is 0, we will have a latch set on
					the cursor page, otherwise we assume
					the caller uses his search latch
					to protect the record! */
	btr_cur_t*	cursor, 	/* out: tree cursor */
	ulint		has_search_latch,/* in: latch mode the caller
					currently has on btr_search_latch:
					RW_S_LATCH, RW_X_LATCH, or 0 */
	mtr_t*		mtr)		/* in: mtr */
{
	buf_block_t*	block;
	rec_t*		rec;
	page_t*		page;
	ibool		success;
	ulint		fold;
	ulint		tuple_n_fields;
	dulint		tree_id;
	ibool           can_only_compare_to_cursor_rec = TRUE;
#ifdef notdefined
	btr_cur_t	cursor2;
	btr_pcur_t	pcur;
#endif
	ut_ad(index && info && tuple && cursor && mtr);
	ut_ad((latch_mode == BTR_SEARCH_LEAF)
					|| (latch_mode == BTR_MODIFY_LEAF));

	/* Note that, for efficiency, the struct info may not be protected by
	any latch here! */

	if (info->n_hash_potential == 0) {

		return(FALSE);
	}

	cursor->n_fields = info->n_fields;
	cursor->n_bytes = info->n_bytes;

	tuple_n_fields = dtuple_get_n_fields(tuple);

	if (tuple_n_fields < cursor->n_fields) {

		return(FALSE);
	}

	if ((cursor->n_bytes > 0) && (tuple_n_fields <= cursor->n_fields)) {

	    	return(FALSE);
	}

	tree_id = (index->tree)->id;

#ifdef UNIV_SEARCH_PERF_STAT
	info->n_hash_succ++;
#endif
	fold = dtuple_fold(tuple, cursor->n_fields, cursor->n_bytes, tree_id);

	cursor->fold = fold;
	cursor->flag = BTR_CUR_HASH;
	
	if (!has_search_latch) {
		rw_lock_s_lock(&btr_search_latch);
	}

	ut_a(btr_search_latch.writer != RW_LOCK_EX);
	ut_a(btr_search_latch.reader_count > 0);

	rec = ha_search_and_get_data(btr_search_sys->hash_index, fold);

	if (!rec) {
		if (!has_search_latch) {
			rw_lock_s_unlock(&btr_search_latch);
		}
		
		goto failure;
	}

	page = buf_frame_align(rec);

	if (!has_search_latch) {

		success = buf_page_get_known_nowait(latch_mode, page,
						BUF_MAKE_YOUNG,
						__FILE__, __LINE__,
						mtr);

		rw_lock_s_unlock(&btr_search_latch);

		if (!success) {

			goto failure;
		}

		can_only_compare_to_cursor_rec = FALSE;

#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(page, SYNC_TREE_NODE_FROM_HASH);
#endif /* UNIV_SYNC_DEBUG */
	}

	block = buf_block_align(page);

	if (block->state == BUF_BLOCK_REMOVE_HASH) {
		if (!has_search_latch) {
	
			btr_leaf_page_release(page, latch_mode, mtr);
		}

		goto failure;
	}

	ut_a(block->state == BUF_BLOCK_FILE_PAGE);
	ut_a(page_rec_is_user_rec(rec));	

	btr_cur_position(index, rec, cursor);

	/* Check the validity of the guess within the page */

	if (0 != ut_dulint_cmp(tree_id, btr_page_get_index_id(page))) {

		success = FALSE;
/*
		fprintf(stderr, "Tree id %lu, page index id %lu fold %lu\n",
				ut_dulint_get_low(tree_id),
				ut_dulint_get_low(btr_page_get_index_id(page)),
				fold);
*/				
	} else {
	        /* If we only have the latch on btr_search_latch, not on the
		page, it only protects the columns of the record the cursor
		is positioned on. We cannot look at the next of the previous
		record to determine if our guess for the cursor position is
		right. */

		success = btr_search_check_guess(cursor,
				               can_only_compare_to_cursor_rec,
					       tuple, mode, mtr);
	}
	
	if (!success) {
		if (!has_search_latch) {
		          btr_leaf_page_release(page, latch_mode, mtr);
		}

		goto failure;
	}

	if (info->n_hash_potential < BTR_SEARCH_BUILD_LIMIT + 5) {
	
		info->n_hash_potential++;
	}

	if (info->last_hash_succ != TRUE) {
		info->last_hash_succ = TRUE;
	}
	
#ifdef notdefined
	/* These lines of code can be used in a debug version to check
	the correctness of the searched cursor position: */
	
	info->last_hash_succ = FALSE;

	/* Currently, does not work if the following fails: */
	ut_a(!has_search_latch);
	
	btr_leaf_page_release(page, latch_mode, mtr);

	btr_cur_search_to_nth_level(index, 0, tuple, mode, latch_mode,
							&cursor2, 0, mtr);
	if (mode == PAGE_CUR_GE
		&& btr_cur_get_rec(&cursor2) == page_get_supremum_rec(
			buf_frame_align(btr_cur_get_rec(&cursor2)))) {

		/* If mode is PAGE_CUR_GE, then the binary search
		in the index tree may actually take us to the supremum
		of the previous page */
					
		info->last_hash_succ = FALSE;

		btr_pcur_open_on_user_rec(index, tuple, mode, latch_mode,
				&pcur, mtr);
		ut_a(btr_pcur_get_rec(&pcur) == btr_cur_get_rec(cursor));
	} else {
		ut_a(btr_cur_get_rec(&cursor2) == btr_cur_get_rec(cursor));
	}

	/* NOTE that it is theoretically possible that the above assertions
	fail if the page of the cursor gets removed from the buffer pool
	meanwhile! Thus it might not be a bug. */

	info->last_hash_succ = TRUE;
#endif

#ifdef UNIV_SEARCH_PERF_STAT
	btr_search_n_succ++;
#endif
	if (!has_search_latch && buf_block_peek_if_too_old(block)) {

		buf_page_make_young(page);
	}	

	/* Increment the page get statistics though we did not really
	fix the page: for user info only */

	buf_pool->n_page_gets++;

	return(TRUE);	

	/*-------------------------------------------*/
failure:
	info->n_hash_fail++;

	cursor->flag = BTR_CUR_HASH_FAIL;

#ifdef UNIV_SEARCH_PERF_STAT
	if (info->n_hash_succ > 0) {
		info->n_hash_succ--;
	}
#endif
	info->last_hash_succ = FALSE;

	return(FALSE);
}

/************************************************************************
Drops a page hash index. */

void
btr_search_drop_page_hash_index(
/*============================*/
	page_t*	page)	/* in: index page, s- or x-latched */
{
	hash_table_t*	table;
	buf_block_t*	block;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_SHARED));
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	rw_lock_s_lock(&btr_search_latch);

	block = buf_block_align(page);

	if (!block->is_hashed) {

		rw_lock_s_unlock(&btr_search_latch);

		return;
	}

	table = btr_search_sys->hash_index;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_SHARED)
	      			|| rw_lock_own(&(block->lock), RW_LOCK_EX)
	      			|| (block->buf_fix_count == 0));
#endif /* UNIV_SYNC_DEBUG */

	ut_a(block->curr_n_fields + block->curr_n_bytes > 0);

	rw_lock_s_unlock(&btr_search_latch);
	
	rw_lock_x_lock(&btr_search_latch);

	ha_remove_all_nodes_to_page(table, page);

	block->is_hashed = FALSE;

	rw_lock_x_unlock(&btr_search_latch);
}

/************************************************************************
Drops a page hash index when a page is freed from a fseg to the file system.
Drops possible hash index if the page happens to be in the buffer pool. */

void
btr_search_drop_page_hash_when_freed(
/*=================================*/
	ulint	space,		/* in: space id */
	ulint	page_no)	/* in: page number */
{
	ibool	is_hashed;
	page_t*	page;
	mtr_t	mtr;

	is_hashed = buf_page_peek_if_search_hashed(space, page_no);

	if (!is_hashed) {

		return;
	}
	
	mtr_start(&mtr);

	/* We assume that if the caller has a latch on the page, then the
	caller has already dropped the hash index for the page, and we never
	get here. Therefore we can acquire the s-latch to the page without
	having to fear a deadlock. */
	
	page = buf_page_get_gen(space, page_no, RW_S_LATCH, NULL,
				BUF_GET_IF_IN_POOL, __FILE__, __LINE__,
				&mtr);

#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(page, SYNC_TREE_NODE_FROM_HASH);
#endif /* UNIV_SYNC_DEBUG */

	btr_search_drop_page_hash_index(page);

	mtr_commit(&mtr);
}

/************************************************************************
Builds a hash index on a page with the given parameters. If the page already
has a hash index with different parameters, the old hash index is removed.
If index is non-NULL, this function checks if n_fields and n_bytes are
sensible values, and does not build a hash index if not. */
static
void
btr_search_build_page_hash_index(
/*=============================*/
	dict_index_t*	index,	/* in: index for which to build */
	page_t*		page,	/* in: index page, s- or x-latched */
	ulint		n_fields,/* in: hash this many full fields */
	ulint		n_bytes,/* in: hash this many bytes from the next
				field */
	ulint		side)	/* in: hash for searches from this side */
{
	hash_table_t*	table;
	buf_block_t*	block;
	rec_t*		rec;
	rec_t*		next_rec;
	rec_t*		sup;
	ulint		fold;
	ulint		next_fold;
	dulint		tree_id;
	ulint		n_cached;
	ulint		n_recs;
	ulint*		folds;
	rec_t**		recs;
	ulint		i;
	mem_heap_t*	heap;
	ulint*		offsets;

	ut_ad(index);

	block = buf_block_align(page);
	table = btr_search_sys->hash_index;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!rw_lock_own(&btr_search_latch, RW_LOCK_EX));
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_SHARED)
				|| rw_lock_own(&(block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	rw_lock_s_lock(&btr_search_latch);
				
	if (block->is_hashed && ((block->curr_n_fields != n_fields)
	        			|| (block->curr_n_bytes != n_bytes)
	        			|| (block->curr_side != side))) {

		rw_lock_s_unlock(&btr_search_latch);

		btr_search_drop_page_hash_index(page);
	} else {
		rw_lock_s_unlock(&btr_search_latch);
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
	heap = mem_heap_create(100);

	n_cached = 0;

	tree_id = btr_page_get_index_id(page);

	sup = page_get_supremum_rec(page);

	rec = page_get_infimum_rec(page);
	rec = page_rec_get_next(rec);

	offsets = rec_get_offsets(rec, index, n_fields + (n_bytes > 0), heap);

	if (rec != sup) {
		ut_a(n_fields <= rec_offs_n_fields(offsets));

		if (n_bytes > 0) {
			ut_a(n_fields < rec_offs_n_fields(offsets));
		}
	}

	/* FIXME: in a mixed tree, all records may not have enough ordering
	fields: */
	fold = rec_fold(rec, offsets, n_fields, n_bytes, tree_id);

	if (side == BTR_SEARCH_LEFT_SIDE) {

		folds[n_cached] = fold;
		recs[n_cached] = rec;
		n_cached++;
	}
	
	for (;;) {
		next_rec = page_rec_get_next(rec);

		if (next_rec == sup) {

			if (side == BTR_SEARCH_RIGHT_SIDE) {
	
				folds[n_cached] = fold;
				recs[n_cached] = rec;
				n_cached++;
			}

		 	break;
		}

		offsets = rec_reget_offsets(next_rec, index,
				offsets, n_fields + (n_bytes > 0), heap);
		next_fold = rec_fold(next_rec, offsets, n_fields,
						n_bytes, tree_id);

		if (fold != next_fold) {
			/* Insert an entry into the hash index */

			if (side == BTR_SEARCH_LEFT_SIDE) {

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

	btr_search_check_free_space_in_heap();

	rw_lock_x_lock(&btr_search_latch);

	if (block->is_hashed && ((block->curr_n_fields != n_fields)
	        			|| (block->curr_n_bytes != n_bytes)
	        			|| (block->curr_side != side))) {
		goto exit_func;
	}
	
	block->is_hashed = TRUE;
	block->n_hash_helps = 0;
	
	block->curr_n_fields = n_fields;
	block->curr_n_bytes = n_bytes;
	block->curr_side = side;

	for (i = 0; i < n_cached; i++) {
	
		ha_insert_for_fold(table, folds[i], recs[i]);
	}

exit_func:
	rw_lock_x_unlock(&btr_search_latch);

	mem_free(folds);
	mem_free(recs);
	mem_heap_free(heap);
}

/************************************************************************
Moves or deletes hash entries for moved records. If new_page is already hashed,
then the hash index for page, if any, is dropped. If new_page is not hashed,
and page is hashed, then a new hash index is built to new_page with the same
parameters as page (this often happens when a page is split). */

void
btr_search_move_or_delete_hash_entries(
/*===================================*/
	page_t*		new_page,	/* in: records are copied
					to this page */
	page_t*		page,		/* in: index page from which
					records were copied, and the
					copied records will be deleted
					from this page */
	dict_index_t*	index)		/* in: record descriptor */
{
	buf_block_t*	block;
	buf_block_t*	new_block;
	ulint		n_fields;
	ulint		n_bytes;
	ulint		side;

	block = buf_block_align(page);
	new_block = buf_block_align(new_page);
	ut_a(page_is_comp(page) == page_is_comp(new_page));

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_EX));
	ut_ad(rw_lock_own(&(new_block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	rw_lock_s_lock(&btr_search_latch);
			
	if (new_block->is_hashed) {

		rw_lock_s_unlock(&btr_search_latch);

		btr_search_drop_page_hash_index(page);

		return;
	}

	if (block->is_hashed) {

		n_fields = block->curr_n_fields;
		n_bytes = block->curr_n_bytes;
		side = block->curr_side;

		new_block->n_fields = block->curr_n_fields;
		new_block->n_bytes = block->curr_n_bytes;
		new_block->side = block->curr_side;

		rw_lock_s_unlock(&btr_search_latch);

		ut_a(n_fields + n_bytes > 0);

		btr_search_build_page_hash_index(index, new_page, n_fields,
							n_bytes, side);
		ut_a(n_fields == block->curr_n_fields);
		ut_a(n_bytes == block->curr_n_bytes);
		ut_a(side == block->curr_side);

		return;
	}

	rw_lock_s_unlock(&btr_search_latch);
}

/************************************************************************
Updates the page hash index when a single record is deleted from a page. */

void
btr_search_update_hash_on_delete(
/*=============================*/
	btr_cur_t*	cursor)	/* in: cursor which was positioned on the
				record to delete using btr_cur_search_...,
				the record is not yet deleted */
{
	hash_table_t*	table;
	buf_block_t*	block;
	rec_t*		rec;
	ulint		fold;
	dulint		tree_id;
	ibool		found;
	mem_heap_t*	heap;

	rec = btr_cur_get_rec(cursor);

	block = buf_block_align(rec);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	if (!block->is_hashed) {

		return;
	}

	ut_a(block->curr_n_fields + block->curr_n_bytes > 0);

	table = btr_search_sys->hash_index;

	tree_id = cursor->index->tree->id;
	heap = mem_heap_create(100);
	fold = rec_fold(rec, rec_get_offsets(rec, cursor->index,
				ULINT_UNDEFINED, heap), block->curr_n_fields,
				block->curr_n_bytes, tree_id);
	mem_heap_free(heap);
	rw_lock_x_lock(&btr_search_latch);

	found = ha_search_and_delete_if_found(table, fold, rec);

	rw_lock_x_unlock(&btr_search_latch);
}

/************************************************************************
Updates the page hash index when a single record is inserted on a page. */

void
btr_search_update_hash_node_on_insert(
/*==================================*/
	btr_cur_t*	cursor)	/* in: cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
{
	hash_table_t*	table;
	buf_block_t*	block;
	rec_t*		rec;

	rec = btr_cur_get_rec(cursor);

	block = buf_block_align(rec);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	if (!block->is_hashed) {

		return;
	}

	rw_lock_x_lock(&btr_search_latch);

	if ((cursor->flag == BTR_CUR_HASH)
	    && (cursor->n_fields == block->curr_n_fields)
	    && (cursor->n_bytes == block->curr_n_bytes)
	    && (block->curr_side == BTR_SEARCH_RIGHT_SIDE)) {

	    	table = btr_search_sys->hash_index;
	    	
	    	ha_search_and_update_if_found(table, cursor->fold, rec,
						page_rec_get_next(rec));

		rw_lock_x_unlock(&btr_search_latch);
	} else {
		rw_lock_x_unlock(&btr_search_latch);

		btr_search_update_hash_on_insert(cursor);
	}
}

/************************************************************************
Updates the page hash index when a single record is inserted on a page. */

void
btr_search_update_hash_on_insert(
/*=============================*/
	btr_cur_t*	cursor)	/* in: cursor which was positioned to the
				place to insert using btr_cur_search_...,
				and the new record has been inserted next
				to the cursor */
{
	hash_table_t*	table; 
	buf_block_t*	block;
	page_t*		page;
	rec_t*		rec;
	rec_t*		ins_rec;
	rec_t*		next_rec;
	dulint		tree_id;
	ulint		fold;
	ulint		ins_fold;
	ulint		next_fold = 0; /* remove warning (??? bug ???) */
	ulint		n_fields;
	ulint		n_bytes;
	ulint		side;
	ibool		locked	= FALSE;
	mem_heap_t*	heap;
	ulint*		offsets;

	table = btr_search_sys->hash_index;

	btr_search_check_free_space_in_heap();

	rec = btr_cur_get_rec(cursor);

	block = buf_block_align(rec);

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&(block->lock), RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */
	
	if (!block->is_hashed) {

		return;
	}

	tree_id = ((cursor->index)->tree)->id;

	n_fields = block->curr_n_fields;
	n_bytes = block->curr_n_bytes;
	side = block->curr_side;

	ins_rec = page_rec_get_next(rec);
	next_rec = page_rec_get_next(ins_rec);

	page = buf_frame_align(rec);
	heap = mem_heap_create(100);
	offsets = rec_get_offsets(ins_rec, cursor->index,
					ULINT_UNDEFINED, heap);
	ins_fold = rec_fold(ins_rec, offsets, n_fields, n_bytes, tree_id);

	if (next_rec != page_get_supremum_rec(page)) {
		offsets = rec_reget_offsets(next_rec, cursor->index,
				offsets, n_fields + (n_bytes > 0), heap);
		next_fold = rec_fold(next_rec, offsets, n_fields,
							n_bytes, tree_id);
	}

	if (rec != page_get_infimum_rec(page)) {
		offsets = rec_reget_offsets(rec, cursor->index,
				offsets, n_fields + (n_bytes > 0), heap);
		fold = rec_fold(rec, offsets, n_fields, n_bytes, tree_id);
	} else {
		if (side == BTR_SEARCH_LEFT_SIDE) {

			rw_lock_x_lock(&btr_search_latch);

			locked = TRUE;

			ha_insert_for_fold(table, ins_fold, ins_rec);
		}

		goto check_next_rec;
	}
	
 	if (fold != ins_fold) {

 		if (!locked) {

			rw_lock_x_lock(&btr_search_latch);

			locked = TRUE;
		}

		if (side == BTR_SEARCH_RIGHT_SIDE) {
			ha_insert_for_fold(table, fold, rec);
		} else {
			ha_insert_for_fold(table, ins_fold, ins_rec);
		}
	}

check_next_rec:
	if (next_rec == page_get_supremum_rec(page)) {

		if (side == BTR_SEARCH_RIGHT_SIDE) {

 			if (!locked) {
				rw_lock_x_lock(&btr_search_latch);

				locked = TRUE;
			}
			
			ha_insert_for_fold(table, ins_fold, ins_rec);
		}

		goto function_exit;
	}
	
	if (ins_fold != next_fold) {

 		if (!locked) {
	
			rw_lock_x_lock(&btr_search_latch);

			locked = TRUE;
		}

		if (side == BTR_SEARCH_RIGHT_SIDE) {

			ha_insert_for_fold(table, ins_fold, ins_rec);
/*
			fputs("Hash insert for ", stderr);
			dict_index_name_print(stderr, cursor->index);
			fprintf(stderr, " fold %lu\n", ins_fold);
*/
		} else {
			ha_insert_for_fold(table, next_fold, next_rec);
		}
	}	
		
function_exit:
	mem_heap_free(heap);
	if (locked) {
		rw_lock_x_unlock(&btr_search_latch);
	}
}

/************************************************************************
Validates the search system. */

ibool
btr_search_validate(
/*================*/
				/* out: TRUE if ok */
	dict_index_t*	index)	/* in: record descriptor */
{
	buf_block_t*	block;
	page_t*		page;
	ha_node_t*	node;
	ulint		n_page_dumps	= 0;
	ibool		ok		= TRUE;
	ulint		i;
	mem_heap_t*	heap		= mem_heap_create(100);
	ulint*		offsets		= NULL;
	
	rw_lock_x_lock(&btr_search_latch);

	for (i = 0; i < hash_get_n_cells(btr_search_sys->hash_index); i++) {
		node = hash_get_nth_cell(btr_search_sys->hash_index, i)->node;

		while (node != NULL) {
			block = buf_block_align(node->data);
			page = buf_frame_align(node->data);
			offsets = rec_reget_offsets((rec_t*) node->data, index,
					offsets, block->curr_n_fields
					+ (block->curr_n_bytes > 0), heap);

			if (!block->is_hashed
			    || node->fold != rec_fold((rec_t*)(node->data),
						offsets,
						block->curr_n_fields,
						block->curr_n_bytes,
						btr_page_get_index_id(page))) {
				ok = FALSE;
				ut_print_timestamp(stderr);

				fprintf(stderr,
"  InnoDB: Error in an adaptive hash index pointer to page %lu\n"
"ptr mem address %p index id %lu %lu, node fold %lu, rec fold %lu\n",
					(ulong) buf_frame_get_page_no(page),
					node->data,
					(ulong) ut_dulint_get_high(btr_page_get_index_id(page)),
					(ulong) ut_dulint_get_low(btr_page_get_index_id(page)),
					(ulong) node->fold,
					(ulong) rec_fold((rec_t*)(node->data),
							  offsets,
					  		  block->curr_n_fields,
					  		  block->curr_n_bytes,
					  		  btr_page_get_index_id(page)));

				fputs("InnoDB: Record ", stderr);
				rec_print(stderr, (rec_t*)node->data, offsets);
				fprintf(stderr, "\nInnoDB: on that page."
"Page mem address %p, is hashed %lu, n fields %lu, n bytes %lu\n"
"side %lu\n",
			        page, (ulong) block->is_hashed,
			        (ulong) block->curr_n_fields,
			        (ulong) block->curr_n_bytes, (ulong) block->curr_side);

				if (n_page_dumps < 20) {	
					buf_page_print(page);
					n_page_dumps++;
				}
			}

			node = node->next;
		}
	}
	
	if (!ha_validate(btr_search_sys->hash_index)) {

		ok = FALSE;
	}

	rw_lock_x_unlock(&btr_search_latch);
	mem_heap_free(heap);

	return(ok);
}
