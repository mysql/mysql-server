/*****************************************************************************

Copyright (c) 1997, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file ibuf/ibuf0ibuf.c
Insert buffer

Created 7/19/1997 Heikki Tuuri
*******************************************************/

#include "ibuf0ibuf.h"

/** Number of bits describing a single page */
#define IBUF_BITS_PER_PAGE	4
#if IBUF_BITS_PER_PAGE % 2
# error "IBUF_BITS_PER_PAGE must be an even number!"
#endif
/** The start address for an insert buffer bitmap page bitmap */
#define IBUF_BITMAP		PAGE_DATA

#ifdef UNIV_NONINL
#include "ibuf0ibuf.ic"
#endif

#ifndef UNIV_HOTBACKUP

#include "buf0buf.h"
#include "buf0rea.h"
#include "fsp0fsp.h"
#include "trx0sys.h"
#include "fil0fil.h"
#include "thr0loc.h"
#include "rem0rec.h"
#include "btr0cur.h"
#include "btr0pcur.h"
#include "btr0btr.h"
#include "row0upd.h"
#include "sync0sync.h"
#include "dict0boot.h"
#include "fut0lst.h"
#include "lock0lock.h"
#include "log0recv.h"
#include "que0que.h"

/*	STRUCTURE OF AN INSERT BUFFER RECORD

In versions < 4.1.x:

1. The first field is the page number.
2. The second field is an array which stores type info for each subsequent
   field. We store the information which affects the ordering of records, and
   also the physical storage size of an SQL NULL value. E.g., for CHAR(10) it
   is 10 bytes.
3. Next we have the fields of the actual index record.

In versions >= 4.1.x:

Note that contary to what we planned in the 1990's, there will only be one
insert buffer tree, and that is in the system tablespace of InnoDB.

1. The first field is the space id.
2. The second field is a one-byte marker (0) which differentiates records from
   the < 4.1.x storage format.
3. The third field is the page number.
4. The fourth field contains the type info, where we have also added 2 bytes to
   store the charset. In the compressed table format of 5.0.x we must add more
   information here so that we can build a dummy 'index' struct which 5.0.x
   can use in the binary search on the index page in the ibuf merge phase.
5. The rest of the fields contain the fields of the actual index record.

In versions >= 5.0.3:

The first byte of the fourth field is an additional marker (0) if the record
is in the compact format.  The presence of this marker can be detected by
looking at the length of the field modulo DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE.

The high-order bit of the character set field in the type info is the
"nullable" flag for the field. */


/*	PREVENTING DEADLOCKS IN THE INSERT BUFFER SYSTEM

If an OS thread performs any operation that brings in disk pages from
non-system tablespaces into the buffer pool, or creates such a page there,
then the operation may have as a side effect an insert buffer index tree
compression. Thus, the tree latch of the insert buffer tree may be acquired
in the x-mode, and also the file space latch of the system tablespace may
be acquired in the x-mode.

Also, an insert to an index in a non-system tablespace can have the same
effect. How do we know this cannot lead to a deadlock of OS threads? There
is a problem with the i\o-handler threads: they break the latching order
because they own x-latches to pages which are on a lower level than the
insert buffer tree latch, its page latches, and the tablespace latch an
insert buffer operation can reserve.

The solution is the following: Let all the tree and page latches connected
with the insert buffer be later in the latching order than the fsp latch and
fsp page latches.

Insert buffer pages must be such that the insert buffer is never invoked
when these pages are accessed as this would result in a recursion violating
the latching order. We let a special i/o-handler thread take care of i/o to
the insert buffer pages and the ibuf bitmap pages, as well as the fsp bitmap
pages and the first inode page, which contains the inode of the ibuf tree: let
us call all these ibuf pages. To prevent deadlocks, we do not let a read-ahead
access both non-ibuf and ibuf pages.

Then an i/o-handler for the insert buffer never needs to access recursively the
insert buffer tree and thus obeys the latching order. On the other hand, other
i/o-handlers for other tablespaces may require access to the insert buffer,
but because all kinds of latches they need to access there are later in the
latching order, no violation of the latching order occurs in this case,
either.

A problem is how to grow and contract an insert buffer tree. As it is later
in the latching order than the fsp management, we have to reserve the fsp
latch first, before adding or removing pages from the insert buffer tree.
We let the insert buffer tree have its own file space management: a free
list of pages linked to the tree root. To prevent recursive using of the
insert buffer when adding pages to the tree, we must first load these pages
to memory, obtaining a latch on them, and only after that add them to the
free list of the insert buffer tree. More difficult is removing of pages
from the free list. If there is an excess of pages in the free list of the
ibuf tree, they might be needed if some thread reserves the fsp latch,
intending to allocate more file space. So we do the following: if a thread
reserves the fsp latch, we check the writer count field of the latch. If
this field has value 1, it means that the thread did not own the latch
before entering the fsp system, and the mtr of the thread contains no
modifications to the fsp pages. Now we are free to reserve the ibuf latch,
and check if there is an excess of pages in the free list. We can then, in a
separate mini-transaction, take them out of the free list and free them to
the fsp system.

To avoid deadlocks in the ibuf system, we divide file pages into three levels:

(1) non-ibuf pages,
(2) ibuf tree pages and the pages in the ibuf tree free list, and
(3) ibuf bitmap pages.

No OS thread is allowed to access higher level pages if it has latches to
lower level pages; even if the thread owns a B-tree latch it must not access
the B-tree non-leaf pages if it has latches on lower level pages. Read-ahead
is only allowed for level 1 and 2 pages. Dedicated i/o-handler threads handle
exclusively level 1 i/o. A dedicated i/o handler thread handles exclusively
level 2 i/o. However, if an OS thread does the i/o handling for itself, i.e.,
it uses synchronous aio, it can access any pages, as long as it obeys the
access order rules. */

/** Buffer pool size per the maximum insert buffer size */
#define IBUF_POOL_SIZE_PER_MAX_SIZE	2

/** Table name for the insert buffer. */
#define IBUF_TABLE_NAME		"SYS_IBUF_TABLE"

/** Operations that can currently be buffered. */
UNIV_INTERN ibuf_use_t	ibuf_use		= IBUF_USE_INSERT;

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Flag to control insert buffer debugging. */
UNIV_INTERN uint	ibuf_debug;
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/** The insert buffer control structure */
UNIV_INTERN ibuf_t*	ibuf			= NULL;

/** Counter for ibuf_should_try() */
UNIV_INTERN ulint	ibuf_flush_count	= 0;

#ifdef UNIV_IBUF_COUNT_DEBUG
/** Number of tablespaces in the ibuf_counts array */
#define IBUF_COUNT_N_SPACES	4
/** Number of pages within each tablespace in the ibuf_counts array */
#define IBUF_COUNT_N_PAGES	130000

/** Buffered entry counts for file pages, used in debugging */
static ulint	ibuf_counts[IBUF_COUNT_N_SPACES][IBUF_COUNT_N_PAGES];

/******************************************************************//**
Checks that the indexes to ibuf_counts[][] are within limits. */
UNIV_INLINE
void
ibuf_count_check(
/*=============*/
	ulint	space_id,	/*!< in: space identifier */
	ulint	page_no)	/*!< in: page number */
{
	if (space_id < IBUF_COUNT_N_SPACES && page_no < IBUF_COUNT_N_PAGES) {
		return;
	}

	fprintf(stderr,
		"InnoDB: UNIV_IBUF_COUNT_DEBUG limits space_id and page_no\n"
		"InnoDB: and breaks crash recovery.\n"
		"InnoDB: space_id=%lu, should be 0<=space_id<%lu\n"
		"InnoDB: page_no=%lu, should be 0<=page_no<%lu\n",
		(ulint) space_id, (ulint) IBUF_COUNT_N_SPACES,
		(ulint) page_no, (ulint) IBUF_COUNT_N_PAGES);
	ut_error;
}
#endif

/** @name Offsets to the per-page bits in the insert buffer bitmap */
/* @{ */
#define	IBUF_BITMAP_FREE	0	/*!< Bits indicating the
					amount of free space */
#define IBUF_BITMAP_BUFFERED	2	/*!< TRUE if there are buffered
					changes for the page */
#define IBUF_BITMAP_IBUF	3	/*!< TRUE if page is a part of
					the ibuf tree, excluding the
					root page, or is in the free
					list of the ibuf */
/* @} */

/** The mutex used to block pessimistic inserts to ibuf trees */
static mutex_t	ibuf_pessimistic_insert_mutex;

/** The mutex protecting the insert buffer structs */
static mutex_t	ibuf_mutex;

/** The mutex protecting the insert buffer bitmaps */
static mutex_t	ibuf_bitmap_mutex;

/** The area in pages from which contract looks for page numbers for merge */
#define	IBUF_MERGE_AREA			8

/** Inside the merge area, pages which have at most 1 per this number less
buffered entries compared to maximum volume that can buffered for a single
page are merged along with the page whose buffer became full */
#define IBUF_MERGE_THRESHOLD		4

/** In ibuf_contract at most this number of pages is read to memory in one
batch, in order to merge the entries for them in the insert buffer */
#define	IBUF_MAX_N_PAGES_MERGED		IBUF_MERGE_AREA

/** If the combined size of the ibuf trees exceeds ibuf->max_size by this
many pages, we start to contract it in connection to inserts there, using
non-synchronous contract */
#define IBUF_CONTRACT_ON_INSERT_NON_SYNC	0

/** If the combined size of the ibuf trees exceeds ibuf->max_size by this
many pages, we start to contract it in connection to inserts there, using
synchronous contract */
#define IBUF_CONTRACT_ON_INSERT_SYNC		5

/** If the combined size of the ibuf trees exceeds ibuf->max_size by
this many pages, we start to contract it synchronous contract, but do
not insert */
#define IBUF_CONTRACT_DO_NOT_INSERT		10

/* TODO: how to cope with drop table if there are records in the insert
buffer for the indexes of the table? Is there actually any problem,
because ibuf merge is done to a page when it is read in, and it is
still physically like the index page even if the index would have been
dropped! So, there seems to be no problem. */

/******************************************************************//**
Sets the flag in the current OS thread local storage denoting that it is
inside an insert buffer routine. */
UNIV_INLINE
void
ibuf_enter(void)
/*============*/
{
	ibool*	ptr;

	ptr = thr_local_get_in_ibuf_field();

	ut_ad(*ptr == FALSE);

	*ptr = TRUE;
}

/******************************************************************//**
Sets the flag in the current OS thread local storage denoting that it is
exiting an insert buffer routine. */
UNIV_INLINE
void
ibuf_exit(void)
/*===========*/
{
	ibool*	ptr;

	ptr = thr_local_get_in_ibuf_field();

	ut_ad(*ptr == TRUE);

	*ptr = FALSE;
}

/******************************************************************//**
Returns TRUE if the current OS thread is performing an insert buffer
routine.

For instance, a read-ahead of non-ibuf pages is forbidden by threads
that are executing an insert buffer routine.
@return TRUE if inside an insert buffer routine */
UNIV_INTERN
ibool
ibuf_inside(void)
/*=============*/
{
	return(*thr_local_get_in_ibuf_field());
}

/******************************************************************//**
Gets the ibuf header page and x-latches it.
@return	insert buffer header page */
static
page_t*
ibuf_header_page_get(
/*=================*/
	mtr_t*	mtr)	/*!< in: mtr */
{
	buf_block_t*	block;

	ut_ad(!ibuf_inside());

	block = buf_page_get(
		IBUF_SPACE_ID, 0, FSP_IBUF_HEADER_PAGE_NO, RW_X_LATCH, mtr);
	buf_block_dbg_add_level(block, SYNC_IBUF_HEADER);

	return(buf_block_get_frame(block));
}

/******************************************************************//**
Gets the root page and x-latches it.
@return	insert buffer tree root page */
static
page_t*
ibuf_tree_root_get(
/*===============*/
	mtr_t*		mtr)	/*!< in: mtr */
{
	buf_block_t*	block;

	ut_ad(ibuf_inside());

	mtr_x_lock(dict_index_get_lock(ibuf->index), mtr);

	block = buf_page_get(
		IBUF_SPACE_ID, 0, FSP_IBUF_TREE_ROOT_PAGE_NO, RW_X_LATCH, mtr);

	buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);

	return(buf_block_get_frame(block));
}

#ifdef UNIV_IBUF_COUNT_DEBUG
/******************************************************************//**
Gets the ibuf count for a given page.
@return number of entries in the insert buffer currently buffered for
this page */
UNIV_INTERN
ulint
ibuf_count_get(
/*===========*/
	ulint	space,	/*!< in: space id */
	ulint	page_no)/*!< in: page number */
{
	ibuf_count_check(space, page_no);

	return(ibuf_counts[space][page_no]);
}

/******************************************************************//**
Sets the ibuf count for a given page. */
static
void
ibuf_count_set(
/*===========*/
	ulint	space,	/*!< in: space id */
	ulint	page_no,/*!< in: page number */
	ulint	val)	/*!< in: value to set */
{
	ibuf_count_check(space, page_no);
	ut_a(val < UNIV_PAGE_SIZE);

	ibuf_counts[space][page_no] = val;
}
#endif

/******************************************************************//**
Closes insert buffer and frees the data structures. */
UNIV_INTERN
void
ibuf_close(void)
/*============*/
{
	mutex_free(&ibuf_pessimistic_insert_mutex);
	memset(&ibuf_pessimistic_insert_mutex,
	       0x0, sizeof(ibuf_pessimistic_insert_mutex));

	mutex_free(&ibuf_mutex);
	memset(&ibuf_mutex, 0x0, sizeof(ibuf_mutex));

	mutex_free(&ibuf_bitmap_mutex);
	memset(&ibuf_bitmap_mutex, 0x0, sizeof(ibuf_mutex));

	mem_free(ibuf);
	ibuf = NULL;
}

/******************************************************************//**
Updates the size information of the ibuf, assuming the segment size has not
changed. */
static
void
ibuf_size_update(
/*=============*/
	const page_t*	root,	/*!< in: ibuf tree root */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ut_ad(mutex_own(&ibuf_mutex));

	ibuf->free_list_len = flst_get_len(root + PAGE_HEADER
					   + PAGE_BTR_IBUF_FREE_LIST, mtr);

	ibuf->height = 1 + btr_page_get_level(root, mtr);

	/* the '1 +' is the ibuf header page */
	ibuf->size = ibuf->seg_size - (1 + ibuf->free_list_len);

	ibuf->empty = page_get_n_recs(root) == 0;
}

