/*****************************************************************************

Copyright (c) 1994, 2013, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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
@file page/page0page.cc
Index page routines

Created 2/2/1994 Heikki Tuuri
*******************************************************/

#include "page0page.h"
#ifdef UNIV_NONINL
#include "page0page.ic"
#endif

#include "page0cur.h"
#include "page0zip.h"
#include "buf0buf.h"
#include "btr0btr.h"
#ifndef UNIV_HOTBACKUP
# include "srv0srv.h"
# include "lock0lock.h"
# include "fut0lst.h"
# include "btr0sea.h"
#endif /* !UNIV_HOTBACKUP */

/*			THE INDEX PAGE
			==============

The index page consists of a page header which contains the page's
id and other information. On top of it are the index records
in a heap linked into a one way linear list according to alphabetic order.

Just below page end is an array of pointers which we call page directory,
to about every sixth record in the list. The pointers are placed in
the directory in the alphabetical order of the records pointed to,
enabling us to make binary search using the array. Each slot n:o I
in the directory points to a record, where a 4-bit field contains a count
of those records which are in the linear list between pointer I and
the pointer I - 1 in the directory, including the record
pointed to by pointer I and not including the record pointed to by I - 1.
We say that the record pointed to by slot I, or that slot I, owns
these records. The count is always kept in the range 4 to 8, with
the exception that it is 1 for the first slot, and 1--8 for the second slot.

An essentially binary search can be performed in the list of index
records, like we could do if we had pointer to every record in the
page directory. The data structure is, however, more efficient when
we are doing inserts, because most inserts are just pushed on a heap.
Only every 8th insert requires block move in the directory pointer
table, which itself is quite small. A record is deleted from the page
by just taking it off the linear list and updating the number of owned
records-field of the record which owns it, and updating the page directory,
if necessary. A special case is the one when the record owns itself.
Because the overhead of inserts is so small, we may also increase the
page size from the projected default of 8 kB to 64 kB without too
much loss of efficiency in inserts. Bigger page becomes actual
when the disk transfer rate compared to seek and latency time rises.
On the present system, the page size is set so that the page transfer
time (3 ms) is 20 % of the disk random access time (15 ms).

When the page is split, merged, or becomes full but contains deleted
records, we have to reorganize the page.

Assuming a page size of 8 kB, a typical index page of a secondary
index contains 300 index entries, and the size of the page directory
is 50 x 4 bytes = 200 bytes. */

/***************************************************************//**
Looks for the directory slot which owns the given record.
@return	the directory slot number */

ulint
page_dir_find_owner_slot(
/*=====================*/
	const rec_t*	rec)	/*!< in: the physical record */
{
	const page_t*			page;
	register uint16			rec_offs_bytes;
	register const page_dir_slot_t*	slot;
	register const page_dir_slot_t*	first_slot;
	register const rec_t*		r = rec;

	ut_ad(page_rec_check(rec));

	page = page_align(rec);
	first_slot = page_dir_get_nth_slot(page, 0);
	slot = page_dir_get_nth_slot(page, page_dir_get_n_slots(page) - 1);

	if (page_is_comp(page)) {
		while (rec_get_n_owned_new(r) == 0) {
			r = rec_get_next_ptr_const(r, TRUE);
			ut_ad(r >= page + PAGE_NEW_SUPREMUM);
			ut_ad(r < page + (UNIV_PAGE_SIZE - PAGE_DIR));
		}
	} else {
		while (rec_get_n_owned_old(r) == 0) {
			r = rec_get_next_ptr_const(r, FALSE);
			ut_ad(r >= page + PAGE_OLD_SUPREMUM);
			ut_ad(r < page + (UNIV_PAGE_SIZE - PAGE_DIR));
		}
	}

	rec_offs_bytes = mach_encode_2(r - page);

	while (UNIV_LIKELY(*(uint16*) slot != rec_offs_bytes)) {

		if (UNIV_UNLIKELY(slot == first_slot)) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"Probable data corruption on page %lu "
				"Original record on that page;",
				(ulong) page_get_page_no(page));

			if (page_is_comp(page)) {
				fputs("(compact record)", stderr);
			} else {
				rec_print_old(stderr, rec);
			}

			ib_logf(IB_LOG_LEVEL_ERROR,
				"Cannot find the dir slot for this record "
				"on that page;");
			if (page_is_comp(page)) {
				fputs("(compact record)", stderr);
			} else {
				rec_print_old(stderr, page
					      + mach_decode_2(rec_offs_bytes));
			}

			buf_page_print(page, 0, 0);

			ut_error;
		}

		slot += PAGE_DIR_SLOT_SIZE;
	}

	return(((ulint) (first_slot - slot)) / PAGE_DIR_SLOT_SIZE);
}

/**************************************************************//**
Used to check the consistency of a directory slot.
@return	TRUE if succeed */
static
ibool
page_dir_slot_check(
/*================*/
	const page_dir_slot_t*	slot)	/*!< in: slot */
{
	const page_t*	page;
	ulint		n_slots;
	ulint		n_owned;

	ut_a(slot);

	page = page_align(slot);

	n_slots = page_dir_get_n_slots(page);

	ut_a(slot <= page_dir_get_nth_slot(page, 0));
	ut_a(slot >= page_dir_get_nth_slot(page, n_slots - 1));

	ut_a(page_rec_check(page_dir_slot_get_rec(slot)));

	if (page_is_comp(page)) {
		n_owned = rec_get_n_owned_new(page_dir_slot_get_rec(slot));
	} else {
		n_owned = rec_get_n_owned_old(page_dir_slot_get_rec(slot));
	}

	if (slot == page_dir_get_nth_slot(page, 0)) {
		ut_a(n_owned == 1);
	} else if (slot == page_dir_get_nth_slot(page, n_slots - 1)) {
		ut_a(n_owned >= 1);
		ut_a(n_owned <= PAGE_DIR_SLOT_MAX_N_OWNED);
	} else {
		ut_a(n_owned >= PAGE_DIR_SLOT_MIN_N_OWNED);
		ut_a(n_owned <= PAGE_DIR_SLOT_MAX_N_OWNED);
	}

	return(TRUE);
}

/*************************************************************//**
Sets the max trx id field value. */

void
page_set_max_trx_id(
/*================*/
	buf_block_t*	block,	/*!< in/out: page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	trx_id_t	trx_id,	/*!< in: transaction id */
	mtr_t*		mtr)	/*!< in/out: mini-transaction, or NULL */
{
	page_t*		page		= buf_block_get_frame(block);
#ifndef UNIV_HOTBACKUP
	ut_ad(!mtr || mtr_memo_contains(mtr, block, MTR_MEMO_PAGE_X_FIX));
#endif /* !UNIV_HOTBACKUP */

	/* It is not necessary to write this change to the redo log, as
	during a database recovery we assume that the max trx id of every
	page is the maximum trx id assigned before the crash. */

	if (page_zip) {
		mach_write_to_8(page + (PAGE_HEADER + PAGE_MAX_TRX_ID), trx_id);
		page_zip_write_header(page_zip,
				      page + (PAGE_HEADER + PAGE_MAX_TRX_ID),
				      8, mtr);
#ifndef UNIV_HOTBACKUP
	} else if (mtr) {
		mlog_write_ull(page + (PAGE_HEADER + PAGE_MAX_TRX_ID),
			       trx_id, mtr);
#endif /* !UNIV_HOTBACKUP */
	} else {
		mach_write_to_8(page + (PAGE_HEADER + PAGE_MAX_TRX_ID), trx_id);
	}
}

/************************************************************//**
Allocates a block of memory from the heap of an index page.
@return	pointer to start of allocated buffer, or NULL if allocation fails */

byte*
page_mem_alloc_heap(
/*================*/
	page_t*		page,	/*!< in/out: index page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page with enough
				space available for inserting the record,
				or NULL */
	ulint		need,	/*!< in: total number of bytes needed */
	ulint*		heap_no)/*!< out: this contains the heap number
				of the allocated record
				if allocation succeeds */
{
	byte*	block;
	ulint	avl_space;

	ut_ad(page && heap_no);

	avl_space = page_get_max_insert_size(page, 1);

	if (avl_space >= need) {
		block = page_header_get_ptr(page, PAGE_HEAP_TOP);

		page_header_set_ptr(page, page_zip, PAGE_HEAP_TOP,
				    block + need);
		*heap_no = page_dir_get_n_heap(page);

		page_dir_set_n_heap(page, page_zip, 1 + *heap_no);

		return(block);
	}

	return(NULL);
}

#ifndef UNIV_HOTBACKUP
/**********************************************************//**
Writes a log record of page creation. */
UNIV_INLINE
void
page_create_write_log(
/*==================*/
	buf_frame_t*	frame,	/*!< in: a buffer frame where the page is
				created */
	mtr_t*		mtr,	/*!< in: mini-transaction handle */
	ibool		comp)	/*!< in: TRUE=compact page format */
{
	mlog_write_initial_log_record(frame, comp
				      ? MLOG_COMP_PAGE_CREATE
				      : MLOG_PAGE_CREATE, mtr);
}
#else /* !UNIV_HOTBACKUP */
# define page_create_write_log(frame,mtr,comp) ((void) 0)
#endif /* !UNIV_HOTBACKUP */

/***********************************************************//**
Parses a redo log record of creating a page.
@return	end of log record or NULL */

byte*
page_parse_create(
/*==============*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr __attribute__((unused)), /*!< in: buffer end */
	ulint		comp,	/*!< in: nonzero=compact page format */
	buf_block_t*	block,	/*!< in: block or NULL */
	mtr_t*		mtr)	/*!< in: mtr or NULL */
{
	ut_ad(ptr && end_ptr);

	/* The record is empty, except for the record initial part */

	if (block) {
		page_create(block, mtr, comp);
	}

	return(ptr);
}

/** The page infimum and supremum of an empty page in ROW_FORMAT=REDUNDANT */
static const byte infimum_supremum_redundant[] = {
	/* the infimum record */
	0x08/*end offset*/,
	0x01/*n_owned*/,
	0x00, 0x00/*heap_no=0*/,
	0x03/*n_fields=1, 1-byte offsets*/,
	0x00, 0x74/* pointer to supremum */,
	0x69, 0x6e, 0x66, 0x69, 0x6d, 0x75, 0x6d, 0x00/* "infimum\0" */,
	/* the supremum record */
	0x09/*end offset*/,
	0x01/*n_owned*/,
	0x00, 0x08/*heap_no=1*/,
	0x03/*n_fields=1, 1-byte offsets*/,
	0x00, 0x00/* end of record list */,
	0x73, 0x75, 0x70, 0x72, 0x65, 0x6d, 0x75, 0x6d, 0x00/* "supremum\0" */
};

