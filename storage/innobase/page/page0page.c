/******************************************************
Index page routines

(c) 1994-1996 Innobase Oy

Created 2/2/1994 Heikki Tuuri
*******************************************************/

#define THIS_MODULE
#include "page0page.h"
#ifdef UNIV_NONINL
#include "page0page.ic"
#endif
#undef THIS_MODULE

#include "page0cur.h"
#include "lock0lock.h"
#include "fut0lst.h"
#include "btr0sea.h"
#include "buf0buf.h"
#include "srv0srv.h"
#include "btr0btr.h"

/*			THE INDEX PAGE
			==============
		
The index page consists of a page header which contains the page's
id and other information. On top of it are the the index records
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

/*******************************************************************
Looks for the directory slot which owns the given record. */

ulint
page_dir_find_owner_slot(
/*=====================*/
			/* out: the directory slot number */
	rec_t*	rec)	/* in: the physical record */
{
	page_t*				page;
	register uint16			rec_offs_bytes;
	register page_dir_slot_t*	slot;
	register const page_dir_slot_t*	first_slot;
	register rec_t*			r = rec;

	ut_ad(page_rec_check(rec));

	page = buf_frame_align(rec);
	first_slot = page_dir_get_nth_slot(page, 0);
	slot = page_dir_get_nth_slot(page, page_dir_get_n_slots(page) - 1);

	if (page_is_comp(page)) {
		while (rec_get_n_owned(r, TRUE) == 0) {
			r = page + rec_get_next_offs(r, TRUE);
			ut_ad(r >= page + PAGE_NEW_SUPREMUM);
			ut_ad(r < page + (UNIV_PAGE_SIZE - PAGE_DIR));
		}
	} else {
		while (rec_get_n_owned(r, FALSE) == 0) {
			r = page + rec_get_next_offs(r, FALSE);
			ut_ad(r >= page + PAGE_OLD_SUPREMUM);
			ut_ad(r < page + (UNIV_PAGE_SIZE - PAGE_DIR));
		}
	}

	rec_offs_bytes = mach_encode_2(r - page);

	while (UNIV_LIKELY(*(uint16*) slot != rec_offs_bytes)) {

		if (UNIV_UNLIKELY(slot == first_slot)) {
			fprintf(stderr,
			"InnoDB: Probable data corruption on page %lu\n"
			"InnoDB: Original record ",
			(ulong) buf_frame_get_page_no(page));

			if (page_is_comp(page)) {
				fputs("(compact record)", stderr);
			} else {
				rec_print_old(stderr, rec);
			}

			fputs("\n"
			"InnoDB: on that page.\n"
			"InnoDB: Cannot find the dir slot for record ",
				stderr);
			if (page_is_comp(page)) {
				fputs("(compact record)", stderr);
			} else {
				rec_print_old(stderr, page
					+ mach_decode_2(rec_offs_bytes));
			}
			fputs("\n"
			"InnoDB: on that page!\n", stderr);

			buf_page_print(page);

			ut_error;
		}

		slot += PAGE_DIR_SLOT_SIZE;
	}

	return(((ulint) (first_slot - slot)) / PAGE_DIR_SLOT_SIZE);
}