/******************************************************************//**
Creates the insert buffer data structure at a database startup and initializes
the data structures for the insert buffer. */
UNIV_INTERN
void
ibuf_init_at_db_start(void)
/*=======================*/
{
	page_t*		root;
	mtr_t		mtr;
	dict_table_t*	table;
	mem_heap_t*	heap;
	dict_index_t*	index;
	ulint		n_used;
	page_t*		header_page;
	ulint		error;

	ibuf = mem_alloc(sizeof(ibuf_t));

	memset(ibuf, 0, sizeof(*ibuf));

	/* Note that also a pessimistic delete can sometimes make a B-tree
	grow in size, as the references on the upper levels of the tree can
	change */

	ibuf->max_size = buf_pool_get_curr_size() / UNIV_PAGE_SIZE
		/ IBUF_POOL_SIZE_PER_MAX_SIZE;

	mutex_create(&ibuf_pessimistic_insert_mutex,
		     SYNC_IBUF_PESS_INSERT_MUTEX);

	mutex_create(&ibuf_mutex, SYNC_IBUF_MUTEX);

	mutex_create(&ibuf_bitmap_mutex, SYNC_IBUF_BITMAP_MUTEX);

	mtr_start(&mtr);

	mutex_enter(&ibuf_mutex);

	mtr_x_lock(fil_space_get_latch(IBUF_SPACE_ID, NULL), &mtr);

	header_page = ibuf_header_page_get(&mtr);

	fseg_n_reserved_pages(header_page + IBUF_HEADER + IBUF_TREE_SEG_HEADER,
			      &n_used, &mtr);
	ibuf_enter();

	ut_ad(n_used >= 2);

	ibuf->seg_size = n_used;

	{
		buf_block_t*	block;

		block = buf_page_get(
			IBUF_SPACE_ID, 0, FSP_IBUF_TREE_ROOT_PAGE_NO,
			RW_X_LATCH, &mtr);
		buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);

		root = buf_block_get_frame(block);
	}

	ibuf_size_update(root, &mtr);
	mutex_exit(&ibuf_mutex);

	mtr_commit(&mtr);

	ibuf_exit();

	heap = mem_heap_create(450);

	/* Use old-style record format for the insert buffer. */
	table = dict_mem_table_create(IBUF_TABLE_NAME, IBUF_SPACE_ID, 1, 0);

	dict_mem_table_add_col(table, heap, "DUMMY_COLUMN", DATA_BINARY, 0, 0);

	table->id = ut_dulint_add(DICT_IBUF_ID_MIN, IBUF_SPACE_ID);

	dict_table_add_to_cache(table, heap);
	mem_heap_free(heap);

	index = dict_mem_index_create(
		IBUF_TABLE_NAME, "CLUST_IND",
		IBUF_SPACE_ID, DICT_CLUSTERED | DICT_UNIVERSAL | DICT_IBUF, 1);

	dict_mem_index_add_field(index, "DUMMY_COLUMN", 0);

	index->id = ut_dulint_add(DICT_IBUF_ID_MIN, IBUF_SPACE_ID);

	error = dict_index_add_to_cache(table, index,
					FSP_IBUF_TREE_ROOT_PAGE_NO, FALSE);
	ut_a(error == DB_SUCCESS);

	ibuf->index = dict_table_get_first_index(table);
}
#endif /* !UNIV_HOTBACKUP */
/*********************************************************************//**
Initializes an ibuf bitmap page. */
UNIV_INTERN
void
ibuf_bitmap_page_init(
/*==================*/
	buf_block_t*	block,	/*!< in: bitmap page */
	mtr_t*		mtr)	/*!< in: mtr */
{
	page_t*	page;
	ulint	byte_offset;
	ulint	zip_size = buf_block_get_zip_size(block);

	ut_a(ut_is_2pow(zip_size));

	page = buf_block_get_frame(block);
	fil_page_set_type(page, FIL_PAGE_IBUF_BITMAP);

	/* Write all zeros to the bitmap */

	if (!zip_size) {
		byte_offset = UT_BITS_IN_BYTES(UNIV_PAGE_SIZE
					       * IBUF_BITS_PER_PAGE);
	} else {
		byte_offset = UT_BITS_IN_BYTES(zip_size * IBUF_BITS_PER_PAGE);
	}

	memset(page + IBUF_BITMAP, 0, byte_offset);

	/* The remaining area (up to the page trailer) is uninitialized. */

#ifndef UNIV_HOTBACKUP
	mlog_write_initial_log_record(page, MLOG_IBUF_BITMAP_INIT, mtr);
#endif /* !UNIV_HOTBACKUP */
}

/*********************************************************************//**
Parses a redo log record of an ibuf bitmap page init.
@return	end of log record or NULL */
UNIV_INTERN
byte*
ibuf_parse_bitmap_init(
/*===================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr __attribute__((unused)), /*!< in: buffer end */
	buf_block_t*	block,	/*!< in: block or NULL */
	mtr_t*		mtr)	/*!< in: mtr or NULL */
{
	ut_ad(ptr && end_ptr);

	if (block) {
		ibuf_bitmap_page_init(block, mtr);
	}

	return(ptr);
}
#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Gets the desired bits for a given page from a bitmap page.
@return	value of bits */
UNIV_INLINE
ulint
ibuf_bitmap_page_get_bits(
/*======================*/
	const page_t*	page,	/*!< in: bitmap page */
	ulint		page_no,/*!< in: page whose bits to get */
	ulint		zip_size,/*!< in: compressed page size in bytes;
				0 for uncompressed pages */
	ulint		bit,	/*!< in: IBUF_BITMAP_FREE,
				IBUF_BITMAP_BUFFERED, ... */
	mtr_t*		mtr __attribute__((unused)))
				/*!< in: mtr containing an
				x-latch to the bitmap page */
{
	ulint	byte_offset;
	ulint	bit_offset;
	ulint	map_byte;
	ulint	value;

	ut_ad(bit < IBUF_BITS_PER_PAGE);
#if IBUF_BITS_PER_PAGE % 2
# error "IBUF_BITS_PER_PAGE % 2 != 0"
#endif
	ut_ad(ut_is_2pow(zip_size));
	ut_ad(mtr_memo_contains_page(mtr, page, MTR_MEMO_PAGE_X_FIX));

	if (!zip_size) {
		bit_offset = (page_no % UNIV_PAGE_SIZE) * IBUF_BITS_PER_PAGE
			+ bit;
	} else {
		bit_offset = (page_no & (zip_size - 1)) * IBUF_BITS_PER_PAGE
			+ bit;
	}

	byte_offset = bit_offset / 8;
	bit_offset = bit_offset % 8;

	ut_ad(byte_offset + IBUF_BITMAP < UNIV_PAGE_SIZE);

	map_byte = mach_read_from_1(page + IBUF_BITMAP + byte_offset);

	value = ut_bit_get_nth(map_byte, bit_offset);

	if (bit == IBUF_BITMAP_FREE) {
		ut_ad(bit_offset + 1 < 8);

		value = value * 2 + ut_bit_get_nth(map_byte, bit_offset + 1);
	}

	return(value);
}

/********************************************************************//**
Sets the desired bit for a given page in a bitmap page. */
static
void
ibuf_bitmap_page_set_bits(
/*======================*/
	page_t*	page,	/*!< in: bitmap page */
	ulint	page_no,/*!< in: page whose bits to set */
	ulint	zip_size,/*!< in: compressed page size in bytes;
			0 for uncompressed pages */
	ulint	bit,	/*!< in: IBUF_BITMAP_FREE, IBUF_BITMAP_BUFFERED, ... */
	ulint	val,	/*!< in: value to set */
	mtr_t*	mtr)	/*!< in: mtr containing an x-latch to the bitmap page */
{
	ulint	byte_offset;
	ulint	bit_offset;
	ulint	map_byte;

	ut_ad(bit < IBUF_BITS_PER_PAGE);
#if IBUF_BITS_PER_PAGE % 2
# error "IBUF_BITS_PER_PAGE % 2 != 0"
#endif
	ut_ad(ut_is_2pow(zip_size));
	ut_ad(mtr_memo_contains_page(mtr, page, MTR_MEMO_PAGE_X_FIX));
#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a((bit != IBUF_BITMAP_BUFFERED) || (val != FALSE)
	     || (0 == ibuf_count_get(page_get_space_id(page),
				     page_no)));
#endif
	if (!zip_size) {
		bit_offset = (page_no % UNIV_PAGE_SIZE) * IBUF_BITS_PER_PAGE
			+ bit;
	} else {
		bit_offset = (page_no & (zip_size - 1)) * IBUF_BITS_PER_PAGE
			+ bit;
	}

	byte_offset = bit_offset / 8;
	bit_offset = bit_offset % 8;

	ut_ad(byte_offset + IBUF_BITMAP < UNIV_PAGE_SIZE);

	map_byte = mach_read_from_1(page + IBUF_BITMAP + byte_offset);

	if (bit == IBUF_BITMAP_FREE) {
		ut_ad(bit_offset + 1 < 8);
		ut_ad(val <= 3);

		map_byte = ut_bit_set_nth(map_byte, bit_offset, val / 2);
		map_byte = ut_bit_set_nth(map_byte, bit_offset + 1, val % 2);
	} else {
		ut_ad(val <= 1);
		map_byte = ut_bit_set_nth(map_byte, bit_offset, val);
	}

	mlog_write_ulint(page + IBUF_BITMAP + byte_offset, map_byte,
			 MLOG_1BYTE, mtr);
}

/********************************************************************//**
Calculates the bitmap page number for a given page number.
@return	the bitmap page number where the file page is mapped */
UNIV_INLINE
ulint
ibuf_bitmap_page_no_calc(
/*=====================*/
	ulint	zip_size,	/*!< in: compressed page size in bytes;
				0 for uncompressed pages */
	ulint	page_no)	/*!< in: tablespace page number */
{
	ut_ad(ut_is_2pow(zip_size));

	if (!zip_size) {
		return(FSP_IBUF_BITMAP_OFFSET
		       + (page_no & ~(UNIV_PAGE_SIZE - 1)));
	} else {
		return(FSP_IBUF_BITMAP_OFFSET
		       + (page_no & ~(zip_size - 1)));
	}
}