/** The page infimum and supremum of an empty page in ROW_FORMAT=COMPACT */
static const byte infimum_supremum_compact[] = {
	/* the infimum record */
	0x01/*n_owned=1*/,
	0x00, 0x02/* heap_no=0, REC_STATUS_INFIMUM */,
	0x00, 0x0d/* pointer to supremum */,
	0x69, 0x6e, 0x66, 0x69, 0x6d, 0x75, 0x6d, 0x00/* "infimum\0" */,
	/* the supremum record */
	0x01/*n_owned=1*/,
	0x00, 0x0b/* heap_no=1, REC_STATUS_SUPREMUM */,
	0x00, 0x00/* end of record list */,
	0x73, 0x75, 0x70, 0x72, 0x65, 0x6d, 0x75, 0x6d /* "supremum" */
};

/**********************************************************//**
The index page creation function.
@return	pointer to the page */
static
page_t*
page_create_low(
/*============*/
	buf_block_t*	block,		/*!< in: a buffer block where the
					page is created */
	ulint		comp)		/*!< in: nonzero=compact page format */
{
	page_t*		page;

#if PAGE_BTR_IBUF_FREE_LIST + FLST_BASE_NODE_SIZE > PAGE_DATA
# error "PAGE_BTR_IBUF_FREE_LIST + FLST_BASE_NODE_SIZE > PAGE_DATA"
#endif
#if PAGE_BTR_IBUF_FREE_LIST_NODE + FLST_NODE_SIZE > PAGE_DATA
# error "PAGE_BTR_IBUF_FREE_LIST_NODE + FLST_NODE_SIZE > PAGE_DATA"
#endif

	/* 1. INCREMENT MODIFY CLOCK */
	buf_block_modify_clock_inc(block);

	page = buf_block_get_frame(block);

	fil_page_set_type(page, FIL_PAGE_INDEX);

	memset(page + PAGE_HEADER, 0, PAGE_HEADER_PRIV_END);
	page[PAGE_HEADER + PAGE_N_DIR_SLOTS + 1] = 2;

	if (comp) {
		page[PAGE_HEADER + PAGE_N_HEAP] = 0x80;/*page_is_comp()*/
		page[PAGE_HEADER + PAGE_N_HEAP + 1] = PAGE_HEAP_NO_USER_LOW;
		page[PAGE_HEADER + PAGE_HEAP_TOP + 1] = PAGE_NEW_SUPREMUM_END;
		memcpy(page + PAGE_DATA, infimum_supremum_compact,
		       sizeof infimum_supremum_compact);
		memset(page
		       + PAGE_NEW_SUPREMUM_END, 0,
		       UNIV_PAGE_SIZE - PAGE_DIR - PAGE_NEW_SUPREMUM_END);
		page[UNIV_PAGE_SIZE - PAGE_DIR - PAGE_DIR_SLOT_SIZE * 2 + 1]
			= PAGE_NEW_SUPREMUM;
		page[UNIV_PAGE_SIZE - PAGE_DIR - PAGE_DIR_SLOT_SIZE + 1]
			= PAGE_NEW_INFIMUM;
	} else {
		page[PAGE_HEADER + PAGE_N_HEAP + 1] = PAGE_HEAP_NO_USER_LOW;
		page[PAGE_HEADER + PAGE_HEAP_TOP + 1] = PAGE_OLD_SUPREMUM_END;
		memcpy(page + PAGE_DATA, infimum_supremum_redundant,
		       sizeof infimum_supremum_redundant);
		memset(page
		       + PAGE_OLD_SUPREMUM_END, 0,
		       UNIV_PAGE_SIZE - PAGE_DIR - PAGE_OLD_SUPREMUM_END);
		page[UNIV_PAGE_SIZE - PAGE_DIR - PAGE_DIR_SLOT_SIZE * 2 + 1]
			= PAGE_OLD_SUPREMUM;
		page[UNIV_PAGE_SIZE - PAGE_DIR - PAGE_DIR_SLOT_SIZE + 1]
			= PAGE_OLD_INFIMUM;
	}

	return(page);
}

/**********************************************************//**
Create an uncompressed B-tree index page.
@return	pointer to the page */

page_t*
page_create(
/*========*/
	buf_block_t*	block,		/*!< in: a buffer block where the
					page is created */
	mtr_t*		mtr,		/*!< in: mini-transaction handle */
	ulint		comp)		/*!< in: nonzero=compact page format */
{
	page_create_write_log(buf_block_get_frame(block), mtr, comp);
	return(page_create_low(block, comp));
}

/** Create a compressed B-tree index page.
@param[in/out]	block		the block containing the B-tree page frame
@param[in]	index		index B-tree
@param[in]	level		B-tree level of the page (0=leaf)
@param[in]	zip_level	compression level
@param[in]	max_trx_id	PAGE_MAX_TRX_ID value
@param[in/out]	mtr		mini-transaction, NULL=no logging
@return	block->frame */

page_t*
page_create_zip(
	buf_block_t*		block,
	const dict_index_t*	index,
	ulint			level,
	ulint			zip_level,
	trx_id_t		max_trx_id,
	mtr_t*			mtr)
{
	page_t*		page;
	page_zip_des_t*	page_zip	= buf_block_get_page_zip(block);

	ut_ad(block);
	ut_ad(page_zip);
	ut_ad(index);
	ut_ad(dict_table_is_comp(index->table));

	page = page_create_low(block, TRUE);
	mach_write_to_2(PAGE_HEADER + PAGE_LEVEL + page, level);
	mach_write_to_8(PAGE_HEADER + PAGE_MAX_TRX_ID + page, max_trx_id);

	if (!page_zip_compress(page_zip, page, index, zip_level, mtr)) {
		/* The compression of a newly created page
		should always succeed. */
		ut_error;
	}

	return(page);
}

/** Empty a previously created B-tree index page.
@param[in/out]	block	B-tree page
@param[in]	index	index B-tree
@param[in/out]	mtr	mini-transaction, or NULL to suppress redo logging */

void
page_create_empty(
	buf_block_t*		block,
	const dict_index_t*	index,
	mtr_t*			mtr)
{
	trx_id_t	max_trx_id = 0;
	const page_t*	page	= buf_block_get_frame(block);
	page_zip_des_t*	page_zip= buf_block_get_page_zip(block);

	ut_ad(fil_page_get_type(page) == FIL_PAGE_INDEX);

	/* Multiple transactions cannot simultaneously operate on the
	same temp-table in parallel.
	max_trx_id is ignored for temp tables because it not required
	for MVCC. */
	if (dict_index_is_sec_or_ibuf(index)
	    && !dict_table_is_temporary(index->table)
	    && page_is_leaf(page)) {
		max_trx_id = page_get_max_trx_id(page);
		ut_ad(max_trx_id);
	}

	if (page_zip) {
		page_create_zip(block, index,
				page_header_get_field(page, PAGE_LEVEL),
				page_zip_level, max_trx_id, mtr);
	} else {
		page_create(block, mtr, page_is_comp(page));

		if (max_trx_id) {
			page_update_max_trx_id(
				block, page_zip, max_trx_id, mtr);
		}
	}
}

/** Copies records from block to new_block, from rec onwards, including rec.
The infimum and supremum records are not copied. The records are
copied to the start of the record list on new_page.

Differs from page_copy_rec_list_end(), because this function does not
touch the lock table and max trx id on page or compress the page.

@param[in/out]	new_block	index page to copy to
@param[in]	block		index page of rec
@param[in]	rec		first record to copy
@param[in]	index		B-tree index
@param[in/out]	mtr		mini-transaction

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if new_block is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit(). */

void
page_copy_rec_list_end_no_locks(
	buf_block_t*		new_block,
	const buf_block_t*	block,
	const rec_t*		rec,
	const dict_index_t*	index,
	mtr_t*			mtr)
{
	const page_t*		new_page= buf_block_get_frame(new_block);

	btr_assert_not_corrupted(new_block, index);
	ut_a(page_is_comp(new_page) == page_rec_is_comp(rec));
	ut_ad(mach_read_from_2(new_page + UNIV_PAGE_SIZE - 10) == (ulint)
	      (page_is_comp(new_page) ? PAGE_NEW_INFIMUM : PAGE_OLD_INFIMUM));

	PageCur cur1(mtr, index, block, rec);

	if ((cur1.isBeforeFirst() && !cur1.next())
	    || cur1.isAfterLast()) {
		return;
	}

	PageCur cur2(mtr, index, new_block);

	/* Copy records from the original page to the new page */

	do {
		if (!cur2.insertNoZip(cur1) || !cur2.next()) {
			ut_error;
		}
	} while (cur1.next());
}

#ifndef UNIV_HOTBACKUP
/*************************************************************//**
Copies records from page to new_page, from a given record onward,
including that record. Infimum and supremum records are not copied.
The records are copied to the start of the record list on new_page.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if new_block is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@return pointer to the original successor of the infimum record on
new_page, or NULL on zip overflow (new_block will be decompressed) */

