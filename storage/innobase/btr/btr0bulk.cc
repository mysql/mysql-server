/*****************************************************************************

Copyright (c) 2014, Oracle and/or its affiliates. All Rights Reserved.

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

Created 03/11/2014 Shaohua Wang
*******************************************************/

#include "btr0bulk.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "ibuf0ibuf.h"

char	innobase_enable_bulk_load;

/* Innodb index fill factor during index build. */
long	innobase_index_fill_factor;

/** Initialize members.
Allocate page and mtr. */
void PageBulk::init()
{
	mtr_t*		mtr;
	buf_block_t*	new_block;
	page_t*		new_page;
	page_zip_des_t*	new_page_zip;
	ulint		new_page_no;

	mtr = static_cast<mtr_t*>(
		mem_heap_alloc(m_heap, sizeof(mtr_t)));
	mtr_start(mtr);
	mtr_x_lock(dict_index_get_lock(m_index), mtr);

	if (dict_table_is_temporary(m_index->table)) {
		mtr_set_log_mode(mtr, MTR_LOG_NO_REDO);
	}

	if (m_page_no == FIL_NULL) {
		/* Allocate a new page. */
		new_block = btr_page_alloc(m_index, 0, FSP_NO_DIR, m_level,
					   mtr, mtr);

		new_page = buf_block_get_frame(new_block);
		new_page_zip = buf_block_get_page_zip(new_block);
		new_page_no = page_get_page_no(new_page);

		if (new_page_zip) {
			page_create_zip(new_block, m_index, m_level, 0,
					NULL, mtr);
		} else {
			page_create(new_block, mtr,
				    dict_table_is_comp(m_index->table));
			btr_page_set_level(new_page, NULL, m_level, mtr);
		}

		btr_page_set_next(new_page, new_page_zip, FIL_NULL, mtr);
		btr_page_set_prev(new_page, new_page_zip, FIL_NULL, mtr);

		btr_page_set_index_id(new_page, new_page_zip, m_index->id, mtr);
	} else {
		page_id_t	page_id(dict_index_get_space(m_index), m_page_no);
		page_size_t	page_size(dict_table_page_size(m_index->table));

		new_block = btr_block_get(page_id, page_size,
					  RW_X_LATCH, m_index, mtr);

		new_page = buf_block_get_frame(new_block);
		new_page_zip = buf_block_get_page_zip(new_block);
		new_page_no = page_get_page_no(new_page);

		ut_ad(page_dir_get_n_heap(new_page) == PAGE_HEAP_NO_USER_LOW);

		btr_page_set_level(new_page, NULL, m_level, mtr);
	}

	new_block->check_index_page_at_flush = FALSE;
	if (!dict_index_is_clust(m_index)) {
		page_set_max_trx_id(new_block, NULL, m_trx_id, mtr);
	}

	m_mtr = mtr;
	m_log = !dict_table_is_temporary(m_index->table);
	m_block = new_block;
	m_page = new_page;
	m_page_zip = new_page_zip;
	m_page_no = new_page_no;
	m_cur_rec = page_get_infimum_rec(new_page);
	m_is_comp = page_is_comp(new_page);
	m_free_space = page_get_free_space_of_empty(m_is_comp);
	m_fill_space =
		UNIV_PAGE_SIZE * (100 - innobase_index_fill_factor) / 100;
	m_pad_space =
		UNIV_PAGE_SIZE - dict_index_zip_pad_optimal_page_size(m_index);
	m_heap_top = page_header_get_ptr(new_page, PAGE_HEAP_TOP);
	m_heap_no = page_dir_get_n_heap(new_page);
	m_rec_no = page_header_get_field(new_page, PAGE_N_RECS);
}