/********************************************************************//**
Gets the ibuf bitmap page where the bits describing a given file page are
stored.
@return bitmap page where the file page is mapped, that is, the bitmap
page containing the descriptor bits for the file page; the bitmap page
is x-latched */
static
page_t*
ibuf_bitmap_get_map_page_func(
/*==========================*/
	ulint		space,	/*!< in: space id of the file page */
	ulint		page_no,/*!< in: page number of the file page */
	ulint		zip_size,/*!< in: compressed page size in bytes;
				0 for uncompressed pages */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mtr */
{
	buf_block_t*	block;

	block = buf_page_get_gen(space, zip_size,
				 ibuf_bitmap_page_no_calc(zip_size, page_no),
				 RW_X_LATCH, NULL, BUF_GET,
				 file, line, mtr);
	buf_block_dbg_add_level(block, SYNC_IBUF_BITMAP);

	return(buf_block_get_frame(block));
}

/********************************************************************//**
Gets the ibuf bitmap page where the bits describing a given file page are
stored.
@return bitmap page where the file page is mapped, that is, the bitmap
page containing the descriptor bits for the file page; the bitmap page
is x-latched
@param space	in: space id of the file page
@param page_no	in: page number of the file page
@param zip_size	in: compressed page size in bytes; 0 for uncompressed pages
@param mtr	in: mini-transaction */
#define ibuf_bitmap_get_map_page(space, page_no, zip_size, mtr)		\
	ibuf_bitmap_get_map_page_func(space, page_no, zip_size,		\
				      __FILE__, __LINE__, mtr)

/************************************************************************//**
Sets the free bits of the page in the ibuf bitmap. This is done in a separate
mini-transaction, hence this operation does not restrict further work to only
ibuf bitmap operations, which would result if the latch to the bitmap page
were kept. */
UNIV_INLINE
void
ibuf_set_free_bits_low(
/*===================*/
	ulint			zip_size,/*!< in: compressed page size in bytes;
					0 for uncompressed pages */
	const buf_block_t*	block,	/*!< in: index page; free bits are set if
					the index is non-clustered and page
					level is 0 */
	ulint			val,	/*!< in: value to set: < 4 */
	mtr_t*			mtr)	/*!< in/out: mtr */
{
	page_t*	bitmap_page;
	ulint	space;
	ulint	page_no;

	if (!page_is_leaf(buf_block_get_frame(block))) {

		return;
	}

	space = buf_block_get_space(block);
	page_no = buf_block_get_page_no(block);
	bitmap_page = ibuf_bitmap_get_map_page(space, page_no, zip_size, mtr);
#ifdef UNIV_IBUF_DEBUG
# if 0
	fprintf(stderr,
		"Setting space %lu page %lu free bits to %lu should be %lu\n",
		space, page_no, val,
		ibuf_index_page_calc_free(zip_size, block));
# endif

	ut_a(val <= ibuf_index_page_calc_free(zip_size, block));
#endif /* UNIV_IBUF_DEBUG */
	ibuf_bitmap_page_set_bits(bitmap_page, page_no, zip_size,
				  IBUF_BITMAP_FREE, val, mtr);
}

/************************************************************************//**
Sets the free bit of the page in the ibuf bitmap. This is done in a separate
mini-transaction, hence this operation does not restrict further work to only
ibuf bitmap operations, which would result if the latch to the bitmap page
were kept. */
UNIV_INTERN
void
ibuf_set_free_bits_func(
/*====================*/
	buf_block_t*	block,	/*!< in: index page of a non-clustered index;
				free bit is reset if page level is 0 */
#ifdef UNIV_IBUF_DEBUG
	ulint		max_val,/*!< in: ULINT_UNDEFINED or a maximum
				value which the bits must have before
				setting; this is for debugging */
#endif /* UNIV_IBUF_DEBUG */
	ulint		val)	/*!< in: value to set: < 4 */
{
	mtr_t	mtr;
	page_t*	page;
	page_t*	bitmap_page;
	ulint	space;
	ulint	page_no;
	ulint	zip_size;

	page = buf_block_get_frame(block);

	if (!page_is_leaf(page)) {

		return;
	}

	mtr_start(&mtr);

	space = buf_block_get_space(block);
	page_no = buf_block_get_page_no(block);
	zip_size = buf_block_get_zip_size(block);
	bitmap_page = ibuf_bitmap_get_map_page(space, page_no, zip_size, &mtr);

#ifdef UNIV_IBUF_DEBUG
	if (max_val != ULINT_UNDEFINED) {
		ulint	old_val;

		old_val = ibuf_bitmap_page_get_bits(
			bitmap_page, page_no, zip_size,
			IBUF_BITMAP_FREE, &mtr);
# if 0
		if (old_val != max_val) {
			fprintf(stderr,
				"Ibuf: page %lu old val %lu max val %lu\n",
				page_get_page_no(page),
				old_val, max_val);
		}
# endif

		ut_a(old_val <= max_val);
	}
# if 0
	fprintf(stderr, "Setting page no %lu free bits to %lu should be %lu\n",
		page_get_page_no(page), val,
		ibuf_index_page_calc_free(zip_size, block));
# endif

	ut_a(val <= ibuf_index_page_calc_free(zip_size, block));
#endif /* UNIV_IBUF_DEBUG */
	ibuf_bitmap_page_set_bits(bitmap_page, page_no, zip_size,
				  IBUF_BITMAP_FREE, val, &mtr);
	mtr_commit(&mtr);
}

/************************************************************************//**
Resets the free bits of the page in the ibuf bitmap. This is done in a
separate mini-transaction, hence this operation does not restrict
further work to only ibuf bitmap operations, which would result if the
latch to the bitmap page were kept.  NOTE: The free bits in the insert
buffer bitmap must never exceed the free space on a page.  It is safe
to decrement or reset the bits in the bitmap in a mini-transaction
that is committed before the mini-transaction that affects the free
space. */
UNIV_INTERN
void
ibuf_reset_free_bits(
/*=================*/
	buf_block_t*	block)	/*!< in: index page; free bits are set to 0
				if the index is a non-clustered
				non-unique, and page level is 0 */
{
	ibuf_set_free_bits(block, 0, ULINT_UNDEFINED);
}

/**********************************************************************//**
Updates the free bits for an uncompressed page to reflect the present
state.  Does this in the mtr given, which means that the latching
order rules virtually prevent any further operations for this OS
thread until mtr is committed.  NOTE: The free bits in the insert
buffer bitmap must never exceed the free space on a page.  It is safe
to set the free bits in the same mini-transaction that updated the
page. */
UNIV_INTERN
void
ibuf_update_free_bits_low(
/*======================*/
	const buf_block_t*	block,		/*!< in: index page */
	ulint			max_ins_size,	/*!< in: value of
						maximum insert size
						with reorganize before
						the latest operation
						performed to the page */
	mtr_t*			mtr)		/*!< in/out: mtr */
{
	ulint	before;
	ulint	after;

	ut_a(!buf_block_get_page_zip(block));

	before = ibuf_index_page_calc_free_bits(0, max_ins_size);

	after = ibuf_index_page_calc_free(0, block);

	/* This approach cannot be used on compressed pages, since the
	computed value of "before" often does not match the current
	state of the bitmap.  This is because the free space may
	increase or decrease when a compressed page is reorganized. */
	if (before != after) {
		ibuf_set_free_bits_low(0, block, after, mtr);
	}
}

/**********************************************************************//**
Updates the free bits for a compressed page to reflect the present
state.  Does this in the mtr given, which means that the latching
order rules virtually prevent any further operations for this OS
thread until mtr is committed.  NOTE: The free bits in the insert
buffer bitmap must never exceed the free space on a page.  It is safe
to set the free bits in the same mini-transaction that updated the
page. */
UNIV_INTERN
void
ibuf_update_free_bits_zip(
/*======================*/
	buf_block_t*	block,	/*!< in/out: index page */
	mtr_t*		mtr)	/*!< in/out: mtr */
{
	page_t*	bitmap_page;
	ulint	space;
	ulint	page_no;
	ulint	zip_size;
	ulint	after;

	space = buf_block_get_space(block);
	page_no = buf_block_get_page_no(block);
	zip_size = buf_block_get_zip_size(block);

	ut_a(page_is_leaf(buf_block_get_frame(block)));
	ut_a(zip_size);

	bitmap_page = ibuf_bitmap_get_map_page(space, page_no, zip_size, mtr);

	after = ibuf_index_page_calc_free_zip(zip_size, block);

	if (after == 0) {
		/* We move the page to the front of the buffer pool LRU list:
		the purpose of this is to prevent those pages to which we
		cannot make inserts using the insert buffer from slipping
		out of the buffer pool */

		buf_page_make_young(&block->page);
	}

	ibuf_bitmap_page_set_bits(bitmap_page, page_no, zip_size,
				  IBUF_BITMAP_FREE, after, mtr);
}

/**********************************************************************//**
Updates the free bits for the two pages to reflect the present state.
Does this in the mtr given, which means that the latching order rules
virtually prevent any further operations until mtr is committed.
NOTE: The free bits in the insert buffer bitmap must never exceed the
free space on a page.  It is safe to set the free bits in the same
mini-transaction that updated the pages. */
UNIV_INTERN
void
ibuf_update_free_bits_for_two_pages_low(
/*====================================*/
	ulint		zip_size,/*!< in: compressed page size in bytes;
				0 for uncompressed pages */
	buf_block_t*	block1,	/*!< in: index page */
	buf_block_t*	block2,	/*!< in: index page */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ulint	state;

	/* As we have to x-latch two random bitmap pages, we have to acquire
	the bitmap mutex to prevent a deadlock with a similar operation
	performed by another OS thread. */

	mutex_enter(&ibuf_bitmap_mutex);

	state = ibuf_index_page_calc_free(zip_size, block1);

	ibuf_set_free_bits_low(zip_size, block1, state, mtr);

	state = ibuf_index_page_calc_free(zip_size, block2);

	ibuf_set_free_bits_low(zip_size, block2, state, mtr);

	mutex_exit(&ibuf_bitmap_mutex);
}

/**********************************************************************//**
Returns TRUE if the page is one of the fixed address ibuf pages.
@return	TRUE if a fixed address ibuf i/o page */
UNIV_INLINE
ibool
ibuf_fixed_addr_page(
/*=================*/
	ulint	space,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size in bytes;
			0 for uncompressed pages */
	ulint	page_no)/*!< in: page number */
{
	return((space == IBUF_SPACE_ID && page_no == IBUF_TREE_ROOT_PAGE_NO)
	       || ibuf_bitmap_page(zip_size, page_no));
}

/***********************************************************************//**
Checks if a page is a level 2 or 3 page in the ibuf hierarchy of pages.
Must not be called when recv_no_ibuf_operations==TRUE.
@return	TRUE if level 2 or level 3 page */
UNIV_INTERN
ibool
ibuf_page(
/*======*/
	ulint	space,	/*!< in: space id */
	ulint	zip_size,/*!< in: compressed page size in bytes, or 0 */
	ulint	page_no,/*!< in: page number */
	mtr_t*	mtr)	/*!< in: mtr which will contain an x-latch to the
			bitmap page if the page is not one of the fixed
			address ibuf pages, or NULL, in which case a new
			transaction is created. */
{
	ibool	ret;
	mtr_t	local_mtr;
	page_t*	bitmap_page;

	ut_ad(!recv_no_ibuf_operations);

	if (ibuf_fixed_addr_page(space, zip_size, page_no)) {

		return(TRUE);
	} else if (space != IBUF_SPACE_ID) {

		return(FALSE);
	}

	ut_ad(fil_space_get_type(IBUF_SPACE_ID) == FIL_TABLESPACE);

	if (mtr == NULL) {
		mtr = &local_mtr;
		mtr_start(mtr);
	}

	bitmap_page = ibuf_bitmap_get_map_page(space, page_no, zip_size, mtr);

	ret = ibuf_bitmap_page_get_bits(bitmap_page, page_no, zip_size,
					IBUF_BITMAP_IBUF, mtr);

	if (mtr == &local_mtr) {
		mtr_commit(mtr);
	}

	return(ret);
}

/********************************************************************//**
Returns the page number field of an ibuf record.
@return	page number */
static
ulint
ibuf_rec_get_page_no(
/*=================*/
	const rec_t*	rec)	/*!< in: ibuf record */
{
	const byte*	field;
	ulint		len;

	ut_ad(ibuf_inside());
	ut_ad(rec_get_n_fields_old(rec) > 2);

	field = rec_get_nth_field_old(rec, 1, &len);

	if (len == 1) {
		/* This is of the >= 4.1.x record format */
		ut_a(trx_sys_multiple_tablespace_format);

		field = rec_get_nth_field_old(rec, 2, &len);
	} else {
		ut_a(trx_doublewrite_must_reset_space_ids);
		ut_a(!trx_sys_multiple_tablespace_format);

		field = rec_get_nth_field_old(rec, 0, &len);
	}

	ut_a(len == 4);

	return(mach_read_from_4(field));
}

/********************************************************************//**
Returns the space id field of an ibuf record. For < 4.1.x format records
returns 0.
@return	space id */
static
ulint
ibuf_rec_get_space(
/*===============*/
	const rec_t*	rec)	/*!< in: ibuf record */
{
	const byte*	field;
	ulint		len;

	ut_ad(ibuf_inside());
	ut_ad(rec_get_n_fields_old(rec) > 2);

	field = rec_get_nth_field_old(rec, 1, &len);

	if (len == 1) {
		/* This is of the >= 4.1.x record format */

		ut_a(trx_sys_multiple_tablespace_format);
		field = rec_get_nth_field_old(rec, 0, &len);
		ut_a(len == 4);

		return(mach_read_from_4(field));
	}

	ut_a(trx_doublewrite_must_reset_space_ids);
	ut_a(!trx_sys_multiple_tablespace_format);

	return(0);
}

/********************************************************************//**
Creates a dummy index for inserting a record to a non-clustered index.

@return	dummy index */
static
dict_index_t*
ibuf_dummy_index_create(
/*====================*/
	ulint		n,	/*!< in: number of fields */
	ibool		comp)	/*!< in: TRUE=use compact record format */
{
	dict_table_t*	table;
	dict_index_t*	index;

	table = dict_mem_table_create("IBUF_DUMMY",
				      DICT_HDR_SPACE, n,
				      comp ? DICT_TF_COMPACT : 0);

	index = dict_mem_index_create("IBUF_DUMMY", "IBUF_DUMMY",
				      DICT_HDR_SPACE, 0, n);

	index->table = table;

	/* avoid ut_ad(index->cached) in dict_index_get_n_unique_in_tree */
	index->cached = TRUE;

	return(index);
}
/********************************************************************//**
Add a column to the dummy index */
static
void
ibuf_dummy_index_add_col(
/*=====================*/
	dict_index_t*	index,	/*!< in: dummy index */
	const dtype_t*	type,	/*!< in: the data type of the column */
	ulint		len)	/*!< in: length of the column */
{
	ulint	i	= index->table->n_def;
	dict_mem_table_add_col(index->table, NULL, NULL,
			       dtype_get_mtype(type),
			       dtype_get_prtype(type),
			       dtype_get_len(type));
	dict_index_add_col(index, index->table,
			   dict_table_get_nth_col(index->table, i), len);
}
/********************************************************************//**
Deallocates a dummy index for inserting a record to a non-clustered index. */
static
void
ibuf_dummy_index_free(
/*==================*/
	dict_index_t*	index)	/*!< in, own: dummy index */
{
	dict_table_t*	table = index->table;

	dict_mem_index_free(index);
	dict_mem_table_free(table);
}

/*********************************************************************//**
Builds the entry to insert into a non-clustered index when we have the
corresponding record in an ibuf index.

NOTE that as we copy pointers to fields in ibuf_rec, the caller must
hold a latch to the ibuf_rec page as long as the entry is used!

@return own: entry to insert to a non-clustered index */
UNIV_INLINE
dtuple_t*
ibuf_build_entry_pre_4_1_x(
/*=======================*/
	const rec_t*	ibuf_rec,	/*!< in: record in an insert buffer */
	mem_heap_t*	heap,		/*!< in: heap where built */
	dict_index_t**	pindex)		/*!< out, own: dummy index that
					describes the entry */
{
	ulint		i;
	ulint		len;
	const byte*	types;
	dtuple_t*	tuple;
	ulint		n_fields;

	ut_a(trx_doublewrite_must_reset_space_ids);
	ut_a(!trx_sys_multiple_tablespace_format);

	n_fields = rec_get_n_fields_old(ibuf_rec) - 2;
	tuple = dtuple_create(heap, n_fields);
	types = rec_get_nth_field_old(ibuf_rec, 1, &len);

	ut_a(len == n_fields * DATA_ORDER_NULL_TYPE_BUF_SIZE);

	for (i = 0; i < n_fields; i++) {
		const byte*	data;
		dfield_t*	field;

		field = dtuple_get_nth_field(tuple, i);

		data = rec_get_nth_field_old(ibuf_rec, i + 2, &len);

		dfield_set_data(field, data, len);

		dtype_read_for_order_and_null_size(
			dfield_get_type(field),
			types + i * DATA_ORDER_NULL_TYPE_BUF_SIZE);
	}

	*pindex = ibuf_dummy_index_create(n_fields, FALSE);

	return(tuple);
}

/*********************************************************************//**
Builds the entry to insert into a non-clustered index when we have the
corresponding record in an ibuf index.

NOTE that as we copy pointers to fields in ibuf_rec, the caller must
hold a latch to the ibuf_rec page as long as the entry is used!

@return own: entry to insert to a non-clustered index */
static
dtuple_t*
ibuf_build_entry_from_ibuf_rec(
/*===========================*/
	const rec_t*	ibuf_rec,	/*!< in: record in an insert buffer */
	mem_heap_t*	heap,		/*!< in: heap where built */
	dict_index_t**	pindex)		/*!< out, own: dummy index that
					describes the entry */
{
	dtuple_t*	tuple;
	dfield_t*	field;
	ulint		n_fields;
	const byte*	types;
	const byte*	data;
	ulint		len;
	ulint		i;
	dict_index_t*	index;

	data = rec_get_nth_field_old(ibuf_rec, 1, &len);

	if (len > 1) {
		/* This a < 4.1.x format record */

		return(ibuf_build_entry_pre_4_1_x(ibuf_rec, heap, pindex));
	}

	/* This a >= 4.1.x format record */

	ut_a(trx_sys_multiple_tablespace_format);
	ut_a(*data == 0);
	ut_a(rec_get_n_fields_old(ibuf_rec) > 4);

	n_fields = rec_get_n_fields_old(ibuf_rec) - 4;

	tuple = dtuple_create(heap, n_fields);

	types = rec_get_nth_field_old(ibuf_rec, 3, &len);

	ut_a(len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE <= 1);
	index = ibuf_dummy_index_create(
		n_fields, len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

	if (len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE) {
		/* compact record format */
		len--;
		ut_a(*types == 0);
		types++;
	}

	ut_a(len == n_fields * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

	for (i = 0; i < n_fields; i++) {
		field = dtuple_get_nth_field(tuple, i);

		data = rec_get_nth_field_old(ibuf_rec, i + 4, &len);

		dfield_set_data(field, data, len);

		dtype_new_read_for_order_and_null_size(
			dfield_get_type(field),
			types + i * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

		ibuf_dummy_index_add_col(index, dfield_get_type(field), len);
	}

	/* Prevent an ut_ad() failure in page_zip_write_rec() by
	adding system columns to the dummy table pointed to by the
	dummy secondary index.  The insert buffer is only used for
	secondary indexes, whose records never contain any system
	columns, such as DB_TRX_ID. */
	ut_d(dict_table_add_system_columns(index->table, index->table->heap));

	*pindex = index;

	return(tuple);
}

/********************************************************************//**
Returns the space taken by a stored non-clustered index entry if converted to
an index record.
@return size of index record in bytes + an upper limit of the space
taken in the page directory */
static
ulint
ibuf_rec_get_volume(
/*================*/
	const rec_t*	ibuf_rec)/*!< in: ibuf record */
{
	dtype_t		dtype;
	ibool		new_format	= FALSE;
	ulint		data_size	= 0;
	ulint		n_fields;
	const byte*	types;
	const byte*	data;
	ulint		len;
	ulint		i;
	ulint		comp;

	ut_ad(ibuf_inside());
	ut_ad(rec_get_n_fields_old(ibuf_rec) > 2);

	data = rec_get_nth_field_old(ibuf_rec, 1, &len);

	if (len > 1) {
		/* < 4.1.x format record */

		ut_a(trx_doublewrite_must_reset_space_ids);
		ut_a(!trx_sys_multiple_tablespace_format);

		n_fields = rec_get_n_fields_old(ibuf_rec) - 2;

		types = rec_get_nth_field_old(ibuf_rec, 1, &len);

		ut_ad(len == n_fields * DATA_ORDER_NULL_TYPE_BUF_SIZE);
		comp = 0;
	} else {
		/* >= 4.1.x format record */

		ut_a(trx_sys_multiple_tablespace_format);
		ut_a(*data == 0);

		types = rec_get_nth_field_old(ibuf_rec, 3, &len);

		comp = len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE;

		ut_a(comp <= 1);
		if (comp) {
			/* compact record format */
			ulint		volume;
			dict_index_t*	dummy_index;
			mem_heap_t*	heap = mem_heap_create(500);
			dtuple_t*	entry = ibuf_build_entry_from_ibuf_rec(
				ibuf_rec, heap, &dummy_index);
			volume = rec_get_converted_size(dummy_index, entry, 0);
			ibuf_dummy_index_free(dummy_index);
			mem_heap_free(heap);
			return(volume + page_dir_calc_reserved_space(1));
		}

		n_fields = rec_get_n_fields_old(ibuf_rec) - 4;

		new_format = TRUE;
	}

	for (i = 0; i < n_fields; i++) {
		if (new_format) {
			data = rec_get_nth_field_old(ibuf_rec, i + 4, &len);

			dtype_new_read_for_order_and_null_size(
				&dtype, types + i
				* DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);
		} else {
			data = rec_get_nth_field_old(ibuf_rec, i + 2, &len);

			dtype_read_for_order_and_null_size(
				&dtype, types + i
				* DATA_ORDER_NULL_TYPE_BUF_SIZE);
		}

		if (len == UNIV_SQL_NULL) {
			data_size += dtype_get_sql_null_size(&dtype, comp);
		} else {
			data_size += len;
		}
	}

	return(data_size + rec_get_converted_extra_size(data_size, n_fields, 0)
	       + page_dir_calc_reserved_space(1));
}

/*********************************************************************//**
Builds the tuple to insert to an ibuf tree when we have an entry for a
non-clustered index.

NOTE that the original entry must be kept because we copy pointers to
its fields.

@return	own: entry to insert into an ibuf index tree */
static
dtuple_t*
ibuf_entry_build(
/*=============*/
	dict_index_t*	index,	/*!< in: non-clustered index */
	const dtuple_t*	entry,	/*!< in: entry for a non-clustered index */
	ulint		space,	/*!< in: space id */
	ulint		page_no,/*!< in: index page number where entry should
				be inserted */
	mem_heap_t*	heap)	/*!< in: heap into which to build */
{
	dtuple_t*	tuple;
	dfield_t*	field;
	const dfield_t*	entry_field;
	ulint		n_fields;
	byte*		buf;
	byte*		buf2;
	ulint		i;

	/* Starting from 4.1.x, we have to build a tuple whose
	(1) first field is the space id,
	(2) the second field a single marker byte (0) to tell that this
	is a new format record,
	(3) the third contains the page number, and
	(4) the fourth contains the relevent type information of each data
	field; the length of this field % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE is
	(a) 0 for b-trees in the old format, and
	(b) 1 for b-trees in the compact format, the first byte of the field
	being the marker (0);
	(5) and the rest of the fields are copied from entry. All fields
	in the tuple are ordered like the type binary in our insert buffer
	tree. */

	n_fields = dtuple_get_n_fields(entry);

	tuple = dtuple_create(heap, n_fields + 4);

	/* Store the space id in tuple */

	field = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, space);

	dfield_set_data(field, buf, 4);

	/* Store the marker byte field in tuple */

	field = dtuple_get_nth_field(tuple, 1);

	buf = mem_heap_alloc(heap, 1);

	/* We set the marker byte zero */

	mach_write_to_1(buf, 0);

	dfield_set_data(field, buf, 1);

	/* Store the page number in tuple */

	field = dtuple_get_nth_field(tuple, 2);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, page_no);

	dfield_set_data(field, buf, 4);

	/* Store the type info in buf2, and add the fields from entry to
	tuple */
	buf2 = mem_heap_alloc(heap, n_fields
			      * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE
			      + dict_table_is_comp(index->table));
	if (dict_table_is_comp(index->table)) {
		*buf2++ = 0; /* write the compact format indicator */
	}
	for (i = 0; i < n_fields; i++) {
		ulint			fixed_len;
		const dict_field_t*	ifield;

		/* We add 4 below because we have the 4 extra fields at the
		start of an ibuf record */

		field = dtuple_get_nth_field(tuple, i + 4);
		entry_field = dtuple_get_nth_field(entry, i);
		dfield_copy(field, entry_field);

		ifield = dict_index_get_nth_field(index, i);
		/* Prefix index columns of fixed-length columns are of
		fixed length.  However, in the function call below,
		dfield_get_type(entry_field) contains the fixed length
		of the column in the clustered index.  Replace it with
		the fixed length of the secondary index column. */
		fixed_len = ifield->fixed_len;

#ifdef UNIV_DEBUG
		if (fixed_len) {
			/* dict_index_add_col() should guarantee these */
			ut_ad(fixed_len <= (ulint)
			      dfield_get_type(entry_field)->len);
			if (ifield->prefix_len) {
				ut_ad(ifield->prefix_len == fixed_len);
			} else {
				ut_ad(fixed_len == (ulint)
				      dfield_get_type(entry_field)->len);
			}
		}
#endif /* UNIV_DEBUG */

		dtype_new_store_for_order_and_null_size(
			buf2 + i * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE,
			dfield_get_type(entry_field), fixed_len);
	}

	/* Store the type info in buf2 to field 3 of tuple */

	field = dtuple_get_nth_field(tuple, 3);

	if (dict_table_is_comp(index->table)) {
		buf2--;
	}

	dfield_set_data(field, buf2, n_fields
			* DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE
			+ dict_table_is_comp(index->table));
	/* Set all the types in the new tuple binary */

	dtuple_set_types_binary(tuple, n_fields + 4);

	return(tuple);
}

/*********************************************************************//**
Builds a search tuple used to search buffered inserts for an index page.
This is for < 4.1.x format records
@return	own: search tuple */
static
dtuple_t*
ibuf_search_tuple_build(
/*====================*/
	ulint		space,	/*!< in: space id */
	ulint		page_no,/*!< in: index page number */
	mem_heap_t*	heap)	/*!< in: heap into which to build */
{
	dtuple_t*	tuple;
	dfield_t*	field;
	byte*		buf;

	ut_a(space == 0);
	ut_a(trx_doublewrite_must_reset_space_ids);
	ut_a(!trx_sys_multiple_tablespace_format);

	tuple = dtuple_create(heap, 1);

	/* Store the page number in tuple */

	field = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, page_no);

	dfield_set_data(field, buf, 4);

	dtuple_set_types_binary(tuple, 1);

	return(tuple);
}

/*********************************************************************//**
Builds a search tuple used to search buffered inserts for an index page.
This is for >= 4.1.x format records.
@return	own: search tuple */
static
dtuple_t*
ibuf_new_search_tuple_build(
/*========================*/
	ulint		space,	/*!< in: space id */
	ulint		page_no,/*!< in: index page number */
	mem_heap_t*	heap)	/*!< in: heap into which to build */
{
	dtuple_t*	tuple;
	dfield_t*	field;
	byte*		buf;

	ut_a(trx_sys_multiple_tablespace_format);

	tuple = dtuple_create(heap, 3);

	/* Store the space id in tuple */

	field = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, space);

	dfield_set_data(field, buf, 4);

	/* Store the new format record marker byte */

	field = dtuple_get_nth_field(tuple, 1);

	buf = mem_heap_alloc(heap, 1);

	mach_write_to_1(buf, 0);

	dfield_set_data(field, buf, 1);

	/* Store the page number in tuple */

	field = dtuple_get_nth_field(tuple, 2);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, page_no);

	dfield_set_data(field, buf, 4);

	dtuple_set_types_binary(tuple, 3);

	return(tuple);
}