const rec_t*
page_copy_rec_list_end(
/*===================*/
	buf_block_t*	new_block,	/*!< in/out: index page to copy to */
	buf_block_t*	block,		/*!< in: index page containing rec */
	rec_t*		rec,		/*!< in: record on page */
	dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*		mtr)		/*!< in: mtr */
{
	PageCur dst(mtr, index, new_block);
	PageCur src(mtr, index, block, rec);

	ut_ad(!page_is_empty(buf_block_get_frame(block)));

	dst.next();

	/* The target page cursor may point to a user record or the
	predefined supremum record (in case new_block is empty). */

	/* Strict page_zip_validate() may fail here.
	Furthermore, btr_compress() may set FIL_PAGE_PREV to
	FIL_NULL on new_page while leaving it intact on
	new_page_zip. So, we cannot validate new_page_zip. */
	page_zip_validate_sloppy_if_zip(
		buf_block_get_page_zip(block),
		buf_block_get_frame(block), index);

	if (src.isBeforeFirst() ? !src.next() : src.isAfterLast()) {
		/* Nothing to copy. But, we have to ensure that the
		PAGE_MAX_TRX_ID is set on secondary index leaf pages. */
		const page_t*	new_page
			= buf_block_get_frame(new_block);

		if (dict_index_is_sec_or_ibuf(index)
		    && page_is_leaf(new_page)
		    && !page_get_max_trx_id(new_page)
		    && !dict_table_is_temporary(index->table)) {
			ut_ad(page_is_empty(new_page));

			page_update_max_trx_id(
				new_block, buf_block_get_page_zip(new_block),
				page_get_max_trx_id(
					buf_block_get_frame(block)),
				mtr);
		}

		return(dst.getRec());
	}

	const ulint	log_mode= dst.isZip()
		? mtr_set_log_mode(mtr, MTR_LOG_NONE)
		: 0;
	const rec_t*	ret	= dst.getRec();
	dst.setBeforeFirst();
	dst.appendNoZip(src);
	dst.setRec(ret);
	ut_ad(!log_mode == !dst.isZip());

	/* Update PAGE_MAX_TRX_ID on the uncompressed page.
	Modifications will be redo logged and copied to the compressed
	page in page_zip_compress() or page_zip_reorganize() below.
	Multiple transactions cannot simultaneously operate on the
	same temp-table in parallel.
	max_trx_id is ignored for temp tables because it not required
	for MVCC. */
	if (dict_index_is_sec_or_ibuf(index)
	    && page_is_leaf(buf_block_get_frame(block))
	    && !dict_table_is_temporary(index->table)) {
		page_update_max_trx_id(
			new_block, NULL,
			page_get_max_trx_id(buf_block_get_frame(block)), mtr);
	}

	if (log_mode) {
		mtr_set_log_mode(mtr, log_mode);

		if (!dst.compress(page_zip_level)) {
			return(NULL);
		}
	}

	/* Update the lock table and possible hash index */

	lock_move_rec_list_end(new_block, block, rec);

	btr_search_move_or_delete_hash_entries(new_block, block, index);

	return(dst.getRec());
}

/*************************************************************//**
Copies records from page to new_page, up to the given record,
NOT including that record. Infimum and supremum records are not copied.
The records are copied to the end of the record list on new_page.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if new_block is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@return pointer to the original predecessor of the supremum record on
new_page, or NULL on zip overflow (new_block will be decompressed) */

rec_t*
page_copy_rec_list_start(
/*=====================*/
	buf_block_t*	new_block,	/*!< in/out: index page to copy to */
	buf_block_t*	block,		/*!< in: index page containing rec */
	rec_t*		rec,		/*!< in: record on page */
	dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*		mtr)		/*!< in: mtr */
{
	page_t*		new_page	= buf_block_get_frame(new_block);
	page_zip_des_t*	new_page_zip	= buf_block_get_page_zip(new_block);
	ulint		log_mode	= 0 /* remove warning */;
	rec_t*		ret		= page_rec_get_prev(
		page_get_supremum_rec(new_page));

	btr_assert_not_corrupted(new_block, index);
	ut_a(page_is_comp(new_page) == page_rec_is_comp(rec));
	ut_ad(mach_read_from_2(new_page + UNIV_PAGE_SIZE - 10) == (ulint)
	      (page_is_comp(new_page) ? PAGE_NEW_INFIMUM : PAGE_OLD_INFIMUM));

	/* Here, "cur2" may be pointing to a user record or the
	predefined infimum record. */

	if (page_rec_is_infimum(rec)) {

		return(ret);
	}

	PageCur	cur1(mtr, index, block);

	if (new_page_zip) {
		log_mode = mtr_set_log_mode(mtr, MTR_LOG_NONE);
	}

	/* Copy records from the original page to the new page */
	for (PageCur cur2(mtr, index, new_block, ret);
	     cur1.next() && cur1.getRec() != rec; ) {
		if (!cur2.insertNoZip(cur1) || !cur2.next()) {
			ut_error;
		}
	}

	ut_ad(!cur1.isUser() == page_rec_is_supremum(rec));

	/* Update PAGE_MAX_TRX_ID on the uncompressed page.
	Modifications will be redo logged and copied to the compressed
	page in page_zip_compress() or page_zip_reorganize() below.
	Multiple transactions cannot simultaneously operate on the
	same temp-table in parallel.
	max_trx_id is ignored for temp tables because it not required
	for MVCC. */
	if (dict_index_is_sec_or_ibuf(index)
	    && page_is_leaf(page_align(rec))
	    && !dict_table_is_temporary(index->table)) {
		page_update_max_trx_id(new_block, NULL,
				       page_get_max_trx_id(page_align(rec)),
				       mtr);
	}

	if (new_page_zip) {
		mtr_set_log_mode(mtr, log_mode);

		DBUG_EXECUTE_IF("page_copy_rec_list_start_compress_fail",
				goto zip_reorganize;);

		if (!page_zip_compress(new_page_zip, new_page, index,
				       page_zip_level, mtr)) {

			ulint	ret_pos;
#ifndef DBUG_OFF
zip_reorganize:
#endif /* DBUG_OFF */
			/* Before trying to reorganize the page,
			store the number of preceding records on the page. */
			ret_pos = page_rec_get_n_recs_before(ret);
			/* Before copying, "ret" was the predecessor
			of the predefined supremum record.  If it was
			the predefined infimum record, then it would
			still be the infimum, and we would have
			ret_pos == 0. */

			if (UNIV_UNLIKELY
			    (!page_zip_reorganize(new_block, index, mtr))) {

				if (UNIV_UNLIKELY
				    (!page_zip_decompress(new_page_zip,
							  new_page, FALSE))) {
					ut_error;
				}
				ut_ad(page_validate(new_block, index));
				return(NULL);
			}

			/* The page was reorganized: Seek to ret_pos. */
			ret = page_rec_get_nth(new_page, ret_pos);
		}
	}

	/* Update the lock table and possible hash index */

	lock_move_rec_list_start(new_block, block, rec, ret);

	btr_search_move_or_delete_hash_entries(new_block, block, index);

	return(ret);
}

/** Write a log record of a record list end or start deletion.

@param[in]	rec	limit record
@param[in]	index	B-tree index
@param[in]	type	redo log entry type
@param[in/out]	mtr	mini-transaction */
UNIV_INLINE
void
page_delete_rec_list_write_log(
	const rec_t*		rec,
	const dict_index_t*	index,
	byte			type,
	mtr_t*			mtr)
{
	byte*	log_ptr;
	ut_ad(type == MLOG_LIST_END_DELETE
	      || type == MLOG_LIST_START_DELETE
	      || type == MLOG_COMP_LIST_END_DELETE
	      || type == MLOG_COMP_LIST_START_DELETE);

	log_ptr = mlog_open_and_write_index(mtr, rec, index, type, 2);
	if (log_ptr) {
		/* Write the parameter as a 2-byte ulint */
		mach_write_to_2(log_ptr, page_offset(rec));
		mlog_close(mtr, log_ptr + 2);
	}
}
#else /* !UNIV_HOTBACKUP */
# define page_delete_rec_list_write_log(rec,index,type,mtr) ((void) 0)
#endif /* !UNIV_HOTBACKUP */

/**********************************************************//**
Parses a log record of a record list end or start deletion.
@return	end of log record or NULL */

byte*
page_parse_delete_rec_list(
/*=======================*/
	byte		type,	/*!< in: MLOG_LIST_END_DELETE,
				MLOG_LIST_START_DELETE,
				MLOG_COMP_LIST_END_DELETE or
				MLOG_COMP_LIST_START_DELETE */
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	buf_block_t*	block,	/*!< in/out: buffer block or NULL */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr)	/*!< in: mtr or NULL */
{
	page_t*	page;
	ulint	offset;

	ut_ad(type == MLOG_LIST_END_DELETE
	      || type == MLOG_LIST_START_DELETE
	      || type == MLOG_COMP_LIST_END_DELETE
	      || type == MLOG_COMP_LIST_START_DELETE);

	/* Read the record offset as a 2-byte ulint */

	if (end_ptr < ptr + 2) {

		return(NULL);
	}

	offset = mach_read_from_2(ptr);
	ptr += 2;

	if (!block) {

		return(ptr);
	}

	page = buf_block_get_frame(block);

	ut_ad(!!page_is_comp(page) == dict_table_is_comp(index->table));

	if (type == MLOG_LIST_END_DELETE
	    || type == MLOG_COMP_LIST_END_DELETE) {
		page_delete_rec_list_end(page + offset, block, index,
					 ULINT_UNDEFINED, ULINT_UNDEFINED,
					 mtr);
	} else {
		page_delete_rec_list_start(page + offset, block, index, mtr);
	}

	return(ptr);
}

/*************************************************************//**
Deletes records from a page from a given record onward, including that record.
The infimum and supremum records are not deleted. */