/** Insert a record in the page.
We insert a record in the page and update releated members,
it should succeed.
@param[in]	rec		record
@param[in]	offsets		record offsets */
void PageBulk::insert(
	rec_t*		rec,
	ulint*		offsets)
{
	ulint		rec_size;
	rec_t*		insert_rec;
	rec_t*		next_rec;
	ulint		slot_size;

	ut_ad(m_heap_no == m_rec_no + PAGE_HEAP_NO_USER_LOW);

#ifdef UNIV_DEBUG
	/** Check whether records are in order. */
	if (!page_rec_is_infimum(m_cur_rec)) {
		rec_t*	old_rec = m_cur_rec;
		ulint*	old_offsets = rec_get_offsets(
			old_rec, m_index, NULL,
			ULINT_UNDEFINED, &m_heap);

		ut_ad(cmp_rec_rec(rec, old_rec, offsets, old_offsets, m_index)
		      > 0);
	}
#endif

	rec_size = rec_offs_size(offsets);

	/* 1. Get the insert space. (page_mem_alloc_heap)*/
	page_header_set_ptr(m_page, NULL, PAGE_HEAP_TOP, m_heap_top + rec_size);

	/* 2. Create the record. */
	insert_rec = rec_copy(m_heap_top, rec, offsets);
	rec_offs_make_valid(insert_rec, m_index, offsets);

	/* 3. Insert the record in the linked list. */
	next_rec = page_rec_get_next(m_cur_rec);

	page_rec_set_next(insert_rec, next_rec);
	page_rec_set_next(m_cur_rec, insert_rec);

	/* 4. Set the n_owned field in the inserted record to zero,
	and set the heap_no field. */
	if (m_is_comp) {
		rec_set_n_owned_new(insert_rec, NULL, 0);
		rec_set_heap_no_new(insert_rec, m_heap_no);
	} else {
		rec_set_n_owned_old(insert_rec, 0);
		rec_set_heap_no_old(insert_rec, m_heap_no);
	}

	/* 5. Update page_bulk. */
	slot_size = page_dir_calc_reserved_space(m_rec_no + 1)
		- page_dir_calc_reserved_space(m_rec_no);
	ut_ad(m_free_space >= rec_size + slot_size);
	m_free_space -= rec_size + slot_size;
	m_heap_top += rec_size;
	m_heap_no += 1;
	m_rec_no += 1;
	m_cur_rec = insert_rec;
}