/*********************************************************************//**
Checks if there are enough pages in the free list of the ibuf tree that we
dare to start a pessimistic insert to the insert buffer.
@return	TRUE if enough free pages in list */
UNIV_INLINE
ibool
ibuf_data_enough_free_for_insert(void)
/*==================================*/
{
	ut_ad(mutex_own(&ibuf_mutex));

	/* We want a big margin of free pages, because a B-tree can sometimes
	grow in size also if records are deleted from it, as the node pointers
	can change, and we must make sure that we are able to delete the
	inserts buffered for pages that we read to the buffer pool, without
	any risk of running out of free space in the insert buffer. */

	return(ibuf->free_list_len >= (ibuf->size / 2) + 3 * ibuf->height);
}

/*********************************************************************//**
Checks if there are enough pages in the free list of the ibuf tree that we
should remove them and free to the file space management.
@return	TRUE if enough free pages in list */
UNIV_INLINE
ibool
ibuf_data_too_much_free(void)
/*=========================*/
{
	ut_ad(mutex_own(&ibuf_mutex));

	return(ibuf->free_list_len >= 3 + (ibuf->size / 2) + 3 * ibuf->height);
}

/*********************************************************************//**
Allocates a new page from the ibuf file segment and adds it to the free
list.
@return	DB_SUCCESS, or DB_STRONG_FAIL if no space left */
static
ulint
ibuf_add_free_page(void)
/*====================*/
{
	mtr_t		mtr;
	page_t*		header_page;
	ulint		flags;
	ulint		zip_size;
	buf_block_t*	block;
	page_t*		page;
	page_t*		root;
	page_t*		bitmap_page;

	mtr_start(&mtr);

	/* Acquire the fsp latch before the ibuf header, obeying the latching
	order */
	mtr_x_lock(fil_space_get_latch(IBUF_SPACE_ID, &flags), &mtr);
	zip_size = dict_table_flags_to_zip_size(flags);

	header_page = ibuf_header_page_get(&mtr);

	/* Allocate a new page: NOTE that if the page has been a part of a
	non-clustered index which has subsequently been dropped, then the
	page may have buffered inserts in the insert buffer, and these
	should be deleted from there. These get deleted when the page
	allocation creates the page in buffer. Thus the call below may end
	up calling the insert buffer routines and, as we yet have no latches
	to insert buffer tree pages, these routines can run without a risk
	of a deadlock. This is the reason why we created a special ibuf
	header page apart from the ibuf tree. */

	block = fseg_alloc_free_page(
		header_page + IBUF_HEADER + IBUF_TREE_SEG_HEADER, 0, FSP_UP,
		&mtr);

	if (block == NULL) {
		mtr_commit(&mtr);

		return(DB_STRONG_FAIL);
	}

	ut_ad(rw_lock_get_x_lock_count(&block->lock) == 1);
	ibuf_enter();
	mutex_enter(&ibuf_mutex);
	root = ibuf_tree_root_get(&mtr);

	buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE_NEW);
	page = buf_block_get_frame(block);

	/* Add the page to the free list and update the ibuf size data */

	flst_add_last(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
		      page + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE, &mtr);

	mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_IBUF_FREE_LIST,
			 MLOG_2BYTES, &mtr);

	ibuf->seg_size++;
	ibuf->free_list_len++;

	/* Set the bit indicating that this page is now an ibuf tree page
	(level 2 page) */

	bitmap_page = ibuf_bitmap_get_map_page(
		IBUF_SPACE_ID, buf_block_get_page_no(block), zip_size, &mtr);

	ibuf_bitmap_page_set_bits(
		bitmap_page, buf_block_get_page_no(block), zip_size,
		IBUF_BITMAP_IBUF, TRUE, &mtr);

	mtr_commit(&mtr);

	mutex_exit(&ibuf_mutex);

	ibuf_exit();

	return(DB_SUCCESS);
}