void
page_delete_rec_list_end(
/*=====================*/
	rec_t*		rec,	/*!< in: pointer to record on page */
	buf_block_t*	block,	/*!< in: buffer block of the page */
	dict_index_t*	index,	/*!< in: record descriptor */
	ulint		n_recs,	/*!< in: number of records to delete,
				or ULINT_UNDEFINED if not known */
	ulint		size,	/*!< in: the sum of the sizes of the
				records in the end of the chain to
				delete, or ULINT_UNDEFINED if not known */
	mtr_t*		mtr)	/*!< in: mtr */
{
	page_dir_slot_t*slot;
	ulint		slot_index;
	rec_t*		last_rec;
	rec_t*		prev_rec;
	ulint		n_owned;
	page_zip_des_t*	page_zip	= buf_block_get_page_zip(block);
	page_t*		page		= page_align(rec);

	ut_ad(size == ULINT_UNDEFINED || size < UNIV_PAGE_SIZE);
	ut_ad(!page_zip || page_rec_is_comp(rec));
	page_zip_validate_if_zip(page_zip, page, index);

	if (page_rec_is_supremum(rec)) {
		ut_ad(n_recs == 0 || n_recs == ULINT_UNDEFINED);
		return;
	}

	if (page_rec_is_infimum(rec)
	    || n_recs == page_get_n_recs(page)) {
delete_all:
		/* We are deleting all records. */
		page_create_empty(block, index, mtr);
		return;
	}

	/* If we are deleting everything from the first user record
	onwards, create an empty page. */

	if (ulint comp = page_is_comp(page)) {
		if (page_rec_get_next_low(page + PAGE_NEW_INFIMUM, comp)
		    == rec) {
			goto delete_all;
		}
	} else {
		if (page_rec_get_next_low(page + PAGE_OLD_INFIMUM, comp)
		    == rec) {
			goto delete_all;
		}
	}

	/* Reset the last insert info in the page header and increment
	the modify clock for the frame */

	page_header_set_ptr(page, page_zip, PAGE_LAST_INSERT, NULL);

	/* The page gets invalid for optimistic searches: increment the
	frame modify clock */

	buf_block_modify_clock_inc(block);

	page_delete_rec_list_write_log(rec, index, page_is_comp(page)
				       ? MLOG_COMP_LIST_END_DELETE
				       : MLOG_LIST_END_DELETE, mtr);

	if (page_zip) {
		ut_a(page_is_comp(page));
		/* Individual deletes are not logged */
		PageCur cur(NULL, index, block, rec);
		while (cur.purge());
		return;
	}

	prev_rec = page_rec_get_prev(rec);

	last_rec = page_rec_get_prev(page_get_supremum_rec(page));

	/* Update the page directory; there is no need to balance the number
	of the records owned by the supremum record, as it is allowed to be
	less than PAGE_DIR_SLOT_MIN_N_OWNED */

	const rec_t*	rec2;
	ulint		count	= 0;

	if (page_is_comp(page)) {
		if ((size == ULINT_UNDEFINED) || (n_recs == ULINT_UNDEFINED)) {
			/* Calculate the total number and size of records. */
			size = 0;
			n_recs = 0;

			for (rec2 = rec; !page_rec_is_supremum(rec2);
			     rec2 = rec_get_next_ptr_const(rec2, TRUE)) {
				ulint	s;
				ulint	extra;
				s = rec_get_size_comp(rec2, index, extra);
				ut_ad(rec2 - page + s < UNIV_PAGE_SIZE);
				ut_ad(size + s + extra < UNIV_PAGE_SIZE);
				size += s + extra;
				n_recs++;
			}
		}

		ut_ad(size < UNIV_PAGE_SIZE);

		for (rec2 = rec; rec_get_n_owned_new(rec2) == 0;
		     rec2 = rec_get_next_ptr_const(rec2, TRUE)) {
			count++;
		}

		ut_ad(rec_get_n_owned_new(rec2) > count);

		n_owned = rec_get_n_owned_new(rec2) - count;
		slot_index = page_dir_find_owner_slot(rec2);
		ut_ad(slot_index > 0);
		slot = page_dir_get_nth_slot(page, slot_index);
	} else {
		if ((size == ULINT_UNDEFINED) || (n_recs == ULINT_UNDEFINED)) {
			/* Calculate the total number and size of records. */
			size = 0;
			n_recs = 0;

			for (rec2 = rec; !page_rec_is_supremum(rec2);
			     rec2 = rec_get_next_ptr_const(rec2, FALSE)) {
				ulint	s;
				ulint	extra;
				s = rec_get_size_old(rec2, extra);
				ut_ad(rec2 - page + s < UNIV_PAGE_SIZE);
				ut_ad(size + s + extra < UNIV_PAGE_SIZE);
				size += s + extra;
				n_recs++;
			}
		}

		ut_ad(size < UNIV_PAGE_SIZE);

		for (rec2 = rec; rec_get_n_owned_old(rec2) == 0;
		     rec2 = rec_get_next_ptr_const(rec2, FALSE)) {
			count++;
		}

		ut_ad(rec_get_n_owned_old(rec2) > count);

		n_owned = rec_get_n_owned_old(rec2) - count;
		slot_index = page_dir_find_owner_slot(rec2);
		ut_ad(slot_index > 0);
		slot = page_dir_get_nth_slot(page, slot_index);
	}

	page_dir_slot_set_rec(slot, page_get_supremum_rec(page));
	page_dir_slot_set_n_owned(slot, NULL, n_owned);

	page_dir_set_n_slots(page, NULL, slot_index + 1);

	/* Remove the record chain segment from the record chain */
	page_rec_set_next(prev_rec, page_get_supremum_rec(page));

	/* Catenate the deleted chain segment to the page free list */

	page_rec_set_next(last_rec, page_header_get_ptr(page, PAGE_FREE));
	page_header_set_ptr(page, NULL, PAGE_FREE, rec);

	page_header_set_field(page, NULL, PAGE_GARBAGE, size
			      + page_header_get_field(page, PAGE_GARBAGE));

	page_header_set_field(page, NULL, PAGE_N_RECS,
			      (ulint)(page_get_n_recs(page) - n_recs));
}

/** Delete all user records up to the given record, excluding that
record. Infimum and supremum records are not deleted.

@param[in]	rec	record up to which to delete
@param[in/out]	block	B-tree page
@param[in]	index	B-tree index
@param[in/out]	mtr	mini-transaction, or NULL to suppress redo logging */

void
page_delete_rec_list_start(
	const rec_t*		rec,
	buf_block_t*		block,
	const dict_index_t*	index,
	mtr_t*			mtr)
{
	ut_ad(page_align(rec) == buf_block_get_frame(block));

	/* page_zip_validate() would detect a min_rec_mark mismatch
	in btr_page_split_and_insert()
	between btr_attach_half_pages() and insert_page = ...
	when btr_page_get_split_rec_to_left() holds
	(direction == FSP_DOWN). */
	page_zip_validate_sloppy_if_zip(buf_block_get_page_zip(block),
					buf_block_get_frame(block), index);

	if (page_rec_is_infimum(rec)) {
		/* Nothing to delete. */
	} else if (page_rec_is_supremum(rec)) {
		/* Delete all records. */
		page_create_empty(block, index, mtr);
	} else {
		/* Individual deletes are not logged */
		PageCur cur(NULL, index, block);

		if (mtr) {
			page_delete_rec_list_write_log(
				rec, index, cur.isComp()
				? MLOG_COMP_LIST_START_DELETE
				: MLOG_LIST_START_DELETE, mtr);
		}

		if (!cur.next()) {
			/* If the page were empty, then rec should
			either be the page infimum or supremum. */
			ut_error;
		}

		while (cur.getRec() != rec) {
			cur.purge();
		}

		ut_ad(cur.getRec() == rec);
	}
}

#ifndef UNIV_HOTBACKUP
/*************************************************************//**
Moves record list end to another page. Moved records include
split_rec.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if new_block is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@return TRUE on success; FALSE on compression failure (new_block will
be decompressed) */

ibool
page_move_rec_list_end(
/*===================*/
	buf_block_t*	new_block,	/*!< in/out: index page where to move */
	buf_block_t*	block,		/*!< in: index page from where to move */
	rec_t*		split_rec,	/*!< in: first record to move */
	dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*		mtr)		/*!< in: mtr */
{
	page_t*		new_page	= buf_block_get_frame(new_block);
	ulint		old_data_size;
	ulint		new_data_size;
	ulint		old_n_recs;
	ulint		new_n_recs;

	old_data_size = page_get_data_size(new_page);
	old_n_recs = page_get_n_recs(new_page);

	ut_ad(!buf_block_get_page_zip(block)
	      == !buf_block_get_page_zip(new_block));
	page_zip_validate_if_zip(buf_block_get_page_zip(block),
				 page_align(split_rec), index);
	page_zip_validate_if_zip(buf_block_get_page_zip(new_block),
				 new_page, index);

	if (UNIV_UNLIKELY(!page_copy_rec_list_end(new_block, block,
						  split_rec, index, mtr))) {
		return(FALSE);
	}

	new_data_size = page_get_data_size(new_page);
	new_n_recs = page_get_n_recs(new_page);

	ut_ad(new_data_size >= old_data_size);

	page_delete_rec_list_end(split_rec, block, index,
				 new_n_recs - old_n_recs,
				 new_data_size - old_data_size, mtr);

	return(TRUE);
}

/*************************************************************//**
Moves record list start to another page. Moved records do not include
split_rec.

IMPORTANT: The caller will have to update IBUF_BITMAP_FREE
if new_block is a compressed leaf page in a secondary index.
This has to be done either within the same mini-transaction,
or by invoking ibuf_reset_free_bits() before mtr_commit().

@return	TRUE on success; FALSE on compression failure */

ibool
page_move_rec_list_start(
/*=====================*/
	buf_block_t*	new_block,	/*!< in/out: index page where to move */
	buf_block_t*	block,		/*!< in/out: page containing split_rec */
	rec_t*		split_rec,	/*!< in: first record not to move */
	dict_index_t*	index,		/*!< in: record descriptor */
	mtr_t*		mtr)		/*!< in: mtr */
{
	if (UNIV_UNLIKELY(!page_copy_rec_list_start(new_block, block,
						    split_rec, index, mtr))) {
		return(FALSE);
	}

	page_delete_rec_list_start(split_rec, block, index, mtr);

	return(TRUE);
}
#endif /* !UNIV_HOTBACKUP */

