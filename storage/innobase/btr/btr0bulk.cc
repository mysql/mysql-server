/*****************************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file btr/btr0bulk.cc
The B-tree bulk load

Created 11/21/2013 Shaohua Wang
*******************************************************/

#include "btr0bulk.h"
#include "btr0btr.h"
#include "btr0cur.h"

char	innobase_enable_bulk_load;

/* Innodb index fill factor during index build. */
long	innobase_index_fill_factor;

/** Insert a tupe in a page.
We do tuple conversion, free space check and blob insert.
@param[in,out]	btr_bulk	btree bulk load state
@param[in,out]	page_bulk	page bulk load state
@param[in]	tuple		tuple to insert
@return error code */
static
dberr_t
btr_bulk_load_page_insert(
	btr_bulk_t*	btr_bulk,
	page_bulk_t*	page_bulk,
	dtuple_t*	tuple);

/** Commit mtr for a page when it's full.
Set page dir and page next & prev link.
@param[in/out]	btr_bulk	btr bulk load state
@param[in]	page_bulk	page bulk load state
@param[in/out]	next_page_bulk	page bulk load state of next page
@param[in]	insert_father	whether insert a node ptr to father page
@return error code */
static
dberr_t
btr_bulk_load_page_commit(
	btr_bulk_t*	btr_bulk,
	page_bulk_t*	page_bulk,
	page_bulk_t*	next_page_bulk,
	bool		insert_father);

/** Create and initialize a new page bulk load state.
if page no is FIL_NULL, allocate a new page, or use the page.
Set whatever we can set in pager header at this point.
Note: when error occurs:
    1. page bulk should be freeed in the same function
if it's not in btr_bulk->page_bulks.
    2. page bulk should not be freeed if it's in btr_bulk->page_bulks.
@param[in]	page_no		page no
@param[in]	level		page level
@param[in]	trx_id		trx id
@param[in]	index		index dict
@return page bulk object */
static
page_bulk_t*
btr_bulk_load_page_create(
	ulint		page_no,
	ulint		level,
	ulint		trx_id,
	dict_index_t*	index)
{
	page_bulk_t*	page_bulk;
	mem_heap_t*	heap;
	mtr_t*		mtr;
	buf_block_t*	new_block;
	page_t*		new_page;
	page_zip_des_t*	new_page_zip;
	ulint		new_page_no;

	heap = mem_heap_create(1000);

	mtr = static_cast<mtr_t*>(
		mem_heap_alloc(heap, sizeof(mtr_t)));
	mtr_start(mtr);
	mtr_x_lock(dict_index_get_lock(index), mtr);

	if (dict_table_is_temporary(index->table)) {
		mtr_set_log_mode(mtr, MTR_LOG_NO_REDO);
	}

	if (page_no == FIL_NULL) {
		/* Allocate a new page. */
		new_block = btr_page_alloc(index, 0, FSP_NO_DIR, level, mtr, mtr);

		new_page = buf_block_get_frame(new_block);
		new_page_zip = buf_block_get_page_zip(new_block);
		new_page_no = page_get_page_no(new_page);

		if (new_page_zip) {
			page_create_zip(new_block, index, level, 0, NULL, mtr);
		} else {
			page_create(new_block, mtr, dict_table_is_comp(index->table));
			/* Set the level of the new index page */
			btr_page_set_level(new_page, NULL, level, mtr);
		}

		btr_page_set_next(new_page, new_page_zip, FIL_NULL, mtr);
		btr_page_set_prev(new_page, new_page_zip, FIL_NULL, mtr);

		btr_page_set_index_id(new_page, new_page_zip, index->id, mtr);
	} else {
		page_id_t	page_id(dict_index_get_space(index), page_no);
		page_size_t	page_size(dict_table_page_size(index->table));

		new_block = btr_block_get(page_id, page_size,
					  RW_X_LATCH, index, mtr);

		new_page = buf_block_get_frame(new_block);
		new_page_zip = buf_block_get_page_zip(new_block);
		new_page_no = page_get_page_no(new_page);

		ut_ad(page_dir_get_n_heap(new_page) == PAGE_HEAP_NO_USER_LOW);

		btr_page_set_level(new_page, NULL, level, mtr);
	}

	new_block->check_index_page_at_flush = FALSE;
	if (!dict_index_is_clust(index)) {
		page_set_max_trx_id(new_block, NULL,
				    trx_id, mtr);
	}

	page_bulk = static_cast<page_bulk_t*>(mem_heap_alloc(
		heap, sizeof(page_bulk_t)));
	page_bulk->heap = heap;
	page_bulk->index = index;
	page_bulk->mtr = mtr;
	page_bulk->logging = !dict_table_is_temporary(index->table);
	page_bulk->block = new_block;
	page_bulk->page = new_page;
	page_bulk->page_zip = new_page_zip;
	page_bulk->page_no = new_page_no;
	page_bulk->cur_rec = page_get_infimum_rec(new_page);
	page_bulk->is_comp = page_is_comp(new_page);
	page_bulk->free_space = page_get_free_space_of_empty(page_bulk->is_comp);
	page_bulk->fill_space =
		UNIV_PAGE_SIZE * (100 - innobase_index_fill_factor) / 100;
	page_bulk->pad_space =
		UNIV_PAGE_SIZE - dict_index_zip_pad_optimal_page_size(index);
	page_bulk->heap_top = page_header_get_ptr(new_page, PAGE_HEAP_TOP);
	page_bulk->level = level;
	page_bulk->heap_no = page_dir_get_n_heap(new_page);
	page_bulk->rec_no = page_header_get_field(new_page, PAGE_N_RECS);

	return(page_bulk);
}