/*********************************************************************//**
Removes a page from the free list and frees it to the fsp system. */
static
void
ibuf_remove_free_page(void)
/*=======================*/
{
	mtr_t	mtr;
	mtr_t	mtr2;
	page_t*	header_page;
	ulint	flags;
	ulint	zip_size;
	ulint	page_no;
	page_t*	page;
	page_t*	root;
	page_t*	bitmap_page;

	mtr_start(&mtr);

	/* Acquire the fsp latch before the ibuf header, obeying the latching
	order */
	mtr_x_lock(fil_space_get_latch(IBUF_SPACE_ID, &flags), &mtr);
	zip_size = dict_table_flags_to_zip_size(flags);

	header_page = ibuf_header_page_get(&mtr);

	/* Prevent pessimistic inserts to insert buffer trees for a while */
	mutex_enter(&ibuf_pessimistic_insert_mutex);

	ibuf_enter();

	mutex_enter(&ibuf_mutex);

	if (!ibuf_data_too_much_free()) {

		mutex_exit(&ibuf_mutex);

		ibuf_exit();

		mutex_exit(&ibuf_pessimistic_insert_mutex);

		mtr_commit(&mtr);

		return;
	}

	mtr_start(&mtr2);

	root = ibuf_tree_root_get(&mtr2);

	page_no = flst_get_last(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
				&mtr2).page;

	/* NOTE that we must release the latch on the ibuf tree root
	because in fseg_free_page we access level 1 pages, and the root
	is a level 2 page. */

	mtr_commit(&mtr2);
	mutex_exit(&ibuf_mutex);

	ibuf_exit();

	/* Since pessimistic inserts were prevented, we know that the
	page is still in the free list. NOTE that also deletes may take
	pages from the free list, but they take them from the start, and
	the free list was so long that they cannot have taken the last
	page from it. */

	fseg_free_page(header_page + IBUF_HEADER + IBUF_TREE_SEG_HEADER,
		       IBUF_SPACE_ID, page_no, &mtr);

#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	buf_page_reset_file_page_was_freed(IBUF_SPACE_ID, page_no);
#endif /* UNIV_DEBUG_FILE_ACCESSES || UNIV_DEBUG */

	ibuf_enter();

	mutex_enter(&ibuf_mutex);

	root = ibuf_tree_root_get(&mtr);

	ut_ad(page_no == flst_get_last(root + PAGE_HEADER
				       + PAGE_BTR_IBUF_FREE_LIST, &mtr).page);

	{
		buf_block_t*	block;

		block = buf_page_get(
			IBUF_SPACE_ID, 0, page_no, RW_X_LATCH, &mtr);

		buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);

		page = buf_block_get_frame(block);
	}

	/* Remove the page from the free list and update the ibuf size data */

	flst_remove(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
		    page + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST_NODE, &mtr);

	ibuf->seg_size--;
	ibuf->free_list_len--;

	mutex_exit(&ibuf_pessimistic_insert_mutex);

	/* Set the bit indicating that this page is no more an ibuf tree page
	(level 2 page) */

	bitmap_page = ibuf_bitmap_get_map_page(
		IBUF_SPACE_ID, page_no, zip_size, &mtr);

	ibuf_bitmap_page_set_bits(
		bitmap_page, page_no, zip_size, IBUF_BITMAP_IBUF, FALSE, &mtr);

#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	buf_page_set_file_page_was_freed(IBUF_SPACE_ID, page_no);
#endif /* UNIV_DEBUG_FILE_ACCESSES || UNIV_DEBUG */
	mtr_commit(&mtr);

	mutex_exit(&ibuf_mutex);

	ibuf_exit();
}

/***********************************************************************//**
Frees excess pages from the ibuf free list. This function is called when an OS
thread calls fsp services to allocate a new file segment, or a new page to a
file segment, and the thread did not own the fsp latch before this call. */
UNIV_INTERN
void
ibuf_free_excess_pages(void)
/*========================*/
{
	ulint		i;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(fil_space_get_latch(IBUF_SPACE_ID, NULL),
			  RW_LOCK_EX));
#endif /* UNIV_SYNC_DEBUG */

	ut_ad(rw_lock_get_x_lock_count(
		fil_space_get_latch(IBUF_SPACE_ID, NULL)) == 1);

	ut_ad(!ibuf_inside());

	/* NOTE: We require that the thread did not own the latch before,
	because then we know that we can obey the correct latching order
	for ibuf latches */

	if (!ibuf) {
		/* Not yet initialized; not sure if this is possible, but
		does no harm to check for it. */

		return;
	}

	/* Free at most a few pages at a time, so that we do not delay the
	requested service too much */

	for (i = 0; i < 4; i++) {

		mutex_enter(&ibuf_mutex);

		if (!ibuf_data_too_much_free()) {

			mutex_exit(&ibuf_mutex);

			return;
		}

		mutex_exit(&ibuf_mutex);

		ibuf_remove_free_page();
	}
}

/*********************************************************************//**
Reads page numbers from a leaf in an ibuf tree.
@return a lower limit for the combined volume of records which will be
merged */
static
ulint
ibuf_get_merge_page_nos(
/*====================*/
	ibool		contract,/*!< in: TRUE if this function is called to
				contract the tree, FALSE if this is called
				when a single page becomes full and we look
				if it pays to read also nearby pages */
	rec_t*		rec,	/*!< in: record from which we read up and down
				in the chain of records */
	ulint*		space_ids,/*!< in/out: space id's of the pages */
	ib_int64_t*	space_versions,/*!< in/out: tablespace version
				timestamps; used to prevent reading in old
				pages after DISCARD + IMPORT tablespace */
	ulint*		page_nos,/*!< in/out: buffer for at least
				IBUF_MAX_N_PAGES_MERGED many page numbers;
				the page numbers are in an ascending order */
	ulint*		n_stored)/*!< out: number of page numbers stored to
				page_nos in this function */
{
	ulint	prev_page_no;
	ulint	prev_space_id;
	ulint	first_page_no;
	ulint	first_space_id;
	ulint	rec_page_no;
	ulint	rec_space_id;
	ulint	sum_volumes;
	ulint	volume_for_page;
	ulint	rec_volume;
	ulint	limit;
	ulint	n_pages;

	*n_stored = 0;

	limit = ut_min(IBUF_MAX_N_PAGES_MERGED, buf_pool->curr_size / 4);

	if (page_rec_is_supremum(rec)) {

		rec = page_rec_get_prev(rec);
	}

	if (page_rec_is_infimum(rec)) {

		rec = page_rec_get_next(rec);
	}

	if (page_rec_is_supremum(rec)) {

		return(0);
	}

	first_page_no = ibuf_rec_get_page_no(rec);
	first_space_id = ibuf_rec_get_space(rec);
	n_pages = 0;
	prev_page_no = 0;
	prev_space_id = 0;

	/* Go backwards from the first rec until we reach the border of the
	'merge area', or the page start or the limit of storeable pages is
	reached */

	while (!page_rec_is_infimum(rec) && UNIV_LIKELY(n_pages < limit)) {

		rec_page_no = ibuf_rec_get_page_no(rec);
		rec_space_id = ibuf_rec_get_space(rec);

		if (rec_space_id != first_space_id
		    || (rec_page_no / IBUF_MERGE_AREA)
		    != (first_page_no / IBUF_MERGE_AREA)) {

			break;
		}

		if (rec_page_no != prev_page_no
		    || rec_space_id != prev_space_id) {
			n_pages++;
		}

		prev_page_no = rec_page_no;
		prev_space_id = rec_space_id;

		rec = page_rec_get_prev(rec);
	}

	rec = page_rec_get_next(rec);

	/* At the loop start there is no prev page; we mark this with a pair
	of space id, page no (0, 0) for which there can never be entries in
	the insert buffer */

	prev_page_no = 0;
	prev_space_id = 0;
	sum_volumes = 0;
	volume_for_page = 0;

	while (*n_stored < limit) {
		if (page_rec_is_supremum(rec)) {
			/* When no more records available, mark this with
			another 'impossible' pair of space id, page no */
			rec_page_no = 1;
			rec_space_id = 0;
		} else {
			rec_page_no = ibuf_rec_get_page_no(rec);
			rec_space_id = ibuf_rec_get_space(rec);
			/* In the system tablespace, the smallest
			possible secondary index leaf page number is
			bigger than IBUF_TREE_ROOT_PAGE_NO (4). In
			other tablespaces, the clustered index tree is
			created at page 3, which makes page 4 the
			smallest possible secondary index leaf page
			(and that only after DROP INDEX). */
			ut_ad(rec_page_no
			      > IBUF_TREE_ROOT_PAGE_NO - (rec_space_id != 0));
		}

#ifdef UNIV_IBUF_DEBUG
		ut_a(*n_stored < IBUF_MAX_N_PAGES_MERGED);
#endif
		if ((rec_space_id != prev_space_id
		     || rec_page_no != prev_page_no)
		    && (prev_space_id != 0 || prev_page_no != 0)) {

			if ((prev_page_no == first_page_no
			     && prev_space_id == first_space_id)
			    || contract
			    || (volume_for_page
				> ((IBUF_MERGE_THRESHOLD - 1)
				   * 4 * UNIV_PAGE_SIZE
				   / IBUF_PAGE_SIZE_PER_FREE_SPACE)
				/ IBUF_MERGE_THRESHOLD)) {

				space_ids[*n_stored] = prev_space_id;
				space_versions[*n_stored]
					= fil_space_get_version(prev_space_id);
				page_nos[*n_stored] = prev_page_no;

				(*n_stored)++;

				sum_volumes += volume_for_page;
			}

			if (rec_space_id != first_space_id
			    || rec_page_no / IBUF_MERGE_AREA
			    != first_page_no / IBUF_MERGE_AREA) {

				break;
			}

			volume_for_page = 0;
		}

		if (rec_page_no == 1 && rec_space_id == 0) {
			/* Supremum record */

			break;
		}

		rec_volume = ibuf_rec_get_volume(rec);

		volume_for_page += rec_volume;

		prev_page_no = rec_page_no;
		prev_space_id = rec_space_id;

		rec = page_rec_get_next(rec);
	}

#ifdef UNIV_IBUF_DEBUG
	ut_a(*n_stored <= IBUF_MAX_N_PAGES_MERGED);
#endif
#if 0
	fprintf(stderr, "Ibuf merge batch %lu pages %lu volume\n",
		*n_stored, sum_volumes);
#endif
	return(sum_volumes);
}

/*********************************************************************//**
Contracts insert buffer trees by reading pages to the buffer pool.
@return a lower limit for the combined size in bytes of entries which
will be merged from ibuf trees to the pages read, 0 if ibuf is
empty */
static
ulint
ibuf_contract_ext(
/*==============*/
	ulint*	n_pages,/*!< out: number of pages to which merged */
	ibool	sync)	/*!< in: TRUE if the caller wants to wait for the
			issued read with the highest tablespace address
			to complete */
{
	btr_pcur_t	pcur;
	ulint		page_nos[IBUF_MAX_N_PAGES_MERGED];
	ulint		space_ids[IBUF_MAX_N_PAGES_MERGED];
	ib_int64_t	space_versions[IBUF_MAX_N_PAGES_MERGED];
	ulint		n_stored;
	ulint		sum_sizes;
	mtr_t		mtr;

	*n_pages = 0;
	ut_ad(!ibuf_inside());

	mutex_enter(&ibuf_mutex);

	if (ibuf->empty) {
ibuf_is_empty:
		mutex_exit(&ibuf_mutex);

		return(0);
	}

	mtr_start(&mtr);

	ibuf_enter();

	/* Open a cursor to a randomly chosen leaf of the tree, at a random
	position within the leaf */

	btr_pcur_open_at_rnd_pos(ibuf->index, BTR_SEARCH_LEAF, &pcur, &mtr);

	if (page_get_n_recs(btr_pcur_get_page(&pcur)) == 0) {
		/* When the ibuf tree is emptied completely, the last record
		is removed using an optimistic delete and ibuf_size_update
		is not called, causing ibuf->empty to remain FALSE. If we do
		not reset it to TRUE here then database shutdown will hang
		in the loop in ibuf_contract_for_n_pages. */

		ibuf->empty = TRUE;

		ibuf_exit();

		mtr_commit(&mtr);
		btr_pcur_close(&pcur);

		goto ibuf_is_empty;
	}

	mutex_exit(&ibuf_mutex);

	sum_sizes = ibuf_get_merge_page_nos(TRUE, btr_pcur_get_rec(&pcur),
					    space_ids, space_versions,
					    page_nos, &n_stored);
#if 0 /* defined UNIV_IBUF_DEBUG */
	fprintf(stderr, "Ibuf contract sync %lu pages %lu volume %lu\n",
		sync, n_stored, sum_sizes);
#endif
	ibuf_exit();

	mtr_commit(&mtr);
	btr_pcur_close(&pcur);

	buf_read_ibuf_merge_pages(sync, space_ids, space_versions, page_nos,
				  n_stored);
	*n_pages = n_stored;

	return(sum_sizes + 1);
}

/*********************************************************************//**
Contracts insert buffer trees by reading pages to the buffer pool.
@return a lower limit for the combined size in bytes of entries which
will be merged from ibuf trees to the pages read, 0 if ibuf is
empty */
UNIV_INTERN
ulint
ibuf_contract(
/*==========*/
	ibool	sync)	/*!< in: TRUE if the caller wants to wait for the
			issued read with the highest tablespace address
			to complete */
{
	ulint	n_pages;

	return(ibuf_contract_ext(&n_pages, sync));
}

/*********************************************************************//**
Contracts insert buffer trees by reading pages to the buffer pool.
@return a lower limit for the combined size in bytes of entries which
will be merged from ibuf trees to the pages read, 0 if ibuf is
empty */
UNIV_INTERN
ulint
ibuf_contract_for_n_pages(
/*======================*/
	ibool	sync,	/*!< in: TRUE if the caller wants to wait for the
			issued read with the highest tablespace address
			to complete */
	ulint	n_pages)/*!< in: try to read at least this many pages to
			the buffer pool and merge the ibuf contents to
			them */
{
	ulint	sum_bytes	= 0;
	ulint	sum_pages	= 0;
	ulint	n_bytes;
	ulint	n_pag2;

	while (sum_pages < n_pages) {
		n_bytes = ibuf_contract_ext(&n_pag2, sync);

		if (n_bytes == 0) {
			return(sum_bytes);
		}

		sum_bytes += n_bytes;
		sum_pages += n_pag2;
	}

	return(sum_bytes);
}

/*********************************************************************//**
Contract insert buffer trees after insert if they are too big. */
UNIV_INLINE
void
ibuf_contract_after_insert(
/*=======================*/
	ulint	entry_size)	/*!< in: size of a record which was inserted
				into an ibuf tree */
{
	ibool	sync;
	ulint	sum_sizes;
	ulint	size;

	mutex_enter(&ibuf_mutex);

	if (ibuf->size < ibuf->max_size + IBUF_CONTRACT_ON_INSERT_NON_SYNC) {
		mutex_exit(&ibuf_mutex);

		return;
	}

	sync = FALSE;

	if (ibuf->size >= ibuf->max_size + IBUF_CONTRACT_ON_INSERT_SYNC) {

		sync = TRUE;
	}

	mutex_exit(&ibuf_mutex);

	/* Contract at least entry_size many bytes */
	sum_sizes = 0;
	size = 1;

	while ((size > 0) && (sum_sizes < entry_size)) {

		size = ibuf_contract(sync);
		sum_sizes += size;
	}
}