/**************************************************************//**
Used to delete n slots from the directory. This function updates
also n_owned fields in the records, so that the first slot after
the deleted ones inherits the records of the deleted slots. */
UNIV_INLINE
void
page_dir_delete_slot(
/*=================*/
	page_t*		page,	/*!< in/out: the index page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	ulint		slot_no)/*!< in: slot to be deleted */
{
	page_dir_slot_t*	slot;
	ulint			n_owned;
	ulint			i;
	ulint			n_slots;

	ut_ad(!page_zip || page_is_comp(page));
	ut_ad(slot_no > 0);
	ut_ad(slot_no + 1 < page_dir_get_n_slots(page));

	n_slots = page_dir_get_n_slots(page);

	/* 1. Reset the n_owned fields of the slots to be
	deleted */
	slot = page_dir_get_nth_slot(page, slot_no);
	n_owned = page_dir_slot_get_n_owned(slot);
	page_dir_slot_set_n_owned(slot, page_zip, 0);

	/* 2. Update the n_owned value of the first non-deleted slot */

	slot = page_dir_get_nth_slot(page, slot_no + 1);
	page_dir_slot_set_n_owned(slot, page_zip,
				  n_owned + page_dir_slot_get_n_owned(slot));

	/* 3. Destroy the slot by copying slots */
	for (i = slot_no + 1; i < n_slots; i++) {
		rec_t*	rec = (rec_t*)
			page_dir_slot_get_rec(page_dir_get_nth_slot(page, i));
		page_dir_slot_set_rec(page_dir_get_nth_slot(page, i - 1), rec);
	}

	/* 4. Zero out the last slot, which will be removed */
	mach_write_to_2(page_dir_get_nth_slot(page, n_slots - 1), 0);

	/* 5. Update the page header */
	page_header_set_field(page, page_zip, PAGE_N_DIR_SLOTS, n_slots - 1);
}

/**************************************************************//**
Used to add n slots to the directory. Does not set the record pointers
in the added slots or update n_owned values: this is the responsibility
of the caller. */
UNIV_INLINE
void
page_dir_add_slot(
/*==============*/
	page_t*		page,	/*!< in/out: the index page */
	page_zip_des_t*	page_zip,/*!< in/out: comprssed page, or NULL */
	ulint		start)	/*!< in: the slot above which the new slots
				are added */
{
	page_dir_slot_t*	slot;
	ulint			n_slots;

	n_slots = page_dir_get_n_slots(page);

	ut_ad(start < n_slots - 1);

	/* Update the page header */
	page_dir_set_n_slots(page, page_zip, n_slots + 1);

	/* Move slots up */
	slot = page_dir_get_nth_slot(page, n_slots);
	memmove(slot, slot + PAGE_DIR_SLOT_SIZE,
		(n_slots - 1 - start) * PAGE_DIR_SLOT_SIZE);
}

/****************************************************************//**
Splits a directory slot which owns too many records. */

void
page_dir_split_slot(
/*================*/
	page_t*		page,	/*!< in/out: index page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page whose
				uncompressed part will be written, or NULL */
	ulint		slot_no)/*!< in: the directory slot */
{
	rec_t*			rec;
	page_dir_slot_t*	new_slot;
	page_dir_slot_t*	prev_slot;
	page_dir_slot_t*	slot;
	ulint			i;
	ulint			n_owned;

	ut_ad(page);
	ut_ad(!page_zip || page_is_comp(page));
	ut_ad(slot_no > 0);

	slot = page_dir_get_nth_slot(page, slot_no);

	n_owned = page_dir_slot_get_n_owned(slot);
	ut_ad(n_owned == PAGE_DIR_SLOT_MAX_N_OWNED + 1);

	/* 1. We loop to find a record approximately in the middle of the
	records owned by the slot. */

	prev_slot = page_dir_get_nth_slot(page, slot_no - 1);
	rec = (rec_t*) page_dir_slot_get_rec(prev_slot);

	for (i = 0; i < n_owned / 2; i++) {
		rec = page_rec_get_next(rec);
	}

	ut_ad(n_owned / 2 >= PAGE_DIR_SLOT_MIN_N_OWNED);

	/* 2. We add one directory slot immediately below the slot to be
	split. */

	page_dir_add_slot(page, page_zip, slot_no - 1);

	/* The added slot is now number slot_no, and the old slot is
	now number slot_no + 1 */

	new_slot = page_dir_get_nth_slot(page, slot_no);
	slot = page_dir_get_nth_slot(page, slot_no + 1);

	/* 3. We store the appropriate values to the new slot. */

	page_dir_slot_set_rec(new_slot, rec);
	page_dir_slot_set_n_owned(new_slot, page_zip, n_owned / 2);

	/* 4. Finally, we update the number of records field of the
	original slot */

	page_dir_slot_set_n_owned(slot, page_zip, n_owned - (n_owned / 2));
}

/*************************************************************//**
Tries to balance the given directory slot with too few records with the upper
neighbor, so that there are at least the minimum number of records owned by
the slot; this may result in the merging of two slots. */

void
page_dir_balance_slot(
/*==================*/
	page_t*		page,	/*!< in/out: index page */
	page_zip_des_t*	page_zip,/*!< in/out: compressed page, or NULL */
	ulint		slot_no)/*!< in: the directory slot */
{
	page_dir_slot_t*	slot;
	page_dir_slot_t*	up_slot;
	ulint			n_owned;
	ulint			up_n_owned;
	rec_t*			old_rec;
	rec_t*			new_rec;

	ut_ad(page);
	ut_ad(!page_zip || page_is_comp(page));
	ut_ad(slot_no > 0);

	slot = page_dir_get_nth_slot(page, slot_no);

	/* The last directory slot cannot be balanced with the upper
	neighbor, as there is none. */

	if (UNIV_UNLIKELY(slot_no == page_dir_get_n_slots(page) - 1)) {

		return;
	}

	up_slot = page_dir_get_nth_slot(page, slot_no + 1);

	n_owned = page_dir_slot_get_n_owned(slot);
	up_n_owned = page_dir_slot_get_n_owned(up_slot);

	ut_ad(n_owned == PAGE_DIR_SLOT_MIN_N_OWNED - 1);

	/* If the upper slot has the minimum value of n_owned, we will merge
	the two slots, therefore we assert: */
	ut_ad(2 * PAGE_DIR_SLOT_MIN_N_OWNED - 1 <= PAGE_DIR_SLOT_MAX_N_OWNED);

	if (up_n_owned > PAGE_DIR_SLOT_MIN_N_OWNED) {

		/* In this case we can just transfer one record owned
		by the upper slot to the property of the lower slot */
		old_rec = (rec_t*) page_dir_slot_get_rec(slot);

		if (page_is_comp(page)) {
			new_rec = rec_get_next_ptr(old_rec, TRUE);

			rec_set_n_owned_new(old_rec, page_zip, 0);
			rec_set_n_owned_new(new_rec, page_zip, n_owned + 1);
		} else {
			new_rec = rec_get_next_ptr(old_rec, FALSE);

			rec_set_n_owned_old(old_rec, 0);
			rec_set_n_owned_old(new_rec, n_owned + 1);
		}

		page_dir_slot_set_rec(slot, new_rec);

		page_dir_slot_set_n_owned(up_slot, page_zip, up_n_owned -1);
	} else {
		/* In this case we may merge the two slots */
		page_dir_delete_slot(page, page_zip, slot_no);
	}
}

/************************************************************//**
Returns the nth record of the record list.
This is the inverse function of page_rec_get_n_recs_before().
@return	nth record */

const rec_t*
page_rec_get_nth_const(
/*===================*/
	const page_t*	page,	/*!< in: page */
	ulint		nth)	/*!< in: nth record */
{
	const page_dir_slot_t*	slot;
	ulint			i;
	ulint			n_owned;
	const rec_t*		rec;

	if (nth == 0) {
		return(page_get_infimum_rec(page));
	}

	ut_ad(nth < UNIV_PAGE_SIZE / (REC_N_NEW_EXTRA_BYTES + 1));

	for (i = 0;; i++) {

		slot = page_dir_get_nth_slot(page, i);
		n_owned = page_dir_slot_get_n_owned(slot);

		if (n_owned > nth) {
			break;
		} else {
			nth -= n_owned;
		}
	}

	ut_ad(i > 0);
	slot = page_dir_get_nth_slot(page, i - 1);
	rec = page_dir_slot_get_rec(slot);

	if (page_is_comp(page)) {
		do {
			rec = page_rec_get_next_low(rec, TRUE);
			ut_ad(rec);
		} while (nth--);
	} else {
		do {
			rec = page_rec_get_next_low(rec, FALSE);
			ut_ad(rec);
		} while (nth--);
	}

	return(rec);
}

/***************************************************************//**
Returns the number of records before the given record in chain.
The number includes infimum and supremum records.
@return	number of records */

ulint
page_rec_get_n_recs_before(
/*=======================*/
	const rec_t*	rec)	/*!< in: the physical record */
{
	const page_dir_slot_t*	slot;
	const rec_t*		slot_rec;
	const page_t*		page;
	ulint			i;
	lint			n	= 0;

	ut_ad(page_rec_check(rec));

	page = page_align(rec);
	if (page_is_comp(page)) {
		while (rec_get_n_owned_new(rec) == 0) {

			rec = rec_get_next_ptr_const(rec, TRUE);
			n--;
		}

		for (i = 0; ; i++) {
			slot = page_dir_get_nth_slot(page, i);
			slot_rec = page_dir_slot_get_rec(slot);

			n += rec_get_n_owned_new(slot_rec);

			if (rec == slot_rec) {

				break;
			}
		}
	} else {
		while (rec_get_n_owned_old(rec) == 0) {

			rec = rec_get_next_ptr_const(rec, FALSE);
			n--;
		}

		for (i = 0; ; i++) {
			slot = page_dir_get_nth_slot(page, i);
			slot_rec = page_dir_slot_get_rec(slot);

			n += rec_get_n_owned_old(slot_rec);

			if (rec == slot_rec) {

				break;
			}
		}
	}

	n--;

	ut_ad(n >= 0);
	ut_ad((ulong) n < UNIV_PAGE_SIZE / (REC_N_NEW_EXTRA_BYTES + 1));

	return((ulint) n);
}

#ifndef UNIV_HOTBACKUP
/************************************************************//**
Prints record contents including the data relevant only in
the index page context. */

void
page_rec_print(
/*===========*/
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets)/*!< in: record descriptor */
{
	ut_a(!page_rec_is_comp(rec) == !rec_offs_comp(offsets));
	rec_print_new(stderr, rec, offsets);
	if (page_rec_is_comp(rec)) {
		fprintf(stderr,
			" n_owned: %lu; heap_no: %lu; next rec: %lu\n",
			(ulong) rec_get_n_owned_new(rec),
			(ulong) rec_get_heap_no_new(rec),
			(ulong) rec_get_next_offs(rec, TRUE));
	} else {
		fprintf(stderr,
			" n_owned: %lu; heap_no: %lu; next rec: %lu\n",
			(ulong) rec_get_n_owned_old(rec),
			(ulong) rec_get_heap_no_old(rec),
			(ulong) rec_get_next_offs(rec, FALSE));
	}

	page_rec_check(rec);
	rec_validate(rec, offsets);
}

# ifdef UNIV_BTR_PRINT
/***************************************************************//**
This is used to print the contents of the directory for
debugging purposes. */

void
page_dir_print(
/*===========*/
	page_t*	page,	/*!< in: index page */
	ulint	pr_n)	/*!< in: print n first and n last entries */
{
	ulint			n;
	ulint			i;
	page_dir_slot_t*	slot;

	n = page_dir_get_n_slots(page);

	fprintf(stderr, "--------------------------------\n"
		"PAGE DIRECTORY\n"
		"Page address %p\n"
		"Directory stack top at offs: %lu; number of slots: %lu\n",
		page, (ulong) page_offset(page_dir_get_nth_slot(page, n - 1)),
		(ulong) n);
	for (i = 0; i < n; i++) {
		slot = page_dir_get_nth_slot(page, i);
		if ((i == pr_n) && (i < n - pr_n)) {
			fputs("    ...   \n", stderr);
		}
		if ((i < pr_n) || (i >= n - pr_n)) {
			fprintf(stderr,
				"Contents of slot: %lu: n_owned: %lu,"
				" rec offs: %lu\n",
				(ulong) i,
				(ulong) page_dir_slot_get_n_owned(slot),
				(ulong)
				page_offset(page_dir_slot_get_rec(slot)));
		}
	}
	fprintf(stderr, "Total of %lu records\n"
		"--------------------------------\n",
		(ulong) (PAGE_HEAP_NO_USER_LOW + page_get_n_recs(page)));
}

/***************************************************************//**
This is used to print the contents of the page record list for
debugging purposes. */

void
page_print_list(
/*============*/
	buf_block_t*	block,	/*!< in: index page */
	dict_index_t*	index,	/*!< in: dictionary index of the page */
	ulint		pr_n)	/*!< in: print n first and n last entries */
{
	page_t*		page		= block->frame;
	page_cur_t	cur;
	ulint		count;
	ulint		n_recs;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	rec_offs_init(offsets_);

	ut_a((ibool)!!page_is_comp(page) == dict_table_is_comp(index->table));

	fprintf(stderr,
		"--------------------------------\n"
		"PAGE RECORD LIST\n"
		"Page address %p\n", page);

	n_recs = page_get_n_recs(page);

	page_cur_set_before_first(block, &cur);
	count = 0;
	for (;;) {
		offsets = rec_get_offsets(cur.rec, index, offsets,
					  ULINT_UNDEFINED, &heap);
		page_rec_print(cur.rec, offsets);

		if (count == pr_n) {
			break;
		}
		if (page_cur_is_after_last(&cur)) {
			break;
		}
		page_cur_move_to_next(&cur);
		count++;
	}

	if (n_recs > 2 * pr_n) {
		fputs(" ... \n", stderr);
	}

	while (!page_cur_is_after_last(&cur)) {
		page_cur_move_to_next(&cur);

		if (count + pr_n >= n_recs) {
			offsets = rec_get_offsets(cur.rec, index, offsets,
						  ULINT_UNDEFINED, &heap);
			page_rec_print(cur.rec, offsets);
		}
		count++;
	}

	fprintf(stderr,
		"Total of %lu records \n"
		"--------------------------------\n",
		(ulong) (count + 1));

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/***************************************************************//**
Prints the info in a page header. */

void
page_header_print(
/*==============*/
	const page_t*	page)
{
	fprintf(stderr,
		"--------------------------------\n"
		"PAGE HEADER INFO\n"
		"Page address %p, n records %lu (%s)\n"
		"n dir slots %lu, heap top %lu\n"
		"Page n heap %lu, free %lu, garbage %lu\n"
		"Page last insert %lu, direction %lu, n direction %lu\n",
		page, (ulong) page_header_get_field(page, PAGE_N_RECS),
		page_is_comp(page) ? "compact format" : "original format",
		(ulong) page_header_get_field(page, PAGE_N_DIR_SLOTS),
		(ulong) page_header_get_field(page, PAGE_HEAP_TOP),
		(ulong) page_dir_get_n_heap(page),
		(ulong) page_header_get_field(page, PAGE_FREE),
		(ulong) page_header_get_field(page, PAGE_GARBAGE),
		(ulong) page_header_get_field(page, PAGE_LAST_INSERT),
		(ulong) page_header_get_field(page, PAGE_DIRECTION),
		(ulong) page_header_get_field(page, PAGE_N_DIRECTION));
}

/***************************************************************//**
This is used to print the contents of the page for
debugging purposes. */

void
page_print(
/*=======*/
	buf_block_t*	block,	/*!< in: index page */
	dict_index_t*	index,	/*!< in: dictionary index of the page */
	ulint		dn,	/*!< in: print dn first and last entries
				in directory */
	ulint		rn)	/*!< in: print rn first and last records
				in directory */
{
	page_t*	page = block->frame;

	page_header_print(page);
	page_dir_print(page, dn);
	page_print_list(block, index, rn);
}
# endif /* UNIV_BTR_PRINT */
#endif /* !UNIV_HOTBACKUP */

/***************************************************************//**
The following is used to validate a record on a page. This function
differs from rec_validate as it can also check the n_owned field and
the heap_no field.
@return	TRUE if ok */

ibool
page_rec_validate(
/*==============*/
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets)/*!< in: array returned by rec_get_offsets() */
{
	ulint		n_owned;
	ulint		heap_no;
	const page_t*	page;

	page = page_align(rec);
	ut_a(!page_is_comp(page) == !rec_offs_comp(offsets));

	page_rec_check(rec);
	rec_validate(rec, offsets);

	if (page_rec_is_comp(rec)) {
		n_owned = rec_get_n_owned_new(rec);
		heap_no = rec_get_heap_no_new(rec);
	} else {
		n_owned = rec_get_n_owned_old(rec);
		heap_no = rec_get_heap_no_old(rec);
	}

	if (UNIV_UNLIKELY(!(n_owned <= PAGE_DIR_SLOT_MAX_N_OWNED))) {
		fprintf(stderr,
			"InnoDB: Dir slot of rec %lu, n owned too big %lu\n",
			(ulong) page_offset(rec), (ulong) n_owned);
		return(FALSE);
	}

	if (UNIV_UNLIKELY(!(heap_no < page_dir_get_n_heap(page)))) {
		fprintf(stderr,
			"InnoDB: Heap no of rec %lu too big %lu %lu\n",
			(ulong) page_offset(rec), (ulong) heap_no,
			(ulong) page_dir_get_n_heap(page));
		return(FALSE);
	}

	return(TRUE);
}

#ifndef UNIV_HOTBACKUP
/***************************************************************//**
Checks that the first directory slot points to the infimum record and
the last to the supremum. This function is intended to track if the
bug fixed in 4.0.14 has caused corruption to users' databases. */

void
page_check_dir(
/*===========*/
	const page_t*	page)	/*!< in: index page */
{
	ulint	n_slots;
	ulint	infimum_offs;
	ulint	supremum_offs;

	n_slots = page_dir_get_n_slots(page);
	infimum_offs = mach_read_from_2(page_dir_get_nth_slot(page, 0));
	supremum_offs = mach_read_from_2(page_dir_get_nth_slot(page,
							       n_slots - 1));

	if (UNIV_UNLIKELY(!page_rec_is_infimum_low(infimum_offs))) {

		fprintf(stderr,
			"InnoDB: Page directory corruption:"
			" infimum not pointed to\n");
		buf_page_print(page, 0, 0);
	}

	if (UNIV_UNLIKELY(!page_rec_is_supremum_low(supremum_offs))) {

		fprintf(stderr,
			"InnoDB: Page directory corruption:"
			" supremum not pointed to\n");
		buf_page_print(page, 0, 0);
	}
}
#endif /* !UNIV_HOTBACKUP */

/***************************************************************//**
This function checks the consistency of an index page when we do not
know the index. This is also resilient so that this should never crash
even if the page is total garbage.
@return	TRUE if ok */

ibool
page_simple_validate_old(
/*=====================*/
	const page_t*	page)	/*!< in: index page in ROW_FORMAT=REDUNDANT */
{
	const page_dir_slot_t*	slot;
	ulint			slot_no;
	ulint			n_slots;
	const rec_t*		rec;
	const byte*		rec_heap_top;
	ulint			count;
	ulint			own_count;
	ibool			ret	= FALSE;

	ut_a(!page_is_comp(page));

	/* Check first that the record heap and the directory do not
	overlap. */

	n_slots = page_dir_get_n_slots(page);

	if (UNIV_UNLIKELY(n_slots > UNIV_PAGE_SIZE / 4)) {
		fprintf(stderr,
			"InnoDB: Nonsensical number %lu of page dir slots\n",
			(ulong) n_slots);

		goto func_exit;
	}

	rec_heap_top = page_header_get_ptr(page, PAGE_HEAP_TOP);

	if (UNIV_UNLIKELY(rec_heap_top
			  > page_dir_get_nth_slot(page, n_slots - 1))) {

		fprintf(stderr,
			"InnoDB: Record heap and dir overlap on a page,"
			" heap top %lu, dir %lu\n",
			(ulong) page_header_get_field(page, PAGE_HEAP_TOP),
			(ulong)
			page_offset(page_dir_get_nth_slot(page, n_slots - 1)));

		goto func_exit;
	}

	/* Validate the record list in a loop checking also that it is
	consistent with the page record directory. */

	count = 0;
	own_count = 1;
	slot_no = 0;
	slot = page_dir_get_nth_slot(page, slot_no);

	rec = page_get_infimum_rec(page);

	for (;;) {
		if (UNIV_UNLIKELY(rec > rec_heap_top)) {
			fprintf(stderr,
				"InnoDB: Record %lu is above"
				" rec heap top %lu\n",
				(ulong)(rec - page),
				(ulong)(rec_heap_top - page));

			goto func_exit;
		}

		if (UNIV_UNLIKELY(rec_get_n_owned_old(rec))) {
			/* This is a record pointed to by a dir slot */
			if (UNIV_UNLIKELY(rec_get_n_owned_old(rec)
					  != own_count)) {

				fprintf(stderr,
					"InnoDB: Wrong owned count %lu, %lu,"
					" rec %lu\n",
					(ulong) rec_get_n_owned_old(rec),
					(ulong) own_count,
					(ulong)(rec - page));

				goto func_exit;
			}

			if (UNIV_UNLIKELY
			    (page_dir_slot_get_rec(slot) != rec)) {
				fprintf(stderr,
					"InnoDB: Dir slot does not point"
					" to right rec %lu\n",
					(ulong)(rec - page));

				goto func_exit;
			}

			own_count = 0;

			if (!page_rec_is_supremum(rec)) {
				slot_no++;
				slot = page_dir_get_nth_slot(page, slot_no);
			}
		}

		if (page_rec_is_supremum(rec)) {

			break;
		}

		if (UNIV_UNLIKELY
		    (rec_get_next_offs(rec, FALSE) < FIL_PAGE_DATA
		     || rec_get_next_offs(rec, FALSE) >= UNIV_PAGE_SIZE)) {
			fprintf(stderr,
				"InnoDB: Next record offset"
				" nonsensical %lu for rec %lu\n",
				(ulong) rec_get_next_offs(rec, FALSE),
				(ulong) (rec - page));

			goto func_exit;
		}

		count++;

		if (UNIV_UNLIKELY(count > UNIV_PAGE_SIZE)) {
			fprintf(stderr,
				"InnoDB: Page record list appears"
				" to be circular %lu\n",
				(ulong) count);
			goto func_exit;
		}

		rec = page_rec_get_next_const(rec);
		own_count++;
	}

	if (UNIV_UNLIKELY(rec_get_n_owned_old(rec) == 0)) {
		fprintf(stderr, "InnoDB: n owned is zero in a supremum rec\n");

		goto func_exit;
	}

	if (UNIV_UNLIKELY(slot_no != n_slots - 1)) {
		fprintf(stderr, "InnoDB: n slots wrong %lu, %lu\n",
			(ulong) slot_no, (ulong) (n_slots - 1));
		goto func_exit;
	}

	if (UNIV_UNLIKELY(page_header_get_field(page, PAGE_N_RECS)
			  + PAGE_HEAP_NO_USER_LOW
			  != count + 1)) {
		fprintf(stderr, "InnoDB: n recs wrong %lu %lu\n",
			(ulong) page_header_get_field(page, PAGE_N_RECS)
			+ PAGE_HEAP_NO_USER_LOW,
			(ulong) (count + 1));

		goto func_exit;
	}

	/* Check then the free list */
	rec = page_header_get_ptr(page, PAGE_FREE);

	while (rec != NULL) {
		if (UNIV_UNLIKELY(rec < page + FIL_PAGE_DATA
				  || rec >= page + UNIV_PAGE_SIZE)) {
			fprintf(stderr,
				"InnoDB: Free list record has"
				" a nonsensical offset %lu\n",
				(ulong) (rec - page));

			goto func_exit;
		}

		if (UNIV_UNLIKELY(rec > rec_heap_top)) {
			fprintf(stderr,
				"InnoDB: Free list record %lu"
				" is above rec heap top %lu\n",
				(ulong) (rec - page),
				(ulong) (rec_heap_top - page));

			goto func_exit;
		}

		count++;

		if (UNIV_UNLIKELY(count > UNIV_PAGE_SIZE)) {
			fprintf(stderr,
				"InnoDB: Page free list appears"
				" to be circular %lu\n",
				(ulong) count);
			goto func_exit;
		}

		rec = page_rec_get_next_const(rec);
	}

	if (UNIV_UNLIKELY(page_dir_get_n_heap(page) != count + 1)) {

		fprintf(stderr, "InnoDB: N heap is wrong %lu, %lu\n",
			(ulong) page_dir_get_n_heap(page),
			(ulong) (count + 1));

		goto func_exit;
	}

	ret = TRUE;

func_exit:
	return(ret);
}

/***************************************************************//**
This function checks the consistency of an index page when we do not
know the index. This is also resilient so that this should never crash
even if the page is total garbage.
@return	TRUE if ok */

ibool
page_simple_validate_new(
/*=====================*/
	const page_t*	page)	/*!< in: index page in ROW_FORMAT!=REDUNDANT */
{
	const page_dir_slot_t*	slot;
	ulint			slot_no;
	ulint			n_slots;
	const rec_t*		rec;
	const byte*		rec_heap_top;
	ulint			count;
	ulint			own_count;
	ibool			ret	= FALSE;

	ut_a(page_is_comp(page));

	/* Check first that the record heap and the directory do not
	overlap. */

	n_slots = page_dir_get_n_slots(page);

	if (UNIV_UNLIKELY(n_slots > UNIV_PAGE_SIZE / 4)) {
		fprintf(stderr,
			"InnoDB: Nonsensical number %lu"
			" of page dir slots\n", (ulong) n_slots);

		goto func_exit;
	}

	rec_heap_top = page_header_get_ptr(page, PAGE_HEAP_TOP);

	if (UNIV_UNLIKELY(rec_heap_top
			  > page_dir_get_nth_slot(page, n_slots - 1))) {

		fprintf(stderr,
			"InnoDB: Record heap and dir overlap on a page,"
			" heap top %lu, dir %lu\n",
			(ulong) page_header_get_field(page, PAGE_HEAP_TOP),
			(ulong)
			page_offset(page_dir_get_nth_slot(page, n_slots - 1)));

		goto func_exit;
	}

	/* Validate the record list in a loop checking also that it is
	consistent with the page record directory. */

	count = 0;
	own_count = 1;
	slot_no = 0;
	slot = page_dir_get_nth_slot(page, slot_no);

	rec = page_get_infimum_rec(page);

	for (;;) {
		if (UNIV_UNLIKELY(rec > rec_heap_top)) {
			fprintf(stderr,
				"InnoDB: Record %lu is above rec"
				" heap top %lu\n",
				(ulong) page_offset(rec),
				(ulong) page_offset(rec_heap_top));

			goto func_exit;
		}

		if (UNIV_UNLIKELY(rec_get_n_owned_new(rec))) {
			/* This is a record pointed to by a dir slot */
			if (UNIV_UNLIKELY(rec_get_n_owned_new(rec)
					  != own_count)) {

				fprintf(stderr,
					"InnoDB: Wrong owned count %lu, %lu,"
					" rec %lu\n",
					(ulong) rec_get_n_owned_new(rec),
					(ulong) own_count,
					(ulong) page_offset(rec));

				goto func_exit;
			}

			if (UNIV_UNLIKELY
			    (page_dir_slot_get_rec(slot) != rec)) {
				fprintf(stderr,
					"InnoDB: Dir slot does not point"
					" to right rec %lu\n",
					(ulong) page_offset(rec));

				goto func_exit;
			}

			own_count = 0;

			if (!page_rec_is_supremum(rec)) {
				slot_no++;
				slot = page_dir_get_nth_slot(page, slot_no);
			}
		}

		if (page_rec_is_supremum(rec)) {

			break;
		}

		if (UNIV_UNLIKELY
		    (rec_get_next_offs(rec, TRUE) < FIL_PAGE_DATA
		     || rec_get_next_offs(rec, TRUE) >= UNIV_PAGE_SIZE)) {
			fprintf(stderr,
				"InnoDB: Next record offset nonsensical %lu"
				" for rec %lu\n",
				(ulong) rec_get_next_offs(rec, TRUE),
				(ulong) page_offset(rec));

			goto func_exit;
		}

		count++;

		if (UNIV_UNLIKELY(count > UNIV_PAGE_SIZE)) {
			fprintf(stderr,
				"InnoDB: Page record list appears"
				" to be circular %lu\n",
				(ulong) count);
			goto func_exit;
		}

		rec = page_rec_get_next_const(rec);
		own_count++;
	}

	if (UNIV_UNLIKELY(rec_get_n_owned_new(rec) == 0)) {
		fprintf(stderr, "InnoDB: n owned is zero"
			" in a supremum rec\n");

		goto func_exit;
	}

	if (UNIV_UNLIKELY(slot_no != n_slots - 1)) {
		fprintf(stderr, "InnoDB: n slots wrong %lu, %lu\n",
			(ulong) slot_no, (ulong) (n_slots - 1));
		goto func_exit;
	}

	if (UNIV_UNLIKELY(page_header_get_field(page, PAGE_N_RECS)
			  + PAGE_HEAP_NO_USER_LOW
			  != count + 1)) {
		fprintf(stderr, "InnoDB: n recs wrong %lu %lu\n",
			(ulong) page_header_get_field(page, PAGE_N_RECS)
			+ PAGE_HEAP_NO_USER_LOW,
			(ulong) (count + 1));

		goto func_exit;
	}

	/* Check then the free list */
	rec = page_header_get_ptr(page, PAGE_FREE);

	while (rec != NULL) {
		if (UNIV_UNLIKELY(rec < page + FIL_PAGE_DATA
				  || rec >= page + UNIV_PAGE_SIZE)) {
			fprintf(stderr,
				"InnoDB: Free list record has"
				" a nonsensical offset %lu\n",
				(ulong) page_offset(rec));

			goto func_exit;
		}

		if (UNIV_UNLIKELY(rec > rec_heap_top)) {
			fprintf(stderr,
				"InnoDB: Free list record %lu"
				" is above rec heap top %lu\n",
				(ulong) page_offset(rec),
				(ulong) page_offset(rec_heap_top));

			goto func_exit;
		}

		count++;

		if (UNIV_UNLIKELY(count > UNIV_PAGE_SIZE)) {
			fprintf(stderr,
				"InnoDB: Page free list appears"
				" to be circular %lu\n",
				(ulong) count);
			goto func_exit;
		}

		rec = page_rec_get_next_const(rec);
	}

	if (UNIV_UNLIKELY(page_dir_get_n_heap(page) != count + 1)) {

		fprintf(stderr, "InnoDB: N heap is wrong %lu, %lu\n",
			(ulong) page_dir_get_n_heap(page),
			(ulong) (count + 1));

		goto func_exit;
	}

	ret = TRUE;

func_exit:
	return(ret);
}

/** Check the consistency of an index B-tree page.
@param[in]	block	buffer block containing B-tree page
@param[in]	index	B-tree index
@return	true if ok */

bool
page_validate(
	const buf_block_t*	block,
	const dict_index_t*	index)
{
	const page_dir_slot_t*	slot;
	const page_t*		page		= buf_block_get_frame(block);
	mem_heap_t*		heap;
	byte*			buf;
	ulint			count;
	ulint			own_count;
	ulint			slot_no;
	ulint			data_size;
	const rec_t*		rec;
	const rec_t*		old_rec		= NULL;
	ulint			n_slots;
	bool			ret		= false;
	ulint*			offsets		= NULL;
	ulint*			old_offsets	= NULL;

	ut_ad(buf_page_in_file(&block->page));

	if (UNIV_UNLIKELY((ibool) !!page_is_comp(page)
			  != dict_table_is_comp(index->table))) {
		fputs("InnoDB: 'compact format' flag mismatch\n", stderr);
		goto func_exit2;
	}
	if (page_is_comp(page)) {
		if (UNIV_UNLIKELY(!page_simple_validate_new(page))) {
			goto func_exit2;
		}
	} else {
		if (UNIV_UNLIKELY(!page_simple_validate_old(page))) {
			goto func_exit2;
		}
	}

	/* Multiple transactions cannot simultaneously operate on the
	same temp-table in parallel.
	max_trx_id is ignored for temp tables because it not required
	for MVCC. */
	if (dict_index_is_sec_or_ibuf(index)
	    && !dict_table_is_temporary(index->table)
	    && page_is_leaf(page)
	    && !page_is_empty(page)) {
		trx_id_t	max_trx_id	= page_get_max_trx_id(page);
		trx_id_t	sys_max_trx_id	= trx_sys_get_max_trx_id();

		if (max_trx_id == 0 || max_trx_id > sys_max_trx_id) {
			ib_logf(IB_LOG_LEVEL_ERROR,
				"PAGE_MAX_TRX_ID out of bounds: "
				TRX_ID_FMT ", " TRX_ID_FMT,
				max_trx_id, sys_max_trx_id);
			goto func_exit2;
		}
	}

	heap = mem_heap_create(UNIV_PAGE_SIZE + 200);

	/* The following buffer is used to check that the
	records in the page record heap do not overlap */

	buf = static_cast<byte*>(mem_heap_zalloc(heap, UNIV_PAGE_SIZE));

	/* Check first that the record heap and the directory do not
	overlap. */

	n_slots = page_dir_get_n_slots(page);

	if (UNIV_UNLIKELY(!(page_header_get_ptr(page, PAGE_HEAP_TOP)
			    <= page_dir_get_nth_slot(page, n_slots - 1)))) {

		fprintf(stderr,
			"InnoDB: Record heap and dir overlap"
			" on page %u:%u index " IB_ID_FMT " (%s), %p, %p\n",
			block->page.space, block->page.offset,
			index->id, index->name,
			page_header_get_ptr(page, PAGE_HEAP_TOP),
			page_dir_get_nth_slot(page, n_slots - 1));

		goto func_exit;
	}

	/* Validate the record list in a loop checking also that
	it is consistent with the directory. */
	count = 0;
	data_size = 0;
	own_count = 1;
	slot_no = 0;
	slot = page_dir_get_nth_slot(page, slot_no);

	rec = page_get_infimum_rec(page);

	for (;;) {
		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);

		if (page_is_comp(page) && page_rec_is_user_rec(rec)
		    && UNIV_UNLIKELY(rec_get_node_ptr_flag(rec)
				     == page_is_leaf(page))) {
			fputs("InnoDB: node_ptr flag mismatch\n", stderr);
			goto func_exit;
		}

		if (UNIV_UNLIKELY(!page_rec_validate(rec, offsets))) {
			goto func_exit;
		}

#ifndef UNIV_HOTBACKUP
		/* Check that the records are in the ascending order */
		if (UNIV_LIKELY(count >= PAGE_HEAP_NO_USER_LOW)
		    && !page_rec_is_supremum(rec)) {
			if (UNIV_UNLIKELY
			    (0 >= cmp_rec_rec(rec, old_rec,
					      offsets, old_offsets, index))) {
				fprintf(stderr,
					"InnoDB: Records in wrong order"
					" on page %u:%u index " IB_ID_FMT
					" (%s)\n",
					block->page.space, block->page.offset,
					index->id, index->name);
				fputs("\nInnoDB: previous record ", stderr);
				rec_print_new(stderr, old_rec, old_offsets);
				fputs("\nInnoDB: record ", stderr);
				rec_print_new(stderr, rec, offsets);
				putc('\n', stderr);

				goto func_exit;
			}
		}
#endif /* !UNIV_HOTBACKUP */

		if (page_rec_is_user_rec(rec)) {

			data_size += rec_offs_size(offsets);
		}

		ulint	offs	= page_offset(rec_get_start(rec, offsets));
		ulint	i	= rec_offs_size(offsets);
		if (UNIV_UNLIKELY(offs + i >= UNIV_PAGE_SIZE)) {
			fputs("InnoDB: record offset out of bounds\n", stderr);
			goto func_exit;
		}

		for (byte* b = buf + offs; i--; ) {
			if (UNIV_UNLIKELY(*b)) {
				/* No other record may overlap this */

				fputs("InnoDB: Record overlaps another\n",
				      stderr);
				goto func_exit;
			}

			*b++ = 1;
		}

		ulint	rec_own_count	= page_is_comp(page)
			? rec_get_n_owned_new(rec)
			: rec_get_n_owned_old(rec);

		if (UNIV_UNLIKELY(rec_own_count)) {
			/* This is a record pointed to by a dir slot */
			if (UNIV_UNLIKELY(rec_own_count != own_count)) {
				fprintf(stderr,
					"InnoDB: Wrong owned count %lu, %lu\n",
					(ulong) rec_own_count,
					(ulong) own_count);
				goto func_exit;
			}

			if (page_dir_slot_get_rec(slot) != rec) {
				fputs("InnoDB: Dir slot does not"
				      " point to right rec\n",
				      stderr);
				goto func_exit;
			}

			page_dir_slot_check(slot);

			if (page_rec_is_supremum(rec)) {
				break;
			}

			own_count = 0;
			slot = page_dir_get_nth_slot(page, ++slot_no);
		} else if (page_rec_is_supremum(rec)) {
			break;
		}

		count++;
		own_count++;
		old_rec = rec;
		rec = page_rec_get_next_const(rec);

		/* set old_offsets to offsets; recycle offsets */
		ulint* offsets2 = old_offsets;
		old_offsets = offsets;
		offsets = offsets2;
	}

	if (page_is_comp(page)) {
		if (UNIV_UNLIKELY(rec_get_n_owned_new(rec) == 0)) {

			goto n_owned_zero;
		}
	} else if (UNIV_UNLIKELY(rec_get_n_owned_old(rec) == 0)) {
n_owned_zero:
		fputs("InnoDB: n owned is zero\n", stderr);
		goto func_exit;
	}

	if (UNIV_UNLIKELY(slot_no != n_slots - 1)) {
		fprintf(stderr, "InnoDB: n slots wrong %lu %lu\n",
			(ulong) slot_no, (ulong) (n_slots - 1));
		goto func_exit;
	}

	if (UNIV_UNLIKELY(page_header_get_field(page, PAGE_N_RECS)
			  + PAGE_HEAP_NO_USER_LOW
			  != count + 1)) {
		fprintf(stderr, "InnoDB: n recs wrong %lu %lu\n",
			(ulong) page_header_get_field(page, PAGE_N_RECS)
			+ PAGE_HEAP_NO_USER_LOW,
			(ulong) (count + 1));
		goto func_exit;
	}

	if (UNIV_UNLIKELY(data_size != page_get_data_size(page))) {
		fprintf(stderr,
			"InnoDB: Summed data size %lu, returned by func %lu\n",
			(ulong) data_size, (ulong) page_get_data_size(page));
		goto func_exit;
	}

	/* Check then the free list */
	rec = page_header_get_ptr(page, PAGE_FREE);

	while (rec != NULL) {
		offsets = rec_get_offsets(rec, index, offsets,
					  ULINT_UNDEFINED, &heap);
		if (UNIV_UNLIKELY(!page_rec_validate(rec, offsets))) {

			goto func_exit;
		}

		count++;
		ulint	offs	= page_offset(rec_get_start(rec, offsets));
		ulint	i	= rec_offs_size(offsets);
		if (UNIV_UNLIKELY(offs + i >= UNIV_PAGE_SIZE)) {
			fputs("InnoDB: record offset out of bounds\n", stderr);
			goto func_exit;
		}

		for (byte* b = buf + offs; i--; ) {
			if (UNIV_UNLIKELY(*b)) {
				fputs("InnoDB: Record overlaps another"
				      " in free list\n", stderr);
				goto func_exit;
			}

			*b++ = 1;
		}

		rec = page_rec_get_next_const(rec);
	}

	if (UNIV_UNLIKELY(page_dir_get_n_heap(page) != count + 1)) {
		fprintf(stderr, "InnoDB: N heap is wrong %lu %lu\n",
			(ulong) page_dir_get_n_heap(page),
			(ulong) count + 1);
		goto func_exit;
	}

	ret = true;

func_exit:
	mem_heap_free(heap);

	if (UNIV_UNLIKELY(!ret)) {
func_exit2:
		fprintf(stderr,
			"InnoDB: Apparent corruption"
			" in page %u:%u index " IB_ID_FMT " (%s)\n",
			block->page.space, block->page.offset,
			index->id, index->name);
		buf_page_print(page, 0, 0);
	}

	return(ret);
}

#ifndef UNIV_HOTBACKUP
/***************************************************************//**
Looks in the page record list for a record with the given heap number.
@return	record, NULL if not found */

const rec_t*
page_find_rec_with_heap_no(
/*=======================*/
	const page_t*	page,	/*!< in: index page */
	ulint		heap_no)/*!< in: heap number */
{
	const rec_t*	rec;

	if (page_is_comp(page)) {
		rec = page + PAGE_NEW_INFIMUM;

		for(;;) {
			ulint	rec_heap_no = rec_get_heap_no_new(rec);

			if (rec_heap_no == heap_no) {

				return(rec);
			} else if (rec_heap_no == PAGE_HEAP_NO_SUPREMUM) {

				return(NULL);
			}

			rec = page + rec_get_next_offs(rec, TRUE);
		}
	} else {
		rec = page + PAGE_OLD_INFIMUM;

		for (;;) {
			ulint	rec_heap_no = rec_get_heap_no_old(rec);

			if (rec_heap_no == heap_no) {

				return(rec);
			} else if (rec_heap_no == PAGE_HEAP_NO_SUPREMUM) {

				return(NULL);
			}

			rec = page + rec_get_next_offs(rec, FALSE);
		}
	}
}
#endif /* !UNIV_HOTBACKUP */

/*******************************************************//**
Removes the record from a leaf page. This function does not log
any changes. It is used by the IMPORT tablespace functions.
The cursor is moved to the next record after the deleted one.
@return	true if success, i.e., the page did not become too empty */

bool
page_delete_rec(
/*============*/
	const dict_index_t*	index,	/*!< in: The index that the record
					belongs to */
	page_cur_t*		pcur,	/*!< in/out: page cursor on record
					to delete */
	page_zip_des_t*		page_zip,/*!< in: compressed page descriptor */
	const ulint*		offsets)/*!< in: offsets for record */
{
	bool		no_compress_needed;
	buf_block_t*	block = pcur->block;
	page_t*		page = buf_block_get_frame(block);

	ut_ad(page_is_leaf(page));

	if (!rec_offs_any_extern(offsets)
	    && ((page_get_data_size(page) - rec_offs_size(offsets)
		< BTR_CUR_PAGE_COMPRESS_LIMIT)
		|| (mach_read_from_4(page + FIL_PAGE_NEXT) == FIL_NULL
		    && mach_read_from_4(page + FIL_PAGE_PREV) == FIL_NULL)
		|| (page_get_n_recs(page) < 2))) {

		ulint	root_page_no = dict_index_get_page(index);

		/* The page fillfactor will drop below a predefined
		minimum value, OR the level in the B-tree contains just
		one page, OR the page will become empty: we recommend
		compression if this is not the root page. */

		no_compress_needed = page_get_page_no(page) == root_page_no;
	} else {
		no_compress_needed = true;
	}

	if (no_compress_needed) {
		page_zip_validate_if_zip(page_zip, page, index);
		page_cur_delete_rec(pcur, index, offsets, 0);
		page_zip_validate_if_zip(page_zip, page, index);
	}

	return(no_compress_needed);
}

