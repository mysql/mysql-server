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
#include "srv0start.h" /* srv_shutdown_state */

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
"nullable" flag for the field.

In versions >= 5.5:

The optional marker byte at the start of the fourth field is replaced by
mandatory 3 fields, totaling 4 bytes:

 1. 2 bytes: Counter field, used to sort records within a (space id, page
    no) in the order they were added. This is needed so that for example the
    sequence of operations "INSERT x, DEL MARK x, INSERT x" is handled
    correctly.

 2. 1 byte: Operation type (see ibuf_op_t).

 3. 1 byte: Flags. Currently only one flag exists, IBUF_REC_COMPACT.

To ensure older records, which do not have counters to enforce correct
sorting, are merged before any new records, ibuf_insert checks if we're
trying to insert to a position that contains old-style records, and if so,
refuses the insert. Thus, ibuf pages are gradually converted to the new
format as their corresponding buffer pool pages are read into memory.
*/


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
UNIV_INTERN ibuf_use_t	ibuf_use		= IBUF_USE_ALL;

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/** Flag to control insert buffer debugging. */
UNIV_INTERN uint	ibuf_debug;
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/** The insert buffer control structure */
UNIV_INTERN ibuf_t*	ibuf			= NULL;

/** Counter for ibuf_should_try() */
UNIV_INTERN ulint	ibuf_flush_count	= 0;

#ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	ibuf_pessimistic_insert_mutex_key;
UNIV_INTERN mysql_pfs_key_t	ibuf_mutex_key;
UNIV_INTERN mysql_pfs_key_t	ibuf_bitmap_mutex_key;
#endif /* UNIV_PFS_MUTEX */

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

#define IBUF_REC_FIELD_SPACE	0	/*!< in the pre-4.1 format,
					the page number. later, the space_id */
#define IBUF_REC_FIELD_MARKER	1	/*!< starting with 4.1, a marker
					consisting of 1 byte that is 0 */
#define IBUF_REC_FIELD_PAGE	2	/*!< starting with 4.1, the
					page number */
#define IBUF_REC_FIELD_METADATA	3	/* the metadata field */
#define IBUF_REC_FIELD_USER	4	/* first user field */

/* Various constants for checking the type of an ibuf record and extracting
data from it. For details, see the description of the record format at the
top of this file. */

/** @name Format of the IBUF_REC_FIELD_METADATA of an insert buffer record
The fourth column in the MySQL 5.5 format contains an operation
type, counter, and some flags. */
/* @{ */
#define IBUF_REC_INFO_SIZE	4	/*!< Combined size of info fields at
					the beginning of the fourth field */
#if IBUF_REC_INFO_SIZE >= DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE
# error "IBUF_REC_INFO_SIZE >= DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE"
#endif

/* Offsets for the fields at the beginning of the fourth field */
#define IBUF_REC_OFFSET_COUNTER	0	/*!< Operation counter */
#define IBUF_REC_OFFSET_TYPE	2	/*!< Type of operation */
#define IBUF_REC_OFFSET_FLAGS	3	/*!< Additional flags */

/* Record flag masks */
#define IBUF_REC_COMPACT	0x1	/*!< Set in
					IBUF_REC_OFFSET_FLAGS if the
					user index is in COMPACT
					format or later */


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
Sets the flag in the current mini-transaction record indicating we're
inside an insert buffer routine. */
UNIV_INLINE
void
ibuf_enter(
/*=======*/
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	ut_ad(!mtr->inside_ibuf);
	mtr->inside_ibuf = TRUE;
}

/******************************************************************//**
Sets the flag in the current mini-transaction record indicating we're
exiting an insert buffer routine. */
UNIV_INLINE
void
ibuf_exit(
/*======*/
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	ut_ad(mtr->inside_ibuf);
	mtr->inside_ibuf = FALSE;
}

/**************************************************************//**
Commits an insert buffer mini-transaction and sets the persistent
cursor latch mode to BTR_NO_LATCHES, that is, detaches the cursor. */
UNIV_INLINE
void
ibuf_btr_pcur_commit_specify_mtr(
/*=============================*/
	btr_pcur_t*	pcur,	/*!< in/out: persistent cursor */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ut_d(ibuf_exit(mtr));
	btr_pcur_commit_specify_mtr(pcur, mtr);
}