/*********************************************************************//**
Gets an upper limit for the combined size of entries buffered in the insert
buffer for a given page.
@return upper limit for the volume of buffered inserts for the index
page, in bytes; UNIV_PAGE_SIZE, if the entries for the index page span
several pages in the insert buffer */
static
ulint
ibuf_get_volume_buffered(
/*=====================*/
	btr_pcur_t*	pcur,	/*!< in: pcur positioned at a place in an
				insert buffer tree where we would insert an
				entry for the index page whose number is
				page_no, latch mode has to be BTR_MODIFY_PREV
				or BTR_MODIFY_TREE */
	ulint		space,	/*!< in: space id */
	ulint		page_no,/*!< in: page number of an index page */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ulint	volume;
	rec_t*	rec;
	page_t*	page;
	ulint	prev_page_no;
	page_t*	prev_page;
	ulint	next_page_no;
	page_t*	next_page;

	ut_a(trx_sys_multiple_tablespace_format);

	ut_ad((pcur->latch_mode == BTR_MODIFY_PREV)
	      || (pcur->latch_mode == BTR_MODIFY_TREE));

	/* Count the volume of records earlier in the alphabetical order than
	pcur */

	volume = 0;

	rec = btr_pcur_get_rec(pcur);
	page = page_align(rec);

	if (page_rec_is_supremum(rec)) {
		rec = page_rec_get_prev(rec);
	}

	for (;;) {
		if (page_rec_is_infimum(rec)) {

			break;
		}

		if (page_no != ibuf_rec_get_page_no(rec)
		    || space != ibuf_rec_get_space(rec)) {

			goto count_later;
		}

		volume += ibuf_rec_get_volume(rec);

		rec = page_rec_get_prev(rec);
	}

	/* Look at the previous page */

	prev_page_no = btr_page_get_prev(page, mtr);

	if (prev_page_no == FIL_NULL) {

		goto count_later;
	}

	{
		buf_block_t*	block;

		block = buf_page_get(
			IBUF_SPACE_ID, 0, prev_page_no, RW_X_LATCH, mtr);

		buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);


		prev_page = buf_block_get_frame(block);
	}

#ifdef UNIV_BTR_DEBUG
	ut_a(btr_page_get_next(prev_page, mtr)
	     == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

	rec = page_get_supremum_rec(prev_page);
	rec = page_rec_get_prev(rec);

	for (;;) {
		if (page_rec_is_infimum(rec)) {

			/* We cannot go to yet a previous page, because we
			do not have the x-latch on it, and cannot acquire one
			because of the latching order: we have to give up */

			return(UNIV_PAGE_SIZE);
		}

		if (page_no != ibuf_rec_get_page_no(rec)
		    || space != ibuf_rec_get_space(rec)) {

			goto count_later;
		}

		volume += ibuf_rec_get_volume(rec);

		rec = page_rec_get_prev(rec);
	}

count_later:
	rec = btr_pcur_get_rec(pcur);

	if (!page_rec_is_supremum(rec)) {
		rec = page_rec_get_next(rec);
	}

	for (;;) {
		if (page_rec_is_supremum(rec)) {

			break;
		}

		if (page_no != ibuf_rec_get_page_no(rec)
		    || space != ibuf_rec_get_space(rec)) {

			return(volume);
		}

		volume += ibuf_rec_get_volume(rec);

		rec = page_rec_get_next(rec);
	}

	/* Look at the next page */

	next_page_no = btr_page_get_next(page, mtr);

	if (next_page_no == FIL_NULL) {

		return(volume);
	}

	{
		buf_block_t*	block;

		block = buf_page_get(
			IBUF_SPACE_ID, 0, next_page_no, RW_X_LATCH, mtr);

		buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);


		next_page = buf_block_get_frame(block);
	}

#ifdef UNIV_BTR_DEBUG
	ut_a(btr_page_get_prev(next_page, mtr) == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

	rec = page_get_infimum_rec(next_page);
	rec = page_rec_get_next(rec);

	for (;;) {
		if (page_rec_is_supremum(rec)) {

			/* We give up */

			return(UNIV_PAGE_SIZE);
		}

		if (page_no != ibuf_rec_get_page_no(rec)
		    || space != ibuf_rec_get_space(rec)) {

			return(volume);
		}

		volume += ibuf_rec_get_volume(rec);

		rec = page_rec_get_next(rec);
	}
}

/*********************************************************************//**
Reads the biggest tablespace id from the high end of the insert buffer
tree and updates the counter in fil_system. */
UNIV_INTERN
void
ibuf_update_max_tablespace_id(void)
/*===============================*/
{
	ulint		max_space_id;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	btr_pcur_t	pcur;
	mtr_t		mtr;

	ut_a(!dict_table_is_comp(ibuf->index->table));

	ibuf_enter();

	mtr_start(&mtr);

	btr_pcur_open_at_index_side(
		FALSE, ibuf->index, BTR_SEARCH_LEAF, &pcur, TRUE, &mtr);

	btr_pcur_move_to_prev(&pcur, &mtr);

	if (btr_pcur_is_before_first_on_page(&pcur)) {
		/* The tree is empty */

		max_space_id = 0;
	} else {
		rec = btr_pcur_get_rec(&pcur);

		field = rec_get_nth_field_old(rec, 0, &len);

		ut_a(len == 4);

		max_space_id = mach_read_from_4(field);
	}

	mtr_commit(&mtr);
	ibuf_exit();

	/* printf("Maximum space id in insert buffer %lu\n", max_space_id); */

	fil_set_max_space_id_if_bigger(max_space_id);
}

/*********************************************************************//**
Makes an index insert to the insert buffer, instead of directly to the disk
page, if this is possible.
@return	DB_SUCCESS, DB_FAIL, DB_STRONG_FAIL */
static
ulint
ibuf_insert_low(
/*============*/
	ulint		mode,	/*!< in: BTR_MODIFY_PREV or BTR_MODIFY_TREE */
	const dtuple_t*	entry,	/*!< in: index entry to insert */
	ulint		entry_size,
				/*!< in: rec_get_converted_size(index, entry) */
	dict_index_t*	index,	/*!< in: index where to insert; must not be
				unique or clustered */
	ulint		space,	/*!< in: space id where to insert */
	ulint		zip_size,/*!< in: compressed page size in bytes, or 0 */
	ulint		page_no,/*!< in: page number where to insert */
	que_thr_t*	thr)	/*!< in: query thread */
{
	big_rec_t*	dummy_big_rec;
	btr_pcur_t	pcur;
	btr_cur_t*	cursor;
	dtuple_t*	ibuf_entry;
	mem_heap_t*	heap;
	ulint		buffered;
	rec_t*		ins_rec;
	ibool		old_bit_value;
	page_t*		bitmap_page;
	page_t*		root;
	ulint		err;
	ibool		do_merge;
	ulint		space_ids[IBUF_MAX_N_PAGES_MERGED];
	ib_int64_t	space_versions[IBUF_MAX_N_PAGES_MERGED];
	ulint		page_nos[IBUF_MAX_N_PAGES_MERGED];
	ulint		n_stored;
	ulint		bits;
	mtr_t		mtr;
	mtr_t		bitmap_mtr;

	ut_a(!dict_index_is_clust(index));
	ut_ad(dtuple_check_typed(entry));
	ut_ad(ut_is_2pow(zip_size));

	ut_a(trx_sys_multiple_tablespace_format);

	do_merge = FALSE;

	mutex_enter(&ibuf_mutex);

	if (ibuf->size >= ibuf->max_size + IBUF_CONTRACT_DO_NOT_INSERT) {
		/* Insert buffer is now too big, contract it but do not try
		to insert */

		mutex_exit(&ibuf_mutex);

#ifdef UNIV_IBUF_DEBUG
		fputs("Ibuf too big\n", stderr);
#endif
		/* Use synchronous contract (== TRUE) */
		ibuf_contract(TRUE);

		return(DB_STRONG_FAIL);
	}

	mutex_exit(&ibuf_mutex);

	if (mode == BTR_MODIFY_TREE) {
		mutex_enter(&ibuf_pessimistic_insert_mutex);

		ibuf_enter();

		mutex_enter(&ibuf_mutex);

		while (!ibuf_data_enough_free_for_insert()) {

			mutex_exit(&ibuf_mutex);

			ibuf_exit();

			mutex_exit(&ibuf_pessimistic_insert_mutex);

			err = ibuf_add_free_page();

			if (err == DB_STRONG_FAIL) {

				return(err);
			}

			mutex_enter(&ibuf_pessimistic_insert_mutex);

			ibuf_enter();

			mutex_enter(&ibuf_mutex);
		}
	} else {
		ibuf_enter();
	}

	heap = mem_heap_create(512);

	/* Build the entry which contains the space id and the page number as
	the first fields and the type information for other fields, and which
	will be inserted to the insert buffer. */

	ibuf_entry = ibuf_entry_build(index, entry, space, page_no, heap);

	/* Open a cursor to the insert buffer tree to calculate if we can add
	the new entry to it without exceeding the free space limit for the
	page. */

	mtr_start(&mtr);

	btr_pcur_open(ibuf->index, ibuf_entry, PAGE_CUR_LE, mode, &pcur, &mtr);

	/* Find out the volume of already buffered inserts for the same index
	page */
	buffered = ibuf_get_volume_buffered(&pcur, space, page_no, &mtr);

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a((buffered == 0) || ibuf_count_get(space, page_no));
#endif
	mtr_start(&bitmap_mtr);

	bitmap_page = ibuf_bitmap_get_map_page(space, page_no,
					       zip_size, &bitmap_mtr);

	/* We check if the index page is suitable for buffered entries */

	if (buf_page_peek(space, page_no)
	    || lock_rec_expl_exist_on_page(space, page_no)) {
		err = DB_STRONG_FAIL;

		mtr_commit(&bitmap_mtr);

		goto function_exit;
	}

	bits = ibuf_bitmap_page_get_bits(bitmap_page, page_no, zip_size,
					 IBUF_BITMAP_FREE, &bitmap_mtr);

	if (buffered + entry_size + page_dir_calc_reserved_space(1)
	    > ibuf_index_page_calc_free_from_bits(zip_size, bits)) {
		mtr_commit(&bitmap_mtr);

		/* It may not fit */
		err = DB_STRONG_FAIL;

		do_merge = TRUE;

		ibuf_get_merge_page_nos(FALSE, btr_pcur_get_rec(&pcur),
					space_ids, space_versions,
					page_nos, &n_stored);
		goto function_exit;
	}

	/* Set the bitmap bit denoting that the insert buffer contains
	buffered entries for this index page, if the bit is not set yet */

	old_bit_value = ibuf_bitmap_page_get_bits(
		bitmap_page, page_no, zip_size,
		IBUF_BITMAP_BUFFERED, &bitmap_mtr);

	if (!old_bit_value) {
		ibuf_bitmap_page_set_bits(bitmap_page, page_no, zip_size,
					  IBUF_BITMAP_BUFFERED, TRUE,
					  &bitmap_mtr);
	}

	mtr_commit(&bitmap_mtr);

	cursor = btr_pcur_get_btr_cur(&pcur);

	if (mode == BTR_MODIFY_PREV) {
		err = btr_cur_optimistic_insert(BTR_NO_LOCKING_FLAG, cursor,
						ibuf_entry, &ins_rec,
						&dummy_big_rec, 0, thr, &mtr);
		if (err == DB_SUCCESS) {
			/* Update the page max trx id field */
			page_update_max_trx_id(btr_cur_get_block(cursor), NULL,
					       thr_get_trx(thr)->id, &mtr);
		}
	} else {
		ut_ad(mode == BTR_MODIFY_TREE);

		/* We acquire an x-latch to the root page before the insert,
		because a pessimistic insert releases the tree x-latch,
		which would cause the x-latching of the root after that to
		break the latching order. */

		root = ibuf_tree_root_get(&mtr);

		err = btr_cur_optimistic_insert(
			BTR_NO_LOCKING_FLAG | BTR_NO_UNDO_LOG_FLAG,
			cursor, ibuf_entry, &ins_rec,
			&dummy_big_rec, 0, thr, &mtr);

		if (err == DB_FAIL) {
			err = btr_cur_pessimistic_insert(
				BTR_NO_LOCKING_FLAG
				| BTR_NO_UNDO_LOG_FLAG,
				cursor, ibuf_entry, &ins_rec,
				&dummy_big_rec, 0, thr, &mtr);
		}

		if (err == DB_SUCCESS) {
			/* Update the page max trx id field */
			page_update_max_trx_id(btr_cur_get_block(cursor), NULL,
					       thr_get_trx(thr)->id, &mtr);
		}

		ibuf_size_update(root, &mtr);
	}

function_exit:
#ifdef UNIV_IBUF_COUNT_DEBUG
	if (err == DB_SUCCESS) {
		fprintf(stderr,
			"Incrementing ibuf count of space %lu page %lu\n"
			"from %lu by 1\n", space, page_no,
			ibuf_count_get(space, page_no));

		ibuf_count_set(space, page_no,
			       ibuf_count_get(space, page_no) + 1);
	}
#endif
	if (mode == BTR_MODIFY_TREE) {

		mutex_exit(&ibuf_mutex);
		mutex_exit(&ibuf_pessimistic_insert_mutex);
	}

	mtr_commit(&mtr);
	btr_pcur_close(&pcur);
	ibuf_exit();

	mem_heap_free(heap);

	if (err == DB_SUCCESS) {
		mutex_enter(&ibuf_mutex);

		ibuf->empty = FALSE;
		ibuf->n_inserts++;

		mutex_exit(&ibuf_mutex);

		if (mode == BTR_MODIFY_TREE) {
			ibuf_contract_after_insert(entry_size);
		}
	}

	if (do_merge) {
#ifdef UNIV_IBUF_DEBUG
		ut_a(n_stored <= IBUF_MAX_N_PAGES_MERGED);
#endif
		buf_read_ibuf_merge_pages(FALSE, space_ids, space_versions,
					  page_nos, n_stored);
	}

	return(err);
}

/*********************************************************************//**
Makes an index insert to the insert buffer, instead of directly to the disk
page, if this is possible. Does not do insert if the index is clustered
or unique.
@return	TRUE if success */
UNIV_INTERN
ibool
ibuf_insert(
/*========*/
	const dtuple_t*	entry,	/*!< in: index entry to insert */
	dict_index_t*	index,	/*!< in: index where to insert */
	ulint		space,	/*!< in: space id where to insert */
	ulint		zip_size,/*!< in: compressed page size in bytes, or 0 */
	ulint		page_no,/*!< in: page number where to insert */
	que_thr_t*	thr)	/*!< in: query thread */
{
	ulint	err;
	ulint	entry_size;

	ut_a(trx_sys_multiple_tablespace_format);
	ut_ad(dtuple_check_typed(entry));
	ut_ad(ut_is_2pow(zip_size));

	ut_a(!dict_index_is_clust(index));

	switch (UNIV_EXPECT(ibuf_use, IBUF_USE_INSERT)) {
	case IBUF_USE_NONE:
		return(FALSE);
	case IBUF_USE_INSERT:
		goto do_insert;
	case IBUF_USE_COUNT:
		break;
	}

	ut_error; /* unknown value of ibuf_use */

do_insert:
	entry_size = rec_get_converted_size(index, entry, 0);

	if (entry_size
	    >= (page_get_free_space_of_empty(dict_table_is_comp(index->table))
		/ 2)) {
		return(FALSE);
	}

	err = ibuf_insert_low(BTR_MODIFY_PREV, entry, entry_size,
			      index, space, zip_size, page_no, thr);
	if (err == DB_FAIL) {
		err = ibuf_insert_low(BTR_MODIFY_TREE, entry, entry_size,
				      index, space, zip_size, page_no, thr);
	}

	if (err == DB_SUCCESS) {
#ifdef UNIV_IBUF_DEBUG
		/* fprintf(stderr, "Ibuf insert for page no %lu of index %s\n",
		page_no, index->name); */
#endif
		return(TRUE);

	} else {
		ut_a(err == DB_STRONG_FAIL);

		return(FALSE);
	}
}