/** Finish a page
Scan all records to set page dirs, and set page header members,
redo log all inserts. */
void PageBulk::finish()
{
	rec_t*		insert_rec;
	rec_t*		prev_rec;
	ulint		count;
	ulint		n_recs;
	ulint		slot_index;
	ulint		rec_size;
	byte*		log_ptr = NULL;
	ulint		log_data_len = 0;
	mtr_log_t	log_mode = MTR_LOG_ALL;
	bool		log_insert;
	page_dir_slot_t* slot = NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets = offsets_;

	rec_offs_init(offsets_);

	/* Set n recs */
	ut_ad(m_heap_no > PAGE_HEAP_NO_USER_LOW);
	ut_ad(m_heap_no == m_rec_no + PAGE_HEAP_NO_USER_LOW);
	page_header_set_field(m_page, NULL, PAGE_N_RECS, m_rec_no);
	page_dir_set_n_heap(m_page, NULL, m_heap_no);
	page_header_set_ptr(m_page, NULL, PAGE_HEAP_TOP, m_heap_top);

	/* Update the last insert info. */
	page_header_set_field(m_page, NULL, PAGE_DIRECTION, PAGE_RIGHT);
	page_header_set_field(m_page, NULL, PAGE_N_DIRECTION, 0);
	page_header_set_ptr(m_page, NULL, PAGE_LAST_INSERT, m_cur_rec);

	/* We need to log insert for non-compressed table,
	and we have page_zip_log_pages for compressed table. */
	log_insert = m_log && (m_page_zip == NULL);

	if (log_insert) {
		/* Fixme: if we use logging, we can simply logging the
		whole page. */
		log_ptr = page_copy_rec_list_to_created_page_write_log(
			m_page, m_index, m_mtr);
		log_data_len = m_mtr->get_log()->size();
		log_mode = mtr_set_log_mode(m_mtr, MTR_LOG_SHORT_INSERTS);
	}

#ifdef UNIV_DEBUG
	/* To pass the debug tests we have to set these dummy values
	in the debug version */
	page_dir_set_n_slots(m_page, NULL, UNIV_PAGE_SIZE / 2);
#endif

	/* Set owner & dir here. */
	count = 0;
	slot_index = 0;
	n_recs = 0;
	prev_rec = page_get_infimum_rec(m_page);
	insert_rec = page_rec_get_next(prev_rec);

	/* We refer to page_copy_rec_list_end_to_created_page */
	do {

		count++;
		n_recs++;

		if (UNIV_UNLIKELY
		    (count == (PAGE_DIR_SLOT_MAX_N_OWNED + 1) / 2)) {

			slot_index++;

			slot = page_dir_get_nth_slot(m_page, slot_index);

			page_dir_slot_set_rec(slot, insert_rec);
			page_dir_slot_set_n_owned(slot, NULL, count);

			count = 0;
		}

		if (log_insert) {
			offsets = rec_get_offsets(insert_rec, m_index, offsets,
					  ULINT_UNDEFINED, &m_heap);
			rec_size = rec_offs_size(offsets);
			page_cur_insert_rec_write_log(insert_rec, rec_size,
						      prev_rec, m_index, m_mtr);
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

	slot = page_dir_get_nth_slot(m_page, 1 + slot_index);
	page_dir_slot_set_rec(slot, page_get_supremum_rec(m_page));
	page_dir_slot_set_n_owned(slot, NULL, count + 1);
	page_dir_set_n_slots(m_page, NULL, 2 + slot_index);

	if (log_insert) {
		log_data_len = m_mtr->get_log()->size() - log_data_len;

		ut_a(log_data_len < 100 * UNIV_PAGE_SIZE);

		if (log_ptr != NULL) {
			mach_write_to_4(log_ptr, log_data_len);
		}

		/* Restore the log mode */
		mtr_set_log_mode(m_mtr, log_mode);
	}

	m_block->check_index_page_at_flush = TRUE;
}

/** Commit mtr for a page
@param[in]	success		Flag whether all inserts succeed.
@return error code */
void PageBulk::commit(bool	success)
{
	if (success) {
		ut_ad(page_validate(m_page, m_index));

		/* Set no free space left and no buffered changes in ibuf. */
		if (!dict_index_is_clust(m_index)
		    && !dict_table_is_temporary(m_index->table)
		    && page_is_leaf(m_page)) {
			ibuf_set_bitmap_for_bulk_load(
				m_block, innobase_index_fill_factor == 100);
        	}
	}

	mtr_commit(m_mtr);
}

/** Compress if it is compressed table
@return	true	compress successfully or no need to compress
@return	false	compress failed. */
bool PageBulk::compress()
{
	bool	ret = true;

	ut_ad(m_page_zip != NULL && page_zip_log_pages);

	ulint   zip_level = page_zip_level;

	/* Debug page split with Z_NO_COMPRESSION.*/
	DBUG_EXECUTE_IF("btr_bulk_load_page_split_instrument",
		zip_level = 0;
	);

	if (!page_zip_compress(m_page_zip, m_page, m_index,
			       zip_level, NULL, m_mtr)) {
		ret = false;
	}

	return(ret);
}

/** Get node pointer
Note: should before mtr commit */
dtuple_t* PageBulk::getNodePtr()
{
	rec_t*		first_rec;
	dtuple_t*	node_ptr;

	/* Create node pointer */
	first_rec = page_rec_get_next(page_get_infimum_rec(m_page));
	ut_a(page_rec_is_user_rec(first_rec));
	node_ptr = dict_index_build_node_ptr(m_index, first_rec, m_page_no,
					     m_heap, m_level);

	return(node_ptr);
}

/** Get split rec in the page.
We split a page in half when compresssion fails, and the split rec
should be copied to the new page.
@return split rec */
rec_t*	PageBulk::getSplitRec()
{
	rec_t*		rec;
	ulint*		offsets;
	ulint		incl_data;
	ulint		total_space;
	ulint		n;

	ut_ad(m_page_zip != NULL);
	ut_ad(m_rec_no >= 2);

	total_space = page_get_free_space_of_empty(m_is_comp)
		- m_free_space;

	incl_data = 0;
	n = 0;
	offsets = NULL;
	rec = page_get_infimum_rec(m_page);

	do {
		rec = page_rec_get_next(rec);
		offsets = rec_get_offsets(rec, m_index,
					  offsets, ULINT_UNDEFINED,
					  &(m_heap));
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

	return(rec);
}

/** Copy all records after split rec including itself.
@param[in]	rec	split rec
Note: the page where split rec resizes is locked by another mtr.*/
void PageBulk::copyIn(rec_t*	split_rec)
{

	rec_t*		rec;
	ulint*		offsets;

	ut_ad(m_rec_no == 0);

	rec = split_rec;
	offsets  = NULL;

	ut_ad(m_rec_no == 0);
	ut_ad(page_rec_is_user_rec(rec));

	do {
		offsets = rec_get_offsets(rec, m_index, offsets,
					  ULINT_UNDEFINED, &(m_heap));

		insert(rec, offsets);

		rec = page_rec_get_next(rec);
	} while (!page_rec_is_supremum(rec));

	ut_ad(m_rec_no > 0);
}

/** Remove all records after split rec including itself.
@param[in]	rec	split rec	*/
void PageBulk::copyOut(rec_t*	split_rec)
{
	rec_t*		rec;
	rec_t*		last_rec;
	ulint*		offsets;
	ulint		n;

	last_rec = page_rec_get_prev(page_get_supremum_rec(m_page));

	n = 0;
	rec = page_rec_get_next(page_get_infimum_rec(m_page));

	while (rec != split_rec) {
		rec = page_rec_get_next(rec);
		n++;
	}

	/* Set last record's next in page */
	ut_ad(n > 0);
	offsets = NULL;
	rec = page_rec_get_prev(split_rec);
	offsets = rec_get_offsets(rec, m_index,
				  offsets, ULINT_UNDEFINED,
				  &(m_heap));
	page_rec_set_next(rec, page_get_supremum_rec(m_page));

	/* Set related members */
	m_cur_rec = rec;
	m_heap_top = rec_get_end(rec, offsets);

	offsets = rec_get_offsets(last_rec, m_index,
				  offsets, ULINT_UNDEFINED,
				  &(m_heap));

	m_free_space += rec_get_end(last_rec, offsets)
		- m_heap_top
		+ page_dir_calc_reserved_space(m_rec_no)
		- page_dir_calc_reserved_space(n);
	ut_ad(m_free_space > 0);
	m_heap_no = n + PAGE_HEAP_NO_USER_LOW;
	m_rec_no = n;
}

/** Set next page
@param[in]	next_page_no	next page no */
void PageBulk::setNext(ulint	next_page_no)
{
	btr_page_set_next(m_page, m_page_zip, next_page_no, m_mtr);
}

/** Set previous page
@param[in]	prev_page_no	previous page no */
void PageBulk::setPrev(ulint	prev_page_no)
{
	btr_page_set_prev(m_page, m_page_zip, prev_page_no, m_mtr);
}

/** Check if required length is available in the page.
We check fill factor & padding here.
@param[in]	length		required length
@retval true	if space is available
@retval false	if no space is available */
bool PageBulk::spaceAvailable(ulint        rec_size)
{
	ulint	slot_size;
	ulint	total_size;

	slot_size = page_dir_calc_reserved_space(m_rec_no + 1)
		- page_dir_calc_reserved_space(m_rec_no);

	total_size = rec_size + slot_size;

	if (total_size > m_free_space) {
		ut_ad(m_rec_no > 0);
		return false;
	}

	/* Fillfactor & Padding only apply to leaf pages. */
	if (page_is_leaf(m_page)
	    && m_rec_no > 0
	    && ((m_page_zip == NULL && m_free_space - total_size < m_fill_space)
		|| (m_page_zip != NULL && m_free_space - total_size < m_pad_space)
	        )) {
		return(false);
	}

	return(true);
}

/** Check whether the record needs to be stored externally.
@return	true
@return	false */
bool PageBulk::needExt(dtuple_t*	tuple, ulint	rec_size)
{
	return(page_zip_rec_needs_ext(rec_size, m_is_comp,
		dtuple_get_n_fields(tuple), m_block->page.size));
}

/** Store external record
@param[in]	big_rec		external recrod
@param[in]	offsets		record offsets
@return	error code */
dberr_t PageBulk::storeExt(const big_rec_t* big_rec, const ulint* offsets)
{
	return(btr_store_big_rec_extern_fields(
		m_index, m_block, m_cur_rec,
		offsets, big_rec, m_mtr, BTR_STORE_INSERT_BULK));
}

/** Release block by commiting mtr */
void PageBulk::release()
{
	mtr_commit(m_mtr);
}

/** Start mtr and lock block */
void PageBulk::lock()
{
	page_id_t	page_id(dict_index_get_space(m_index), m_page_no);
	page_size_t	page_size(dict_table_page_size(m_index->table));

	mtr_start(m_mtr);
	mtr_x_lock(dict_index_get_lock(m_index), m_mtr);

	if (!m_log) {
		mtr_set_log_mode(m_mtr, MTR_LOG_NO_REDO);
	}

	m_block = btr_block_get(page_id, page_size, RW_X_LATCH, m_index, m_mtr);
	m_page = buf_block_get_frame(m_block);
}

/** Split a page
@param[in]	page_bulk	page to split
@param[in]	next_page_bulk	next page
@return	error code */
dberr_t BtrBulk::pageSplit(PageBulk* page_bulk, PageBulk* next_page_bulk)
{
	dberr_t		err = DB_SUCCESS;
	rec_t*		split_rec;

	ut_ad(page_bulk->getPageZip() != NULL);

	/* 1. Check if we have only one user record on the page. */
	if (page_bulk->getRecNo() <= 1) {
		return DB_TOO_BIG_RECORD;
	}

	/* 2. create a new page. */
	PageBulk new_page_bulk(m_index, m_trx_id, FIL_NULL,
			       page_bulk->getLevel());

	/* 3. copy the upper half to new page. */
	split_rec = page_bulk->getSplitRec();
	new_page_bulk.copyIn(split_rec);
	page_bulk->copyOut(split_rec);

	/* 4. commit the splitted page. */
	err = pageCommit(page_bulk, &new_page_bulk, true);
	if (err != DB_SUCCESS) {
		pageAbort(&new_page_bulk);
		return(err);
	}

	/* 5. commit the new page. */
	err = pageCommit(&new_page_bulk, next_page_bulk, true);
	if (err != DB_SUCCESS) {
		pageAbort(&new_page_bulk);
		return(err);
	}

	return(err);
}

/** Commit(finish) a page
@param[in]	page_bulk	page to commit
@param[in]	next_page_bulk	next page
@param[in]	insert_father	flag whether need to insert node ptr
@return	error code */
dberr_t BtrBulk::pageCommit(PageBulk* page_bulk, PageBulk* next_page_bulk,
			    bool insert_father)
{
	dberr_t		err = DB_SUCCESS;

	page_bulk->finish();

	/* Set page links */
	if (next_page_bulk != NULL) {
		ut_ad(page_bulk->getLevel() == next_page_bulk->getLevel());

		page_bulk->setNext(next_page_bulk->getPageNo());
		next_page_bulk->setPrev(page_bulk->getPageNo());
	}

	/* Compress page if it's a compressed table. */
	if (page_bulk->getPageZip() != NULL && !page_bulk->compress()) {
		return(pageSplit(page_bulk, next_page_bulk));
	}

	/* Insert node pointer to father page. */
	if (insert_father) {
		dtuple_t*	node_ptr;

		node_ptr = page_bulk->getNodePtr();
		err = insert(node_ptr, page_bulk->getLevel() + 1);
		if (err != DB_SUCCESS) {
			return(err);
		}
	}

	/* Commit mtr. */
	page_bulk->commit(true);

	return(err);
}

/** Log free check */
void BtrBulk::logFreeCheck()
{
	ut_ad(m_root_level + 1 == m_page_bulks->size());

	for (ulint level = 0; level <= m_root_level; level++) {
		PageBulk*    page_bulk = m_page_bulks->at(level);

		page_bulk->release();
	}

	log_free_check();

	for (ulint level = 0; level <= m_root_level; level++) {
		PageBulk*    page_bulk = m_page_bulks->at(level);
		page_bulk->lock();
	}
}

/** Insert a tuple to page in a level
@param[in]	tuple	tuple to insert
@param[in]	level	B-tree level
@return error code */
dberr_t	BtrBulk::insert(dtuple_t*	tuple, ulint	level)
{
	PageBulk*	page_bulk;
	ulint		rec_size;
	ulint		n_ext = 0;
	big_rec_t*	big_rec = NULL;
	ulint*		offsets;
	rec_t*		rec;
	bool		is_left_most = false;
	dberr_t		err = DB_SUCCESS;

	if (level + 1 > m_page_bulks->size()) {
		PageBulk* new_page_bulk = new PageBulk(m_index, m_trx_id,
						   FIL_NULL, level);

		m_page_bulks->push_back(new_page_bulk);
		ut_ad(level + 1 == m_page_bulks->size());
		m_root_level = level;

		is_left_most = true;
	}

	ut_ad(m_page_bulks->size() > level);
	page_bulk = m_page_bulks->at(level);

	if (is_left_most && level > 0 && page_bulk->getRecNo() == 0) {
		/* The node pointer must be marked as the predefined minimum record,
		as there is no lower alphabetical limit to records in the leftmost
		node of a level: */
		dtuple_set_info_bits(tuple,
			dtuple_get_info_bits(tuple)
			| REC_INFO_MIN_REC_FLAG);
	}

	/* Calculate the record size when entry is converted to a record */
        rec_size = rec_get_converted_size(m_index, tuple, n_ext);
	if (page_bulk->needExt(tuple, rec_size)) {
		/* The record is so big that we have to store some fields
		externally on separate database pages */
		big_rec = dtuple_convert_big_rec(m_index, tuple, &n_ext);

		if (UNIV_UNLIKELY(big_rec == NULL)) {
			return(DB_TOO_BIG_RECORD);
		}

		rec_size = rec_get_converted_size(m_index, tuple, n_ext);
	}

	if (!page_bulk->spaceAvailable(rec_size)) {
		PageBulk*	sibling_page_bulk;

		/* Create a sibling page_bulk. */
		sibling_page_bulk = new PageBulk(m_index, m_trx_id,
						 FIL_NULL, level);

		/* Commit page bulk. */
		err = pageCommit(page_bulk, sibling_page_bulk, true);
		if (err != DB_SUCCESS) {
			pageAbort(sibling_page_bulk);
			delete sibling_page_bulk;
			return(err);
		}

		/* Set new page bulk to page_bulks. */
		ut_ad(sibling_page_bulk->getLevel() <= m_root_level);
		m_page_bulks->at(level) = sibling_page_bulk;

		delete page_bulk;
		page_bulk = sibling_page_bulk;

		/* Important: log_free_check whether we need a checkpoint. */
		if (page_is_leaf(sibling_page_bulk->getPage())) {
			logFreeCheck();
		}
	}

	/* Convert tuple to rec. */
	offsets = NULL;
        rec = rec_convert_dtuple_to_rec(static_cast<byte*>(mem_heap_alloc(
		page_bulk->m_heap, rec_size)), m_index, tuple, n_ext);
        offsets = rec_get_offsets(rec, m_index, offsets, ULINT_UNDEFINED,
		&(page_bulk->m_heap));

	page_bulk->insert(rec, offsets);

	if (big_rec != NULL) {
		ut_ad(err == DB_SUCCESS);
		ut_ad(dict_index_is_clust(m_index));
		ut_ad(page_bulk->getLevel() == 0);

		err = page_bulk->storeExt(big_rec, offsets);
	}

	return(err);
}

/** Btree bulk load finish
@param[in]	success		whether bulk load is successful */
dberr_t BtrBulk::finish(dberr_t	err)
{
	ulint		last_page_no;

	if (m_page_bulks->size() == 0) {
		return(err);
	}

	ut_ad(m_root_level + 1 == m_page_bulks->size());

	/* Finish all page bulks */
	for (ulint level = 0; level <= m_root_level; level++) {
		PageBulk*	page_bulk = m_page_bulks->at(level);

		last_page_no = page_bulk->getPageNo();

		if (err == DB_SUCCESS) {
			err = pageCommit(page_bulk, NULL,
					 level != m_root_level);
		}

		if (err != DB_SUCCESS) {
			pageAbort(page_bulk);
		}

		delete page_bulk;
	}

	if (err == DB_SUCCESS) {
		rec_t*		first_rec;
		mtr_t		mtr;
		buf_block_t*	last_block;
		page_t*		last_page;
		page_id_t	page_id(dict_index_get_space(m_index),
					last_page_no);
		page_size_t	page_size(dict_table_page_size(m_index->table));
		ulint		root_page_no = dict_index_get_page(m_index);
		PageBulk	root_page_bulk(m_index, m_trx_id, root_page_no,
					       m_root_level);

		mtr_start(&mtr);
		mtr_x_lock(dict_index_get_lock(m_index), &mtr);

		ut_ad(last_page_no != FIL_NULL);
		last_block = btr_block_get(page_id, page_size,
					   RW_X_LATCH, m_index, &mtr);
		last_page = buf_block_get_frame(last_block);
		first_rec = page_rec_get_next(page_get_infimum_rec(last_page));
		ut_ad(page_rec_is_user_rec(first_rec));

		/* Copy last page to root page. */
		root_page_bulk.copyIn(first_rec);

		/* Remove last page. */
		btr_page_free_low(m_index, last_block, m_root_level, &mtr);

		mtr_commit(&mtr);

		err = pageCommit(&root_page_bulk, NULL, false);
		ut_ad(err == DB_SUCCESS);
	}

	return(err);
}