/******************************************************************//**
Gets the ibuf header page and x-latches it.
@return	insert buffer header page */
static
page_t*
ibuf_header_page_get(
/*=================*/
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
{
	buf_block_t*	block;

	ut_ad(!ibuf_inside(mtr));

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
	page_t*		root;

	ut_ad(ibuf_inside(mtr));
	ut_ad(mutex_own(&ibuf_mutex));

	mtr_x_lock(dict_index_get_lock(ibuf->index), mtr);

	block = buf_page_get(
		IBUF_SPACE_ID, 0, FSP_IBUF_TREE_ROOT_PAGE_NO, RW_X_LATCH, mtr);

	buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE_NEW);

	root = buf_block_get_frame(block);

	ut_ad(page_get_space_id(root) == IBUF_SPACE_ID);
	ut_ad(page_get_page_no(root) == FSP_IBUF_TREE_ROOT_PAGE_NO);
	ut_ad(ibuf->empty == (page_get_n_recs(root) == 0));

	return(root);
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

	mutex_create(ibuf_pessimistic_insert_mutex_key,
		     &ibuf_pessimistic_insert_mutex,
		     SYNC_IBUF_PESS_INSERT_MUTEX);

	mutex_create(ibuf_mutex_key,
		     &ibuf_mutex, SYNC_IBUF_MUTEX);

	mutex_create(ibuf_bitmap_mutex_key,
		     &ibuf_bitmap_mutex, SYNC_IBUF_BITMAP_MUTEX);

	mtr_start(&mtr);

	mutex_enter(&ibuf_mutex);

	mtr_x_lock(fil_space_get_latch(IBUF_SPACE_ID, NULL), &mtr);

	header_page = ibuf_header_page_get(&mtr);

	fseg_n_reserved_pages(header_page + IBUF_HEADER + IBUF_TREE_SEG_HEADER,
			      &n_used, &mtr);
	ibuf_enter(&mtr);

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

	ibuf->empty = (page_get_n_recs(root) == 0);
	ibuf_mtr_commit(&mtr);

	heap = mem_heap_create(450);

	/* Use old-style record format for the insert buffer. */
	table = dict_mem_table_create(IBUF_TABLE_NAME, IBUF_SPACE_ID, 1, 0);

	dict_mem_table_add_col(table, heap, "DUMMY_COLUMN", DATA_BINARY, 0, 0);

	table->id = DICT_IBUF_ID_MIN + IBUF_SPACE_ID;

	dict_table_add_to_cache(table, heap);
	mem_heap_free(heap);

	index = dict_mem_index_create(
		IBUF_TABLE_NAME, "CLUST_IND",
		IBUF_SPACE_ID, DICT_CLUSTERED | DICT_UNIVERSAL | DICT_IBUF, 1);

	dict_mem_index_add_field(index, "DUMMY_COLUMN", 0);

	index->id = DICT_IBUF_ID_MIN + IBUF_SPACE_ID;

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
# ifdef UNIV_DEBUG
/** Gets the desired bits for a given page from a bitmap page.
@param page	in: bitmap page
@param offset	in: page whose bits to get
@param zs	in: compressed page size in bytes; 0 for uncompressed pages
@param bit	in: IBUF_BITMAP_FREE, IBUF_BITMAP_BUFFERED, ...
@param mtr	in: mini-transaction holding an x-latch on the bitmap page
@return	value of bits */
#  define ibuf_bitmap_page_get_bits(page, offset, zs, bit, mtr)	\
	ibuf_bitmap_page_get_bits_low(page, offset, zs,	\
				      MTR_MEMO_PAGE_X_FIX, mtr, bit)
# else /* UNIV_DEBUG */
/** Gets the desired bits for a given page from a bitmap page.
@param page	in: bitmap page
@param offset	in: page whose bits to get
@param zs	in: compressed page size in bytes; 0 for uncompressed pages
@param bit	in: IBUF_BITMAP_FREE, IBUF_BITMAP_BUFFERED, ...
@param mtr	in: mini-transaction holding an x-latch on the bitmap page
@return	value of bits */
#  define ibuf_bitmap_page_get_bits(page, offset, zs, bit, mtr)		\
	ibuf_bitmap_page_get_bits_low(page, offset, zs, bit)
# endif /* UNIV_DEBUG */

/********************************************************************//**
Gets the desired bits for a given page from a bitmap page.
@return	value of bits */
UNIV_INLINE
ulint
ibuf_bitmap_page_get_bits_low(
/*==========================*/
	const page_t*	page,	/*!< in: bitmap page */
	ulint		page_no,/*!< in: page whose bits to get */
	ulint		zip_size,/*!< in: compressed page size in bytes;
				0 for uncompressed pages */
#ifdef UNIV_DEBUG
	ulint		latch_type,
				/*!< in: MTR_MEMO_PAGE_X_FIX,
				MTR_MEMO_BUF_FIX, ... */
	mtr_t*		mtr,	/*!< in: mini-transaction holding latch_type
				on the bitmap page */
#endif /* UNIV_DEBUG */
	ulint		bit)	/*!< in: IBUF_BITMAP_FREE,
				IBUF_BITMAP_BUFFERED, ... */
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
	ut_ad(mtr_memo_contains_page(mtr, page, latch_type));

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
ibuf_page_low(
/*==========*/
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes, or 0 */
	ulint		page_no,/*!< in: page number */
#ifdef UNIV_DEBUG
	ibool		x_latch,/*!< in: FALSE if relaxed check
				(avoid latching the bitmap page) */
#endif /* UNIV_DEBUG */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mtr which will contain an
				x-latch to the bitmap page if the page
				is not one of the fixed address ibuf
				pages, or NULL, in which case a new
				transaction is created. */
{
	ibool	ret;
	mtr_t	local_mtr;
	page_t*	bitmap_page;

	ut_ad(!recv_no_ibuf_operations);
	ut_ad(x_latch || mtr == NULL);

	if (ibuf_fixed_addr_page(space, zip_size, page_no)) {

		return(TRUE);
	} else if (space != IBUF_SPACE_ID) {

		return(FALSE);
	}

	ut_ad(fil_space_get_type(IBUF_SPACE_ID) == FIL_TABLESPACE);

#ifdef UNIV_DEBUG
	if (!x_latch) {
		mtr_start(&local_mtr);

		/* Get the bitmap page without a page latch, so that
		we will not be violating the latching order when
		another bitmap page has already been latched by this
		thread. The page will be buffer-fixed, and thus it
		cannot be removed or relocated while we are looking at
		it. The contents of the page could change, but the
		IBUF_BITMAP_IBUF bit that we are interested in should
		not be modified by any other thread. Nobody should be
		calling ibuf_add_free_page() or ibuf_remove_free_page()
		while the page is linked to the insert buffer b-tree. */

		bitmap_page = buf_block_get_frame(
			buf_page_get_gen(
				space, zip_size,
				ibuf_bitmap_page_no_calc(zip_size, page_no),
				RW_NO_LATCH, NULL, BUF_GET_NO_LATCH,
				file, line, &local_mtr));

		ret = ibuf_bitmap_page_get_bits_low(
			bitmap_page, page_no, zip_size,
			MTR_MEMO_BUF_FIX, &local_mtr, IBUF_BITMAP_IBUF);

		mtr_commit(&local_mtr);
		return(ret);
	}
#endif /* UNIV_DEBUG */

	if (mtr == NULL) {
		mtr = &local_mtr;
		mtr_start(mtr);
	}

	bitmap_page = ibuf_bitmap_get_map_page_func(space, page_no, zip_size,
						    file, line, mtr);

	ret = ibuf_bitmap_page_get_bits(bitmap_page, page_no, zip_size,
					IBUF_BITMAP_IBUF, mtr);

	if (mtr == &local_mtr) {
		mtr_commit(mtr);
	}

	return(ret);
}

#ifdef UNIV_DEBUG
# define ibuf_rec_get_page_no(mtr,rec) ibuf_rec_get_page_no_func(mtr,rec)
#else /* UNIV_DEBUG */
# define ibuf_rec_get_page_no(mtr,rec) ibuf_rec_get_page_no_func(rec)
#endif /* UNIV_DEBUG */

/********************************************************************//**
Returns the page number field of an ibuf record.
@return	page number */
static
ulint
ibuf_rec_get_page_no_func(
/*======================*/
#ifdef UNIV_DEBUG
	mtr_t*		mtr,	/*!< in: mini-transaction owning rec */
#endif /* UNIV_DEBUG */
	const rec_t*	rec)	/*!< in: ibuf record */
{
	const byte*	field;
	ulint		len;

	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(ibuf_inside(mtr));
	ut_ad(rec_get_n_fields_old(rec) > 2);

	field = rec_get_nth_field_old(rec, IBUF_REC_FIELD_MARKER, &len);

	if (len == 1) {
		/* This is of the >= 4.1.x record format */
		ut_a(trx_sys_multiple_tablespace_format);

		field = rec_get_nth_field_old(rec, IBUF_REC_FIELD_PAGE, &len);
	} else {
		ut_a(trx_doublewrite_must_reset_space_ids);
		ut_a(!trx_sys_multiple_tablespace_format);

		field = rec_get_nth_field_old(rec, 0, &len);
	}

	ut_a(len == 4);

	return(mach_read_from_4(field));
}

#ifdef UNIV_DEBUG
# define ibuf_rec_get_space(mtr,rec) ibuf_rec_get_space_func(mtr,rec)
#else /* UNIV_DEBUG */
# define ibuf_rec_get_space(mtr,rec) ibuf_rec_get_space_func(rec)
#endif /* UNIV_DEBUG */

/********************************************************************//**
Returns the space id field of an ibuf record. For < 4.1.x format records
returns 0.
@return	space id */
static
ulint
ibuf_rec_get_space_func(
/*====================*/
#ifdef UNIV_DEBUG
	mtr_t*		mtr,	/*!< in: mini-transaction owning rec */
#endif /* UNIV_DEBUG */
	const rec_t*	rec)	/*!< in: ibuf record */
{
	const byte*	field;
	ulint		len;

	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(ibuf_inside(mtr));
	ut_ad(rec_get_n_fields_old(rec) > 2);

	field = rec_get_nth_field_old(rec, IBUF_REC_FIELD_MARKER, &len);

	if (len == 1) {
		/* This is of the >= 4.1.x record format */

		ut_a(trx_sys_multiple_tablespace_format);
		field = rec_get_nth_field_old(rec, IBUF_REC_FIELD_SPACE, &len);
		ut_a(len == 4);

		return(mach_read_from_4(field));
	}

	ut_a(trx_doublewrite_must_reset_space_ids);
	ut_a(!trx_sys_multiple_tablespace_format);

	return(0);
}

#ifdef UNIV_DEBUG
# define ibuf_rec_get_info(mtr,rec,op,comp,info_len,counter)	\
	ibuf_rec_get_info_func(mtr,rec,op,comp,info_len,counter)
#else /* UNIV_DEBUG */
# define ibuf_rec_get_info(mtr,rec,op,comp,info_len,counter)	\
	ibuf_rec_get_info_func(rec,op,comp,info_len,counter)
#endif
/****************************************************************//**
Get various information about an ibuf record in >= 4.1.x format. */
static
void
ibuf_rec_get_info_func(
/*===================*/
#ifdef UNIV_DEBUG
	mtr_t*		mtr,	/*!< in: mini-transaction owning rec */
#endif /* UNIV_DEBUG */
	const rec_t*	rec,		/*!< in: ibuf record */
	ibuf_op_t*	op,		/*!< out: operation type, or NULL */
	ibool*		comp,		/*!< out: compact flag, or NULL */
	ulint*		info_len,	/*!< out: length of info fields at the
					start of the fourth field, or
					NULL */
	ulint*		counter)	/*!< in: counter value, or NULL */
{
	const byte*	types;
	ulint		fields;
	ulint		len;

	/* Local variables to shadow arguments. */
	ibuf_op_t	op_local;
	ibool		comp_local;
	ulint		info_len_local;
	ulint		counter_local;

	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(ibuf_inside(mtr));
	fields = rec_get_n_fields_old(rec);
	ut_a(fields > IBUF_REC_FIELD_USER);

	types = rec_get_nth_field_old(rec, IBUF_REC_FIELD_METADATA, &len);

	info_len_local = len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE;

	switch (info_len_local) {
	case 0:
	case 1:
		op_local = IBUF_OP_INSERT;
		comp_local = info_len_local;
		ut_ad(!counter);
		counter_local = ULINT_UNDEFINED;
		break;

	case IBUF_REC_INFO_SIZE:
		op_local = (ibuf_op_t)types[IBUF_REC_OFFSET_TYPE];
		comp_local = types[IBUF_REC_OFFSET_FLAGS] & IBUF_REC_COMPACT;
		counter_local = mach_read_from_2(
			types + IBUF_REC_OFFSET_COUNTER);
		break;

	default:
		ut_error;
	}

	ut_a(op_local < IBUF_OP_COUNT);
	ut_a((len - info_len_local) ==
	     (fields - IBUF_REC_FIELD_USER)
	     * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

	if (op) {
		*op = op_local;
	}

	if (comp) {
		*comp = comp_local;
	}

	if (info_len) {
		*info_len = info_len_local;
	}

	if (counter) {
		*counter = counter_local;
	}
}

#ifdef UNIV_DEBUG
# define ibuf_rec_get_op_type(mtr,rec) ibuf_rec_get_op_type_func(mtr,rec)
#else /* UNIV_DEBUG */
# define ibuf_rec_get_op_type(mtr,rec) ibuf_rec_get_op_type_func(rec)
#endif

/****************************************************************//**
Returns the operation type field of an ibuf record.
@return	operation type */
static
ibuf_op_t
ibuf_rec_get_op_type_func(
/*======================*/
#ifdef UNIV_DEBUG
	mtr_t*		mtr,	/*!< in: mini-transaction owning rec */
#endif /* UNIV_DEBUG */
	const rec_t*	rec)	/*!< in: ibuf record */
{
	ulint		len;

	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(ibuf_inside(mtr));
	ut_ad(rec_get_n_fields_old(rec) > 2);

	(void) rec_get_nth_field_old(rec, IBUF_REC_FIELD_MARKER, &len);

	if (len > 1) {
		/* This is a < 4.1.x format record */

		return(IBUF_OP_INSERT);
	} else {
		ibuf_op_t	op;

		ibuf_rec_get_info(mtr, rec, &op, NULL, NULL, NULL);

		return(op);
	}
}

/****************************************************************//**
Read the first two bytes from a record's fourth field (counter field in new
records; something else in older records).
@return "counter" field, or ULINT_UNDEFINED if for some reason it
can't be read */
UNIV_INTERN
ulint
ibuf_rec_get_counter(
/*=================*/
	const rec_t*	rec)	/*!< in: ibuf record */
{
	const byte*	ptr;
	ulint		len;

	if (rec_get_n_fields_old(rec) <= IBUF_REC_FIELD_METADATA) {

		return(ULINT_UNDEFINED);
	}

	ptr = rec_get_nth_field_old(rec, IBUF_REC_FIELD_METADATA, &len);

	if (len >= 2) {

		return(mach_read_from_2(ptr));
	} else {

		return(ULINT_UNDEFINED);
	}
}

/****************************************************************//**
Add accumulated operation counts to a permanent array. Both arrays must be
of size IBUF_OP_COUNT. */
static
void
ibuf_add_ops(
/*=========*/
	ulint*		arr,	/*!< in/out: array to modify */
	const ulint*	ops)	/*!< in: operation counts */

{
	ulint	i;

#ifndef HAVE_ATOMIC_BUILTINS
	ut_ad(mutex_own(&ibuf_mutex));
#endif /* !HAVE_ATOMIC_BUILTINS */

	for (i = 0; i < IBUF_OP_COUNT; i++) {
#ifdef HAVE_ATOMIC_BUILTINS
		os_atomic_increment_ulint(&arr[i], ops[i]);
#else /* HAVE_ATOMIC_BUILTINS */
		arr[i] += ops[i];
#endif /* HAVE_ATOMIC_BUILTINS */
	}
}

/****************************************************************//**
Print operation counts. The array must be of size IBUF_OP_COUNT. */
static
void
ibuf_print_ops(
/*===========*/
	const ulint*	ops,	/*!< in: operation counts */
	FILE*		file)	/*!< in: file where to print */
{
	static const char* op_names[] = {
		"insert",
		"delete mark",
		"delete"
	};
	ulint	i;

	ut_a(UT_ARR_SIZE(op_names) == IBUF_OP_COUNT);

	for (i = 0; i < IBUF_OP_COUNT; i++) {
		fprintf(file, "%s %lu%s", op_names[i],
			(ulong) ops[i], (i < (IBUF_OP_COUNT - 1)) ? ", " : "");
	}

	putc('\n', file);
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

#ifdef UNIV_DEBUG
# define ibuf_build_entry_from_ibuf_rec(mtr,ibuf_rec,heap,pindex)	\
	ibuf_build_entry_from_ibuf_rec_func(mtr,ibuf_rec,heap,pindex)
#else /* UNIV_DEBUG */
# define ibuf_build_entry_from_ibuf_rec(mtr,ibuf_rec,heap,pindex)	\
	ibuf_build_entry_from_ibuf_rec_func(ibuf_rec,heap,pindex)
#endif

/*********************************************************************//**
Builds the entry used to

1) IBUF_OP_INSERT: insert into a non-clustered index

2) IBUF_OP_DELETE_MARK: find the record whose delete-mark flag we need to
   activate

3) IBUF_OP_DELETE: find the record we need to delete

when we have the corresponding record in an ibuf index.

NOTE that as we copy pointers to fields in ibuf_rec, the caller must
hold a latch to the ibuf_rec page as long as the entry is used!

@return own: entry to insert to a non-clustered index */
static
dtuple_t*
ibuf_build_entry_from_ibuf_rec_func(
/*================================*/
#ifdef UNIV_DEBUG
	mtr_t*		mtr,	/*!< in: mini-transaction owning rec */
#endif /* UNIV_DEBUG */
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
	ulint		info_len;
	ulint		i;
	ulint		comp;
	dict_index_t*	index;

	ut_ad(mtr_memo_contains_page(mtr, ibuf_rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, ibuf_rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(ibuf_inside(mtr));

	data = rec_get_nth_field_old(ibuf_rec, IBUF_REC_FIELD_MARKER, &len);

	if (len > 1) {
		/* This a < 4.1.x format record */

		return(ibuf_build_entry_pre_4_1_x(ibuf_rec, heap, pindex));
	}

	/* This a >= 4.1.x format record */

	ut_a(trx_sys_multiple_tablespace_format);
	ut_a(*data == 0);
	ut_a(rec_get_n_fields_old(ibuf_rec) > IBUF_REC_FIELD_USER);

	n_fields = rec_get_n_fields_old(ibuf_rec) - IBUF_REC_FIELD_USER;

	tuple = dtuple_create(heap, n_fields);

	types = rec_get_nth_field_old(ibuf_rec, IBUF_REC_FIELD_METADATA, &len);

	ibuf_rec_get_info(mtr, ibuf_rec, NULL, &comp, &info_len, NULL);

	index = ibuf_dummy_index_create(n_fields, comp);

	len -= info_len;
	types += info_len;

	ut_a(len == n_fields * DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

	for (i = 0; i < n_fields; i++) {
		field = dtuple_get_nth_field(tuple, i);

		data = rec_get_nth_field_old(
			ibuf_rec, i + IBUF_REC_FIELD_USER, &len);

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

/******************************************************************//**
Get the data size.
@return	size of fields */
UNIV_INLINE
ulint
ibuf_rec_get_size(
/*==============*/
	const rec_t*	rec,			/*!< in: ibuf record */
	const byte*	types,			/*!< in: fields */
	ulint		n_fields,		/*!< in: number of fields */
	ibool		pre_4_1,		/*!< in: TRUE=pre-4.1 format,
						FALSE=newer */
	ulint		comp)			/*!< in: 0=ROW_FORMAT=REDUNDANT,
						nonzero=ROW_FORMAT=COMPACT */
{
	ulint	i;
	ulint	field_offset;
	ulint	types_offset;
	ulint	size = 0;

	if (pre_4_1) {
		field_offset = 2;
		types_offset = DATA_ORDER_NULL_TYPE_BUF_SIZE;
	} else {
		field_offset = IBUF_REC_FIELD_USER;
		types_offset = DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE;
	}

	for (i = 0; i < n_fields; i++) {
		ulint		len;
		dtype_t		dtype;

		rec_get_nth_field_offs_old(rec, i + field_offset, &len);

		if (len != UNIV_SQL_NULL) {
			size += len;
		} else if (pre_4_1) {
			dtype_read_for_order_and_null_size(&dtype, types);

			size += dtype_get_sql_null_size(&dtype, comp);
		} else {
			dtype_new_read_for_order_and_null_size(&dtype, types);

			size += dtype_get_sql_null_size(&dtype, comp);
		}

		types += types_offset;
	}

	return(size);
}

#ifdef UNIV_DEBUG
# define ibuf_rec_get_volume(mtr,rec) ibuf_rec_get_volume_func(mtr,rec)
#else /* UNIV_DEBUG */
# define ibuf_rec_get_volume(mtr,rec) ibuf_rec_get_volume_func(rec)
#endif

/********************************************************************//**
Returns the space taken by a stored non-clustered index entry if converted to
an index record.
@return size of index record in bytes + an upper limit of the space
taken in the page directory */
static
ulint
ibuf_rec_get_volume_func(
/*=====================*/
#ifdef UNIV_DEBUG
	mtr_t*		mtr,	/*!< in: mini-transaction owning rec */
#endif /* UNIV_DEBUG */
	const rec_t*	ibuf_rec)/*!< in: ibuf record */
{
	ulint		len;
	const byte*	data;
	const byte*	types;
	ulint		n_fields;
	ulint		data_size;
	ibool		pre_4_1;
	ulint		comp;

	ut_ad(mtr_memo_contains_page(mtr, ibuf_rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, ibuf_rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(ibuf_inside(mtr));
	ut_ad(rec_get_n_fields_old(ibuf_rec) > 2);

	data = rec_get_nth_field_old(ibuf_rec, IBUF_REC_FIELD_MARKER, &len);
	pre_4_1 = (len > 1);

	if (pre_4_1) {
		/* < 4.1.x format record */

		ut_a(trx_doublewrite_must_reset_space_ids);
		ut_a(!trx_sys_multiple_tablespace_format);

		n_fields = rec_get_n_fields_old(ibuf_rec) - 2;

		types = rec_get_nth_field_old(ibuf_rec, 1, &len);

		ut_ad(len == n_fields * DATA_ORDER_NULL_TYPE_BUF_SIZE);
		comp = 0;
	} else {
		/* >= 4.1.x format record */
		ibuf_op_t	op;
		ulint		info_len;

		ut_a(trx_sys_multiple_tablespace_format);
		ut_a(*data == 0);

		types = rec_get_nth_field_old(
			ibuf_rec, IBUF_REC_FIELD_METADATA, &len);

		ibuf_rec_get_info(mtr, ibuf_rec, &op, &comp, &info_len, NULL);

		if (op == IBUF_OP_DELETE_MARK || op == IBUF_OP_DELETE) {
			/* Delete-marking a record doesn't take any
			additional space, and while deleting a record
			actually frees up space, we have to play it safe and
			pretend it takes no additional space (the record
			might not exist, etc.).  */

			return(0);
		} else if (comp) {
			dtuple_t*	entry;
			ulint		volume;
			dict_index_t*	dummy_index;
			mem_heap_t*	heap = mem_heap_create(500);

			entry = ibuf_build_entry_from_ibuf_rec(
				mtr, ibuf_rec, heap, &dummy_index);

			volume = rec_get_converted_size(dummy_index, entry, 0);

			ibuf_dummy_index_free(dummy_index);
			mem_heap_free(heap);

			return(volume + page_dir_calc_reserved_space(1));
		}

		types += info_len;
		n_fields = rec_get_n_fields_old(ibuf_rec)
			- IBUF_REC_FIELD_USER;
	}

	data_size = ibuf_rec_get_size(ibuf_rec, types, n_fields, pre_4_1, comp);

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
	ibuf_op_t	op,	/*!< in: operation type */
	dict_index_t*	index,	/*!< in: non-clustered index */
	const dtuple_t*	entry,	/*!< in: entry for a non-clustered index */
	ulint		space,	/*!< in: space id */
	ulint		page_no,/*!< in: index page number where entry should
				be inserted */
	ulint		counter,/*!< in: counter value;
				ULINT_UNDEFINED=not used */
	mem_heap_t*	heap)	/*!< in: heap into which to build */
{
	dtuple_t*	tuple;
	dfield_t*	field;
	const dfield_t*	entry_field;
	ulint		n_fields;
	byte*		buf;
	byte*		ti;
	byte*		type_info;
	ulint		i;

	ut_ad(counter != ULINT_UNDEFINED || op == IBUF_OP_INSERT);
	ut_ad(counter == ULINT_UNDEFINED || counter <= 0xFFFF);
	ut_ad(op < IBUF_OP_COUNT);

	/* We have to build a tuple with the following fields:

	1-4) These are described at the top of this file.

	5) The rest of the fields are copied from the entry.

	All fields in the tuple are ordered like the type binary in our
	insert buffer tree. */

	n_fields = dtuple_get_n_fields(entry);

	tuple = dtuple_create(heap, n_fields + IBUF_REC_FIELD_USER);

	/* 1) Space Id */

	field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_SPACE);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, space);

	dfield_set_data(field, buf, 4);

	/* 2) Marker byte */

	field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_MARKER);

	buf = mem_heap_alloc(heap, 1);

	/* We set the marker byte zero */

	mach_write_to_1(buf, 0);

	dfield_set_data(field, buf, 1);

	/* 3) Page number */

	field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_PAGE);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, page_no);

	dfield_set_data(field, buf, 4);

	/* 4) Type info, part #1 */

	if (counter == ULINT_UNDEFINED) {
		i = dict_table_is_comp(index->table) ? 1 : 0;
	} else {
		ut_ad(counter <= 0xFFFF);
		i = IBUF_REC_INFO_SIZE;
	}

	ti = type_info = mem_heap_alloc(heap, i + n_fields
					* DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE);

	switch (i) {
	default:
		ut_error;
		break;
	case 1:
		/* set the flag for ROW_FORMAT=COMPACT */
		*ti++ = 0;
		/* fall through */
	case 0:
		/* the old format does not allow delete buffering */
		ut_ad(op == IBUF_OP_INSERT);
		break;
	case IBUF_REC_INFO_SIZE:
		mach_write_to_2(ti + IBUF_REC_OFFSET_COUNTER, counter);

		ti[IBUF_REC_OFFSET_TYPE] = (byte) op;
		ti[IBUF_REC_OFFSET_FLAGS] = dict_table_is_comp(index->table)
			? IBUF_REC_COMPACT : 0;
		ti += IBUF_REC_INFO_SIZE;
		break;
	}

	/* 5+) Fields from the entry */

	for (i = 0; i < n_fields; i++) {
		ulint			fixed_len;
		const dict_field_t*	ifield;

		field = dtuple_get_nth_field(tuple, i + IBUF_REC_FIELD_USER);
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
			ti, dfield_get_type(entry_field), fixed_len);
		ti += DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE;
	}

	/* 4) Type info, part #2 */

	field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_METADATA);

	dfield_set_data(field, type_info, ti - type_info);

	/* Set all the types in the new tuple binary */

	dtuple_set_types_binary(tuple, n_fields + IBUF_REC_FIELD_USER);

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

	tuple = dtuple_create(heap, IBUF_REC_FIELD_METADATA);

	/* Store the space id in tuple */

	field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_SPACE);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, space);

	dfield_set_data(field, buf, 4);

	/* Store the new format record marker byte */

	field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_MARKER);

	buf = mem_heap_alloc(heap, 1);

	mach_write_to_1(buf, 0);

	dfield_set_data(field, buf, 1);

	/* Store the page number in tuple */

	field = dtuple_get_nth_field(tuple, IBUF_REC_FIELD_PAGE);

	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, page_no);

	dfield_set_data(field, buf, 4);

	dtuple_set_types_binary(tuple, IBUF_REC_FIELD_METADATA);

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
@return	TRUE on success, FALSE if no space left */
static
ibool
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

		return(FALSE);
	}

	ut_ad(rw_lock_get_x_lock_count(&block->lock) == 1);
	ibuf_enter(&mtr);
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

	mutex_exit(&ibuf_mutex);

	ibuf_bitmap_page_set_bits(
		bitmap_page, buf_block_get_page_no(block), zip_size,
		IBUF_BITMAP_IBUF, TRUE, &mtr);

	ibuf_mtr_commit(&mtr);

	return(TRUE);
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
	ibuf_enter(&mtr);
	mutex_enter(&ibuf_pessimistic_insert_mutex);
	mutex_enter(&ibuf_mutex);

	if (!ibuf_data_too_much_free()) {

		mutex_exit(&ibuf_mutex);
		mutex_exit(&ibuf_pessimistic_insert_mutex);

		ibuf_mtr_commit(&mtr);

		return;
	}

	ibuf_mtr_start(&mtr2);

	root = ibuf_tree_root_get(&mtr2);

	mutex_exit(&ibuf_mutex);

	page_no = flst_get_last(root + PAGE_HEADER + PAGE_BTR_IBUF_FREE_LIST,
				&mtr2).page;

	/* NOTE that we must release the latch on the ibuf tree root
	because in fseg_free_page we access level 1 pages, and the root
	is a level 2 page. */

	ibuf_mtr_commit(&mtr2);
	ibuf_exit(&mtr);

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

	ibuf_enter(&mtr);

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

	mutex_exit(&ibuf_pessimistic_insert_mutex);

	ibuf->seg_size--;
	ibuf->free_list_len--;

	/* Set the bit indicating that this page is no more an ibuf tree page
	(level 2 page) */

	bitmap_page = ibuf_bitmap_get_map_page(
		IBUF_SPACE_ID, page_no, zip_size, &mtr);

	mutex_exit(&ibuf_mutex);

	ibuf_bitmap_page_set_bits(
		bitmap_page, page_no, zip_size, IBUF_BITMAP_IBUF, FALSE, &mtr);