/********************************************************************//**
During merge, inserts to an index page a secondary index entry extracted
from the insert buffer. */
static
void
ibuf_insert_to_index_page_low(
/*==========================*/
	const dtuple_t*	entry,	/*!< in: buffered entry to insert */
	buf_block_t*	block,	/*!< in/out: index page where the buffered
				entry should be placed */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr,	/*!< in/out: mtr */
	page_cur_t*	page_cur)/*!< in/out: cursor positioned on the record
				after which to insert the buffered entry */
{
	const page_t*	page;
	ulint		space;
	ulint		page_no;
	ulint		zip_size;
	const page_t*	bitmap_page;
	ulint		old_bits;

	if (UNIV_LIKELY
	    (page_cur_tuple_insert(page_cur, entry, index, 0, mtr) != NULL)) {
		return;
	}

	/* If the record did not fit, reorganize */

	btr_page_reorganize(block, index, mtr);
	page_cur_search(block, index, entry, PAGE_CUR_LE, page_cur);

	/* This time the record must fit */

	if (UNIV_LIKELY
	    (page_cur_tuple_insert(page_cur, entry, index, 0, mtr) != NULL)) {
		return;
	}

	page = buf_block_get_frame(block);

	ut_print_timestamp(stderr);

	fprintf(stderr,
		"  InnoDB: Error: Insert buffer insert fails;"
		" page free %lu, dtuple size %lu\n",
		(ulong) page_get_max_insert_size(page, 1),
		(ulong) rec_get_converted_size(index, entry, 0));
	fputs("InnoDB: Cannot insert index record ", stderr);
	dtuple_print(stderr, entry);
	fputs("\nInnoDB: The table where this index record belongs\n"
	      "InnoDB: is now probably corrupt. Please run CHECK TABLE on\n"
	      "InnoDB: that table.\n", stderr);

	space = page_get_space_id(page);
	zip_size = buf_block_get_zip_size(block);
	page_no = page_get_page_no(page);

	bitmap_page = ibuf_bitmap_get_map_page(space, page_no, zip_size, mtr);
	old_bits = ibuf_bitmap_page_get_bits(bitmap_page, page_no, zip_size,
					     IBUF_BITMAP_FREE, mtr);

	fprintf(stderr,
		"InnoDB: space %lu, page %lu, zip_size %lu, bitmap bits %lu\n",
		(ulong) space, (ulong) page_no,
		(ulong) zip_size, (ulong) old_bits);

	fputs("InnoDB: Submit a detailed bug report"
	      " to http://bugs.mysql.com\n", stderr);
}

/************************************************************************
During merge, inserts to an index page a secondary index entry extracted
from the insert buffer. */
static
void
ibuf_insert_to_index_page(
/*======================*/
	const dtuple_t*	entry,	/*!< in: buffered entry to insert */
	buf_block_t*	block,	/*!< in/out: index page where the buffered entry
				should be placed */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr)	/*!< in: mtr */
{
	page_cur_t	page_cur;
	ulint		low_match;
	page_t*		page		= buf_block_get_frame(block);
	rec_t*		rec;

	ut_ad(ibuf_inside());
	ut_ad(dtuple_check_typed(entry));
	ut_ad(!buf_block_align(page)->index);

	if (UNIV_UNLIKELY(dict_table_is_comp(index->table)
			  != (ibool)!!page_is_comp(page))) {
		fputs("InnoDB: Trying to insert a record from"
		      " the insert buffer to an index page\n"
		      "InnoDB: but the 'compact' flag does not match!\n",
		      stderr);
		goto dump;
	}

	rec = page_rec_get_next(page_get_infimum_rec(page));

	if (UNIV_UNLIKELY(rec_get_n_fields(rec, index)
			  != dtuple_get_n_fields(entry))) {
		fputs("InnoDB: Trying to insert a record from"
		      " the insert buffer to an index page\n"
		      "InnoDB: but the number of fields does not match!\n",
		      stderr);
dump:
		buf_page_print(page, 0);

		dtuple_print(stderr, entry);

		fputs("InnoDB: The table where where"
		      " this index record belongs\n"
		      "InnoDB: is now probably corrupt."
		      " Please run CHECK TABLE on\n"
		      "InnoDB: your tables.\n"
		      "InnoDB: Submit a detailed bug report to"
		      " http://bugs.mysql.com!\n", stderr);

		return;
	}

	low_match = page_cur_search(block, index, entry,
				    PAGE_CUR_LE, &page_cur);

	if (UNIV_UNLIKELY(low_match == dtuple_get_n_fields(entry))) {
		mem_heap_t*	heap;
		upd_t*		update;
		ulint*		offsets;
		page_zip_des_t*	page_zip;

		rec = page_cur_get_rec(&page_cur);

		/* This is based on
		row_ins_sec_index_entry_by_modify(BTR_MODIFY_LEAF). */
		ut_ad(rec_get_deleted_flag(rec, page_is_comp(page)));

		heap = mem_heap_create(1024);

		offsets = rec_get_offsets(rec, index, NULL, ULINT_UNDEFINED,
					  &heap);
		update = row_upd_build_sec_rec_difference_binary(
			index, entry, rec, NULL, heap);

		page_zip = buf_block_get_page_zip(block);

		if (update->n_fields == 0) {
			/* The records only differ in the delete-mark.
			Clear the delete-mark, like we did before
			Bug #56680 was fixed. */
			btr_cur_set_deleted_flag_for_ibuf(
				rec, page_zip, FALSE, mtr);
updated_in_place:
			mem_heap_free(heap);
			return;
		}

		/* Copy the info bits. Clear the delete-mark. */
		update->info_bits = rec_get_info_bits(rec, page_is_comp(page));
		update->info_bits &= ~REC_INFO_DELETED_FLAG;

		/* We cannot invoke btr_cur_optimistic_update() here,
		because we do not have a btr_cur_t or que_thr_t,
		as the insert buffer merge occurs at a very low level. */
		if (!row_upd_changes_field_size_or_external(index, offsets,
							    update)
		    && (!page_zip || btr_cur_update_alloc_zip(
				page_zip, block, index,
				rec_offs_size(offsets), FALSE, mtr))) {
			/* This is the easy case. Do something similar
			to btr_cur_update_in_place(). */
			row_upd_rec_in_place(rec, index, offsets,
					     update, page_zip);
			goto updated_in_place;
		}

		/* A collation may identify values that differ in
		storage length.
		Some examples (1 or 2 bytes):
		utf8_turkish_ci: I = U+0131 LATIN SMALL LETTER DOTLESS I
		utf8_general_ci: S = U+00DF LATIN SMALL LETTER SHARP S
		utf8_general_ci: A = U+00E4 LATIN SMALL LETTER A WITH DIAERESIS

		latin1_german2_ci: SS = U+00DF LATIN SMALL LETTER SHARP S

		Examples of a character (3-byte UTF-8 sequence)
		identified with 2 or 4 characters (1-byte UTF-8 sequences):

		utf8_unicode_ci: 'II' = U+2171 SMALL ROMAN NUMERAL TWO
		utf8_unicode_ci: '(10)' = U+247D PARENTHESIZED NUMBER TEN
		*/

		/* Delete the different-length record, and insert the
		buffered one. */

		lock_rec_store_on_page_infimum(block, rec);
		page_cur_delete_rec(&page_cur, index, offsets, mtr);
		page_cur_move_to_prev(&page_cur);
		mem_heap_free(heap);

		ibuf_insert_to_index_page_low(entry, block, index, mtr,
					      &page_cur);
		lock_rec_restore_from_page_infimum(block, rec, block);
	} else {
		ibuf_insert_to_index_page_low(entry, block, index, mtr,
					      &page_cur);
	}
}

/*********************************************************************//**
Deletes from ibuf the record on which pcur is positioned. If we have to
resort to a pessimistic delete, this function commits mtr and closes
the cursor.
@return	TRUE if mtr was committed and pcur closed in this operation */
static
ibool
ibuf_delete_rec(
/*============*/
	ulint		space,	/*!< in: space id */
	ulint		page_no,/*!< in: index page number where the record
				should belong */
	btr_pcur_t*	pcur,	/*!< in: pcur positioned on the record to
				delete, having latch mode BTR_MODIFY_LEAF */
	const dtuple_t*	search_tuple,
				/*!< in: search tuple for entries of page_no */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ibool		success;
	page_t*		root;
	ulint		err;

	ut_ad(ibuf_inside());
	ut_ad(page_rec_is_user_rec(btr_pcur_get_rec(pcur)));
	ut_ad(ibuf_rec_get_page_no(btr_pcur_get_rec(pcur)) == page_no);
	ut_ad(ibuf_rec_get_space(btr_pcur_get_rec(pcur)) == space);

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
	if (ibuf_debug == 2) {
		/* Inject a fault (crash). We do this before trying
		optimistic delete, because a pessimistic delete in the
		change buffer would require a larger test case. */

		/* Flag the buffered record as processed, to avoid
		an assertion failure after crash recovery. */
		btr_cur_set_deleted_flag_for_ibuf(
			btr_pcur_get_rec(pcur), NULL, TRUE, mtr);
		mtr_commit(mtr);
		log_make_checkpoint_at(IB_ULONGLONG_MAX, TRUE);
		DBUG_SUICIDE();
	}
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

	success = btr_cur_optimistic_delete(btr_pcur_get_btr_cur(pcur), mtr);

	if (success) {
#ifdef UNIV_IBUF_COUNT_DEBUG
		fprintf(stderr,
			"Decrementing ibuf count of space %lu page %lu\n"
			"from %lu by 1\n", space, page_no,
			ibuf_count_get(space, page_no));
		ibuf_count_set(space, page_no,
			       ibuf_count_get(space, page_no) - 1);
#endif
		return(FALSE);
	}

	ut_ad(page_rec_is_user_rec(btr_pcur_get_rec(pcur)));
	ut_ad(ibuf_rec_get_page_no(btr_pcur_get_rec(pcur)) == page_no);
	ut_ad(ibuf_rec_get_space(btr_pcur_get_rec(pcur)) == space);

	/* We have to resort to a pessimistic delete from ibuf.
	Delete-mark the record so that it will not be applied again,
	in case the server crashes before the pessimistic delete is
	made persistent. */
	btr_cur_set_deleted_flag_for_ibuf(
		btr_pcur_get_rec(pcur), NULL, TRUE, mtr);

	btr_pcur_store_position(pcur, mtr);

	btr_pcur_commit_specify_mtr(pcur, mtr);

	mutex_enter(&ibuf_mutex);

	mtr_start(mtr);

	success = btr_pcur_restore_position(BTR_MODIFY_TREE, pcur, mtr);

	if (!success) {
		if (fil_space_get_flags(space) == ULINT_UNDEFINED) {
			/* The tablespace has been dropped.  It is possible
			that another thread has deleted the insert buffer
			entry.  Do not complain. */
			goto commit_and_exit;
		}

		fprintf(stderr,
			"InnoDB: ERROR: Submit the output to"
			" http://bugs.mysql.com\n"
			"InnoDB: ibuf cursor restoration fails!\n"
			"InnoDB: ibuf record inserted to page %lu\n",
			(ulong) page_no);
		fflush(stderr);

		rec_print_old(stderr, btr_pcur_get_rec(pcur));
		rec_print_old(stderr, pcur->old_rec);
		dtuple_print(stderr, search_tuple);

		rec_print_old(stderr,
			      page_rec_get_next(btr_pcur_get_rec(pcur)));
		fflush(stderr);

		btr_pcur_commit_specify_mtr(pcur, mtr);

		fputs("InnoDB: Validating insert buffer tree:\n", stderr);
		if (!btr_validate_index(ibuf->index, NULL)) {
			ut_error;
		}

		fprintf(stderr, "InnoDB: ibuf tree ok\n");
		fflush(stderr);

		goto func_exit;
	}

	root = ibuf_tree_root_get(mtr);

	btr_cur_pessimistic_delete(&err, TRUE, btr_pcur_get_btr_cur(pcur),
				   RB_NONE, mtr);
	ut_a(err == DB_SUCCESS);

#ifdef UNIV_IBUF_COUNT_DEBUG
	ibuf_count_set(space, page_no, ibuf_count_get(space, page_no) - 1);
#endif
	ibuf_size_update(root, mtr);

commit_and_exit:
	btr_pcur_commit_specify_mtr(pcur, mtr);

func_exit:
	btr_pcur_close(pcur);

	mutex_exit(&ibuf_mutex);

	return(TRUE);
}