/******************************************************************
Used to check the consistency of a directory slot. */
static
ibool
page_dir_slot_check(
/*================*/
					/* out: TRUE if succeed */
	page_dir_slot_t*	slot)	/* in: slot */
{
	page_t*	page;
	ulint	n_slots;
	ulint	n_owned;

	ut_a(slot);

	page = buf_frame_align(slot);

	n_slots = page_dir_get_n_slots(page);

	ut_a(slot <= page_dir_get_nth_slot(page, 0));
	ut_a(slot >= page_dir_get_nth_slot(page, n_slots - 1));

	ut_a(page_rec_check(page_dir_slot_get_rec(slot)));

	n_owned = rec_get_n_owned(page_dir_slot_get_rec(slot),
					page_is_comp(page));

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

/*****************************************************************
Sets the max trx id field value. */

void
page_set_max_trx_id(
/*================*/
	page_t*	page,	/* in: page */
	dulint	trx_id)	/* in: transaction id */
{
	buf_block_t*	block;

	ut_ad(page);

	block = buf_block_align(page);

	if (block->is_hashed) {
		rw_lock_x_lock(&btr_search_latch);
	}

	/* It is not necessary to write this change to the redo log, as
	during a database recovery we assume that the max trx id of every
	page is the maximum trx id assigned before the crash. */
	
	mach_write_to_8(page + PAGE_HEADER + PAGE_MAX_TRX_ID, trx_id);

	if (block->is_hashed) {
		rw_lock_x_unlock(&btr_search_latch);
	}
}

/****************************************************************
Allocates a block of memory from an index page. */

byte*
page_mem_alloc(
/*===========*/
				/* out: pointer to start of allocated
				buffer, or NULL if allocation fails */
	page_t*		page,	/* in: index page */
	ulint		need,	/* in: number of bytes needed */
	dict_index_t*	index,	/* in: record descriptor */
	ulint*		heap_no)/* out: this contains the heap number
				of the allocated record
				if allocation succeeds */
{
	rec_t*	rec;
	byte*	block;
	ulint	avl_space;
	ulint	garbage;
	
	ut_ad(page && heap_no);

	/* If there are records in the free list, look if the first is
	big enough */

	rec = page_header_get_ptr(page, PAGE_FREE);

	if (rec) {
		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*		offsets		= offsets_;
		*offsets_ = (sizeof offsets_) / sizeof *offsets_;

		offsets = rec_get_offsets(rec, index, offsets,
						ULINT_UNDEFINED, &heap);

		if (rec_offs_size(offsets) >= need) {
			page_header_set_ptr(page, PAGE_FREE,
				page_rec_get_next(rec));

			garbage = page_header_get_field(page, PAGE_GARBAGE);
			ut_ad(garbage >= need);

			page_header_set_field(page, PAGE_GARBAGE,
							garbage - need);

			*heap_no = rec_get_heap_no(rec, page_is_comp(page));

			block = rec_get_start(rec, offsets);
			if (UNIV_LIKELY_NULL(heap)) {
				mem_heap_free(heap);
			}
			return(block);
		}

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
	}

	/* Could not find space from the free list, try top of heap */
	
	avl_space = page_get_max_insert_size(page, 1);
	
	if (avl_space >= need) {
		block = page_header_get_ptr(page, PAGE_HEAP_TOP);

		page_header_set_ptr(page, PAGE_HEAP_TOP, block + need);
		*heap_no = page_dir_get_n_heap(page);

		page_dir_set_n_heap(page, 1 + *heap_no);

		return(block);
	}

	return(NULL);
}

/**************************************************************
Writes a log record of page creation. */
UNIV_INLINE
void
page_create_write_log(
/*==================*/
	buf_frame_t*	frame,	/* in: a buffer frame where the page is
				created */
	mtr_t*		mtr,	/* in: mini-transaction handle */
	ulint		comp)	/* in: nonzero=compact page format */
{
	mlog_write_initial_log_record(frame,
			comp ? MLOG_COMP_PAGE_CREATE : MLOG_PAGE_CREATE, mtr);
}

/***************************************************************
Parses a redo log record of creating a page. */

byte*
page_parse_create(
/*==============*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr __attribute__((unused)), /* in: buffer end */
	ulint	comp,	/* in: nonzero=compact page format */
	page_t*	page,	/* in: page or NULL */
	mtr_t*	mtr)	/* in: mtr or NULL */
{
	ut_ad(ptr && end_ptr);

	/* The record is empty, except for the record initial part */

	if (page) {
		page_create(page, mtr, comp);
	}

	return(ptr);
}

/**************************************************************
The index page creation function. */

page_t* 
page_create(
/*========*/
				/* out: pointer to the page */
	buf_frame_t*	frame,	/* in: a buffer frame where the page is
				created */
	mtr_t*		mtr,	/* in: mini-transaction handle */
	ulint		comp)	/* in: nonzero=compact page format */
{
	page_dir_slot_t* slot;
	mem_heap_t*	heap;
	dtuple_t*	tuple;	
	dfield_t*	field;
	byte*		heap_top;
	rec_t*		infimum_rec;
	rec_t*		supremum_rec;
	page_t*		page;
	dict_index_t*	index;
	ulint*		offsets;

	index = comp ? srv_sys->dummy_ind2 : srv_sys->dummy_ind1;
	
	ut_ad(frame && mtr);
	ut_ad(PAGE_BTR_IBUF_FREE_LIST + FLST_BASE_NODE_SIZE
	      <= PAGE_DATA);
	ut_ad(PAGE_BTR_IBUF_FREE_LIST_NODE + FLST_NODE_SIZE
	      <= PAGE_DATA);

	/* 1. INCREMENT MODIFY CLOCK */
	buf_frame_modify_clock_inc(frame);

	/* 2. WRITE LOG INFORMATION */
	page_create_write_log(frame, mtr, comp);
	
	page = frame;

	fil_page_set_type(page, FIL_PAGE_INDEX);

	heap = mem_heap_create(200);
		
	/* 3. CREATE THE INFIMUM AND SUPREMUM RECORDS */

	/* Create first a data tuple for infimum record */
	tuple = dtuple_create(heap, 1);
	dtuple_set_info_bits(tuple, REC_STATUS_INFIMUM);
	field = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(field, "infimum", 8);
	dtype_set(dfield_get_type(field),
			DATA_VARCHAR, DATA_ENGLISH | DATA_NOT_NULL, 8, 0);
	/* Set the corresponding physical record to its place in the page
	record heap */

	heap_top = page + PAGE_DATA;
	
	infimum_rec = rec_convert_dtuple_to_rec(heap_top, index, tuple);

	ut_a(infimum_rec ==
		page + (comp ? PAGE_NEW_INFIMUM : PAGE_OLD_INFIMUM));

	rec_set_n_owned(infimum_rec, comp, 1);
	rec_set_heap_no(infimum_rec, comp, 0);
	offsets = rec_get_offsets(infimum_rec, index, NULL,
						ULINT_UNDEFINED, &heap);

	heap_top = rec_get_end(infimum_rec, offsets);

	/* Create then a tuple for supremum */

	tuple = dtuple_create(heap, 1);
	dtuple_set_info_bits(tuple, REC_STATUS_SUPREMUM);
	field = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(field, "supremum", comp ? 8 : 9);
	dtype_set(dfield_get_type(field),
		DATA_VARCHAR, DATA_ENGLISH | DATA_NOT_NULL, comp ? 8 : 9, 0);

	supremum_rec = rec_convert_dtuple_to_rec(heap_top, index, tuple);

	ut_a(supremum_rec ==
		page + (comp ? PAGE_NEW_SUPREMUM : PAGE_OLD_SUPREMUM));

	rec_set_n_owned(supremum_rec, comp, 1);
	rec_set_heap_no(supremum_rec, comp, 1);

	offsets = rec_get_offsets(supremum_rec, index, offsets,
						ULINT_UNDEFINED, &heap);
	heap_top = rec_get_end(supremum_rec, offsets);

	ut_ad(heap_top ==
		page + (comp ? PAGE_NEW_SUPREMUM_END : PAGE_OLD_SUPREMUM_END));

	mem_heap_free(heap);

	/* 4. INITIALIZE THE PAGE */

	page_header_set_field(page, PAGE_N_DIR_SLOTS, 2);
	page_header_set_ptr(page, PAGE_HEAP_TOP, heap_top);
	page_header_set_field(page, PAGE_N_HEAP, comp ? 0x8002 : 2);
	page_header_set_ptr(page, PAGE_FREE, NULL);
	page_header_set_field(page, PAGE_GARBAGE, 0);
	page_header_set_ptr(page, PAGE_LAST_INSERT, NULL);
	page_header_set_field(page, PAGE_DIRECTION, PAGE_NO_DIRECTION);
	page_header_set_field(page, PAGE_N_DIRECTION, 0);
	page_header_set_field(page, PAGE_N_RECS, 0);
	page_set_max_trx_id(page, ut_dulint_zero);
	memset(heap_top, 0, UNIV_PAGE_SIZE - PAGE_EMPTY_DIR_START
						- (heap_top - page));

	/* 5. SET POINTERS IN RECORDS AND DIR SLOTS */

	/* Set the slots to point to infimum and supremum. */

	slot = page_dir_get_nth_slot(page, 0);
	page_dir_slot_set_rec(slot, infimum_rec);

	slot = page_dir_get_nth_slot(page, 1);
	page_dir_slot_set_rec(slot, supremum_rec);

	/* Set the next pointers in infimum and supremum */
	
	rec_set_next_offs(infimum_rec, comp, (ulint)(supremum_rec - page));
	rec_set_next_offs(supremum_rec, comp, 0);

	return(page);
}

/*****************************************************************
Differs from page_copy_rec_list_end, because this function does not
touch the lock table and max trx id on page. */

void
page_copy_rec_list_end_no_locks(
/*============================*/
	page_t*		new_page,	/* in: index page to copy to */
	page_t*		page,		/* in: index page */
	rec_t*		rec,		/* in: record on page */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr)		/* in: mtr */
{
	page_cur_t	cur1;
	page_cur_t	cur2;
	rec_t*		sup;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	page_cur_position(rec, &cur1);

	if (page_cur_is_before_first(&cur1)) {

		page_cur_move_to_next(&cur1);
	}

	ut_a((ibool)!!page_is_comp(new_page) == index->table->comp);
	ut_a(page_is_comp(new_page) == page_is_comp(page));
	ut_a(mach_read_from_2(new_page + UNIV_PAGE_SIZE - 10) == (ulint)
		(page_is_comp(new_page)
		? PAGE_NEW_INFIMUM : PAGE_OLD_INFIMUM));

	page_cur_set_before_first(new_page, &cur2);
	
	/* Copy records from the original page to the new page */	

	sup = page_get_supremum_rec(page);
	
	for (;;) {
		rec_t*	cur1_rec = page_cur_get_rec(&cur1);
		if (cur1_rec == sup) {
			break;
		}
		offsets = rec_get_offsets(cur1_rec, index, offsets,
					ULINT_UNDEFINED, &heap);
		if (UNIV_UNLIKELY(!page_cur_rec_insert(&cur2, cur1_rec, index,
							offsets, mtr))) {
			/* Track an assertion failure reported on the mailing
			list on June 18th, 2003 */

		        buf_page_print(new_page);
		        buf_page_print(page);
			ut_print_timestamp(stderr);

			fprintf(stderr,
"InnoDB: rec offset %lu, cur1 offset %lu, cur2 offset %lu\n",
			      (ulong)(rec - page),
			      (ulong)(page_cur_get_rec(&cur1) - page),
			      (ulong)(page_cur_get_rec(&cur2) - new_page));
			ut_error;
		}

		page_cur_move_to_next(&cur1);
		page_cur_move_to_next(&cur2);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/*****************************************************************
Copies records from page to new_page, from a given record onward,
including that record. Infimum and supremum records are not copied.
The records are copied to the start of the record list on new_page. */

void
page_copy_rec_list_end(
/*===================*/
	page_t*		new_page,	/* in: index page to copy to */
	page_t*		page,		/* in: index page */
	rec_t*		rec,		/* in: record on page */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr)		/* in: mtr */
{
	if (page_dir_get_n_heap(new_page) == 2) {
		page_copy_rec_list_end_to_created_page(new_page, page, rec,
								index, mtr);
	} else {
		page_copy_rec_list_end_no_locks(new_page, page, rec,
								index, mtr);
	}

	/* Update the lock table, MAX_TRX_ID, and possible hash index */

	lock_move_rec_list_end(new_page, page, rec);

	page_update_max_trx_id(new_page, page_get_max_trx_id(page));

	btr_search_move_or_delete_hash_entries(new_page, page, index);
}	

/*****************************************************************
Copies records from page to new_page, up to the given record,
NOT including that record. Infimum and supremum records are not copied.
The records are copied to the end of the record list on new_page. */

void
page_copy_rec_list_start(
/*=====================*/
	page_t*		new_page,	/* in: index page to copy to */
	page_t*		page,		/* in: index page */
	rec_t*		rec,		/* in: record on page */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr)		/* in: mtr */
{
	page_cur_t	cur1;
	page_cur_t	cur2;
	rec_t*		old_end;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	page_cur_set_before_first(page, &cur1);

	if (rec == page_cur_get_rec(&cur1)) {

		return;
	}

	page_cur_move_to_next(&cur1);
	
	page_cur_set_after_last(new_page, &cur2);
	page_cur_move_to_prev(&cur2);
	old_end = page_cur_get_rec(&cur2);
	
	/* Copy records from the original page to the new page */	

	while (page_cur_get_rec(&cur1) != rec) {
		rec_t*	ins_rec;
		rec_t*	cur1_rec = page_cur_get_rec(&cur1);
		offsets = rec_get_offsets(cur1_rec, index, offsets,
					ULINT_UNDEFINED, &heap);
		ins_rec = page_cur_rec_insert(&cur2, cur1_rec, index,
						offsets, mtr);
		ut_a(ins_rec);

		page_cur_move_to_next(&cur1);
		page_cur_move_to_next(&cur2);
	}

	/* Update the lock table, MAX_TRX_ID, and possible hash index */
	
	lock_move_rec_list_start(new_page, page, rec, old_end);

	page_update_max_trx_id(new_page, page_get_max_trx_id(page));

	btr_search_move_or_delete_hash_entries(new_page, page, index);

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}
}

/**************************************************************
Writes a log record of a record list end or start deletion. */
UNIV_INLINE
void
page_delete_rec_list_write_log(
/*===========================*/
	rec_t*		rec,	/* in: record on page */
	dict_index_t*	index,	/* in: record descriptor */
	byte		type,	/* in: operation type:
				MLOG_LIST_END_DELETE, ... */
	mtr_t*		mtr)	/* in: mtr */
{
	byte*	log_ptr;
	ut_ad(type == MLOG_LIST_END_DELETE
		|| type == MLOG_LIST_START_DELETE
		|| type == MLOG_COMP_LIST_END_DELETE
		|| type == MLOG_COMP_LIST_START_DELETE);

	log_ptr = mlog_open_and_write_index(mtr, rec, index, type, 2);
	if (log_ptr) {
		/* Write the parameter as a 2-byte ulint */
		mach_write_to_2(log_ptr, ut_align_offset(rec, UNIV_PAGE_SIZE));
		mlog_close(mtr, log_ptr + 2);
	}
}

/**************************************************************
Parses a log record of a record list end or start deletion. */

byte*
page_parse_delete_rec_list(
/*=======================*/
				/* out: end of log record or NULL */
	byte		type,	/* in: MLOG_LIST_END_DELETE,
				MLOG_LIST_START_DELETE,
				MLOG_COMP_LIST_END_DELETE or
				MLOG_COMP_LIST_START_DELETE */
	byte*		ptr,	/* in: buffer */
	byte*		end_ptr,/* in: buffer end */
	dict_index_t*	index,	/* in: record descriptor */
	page_t*		page,	/* in: page or NULL */
	mtr_t*		mtr)	/* in: mtr or NULL */
{
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

	if (!page) {

		return(ptr);
	}

	ut_ad(!!page_is_comp(page) == index->table->comp);

	if (type == MLOG_LIST_END_DELETE
			|| type == MLOG_COMP_LIST_END_DELETE) {
		page_delete_rec_list_end(page, page + offset, index,
					ULINT_UNDEFINED, ULINT_UNDEFINED, mtr);
	} else {
		page_delete_rec_list_start(page, page + offset, index, mtr);
	}

	return(ptr);
}

/*****************************************************************
Deletes records from a page from a given record onward, including that record.
The infimum and supremum records are not deleted. */

void
page_delete_rec_list_end(
/*=====================*/
	page_t*		page,	/* in: index page */
	rec_t*		rec,	/* in: record on page */
	dict_index_t*	index,	/* in: record descriptor */
	ulint		n_recs,	/* in: number of records to delete,
				or ULINT_UNDEFINED if not known */
	ulint		size,	/* in: the sum of the sizes of the
				records in the end of the chain to
				delete, or ULINT_UNDEFINED if not known */
	mtr_t*		mtr)	/* in: mtr */
{
	page_dir_slot_t* slot;
	ulint	slot_index;
	rec_t*	last_rec;
	rec_t*	prev_rec;
	rec_t*	free;
	rec_t*	rec2;
	ulint	count;
	ulint	n_owned;
	rec_t*	sup;
	ulint	comp;

	/* Reset the last insert info in the page header and increment
	the modify clock for the frame */

	ut_ad(size == ULINT_UNDEFINED || size < UNIV_PAGE_SIZE);
	page_header_set_ptr(page, PAGE_LAST_INSERT, NULL);

	/* The page gets invalid for optimistic searches: increment the
	frame modify clock */

	buf_frame_modify_clock_inc(page);
	
	sup = page_get_supremum_rec(page);
	
	comp = page_is_comp(page);
	if (page_rec_is_infimum_low(rec - page)) {
		rec = page_rec_get_next(rec);
	}

	page_delete_rec_list_write_log(rec, index,
		comp ? MLOG_COMP_LIST_END_DELETE : MLOG_LIST_END_DELETE, mtr);

	if (rec == sup) {

		return;
	}
	
	prev_rec = page_rec_get_prev(rec);

	last_rec = page_rec_get_prev(sup);

	if ((size == ULINT_UNDEFINED) || (n_recs == ULINT_UNDEFINED)) {
		mem_heap_t*	heap		= NULL;
		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*		offsets		= offsets_;
		*offsets_ = (sizeof offsets_) / sizeof *offsets_;
		/* Calculate the sum of sizes and the number of records */
		size = 0;
		n_recs = 0;
		rec2 = rec;

		while (rec2 != sup) {
			ulint	s;
			offsets = rec_get_offsets(rec2, index, offsets,
						ULINT_UNDEFINED, &heap);
			s = rec_offs_size(offsets);
			ut_ad(rec2 - page + s - rec_offs_extra_size(offsets)
				< UNIV_PAGE_SIZE);
			ut_ad(size + s < UNIV_PAGE_SIZE);
			size += s;
			n_recs++;

			rec2 = page_rec_get_next(rec2);
		}

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
	}

	ut_ad(size < UNIV_PAGE_SIZE);

	/* Update the page directory; there is no need to balance the number
	of the records owned by the supremum record, as it is allowed to be
	less than PAGE_DIR_SLOT_MIN_N_OWNED */
	
	rec2 = rec;
	count = 0;
	
	while (rec_get_n_owned(rec2, comp) == 0) {
		count++;

		rec2 = page_rec_get_next(rec2);
	}

	ut_ad(rec_get_n_owned(rec2, comp) - count > 0);

	n_owned = rec_get_n_owned(rec2, comp) - count;
	
	slot_index = page_dir_find_owner_slot(rec2);
	slot = page_dir_get_nth_slot(page, slot_index);
	
	page_dir_slot_set_rec(slot, sup);
	page_dir_slot_set_n_owned(slot, n_owned);

	page_dir_set_n_slots(page, slot_index + 1);
	
	/* Remove the record chain segment from the record chain */
	page_rec_set_next(prev_rec, page_get_supremum_rec(page));

	/* Catenate the deleted chain segment to the page free list */

	free = page_header_get_ptr(page, PAGE_FREE);

	page_rec_set_next(last_rec, free);
	page_header_set_ptr(page, PAGE_FREE, rec);

	page_header_set_field(page, PAGE_GARBAGE,
			size + page_header_get_field(page, PAGE_GARBAGE));

	page_header_set_field(page, PAGE_N_RECS,
				(ulint)(page_get_n_recs(page) - n_recs));
}	

/*****************************************************************
Deletes records from page, up to the given record, NOT including
that record. Infimum and supremum records are not deleted. */

void
page_delete_rec_list_start(
/*=======================*/
	page_t*		page,	/* in: index page */
	rec_t*		rec,	/* in: record on page */
	dict_index_t*	index,	/* in: record descriptor */
	mtr_t*		mtr)	/* in: mtr */
{
	page_cur_t	cur1;
	ulint		log_mode;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	mem_heap_t*	heap		= NULL;
	byte		type;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_ad(!!page_is_comp(page) == index->table->comp);

	if (page_is_comp(page)) {
		type = MLOG_COMP_LIST_START_DELETE;
	} else {
		type = MLOG_LIST_START_DELETE;
	}

	page_delete_rec_list_write_log(rec, index, type, mtr);

	page_cur_set_before_first(page, &cur1);

	if (rec == page_cur_get_rec(&cur1)) {

		return;
	}

	page_cur_move_to_next(&cur1);
	
	/* Individual deletes are not logged */

	log_mode = mtr_set_log_mode(mtr, MTR_LOG_NONE);

	while (page_cur_get_rec(&cur1) != rec) {
		offsets = rec_get_offsets(page_cur_get_rec(&cur1), index,
					offsets, ULINT_UNDEFINED, &heap);
		page_cur_delete_rec(&cur1, index, offsets, mtr);
	}

	if (UNIV_LIKELY_NULL(heap)) {
		mem_heap_free(heap);
	}

	/* Restore log mode */

	mtr_set_log_mode(mtr, log_mode);
}	

/*****************************************************************
Moves record list end to another page. Moved records include
split_rec. */

void
page_move_rec_list_end(
/*===================*/
	page_t*		new_page,	/* in: index page where to move */
	page_t*		page,		/* in: index page */
	rec_t*		split_rec,	/* in: first record to move */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr)		/* in: mtr */
{
	ulint	old_data_size;
	ulint	new_data_size;
	ulint	old_n_recs;
	ulint	new_n_recs;

	old_data_size = page_get_data_size(new_page);
	old_n_recs = page_get_n_recs(new_page);
	
	page_copy_rec_list_end(new_page, page, split_rec, index, mtr);

	new_data_size = page_get_data_size(new_page);
	new_n_recs = page_get_n_recs(new_page);

	ut_ad(new_data_size >= old_data_size);

	page_delete_rec_list_end(page, split_rec, index,
		new_n_recs - old_n_recs, new_data_size - old_data_size, mtr);
}

/*****************************************************************
Moves record list start to another page. Moved records do not include
split_rec. */

void
page_move_rec_list_start(
/*=====================*/
	page_t*		new_page,	/* in: index page where to move */
	page_t*		page,		/* in: index page */
	rec_t*		split_rec,	/* in: first record not to move */
	dict_index_t*	index,		/* in: record descriptor */
	mtr_t*		mtr)		/* in: mtr */
{
	page_copy_rec_list_start(new_page, page, split_rec, index, mtr);

	page_delete_rec_list_start(page, split_rec, index, mtr);
}

/***************************************************************************
This is a low-level operation which is used in a database index creation
to update the page number of a created B-tree to a data dictionary record. */

void
page_rec_write_index_page_no(
/*=========================*/
	rec_t*	rec,	/* in: record to update */
	ulint	i,	/* in: index of the field to update */
	ulint	page_no,/* in: value to write */
	mtr_t*	mtr)	/* in: mtr */
{
	byte*	data;
	ulint	len;
	
	data = rec_get_nth_field_old(rec, i, &len);

	ut_ad(len == 4);

	mlog_write_ulint(data, page_no, MLOG_4BYTES, mtr);
}

/******************************************************************
Used to delete n slots from the directory. This function updates
also n_owned fields in the records, so that the first slot after
the deleted ones inherits the records of the deleted slots. */
UNIV_INLINE
void
page_dir_delete_slots(
/*==================*/
	page_t*	page,	/* in: the index page */
	ulint	start,	/* in: first slot to be deleted */
	ulint	n)	/* in: number of slots to delete (currently 
			only n == 1 allowed) */
{
	page_dir_slot_t*	slot;
	ulint			i;
	ulint			sum_owned = 0;
	ulint			n_slots;
	rec_t*			rec;

	ut_ad(n == 1);	
	ut_ad(start > 0);
	ut_ad(start + n < page_dir_get_n_slots(page));

	n_slots = page_dir_get_n_slots(page);

	/* 1. Reset the n_owned fields of the slots to be
	deleted */
	for (i = start; i < start + n; i++) {
		slot = page_dir_get_nth_slot(page, i);
		sum_owned += page_dir_slot_get_n_owned(slot);
		page_dir_slot_set_n_owned(slot, 0);
	}

	/* 2. Update the n_owned value of the first non-deleted slot */

	slot = page_dir_get_nth_slot(page, start + n);
	page_dir_slot_set_n_owned(slot,
				sum_owned + page_dir_slot_get_n_owned(slot));

	/* 3. Destroy start and other slots by copying slots */
	for (i = start + n; i < n_slots; i++) {
		slot = page_dir_get_nth_slot(page, i);
		rec = page_dir_slot_get_rec(slot);

		slot = page_dir_get_nth_slot(page, i - n);
		page_dir_slot_set_rec(slot, rec);
	}

	/* 4. Update the page header */
	page_header_set_field(page, PAGE_N_DIR_SLOTS, n_slots - n);
}

/******************************************************************
Used to add n slots to the directory. Does not set the record pointers
in the added slots or update n_owned values: this is the responsibility
of the caller. */
UNIV_INLINE
void
page_dir_add_slots(
/*===============*/
	page_t*	page,	/* in: the index page */
	ulint	start,	/* in: the slot above which the new slots are added */
	ulint	n)	/* in: number of slots to add (currently only n == 1 
			allowed) */
{
	page_dir_slot_t*	slot;
	ulint			n_slots;
	ulint			i;
	rec_t*			rec;

	ut_ad(n == 1);
	
	n_slots = page_dir_get_n_slots(page);

	ut_ad(start < n_slots - 1);

	/* Update the page header */
	page_dir_set_n_slots(page, n_slots + n);

	/* Move slots up */

	for (i = n_slots - 1; i > start; i--) {

		slot = page_dir_get_nth_slot(page, i);
		rec = page_dir_slot_get_rec(slot);

		slot = page_dir_get_nth_slot(page, i + n);
		page_dir_slot_set_rec(slot, rec);
	}
}

/********************************************************************
Splits a directory slot which owns too many records. */

void
page_dir_split_slot(
/*================*/
	page_t*	page,		/* in: the index page in question */
	ulint	slot_no)	/* in: the directory slot */
{		
	rec_t*			rec;
	page_dir_slot_t*	new_slot;
	page_dir_slot_t*	prev_slot;
	page_dir_slot_t*	slot;
	ulint			i;
	ulint			n_owned;

	ut_ad(page);
	ut_ad(slot_no > 0);

	slot = page_dir_get_nth_slot(page, slot_no);
	
	n_owned = page_dir_slot_get_n_owned(slot);
	ut_ad(n_owned == PAGE_DIR_SLOT_MAX_N_OWNED + 1);

	/* 1. We loop to find a record approximately in the middle of the 
	records owned by the slot. */
	
	prev_slot = page_dir_get_nth_slot(page, slot_no - 1);
	rec = page_dir_slot_get_rec(prev_slot);

	for (i = 0; i < n_owned / 2; i++) {
		rec = page_rec_get_next(rec);
	}

	ut_ad(n_owned / 2 >= PAGE_DIR_SLOT_MIN_N_OWNED);

	/* 2. We add one directory slot immediately below the slot to be
	split. */

	page_dir_add_slots(page, slot_no - 1, 1);

	/* The added slot is now number slot_no, and the old slot is
	now number slot_no + 1 */

	new_slot = page_dir_get_nth_slot(page, slot_no);
	slot = page_dir_get_nth_slot(page, slot_no + 1);

	/* 3. We store the appropriate values to the new slot. */
	
	page_dir_slot_set_rec(new_slot, rec);
	page_dir_slot_set_n_owned(new_slot, n_owned / 2);
	
	/* 4. Finally, we update the number of records field of the 
	original slot */

	page_dir_slot_set_n_owned(slot, n_owned - (n_owned / 2));
}

/*****************************************************************
Tries to balance the given directory slot with too few records with the upper
neighbor, so that there are at least the minimum number of records owned by
the slot; this may result in the merging of two slots. */

void
page_dir_balance_slot(
/*==================*/
	page_t*	page,		/* in: index page */
	ulint	slot_no) 	/* in: the directory slot */
{
	page_dir_slot_t*	slot;
	page_dir_slot_t*	up_slot;
	ulint			n_owned;
	ulint			up_n_owned;
	rec_t*			old_rec;
	rec_t*			new_rec;

	ut_ad(page);
	ut_ad(slot_no > 0);

	slot = page_dir_get_nth_slot(page, slot_no);
	
	/* The last directory slot cannot be balanced with the upper
	neighbor, as there is none. */

	if (slot_no == page_dir_get_n_slots(page) - 1) {

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
		old_rec = page_dir_slot_get_rec(slot);
		new_rec = page_rec_get_next(old_rec);
		
		rec_set_n_owned(old_rec, page_is_comp(page), 0);
		rec_set_n_owned(new_rec, page_is_comp(page), n_owned + 1);
		
		page_dir_slot_set_rec(slot, new_rec);
		
		page_dir_slot_set_n_owned(up_slot, up_n_owned -1);
	} else {
		/* In this case we may merge the two slots */
		page_dir_delete_slots(page, slot_no, 1);
	}		
}

/****************************************************************
Returns the middle record of the record list. If there are an even number
of records in the list, returns the first record of the upper half-list. */

rec_t*
page_get_middle_rec(
/*================*/
			/* out: middle record */
	page_t*	page)	/* in: page */
{
	page_dir_slot_t*	slot;
	ulint			middle;
	ulint			i;
	ulint			n_owned;
	ulint			count;
	rec_t*			rec;

	/* This many records we must leave behind */
	middle = (page_get_n_recs(page) + 2) / 2;

	count = 0;

	for (i = 0;; i++) {

		slot = page_dir_get_nth_slot(page, i);
		n_owned = page_dir_slot_get_n_owned(slot);

		if (count + n_owned > middle) {
			break;
		} else {
			count += n_owned;
		}
	}

	ut_ad(i > 0);
	slot = page_dir_get_nth_slot(page, i - 1);
	rec = page_dir_slot_get_rec(slot);
	rec = page_rec_get_next(rec);

	/* There are now count records behind rec */

	for (i = 0; i < middle - count; i++) {
		rec = page_rec_get_next(rec);
	}

	return(rec);
}
	
/*******************************************************************
Returns the number of records before the given record in chain.
The number includes infimum and supremum records. */

ulint
page_rec_get_n_recs_before(
/*=======================*/
			/* out: number of records */
	rec_t*	rec)	/* in: the physical record */
{
	page_dir_slot_t*	slot;
	rec_t*			slot_rec;
	page_t*			page;
	ulint			i;
	ulint			comp;
	lint			n	= 0;

	ut_ad(page_rec_check(rec));

	page = buf_frame_align(rec);
	comp = page_is_comp(page);

	while (rec_get_n_owned(rec, comp) == 0) {

		rec = page_rec_get_next(rec);
		n--;
	}
	
	for (i = 0; ; i++) {
		slot = page_dir_get_nth_slot(page, i);
		slot_rec = page_dir_slot_get_rec(slot);

		n += rec_get_n_owned(slot_rec, comp);

		if (rec == slot_rec) {

			break;
		}
	}

	n--;

	ut_ad(n >= 0);

	return((ulint) n);
}

/****************************************************************
Prints record contents including the data relevant only in
the index page context. */
 
void
page_rec_print(
/*===========*/
	rec_t*		rec,	/* in: physical record */
	const ulint*	offsets)/* in: record descriptor */
{
	ulint	comp	= page_is_comp(buf_frame_align(rec));

	ut_a(!comp == !rec_offs_comp(offsets));
	rec_print_new(stderr, rec, offsets);
	fprintf(stderr,
     		"            n_owned: %lu; heap_no: %lu; next rec: %lu\n",
		(ulong) rec_get_n_owned(rec, comp),
		(ulong) rec_get_heap_no(rec, comp),
		(ulong) rec_get_next_offs(rec, comp));

	page_rec_check(rec);
	rec_validate(rec, offsets);
}

/*******************************************************************
This is used to print the contents of the directory for
debugging purposes. */

void
page_dir_print(
/*===========*/
	page_t*	page,	/* in: index page */
	ulint	pr_n)	/* in: print n first and n last entries */
{
	ulint			n;
	ulint			i;
	page_dir_slot_t*	slot;

	n = page_dir_get_n_slots(page);
	
	fprintf(stderr, "--------------------------------\n"
		"PAGE DIRECTORY\n"
		"Page address %p\n"
		"Directory stack top at offs: %lu; number of slots: %lu\n",
		page, (ulong)(page_dir_get_nth_slot(page, n - 1) - page), (ulong) n);
	for (i = 0; i < n; i++) {
		slot = page_dir_get_nth_slot(page, i);
		if ((i == pr_n) && (i < n - pr_n)) {
			fputs("    ...   \n", stderr);
		}
	    	if ((i < pr_n) || (i >= n - pr_n)) {
			fprintf(stderr,
	   	   "Contents of slot: %lu: n_owned: %lu, rec offs: %lu\n",
			(ulong) i, (ulong) page_dir_slot_get_n_owned(slot),
			(ulong)(page_dir_slot_get_rec(slot) - page));
	    	}
	}
	fprintf(stderr, "Total of %lu records\n"
		"--------------------------------\n",
		(ulong) (2 + page_get_n_recs(page)));
}	
	
/*******************************************************************
This is used to print the contents of the page record list for
debugging purposes. */

void
page_print_list(
/*============*/
	page_t*		page,	/* in: index page */
	dict_index_t*	index,	/* in: dictionary index of the page */
	ulint		pr_n)	/* in: print n first and n last entries */
{
	page_cur_t	cur;
	ulint		count;
	ulint		n_recs;
	mem_heap_t*	heap		= NULL;
	ulint		offsets_[REC_OFFS_NORMAL_SIZE];
	ulint*		offsets		= offsets_;
	*offsets_ = (sizeof offsets_) / sizeof *offsets_;

	ut_a((ibool)!!page_is_comp(page) == index->table->comp);

	fprintf(stderr,
		"--------------------------------\n"
		"PAGE RECORD LIST\n"
		"Page address %p\n", page);

	n_recs = page_get_n_recs(page);

	page_cur_set_before_first(page, &cur);
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

/*******************************************************************
Prints the info in a page header. */

void
page_header_print(
/*==============*/
	page_t*	page)
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

/*******************************************************************
This is used to print the contents of the page for
debugging purposes. */

void
page_print(
/*======*/
	page_t*		page,	/* in: index page */
	dict_index_t*	index,	/* in: dictionary index of the page */
	ulint		dn,	/* in: print dn first and last entries
				in directory */
	ulint		rn)	/* in: print rn first and last records
				in directory */
{
	page_header_print(page);
	page_dir_print(page, dn);
	page_print_list(page, index, rn);
}	

/*******************************************************************
The following is used to validate a record on a page. This function
differs from rec_validate as it can also check the n_owned field and
the heap_no field. */

ibool
page_rec_validate(
/*==============*/
				/* out: TRUE if ok */
	rec_t*		rec,	/* in: physical record */
	const ulint*	offsets)/* in: array returned by rec_get_offsets() */
{
	ulint	n_owned;
	ulint	heap_no;
	page_t*	page;
	ulint	comp;

	page = buf_frame_align(rec);
	comp = page_is_comp(page);
	ut_a(!comp == !rec_offs_comp(offsets));

	page_rec_check(rec);
	rec_validate(rec, offsets);

	n_owned = rec_get_n_owned(rec, comp);
	heap_no = rec_get_heap_no(rec, comp);

	if (!(n_owned <= PAGE_DIR_SLOT_MAX_N_OWNED)) {
		fprintf(stderr,
			"InnoDB: Dir slot of rec %lu, n owned too big %lu\n",
				(ulong)(rec - page), (ulong) n_owned);
		return(FALSE);
	}

	if (!(heap_no < page_dir_get_n_heap(page))) {
		fprintf(stderr,
		"InnoDB: Heap no of rec %lu too big %lu %lu\n",
				(ulong)(rec - page), (ulong) heap_no,
				(ulong) page_dir_get_n_heap(page));
		return(FALSE);
	}
	
	return(TRUE);
}

/*******************************************************************
Checks that the first directory slot points to the infimum record and
the last to the supremum. This function is intended to track if the
bug fixed in 4.0.14 has caused corruption to users' databases. */

void
page_check_dir(
/*===========*/
	page_t*	page)	/* in: index page */
{
	ulint	n_slots;

	n_slots = page_dir_get_n_slots(page);

	if (page_dir_slot_get_rec(page_dir_get_nth_slot(page, 0))
	    != page_get_infimum_rec(page)) {

	        fprintf(stderr,
"InnoDB: Page directory corruption: supremum not pointed to\n");
		buf_page_print(page);
       	}

	if (page_dir_slot_get_rec(page_dir_get_nth_slot(page, n_slots - 1))
	    != page_get_supremum_rec(page)) {

	        fprintf(stderr,
"InnoDB: Page directory corruption: supremum not pointed to\n");
		buf_page_print(page);
       	}
}
	
/*******************************************************************
This function checks the consistency of an index page when we do not
know the index. This is also resilient so that this should never crash
even if the page is total garbage. */

ibool
page_simple_validate(
/*=================*/
			/* out: TRUE if ok */
	page_t*	page)	/* in: index page */
{
	page_cur_t 	cur;
	page_dir_slot_t* slot;
	ulint		slot_no;
	ulint		n_slots;
	rec_t*		rec;
	byte*		rec_heap_top;
	ulint		count;
	ulint		own_count;
	ibool		ret	= FALSE;
	ulint		comp	= page_is_comp(page);

	/* Check first that the record heap and the directory do not
	overlap. */

	n_slots = page_dir_get_n_slots(page);

	if (n_slots > UNIV_PAGE_SIZE / 4) {
		fprintf(stderr,
	"InnoDB: Nonsensical number %lu of page dir slots\n", (ulong) n_slots);

		goto func_exit;
	}

	rec_heap_top = page_header_get_ptr(page, PAGE_HEAP_TOP);
	
	if (rec_heap_top > page_dir_get_nth_slot(page, n_slots - 1)) {

		fprintf(stderr,
    "InnoDB: Record heap and dir overlap on a page, heap top %lu, dir %lu\n",
       		(ulong)(page_header_get_ptr(page, PAGE_HEAP_TOP) - page),
       		(ulong)(page_dir_get_nth_slot(page, n_slots - 1) - page));

       		goto func_exit;
       	}

	/* Validate the record list in a loop checking also that it is
	consistent with the page record directory. */

	count = 0;
	own_count = 1;
	slot_no = 0;
	slot = page_dir_get_nth_slot(page, slot_no);

	page_cur_set_before_first(page, &cur);

	for (;;) {
		rec = (&cur)->rec;
		
		if (rec > rec_heap_top) {
			fprintf(stderr,
			"InnoDB: Record %lu is above rec heap top %lu\n",
			(ulong)(rec - page), (ulong)(rec_heap_top - page));

			goto func_exit;
		}

		if (rec_get_n_owned(rec, comp) != 0) {
			/* This is a record pointed to by a dir slot */
			if (rec_get_n_owned(rec, comp) != own_count) {

				fprintf(stderr,
		"InnoDB: Wrong owned count %lu, %lu, rec %lu\n",
				(ulong) rec_get_n_owned(rec, comp),
				(ulong) own_count,
				(ulong)(rec - page));

				goto func_exit;
			}

			if (page_dir_slot_get_rec(slot) != rec) {
				fprintf(stderr,
		"InnoDB: Dir slot does not point to right rec %lu\n",
					(ulong)(rec - page));

				goto func_exit;
			}
						
			own_count = 0;

			if (!page_cur_is_after_last(&cur)) {
				slot_no++;
				slot = page_dir_get_nth_slot(page, slot_no);
			}
		}

		if (page_cur_is_after_last(&cur)) {

			break;
		}

		if (rec_get_next_offs(rec, comp) < FIL_PAGE_DATA
			|| rec_get_next_offs(rec, comp) >= UNIV_PAGE_SIZE) {
			fprintf(stderr,
		"InnoDB: Next record offset nonsensical %lu for rec %lu\n",
			  (ulong) rec_get_next_offs(rec, comp),
			  (ulong)(rec - page));

			goto func_exit;
		}

		count++;		

		if (count > UNIV_PAGE_SIZE) {
			fprintf(stderr,
		"InnoDB: Page record list appears to be circular %lu\n",
								(ulong) count);
			goto func_exit;
		}
		
		page_cur_move_to_next(&cur);
		own_count++;
	}
	
	if (rec_get_n_owned(rec, comp) == 0) {
		fprintf(stderr, "InnoDB: n owned is zero in a supremum rec\n");

		goto func_exit;
	}
		
	if (slot_no != n_slots - 1) {
		fprintf(stderr, "InnoDB: n slots wrong %lu, %lu\n",
			(ulong) slot_no, (ulong) (n_slots - 1));
		goto func_exit;
	}		

	if (page_header_get_field(page, PAGE_N_RECS) + 2 != count + 1) {
		fprintf(stderr, "InnoDB: n recs wrong %lu %lu\n",
		(ulong) page_header_get_field(page, PAGE_N_RECS) + 2,
		(ulong) (count + 1));

		goto func_exit;
	}

	/* Check then the free list */
	rec = page_header_get_ptr(page, PAGE_FREE);

	while (rec != NULL) {
		if (rec < page + FIL_PAGE_DATA
				|| rec >= page + UNIV_PAGE_SIZE) {
			fprintf(stderr,
		"InnoDB: Free list record has a nonsensical offset %lu\n",
			(ulong)(rec - page));

			goto func_exit;
		}

		if (rec > rec_heap_top) {
			fprintf(stderr,
		"InnoDB: Free list record %lu is above rec heap top %lu\n",
			(ulong)(rec - page), (ulong)(rec_heap_top - page));

			goto func_exit;
		}

		count++;
		
		if (count > UNIV_PAGE_SIZE) {
			fprintf(stderr,
		"InnoDB: Page free list appears to be circular %lu\n",
							    (ulong) count);
			goto func_exit;
		}

		rec = page_rec_get_next(rec);
	}
	
	if (page_dir_get_n_heap(page) != count + 1) {

		fprintf(stderr, "InnoDB: N heap is wrong %lu, %lu\n",
		(ulong) page_dir_get_n_heap(page),
		(ulong) (count + 1));

		goto func_exit;
	}

	ret = TRUE;	

func_exit:
	return(ret);			  
}

/*******************************************************************
This function checks the consistency of an index page. */

ibool
page_validate(
/*==========*/
				/* out: TRUE if ok */
	page_t*		page,	/* in: index page */
	dict_index_t*	index)	/* in: data dictionary index containing
				the page record type definition */
{
	page_dir_slot_t* slot;
	mem_heap_t*	heap;
	page_cur_t 	cur;
	byte*		buf;
	ulint		count;
	ulint		own_count;
	ulint		slot_no;
	ulint		data_size;
	rec_t*		rec;
	rec_t*		old_rec		= NULL;
	ulint		offs;
	ulint		n_slots;
	ibool		ret		= FALSE;
	ulint		i;
	ulint		comp		= page_is_comp(page);
	ulint*		offsets		= NULL;
	ulint*		old_offsets	= NULL;

	if ((ibool)!!comp != index->table->comp) {
		fputs("InnoDB: 'compact format' flag mismatch\n", stderr);
		goto func_exit2;
	}
	if (!page_simple_validate(page)) {
		goto func_exit2;
	}

	heap = mem_heap_create(UNIV_PAGE_SIZE + 200);
	
	/* The following buffer is used to check that the
	records in the page record heap do not overlap */

	buf = mem_heap_alloc(heap, UNIV_PAGE_SIZE);
	memset(buf, 0, UNIV_PAGE_SIZE);

	/* Check first that the record heap and the directory do not
	overlap. */

	n_slots = page_dir_get_n_slots(page);

	if (!(page_header_get_ptr(page, PAGE_HEAP_TOP) <=
			page_dir_get_nth_slot(page, n_slots - 1))) {

		fputs("InnoDB: Record heap and dir overlap on a page ",
			stderr);
		dict_index_name_print(stderr, NULL, index);
		fprintf(stderr, ", %p, %p\n",
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

	page_cur_set_before_first(page, &cur);

	for (;;) {
		rec = cur.rec;
		offsets = rec_get_offsets(rec, index, offsets,
						ULINT_UNDEFINED, &heap);

		if (comp && page_rec_is_user_rec(rec)
				&& rec_get_node_ptr_flag(rec)
				!= (ibool)
				(btr_page_get_level_low(page) != 0)) {
			fputs("InnoDB: node_ptr flag mismatch\n", stderr);
			goto func_exit;
		}

		if (!page_rec_validate(rec, offsets)) {
			goto func_exit;
		}
		
		/* Check that the records are in the ascending order */
		if ((count >= 2) && (!page_cur_is_after_last(&cur))) {
			if (!(1 == cmp_rec_rec(rec, old_rec,
					offsets, old_offsets, index))) {
				fprintf(stderr,
				"InnoDB: Records in wrong order on page %lu",
					(ulong) buf_frame_get_page_no(page));
				dict_index_name_print(stderr, NULL, index);
				fputs("\nInnoDB: previous record ", stderr);
				rec_print_new(stderr, old_rec, old_offsets);
				fputs("\nInnoDB: record ", stderr);
				rec_print_new(stderr, rec, offsets);
				putc('\n', stderr);
				
				goto func_exit;
			}
		}

		if (page_rec_is_user_rec(rec)) {

			data_size += rec_offs_size(offsets);
		}
		
		offs = rec_get_start(rec, offsets) - page;
		
		for (i = 0; i < rec_offs_size(offsets); i++) {
			if (!buf[offs + i] == 0) {
				/* No other record may overlap this */

				fputs("InnoDB: Record overlaps another\n",
					stderr);
				goto func_exit;
			}
				
			buf[offs + i] = 1;
		}
		
		if (rec_get_n_owned(rec, comp) != 0) {
			/* This is a record pointed to by a dir slot */
			if (rec_get_n_owned(rec, comp) != own_count) {
				fprintf(stderr,
			"InnoDB: Wrong owned count %lu, %lu\n",
				(ulong) rec_get_n_owned(rec, comp),
				(ulong) own_count);
				goto func_exit;
			}

			if (page_dir_slot_get_rec(slot) != rec) {
				fputs(
			"InnoDB: Dir slot does not point to right rec\n",
					stderr);
				goto func_exit;
			}
			
			page_dir_slot_check(slot);
			
			own_count = 0;
			if (!page_cur_is_after_last(&cur)) {
				slot_no++;
				slot = page_dir_get_nth_slot(page, slot_no);
			}
		}

		if (page_cur_is_after_last(&cur)) {
			break;
		}

		if (rec_get_next_offs(rec, comp) < FIL_PAGE_DATA
			|| rec_get_next_offs(rec, comp) >= UNIV_PAGE_SIZE) {
			fprintf(stderr,
				"InnoDB: Next record offset wrong %lu\n",
				(ulong) rec_get_next_offs(rec, comp));
			goto func_exit;
		}

		count++;		
		page_cur_move_to_next(&cur);
		own_count++;
		old_rec = rec;
		/* set old_offsets to offsets; recycle offsets */
		{
			ulint* offs = old_offsets;
			old_offsets = offsets;
			offsets = offs;
		}
	}
	
	if (rec_get_n_owned(rec, comp) == 0) {
		fputs("InnoDB: n owned is zero\n", stderr);
		goto func_exit;
	}
		
	if (slot_no != n_slots - 1) {
		fprintf(stderr, "InnoDB: n slots wrong %lu %lu\n",
			(ulong) slot_no, (ulong) (n_slots - 1));
		goto func_exit;
	}		

	if (page_header_get_field(page, PAGE_N_RECS) + 2 != count + 1) {
		fprintf(stderr, "InnoDB: n recs wrong %lu %lu\n",
		(ulong) page_header_get_field(page, PAGE_N_RECS) + 2,
		(ulong) (count + 1));
		goto func_exit;
	}

	if (data_size != page_get_data_size(page)) {
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
		if (!page_rec_validate(rec, offsets)) {

			goto func_exit;
		}
		
		count++;	
		offs = rec_get_start(rec, offsets) - page;
		
		for (i = 0; i < rec_offs_size(offsets); i++) {

			if (buf[offs + i] != 0) {
				fputs(
		"InnoDB: Record overlaps another in free list\n", stderr);
				goto func_exit;
			}
				
			buf[offs + i] = 1;
		}
		
		rec = page_rec_get_next(rec);
	}
	
	if (page_dir_get_n_heap(page) != count + 1) {
		fprintf(stderr, "InnoDB: N heap is wrong %lu %lu\n",
			(ulong) page_dir_get_n_heap(page),
			(ulong) count + 1);
		goto func_exit;
	}

	ret = TRUE;	

func_exit:
	mem_heap_free(heap);

	if (ret == FALSE) {
	func_exit2:
		fprintf(stderr, "InnoDB: Apparent corruption in page %lu in ",
			(ulong) buf_frame_get_page_no(page));
		dict_index_name_print(stderr, NULL, index);
		putc('\n', stderr);
		buf_page_print(page);
	}
	
	return(ret);			  
}

/*******************************************************************
Looks in the page record list for a record with the given heap number. */

rec_t*
page_find_rec_with_heap_no(
/*=======================*/
			/* out: record, NULL if not found */
	page_t*	page,	/* in: index page */
	ulint	heap_no)/* in: heap number */
{
	page_cur_t	cur;

	page_cur_set_before_first(page, &cur);

	for (;;) {
		if (rec_get_heap_no(cur.rec, page_is_comp(page)) == heap_no) {

			return(cur.rec);
		}

		if (page_cur_is_after_last(&cur)) {

			return(NULL);
		}	

		page_cur_move_to_next(&cur);
	}
}