#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	buf_page_set_file_page_was_freed(IBUF_SPACE_ID, page_no);
#endif /* UNIV_DEBUG_FILE_ACCESSES || UNIV_DEBUG */
	ibuf_mtr_commit(&mtr);
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

		ibool	too_much_free;

		mutex_enter(&ibuf_mutex);
		too_much_free = ibuf_data_too_much_free();
		mutex_exit(&ibuf_mutex);

		if (!too_much_free) {
			return;
		}

		ibuf_remove_free_page();
	}
}

#ifdef UNIV_DEBUG
# define ibuf_get_merge_page_nos(contract,rec,mtr,ids,vers,pages,n_stored) \
	ibuf_get_merge_page_nos_func(contract,rec,mtr,ids,vers,pages,n_stored)
#else /* UNIV_DEBUG */
# define ibuf_get_merge_page_nos(contract,rec,mtr,ids,vers,pages,n_stored) \
	ibuf_get_merge_page_nos_func(contract,rec,ids,vers,pages,n_stored)
#endif /* UNIV_DEBUG */

/*********************************************************************//**
Reads page numbers from a leaf in an ibuf tree.
@return a lower limit for the combined volume of records which will be
merged */
static
ulint
ibuf_get_merge_page_nos_func(
/*=========================*/
	ibool		contract,/*!< in: TRUE if this function is called to
				contract the tree, FALSE if this is called
				when a single page becomes full and we look
				if it pays to read also nearby pages */
	const rec_t*	rec,	/*!< in: insert buffer record */
#ifdef UNIV_DEBUG
	mtr_t*		mtr,	/*!< in: mini-transaction holding rec */
#endif /* UNIV_DEBUG */
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

	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(ibuf_inside(mtr));

	*n_stored = 0;

	limit = ut_min(IBUF_MAX_N_PAGES_MERGED, buf_pool_get_curr_size() / 4);

	if (page_rec_is_supremum(rec)) {

		rec = page_rec_get_prev_const(rec);
	}

	if (page_rec_is_infimum(rec)) {

		rec = page_rec_get_next_const(rec);
	}

	if (page_rec_is_supremum(rec)) {

		return(0);
	}

	first_page_no = ibuf_rec_get_page_no(mtr, rec);
	first_space_id = ibuf_rec_get_space(mtr, rec);
	n_pages = 0;
	prev_page_no = 0;
	prev_space_id = 0;

	/* Go backwards from the first rec until we reach the border of the
	'merge area', or the page start or the limit of storeable pages is
	reached */

	while (!page_rec_is_infimum(rec) && UNIV_LIKELY(n_pages < limit)) {

		rec_page_no = ibuf_rec_get_page_no(mtr, rec);
		rec_space_id = ibuf_rec_get_space(mtr, rec);

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

		rec = page_rec_get_prev_const(rec);
	}

	rec = page_rec_get_next_const(rec);

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
			rec_page_no = ibuf_rec_get_page_no(mtr, rec);
			rec_space_id = ibuf_rec_get_space(mtr, rec);
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

			if (contract
			    || (prev_page_no == first_page_no
				&& prev_space_id == first_space_id)
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

		rec_volume = ibuf_rec_get_volume(mtr, rec);

		volume_for_page += rec_volume;

		prev_page_no = rec_page_no;
		prev_space_id = rec_space_id;

		rec = page_rec_get_next_const(rec);
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
	ulint		sum_sizes;
	mtr_t		mtr;

	*n_pages = 0;

	/* We perform a dirty read of ibuf->empty, without latching
	the insert buffer root page. We trust this dirty read except
	when a slow shutdown is being executed. During a slow
	shutdown, the insert buffer merge must be completed. */

	if (UNIV_UNLIKELY(ibuf->empty)
	    && UNIV_LIKELY(!srv_shutdown_state)) {
		return(0);
	}

	ibuf_mtr_start(&mtr);

	/* Open a cursor to a randomly chosen leaf of the tree, at a random
	position within the leaf */

	btr_pcur_open_at_rnd_pos(ibuf->index, BTR_SEARCH_LEAF, &pcur, &mtr);

	ut_ad(page_validate(btr_pcur_get_page(&pcur), ibuf->index));

	if (page_get_n_recs(btr_pcur_get_page(&pcur)) == 0) {
		/* If a B-tree page is empty, it must be the root page
		and the whole B-tree must be empty. InnoDB does not
		allow empty B-tree pages other than the root. */
		ut_ad(ibuf->empty);
		ut_ad(page_get_space_id(btr_pcur_get_page(&pcur))
		      == IBUF_SPACE_ID);
		ut_ad(page_get_page_no(btr_pcur_get_page(&pcur))
		      == FSP_IBUF_TREE_ROOT_PAGE_NO);

		ibuf_mtr_commit(&mtr);
		btr_pcur_close(&pcur);

		return(0);
	}

	sum_sizes = ibuf_get_merge_page_nos(TRUE,
					    btr_pcur_get_rec(&pcur), &mtr,
					    space_ids, space_versions,
					    page_nos, n_pages);
#if 0 /* defined UNIV_IBUF_DEBUG */
	fprintf(stderr, "Ibuf contract sync %lu pages %lu volume %lu\n",
		sync, *n_pages, sum_sizes);
#endif
	ibuf_mtr_commit(&mtr);
	btr_pcur_close(&pcur);

	buf_read_ibuf_merge_pages(sync, space_ids, space_versions, page_nos,
				  *n_pages);

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
	ulint	max_size;

	/* Perform dirty reads of ibuf->size and ibuf->max_size, to
	reduce ibuf_mutex contention. ibuf->max_size remains constant
	after ibuf_init_at_db_start(), but ibuf->size should be
	protected by ibuf_mutex. Given that ibuf->size fits in a
	machine word, this should be OK; at worst we are doing some
	excessive ibuf_contract() or occasionally skipping a
	ibuf_contract(). */
	size = ibuf->size;
	max_size = ibuf->max_size;

	if (size < max_size + IBUF_CONTRACT_ON_INSERT_NON_SYNC) {
		return;
	}

	sync = (size >= max_size + IBUF_CONTRACT_ON_INSERT_SYNC);

	/* Contract at least entry_size many bytes */
	sum_sizes = 0;
	size = 1;

	do {

		size = ibuf_contract(sync);
		sum_sizes += size;
	} while (size > 0 && sum_sizes < entry_size);
}

/*********************************************************************//**
Determine if an insert buffer record has been encountered already.
@return	TRUE if a new record, FALSE if possible duplicate */
static
ibool
ibuf_get_volume_buffered_hash(
/*==========================*/
	const rec_t*	rec,	/*!< in: ibuf record in post-4.1 format */
	const byte*	types,	/*!< in: fields */
	const byte*	data,	/*!< in: start of user record data */
	ulint		comp,	/*!< in: 0=ROW_FORMAT=REDUNDANT,
				nonzero=ROW_FORMAT=COMPACT */
	ulint*		hash,	/*!< in/out: hash array */
	ulint		size)	/*!< in: number of elements in hash array */
{
	ulint		len;
	ulint		fold;
	ulint		bitmask;

	len = ibuf_rec_get_size(
		rec, types,
		rec_get_n_fields_old(rec) - IBUF_REC_FIELD_USER,
		FALSE, comp);
	fold = ut_fold_binary(data, len);

	hash += (fold / (CHAR_BIT * sizeof *hash)) % size;
	bitmask = 1 << (fold % (CHAR_BIT * sizeof *hash));

	if (*hash & bitmask) {

		return(FALSE);
	}

	/* We have not seen this record yet.  Insert it. */
	*hash |= bitmask;

	return(TRUE);
}

#ifdef UNIV_DEBUG
# define ibuf_get_volume_buffered_count(mtr,rec,hash,size,n_recs)	\
	ibuf_get_volume_buffered_count_func(mtr,rec,hash,size,n_recs)
#else /* UNIV_DEBUG */
# define ibuf_get_volume_buffered_count(mtr,rec,hash,size,n_recs)	\
	ibuf_get_volume_buffered_count_func(rec,hash,size,n_recs)
#endif
/*********************************************************************//**
Update the estimate of the number of records on a page, and
get the space taken by merging the buffered record to the index page.
@return size of index record in bytes + an upper limit of the space
taken in the page directory */
static
ulint
ibuf_get_volume_buffered_count_func(
/*================================*/
#ifdef UNIV_DEBUG
	mtr_t*		mtr,	/*!< in: mini-transaction owning rec */
#endif /* UNIV_DEBUG */
	const rec_t*	rec,	/*!< in: insert buffer record */
	ulint*		hash,	/*!< in/out: hash array */
	ulint		size,	/*!< in: number of elements in hash array */
	lint*		n_recs)	/*!< in/out: estimated number of records
				on the page that rec points to */
{
	ulint		len;
	ibuf_op_t	ibuf_op;
	const byte*	types;
	ulint		n_fields;

	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(ibuf_inside(mtr));

	n_fields = rec_get_n_fields_old(rec);
	ut_ad(n_fields > IBUF_REC_FIELD_USER);
	n_fields -= IBUF_REC_FIELD_USER;

	rec_get_nth_field_offs_old(rec, 1, &len);
	/* This function is only invoked when buffering new
	operations.  All pre-4.1 records should have been merged
	when the database was started up. */
	ut_a(len == 1);
	ut_ad(trx_sys_multiple_tablespace_format);

	types = rec_get_nth_field_old(rec, IBUF_REC_FIELD_METADATA, &len);

	switch (UNIV_EXPECT(len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE,
			    IBUF_REC_INFO_SIZE)) {
	default:
		ut_error;
	case 0:
		/* This ROW_TYPE=REDUNDANT record does not include an
		operation counter.  Exclude it from the *n_recs,
		because deletes cannot be buffered if there are
		old-style inserts buffered for the page. */

		len = ibuf_rec_get_size(rec, types, n_fields, FALSE, 0);

		return(len
		       + rec_get_converted_extra_size(len, n_fields, 0)
		       + page_dir_calc_reserved_space(1));
	case 1:
		/* This ROW_TYPE=COMPACT record does not include an
		operation counter.  Exclude it from the *n_recs,
		because deletes cannot be buffered if there are
		old-style inserts buffered for the page. */
		goto get_volume_comp;

	case IBUF_REC_INFO_SIZE:
		ibuf_op = (ibuf_op_t) types[IBUF_REC_OFFSET_TYPE];
		break;
	}

	switch (ibuf_op) {
	case IBUF_OP_INSERT:
		/* Inserts can be done by updating a delete-marked record.
		Because delete-mark and insert operations can be pointing to
		the same records, we must not count duplicates. */
	case IBUF_OP_DELETE_MARK:
		/* There must be a record to delete-mark.
		See if this record has been already buffered. */
		if (n_recs && ibuf_get_volume_buffered_hash(
			    rec, types + IBUF_REC_INFO_SIZE,
			    types + len,
			    types[IBUF_REC_OFFSET_FLAGS] & IBUF_REC_COMPACT,
			    hash, size)) {
			(*n_recs)++;
		}

		if (ibuf_op == IBUF_OP_DELETE_MARK) {
			/* Setting the delete-mark flag does not
			affect the available space on the page. */
			return(0);
		}
		break;
	case IBUF_OP_DELETE:
		/* A record will be removed from the page. */
		if (n_recs) {
			(*n_recs)--;
		}
		/* While deleting a record actually frees up space,
		we have to play it safe and pretend that it takes no
		additional space (the record might not exist, etc.). */
		return(0);
	default:
		ut_error;
	}

	ut_ad(ibuf_op == IBUF_OP_INSERT);

get_volume_comp:
	{
		dtuple_t*	entry;
		ulint		volume;
		dict_index_t*	dummy_index;
		mem_heap_t*	heap = mem_heap_create(500);

		entry = ibuf_build_entry_from_ibuf_rec(
			mtr, rec, heap, &dummy_index);

		volume = rec_get_converted_size(dummy_index, entry, 0);

		ibuf_dummy_index_free(dummy_index);
		mem_heap_free(heap);

		return(volume + page_dir_calc_reserved_space(1));
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
	const btr_pcur_t*pcur,	/*!< in: pcur positioned at a place in an
				insert buffer tree where we would insert an
				entry for the index page whose number is
				page_no, latch mode has to be BTR_MODIFY_PREV
				or BTR_MODIFY_TREE */
	ulint		space,	/*!< in: space id */
	ulint		page_no,/*!< in: page number of an index page */
	lint*		n_recs,	/*!< in/out: minimum number of records on the
				page after the buffered changes have been
				applied, or NULL to disable the counting */
	mtr_t*		mtr)	/*!< in: mini-transaction of pcur */
{
	ulint		volume;
	const rec_t*	rec;
	const page_t*	page;
	ulint		prev_page_no;
	const page_t*	prev_page;
	ulint		next_page_no;
	const page_t*	next_page;
	/* bitmap of buffered recs */
	ulint		hash_bitmap[128 / sizeof(ulint)];

	ut_a(trx_sys_multiple_tablespace_format);

	ut_ad((pcur->latch_mode == BTR_MODIFY_PREV)
	      || (pcur->latch_mode == BTR_MODIFY_TREE));

	/* Count the volume of inserts earlier in the alphabetical order than
	pcur */

	volume = 0;

	if (n_recs) {
		memset(hash_bitmap, 0, sizeof hash_bitmap);
	}

	rec = btr_pcur_get_rec(pcur);
	page = page_align(rec);
	ut_ad(page_validate(page, ibuf->index));

	if (page_rec_is_supremum(rec)) {
		rec = page_rec_get_prev_const(rec);
	}

	for (; !page_rec_is_infimum(rec);
	     rec = page_rec_get_prev_const(rec)) {
		ut_ad(page_align(rec) == page);

		if (page_no != ibuf_rec_get_page_no(mtr, rec)
		    || space != ibuf_rec_get_space(mtr, rec)) {

			goto count_later;
		}

		volume += ibuf_get_volume_buffered_count(
			mtr, rec,
			hash_bitmap, UT_ARR_SIZE(hash_bitmap), n_recs);
	}

	/* Look at the previous page */

	prev_page_no = btr_page_get_prev(page, mtr);

	if (prev_page_no == FIL_NULL) {

		goto count_later;
	}

	{
		buf_block_t*	block;

		block = buf_page_get(
			IBUF_SPACE_ID, 0, prev_page_no, RW_X_LATCH,
			mtr);

		buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);


		prev_page = buf_block_get_frame(block);
		ut_ad(page_validate(prev_page, ibuf->index));
	}

#ifdef UNIV_BTR_DEBUG
	ut_a(btr_page_get_next(prev_page, mtr) == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

	rec = page_get_supremum_rec(prev_page);
	rec = page_rec_get_prev_const(rec);

	for (;; rec = page_rec_get_prev_const(rec)) {
		ut_ad(page_align(rec) == prev_page);

		if (page_rec_is_infimum(rec)) {

			/* We cannot go to yet a previous page, because we
			do not have the x-latch on it, and cannot acquire one
			because of the latching order: we have to give up */

			return(UNIV_PAGE_SIZE);
		}

		if (page_no != ibuf_rec_get_page_no(mtr, rec)
		    || space != ibuf_rec_get_space(mtr, rec)) {

			goto count_later;
		}

		volume += ibuf_get_volume_buffered_count(
			mtr, rec,
			hash_bitmap, UT_ARR_SIZE(hash_bitmap), n_recs);
	}

count_later:
	rec = btr_pcur_get_rec(pcur);

	if (!page_rec_is_supremum(rec)) {
		rec = page_rec_get_next_const(rec);
	}

	for (; !page_rec_is_supremum(rec);
	     rec = page_rec_get_next_const(rec)) {
		if (page_no != ibuf_rec_get_page_no(mtr, rec)
		    || space != ibuf_rec_get_space(mtr, rec)) {

			return(volume);
		}

		volume += ibuf_get_volume_buffered_count(
			mtr, rec,
			hash_bitmap, UT_ARR_SIZE(hash_bitmap), n_recs);
	}

	/* Look at the next page */

	next_page_no = btr_page_get_next(page, mtr);

	if (next_page_no == FIL_NULL) {

		return(volume);
	}

	{
		buf_block_t*	block;

		block = buf_page_get(
			IBUF_SPACE_ID, 0, next_page_no, RW_X_LATCH,
			mtr);

		buf_block_dbg_add_level(block, SYNC_IBUF_TREE_NODE);


		next_page = buf_block_get_frame(block);
		ut_ad(page_validate(next_page, ibuf->index));
	}

#ifdef UNIV_BTR_DEBUG
	ut_a(btr_page_get_prev(next_page, mtr) == page_get_page_no(page));
#endif /* UNIV_BTR_DEBUG */

	rec = page_get_infimum_rec(next_page);
	rec = page_rec_get_next_const(rec);

	for (;; rec = page_rec_get_next_const(rec)) {
		ut_ad(page_align(rec) == next_page);

		if (page_rec_is_supremum(rec)) {

			/* We give up */

			return(UNIV_PAGE_SIZE);
		}

		if (page_no != ibuf_rec_get_page_no(mtr, rec)
		    || space != ibuf_rec_get_space(mtr, rec)) {

			return(volume);
		}

		volume += ibuf_get_volume_buffered_count(
			mtr, rec,
			hash_bitmap, UT_ARR_SIZE(hash_bitmap), n_recs);
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

	ibuf_mtr_start(&mtr);

	btr_pcur_open_at_index_side(
		FALSE, ibuf->index, BTR_SEARCH_LEAF, &pcur, TRUE, &mtr);

	ut_ad(page_validate(btr_pcur_get_page(&pcur), ibuf->index));

	btr_pcur_move_to_prev(&pcur, &mtr);

	if (btr_pcur_is_before_first_on_page(&pcur)) {
		/* The tree is empty */

		max_space_id = 0;
	} else {
		rec = btr_pcur_get_rec(&pcur);

		field = rec_get_nth_field_old(rec, IBUF_REC_FIELD_SPACE, &len);

		ut_a(len == 4);

		max_space_id = mach_read_from_4(field);
	}

	ibuf_mtr_commit(&mtr);

	/* printf("Maximum space id in insert buffer %lu\n", max_space_id); */

	fil_set_max_space_id_if_bigger(max_space_id);
}

#ifdef UNIV_DEBUG
# define ibuf_get_entry_counter_low(mtr,rec,space,page_no)	\
	ibuf_get_entry_counter_low_func(mtr,rec,space,page_no)
#else /* UNIV_DEBUG */
# define ibuf_get_entry_counter_low(mtr,rec,space,page_no)	\
	ibuf_get_entry_counter_low_func(rec,space,page_no)
#endif
/****************************************************************//**
Helper function for ibuf_get_entry_counter_func. Checks if rec is for
(space, page_no), and if so, reads counter value from it and returns
that + 1.
@retval ULINT_UNDEFINED if the record does not contain any counter
@retval 0 if the record is not for (space, page_no)
@retval 1 + previous counter value, otherwise */
static
ulint
ibuf_get_entry_counter_low_func(
/*============================*/
#ifdef UNIV_DEBUG
	mtr_t*		mtr,		/*!< in: mini-transaction of rec */
#endif /* UNIV_DEBUG */
	const rec_t*	rec,		/*!< in: insert buffer record */
	ulint		space,		/*!< in: space id */
	ulint		page_no)	/*!< in: page number */
{
	ulint		counter;
	const byte*	field;
	ulint		len;

	ut_ad(ibuf_inside(mtr));
	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX)
	      || mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));
	ut_ad(rec_get_n_fields_old(rec) > 2);

	field = rec_get_nth_field_old(rec, IBUF_REC_FIELD_MARKER, &len);

	if (UNIV_UNLIKELY(len != 1)) {
		/* pre-4.1 format */
		ut_a(trx_doublewrite_must_reset_space_ids);
		ut_a(!trx_sys_multiple_tablespace_format);

		return(ULINT_UNDEFINED);
	}

	ut_a(trx_sys_multiple_tablespace_format);

	/* Check the tablespace identifier. */
	field = rec_get_nth_field_old(rec, IBUF_REC_FIELD_SPACE, &len);
	ut_a(len == 4);

	if (mach_read_from_4(field) != space) {

		return(0);
	}

	/* Check the page offset. */
	field = rec_get_nth_field_old(rec, IBUF_REC_FIELD_PAGE, &len);
	ut_a(len == 4);

	if (mach_read_from_4(field) != page_no) {

		return(0);
	}

	/* Check if the record contains a counter field. */
	field = rec_get_nth_field_old(rec, IBUF_REC_FIELD_METADATA, &len);

	switch (len % DATA_NEW_ORDER_NULL_TYPE_BUF_SIZE) {
	default:
		ut_error;
	case 0: /* ROW_FORMAT=REDUNDANT */
	case 1: /* ROW_FORMAT=COMPACT */
		return(ULINT_UNDEFINED);

	case IBUF_REC_INFO_SIZE:
		counter = mach_read_from_2(field + IBUF_REC_OFFSET_COUNTER);
		ut_a(counter < 0xFFFF);
		return(counter + 1);
	}
}