/** Page bulk load ends
Scan all records to set page dirs, and set page header members.
@param[in]	page_bulk	page bulk load state
*/
static
void
btr_bulk_load_page_end(
	page_bulk_t*	page_bulk)
{
	page_t*		page;
	rec_t*		insert_rec;
	rec_t*		prev_rec;
	ulint		count;
	ulint		n_recs;
	ulint		slot_index;
/* log start */
	ulint		rec_size;
	byte*		log_ptr;
	ulint		log_data_len;
	mtr_log_t	log_mode;
/* log end */
	mtr_t*		mtr;
	page_dir_slot_t* slot = NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets = offsets_;
	dict_index_t*	index;

	rec_offs_init(offsets_);

	index = page_bulk->index;
	mtr = page_bulk->mtr;
	page = page_bulk->page;

	/* Set n recs */
	ut_ad(page_bulk->heap_no > PAGE_HEAP_NO_USER_LOW);
	ut_ad(page_bulk->heap_no == page_bulk->rec_no + PAGE_HEAP_NO_USER_LOW);
	page_header_set_field(page, NULL, PAGE_N_RECS, page_bulk->rec_no);
	page_dir_set_n_heap(page, NULL, page_bulk->heap_no);
	page_header_set_ptr(page, NULL, PAGE_HEAP_TOP, page_bulk->heap_top);

	/* Update the last insert info. */
	page_header_set_field(page, NULL, PAGE_DIRECTION, PAGE_RIGHT);
	page_header_set_field(page, NULL, PAGE_N_DIRECTION, 0);
	page_header_set_ptr(page, NULL, PAGE_LAST_INSERT, page_bulk->cur_rec);

	if (page_bulk->logging && page_bulk->page_zip == NULL) {
		/* TODO: if we use logging, we can simply logging the whole page. */
		/* Log bulk insert, refer to page_copy_rec_list_end_to_created_page */
		log_ptr = page_copy_rec_list_to_created_page_write_log(page, index, mtr);
		log_data_len = mtr->get_log()->size();
		log_mode = mtr_set_log_mode(mtr, MTR_LOG_SHORT_INSERTS);
	}

#ifdef UNIV_DEBUG
	/* To pass the debug tests we have to set these dummy values
	in the debug version */
	page_dir_set_n_slots(page, NULL, UNIV_PAGE_SIZE / 2);
#endif

	/* Set owner & dir here. */
	count = 0;
	slot_index = 0;
	n_recs = 0;
	prev_rec = page_get_infimum_rec(page);
	insert_rec = page_rec_get_next(prev_rec);

	/* Refer to page_copy_rec_list_end_to_created_page */
	do {

		count++;
		n_recs++;

		if (UNIV_UNLIKELY
		    (count == (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2)) {

			slot_index++;

			slot = page_dir_get_nth_slot(page, slot_index);

			page_dir_slot_set_rec(slot, insert_rec);
			page_dir_slot_set_n_owned(slot, NULL, count);

			count = 0;
		}

		offsets = rec_get_offsets(insert_rec, index, offsets,
					  ULINT_UNDEFINED, &(page_bulk->heap));
		if (page_bulk->logging && page_bulk->page_zip == NULL) {
			rec_size = rec_offs_size(offsets);
			page_cur_insert_rec_write_log(insert_rec, rec_size, prev_rec,
						      index, mtr);
		}

		insert_rec = page_rec_get_next(insert_rec);
	} while (!page_rec_is_supremum(insert_rec));

	if ((slot_index > 0) && (count + 1
		+ (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2
		<= PAGE_DIR_SLOT_MAX_N_OWNED)) {
		/* We can merge the two last dir slots. This operation is
		here to make this function imitate exactly the equivalent
		task made using page_cur_insert_rec, which we use in database
		recovery to reproduce the task performed by this function.
		To be able to check the correctness of recovery, it is good
		that it imitates exactly. */

		count += (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2;

		page_dir_slot_set_n_owned(slot, NULL, 0);

		slot_index--;
	}

	slot = page_dir_get_nth_slot(page, 1 + slot_index);
	page_dir_slot_set_rec(slot, page_get_supremum_rec(page));
	page_dir_slot_set_n_owned(slot, NULL, count + 1);
	page_dir_set_n_slots(page, NULL, 2 + slot_index);

	if (page_bulk->logging && page_bulk->page_zip == NULL) {
		log_data_len = mtr->get_log()->size() - log_data_len;

		ut_a(log_data_len < 100 * UNIV_PAGE_SIZE);

		if (log_ptr != NULL) {
			mach_write_to_4(log_ptr, log_data_len);
		}

		/* Restore the log mode */
		mtr_set_log_mode(mtr, log_mode);
	}

	page_bulk->block->check_index_page_at_flush = TRUE;
}

/** Insert a record in a page.
We insert a record in a page and update related members in page_bulk.
@param[in/out]	page_bulk	page bulk load state
@param[in]	rec		record
@param[in]	offsets		record offsets */
static
void
btr_bulk_load_page_insert_low(
	page_bulk_t*	page_bulk,
	rec_t*		rec,
	ulint*		offsets)
{
	ulint		rec_size;
	ulint		heap_no;
	byte*		insert_buf;
	rec_t*		insert_rec;
	rec_t*		current_rec;
	rec_t*		next_rec;
	ulint		slot_size;

	ut_ad(page_bulk->heap_no == page_bulk->rec_no + PAGE_HEAP_NO_USER_LOW);

#ifdef UNIV_DEBUG
	if (!page_rec_is_infimum(page_bulk->cur_rec)) {
		dict_index_t*	index = page_bulk->index;
		rec_t*	old_rec = page_bulk->cur_rec;
		ulint*	old_offsets = rec_get_offsets(
			old_rec, index, NULL,
			ULINT_UNDEFINED, &page_bulk->heap);

		ut_ad(cmp_rec_rec(rec, old_rec, offsets, old_offsets, index)
		      > 0);
	}
#endif

	rec_size = rec_offs_size(offsets);

	/* 1. Get the insert space. (page_mem_alloc_heap)*/
	page_header_set_ptr(buf_block_get_frame(page_bulk->block), NULL,
			    PAGE_HEAP_TOP, page_bulk->heap_top + rec_size);
	insert_buf = page_bulk->heap_top;
	heap_no = page_bulk->heap_no;

	/* 2. Create the record. */
	insert_rec = rec_copy(insert_buf, rec, offsets);
	rec_offs_make_valid(insert_rec, page_bulk->index, offsets);

	/* 3. Insert the record in the linked list. */
	current_rec = page_bulk->cur_rec;
	next_rec = page_rec_get_next(current_rec);

	page_rec_set_next(insert_rec, next_rec);
	page_rec_set_next(current_rec, insert_rec);

	/* 4. Set the n_owned field in the inserted record to zero,
	and set the heap_no field. */
	if (page_bulk->is_comp) {
		rec_set_n_owned_new(insert_rec, NULL, 0);
		rec_set_heap_no_new(insert_rec, heap_no);
	} else {
		rec_set_n_owned_old(insert_rec, 0);
		rec_set_heap_no_old(insert_rec, heap_no);
	}

	/* 5. Update page_bulk. */
	slot_size = page_dir_calc_reserved_space(page_bulk->rec_no + 1)
		- page_dir_calc_reserved_space(page_bulk->rec_no);
	ut_ad(page_bulk->free_space >= rec_size + slot_size);
	page_bulk->free_space -= rec_size + slot_size;
	page_bulk->heap_top += rec_size;
	page_bulk->heap_no += 1;
	page_bulk->rec_no += 1;
	page_bulk->cur_rec = insert_rec;
}

/** Get split rec in a page
@param[in]	page_bulk	page bulk load state
@return split rec */
static
rec_t*
btr_bulk_load_page_get_split_rec(
	page_bulk_t*	page_bulk)
{
	page_t*		page;
	rec_t*		rec;
	ulint*		offsets;
	ulint		incl_data;
	ulint		total_space;
	ulint		n;

	ut_ad(page_bulk->page_zip != NULL);
	ut_ad(page_bulk->rec_no >= 2);

	page = page_bulk->page;
	total_space = page_get_free_space_of_empty(page_bulk->is_comp)
		- page_bulk->free_space;

	incl_data = 0;
	n = 0;
	offsets = NULL;
	rec = page_get_infimum_rec(page);

	do {
		rec = page_rec_get_next(rec);
		offsets = rec_get_offsets(rec, page_bulk->index,
					  offsets, ULINT_UNDEFINED,
					  &(page_bulk->heap));
		incl_data += rec_offs_size(offsets);
		n++;

	} while (!page_rec_is_supremum(rec)
		 && incl_data + page_dir_calc_reserved_space(n)
		 < total_space / 2);

	if (page_rec_is_infimum(rec)) {
		rec = page_rec_get_next(rec);
		ut_ad(page_rec_is_supremum(rec));
	} else if (page_rec_is_supremum(rec)) {
		rec = page_rec_get_prev(rec);
		ut_ad(page_rec_is_infimum(rec));
	}

	return rec;
}

/** Page copy starts
Copy records to a new page.
Note: we hold the split page in another page_bulk.
@param[in/out]	page_bulk	page bulk load state
@param[in]	split_rec	split rec to start copy
*/
static
void
btr_bulk_load_page_copy_start(
	page_bulk_t*	page_bulk,
	rec_t*		split_rec)
{
	rec_t*		rec;
	ulint*		offsets;

	ut_ad(page_bulk->rec_no == 0);

	rec = split_rec;
	offsets  = NULL;

	ut_ad(page_bulk->rec_no == 0);
	ut_ad(!page_rec_is_infimum(rec)
	      && !page_rec_is_supremum(rec));

	do {
		offsets = rec_get_offsets(rec, page_bulk->index,
					  offsets, ULINT_UNDEFINED,
					  &(page_bulk->heap));

		btr_bulk_load_page_insert_low(page_bulk, rec, offsets);

		rec = page_rec_get_next(rec);
	} while (!page_rec_is_supremum(rec));

	ut_ad(page_bulk->rec_no > 0);
}

/** Page copy ends
Update the split page.
@param[in/out]	page_bulk	page bulk load state
@param[in]	spit_rec	split rec to start copy
*/
static
void
btr_bulk_load_page_copy_end(
	page_bulk_t*	page_bulk,
	rec_t*		split_rec)
{
	page_t*		page;
	rec_t*		rec;
	rec_t*		last_rec;
	ulint*		offsets;
	ulint		n;

	page = page_bulk->page;
	last_rec = page_rec_get_prev(page_get_supremum_rec(page));

	n = 0;
	rec = page_rec_get_next(page_get_infimum_rec(page));

	while (rec != split_rec) {
		rec = page_rec_get_next(rec);
		n++;
	}

	ut_ad(n > 0);
	offsets = NULL;
	rec = page_rec_get_prev(split_rec);
	offsets = rec_get_offsets(rec, page_bulk->index,
				  offsets, ULINT_UNDEFINED,
				  &(page_bulk->heap));
	page_rec_set_next(rec, page_get_supremum_rec(page));

	page_bulk->cur_rec = rec;
	page_bulk->heap_top = rec_get_end(rec, offsets);

	offsets = rec_get_offsets(last_rec, page_bulk->index,
				  offsets, ULINT_UNDEFINED,
				  &(page_bulk->heap));

	page_bulk->free_space += rec_get_end(last_rec, offsets)
		- page_bulk->heap_top
		+ page_dir_calc_reserved_space(page_bulk->rec_no)
		- page_dir_calc_reserved_space(n);
	ut_ad(page_bulk->free_space > 0);
	page_bulk->heap_no = n + PAGE_HEAP_NO_USER_LOW;
	page_bulk->rec_no = n;
}

/** Page split when compression fails
@param[in/out]	btr_bulk	btr bulk load state
@param[in/out]	page_bulk	page bulk load state to split
@param[in/out]	next_page_bulk	page bulk load state of next page
@return error code */
static
dberr_t
btr_bulk_load_page_split(
	btr_bulk_t*	btr_bulk,
	page_bulk_t*	page_bulk,
	page_bulk_t*	next_page_bulk)
{
	dberr_t		err = DB_SUCCESS;
	page_bulk_t*	new_page_bulk;
	rec_t*		split_rec;

	ut_ad(page_bulk->page_zip != NULL);

	/* 1. Check if we have only one user record on the page. */
	if (page_bulk->rec_no <= 1) {
		return DB_TOO_BIG_RECORD;
	}

	/* 2. create a new page. */
	new_page_bulk = btr_bulk_load_page_create(FIL_NULL, page_bulk->level,
		btr_bulk->trx_id, btr_bulk->index);

	/* 3. copy the upper half to new page. */
	split_rec = btr_bulk_load_page_get_split_rec(page_bulk);
	btr_bulk_load_page_copy_start(new_page_bulk, split_rec);
	btr_bulk_load_page_copy_end(page_bulk, split_rec);

	/* 4. commit the splitted page. */
	err = btr_bulk_load_page_commit(btr_bulk, page_bulk, new_page_bulk, true);
	if (err != DB_SUCCESS) {
		mtr_commit(new_page_bulk->mtr);
		mem_heap_free(new_page_bulk->heap);

		return(err);
	}

	/* 5. commit the new page. */
	err = btr_bulk_load_page_commit(btr_bulk, new_page_bulk, next_page_bulk, true);
	if (err != DB_SUCCESS) {
		mtr_commit(new_page_bulk->mtr);
		mem_heap_free(new_page_bulk->heap);

		return(err);
	}

	return(err);
}

/** Insert a node pointer to father page
@param[in/out]	btr_bulk	btr bulk load state
@param[in]	page_bulk	page_bulk load state
@return error code */
static
dberr_t
btr_bulk_load_father_page_insert(
	btr_bulk_t*	btr_bulk,
	page_bulk_t*	page_bulk)
{
	page_bulk_t*	father_page_bulk;
	ulint		cur_level;
	rec_t*		first_rec;
	dtuple_t*	node_ptr;
	dict_index_t*	index;
	dberr_t		err = DB_SUCCESS;

	index = btr_bulk->index;

	/* Create node pointer */
	first_rec = page_rec_get_next(page_get_infimum_rec(page_bulk->page));
	ut_a(page_rec_is_user_rec(first_rec));
	node_ptr = dict_index_build_node_ptr(
		index, first_rec, page_bulk->page_no,
		page_bulk->heap, page_bulk->level);

	/* Get father page bulk. */
	cur_level = page_bulk->level;
	if (cur_level + 1 <= btr_bulk->root_level) {
		father_page_bulk = btr_bulk->page_bulks->at(cur_level + 1);
	} else {
		ulint	page_bulks_size;

		btr_bulk->root_level += 1;
		ut_ad(btr_bulk->root_level = cur_level + 1);

		page_bulks_size = btr_bulk->page_bulks->size();
		if (btr_bulk->root_level >= page_bulks_size) {
			btr_bulk->page_bulks->resize(
				page_bulks_size * 2, NULL);
		}

		father_page_bulk = btr_bulk_load_page_create(FIL_NULL,
			cur_level + 1,
			btr_bulk->trx_id,
			btr_bulk->index);
		btr_bulk->page_bulks->at(cur_level + 1) = father_page_bulk;

		/* The node pointer must be marked as the predefined minimum record,
		as there is no lower alphabetical limit to records in the leftmost
		node of a level: */
		dtuple_set_info_bits(node_ptr,
			dtuple_get_info_bits(node_ptr)
			| REC_INFO_MIN_REC_FLAG);
	}

	/* Insert node pointer in parent. */
	err = btr_bulk_load_page_insert(btr_bulk, father_page_bulk, node_ptr);

	return err;
}

/** Commit mtr for a page when it's full.
We set page links in this function.
@param[in/out]	btr_bulk	btr bulk load state
@param[in]	page_bulk	page bulk load state
@param[in/out]	next_page_bulk	page bulk load state of next page
@param[in]	insert_father	whether insert a node ptr to father page,
it's false only when the page is root.
@return error code */
dberr_t
btr_bulk_load_page_commit(
	btr_bulk_t*	btr_bulk,
	page_bulk_t*	page_bulk,
	page_bulk_t*	next_page_bulk,
	bool		insert_father)
{
	dberr_t		err = DB_SUCCESS;

	btr_bulk_load_page_end(page_bulk);

	/* Set page links */
	if (next_page_bulk != NULL) {
		ut_ad(page_bulk->level == next_page_bulk->level);
		btr_page_set_next(page_bulk->page, page_bulk->page_zip,
				  next_page_bulk->page_no, page_bulk->mtr);
		btr_page_set_prev(next_page_bulk->page, next_page_bulk->page_zip,
				  page_bulk->page_no, next_page_bulk->mtr);
	}

	if (page_bulk->page_zip != NULL) {
		ulint	zip_level = page_zip_level;

		/* Debug page split with Z_NO_COMPRESSION.*/
		DBUG_EXECUTE_IF("btr_bulk_load_page_split_instrument",
			zip_level = 0;
		);

		if (!page_zip_compress(page_bulk->page_zip, page_bulk->page,
			btr_bulk->index, zip_level, NULL, page_bulk->mtr)) {
			err = btr_bulk_load_page_split(btr_bulk, page_bulk, next_page_bulk);

			return(err);
		}
	}

	if (insert_father) {
		/* Insert into father page. */
		err = btr_bulk_load_father_page_insert(btr_bulk, page_bulk);
		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	ut_ad(page_validate(page_bulk->page, btr_bulk->index));

	/* Commit mtr & free page_bulk. */
	mtr_commit(page_bulk->mtr);
	mem_heap_free(page_bulk->heap);

	return(err);
}

/** Check if required length is available in the page.
We check fill factor & padding here.
@param[in]	page_bulk	page bulk load state
@param[in]	length		required length
@retval	true	if space is available
@retval	false	if no space is available */
UNIV_INLINE
bool
btr_bulk_load_page_available(
	page_bulk_t*	page_bulk,
	ulint		length
)
{
	if (length > page_bulk->free_space) {
		ut_ad(page_bulk->rec_no > 0);
		return false;
	}

	/* Fillfactor & Padding only apply to leaf pages. */
	/* TODO: ask facebook why pad only applies to leaf pages? */
	if (page_is_leaf(page_bulk->page)
	    && page_bulk->rec_no > 0
	    && ((page_bulk->page_zip == NULL
	    && page_bulk->free_space < page_bulk->fill_space)
	    || (page_bulk->page_zip != NULL
	    && page_bulk->free_space < page_bulk->pad_space))) {
		return(false);
	}

	return(true);
}

/** Insert a tupe in a page.
We do tuple conversion, free space check and blob insert.
@param[in,out]	btr_bulk	btree bulk load state
@param[in,out]	page_bulk	page bulk load state
@param[in]	tuple		tuple to insert
@return error code */
static
dberr_t
btr_bulk_load_page_insert(
	btr_bulk_t*	btr_bulk,
	page_bulk_t*	page_bulk,
	dtuple_t*	tuple)
{
	dict_index_t*	index;
	ulint		rec_size;
	ulint		n_ext = 0;
	big_rec_t*	big_rec = NULL;
	ulint*		offsets;
	rec_t*		rec;
	ulint		slot_size;
	dberr_t		err = DB_SUCCESS;

	index = btr_bulk->index;

	/* Calculate the record size when entry is converted to a record */
        rec_size = rec_get_converted_size(index, tuple, 0);
	if (page_zip_rec_needs_ext(rec_size, page_bulk->is_comp,
	    dtuple_get_n_fields(tuple), page_bulk->block->page.size)) {

		/* The record is so big that we have to store some fields
		externally on separate database pages */
		big_rec = dtuple_convert_big_rec(index, tuple, &n_ext);

		if (UNIV_UNLIKELY(big_rec == NULL)) {
			return(DB_TOO_BIG_RECORD);
		}

		rec_size = rec_get_converted_size(index, tuple, n_ext);
	}

	slot_size = page_dir_calc_reserved_space(page_bulk->rec_no + 1)
		- page_dir_calc_reserved_space(page_bulk->rec_no);

	if (!btr_bulk_load_page_available(page_bulk, rec_size + slot_size)) {
		page_bulk_t*	sibling_page_bulk;

		/* Create a sibling page_bulk. */
		sibling_page_bulk = btr_bulk_load_page_create(FIL_NULL,
			page_bulk->level,
			btr_bulk->trx_id,
			btr_bulk->index);

		/* Commit page bulk. */
		err = btr_bulk_load_page_commit(btr_bulk, page_bulk, sibling_page_bulk, true);
		if (err != DB_SUCCESS) {
			mtr_commit(sibling_page_bulk->mtr);
			mem_heap_free(sibling_page_bulk->heap);

			return(err);
		}

		/* Set new page bulk to page_bulks. */
		ut_ad(sibling_page_bulk->level <= btr_bulk->root_level);
		btr_bulk->page_bulks->at(sibling_page_bulk->level) = sibling_page_bulk;

		page_bulk = sibling_page_bulk;

		/* Important: log_free_check whether we need a checkpoint. */
		if (page_is_leaf(sibling_page_bulk->page)) {
			for (ulint level = 0; level <= btr_bulk->root_level; level++) {
				page_bulk_t*    page_bulk;

				page_bulk = btr_bulk->page_bulks->at(level);
				mtr_commit(page_bulk->mtr);
			}

			log_free_check();

			for (ulint level = 0; level <= btr_bulk->root_level; level++) {
				page_bulk_t*    page_bulk = btr_bulk->page_bulks->at(level);
				dict_index_t*	index = page_bulk->index;
				page_id_t	page_id(dict_index_get_space(index),
							page_bulk->page_no);
				page_size_t	page_size(dict_table_page_size(index->table));

				mtr_start(page_bulk->mtr);
				if (!page_bulk->logging) {
					mtr_set_log_mode(page_bulk->mtr, MTR_LOG_NO_REDO);
				}

				page_bulk->block = btr_block_get(page_id, page_size,
					RW_X_LATCH, index, page_bulk->mtr);
				page_bulk->page = buf_block_get_frame(page_bulk->block);
			}
		}
	}

	/* Convert tuple to rec. */
	offsets = NULL;
        rec = rec_convert_dtuple_to_rec(static_cast<byte*>(mem_heap_alloc(
		page_bulk->heap, rec_size)), index, tuple, n_ext);
        offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
		&(page_bulk->heap));

	btr_bulk_load_page_insert_low(page_bulk, rec, offsets);

	if (big_rec != NULL) {
		ut_ad(err == DB_SUCCESS);
		ut_ad(dict_index_is_clust(index));
		ut_ad(page_bulk->level == 0);

		err = btr_store_big_rec_extern_fields(
			index, page_bulk->block,
			page_bulk->cur_rec, offsets, big_rec,
			page_bulk->mtr, BTR_STORE_INSERT);
	}

	return(err);
}

/** Insert a tuple to btree
@param[in/out]	btr_bulk	btr bulk load state
@param[in]	tuple		tuple to insert
@return error code */
dberr_t
btr_bulk_load_insert(
	btr_bulk_t*	btr_bulk,
	dtuple_t*	tuple)
{
	page_bulk_t*	leaf_page_bulk;
	dberr_t		err;

	leaf_page_bulk = btr_bulk->page_bulks->front();

	err = btr_bulk_load_page_insert(btr_bulk, leaf_page_bulk, tuple);

	return(err);
}

/** Btree bulk load init
@param[in/out]	btr_bulk	btr bulk load state
@param[in]	index		index dict
@param[in]	trx_id		trx id
*/
void
btr_bulk_load_init(
	btr_bulk_t*	btr_bulk,
	dict_index_t*	index,
	trx_id_t	trx_id)
{
	page_bulk_t*	page_bulk;

	/* Init btr bulk */
	btr_bulk->heap = mem_heap_create(1000);
	btr_bulk->index = index;
	btr_bulk->trx_id = trx_id;
	btr_bulk->root_level = 0;

	page_bulk = btr_bulk_load_page_create(FIL_NULL, 0, btr_bulk->trx_id,
					      btr_bulk->index);

	btr_bulk->page_bulks = new page_bulk_vector(10);
	btr_bulk->page_bulks->at(0) = page_bulk;

	return;
};

/** Btree bulkd load deinit
@param[in,out]	btr_bulk	btr bulk load state
@param[in]	success		whether bulk load is successful
@return error code */
dberr_t
btr_bulk_load_deinit(
	btr_bulk_t*	btr_bulk,
	bool		success)
{
	page_bulk_t*	root_page_bulk;
	dict_index_t*	index;
	ulint		root_page_no;
	ulint		last_page_no;
	dberr_t		err = DB_SUCCESS;

	if (!success) {
		/* Free all page bulks. */
		for (ulint level = 0; level <= btr_bulk->root_level; level++) {
			page_bulk_t*	page_bulk =
				btr_bulk->page_bulks->at(level);

			mtr_commit(page_bulk->mtr);
			mem_heap_free(page_bulk->heap);
		}

		goto func_exit;
	}

	index = btr_bulk->index;

	/* Finish all page bulks except root level.*/
	for (ulint level = 0; level <= btr_bulk->root_level; level++) {
		page_bulk_t*	page_bulk = btr_bulk->page_bulks->at(level);

		last_page_no = page_bulk->page_no;

		if (err != DB_SUCCESS) {
			mtr_commit(page_bulk->mtr);
			mem_heap_free(page_bulk->heap);
		} else {
			bool	insert_father = true;

			if (level == btr_bulk->root_level) {
				insert_father = false;
			}

			err = btr_bulk_load_page_commit(btr_bulk, page_bulk,
				NULL, insert_father);
		}
	}

	/* Copy root level page in page_bulks to root page. */
	root_page_no = dict_index_get_page(index);
	root_page_bulk = btr_bulk_load_page_create(root_page_no,
		btr_bulk->root_level,
		btr_bulk->trx_id,
		btr_bulk->index);

	{
		rec_t*		rec;
		mtr_t*		mtr;
		buf_block_t*	last_block;
		page_t*		last_page;
		page_id_t	page_id(dict_index_get_space(index), last_page_no);
		page_size_t	page_size(dict_table_page_size(index->table));

		mtr = root_page_bulk->mtr;

		ut_ad(last_page_no != FIL_NULL);
		last_block = btr_block_get(page_id, page_size,
					   RW_X_LATCH, index, mtr);
		last_page = buf_block_get_frame(last_block);
		rec = page_rec_get_next(page_get_infimum_rec(last_page));

		/* Copy last page to root page. */
		btr_bulk_load_page_copy_start(root_page_bulk, rec);

		/* Remove last page. */
		btr_page_free_low(index, last_block, btr_bulk->root_level, mtr);
	}

	err = btr_bulk_load_page_commit(btr_bulk, root_page_bulk, NULL, false);
	ut_ad(err == DB_SUCCESS);

func_exit:
	delete btr_bulk->page_bulks;
	mem_heap_free(btr_bulk->heap);

	return(err);
}