/*********************************************************************//**
When an index page is read from a disk to the buffer pool, this function
inserts to the page the possible index entries buffered in the insert buffer.
The entries are deleted from the insert buffer. If the page is not read, but
created in the buffer pool, this function deletes its buffered entries from
the insert buffer; there can exist entries for such a page if the page
belonged to an index which subsequently was dropped. */
UNIV_INTERN
void
ibuf_merge_or_delete_for_page(
/*==========================*/
	buf_block_t*	block,	/*!< in: if page has been read from
				disk, pointer to the page x-latched,
				else NULL */
	ulint		space,	/*!< in: space id of the index page */
	ulint		page_no,/*!< in: page number of the index page */
	ulint		zip_size,/*!< in: compressed page size in bytes,
				or 0 */
	ibool		update_ibuf_bitmap)/*!< in: normally this is set
				to TRUE, but if we have deleted or are
				deleting the tablespace, then we
				naturally do not want to update a
				non-existent bitmap page */
{
	mem_heap_t*	heap;
	btr_pcur_t	pcur;
	dtuple_t*	search_tuple;
	ulint		n_inserts;
#ifdef UNIV_IBUF_DEBUG
	ulint		volume;
#endif
	page_zip_des_t*	page_zip		= NULL;
	ibool		tablespace_being_deleted = FALSE;
	ibool		corruption_noticed	= FALSE;
	mtr_t		mtr;

	ut_ad(!block || buf_block_get_space(block) == space);
	ut_ad(!block || buf_block_get_page_no(block) == page_no);
	ut_ad(!block || buf_block_get_zip_size(block) == zip_size);
	ut_ad(!block || buf_block_get_io_fix(block) == BUF_IO_READ);

	if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE
	    || trx_sys_hdr_page(space, page_no)) {
		return;
	}

	/* We cannot refer to zip_size in the following, because
	zip_size is passed as ULINT_UNDEFINED (it is unknown) when
	buf_read_ibuf_merge_pages() is merging (discarding) changes
	for a dropped tablespace.  When block != NULL or
	update_ibuf_bitmap is specified, the zip_size must be known.
	That is why we will repeat the check below, with zip_size in
	place of 0.  Passing zip_size as 0 assumes that the
	uncompressed page size always is a power-of-2 multiple of the
	compressed page size. */

	if (ibuf_fixed_addr_page(space, 0, page_no)
	    || fsp_descr_page(0, page_no)) {
		return;
	}

	if (UNIV_LIKELY(update_ibuf_bitmap)) {
		ut_a(ut_is_2pow(zip_size));

		if (ibuf_fixed_addr_page(space, zip_size, page_no)
		    || fsp_descr_page(zip_size, page_no)) {
			return;
		}

		/* If the following returns FALSE, we get the counter
		incremented, and must decrement it when we leave this
		function. When the counter is > 0, that prevents tablespace
		from being dropped. */

		tablespace_being_deleted = fil_inc_pending_ops(space);

		if (UNIV_UNLIKELY(tablespace_being_deleted)) {
			/* Do not try to read the bitmap page from space;
			just delete the ibuf records for the page */

			block = NULL;
			update_ibuf_bitmap = FALSE;
		} else {
			page_t*	bitmap_page;

			mtr_start(&mtr);

			bitmap_page = ibuf_bitmap_get_map_page(
				space, page_no, zip_size, &mtr);

			if (!ibuf_bitmap_page_get_bits(bitmap_page, page_no,
						       zip_size,
						       IBUF_BITMAP_BUFFERED,
						       &mtr)) {
				/* No inserts buffered for this page */
				mtr_commit(&mtr);

				if (!tablespace_being_deleted) {
					fil_decr_pending_ops(space);
				}

				return;
			}
			mtr_commit(&mtr);
		}
	} else if (block
		   && (ibuf_fixed_addr_page(space, zip_size, page_no)
		      || fsp_descr_page(zip_size, page_no))) {

		return;
	}

	ibuf_enter();

	heap = mem_heap_create(512);

	if (!trx_sys_multiple_tablespace_format) {
		ut_a(trx_doublewrite_must_reset_space_ids);
		search_tuple = ibuf_search_tuple_build(space, page_no, heap);
	} else {
		search_tuple = ibuf_new_search_tuple_build(space, page_no,
							   heap);
	}

	if (block) {
		/* Move the ownership of the x-latch on the page to this OS
		thread, so that we can acquire a second x-latch on it. This
		is needed for the insert operations to the index page to pass
		the debug checks. */

		rw_lock_x_lock_move_ownership(&(block->lock));
		page_zip = buf_block_get_page_zip(block);

		if (UNIV_UNLIKELY(fil_page_get_type(block->frame)
				  != FIL_PAGE_INDEX)
		    || UNIV_UNLIKELY(!page_is_leaf(block->frame))) {

			page_t*	bitmap_page;

			corruption_noticed = TRUE;

			ut_print_timestamp(stderr);

			mtr_start(&mtr);

			fputs("  InnoDB: Dump of the ibuf bitmap page:\n",
			      stderr);

			bitmap_page = ibuf_bitmap_get_map_page(space, page_no,
							       zip_size, &mtr);
			buf_page_print(bitmap_page, 0);

			mtr_commit(&mtr);

			fputs("\nInnoDB: Dump of the page:\n", stderr);

			buf_page_print(block->frame, 0);

			fprintf(stderr,
				"InnoDB: Error: corruption in the tablespace."
				" Bitmap shows insert\n"
				"InnoDB: buffer records to page n:o %lu"
				" though the page\n"
				"InnoDB: type is %lu, which is"
				" not an index leaf page!\n"
				"InnoDB: We try to resolve the problem"
				" by skipping the insert buffer\n"
				"InnoDB: merge for this page."
				" Please run CHECK TABLE on your tables\n"
				"InnoDB: to determine if they are corrupt"
				" after this.\n\n"
				"InnoDB: Please submit a detailed bug report"
				" to http://bugs.mysql.com\n\n",
				(ulong) page_no,
				(ulong)
				fil_page_get_type(block->frame));
		}
	}

	n_inserts = 0;
#ifdef UNIV_IBUF_DEBUG
	volume = 0;
#endif
loop:
	mtr_start(&mtr);

	if (block) {
		ibool success;

		success = buf_page_get_known_nowait(
			RW_X_LATCH, block,
			BUF_KEEP_OLD, __FILE__, __LINE__, &mtr);

		ut_a(success);

		/* This is a user page (secondary index leaf page),
		but we pretend that it is a change buffer page in
		order to obey the latching order. This should be OK,
		because buffered changes are applied immediately while
		the block is io-fixed. Other threads must not try to
		latch an io-fixed block. */
		buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);
	}

	/* Position pcur in the insert buffer at the first entry for this
	index page */
	btr_pcur_open_on_user_rec(
		ibuf->index, search_tuple, PAGE_CUR_GE, BTR_MODIFY_LEAF,
		&pcur, &mtr);

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		ut_ad(btr_pcur_is_after_last_in_tree(&pcur, &mtr));

		goto reset_bit;
	}

	for (;;) {
		rec_t*	rec;

		ut_ad(btr_pcur_is_on_user_rec(&pcur));

		rec = btr_pcur_get_rec(&pcur);

		/* Check if the entry is for this index page */
		if (ibuf_rec_get_page_no(rec) != page_no
		    || ibuf_rec_get_space(rec) != space) {

			if (block) {
				page_header_reset_last_insert(
					block->frame, page_zip, &mtr);
			}

			goto reset_bit;
		}

		if (UNIV_UNLIKELY(corruption_noticed)) {
			fputs("InnoDB: Discarding record\n ", stderr);
			rec_print_old(stderr, rec);
			fputs("\nInnoDB: from the insert buffer!\n\n", stderr);
		} else if (block && !rec_get_deleted_flag(rec, 0)) {
			/* Now we have at pcur a record which should be
			inserted to the index page; NOTE that the call below
			copies pointers to fields in rec, and we must
			keep the latch to the rec page until the
			insertion is finished! */
			dtuple_t*	entry;
			trx_id_t	max_trx_id;
			dict_index_t*	dummy_index;

			max_trx_id = page_get_max_trx_id(page_align(rec));
			page_update_max_trx_id(block, page_zip, max_trx_id,
					       &mtr);

			entry = ibuf_build_entry_from_ibuf_rec(
				rec, heap, &dummy_index);
#ifdef UNIV_IBUF_DEBUG
			volume += rec_get_converted_size(dummy_index, entry, 0)
				+ page_dir_calc_reserved_space(1);
			ut_a(volume <= 4 * UNIV_PAGE_SIZE
			     / IBUF_PAGE_SIZE_PER_FREE_SPACE);
#endif
			ibuf_insert_to_index_page(entry, block,
						  dummy_index, &mtr);
			ibuf_dummy_index_free(dummy_index);
		}

		n_inserts++;

		/* Delete the record from ibuf */
		if (ibuf_delete_rec(space, page_no, &pcur, search_tuple,
				    &mtr)) {
			/* Deletion was pessimistic and mtr was committed:
			we start from the beginning again */

			goto loop;
		} else if (btr_pcur_is_after_last_on_page(&pcur)) {
			mtr_commit(&mtr);
			btr_pcur_close(&pcur);

			goto loop;
		}
	}

reset_bit:
#ifdef UNIV_IBUF_COUNT_DEBUG
	if (ibuf_count_get(space, page_no) > 0) {
		/* btr_print_tree(ibuf_data->index->tree, 100);
		ibuf_print(); */
	}
#endif
	if (UNIV_LIKELY(update_ibuf_bitmap)) {
		page_t*	bitmap_page;

		bitmap_page = ibuf_bitmap_get_map_page(
			space, page_no, zip_size, &mtr);

		ibuf_bitmap_page_set_bits(
			bitmap_page, page_no, zip_size,
			IBUF_BITMAP_BUFFERED, FALSE, &mtr);

		if (block) {
			ulint old_bits = ibuf_bitmap_page_get_bits(
				bitmap_page, page_no, zip_size,
				IBUF_BITMAP_FREE, &mtr);

			ulint new_bits = ibuf_index_page_calc_free(
				zip_size, block);

			if (old_bits != new_bits) {
				ibuf_bitmap_page_set_bits(
					bitmap_page, page_no, zip_size,
					IBUF_BITMAP_FREE, new_bits, &mtr);
			}
		}
	}

	mtr_commit(&mtr);
	btr_pcur_close(&pcur);
	mem_heap_free(heap);

	/* Protect our statistics keeping from race conditions */
	mutex_enter(&ibuf_mutex);

	ibuf->n_merges++;
	ibuf->n_merged_recs += n_inserts;

	mutex_exit(&ibuf_mutex);

	if (update_ibuf_bitmap && !tablespace_being_deleted) {

		fil_decr_pending_ops(space);
	}

	ibuf_exit();

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(space, page_no) == 0);
#endif
}

/*********************************************************************//**
Deletes all entries in the insert buffer for a given space id. This is used
in DISCARD TABLESPACE and IMPORT TABLESPACE.
NOTE: this does not update the page free bitmaps in the space. The space will
become CORRUPT when you call this function! */
UNIV_INTERN
void
ibuf_delete_for_discarded_space(
/*============================*/
	ulint	space)	/*!< in: space id */
{
	mem_heap_t*	heap;
	btr_pcur_t	pcur;
	dtuple_t*	search_tuple;
	rec_t*		ibuf_rec;
	ulint		page_no;
	ibool		closed;
	ulint		n_inserts;
	mtr_t		mtr;

	heap = mem_heap_create(512);

	/* Use page number 0 to build the search tuple so that we get the
	cursor positioned at the first entry for this space id */

	search_tuple = ibuf_new_search_tuple_build(space, 0, heap);

	n_inserts = 0;
loop:
	ibuf_enter();

	mtr_start(&mtr);

	/* Position pcur in the insert buffer at the first entry for the
	space */
	btr_pcur_open_on_user_rec(
		ibuf->index, search_tuple, PAGE_CUR_GE, BTR_MODIFY_LEAF,
		&pcur, &mtr);

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		ut_ad(btr_pcur_is_after_last_in_tree(&pcur, &mtr));

		goto leave_loop;
	}

	for (;;) {
		ut_ad(btr_pcur_is_on_user_rec(&pcur));

		ibuf_rec = btr_pcur_get_rec(&pcur);

		/* Check if the entry is for this space */
		if (ibuf_rec_get_space(ibuf_rec) != space) {

			goto leave_loop;
		}

		page_no = ibuf_rec_get_page_no(ibuf_rec);

		n_inserts++;

		/* Delete the record from ibuf */
		closed = ibuf_delete_rec(space, page_no, &pcur, search_tuple,
					 &mtr);
		if (closed) {
			/* Deletion was pessimistic and mtr was committed:
			we start from the beginning again */

			ibuf_exit();

			goto loop;
		}

		if (btr_pcur_is_after_last_on_page(&pcur)) {
			mtr_commit(&mtr);
			btr_pcur_close(&pcur);

			ibuf_exit();

			goto loop;
		}
	}

leave_loop:
	mtr_commit(&mtr);
	btr_pcur_close(&pcur);

	/* Protect our statistics keeping from race conditions */
	mutex_enter(&ibuf_mutex);

	ibuf->n_merges++;
	ibuf->n_merged_recs += n_inserts;

	mutex_exit(&ibuf_mutex);

	ibuf_exit();

	mem_heap_free(heap);
}

/******************************************************************//**
Looks if the insert buffer is empty.
@return	TRUE if empty */
UNIV_INTERN
ibool
ibuf_is_empty(void)
/*===============*/
{
	ibool		is_empty;
	const page_t*	root;
	mtr_t		mtr;

	ibuf_enter();

	mutex_enter(&ibuf_mutex);

	mtr_start(&mtr);

	root = ibuf_tree_root_get(&mtr);

	if (page_get_n_recs(root) == 0) {

		is_empty = TRUE;

		if (ibuf->empty == FALSE) {
			fprintf(stderr,
				"InnoDB: Warning: insert buffer tree is empty"
				" but the data struct does not\n"
				"InnoDB: know it. This condition is legal"
				" if the master thread has not yet\n"
				"InnoDB: run to completion.\n");
		}
	} else {
		ut_a(ibuf->empty == FALSE);

		is_empty = FALSE;
	}

	mtr_commit(&mtr);

	mutex_exit(&ibuf_mutex);

	ibuf_exit();

	return(is_empty);
}

/******************************************************************//**
Prints info of ibuf. */
UNIV_INTERN
void
ibuf_print(
/*=======*/
	FILE*	file)	/*!< in: file where to print */
{
#ifdef UNIV_IBUF_COUNT_DEBUG
	ulint		i;
	ulint		j;
#endif

	mutex_enter(&ibuf_mutex);

	fprintf(file,
		"Ibuf: size %lu, free list len %lu, seg size %lu,\n"
		"%lu inserts, %lu merged recs, %lu merges\n",
		(ulong) ibuf->size,
		(ulong) ibuf->free_list_len,
		(ulong) ibuf->seg_size,
		(ulong) ibuf->n_inserts,
		(ulong) ibuf->n_merged_recs,
		(ulong) ibuf->n_merges);
#ifdef UNIV_IBUF_COUNT_DEBUG
	for (i = 0; i < IBUF_COUNT_N_SPACES; i++) {
		for (j = 0; j < IBUF_COUNT_N_PAGES; j++) {
			ulint	count = ibuf_count_get(i, j);

			if (count > 0) {
				fprintf(stderr,
					"Ibuf count for space/page %lu/%lu"
					" is %lu\n",
					(ulong) i, (ulong) j, (ulong) count);
			}
		}
	}
#endif /* UNIV_IBUF_COUNT_DEBUG */

	mutex_exit(&ibuf_mutex);
}
#endif /* !UNIV_HOTBACKUP */