#ifdef UNIV_DEBUG
# define ibuf_get_entry_counter(space,page_no,rec,mtr,exact_leaf) \
	ibuf_get_entry_counter_func(space,page_no,rec,mtr,exact_leaf)
#else /* UNIV_DEBUG */
# define ibuf_get_entry_counter(space,page_no,rec,mtr,exact_leaf) \
	ibuf_get_entry_counter_func(space,page_no,rec,exact_leaf)
#endif

/****************************************************************//**
Calculate the counter field for an entry based on the current
last record in ibuf for (space, page_no).
@return	the counter field, or ULINT_UNDEFINED
if we should abort this insertion to ibuf */
static
ulint
ibuf_get_entry_counter_func(
/*========================*/
	ulint		space,		/*!< in: space id of entry */
	ulint		page_no,	/*!< in: page number of entry */
	const rec_t*	rec,		/*!< in: the record preceding the
					insertion point */
#ifdef UNIV_DEBUG
	mtr_t*		mtr,		/*!< in: mini-transaction */
#endif /* UNIV_DEBUG */
	ibool		only_leaf)	/*!< in: TRUE if this is the only
					leaf page that can contain entries
					for (space,page_no), that is, there
					was no exact match for (space,page_no)
					in the node pointer */
{
	ut_ad(ibuf_inside(mtr));
	ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_X_FIX));
	ut_ad(page_validate(page_align(rec), ibuf->index));

	if (page_rec_is_supremum(rec)) {
		/* This is just for safety. The record should be a
		page infimum or a user record. */
		ut_ad(0);
		return(ULINT_UNDEFINED);
	} else if (!page_rec_is_infimum(rec)) {
		return(ibuf_get_entry_counter_low(mtr, rec, space, page_no));
	} else if (only_leaf
		   || fil_page_get_prev(page_align(rec)) == FIL_NULL) {
		/* The parent node pointer did not contain the
		searched for (space, page_no), which means that the
		search ended on the correct page regardless of the
		counter value, and since we're at the infimum record,
		there are no existing records. */
		return(0);
	} else {
		/* We used to read the previous page here. It would
		break the latching order, because the caller has
		buffer-fixed an insert buffer bitmap page. */
		return(ULINT_UNDEFINED);
	}
}

/*********************************************************************//**
Buffer an operation in the insert/delete buffer, instead of doing it
directly to the disk page, if this is possible.
@return	DB_SUCCESS, DB_STRONG_FAIL or other error */
static
ulint
ibuf_insert_low(
/*============*/
	ulint		mode,	/*!< in: BTR_MODIFY_PREV or BTR_MODIFY_TREE */
	ibuf_op_t	op,	/*!< in: operation type */
	ibool		no_counter,
				/*!< in: TRUE=use 5.0.3 format;
				FALSE=allow delete buffering */
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
	lint		min_n_recs;
	rec_t*		ins_rec;
	ibool		old_bit_value;
	page_t*		bitmap_page;
	buf_block_t*	block;
	page_t*		root;
	ulint		err;
	ibool		do_merge;
	ulint		space_ids[IBUF_MAX_N_PAGES_MERGED];
	ib_int64_t	space_versions[IBUF_MAX_N_PAGES_MERGED];
	ulint		page_nos[IBUF_MAX_N_PAGES_MERGED];
	ulint		n_stored;
	mtr_t		mtr;
	mtr_t		bitmap_mtr;

	ut_a(!dict_index_is_clust(index));
	ut_ad(dtuple_check_typed(entry));
	ut_ad(ut_is_2pow(zip_size));
	ut_ad(!no_counter || op == IBUF_OP_INSERT);
	ut_a(op < IBUF_OP_COUNT);

	ut_a(trx_sys_multiple_tablespace_format);

	do_merge = FALSE;

	/* Perform dirty reads of ibuf->size and ibuf->max_size, to
	reduce ibuf_mutex contention. ibuf->max_size remains constant
	after ibuf_init_at_db_start(), but ibuf->size should be
	protected by ibuf_mutex. Given that ibuf->size fits in a
	machine word, this should be OK; at worst we are doing some
	excessive ibuf_contract() or occasionally skipping a
	ibuf_contract(). */
	if (ibuf->size >= ibuf->max_size + IBUF_CONTRACT_DO_NOT_INSERT) {
		/* Insert buffer is now too big, contract it but do not try
		to insert */


#ifdef UNIV_IBUF_DEBUG
		fputs("Ibuf too big\n", stderr);
#endif
		/* Use synchronous contract (== TRUE) */
		ibuf_contract(TRUE);

		return(DB_STRONG_FAIL);
	}

	heap = mem_heap_create(512);

	/* Build the entry which contains the space id and the page number
	as the first fields and the type information for other fields, and
	which will be inserted to the insert buffer. Using a counter value
	of 0xFFFF we find the last record for (space, page_no), from which
	we can then read the counter value N and use N + 1 in the record we
	insert. (We patch the ibuf_entry's counter field to the correct
	value just before actually inserting the entry.) */

	ibuf_entry = ibuf_entry_build(
		op, index, entry, space, page_no,
		no_counter ? ULINT_UNDEFINED : 0xFFFF, heap);

	/* Open a cursor to the insert buffer tree to calculate if we can add
	the new entry to it without exceeding the free space limit for the
	page. */

	if (mode == BTR_MODIFY_TREE) {
		for (;;) {
			mutex_enter(&ibuf_pessimistic_insert_mutex);
			mutex_enter(&ibuf_mutex);

			if (UNIV_LIKELY(ibuf_data_enough_free_for_insert())) {

				break;
			}

			mutex_exit(&ibuf_mutex);
			mutex_exit(&ibuf_pessimistic_insert_mutex);

			if (UNIV_UNLIKELY(!ibuf_add_free_page())) {

				mem_heap_free(heap);
				return(DB_STRONG_FAIL);
			}
		}
	}

	ibuf_mtr_start(&mtr);

	btr_pcur_open(ibuf->index, ibuf_entry, PAGE_CUR_LE, mode, &pcur, &mtr);
	ut_ad(page_validate(btr_pcur_get_page(&pcur), ibuf->index));

	/* Find out the volume of already buffered inserts for the same index
	page */
	min_n_recs = 0;
	buffered = ibuf_get_volume_buffered(&pcur, space, page_no,
					    op == IBUF_OP_DELETE
					    ? &min_n_recs
					    : NULL, &mtr);

	if (op == IBUF_OP_DELETE
	    && (min_n_recs < 2
		|| buf_pool_watch_occurred(space, page_no))) {
		/* The page could become empty after the record is
		deleted, or the page has been read in to the buffer
		pool.  Refuse to buffer the operation. */

		/* The buffer pool watch is needed for IBUF_OP_DELETE
		because of latching order considerations.  We can
		check buf_pool_watch_occurred() only after latching
		the insert buffer B-tree pages that contain buffered
		changes for the page.  We never buffer IBUF_OP_DELETE,
		unless some IBUF_OP_INSERT or IBUF_OP_DELETE_MARK have
		been previously buffered for the page.  Because there
		are buffered operations for the page, the insert
		buffer B-tree page latches held by mtr will guarantee
		that no changes for the user page will be merged
		before mtr_commit(&mtr).  We must not mtr_commit(&mtr)
		until after the IBUF_OP_DELETE has been buffered. */

fail_exit:
		if (mode == BTR_MODIFY_TREE) {
			mutex_exit(&ibuf_mutex);
			mutex_exit(&ibuf_pessimistic_insert_mutex);
		}

		err = DB_STRONG_FAIL;
		goto func_exit;
	}

	/* After this point, the page could still be loaded to the
	buffer pool, but we do not have to care about it, since we are
	holding a latch on the insert buffer leaf page that contains
	buffered changes for (space, page_no).  If the page enters the
	buffer pool, buf_page_io_complete() for (space, page_no) will
	have to acquire a latch on the same insert buffer leaf page,
	which it cannot do until we have buffered the IBUF_OP_DELETE
	and done mtr_commit(&mtr) to release the latch. */

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a((buffered == 0) || ibuf_count_get(space, page_no));
#endif
	ibuf_mtr_start(&bitmap_mtr);

	bitmap_page = ibuf_bitmap_get_map_page(space, page_no,
					       zip_size, &bitmap_mtr);

	/* We check if the index page is suitable for buffered entries */

	if (buf_page_peek(space, page_no)
	    || lock_rec_expl_exist_on_page(space, page_no)) {

		goto bitmap_fail;
	}

	if (op == IBUF_OP_INSERT) {
		ulint	bits = ibuf_bitmap_page_get_bits(
			bitmap_page, page_no, zip_size, IBUF_BITMAP_FREE,
			&bitmap_mtr);

		if (buffered + entry_size + page_dir_calc_reserved_space(1)
		    > ibuf_index_page_calc_free_from_bits(zip_size, bits)) {
			/* Release the bitmap page latch early. */
			ibuf_mtr_commit(&bitmap_mtr);

			/* It may not fit */
			do_merge = TRUE;

			ibuf_get_merge_page_nos(FALSE,
						btr_pcur_get_rec(&pcur), &mtr,
						space_ids, space_versions,
						page_nos, &n_stored);

			goto fail_exit;
		}
	}

	if (!no_counter) {
		/* Patch correct counter value to the entry to
		insert. This can change the insert position, which can
		result in the need to abort in some cases. */
		ulint		counter = ibuf_get_entry_counter(
			space, page_no, btr_pcur_get_rec(&pcur), &mtr,
			btr_pcur_get_btr_cur(&pcur)->low_match
			< IBUF_REC_FIELD_METADATA);
		dfield_t*	field;

		if (counter == ULINT_UNDEFINED) {
bitmap_fail:
			ibuf_mtr_commit(&bitmap_mtr);
			goto fail_exit;
		}

		field = dtuple_get_nth_field(
			ibuf_entry, IBUF_REC_FIELD_METADATA);
		mach_write_to_2(
			(byte*) dfield_get_data(field)
			+ IBUF_REC_OFFSET_COUNTER, counter);
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

	ibuf_mtr_commit(&bitmap_mtr);

	cursor = btr_pcur_get_btr_cur(&pcur);

	if (mode == BTR_MODIFY_PREV) {
		err = btr_cur_optimistic_insert(BTR_NO_LOCKING_FLAG, cursor,
						ibuf_entry, &ins_rec,
						&dummy_big_rec, 0, thr, &mtr);
		block = btr_cur_get_block(cursor);
		ut_ad(buf_block_get_space(block) == IBUF_SPACE_ID);

		/* If this is the root page, update ibuf->empty. */
		if (UNIV_UNLIKELY(buf_block_get_page_no(block)
				  == FSP_IBUF_TREE_ROOT_PAGE_NO)) {
			const page_t*	root = buf_block_get_frame(block);

			ut_ad(page_get_space_id(root) == IBUF_SPACE_ID);
			ut_ad(page_get_page_no(root)
			      == FSP_IBUF_TREE_ROOT_PAGE_NO);

			ibuf->empty = (page_get_n_recs(root) == 0);
		}
	} else {
		ut_ad(mode == BTR_MODIFY_TREE);

		/* We acquire an x-latch to the root page before the insert,
		because a pessimistic insert releases the tree x-latch,
		which would cause the x-latching of the root after that to
		break the latching order. */

		root = ibuf_tree_root_get(&mtr);

		err = btr_cur_pessimistic_insert(BTR_NO_LOCKING_FLAG
						 | BTR_NO_UNDO_LOG_FLAG,
						 cursor,
						 ibuf_entry, &ins_rec,
						 &dummy_big_rec, 0, thr, &mtr);
		mutex_exit(&ibuf_pessimistic_insert_mutex);
		ibuf_size_update(root, &mtr);
		mutex_exit(&ibuf_mutex);
		ibuf->empty = (page_get_n_recs(root) == 0);

		block = btr_cur_get_block(cursor);
		ut_ad(buf_block_get_space(block) == IBUF_SPACE_ID);
	}

	if (err == DB_SUCCESS && op != IBUF_OP_DELETE) {
		/* Update the page max trx id field */
		page_update_max_trx_id(block, NULL,
				       thr_get_trx(thr)->id, &mtr);
	}

func_exit:
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

	ibuf_mtr_commit(&mtr);
	btr_pcur_close(&pcur);

	mem_heap_free(heap);

	if (err == DB_SUCCESS && mode == BTR_MODIFY_TREE) {
		ibuf_contract_after_insert(entry_size);
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
Buffer an operation in the insert/delete buffer, instead of doing it
directly to the disk page, if this is possible. Does not do it if the index
is clustered or unique.
@return	TRUE if success */
UNIV_INTERN
ibool
ibuf_insert(
/*========*/
	ibuf_op_t	op,	/*!< in: operation type */
	const dtuple_t*	entry,	/*!< in: index entry to insert */
	dict_index_t*	index,	/*!< in: index where to insert */
	ulint		space,	/*!< in: space id where to insert */
	ulint		zip_size,/*!< in: compressed page size in bytes, or 0 */
	ulint		page_no,/*!< in: page number where to insert */
	que_thr_t*	thr)	/*!< in: query thread */
{
	ulint		err;
	ulint		entry_size;
	ibool		no_counter;
	/* Read the settable global variable ibuf_use only once in
	this function, so that we will have a consistent view of it. */
	ibuf_use_t	use		= ibuf_use;

	ut_a(trx_sys_multiple_tablespace_format);
	ut_ad(dtuple_check_typed(entry));
	ut_ad(ut_is_2pow(zip_size));

	ut_a(!dict_index_is_clust(index));

	no_counter = use <= IBUF_USE_INSERT;

	switch (op) {
	case IBUF_OP_INSERT:
		switch (use) {
		case IBUF_USE_NONE:
		case IBUF_USE_DELETE:
		case IBUF_USE_DELETE_MARK:
			return(FALSE);
		case IBUF_USE_INSERT:
		case IBUF_USE_INSERT_DELETE_MARK:
		case IBUF_USE_ALL:
			goto check_watch;
		case IBUF_USE_COUNT:
			break;
		}
		break;
	case IBUF_OP_DELETE_MARK:
		switch (use) {
		case IBUF_USE_NONE:
		case IBUF_USE_INSERT:
			return(FALSE);
		case IBUF_USE_DELETE_MARK:
		case IBUF_USE_DELETE:
		case IBUF_USE_INSERT_DELETE_MARK:
		case IBUF_USE_ALL:
			ut_ad(!no_counter);
			goto check_watch;
		case IBUF_USE_COUNT:
			break;
		}
		break;
	case IBUF_OP_DELETE:
		switch (use) {
		case IBUF_USE_NONE:
		case IBUF_USE_INSERT:
		case IBUF_USE_INSERT_DELETE_MARK:
			return(FALSE);
		case IBUF_USE_DELETE_MARK:
		case IBUF_USE_DELETE:
		case IBUF_USE_ALL:
			ut_ad(!no_counter);
			goto skip_watch;
		case IBUF_USE_COUNT:
			break;
		}
		break;
	case IBUF_OP_COUNT:
		break;
	}

	/* unknown op or use */
	ut_error;

check_watch:
	/* If a thread attempts to buffer an insert on a page while a
	purge is in progress on the same page, the purge must not be
	buffered, because it could remove a record that was
	re-inserted later.  For simplicity, we block the buffering of
	all operations on a page that has a purge pending.

	We do not check this in the IBUF_OP_DELETE case, because that
	would always trigger the buffer pool watch during purge and
	thus prevent the buffering of delete operations.  We assume
	that the issuer of IBUF_OP_DELETE has called
	buf_pool_watch_set(space, page_no). */

	{
		buf_page_t*	bpage;
		ulint		fold = buf_page_address_fold(space, page_no);
		buf_pool_t*	buf_pool = buf_pool_get(space, page_no);

		buf_pool_mutex_enter(buf_pool);
		bpage = buf_page_hash_get_low(buf_pool, space, page_no, fold);
		buf_pool_mutex_exit(buf_pool);

		if (UNIV_LIKELY_NULL(bpage)) {
			/* A buffer pool watch has been set or the
			page has been read into the buffer pool.
			Do not buffer the request.  If a purge operation
			is being buffered, have this request executed
			directly on the page in the buffer pool after the
			buffered entries for this page have been merged. */
			return(FALSE);
		}
	}

skip_watch:
	entry_size = rec_get_converted_size(index, entry, 0);

	if (entry_size
	    >= page_get_free_space_of_empty(dict_table_is_comp(index->table))
	    / 2) {

		return(FALSE);
	}

	err = ibuf_insert_low(BTR_MODIFY_PREV, op, no_counter,
			      entry, entry_size,
			      index, space, zip_size, page_no, thr);
	if (err == DB_FAIL) {
		err = ibuf_insert_low(BTR_MODIFY_TREE, op, no_counter,
				      entry, entry_size,
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
	ut_ad(0);
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

	ut_ad(ibuf_inside(mtr));
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

	if (page_rec_is_supremum(rec)) {
		fputs("InnoDB: Trying to insert a record from"
		      " the insert buffer to an index page\n"
		      "InnoDB: but the index page is empty!\n",
		      stderr);
		goto dump;
	}

	if (UNIV_UNLIKELY(rec_get_n_fields(rec, index)
			  != dtuple_get_n_fields(entry))) {
		fputs("InnoDB: Trying to insert a record from"
		      " the insert buffer to an index page\n"
		      "InnoDB: but the number of fields does not match!\n",
		      stderr);
dump:
		buf_page_print(page, 0, BUF_PAGE_PRINT_NO_CRASH);

		dtuple_print(stderr, entry);
		ut_ad(0);

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

/****************************************************************//**
During merge, sets the delete mark on a record for a secondary index
entry. */
static
void
ibuf_set_del_mark(
/*==============*/
	const dtuple_t*		entry,	/*!< in: entry */
	buf_block_t*		block,	/*!< in/out: block */
	const dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*			mtr)	/*!< in: mtr */
{
	page_cur_t	page_cur;
	ulint		low_match;

	ut_ad(ibuf_inside(mtr));
	ut_ad(dtuple_check_typed(entry));

	low_match = page_cur_search(
		block, index, entry, PAGE_CUR_LE, &page_cur);

	if (low_match == dtuple_get_n_fields(entry)) {
		rec_t*		rec;
		page_zip_des_t*	page_zip;

		rec = page_cur_get_rec(&page_cur);
		page_zip = page_cur_get_page_zip(&page_cur);

		/* Delete mark the old index record. According to a
		comment in row_upd_sec_index_entry(), it can already
		have been delete marked if a lock wait occurred in
		row_ins_index_entry() in a previous invocation of
		row_upd_sec_index_entry(). */

		if (UNIV_LIKELY
		    (!rec_get_deleted_flag(
			    rec, dict_table_is_comp(index->table)))) {
			btr_cur_set_deleted_flag_for_ibuf(rec, page_zip,
							  TRUE, mtr);
		}
	} else {
		const page_t*		page
			= page_cur_get_page(&page_cur);
		const buf_block_t*	block
			= page_cur_get_block(&page_cur);

		ut_print_timestamp(stderr);
		fputs("  InnoDB: unable to find a record to delete-mark\n",
		      stderr);
		fputs("InnoDB: tuple ", stderr);
		dtuple_print(stderr, entry);
		fputs("\n"
		      "InnoDB: record ", stderr);
		rec_print(stderr, page_cur_get_rec(&page_cur), index);
		fprintf(stderr, "\nspace %u offset %u"
			" (%u records, index id %llu)\n"
			"InnoDB: Submit a detailed bug report"
			" to http://bugs.mysql.com\n",
			(unsigned) buf_block_get_space(block),
			(unsigned) buf_block_get_page_no(block),
			(unsigned) page_get_n_recs(page),
			(ulonglong) btr_page_get_index_id(page));
		ut_ad(0);
	}
}

/****************************************************************//**
During merge, delete a record for a secondary index entry. */
static
void
ibuf_delete(
/*========*/
	const dtuple_t*	entry,	/*!< in: entry */
	buf_block_t*	block,	/*!< in/out: block */
	dict_index_t*	index,	/*!< in: record descriptor */
	mtr_t*		mtr)	/*!< in/out: mtr; must be committed
				before latching any further pages */
{
	page_cur_t	page_cur;
	ulint		low_match;

	ut_ad(ibuf_inside(mtr));
	ut_ad(dtuple_check_typed(entry));

	low_match = page_cur_search(
		block, index, entry, PAGE_CUR_LE, &page_cur);

	if (low_match == dtuple_get_n_fields(entry)) {
		page_zip_des_t*	page_zip= buf_block_get_page_zip(block);
		page_t*		page	= buf_block_get_frame(block);
		rec_t*		rec	= page_cur_get_rec(&page_cur);

		/* TODO: the below should probably be a separate function,
		it's a bastardized version of btr_cur_optimistic_delete. */

		ulint		offsets_[REC_OFFS_NORMAL_SIZE];
		ulint*		offsets	= offsets_;
		mem_heap_t*	heap = NULL;
		ulint		max_ins_size;

		rec_offs_init(offsets_);

		offsets = rec_get_offsets(
			rec, index, offsets, ULINT_UNDEFINED, &heap);

		if (page_get_n_recs(page) <= 1
		    || !(REC_INFO_DELETED_FLAG
			 & rec_get_info_bits(rec, page_is_comp(page)))) {
			/* Refuse to purge the last record or a
			record that has not been marked for deletion. */
			ut_print_timestamp(stderr);
			fputs("  InnoDB: unable to purge a record\n",
			      stderr);
			fputs("InnoDB: tuple ", stderr);
			dtuple_print(stderr, entry);
			fputs("\n"
			      "InnoDB: record ", stderr);
			rec_print_new(stderr, rec, offsets);
			fprintf(stderr, "\nspace %u offset %u"
				" (%u records, index id %llu)\n"
				"InnoDB: Submit a detailed bug report"
				" to http://bugs.mysql.com\n",
				(unsigned) buf_block_get_space(block),
				(unsigned) buf_block_get_page_no(block),
				(unsigned) page_get_n_recs(page),
				(ulonglong) btr_page_get_index_id(page));

			ut_ad(0);
			return;
		}

		lock_update_delete(block, rec);

		if (!page_zip) {
			max_ins_size
				= page_get_max_insert_size_after_reorganize(
					page, 1);
		}
#ifdef UNIV_ZIP_DEBUG
		ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */
		page_cur_delete_rec(&page_cur, index, offsets, mtr);
#ifdef UNIV_ZIP_DEBUG
		ut_a(!page_zip || page_zip_validate(page_zip, page));
#endif /* UNIV_ZIP_DEBUG */

		if (page_zip) {
			ibuf_update_free_bits_zip(block, mtr);
		} else {
			ibuf_update_free_bits_low(block, max_ins_size, mtr);
		}

		if (UNIV_LIKELY_NULL(heap)) {
			mem_heap_free(heap);
		}
	} else {
		/* The record must have been purged already. */
	}
}

/*********************************************************************//**
Restores insert buffer tree cursor position
@return	TRUE if the position was restored; FALSE if not */
static __attribute__((nonnull))
ibool
ibuf_restore_pos(
/*=============*/
	ulint		space,	/*!< in: space id */
	ulint		page_no,/*!< in: index page number where the record
				should belong */
	const dtuple_t*	search_tuple,
				/*!< in: search tuple for entries of page_no */
	ulint		mode,	/*!< in: BTR_MODIFY_LEAF or BTR_MODIFY_TREE */
	btr_pcur_t*	pcur,	/*!< in/out: persistent cursor whose
				position is to be restored */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
{
	ut_ad(mode == BTR_MODIFY_LEAF || mode == BTR_MODIFY_TREE);

	if (btr_pcur_restore_position(mode, pcur, mtr)) {

		return(TRUE);
	}

	if (fil_space_get_flags(space) == ULINT_UNDEFINED) {
		/* The tablespace has been dropped.  It is possible
		that another thread has deleted the insert buffer
		entry.  Do not complain. */
		ibuf_btr_pcur_commit_specify_mtr(pcur, mtr);
	} else {
		fprintf(stderr,
			"InnoDB: ERROR: Submit the output to"
			" http://bugs.mysql.com\n"
			"InnoDB: ibuf cursor restoration fails!\n"
			"InnoDB: ibuf record inserted to page %lu:%lu\n",
			(ulong) space, (ulong) page_no);
		fflush(stderr);

		rec_print_old(stderr, btr_pcur_get_rec(pcur));
		rec_print_old(stderr, pcur->old_rec);
		dtuple_print(stderr, search_tuple);

		rec_print_old(stderr,
			      page_rec_get_next(btr_pcur_get_rec(pcur)));
		fflush(stderr);

		ibuf_btr_pcur_commit_specify_mtr(pcur, mtr);

		fputs("InnoDB: Validating insert buffer tree:\n", stderr);
		if (!btr_validate_index(ibuf->index, NULL)) {
			ut_error;
		}

		fprintf(stderr, "InnoDB: ibuf tree ok\n");
		fflush(stderr);
		ut_ad(0);
	}

	return(FALSE);
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
	ulint		page_no,/*!< in: index page number that the record
				should belong to */
	btr_pcur_t*	pcur,	/*!< in: pcur positioned on the record to
				delete, having latch mode BTR_MODIFY_LEAF */
	const dtuple_t*	search_tuple,
				/*!< in: search tuple for entries of page_no */
	mtr_t*		mtr)	/*!< in: mtr */
{
	ibool		success;
	page_t*		root;
	ulint		err;

	ut_ad(ibuf_inside(mtr));
	ut_ad(page_rec_is_user_rec(btr_pcur_get_rec(pcur)));
	ut_ad(ibuf_rec_get_page_no(mtr, btr_pcur_get_rec(pcur)) == page_no);
	ut_ad(ibuf_rec_get_space(mtr, btr_pcur_get_rec(pcur)) == space);

	success = btr_cur_optimistic_delete(btr_pcur_get_btr_cur(pcur), mtr);

	if (success) {
		if (UNIV_UNLIKELY(!page_get_n_recs(btr_pcur_get_page(pcur)))) {
			/* If a B-tree page is empty, it must be the root page
			and the whole B-tree must be empty. InnoDB does not
			allow empty B-tree pages other than the root. */
			root = btr_pcur_get_page(pcur);

			ut_ad(page_get_space_id(root) == IBUF_SPACE_ID);
			ut_ad(page_get_page_no(root)
			      == FSP_IBUF_TREE_ROOT_PAGE_NO);

			/* ibuf->empty is protected by the root page latch.
			Before the deletion, it had to be FALSE. */
			ut_ad(!ibuf->empty);
			ibuf->empty = TRUE;
		}

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
	ut_ad(ibuf_rec_get_page_no(mtr, btr_pcur_get_rec(pcur)) == page_no);
	ut_ad(ibuf_rec_get_space(mtr, btr_pcur_get_rec(pcur)) == space);

	/* We have to resort to a pessimistic delete from ibuf */
	btr_pcur_store_position(pcur, mtr);
	ibuf_btr_pcur_commit_specify_mtr(pcur, mtr);

	ibuf_mtr_start(mtr);
	mutex_enter(&ibuf_mutex);

	if (!ibuf_restore_pos(space, page_no, search_tuple,
			      BTR_MODIFY_TREE, pcur, mtr)) {

		mutex_exit(&ibuf_mutex);
		ut_ad(!ibuf_inside(mtr));
		ut_ad(mtr->state == MTR_COMMITTED);
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
	mutex_exit(&ibuf_mutex);

	ibuf->empty = (page_get_n_recs(root) == 0);
	ibuf_btr_pcur_commit_specify_mtr(pcur, mtr);

func_exit:
	ut_ad(!ibuf_inside(mtr));
	ut_ad(mtr->state == MTR_COMMITTED);
	btr_pcur_close(pcur);

	return(TRUE);
}

/*********************************************************************//**
When an index page is read from a disk to the buffer pool, this function
applies any buffered operations to the page and deletes the entries from the
insert buffer. If the page is not read, but created in the buffer pool, this
function deletes its buffered entries from the insert buffer; there can
exist entries for such a page if the page belonged to an index which
subsequently was dropped. */
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
#ifdef UNIV_IBUF_DEBUG
	ulint		volume			= 0;
#endif
	page_zip_des_t*	page_zip		= NULL;
	ibool		tablespace_being_deleted = FALSE;
	ibool		corruption_noticed	= FALSE;
	mtr_t		mtr;

	/* Counts for merged & discarded operations. */
	ulint		mops[IBUF_OP_COUNT];
	ulint		dops[IBUF_OP_COUNT];

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
			ulint	bitmap_bits;

			ibuf_mtr_start(&mtr);

			bitmap_page = ibuf_bitmap_get_map_page(
				space, page_no, zip_size, &mtr);
			bitmap_bits = ibuf_bitmap_page_get_bits(
				bitmap_page, page_no, zip_size,
				IBUF_BITMAP_BUFFERED, &mtr);

			ibuf_mtr_commit(&mtr);

			if (!bitmap_bits) {
				/* No inserts buffered for this page */

				if (!tablespace_being_deleted) {
					fil_decr_pending_ops(space);
				}

				return;
			}
		}
	} else if (block
		   && (ibuf_fixed_addr_page(space, zip_size, page_no)
		      || fsp_descr_page(zip_size, page_no))) {

		return;
	}

	heap = mem_heap_create(512);

	if (UNIV_UNLIKELY(!trx_sys_multiple_tablespace_format)) {
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

			ibuf_mtr_start(&mtr);

			fputs("  InnoDB: Dump of the ibuf bitmap page:\n",
			      stderr);

			bitmap_page = ibuf_bitmap_get_map_page(space, page_no,
							       zip_size, &mtr);
			buf_page_print(bitmap_page, 0,
				       BUF_PAGE_PRINT_NO_CRASH);
			ibuf_mtr_commit(&mtr);

			fputs("\nInnoDB: Dump of the page:\n", stderr);

			buf_page_print(block->frame, 0,
				       BUF_PAGE_PRINT_NO_CRASH);

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
			ut_ad(0);
		}
	}

	memset(mops, 0, sizeof(mops));
	memset(dops, 0, sizeof(dops));

loop:
	ibuf_mtr_start(&mtr);

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
		if (ibuf_rec_get_page_no(&mtr, rec) != page_no
		    || ibuf_rec_get_space(&mtr, rec) != space) {

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
		} else if (block) {
			/* Now we have at pcur a record which should be
			applied on the index page; NOTE that the call below
			copies pointers to fields in rec, and we must
			keep the latch to the rec page until the
			insertion is finished! */
			dtuple_t*	entry;
			trx_id_t	max_trx_id;
			dict_index_t*	dummy_index;
			ibuf_op_t	op = ibuf_rec_get_op_type(&mtr, rec);

			max_trx_id = page_get_max_trx_id(page_align(rec));
			page_update_max_trx_id(block, page_zip, max_trx_id,
					       &mtr);

			ut_ad(page_validate(page_align(rec), ibuf->index));

			entry = ibuf_build_entry_from_ibuf_rec(
				&mtr, rec, heap, &dummy_index);

			ut_ad(page_validate(block->frame, dummy_index));

			switch (op) {
				ibool	success;
			case IBUF_OP_INSERT:
#ifdef UNIV_IBUF_DEBUG
				volume += rec_get_converted_size(
					dummy_index, entry, 0);

				volume += page_dir_calc_reserved_space(1);

				ut_a(volume <= 4 * UNIV_PAGE_SIZE
					/ IBUF_PAGE_SIZE_PER_FREE_SPACE);
#endif
				ibuf_insert_to_index_page(
					entry, block, dummy_index, &mtr);
				break;

			case IBUF_OP_DELETE_MARK:
				ibuf_set_del_mark(
					entry, block, dummy_index, &mtr);
				break;

			case IBUF_OP_DELETE:
				ibuf_delete(entry, block, dummy_index, &mtr);
				/* Because ibuf_delete() will latch an
				insert buffer bitmap page, commit mtr
				before latching any further pages.
				Store and restore the cursor position. */
				ut_ad(rec == btr_pcur_get_rec(&pcur));
				ut_ad(page_rec_is_user_rec(rec));
				ut_ad(ibuf_rec_get_page_no(&mtr, rec)
				      == page_no);
				ut_ad(ibuf_rec_get_space(&mtr, rec) == space);

				btr_pcur_store_position(&pcur, &mtr);
				ibuf_btr_pcur_commit_specify_mtr(&pcur, &mtr);

				ibuf_mtr_start(&mtr);

				success = buf_page_get_known_nowait(
					RW_X_LATCH, block,
					BUF_KEEP_OLD,
					__FILE__, __LINE__, &mtr);
				ut_a(success);

				/* This is a user page (secondary
				index leaf page), but it should be OK
				to use too low latching order for it,
				as the block is io-fixed. */
				buf_block_dbg_add_level(
					block, SYNC_IBUF_TREE_NODE);

				if (!ibuf_restore_pos(space, page_no,
						      search_tuple,
						      BTR_MODIFY_LEAF,
						      &pcur, &mtr)) {

					ut_ad(!ibuf_inside(&mtr));
					ut_ad(mtr.state == MTR_COMMITTED);
					mops[op]++;
					ibuf_dummy_index_free(dummy_index);
					goto loop;
				}

				break;
			default:
				ut_error;
			}

			mops[op]++;

			ibuf_dummy_index_free(dummy_index);
		} else {
			dops[ibuf_rec_get_op_type(&mtr, rec)]++;
		}

		/* Delete the record from ibuf */
		if (ibuf_delete_rec(space, page_no, &pcur, search_tuple,
				    &mtr)) {
			/* Deletion was pessimistic and mtr was committed:
			we start from the beginning again */

			goto loop;
		} else if (btr_pcur_is_after_last_on_page(&pcur)) {
			ibuf_mtr_commit(&mtr);
			btr_pcur_close(&pcur);

			goto loop;
		}
	}

reset_bit:
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

	ibuf_mtr_commit(&mtr);
	btr_pcur_close(&pcur);
	mem_heap_free(heap);

#ifdef HAVE_ATOMIC_BUILTINS
	os_atomic_increment_ulint(&ibuf->n_merges, 1);
	ibuf_add_ops(ibuf->n_merged_ops, mops);
	ibuf_add_ops(ibuf->n_discarded_ops, dops);
#else /* HAVE_ATOMIC_BUILTINS */
	/* Protect our statistics keeping from race conditions */
	mutex_enter(&ibuf_mutex);

	ibuf->n_merges++;
	ibuf_add_ops(ibuf->n_merged_ops, mops);
	ibuf_add_ops(ibuf->n_discarded_ops, dops);

	mutex_exit(&ibuf_mutex);
#endif /* HAVE_ATOMIC_BUILTINS */

	if (update_ibuf_bitmap && !tablespace_being_deleted) {

		fil_decr_pending_ops(space);
	}

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
	const rec_t*	ibuf_rec;
	ulint		page_no;
	mtr_t		mtr;

	/* Counts for discarded operations. */
	ulint		dops[IBUF_OP_COUNT];

	heap = mem_heap_create(512);

	/* Use page number 0 to build the search tuple so that we get the
	cursor positioned at the first entry for this space id */

	search_tuple = ibuf_new_search_tuple_build(space, 0, heap);

	memset(dops, 0, sizeof(dops));
loop:
	ibuf_mtr_start(&mtr);

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
		if (ibuf_rec_get_space(&mtr, ibuf_rec) != space) {

			goto leave_loop;
		}

		page_no = ibuf_rec_get_page_no(&mtr, ibuf_rec);

		dops[ibuf_rec_get_op_type(&mtr, ibuf_rec)]++;

		/* Delete the record from ibuf */
		if (ibuf_delete_rec(space, page_no, &pcur, search_tuple,
				    &mtr)) {
			/* Deletion was pessimistic and mtr was committed:
			we start from the beginning again */

			goto loop;
		}

		if (btr_pcur_is_after_last_on_page(&pcur)) {
			ibuf_mtr_commit(&mtr);
			btr_pcur_close(&pcur);

			goto loop;
		}
	}

leave_loop:
	ibuf_mtr_commit(&mtr);
	btr_pcur_close(&pcur);

#ifdef HAVE_ATOMIC_BUILTINS
	ibuf_add_ops(ibuf->n_discarded_ops, dops);
#else /* HAVE_ATOMIC_BUILTINS */
	/* Protect our statistics keeping from race conditions */
	mutex_enter(&ibuf_mutex);
	ibuf_add_ops(ibuf->n_discarded_ops, dops);
	mutex_exit(&ibuf_mutex);
#endif /* HAVE_ATOMIC_BUILTINS */

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

	ibuf_mtr_start(&mtr);

	mutex_enter(&ibuf_mutex);
	root = ibuf_tree_root_get(&mtr);
	mutex_exit(&ibuf_mutex);

	is_empty = (page_get_n_recs(root) == 0);
	ut_a(is_empty == ibuf->empty);
	ibuf_mtr_commit(&mtr);

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
		"Ibuf: size %lu, free list len %lu,"
		" seg size %lu, %lu merges\n",
		(ulong) ibuf->size,
		(ulong) ibuf->free_list_len,
		(ulong) ibuf->seg_size,
		(ulong) ibuf->n_merges);

	fputs("merged operations:\n ", file);
	ibuf_print_ops(ibuf->n_merged_ops, file);

	fputs("discarded operations:\n ", file);
	ibuf_print_ops(ibuf->n_discarded_ops, file);

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
