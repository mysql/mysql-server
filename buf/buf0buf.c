/*****************************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.
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
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file buf/buf0buf.c
The database buffer buf_pool

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#include "buf0buf.h"

#ifdef UNIV_NONINL
#include "buf0buf.ic"
#endif

#include "mem0mem.h"
#include "btr0btr.h"
#include "fil0fil.h"
#ifndef UNIV_HOTBACKUP
#include "buf0buddy.h"
#include "lock0lock.h"
#include "btr0sea.h"
#include "ibuf0ibuf.h"
#include "trx0undo.h"
#include "log0log.h"
#endif /* !UNIV_HOTBACKUP */
#include "srv0srv.h"
#include "dict0dict.h"
#include "log0recv.h"
#include "page0zip.h"
#include "trx0trx.h"
#include "srv0start.h"

/* prototypes for new functions added to ha_innodb.cc */
trx_t* innobase_get_trx();

static inline
void
_increment_page_get_statistics(buf_block_t* block, trx_t* trx)
{
	ulint           block_hash;
	ulint           block_hash_byte;
	byte            block_hash_offset;

	ut_ad(block);

	if (!innobase_get_slow_log() || !trx || !trx->take_stats)
		return;

	if (!trx->distinct_page_access_hash) {
		trx->distinct_page_access_hash = mem_alloc(DPAH_SIZE);
		memset(trx->distinct_page_access_hash, 0, DPAH_SIZE);
	}

	block_hash = ut_hash_ulint((block->page.space << 20) + block->page.space +
					block->page.offset, DPAH_SIZE << 3);
	block_hash_byte = block_hash >> 3;
	block_hash_offset = (byte) block_hash & 0x07;
	if (block_hash_byte >= DPAH_SIZE)
		fprintf(stderr, "!!! block_hash_byte = %lu  block_hash_offset = %d !!!\n", block_hash_byte, block_hash_offset);
	if (block_hash_offset > 7)
		fprintf(stderr, "!!! block_hash_byte = %lu  block_hash_offset = %d !!!\n", block_hash_byte, block_hash_offset);
	if ((trx->distinct_page_access_hash[block_hash_byte] & ((byte) 0x01 << block_hash_offset)) == 0)
		trx->distinct_page_access++;
	trx->distinct_page_access_hash[block_hash_byte] |= (byte) 0x01 << block_hash_offset;
	return;
}

/*
		IMPLEMENTATION OF THE BUFFER POOL
		=================================

Performance improvement:
------------------------
Thread scheduling in NT may be so slow that the OS wait mechanism should
not be used even in waiting for disk reads to complete.
Rather, we should put waiting query threads to the queue of
waiting jobs, and let the OS thread do something useful while the i/o
is processed. In this way we could remove most OS thread switches in
an i/o-intensive benchmark like TPC-C.

A possibility is to put a user space thread library between the database
and NT. User space thread libraries might be very fast.

SQL Server 7.0 can be configured to use 'fibers' which are lightweight
threads in NT. These should be studied.

		Buffer frames and blocks
		------------------------
Following the terminology of Gray and Reuter, we call the memory
blocks where file pages are loaded buffer frames. For each buffer
frame there is a control block, or shortly, a block, in the buffer
control array. The control info which does not need to be stored
in the file along with the file page, resides in the control block.

		Buffer pool struct
		------------------
The buffer buf_pool contains a single mutex which protects all the
control data structures of the buf_pool. The content of a buffer frame is
protected by a separate read-write lock in its control block, though.
These locks can be locked and unlocked without owning the buf_pool->mutex.
The OS events in the buf_pool struct can be waited for without owning the
buf_pool->mutex.

The buf_pool->mutex is a hot-spot in main memory, causing a lot of
memory bus traffic on multiprocessor systems when processors
alternately access the mutex. On our Pentium, the mutex is accessed
maybe every 10 microseconds. We gave up the solution to have mutexes
for each control block, for instance, because it seemed to be
complicated.

A solution to reduce mutex contention of the buf_pool->mutex is to
create a separate mutex for the page hash table. On Pentium,
accessing the hash table takes 2 microseconds, about half
of the total buf_pool->mutex hold time.

		Control blocks
		--------------

The control block contains, for instance, the bufferfix count
which is incremented when a thread wants a file page to be fixed
in a buffer frame. The bufferfix operation does not lock the
contents of the frame, however. For this purpose, the control
block contains a read-write lock.

The buffer frames have to be aligned so that the start memory
address of a frame is divisible by the universal page size, which
is a power of two.

We intend to make the buffer buf_pool size on-line reconfigurable,
that is, the buf_pool size can be changed without closing the database.
Then the database administarator may adjust it to be bigger
at night, for example. The control block array must
contain enough control blocks for the maximum buffer buf_pool size
which is used in the particular database.
If the buf_pool size is cut, we exploit the virtual memory mechanism of
the OS, and just refrain from using frames at high addresses. Then the OS
can swap them to disk.

The control blocks containing file pages are put to a hash table
according to the file address of the page.
We could speed up the access to an individual page by using
"pointer swizzling": we could replace the page references on
non-leaf index pages by direct pointers to the page, if it exists
in the buf_pool. We could make a separate hash table where we could
chain all the page references in non-leaf pages residing in the buf_pool,
using the page reference as the hash key,
and at the time of reading of a page update the pointers accordingly.
Drawbacks of this solution are added complexity and,
possibly, extra space required on non-leaf pages for memory pointers.
A simpler solution is just to speed up the hash table mechanism
in the database, using tables whose size is a power of 2.

		Lists of blocks
		---------------

There are several lists of control blocks.

The free list (buf_pool->free) contains blocks which are currently not
used.

The common LRU list contains all the blocks holding a file page
except those for which the bufferfix count is non-zero.
The pages are in the LRU list roughly in the order of the last
access to the page, so that the oldest pages are at the end of the
list. We also keep a pointer to near the end of the LRU list,
which we can use when we want to artificially age a page in the
buf_pool. This is used if we know that some page is not needed
again for some time: we insert the block right after the pointer,
causing it to be replaced sooner than would normally be the case.
Currently this aging mechanism is used for read-ahead mechanism
of pages, and it can also be used when there is a scan of a full
table which cannot fit in the memory. Putting the pages near the
end of the LRU list, we make sure that most of the buf_pool stays
in the main memory, undisturbed.

The unzip_LRU list contains a subset of the common LRU list.  The
blocks on the unzip_LRU list hold a compressed file page and the
corresponding uncompressed page frame.  A block is in unzip_LRU if and
only if the predicate buf_page_belongs_to_unzip_LRU(&block->page)
holds.  The blocks in unzip_LRU will be in same order as they are in
the common LRU list.  That is, each manipulation of the common LRU
list will result in the same manipulation of the unzip_LRU list.

The chain of modified blocks (buf_pool->flush_list) contains the blocks
holding file pages that have been modified in the memory
but not written to disk yet. The block with the oldest modification
which has not yet been written to disk is at the end of the chain.
The access to this list is protected by buf_pool->flush_list_mutex.

The chain of unmodified compressed blocks (buf_pool->zip_clean)
contains the control blocks (buf_page_t) of those compressed pages
that are not in buf_pool->flush_list and for which no uncompressed
page has been allocated in the buffer pool.  The control blocks for
uncompressed pages are accessible via buf_block_t objects that are
reachable via buf_pool->chunks[].

The chains of free memory blocks (buf_pool->zip_free[]) are used by
the buddy allocator (buf0buddy.c) to keep track of currently unused
memory blocks of size sizeof(buf_page_t)..UNIV_PAGE_SIZE / 2.  These
blocks are inside the UNIV_PAGE_SIZE-sized memory blocks of type
BUF_BLOCK_MEMORY that the buddy allocator requests from the buffer
pool.  The buddy allocator is solely used for allocating control
blocks for compressed pages (buf_page_t) and compressed page frames.

		Loading a file page
		-------------------

First, a victim block for replacement has to be found in the
buf_pool. It is taken from the free list or searched for from the
end of the LRU-list. An exclusive lock is reserved for the frame,
the io_fix field is set in the block fixing the block in buf_pool,
and the io-operation for loading the page is queued. The io-handler thread
releases the X-lock on the frame and resets the io_fix field
when the io operation completes.

A thread may request the above operation using the function
buf_page_get(). It may then continue to request a lock on the frame.
The lock is granted when the io-handler releases the x-lock.

		Read-ahead
		----------

The read-ahead mechanism is intended to be intelligent and
isolated from the semantically higher levels of the database
index management. From the higher level we only need the
information if a file page has a natural successor or
predecessor page. On the leaf level of a B-tree index,
these are the next and previous pages in the natural
order of the pages.

Let us first explain the read-ahead mechanism when the leafs
of a B-tree are scanned in an ascending or descending order.
When a read page is the first time referenced in the buf_pool,
the buffer manager checks if it is at the border of a so-called
linear read-ahead area. The tablespace is divided into these
areas of size 64 blocks, for example. So if the page is at the
border of such an area, the read-ahead mechanism checks if
all the other blocks in the area have been accessed in an
ascending or descending order. If this is the case, the system
looks at the natural successor or predecessor of the page,
checks if that is at the border of another area, and in this case
issues read-requests for all the pages in that area. Maybe
we could relax the condition that all the pages in the area
have to be accessed: if data is deleted from a table, there may
appear holes of unused pages in the area.

A different read-ahead mechanism is used when there appears
to be a random access pattern to a file.
If a new page is referenced in the buf_pool, and several pages
of its random access area (for instance, 32 consecutive pages
in a tablespace) have recently been referenced, we may predict
that the whole area may be needed in the near future, and issue
the read requests for the whole area.
*/

#ifndef UNIV_HOTBACKUP
/** Value in microseconds */
static const int WAIT_FOR_READ	= 5000;
/** Number of attemtps made to read in a page in the buffer pool */
static const ulint BUF_PAGE_READ_MAX_RETRIES = 100;

/** The buffer pools of the database */
UNIV_INTERN buf_pool_t*	buf_pool_ptr;

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
static ulint	buf_dbg_counter	= 0; /*!< This is used to insert validation
					operations in execution in the
					debug version */
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#ifdef UNIV_DEBUG
/** If this is set TRUE, the program prints info whenever
read-ahead or flush occurs */
UNIV_INTERN ibool		buf_debug_prints = FALSE;
#endif /* UNIV_DEBUG */

#ifdef UNIV_PFS_RWLOCK
/* Keys to register buffer block related rwlocks and mutexes with
performance schema */
UNIV_INTERN mysql_pfs_key_t	buf_pool_page_hash_key;
UNIV_INTERN mysql_pfs_key_t	buf_block_lock_key;
# ifdef UNIV_SYNC_DEBUG
UNIV_INTERN mysql_pfs_key_t	buf_block_debug_latch_key;
# endif /* UNIV_SYNC_DEBUG */
#endif /* UNIV_PFS_RWLOCK */

#ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	buffer_block_mutex_key;
UNIV_INTERN mysql_pfs_key_t	buf_pool_mutex_key;
UNIV_INTERN mysql_pfs_key_t	buf_pool_zip_mutex_key;
UNIV_INTERN mysql_pfs_key_t	buf_pool_LRU_list_mutex_key;
UNIV_INTERN mysql_pfs_key_t	buf_pool_free_list_mutex_key;
UNIV_INTERN mysql_pfs_key_t	buf_pool_zip_free_mutex_key;
UNIV_INTERN mysql_pfs_key_t	buf_pool_zip_hash_mutex_key;
UNIV_INTERN mysql_pfs_key_t	flush_list_mutex_key;
#endif /* UNIV_PFS_MUTEX */

#if defined UNIV_PFS_MUTEX || defined UNIV_PFS_RWLOCK
# ifndef PFS_SKIP_BUFFER_MUTEX_RWLOCK

/* Buffer block mutexes and rwlocks can be registered
in one group rather than individually. If PFS_GROUP_BUFFER_SYNC
is defined, register buffer block mutex and rwlock
in one group after their initialization. */
#  define PFS_GROUP_BUFFER_SYNC

/* This define caps the number of mutexes/rwlocks can
be registered with performance schema. Developers can
modify this define if necessary. Please note, this would
be effective only if PFS_GROUP_BUFFER_SYNC is defined. */
#  define PFS_MAX_BUFFER_MUTEX_LOCK_REGISTER	ULINT_MAX

# endif /* !PFS_SKIP_BUFFER_MUTEX_RWLOCK */
#endif /* UNIV_PFS_MUTEX || UNIV_PFS_RWLOCK */

/********************************************************************//**
Gets the smallest oldest_modification lsn for any page in the pool. Returns
zero if all modified pages have been flushed to disk.
@return oldest modification in pool, zero if none */
UNIV_INTERN
ib_uint64_t
buf_pool_get_oldest_modification(void)
/*==================================*/
{
	ulint		i;
	buf_page_t*	bpage;
	ib_uint64_t	lsn = 0;
	ib_uint64_t	oldest_lsn = 0;

	/* When we traverse all the flush lists we don't want another
	thread to add a dirty page to any flush list. */
	if (srv_buf_pool_instances > 1)
	log_flush_order_mutex_enter();

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_flush_list_mutex_enter(buf_pool);

		bpage = UT_LIST_GET_LAST(buf_pool->flush_list);

		if (bpage != NULL) {
			ut_ad(bpage->in_flush_list);
			lsn = bpage->oldest_modification;
		}

		buf_flush_list_mutex_exit(buf_pool);

		if (!oldest_lsn || oldest_lsn > lsn) {
			oldest_lsn = lsn;
		}
	}

	if (srv_buf_pool_instances > 1)
	log_flush_order_mutex_exit();

	/* The returned answer may be out of date: the flush_list can
	change after the mutex has been released. */

	return(oldest_lsn);
}

/********************************************************************//**
Get total buffer pool statistics. */
UNIV_INTERN
void
buf_get_total_list_len(
/*===================*/
	ulint*		LRU_len,	/*!< out: length of all LRU lists */
	ulint*		free_len,	/*!< out: length of all free lists */
	ulint*		flush_list_len)	/*!< out: length of all flush lists */
{
	ulint		i;

	*LRU_len = 0;
	*free_len = 0;
	*flush_list_len = 0;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);
		*LRU_len += UT_LIST_GET_LEN(buf_pool->LRU);
		*free_len += UT_LIST_GET_LEN(buf_pool->free);
		*flush_list_len += UT_LIST_GET_LEN(buf_pool->flush_list);
	}
}

/********************************************************************//**
Get total buffer pool statistics. */
UNIV_INTERN
void
buf_get_total_stat(
/*===============*/
	buf_pool_stat_t*	tot_stat)	/*!< out: buffer pool stats */
{
	ulint			i;

	memset(tot_stat, 0, sizeof(*tot_stat));

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_stat_t*buf_stat;
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_stat = &buf_pool->stat;
		tot_stat->n_page_gets += buf_stat->n_page_gets;
		tot_stat->n_pages_read += buf_stat->n_pages_read;
		tot_stat->n_pages_written += buf_stat->n_pages_written;
		tot_stat->n_pages_created += buf_stat->n_pages_created;
		tot_stat->n_ra_pages_read_rnd += buf_stat->n_ra_pages_read_rnd;
		tot_stat->n_ra_pages_read += buf_stat->n_ra_pages_read;
		tot_stat->n_ra_pages_evicted += buf_stat->n_ra_pages_evicted;
		tot_stat->n_pages_made_young += buf_stat->n_pages_made_young;

		tot_stat->n_pages_not_made_young +=
			buf_stat->n_pages_not_made_young;
	}
}

/********************************************************************//**
Allocates a buffer block.
@return own: the allocated block, in state BUF_BLOCK_MEMORY */
UNIV_INTERN
buf_block_t*
buf_block_alloc(
/*============*/
	buf_pool_t*	buf_pool)	/*!< in/out: buffer pool instance,
					or NULL for round-robin selection
					of the buffer pool */
{
	buf_block_t*	block;
	ulint		index;
	static ulint	buf_pool_index;

	if (buf_pool == NULL) {
		/* We are allocating memory from any buffer pool, ensure
		we spread the grace on all buffer pool instances. */
		index = buf_pool_index++ % srv_buf_pool_instances;
		buf_pool = buf_pool_from_array(index);
	}

	block = buf_LRU_get_free_block(buf_pool);

	buf_block_set_state(block, BUF_BLOCK_MEMORY);

	return(block);
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************************//**
Calculates a page checksum which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value on
32-bit and 64-bit architectures.
@return	checksum */
UNIV_INTERN
ulint
buf_calc_page_new_checksum(
/*=======================*/
	const byte*	page)	/*!< in: buffer page */
{
	ulint checksum;

	/* Since the field FIL_PAGE_FILE_FLUSH_LSN, and in versions <= 4.1.x
	..._ARCH_LOG_NO, are written outside the buffer pool to the first
	pages of data files, we have to skip them in the page checksum
	calculation.
	We must also skip the field FIL_PAGE_SPACE_OR_CHKSUM where the
	checksum is stored, and also the last 8 bytes of page because
	there we store the old formula checksum. */

	checksum = ut_fold_binary(page + FIL_PAGE_OFFSET,
				  FIL_PAGE_FILE_FLUSH_LSN - FIL_PAGE_OFFSET)
		+ ut_fold_binary(page + FIL_PAGE_DATA,
				 UNIV_PAGE_SIZE - FIL_PAGE_DATA
				 - FIL_PAGE_END_LSN_OLD_CHKSUM);
	checksum = checksum & 0xFFFFFFFFUL;

	return(checksum);
}

UNIV_INTERN
ulint
buf_calc_page_new_checksum_32(
/*==========================*/
	const byte*	page)	/*!< in: buffer page */
{
	ulint checksum;

	checksum = ut_fold_binary(page + FIL_PAGE_OFFSET,
				  FIL_PAGE_FILE_FLUSH_LSN - FIL_PAGE_OFFSET)
		+ ut_fold_binary(page + FIL_PAGE_DATA,
				 FIL_PAGE_DATA_ALIGN_32 - FIL_PAGE_DATA)
		+ ut_fold_binary_32(page + FIL_PAGE_DATA_ALIGN_32,
				    UNIV_PAGE_SIZE - FIL_PAGE_DATA_ALIGN_32
				    - FIL_PAGE_END_LSN_OLD_CHKSUM);

	checksum = checksum & 0xFFFFFFFFUL;

	return(checksum);
}

/********************************************************************//**
In versions < 4.0.14 and < 4.1.1 there was a bug that the checksum only
looked at the first few bytes of the page. This calculates that old
checksum.
NOTE: we must first store the new formula checksum to
FIL_PAGE_SPACE_OR_CHKSUM before calculating and storing this old checksum
because this takes that field as an input!
@return	checksum */
UNIV_INTERN
ulint
buf_calc_page_old_checksum(
/*=======================*/
	const byte*	page)	/*!< in: buffer page */
{
	ulint checksum;

	checksum = ut_fold_binary(page, FIL_PAGE_FILE_FLUSH_LSN);

	checksum = checksum & 0xFFFFFFFFUL;

	return(checksum);
}

/********************************************************************//**
Checks if a page is corrupt.
@return	TRUE if corrupted */
UNIV_INTERN
ibool
buf_page_is_corrupted(
/*==================*/
	const byte*	read_buf,	/*!< in: a database page */
	ulint		zip_size)	/*!< in: size of compressed page;
					0 for uncompressed pages */
{
	ulint		checksum_field;
	ulint		old_checksum_field;

	if (UNIV_LIKELY(!zip_size)
	    && memcmp(read_buf + FIL_PAGE_LSN + 4,
		      read_buf + UNIV_PAGE_SIZE
		      - FIL_PAGE_END_LSN_OLD_CHKSUM + 4, 4)) {

		/* Stored log sequence numbers at the start and the end
		of page do not match */

		return(TRUE);
	}

#ifndef UNIV_HOTBACKUP
	if (recv_lsn_checks_on) {
		ib_uint64_t	current_lsn;

		if (log_peek_lsn(&current_lsn)
		    && UNIV_UNLIKELY
		    (current_lsn
		     < mach_read_from_8(read_buf + FIL_PAGE_LSN))) {
			ut_print_timestamp(stderr);

			fprintf(stderr,
				"  InnoDB: Error: page %lu log sequence number"
				" %llu\n"
				"InnoDB: is in the future! Current system "
				"log sequence number %llu.\n"
				"InnoDB: Your database may be corrupt or "
				"you may have copied the InnoDB\n"
				"InnoDB: tablespace but not the InnoDB "
				"log files. See\n"
				"InnoDB: " REFMAN "forcing-innodb-recovery.html\n"
				"InnoDB: for more information.\n",
				(ulong) mach_read_from_4(read_buf
							 + FIL_PAGE_OFFSET),
				mach_read_from_8(read_buf + FIL_PAGE_LSN),
				current_lsn);
		}
	}
#endif

	/* If we use checksums validation, make additional check before
	returning TRUE to ensure that the checksum is not equal to
	BUF_NO_CHECKSUM_MAGIC which might be stored by InnoDB with checksums
	disabled. Otherwise, skip checksum calculation and return FALSE */

	if (UNIV_LIKELY(srv_use_checksums)) {
		checksum_field = mach_read_from_4(read_buf
						  + FIL_PAGE_SPACE_OR_CHKSUM);

		if (UNIV_UNLIKELY(zip_size)) {
			return(checksum_field != BUF_NO_CHECKSUM_MAGIC
			       && checksum_field
			       != page_zip_calc_checksum(read_buf, zip_size));
		}

		old_checksum_field = mach_read_from_4(
			read_buf + UNIV_PAGE_SIZE
			- FIL_PAGE_END_LSN_OLD_CHKSUM);

		/* There are 2 valid formulas for old_checksum_field:

		1. Very old versions of InnoDB only stored 8 byte lsn to the
		start and the end of the page.

		2. Newer InnoDB versions store the old formula checksum
		there. */

		if (old_checksum_field != mach_read_from_4(read_buf
							   + FIL_PAGE_LSN)
		    && old_checksum_field != BUF_NO_CHECKSUM_MAGIC
		    && old_checksum_field
		    != buf_calc_page_old_checksum(read_buf)) {

			return(TRUE);
		}

		/* InnoDB versions < 4.0.14 and < 4.1.1 stored the space id
		(always equal to 0), to FIL_PAGE_SPACE_OR_CHKSUM */

		if (!srv_fast_checksum
		    && checksum_field != 0
		    && checksum_field != BUF_NO_CHECKSUM_MAGIC
		    && checksum_field
		    != buf_calc_page_new_checksum(read_buf)) {

			return(TRUE);
		}

		if (srv_fast_checksum
		    && checksum_field != 0
		    && checksum_field != BUF_NO_CHECKSUM_MAGIC
		    && checksum_field
		    != buf_calc_page_new_checksum_32(read_buf)
		    && checksum_field
		    != buf_calc_page_new_checksum(read_buf)) {

			return(TRUE);
		}
	}

	return(FALSE);
}

/********************************************************************//**
Prints a page to stderr. */
UNIV_INTERN
void
buf_page_print(
/*===========*/
	const byte*	read_buf,	/*!< in: a database page */
	ulint		zip_size,	/*!< in: compressed page size, or
					0 for uncompressed pages */
	ulint		flags)		/*!< in: 0 or
					BUF_PAGE_PRINT_NO_CRASH or
					BUF_PAGE_PRINT_NO_FULL */

{
#ifndef UNIV_HOTBACKUP
	dict_index_t*	index;
#endif /* !UNIV_HOTBACKUP */
	ulint		checksum;
	ulint		checksum_32;
	ulint		old_checksum;
	ulint		size	= zip_size;

	if (!size) {
		size = UNIV_PAGE_SIZE;
	}

	if (!(flags & BUF_PAGE_PRINT_NO_FULL)) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Page dump in ascii and hex (%lu bytes):\n",
			(ulong) size);
		ut_print_buf(stderr, read_buf, size);
		fputs("\nInnoDB: End of page dump\n", stderr);
	}

	if (zip_size) {
		/* Print compressed page. */

		switch (fil_page_get_type(read_buf)) {
		case FIL_PAGE_TYPE_ZBLOB:
		case FIL_PAGE_TYPE_ZBLOB2:
			checksum = srv_use_checksums
				? page_zip_calc_checksum(read_buf, zip_size)
				: BUF_NO_CHECKSUM_MAGIC;
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Compressed BLOB page"
				" checksum %lu, stored %lu\n"
				"InnoDB: Page lsn %lu %lu\n"
				"InnoDB: Page number (if stored"
				" to page already) %lu,\n"
				"InnoDB: space id (if stored"
				" to page already) %lu\n",
				(ulong) checksum,
				(ulong) mach_read_from_4(
					read_buf + FIL_PAGE_SPACE_OR_CHKSUM),
				(ulong) mach_read_from_4(
					read_buf + FIL_PAGE_LSN),
				(ulong) mach_read_from_4(
					read_buf + (FIL_PAGE_LSN + 4)),
				(ulong) mach_read_from_4(
					read_buf + FIL_PAGE_OFFSET),
				(ulong) mach_read_from_4(
					read_buf
					+ FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID));
			return;
		default:
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: unknown page type %lu,"
				" assuming FIL_PAGE_INDEX\n",
				fil_page_get_type(read_buf));
			/* fall through */
		case FIL_PAGE_INDEX:
			checksum = srv_use_checksums
				? page_zip_calc_checksum(read_buf, zip_size)
				: BUF_NO_CHECKSUM_MAGIC;

			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Compressed page checksum %lu,"
				" stored %lu\n"
				"InnoDB: Page lsn %lu %lu\n"
				"InnoDB: Page number (if stored"
				" to page already) %lu,\n"
				"InnoDB: space id (if stored"
				" to page already) %lu\n",
				(ulong) checksum,
				(ulong) mach_read_from_4(
					read_buf + FIL_PAGE_SPACE_OR_CHKSUM),
				(ulong) mach_read_from_4(
					read_buf + FIL_PAGE_LSN),
				(ulong) mach_read_from_4(
					read_buf + (FIL_PAGE_LSN + 4)),
				(ulong) mach_read_from_4(
					read_buf + FIL_PAGE_OFFSET),
				(ulong) mach_read_from_4(
					read_buf
					+ FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID));
			return;
		case FIL_PAGE_TYPE_XDES:
			/* This is an uncompressed page. */
			break;
		}
	}

	checksum = srv_use_checksums
		? buf_calc_page_new_checksum(read_buf) : BUF_NO_CHECKSUM_MAGIC;
	checksum_32 = srv_use_checksums
		? buf_calc_page_new_checksum_32(read_buf) : BUF_NO_CHECKSUM_MAGIC;
	old_checksum = srv_use_checksums
		? buf_calc_page_old_checksum(read_buf) : BUF_NO_CHECKSUM_MAGIC;

	ut_print_timestamp(stderr);
	fprintf(stderr,
		"  InnoDB: Page checksum %lu (32bit_calc: %lu), prior-to-4.0.14-form"
		" checksum %lu\n"
		"InnoDB: stored checksum %lu, prior-to-4.0.14-form"
		" stored checksum %lu\n"
		"InnoDB: Page lsn %lu %lu, low 4 bytes of lsn"
		" at page end %lu\n"
		"InnoDB: Page number (if stored to page already) %lu,\n"
		"InnoDB: space id (if created with >= MySQL-4.1.1"
		" and stored already) %lu\n",
		(ulong) checksum, (ulong) checksum_32, (ulong) old_checksum,
		(ulong) mach_read_from_4(read_buf + FIL_PAGE_SPACE_OR_CHKSUM),
		(ulong) mach_read_from_4(read_buf + UNIV_PAGE_SIZE
					 - FIL_PAGE_END_LSN_OLD_CHKSUM),
		(ulong) mach_read_from_4(read_buf + FIL_PAGE_LSN),
		(ulong) mach_read_from_4(read_buf + FIL_PAGE_LSN + 4),
		(ulong) mach_read_from_4(read_buf + UNIV_PAGE_SIZE
					 - FIL_PAGE_END_LSN_OLD_CHKSUM + 4),
		(ulong) mach_read_from_4(read_buf + FIL_PAGE_OFFSET),
		(ulong) mach_read_from_4(read_buf
					 + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID));

#ifndef UNIV_HOTBACKUP
	if (mach_read_from_2(read_buf + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_TYPE)
	    == TRX_UNDO_INSERT) {
		fprintf(stderr,
			"InnoDB: Page may be an insert undo log page\n");
	} else if (mach_read_from_2(read_buf + TRX_UNDO_PAGE_HDR
				    + TRX_UNDO_PAGE_TYPE)
		   == TRX_UNDO_UPDATE) {
		fprintf(stderr,
			"InnoDB: Page may be an update undo log page\n");
	}
#endif /* !UNIV_HOTBACKUP */

	switch (fil_page_get_type(read_buf)) {
		index_id_t	index_id;
	case FIL_PAGE_INDEX:
		index_id = btr_page_get_index_id(read_buf);
		fprintf(stderr,
			"InnoDB: Page may be an index page where"
			" index id is %llu\n",
			(ullint) index_id);
#ifndef UNIV_HOTBACKUP
		index = dict_index_find_on_id_low(index_id);
		if (index) {
			fputs("InnoDB: (", stderr);
			dict_index_name_print(stderr, NULL, index);
			fputs(")\n", stderr);
		}
#endif /* !UNIV_HOTBACKUP */
		break;
	case FIL_PAGE_INODE:
		fputs("InnoDB: Page may be an 'inode' page\n", stderr);
		break;
	case FIL_PAGE_IBUF_FREE_LIST:
		fputs("InnoDB: Page may be an insert buffer free list page\n",
		      stderr);
		break;
	case FIL_PAGE_TYPE_ALLOCATED:
		fputs("InnoDB: Page may be a freshly allocated page\n",
		      stderr);
		break;
	case FIL_PAGE_IBUF_BITMAP:
		fputs("InnoDB: Page may be an insert buffer bitmap page\n",
		      stderr);
		break;
	case FIL_PAGE_TYPE_SYS:
		fputs("InnoDB: Page may be a system page\n",
		      stderr);
		break;
	case FIL_PAGE_TYPE_TRX_SYS:
		fputs("InnoDB: Page may be a transaction system page\n",
		      stderr);
		break;
	case FIL_PAGE_TYPE_FSP_HDR:
		fputs("InnoDB: Page may be a file space header page\n",
		      stderr);
		break;
	case FIL_PAGE_TYPE_XDES:
		fputs("InnoDB: Page may be an extent descriptor page\n",
		      stderr);
		break;
	case FIL_PAGE_TYPE_BLOB:
		fputs("InnoDB: Page may be a BLOB page\n",
		      stderr);
		break;
	case FIL_PAGE_TYPE_ZBLOB:
	case FIL_PAGE_TYPE_ZBLOB2:
		fputs("InnoDB: Page may be a compressed BLOB page\n",
		      stderr);
		break;
	}

	ut_ad(flags & BUF_PAGE_PRINT_NO_CRASH);
}

#ifndef UNIV_HOTBACKUP

# ifdef PFS_GROUP_BUFFER_SYNC
/********************************************************************//**
This function registers mutexes and rwlocks in buffer blocks with
performance schema. If PFS_MAX_BUFFER_MUTEX_LOCK_REGISTER is
defined to be a value less than chunk->size, then only mutexes
and rwlocks in the first PFS_MAX_BUFFER_MUTEX_LOCK_REGISTER
blocks are registered. */
static
void
pfs_register_buffer_block(
/*======================*/
	buf_chunk_t*	chunk)		/*!< in/out: chunk of buffers */
{
	ulint		i;
	ulint		num_to_register;
	buf_block_t*    block;

	block = chunk->blocks;

	num_to_register = ut_min(chunk->size,
				 PFS_MAX_BUFFER_MUTEX_LOCK_REGISTER);

	for (i = 0; i < num_to_register; i++) {
		mutex_t*	mutex;
		rw_lock_t*	rwlock;

#  ifdef UNIV_PFS_MUTEX
		mutex = &block->mutex;
		ut_a(!mutex->pfs_psi);
		mutex->pfs_psi = (PSI_server)
			? PSI_server->init_mutex(buffer_block_mutex_key, mutex)
			: NULL;
#  endif /* UNIV_PFS_MUTEX */

#  ifdef UNIV_PFS_RWLOCK
		rwlock = &block->lock;
		ut_a(!rwlock->pfs_psi);
		rwlock->pfs_psi = (PSI_server)
			? PSI_server->init_rwlock(buf_block_lock_key, rwlock)
			: NULL;

#   ifdef UNIV_SYNC_DEBUG
		rwlock = &block->debug_latch;
		ut_a(!rwlock->pfs_psi);
		rwlock->pfs_psi = (PSI_server)
			? PSI_server->init_rwlock(buf_block_debug_latch_key,
						  rwlock)
			: NULL;
#   endif /* UNIV_SYNC_DEBUG */

#  endif /* UNIV_PFS_RWLOCK */
		block++;
	}
}
# endif /* PFS_GROUP_BUFFER_SYNC */

/********************************************************************//**
Initializes a buffer control block when the buf_pool is created. */
static
void
buf_block_init(
/*===========*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	buf_block_t*	block,		/*!< in: pointer to control block */
	byte*		frame)		/*!< in: pointer to buffer frame */
{
	UNIV_MEM_DESC(frame, UNIV_PAGE_SIZE, block);

	block->frame = frame;

	block->page.buf_pool_index = buf_pool_index(buf_pool);
	block->page.state = BUF_BLOCK_NOT_USED;
	block->page.buf_fix_count = 0;
	block->page.io_fix = BUF_IO_NONE;

	block->modify_clock = 0;

#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	block->page.file_page_was_freed = FALSE;
#endif /* UNIV_DEBUG_FILE_ACCESSES || UNIV_DEBUG */

	block->check_index_page_at_flush = FALSE;
	block->index = NULL;
	block->btr_search_latch = NULL;

#ifdef UNIV_DEBUG
	block->page.in_page_hash = FALSE;
	block->page.in_zip_hash = FALSE;
	block->page.in_flush_list = FALSE;
	block->page.in_free_list = FALSE;
#endif /* UNIV_DEBUG */
	block->page.flush_list.prev = NULL;
	block->page.flush_list.next = NULL;
	block->page.zip_list.prev = NULL;
	block->page.zip_list.next = NULL;
	block->page.in_LRU_list = FALSE;
	block->in_unzip_LRU_list = FALSE;
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	block->n_pointers = 0;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	page_zip_des_init(&block->page.zip);

#if defined PFS_SKIP_BUFFER_MUTEX_RWLOCK || defined PFS_GROUP_BUFFER_SYNC
	/* If PFS_SKIP_BUFFER_MUTEX_RWLOCK is defined, skip registration
	of buffer block mutex/rwlock with performance schema. If
	PFS_GROUP_BUFFER_SYNC is defined, skip the registration
	since buffer block mutex/rwlock will be registered later in
	pfs_register_buffer_block() */

	mutex_create(PFS_NOT_INSTRUMENTED, &block->mutex, SYNC_BUF_BLOCK);
	rw_lock_create(PFS_NOT_INSTRUMENTED, &block->lock, SYNC_LEVEL_VARYING);

# ifdef UNIV_SYNC_DEBUG
	rw_lock_create(PFS_NOT_INSTRUMENTED,
		       &block->debug_latch, SYNC_NO_ORDER_CHECK);
# endif /* UNIV_SYNC_DEBUG */

#else /* PFS_SKIP_BUFFER_MUTEX_RWLOCK || PFS_GROUP_BUFFER_SYNC */
	mutex_create(buffer_block_mutex_key, &block->mutex, SYNC_BUF_BLOCK);
	rw_lock_create(buf_block_lock_key, &block->lock, SYNC_LEVEL_VARYING);

# ifdef UNIV_SYNC_DEBUG
	rw_lock_create(buf_block_debug_latch_key,
		       &block->debug_latch, SYNC_NO_ORDER_CHECK);
# endif /* UNIV_SYNC_DEBUG */
#endif /* PFS_SKIP_BUFFER_MUTEX_RWLOCK || PFS_GROUP_BUFFER_SYNC */

	ut_ad(rw_lock_validate(&(block->lock)));

}

/********************************************************************//**
Allocates a chunk of buffer frames.
@return	chunk, or NULL on failure */
static
buf_chunk_t*
buf_chunk_init(
/*===========*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	buf_chunk_t*	chunk,		/*!< out: chunk of buffers */
	ulint		mem_size,	/*!< in: requested size in bytes */
	ibool		populate)	/*!< in: virtual page preallocation */
{
	buf_block_t*	block;
	byte*		frame;
	ulint		i;
	ulint		size_target;

	/* Round down to a multiple of page size,
	although it already should be. */
	mem_size = ut_2pow_round(mem_size, UNIV_PAGE_SIZE);
	size_target = (mem_size / UNIV_PAGE_SIZE) - 1;
	/* Reserve space for the block descriptors. */
	mem_size += ut_2pow_round((mem_size / UNIV_PAGE_SIZE) * (sizeof *block)
				  + (UNIV_PAGE_SIZE - 1), UNIV_PAGE_SIZE);

	chunk->mem_size = mem_size;
	chunk->mem = os_mem_alloc_large(&chunk->mem_size, populate);

	if (UNIV_UNLIKELY(chunk->mem == NULL)) {

		return(NULL);
	}

	/* Allocate the block descriptors from
	the start of the memory block. */
	chunk->blocks = chunk->mem;

	/* Align a pointer to the first frame.  Note that when
	os_large_page_size is smaller than UNIV_PAGE_SIZE,
	we may allocate one fewer block than requested.  When
	it is bigger, we may allocate more blocks than requested. */

	frame = ut_align(chunk->mem, UNIV_PAGE_SIZE);
	chunk->size = chunk->mem_size / UNIV_PAGE_SIZE
		- (frame != chunk->mem);

	/* Subtract the space needed for block descriptors. */
	{
		ulint	size = chunk->size;

		while (frame < (byte*) (chunk->blocks + size)) {
			frame += UNIV_PAGE_SIZE;
			size--;
		}

		chunk->size = size;
	}

	if (chunk->size > size_target) {
		chunk->size = size_target;
	}

	/* Init block structs and assign frames for them. Then we
	assign the frames to the first blocks (we already mapped the
	memory above). */

	block = chunk->blocks;

	for (i = chunk->size; i--; ) {

		buf_block_init(buf_pool, block, frame);
		UNIV_MEM_INVALID(block->frame, UNIV_PAGE_SIZE);

		/* Add the block to the free list */
		mutex_enter(&buf_pool->free_list_mutex);
		UT_LIST_ADD_LAST(free, buf_pool->free, (&block->page));

		ut_d(block->page.in_free_list = TRUE);
		mutex_exit(&buf_pool->free_list_mutex);
		ut_ad(buf_pool_from_block(block) == buf_pool);

		block++;
		frame += UNIV_PAGE_SIZE;
	}

#ifdef PFS_GROUP_BUFFER_SYNC
	pfs_register_buffer_block(chunk);
#endif
	return(chunk);
}

#ifdef UNIV_DEBUG
/*********************************************************************//**
Finds a block in the given buffer chunk that points to a
given compressed page.
@return	buffer block pointing to the compressed page, or NULL */
static
buf_block_t*
buf_chunk_contains_zip(
/*===================*/
	buf_chunk_t*	chunk,	/*!< in: chunk being checked */
	const void*	data)	/*!< in: pointer to compressed page */
{
	buf_block_t*	block;
	ulint		i;

	block = chunk->blocks;

	for (i = chunk->size; i--; block++) {
		if (block->page.zip.data == data) {

			return(block);
		}
	}

	return(NULL);
}

/*********************************************************************//**
Finds a block in the buffer pool that points to a
given compressed page.
@return	buffer block pointing to the compressed page, or NULL */
UNIV_INTERN
buf_block_t*
buf_pool_contains_zip(
/*==================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	const void*	data)		/*!< in: pointer to compressed page */
{
	ulint		n;
	buf_chunk_t*	chunk = buf_pool->chunks;

	ut_ad(buf_pool);
	//ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(mutex_own(&buf_pool->zip_free_mutex));
	for (n = buf_pool->n_chunks; n--; chunk++) {

		buf_block_t* block = buf_chunk_contains_zip(chunk, data);

		if (block) {
			return(block);
		}
	}

	return(NULL);
}
#endif /* UNIV_DEBUG */

/*********************************************************************//**
Checks that all file pages in the buffer chunk are in a replaceable state.
@return	address of a non-free block, or NULL if all freed */
static
const buf_block_t*
buf_chunk_not_freed(
/*================*/
	buf_chunk_t*	chunk)	/*!< in: chunk being checked */
{
	buf_block_t*	block;
	ulint		i;

	block = chunk->blocks;

	for (i = chunk->size; i--; block++) {
		ibool	ready;

		switch (buf_block_get_state(block)) {
		case BUF_BLOCK_ZIP_FREE:
		case BUF_BLOCK_ZIP_PAGE:
		case BUF_BLOCK_ZIP_DIRTY:
			/* The uncompressed buffer pool should never
			contain compressed block descriptors. */
			ut_error;
			break;
		case BUF_BLOCK_NOT_USED:
		case BUF_BLOCK_READY_FOR_USE:
		case BUF_BLOCK_MEMORY:
		case BUF_BLOCK_REMOVE_HASH:
			/* Skip blocks that are not being used for
			file pages. */
			break;
		case BUF_BLOCK_FILE_PAGE:
			mutex_enter(&block->mutex);
			ready = buf_flush_ready_for_replace(&block->page);
			mutex_exit(&block->mutex);

			if (block->page.is_corrupt) {
				/* corrupt page may remain, it can be skipped */
				break;
			}

			if (!ready) {

				return(block);
			}

			break;
		}
	}

	return(NULL);
}

/********************************************************************//**
Set buffer pool size variables after resizing it */
static
void
buf_pool_set_sizes(void)
/*====================*/
{
	ulint	i;
	ulint	curr_size = 0;

	buf_pool_mutex_enter_all();

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);
		curr_size += buf_pool->curr_pool_size;
	}

	srv_buf_pool_curr_size = curr_size;
	srv_buf_pool_old_size = srv_buf_pool_size;

	buf_pool_mutex_exit_all();
}

/********************************************************************//**
Initialize a buffer pool instance.
@return DB_SUCCESS if all goes well. */
UNIV_INTERN
ulint
buf_pool_init_instance(
/*===================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ulint		buf_pool_size,	/*!< in: size in bytes */
	ibool		populate,	/*!< in: virtual page preallocation */
	ulint		instance_no)	/*!< in: id of the instance */
{
	ulint		i;
	buf_chunk_t*	chunk;

	/* 1. Initialize general fields
	------------------------------- */
	mutex_create(buf_pool_mutex_key,
		     &buf_pool->mutex, SYNC_BUF_POOL);
	mutex_create(buf_pool_LRU_list_mutex_key,
		     &buf_pool->LRU_list_mutex, SYNC_BUF_LRU_LIST);
	rw_lock_create(buf_pool_page_hash_key,
		       &buf_pool->page_hash_latch, SYNC_BUF_PAGE_HASH);
	mutex_create(buf_pool_free_list_mutex_key,
		     &buf_pool->free_list_mutex, SYNC_BUF_FREE_LIST);
	mutex_create(buf_pool_zip_free_mutex_key,
		     &buf_pool->zip_free_mutex, SYNC_BUF_ZIP_FREE);
	mutex_create(buf_pool_zip_hash_mutex_key,
		     &buf_pool->zip_hash_mutex, SYNC_BUF_ZIP_HASH);
	mutex_create(buf_pool_zip_mutex_key,
		     &buf_pool->zip_mutex, SYNC_BUF_BLOCK);

	mutex_enter(&buf_pool->LRU_list_mutex);
	rw_lock_x_lock(&buf_pool->page_hash_latch);
	buf_pool_mutex_enter(buf_pool);

	if (buf_pool_size > 0) {
		buf_pool->n_chunks = 1;
		buf_pool->chunks = chunk = mem_zalloc(sizeof *chunk);

		UT_LIST_INIT(buf_pool->free);

		if (!buf_chunk_init(buf_pool, chunk, buf_pool_size, populate)) {
			mem_free(chunk);
			mem_free(buf_pool);

			mutex_exit(&buf_pool->LRU_list_mutex);
			rw_lock_x_unlock(&buf_pool->page_hash_latch);
			buf_pool_mutex_exit(buf_pool);

			return(DB_ERROR);
		}

		buf_pool->instance_no = instance_no;
		buf_pool->old_pool_size = buf_pool_size;
		buf_pool->curr_size = chunk->size;
		buf_pool->curr_pool_size = buf_pool->curr_size * UNIV_PAGE_SIZE;

		buf_pool->page_hash = hash_create(2 * buf_pool->curr_size);
		buf_pool->zip_hash = hash_create(2 * buf_pool->curr_size);

		buf_pool->last_printout_time = ut_time();
	}
	/* 2. Initialize flushing fields
	-------------------------------- */

	mutex_create(flush_list_mutex_key, &buf_pool->flush_list_mutex,
		     SYNC_BUF_FLUSH_LIST);

	for (i = BUF_FLUSH_LRU; i < BUF_FLUSH_N_TYPES; i++) {
		buf_pool->no_flush[i] = os_event_create(NULL);
	}

	/* 3. Initialize LRU fields
	--------------------------- */

	/* All fields are initialized by mem_zalloc(). */

	mutex_exit(&buf_pool->LRU_list_mutex);
	rw_lock_x_unlock(&buf_pool->page_hash_latch);
	buf_pool_mutex_exit(buf_pool);

	return(DB_SUCCESS);
}

/********************************************************************//**
free one buffer pool instance */
static
void
buf_pool_free_instance(
/*===================*/
	buf_pool_t*	buf_pool)	/* in,own: buffer pool instance
					to free */
{
	buf_chunk_t*	chunk;
	buf_chunk_t*	chunks;
	buf_page_t*	bpage;

	bpage = UT_LIST_GET_LAST(buf_pool->LRU);
	while (bpage != NULL) {
		buf_page_t*	prev_bpage = UT_LIST_GET_PREV(LRU, bpage);
		enum buf_page_state	state = buf_page_get_state(bpage);

		ut_ad(buf_page_in_file(bpage));
		ut_ad(bpage->in_LRU_list);

		if (state != BUF_BLOCK_FILE_PAGE) {
			/* We must not have any dirty block except
			when doing a fast shutdown. */
			ut_ad(state == BUF_BLOCK_ZIP_PAGE
			      || srv_fast_shutdown == 2);
			buf_page_free_descriptor(bpage);
		}

		bpage = prev_bpage;
	}

	chunks = buf_pool->chunks;
	chunk = chunks + buf_pool->n_chunks;

	while (--chunk >= chunks) {
		os_mem_free_large(chunk->mem, chunk->mem_size);
	}

	mem_free(buf_pool->chunks);
	hash_table_free(buf_pool->page_hash);
	hash_table_free(buf_pool->zip_hash);
}

/********************************************************************//**
Creates the buffer pool.
@return	DB_SUCCESS if success, DB_ERROR if not enough memory or error */
UNIV_INTERN
ulint
buf_pool_init(
/*==========*/
	ulint	total_size,	/*!< in: size of the total pool in bytes */
	ibool	populate,	/*!< in: virtual page preallocation */
	ulint	n_instances)	/*!< in: number of instances */
{
	ulint		i;
	const ulint	size	= total_size / n_instances;

	ut_ad(n_instances > 0);
	ut_ad(n_instances <= MAX_BUFFER_POOLS);
	ut_ad(n_instances == srv_buf_pool_instances);

	/* We create an extra buffer pool instance, this instance is used
	for flushing the flush lists, to keep track of n_flush for all
	the buffer pools and also used as a waiting object during flushing. */
	buf_pool_ptr = mem_zalloc(n_instances * sizeof *buf_pool_ptr);

	for (i = 0; i < n_instances; i++) {
		buf_pool_t*	ptr	= &buf_pool_ptr[i];

		if (buf_pool_init_instance(ptr, size, populate, i) != DB_SUCCESS) {

			/* Free all the instances created so far. */
			buf_pool_free(i);

			return(DB_ERROR);
		}
	}

	buf_pool_set_sizes();
	buf_LRU_old_ratio_update(100 * 3/ 8, FALSE);

	btr_search_sys_create(buf_pool_get_curr_size() / sizeof(void*) / 64);

	return(DB_SUCCESS);
}

/********************************************************************//**
Frees the buffer pool at shutdown.  This must not be invoked before
freeing all mutexes. */
UNIV_INTERN
void
buf_pool_free(
/*==========*/
	ulint	n_instances)	/*!< in: numbere of instances to free */
{
	ulint	i;

	for (i = 0; i < n_instances; i++) {
		buf_pool_free_instance(buf_pool_from_array(i));
	}

	mem_free(buf_pool_ptr);
	buf_pool_ptr = NULL;
}

/********************************************************************//**
Clears the adaptive hash index on all pages in the buffer pool. */
UNIV_INTERN
void
buf_pool_clear_hash_index(void)
/*===========================*/
{
	ulint	p;

#ifdef UNIV_SYNC_DEBUG
	ulint	j;

	for (j = 0; j < btr_search_index_num; j++) {
		ut_ad(rw_lock_own(btr_search_latch_part[j], RW_LOCK_EX));
	}
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(!btr_search_enabled);

	for (p = 0; p < srv_buf_pool_instances; p++) {
		buf_pool_t*	buf_pool = buf_pool_from_array(p);
		buf_chunk_t*	chunks	= buf_pool->chunks;
		buf_chunk_t*	chunk	= chunks + buf_pool->n_chunks;

		while (--chunk >= chunks) {
			buf_block_t*	block	= chunk->blocks;
			ulint		i	= chunk->size;

			for (; i--; block++) {
				dict_index_t*	index	= block->index;

				/* We can set block->index = NULL
				when we have an x-latch on btr_search_latch;
				see the comment in buf0buf.h */

				if (!index) {
					/* Not hashed */
					continue;
				}

				block->index = NULL;
# if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
				block->n_pointers = 0;
# endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
			}
		}
	}
}

/********************************************************************//**
Relocate a buffer control block.  Relocates the block on the LRU list
and in buf_pool->page_hash.  Does not relocate bpage->list.
The caller must take care of relocating bpage->list. */
UNIV_INTERN
void
buf_relocate(
/*=========*/
	buf_page_t*	bpage,	/*!< in/out: control block being relocated;
				buf_page_get_state(bpage) must be
				BUF_BLOCK_ZIP_DIRTY or BUF_BLOCK_ZIP_PAGE */
	buf_page_t*	dpage)	/*!< in/out: destination control block */
{
	buf_page_t*	b;
	ulint		fold;
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);

	//ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(mutex_own(&buf_pool->LRU_list_mutex));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&buf_pool->page_hash_latch, RW_LOCK_EX));
#endif
	ut_ad(mutex_own(buf_page_get_mutex(bpage)));
	ut_a(buf_page_get_io_fix(bpage) == BUF_IO_NONE);
	ut_a(bpage->buf_fix_count == 0);
	ut_ad(bpage->in_LRU_list);
	ut_ad(!bpage->in_zip_hash);
	ut_ad(bpage->in_page_hash);
	ut_ad(bpage == buf_page_hash_get(buf_pool,
			       		 bpage->space, bpage->offset));
	ut_ad(!buf_pool_watch_is_sentinel(buf_pool, bpage));
#ifdef UNIV_DEBUG
	switch (buf_page_get_state(bpage)) {
	case BUF_BLOCK_ZIP_FREE:
	case BUF_BLOCK_NOT_USED:
	case BUF_BLOCK_READY_FOR_USE:
	case BUF_BLOCK_FILE_PAGE:
	case BUF_BLOCK_MEMORY:
	case BUF_BLOCK_REMOVE_HASH:
		ut_error;
	case BUF_BLOCK_ZIP_DIRTY:
	case BUF_BLOCK_ZIP_PAGE:
		break;
	}
#endif /* UNIV_DEBUG */

	memcpy(dpage, bpage, sizeof *dpage);

	bpage->in_LRU_list = FALSE;
	ut_d(bpage->in_page_hash = FALSE);

	/* relocate buf_pool->LRU */
	b = UT_LIST_GET_PREV(LRU, bpage);
	UT_LIST_REMOVE(LRU, buf_pool->LRU, bpage);

	if (b) {
		UT_LIST_INSERT_AFTER(LRU, buf_pool->LRU, b, dpage);
	} else {
		UT_LIST_ADD_FIRST(LRU, buf_pool->LRU, dpage);
	}

	if (UNIV_UNLIKELY(buf_pool->LRU_old == bpage)) {
		buf_pool->LRU_old = dpage;
#ifdef UNIV_LRU_DEBUG
		/* buf_pool->LRU_old must be the first item in the LRU list
		whose "old" flag is set. */
		ut_a(buf_pool->LRU_old->old);
		ut_a(!UT_LIST_GET_PREV(LRU, buf_pool->LRU_old)
		     || !UT_LIST_GET_PREV(LRU, buf_pool->LRU_old)->old);
		ut_a(!UT_LIST_GET_NEXT(LRU, buf_pool->LRU_old)
		     || UT_LIST_GET_NEXT(LRU, buf_pool->LRU_old)->old);
	} else {
		/* Check that the "old" flag is consistent in
		the block and its neighbours. */
		buf_page_set_old(dpage, buf_page_is_old(dpage));
#endif /* UNIV_LRU_DEBUG */
	}

	ut_d(UT_LIST_VALIDATE(LRU, buf_page_t, buf_pool->LRU,
			      ut_ad(ut_list_node_313->in_LRU_list)));

	/* relocate buf_pool->page_hash */
	fold = buf_page_address_fold(bpage->space, bpage->offset);

	HASH_DELETE(buf_page_t, hash, buf_pool->page_hash, fold, bpage);
	HASH_INSERT(buf_page_t, hash, buf_pool->page_hash, fold, dpage);
}

/********************************************************************//**
Determine if a block is a sentinel for a buffer pool watch.
@return	TRUE if a sentinel for a buffer pool watch, FALSE if not */
UNIV_INTERN
ibool
buf_pool_watch_is_sentinel(
/*=======================*/
	buf_pool_t*		buf_pool,	/*!< buffer pool instance */
	const buf_page_t*	bpage)		/*!< in: block */
{
	ut_ad(buf_page_in_file(bpage));

	if (bpage < &buf_pool->watch[0]
	    || bpage >= &buf_pool->watch[BUF_POOL_WATCH_SIZE]) {

		ut_ad(buf_page_get_state(bpage) != BUF_BLOCK_ZIP_PAGE
		      || bpage->zip.data != NULL);

		return(FALSE);
	}

	ut_ad(buf_page_get_state(bpage) == BUF_BLOCK_ZIP_PAGE);
	ut_ad(!bpage->in_zip_hash);
	ut_ad(bpage->in_page_hash);
	ut_ad(bpage->zip.data == NULL);
	ut_ad(bpage->buf_fix_count > 0);
	return(TRUE);
}

/****************************************************************//**
Add watch for the given page to be read in. Caller must have the buffer pool
mutex reserved.
@return NULL if watch set, block if the page is in the buffer pool */
UNIV_INTERN
buf_page_t*
buf_pool_watch_set(
/*===============*/
	ulint	space,	/*!< in: space id */
	ulint	offset,	/*!< in: page number */
	ulint	fold)	/*!< in: buf_page_address_fold(space, offset) */
{
	buf_page_t*	bpage;
	ulint		i;
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);
	mutex_t*	block_mutex;

	//ut_ad(buf_pool_mutex_own(buf_pool));

	rw_lock_x_lock(&buf_pool->page_hash_latch);
	bpage = buf_page_hash_get_low(buf_pool, space, offset, fold);

	if (UNIV_LIKELY_NULL(bpage)) {

		block_mutex = buf_page_get_mutex_enter(bpage);
		ut_a(block_mutex);

		if (!buf_pool_watch_is_sentinel(buf_pool, bpage)) {
			/* The page was loaded meanwhile. */
			rw_lock_x_unlock(&buf_pool->page_hash_latch);
			return(bpage);
		}
		/* Add to an existing watch. */
		bpage->buf_fix_count++;
		rw_lock_x_unlock(&buf_pool->page_hash_latch);
		mutex_exit(block_mutex);
		return(NULL);
	}

	/* buf_pool->watch is protected by zip_mutex for now */
	mutex_enter(&buf_pool->zip_mutex);
	for (i = 0; i < BUF_POOL_WATCH_SIZE; i++) {
		bpage = &buf_pool->watch[i];

		ut_ad(bpage->access_time == 0);
		ut_ad(bpage->newest_modification == 0);
		ut_ad(bpage->oldest_modification == 0);
		ut_ad(bpage->zip.data == NULL);
		ut_ad(!bpage->in_zip_hash);

		switch (bpage->state) {
		case BUF_BLOCK_POOL_WATCH:
			ut_ad(!bpage->in_page_hash);
			ut_ad(bpage->buf_fix_count == 0);

			/* bpage is pointing to buf_pool->watch[],
			which is protected by buf_pool->mutex.
			Normally, buf_page_t objects are protected by
			buf_block_t::mutex or buf_pool->zip_mutex or both. */

			bpage->state = BUF_BLOCK_ZIP_PAGE;
			bpage->space = space;
			bpage->offset = offset;
			bpage->buf_fix_count = 1;
			bpage->buf_pool_index = buf_pool_index(buf_pool);
			ut_d(bpage->in_page_hash = TRUE);
			HASH_INSERT(buf_page_t, hash, buf_pool->page_hash,
				    fold, bpage);
			rw_lock_x_unlock(&buf_pool->page_hash_latch);
			mutex_exit(&buf_pool->zip_mutex);
			return(NULL);
		case BUF_BLOCK_ZIP_PAGE:
			ut_ad(bpage->in_page_hash);
			ut_ad(bpage->buf_fix_count > 0);
			break;
		default:
			ut_error;
		}
	}

	/* Allocation failed.  Either the maximum number of purge
	threads should never exceed BUF_POOL_WATCH_SIZE, or this code
	should be modified to return a special non-NULL value and the
	caller should purge the record directly. */
	ut_error;

	/* Fix compiler warning */
	rw_lock_x_unlock(&buf_pool->page_hash_latch);
	mutex_exit(&buf_pool->zip_mutex);
	return(NULL);
}

/****************************************************************//**
Remove the sentinel block for the watch before replacing it with a real block.
buf_page_watch_clear() or buf_page_watch_occurred() will notice that
the block has been replaced with the real block.
@return reference count, to be added to the replacement block */
static
void
buf_pool_watch_remove(
/*==================*/
	buf_pool_t*	buf_pool,	/*!< buffer pool instance */
	ulint		fold,		/*!< in: buf_page_address_fold(
					space, offset) */
	buf_page_t*	watch)		/*!< in/out: sentinel for watch */
{
	//ut_ad(buf_pool_mutex_own(buf_pool));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&buf_pool->page_hash_latch, RW_LOCK_EX));
#endif
	ut_ad(mutex_own(&buf_pool->zip_mutex)); /* for now */

	HASH_DELETE(buf_page_t, hash, buf_pool->page_hash, fold, watch);
	ut_d(watch->in_page_hash = FALSE);
	watch->buf_fix_count = 0;
	watch->state = BUF_BLOCK_POOL_WATCH;
}

/****************************************************************//**
Stop watching if the page has been read in.
buf_pool_watch_set(space,offset) must have returned NULL before. */
UNIV_INTERN
void
buf_pool_watch_unset(
/*=================*/
	ulint	space,	/*!< in: space id */
	ulint	offset)	/*!< in: page number */
{
	buf_page_t*	bpage;
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);
	ulint		fold = buf_page_address_fold(space, offset);

	//buf_pool_mutex_enter(buf_pool);
	rw_lock_x_lock(&buf_pool->page_hash_latch);
	bpage = buf_page_hash_get_low(buf_pool, space, offset, fold);
	/* The page must exist because buf_pool_watch_set()
	increments buf_fix_count. */
	ut_a(bpage);

	if (UNIV_UNLIKELY(!buf_pool_watch_is_sentinel(buf_pool, bpage))) {
		mutex_t* mutex = buf_page_get_mutex_enter(bpage);

		ut_a(bpage->buf_fix_count > 0);
		bpage->buf_fix_count--;
		mutex_exit(mutex);
	} else {
		mutex_enter(&buf_pool->zip_mutex);
		ut_a(bpage->buf_fix_count > 0);

		if (UNIV_LIKELY(!--bpage->buf_fix_count)) {
			buf_pool_watch_remove(buf_pool, fold, bpage);
		}
		mutex_exit(&buf_pool->zip_mutex);
	}

	//buf_pool_mutex_exit(buf_pool);
	rw_lock_x_unlock(&buf_pool->page_hash_latch);
}

/****************************************************************//**
Check if the page has been read in.
This may only be called after buf_pool_watch_set(space,offset)
has returned NULL and before invoking buf_pool_watch_unset(space,offset).
@return	FALSE if the given page was not read in, TRUE if it was */
UNIV_INTERN
ibool
buf_pool_watch_occurred(
/*====================*/
	ulint	space,	/*!< in: space id */
	ulint	offset)	/*!< in: page number */
{
	ibool		ret;
	buf_page_t*	bpage;
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);
	ulint		fold	= buf_page_address_fold(space, offset);

	//buf_pool_mutex_enter(buf_pool);
	rw_lock_s_lock(&buf_pool->page_hash_latch);

	bpage = buf_page_hash_get_low(buf_pool, space, offset, fold);
	/* The page must exist because buf_pool_watch_set()
	increments buf_fix_count. */
	ut_a(bpage);
	ret = !buf_pool_watch_is_sentinel(buf_pool, bpage);
	//buf_pool_mutex_exit(buf_pool);
	rw_lock_s_unlock(&buf_pool->page_hash_latch);

	return(ret);
}

/********************************************************************//**
Moves a page to the start of the buffer pool LRU list. This high-level
function can be used to prevent an important page from slipping out of
the buffer pool. */
UNIV_INTERN
void
buf_page_make_young(
/*================*/
	buf_page_t*	bpage)	/*!< in: buffer block of a file page */
{
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);

	//buf_pool_mutex_enter(buf_pool);
	mutex_enter(&buf_pool->LRU_list_mutex);

	ut_a(buf_page_in_file(bpage));

	buf_LRU_make_block_young(bpage);

	//buf_pool_mutex_exit(buf_pool);
	mutex_exit(&buf_pool->LRU_list_mutex);
}

/********************************************************************//**
Sets the time of the first access of a page and moves a page to the
start of the buffer pool LRU list if it is too old.  This high-level
function can be used to prevent an important page from slipping
out of the buffer pool. */
static
void
buf_page_set_accessed_make_young(
/*=============================*/
	buf_page_t*	bpage,		/*!< in/out: buffer block of a
					file page */
	unsigned	access_time)	/*!< in: bpage->access_time
					read under mutex protection,
					or 0 if unknown */
{
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);

	ut_ad(!buf_pool_mutex_own(buf_pool));
	ut_a(buf_page_in_file(bpage));

	if (buf_page_peek_if_too_old(bpage)) {
		//buf_pool_mutex_enter(buf_pool);
		mutex_enter(&buf_pool->LRU_list_mutex);
		buf_LRU_make_block_young(bpage);
		//buf_pool_mutex_exit(buf_pool);
		mutex_exit(&buf_pool->LRU_list_mutex);
	} else if (!access_time) {
		ulint	time_ms = ut_time_ms();
		mutex_t*	block_mutex = buf_page_get_mutex_enter(bpage);
		//buf_pool_mutex_enter(buf_pool);
		if (block_mutex) {
		buf_page_set_accessed(bpage, time_ms);
		mutex_exit(block_mutex);
		}
		//buf_pool_mutex_exit(buf_pool);
	}
}

/********************************************************************//**
Resets the check_index_page_at_flush field of a page if found in the buffer
pool. */
UNIV_INTERN
void
buf_reset_check_index_page_at_flush(
/*================================*/
	ulint	space,	/*!< in: space id */
	ulint	offset)	/*!< in: page number */
{
	buf_block_t*	block;
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);

	//buf_pool_mutex_enter(buf_pool);
	rw_lock_s_lock(&buf_pool->page_hash_latch);

	block = (buf_block_t*) buf_page_hash_get(buf_pool, space, offset);

	if (block && buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE) {
		ut_ad(!buf_pool_watch_is_sentinel(buf_pool, &block->page));
		block->check_index_page_at_flush = FALSE;
	}

	//buf_pool_mutex_exit(buf_pool);
	rw_lock_s_unlock(&buf_pool->page_hash_latch);
}

#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
/********************************************************************//**
Sets file_page_was_freed TRUE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated.
@return	control block if found in page hash table, otherwise NULL */
UNIV_INTERN
buf_page_t*
buf_page_set_file_page_was_freed(
/*=============================*/
	ulint	space,	/*!< in: space id */
	ulint	offset)	/*!< in: page number */
{
	buf_page_t*	bpage;
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);

	//buf_pool_mutex_enter(buf_pool);
	rw_lock_s_lock(&buf_pool->page_hash_latch);

	bpage = buf_page_hash_get(buf_pool, space, offset);

	if (bpage) {
		ut_ad(!buf_pool_watch_is_sentinel(buf_pool, bpage));
		/* bpage->file_page_was_freed can already hold
		when this code is invoked from dict_drop_index_tree() */
		bpage->file_page_was_freed = TRUE;
	}

	//buf_pool_mutex_exit(buf_pool);
	rw_lock_s_unlock(&buf_pool->page_hash_latch);

	return(bpage);
}

/********************************************************************//**
Sets file_page_was_freed FALSE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated.
@return	control block if found in page hash table, otherwise NULL */
UNIV_INTERN
buf_page_t*
buf_page_reset_file_page_was_freed(
/*===============================*/
	ulint	space,	/*!< in: space id */
	ulint	offset)	/*!< in: page number */
{
	buf_page_t*	bpage;
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);

	//buf_pool_mutex_enter(buf_pool);
	rw_lock_s_lock(&buf_pool->page_hash_latch);

	bpage = buf_page_hash_get(buf_pool, space, offset);

	if (bpage) {
		ut_ad(!buf_pool_watch_is_sentinel(buf_pool, bpage));
		bpage->file_page_was_freed = FALSE;
	}

	//buf_pool_mutex_exit(buf_pool);
	rw_lock_s_unlock(&buf_pool->page_hash_latch);

	return(bpage);
}
#endif /* UNIV_DEBUG_FILE_ACCESSES || UNIV_DEBUG */

/********************************************************************//**
Get read access to a compressed page (usually of type
FIL_PAGE_TYPE_ZBLOB or FIL_PAGE_TYPE_ZBLOB2).
The page must be released with buf_page_release_zip().
NOTE: the page is not protected by any latch.  Mutual exclusion has to
be implemented at a higher level.  In other words, all possible
accesses to a given page through this function must be protected by
the same set of mutexes or latches.
@return	pointer to the block */
UNIV_INTERN
buf_page_t*
buf_page_get_zip(
/*=============*/
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size */
	ulint		offset)	/*!< in: page number */
{
	buf_page_t*	bpage;
	mutex_t*	block_mutex;
	ibool		must_read;
	unsigned	access_time;
	trx_t*		trx = NULL;
	ulint		sec;
	ulint		ms;
	ib_uint64_t	start_time;
	ib_uint64_t	finish_time;
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);

	if (innobase_get_slow_log()) {
		trx = innobase_get_trx();
	}
	buf_pool->stat.n_page_gets++;

	for (;;) {
		//buf_pool_mutex_enter(buf_pool);
lookup:
		rw_lock_s_lock(&buf_pool->page_hash_latch);
		bpage = buf_page_hash_get(buf_pool, space, offset);
		if (bpage) {
			ut_ad(!buf_pool_watch_is_sentinel(buf_pool, bpage));
			break;
		}

		/* Page not in buf_pool: needs to be read from file */

		//buf_pool_mutex_exit(buf_pool);
		rw_lock_s_unlock(&buf_pool->page_hash_latch);

		buf_read_page(space, zip_size, offset, trx);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		ut_a(++buf_dbg_counter % 37 || buf_validate());
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
	}

	if (UNIV_UNLIKELY(bpage->space_was_being_deleted)) {
		/* This page is obsoleted, should discard and retry */
		rw_lock_s_unlock(&buf_pool->page_hash_latch);

		mutex_enter(&buf_pool->LRU_list_mutex);
		block_mutex = buf_page_get_mutex_enter(bpage);

		if (UNIV_UNLIKELY(!block_mutex)) {
			mutex_exit(&buf_pool->LRU_list_mutex);
			goto lookup;
		}

		buf_LRU_free_block(bpage, TRUE, TRUE);

		mutex_exit(&buf_pool->LRU_list_mutex);
		mutex_exit(block_mutex);
		block_mutex = NULL;

		goto lookup;
	}

	if (UNIV_UNLIKELY(!bpage->zip.data)) {
		/* There is no compressed page. */
err_exit:
		//buf_pool_mutex_exit(buf_pool);
		rw_lock_s_unlock(&buf_pool->page_hash_latch);
		return(NULL);
	}

	if (srv_pass_corrupt_table <= 1) {
		if (bpage->is_corrupt) {
			rw_lock_s_unlock(&buf_pool->page_hash_latch);
			return(NULL);
		}
	}

	block_mutex = buf_page_get_mutex_enter(bpage);

	rw_lock_s_unlock(&buf_pool->page_hash_latch);

	ut_ad(!buf_pool_watch_is_sentinel(buf_pool, bpage));

	switch (buf_page_get_state(bpage)) {
	case BUF_BLOCK_NOT_USED:
	case BUF_BLOCK_READY_FOR_USE:
	case BUF_BLOCK_MEMORY:
	case BUF_BLOCK_REMOVE_HASH:
	case BUF_BLOCK_ZIP_FREE:
		if (block_mutex)
			mutex_exit(block_mutex);
		break;
	case BUF_BLOCK_ZIP_PAGE:
	case BUF_BLOCK_ZIP_DIRTY:
		ut_a(block_mutex == &buf_pool->zip_mutex);
		bpage->buf_fix_count++;
		goto got_block;
	case BUF_BLOCK_FILE_PAGE:
		ut_a(block_mutex == &((buf_block_t*) bpage)->mutex);

		/* release mutex to obey to latch-order */
		mutex_exit(block_mutex);

		/* get LRU_list_mutex for buf_LRU_free_block() */
		mutex_enter(&buf_pool->LRU_list_mutex);
		mutex_enter(block_mutex);

		if (UNIV_UNLIKELY(bpage->space != space
				  || bpage->offset != offset
				  || !bpage->in_LRU_list
				  || !bpage->zip.data)) {
			/* someone should interrupt, retry */
			mutex_exit(&buf_pool->LRU_list_mutex);
			mutex_exit(block_mutex);
			goto lookup;
		}

		/* Discard the uncompressed page frame if possible. */
		if (buf_LRU_free_block(bpage, FALSE, TRUE)) {
			mutex_exit(&buf_pool->LRU_list_mutex);
			mutex_exit(block_mutex);
			goto lookup;
		}

		mutex_exit(&buf_pool->LRU_list_mutex);

		buf_block_buf_fix_inc((buf_block_t*) bpage,
				      __FILE__, __LINE__);
		goto got_block;
	}

	ut_error;
	goto err_exit;

got_block:
	must_read = buf_page_get_io_fix(bpage) == BUF_IO_READ;
	access_time = buf_page_is_accessed(bpage);

	//buf_pool_mutex_exit(buf_pool);

	mutex_exit(block_mutex);

	buf_page_set_accessed_make_young(bpage, access_time);

#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	ut_a(!bpage->file_page_was_freed);
#endif

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(++buf_dbg_counter % 5771 || buf_validate());
	ut_a(bpage->buf_fix_count > 0);
	ut_a(buf_page_in_file(bpage));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	if (must_read) {
		/* Let us wait until the read operation
		completes */

		if (innobase_get_slow_log() && trx && trx->take_stats)
		{
			ut_usectime(&sec, &ms);
			start_time = (ib_uint64_t)sec * 1000000 + ms;
		} else {
			start_time = 0;
		}
		for (;;) {
			enum buf_io_fix	io_fix;

			mutex_enter(block_mutex);
			io_fix = buf_page_get_io_fix(bpage);
			mutex_exit(block_mutex);

			if (io_fix == BUF_IO_READ) {

				os_thread_sleep(WAIT_FOR_READ);
			} else {
				break;
			}
		}
		if (innobase_get_slow_log() && trx && trx->take_stats && start_time)
		{
			ut_usectime(&sec, &ms);
			finish_time = (ib_uint64_t)sec * 1000000 + ms;
			trx->io_reads_wait_timer += (ulint)(finish_time - start_time);
		}
	}

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(buf_page_get_space(bpage),
			    buf_page_get_page_no(bpage)) == 0);
#endif
	return(bpage);
}

/********************************************************************//**
Initialize some fields of a control block. */
UNIV_INLINE
void
buf_block_init_low(
/*===============*/
	buf_block_t*	block)	/*!< in: block to init */
{
	block->check_index_page_at_flush = FALSE;
	block->index		= NULL;
	block->btr_search_latch	= NULL;

	block->n_hash_helps	= 0;
	block->n_fields		= 1;
	block->n_bytes		= 0;
	block->left_side	= TRUE;
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************************//**
Decompress a block.
@return	TRUE if successful */
UNIV_INTERN
ibool
buf_zip_decompress(
/*===============*/
	buf_block_t*	block,	/*!< in/out: block */
	ibool		check)	/*!< in: TRUE=verify the page checksum */
{
	const byte*	frame		= block->page.zip.data;
	ulint		stamp_checksum	= mach_read_from_4(
		frame + FIL_PAGE_SPACE_OR_CHKSUM);

	ut_ad(buf_block_get_zip_size(block));
	ut_a(buf_block_get_space(block) != 0);

	if (UNIV_LIKELY(check && stamp_checksum != BUF_NO_CHECKSUM_MAGIC)) {
		ulint	calc_checksum	= page_zip_calc_checksum(
			frame, page_zip_get_size(&block->page.zip));

		if (UNIV_UNLIKELY(stamp_checksum != calc_checksum)) {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: compressed page checksum mismatch"
				" (space %u page %u): %lu != %lu\n",
				block->page.space, block->page.offset,
				stamp_checksum, calc_checksum);
			return(FALSE);
		}
	}

	switch (fil_page_get_type(frame)) {
	case FIL_PAGE_INDEX:
		if (page_zip_decompress(&block->page.zip,
					block->frame, TRUE)) {
			return(TRUE);
		}

		fprintf(stderr,
			"InnoDB: unable to decompress space %lu page %lu\n",
			(ulong) block->page.space,
			(ulong) block->page.offset);
		return(FALSE);

	case FIL_PAGE_TYPE_ALLOCATED:
	case FIL_PAGE_INODE:
	case FIL_PAGE_IBUF_BITMAP:
	case FIL_PAGE_TYPE_FSP_HDR:
	case FIL_PAGE_TYPE_XDES:
	case FIL_PAGE_TYPE_ZBLOB:
	case FIL_PAGE_TYPE_ZBLOB2:
		/* Copy to uncompressed storage. */
		memcpy(block->frame, frame,
		       buf_block_get_zip_size(block));
		return(TRUE);
	}

	ut_print_timestamp(stderr);
	fprintf(stderr,
		"  InnoDB: unknown compressed page"
		" type %lu\n",
		fil_page_get_type(frame));
	return(FALSE);
}

#ifndef UNIV_HOTBACKUP
/*******************************************************************//**
Gets the block to whose frame the pointer is pointing to if found
in this buffer pool instance.
@return	pointer to block */
UNIV_INTERN
buf_block_t*
buf_block_align_instance(
/*=====================*/
 	buf_pool_t*	buf_pool,	/*!< in: buffer in which the block
					resides */
	const byte*	ptr)		/*!< in: pointer to a frame */
{
	buf_chunk_t*	chunk;
	ulint		i;

	/* TODO: protect buf_pool->chunks with a mutex (it will
	currently remain constant after buf_pool_init()) */
	for (chunk = buf_pool->chunks, i = buf_pool->n_chunks; i--; chunk++) {
		ulint	offs;

		if (UNIV_UNLIKELY(ptr < chunk->blocks->frame)) {

			continue;
		}
		/* else */

		offs = ptr - chunk->blocks->frame;

		offs >>= UNIV_PAGE_SIZE_SHIFT;

		if (UNIV_LIKELY(offs < chunk->size)) {
			buf_block_t*	block = &chunk->blocks[offs];

			/* The function buf_chunk_init() invokes
			buf_block_init() so that block[n].frame ==
			block->frame + n * UNIV_PAGE_SIZE.  Check it. */
			ut_ad(block->frame == page_align(ptr));
#ifdef UNIV_DEBUG
			/* A thread that updates these fields must
			hold buf_pool->mutex and block->mutex.  Acquire
			only the latter. */
			mutex_enter(&block->mutex);

			switch (buf_block_get_state(block)) {
			case BUF_BLOCK_ZIP_FREE:
			case BUF_BLOCK_ZIP_PAGE:
			case BUF_BLOCK_ZIP_DIRTY:
				/* These types should only be used in
				the compressed buffer pool, whose
				memory is allocated from
				buf_pool->chunks, in UNIV_PAGE_SIZE
				blocks flagged as BUF_BLOCK_MEMORY. */
				ut_error;
				break;
			case BUF_BLOCK_NOT_USED:
			case BUF_BLOCK_READY_FOR_USE:
			case BUF_BLOCK_MEMORY:
				/* Some data structures contain
				"guess" pointers to file pages.  The
				file pages may have been freed and
				reused.  Do not complain. */
				break;
			case BUF_BLOCK_REMOVE_HASH:
				/* buf_LRU_block_remove_hashed_page()
				will overwrite the FIL_PAGE_OFFSET and
				FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID with
				0xff and set the state to
				BUF_BLOCK_REMOVE_HASH. */
				ut_ad(page_get_space_id(page_align(ptr))
				      == 0xffffffff);
				ut_ad(page_get_page_no(page_align(ptr))
				      == 0xffffffff);
				break;
			case BUF_BLOCK_FILE_PAGE:
				ut_ad(block->page.space
				      == page_get_space_id(page_align(ptr)));
				ut_ad(block->page.offset
				      == page_get_page_no(page_align(ptr)));
				break;
			}

			mutex_exit(&block->mutex);
#endif /* UNIV_DEBUG */

			return(block);
		}
	}

	return(NULL);
}

/*******************************************************************//**
Gets the block to whose frame the pointer is pointing to.
@return	pointer to block, never NULL */
UNIV_INTERN
buf_block_t*
buf_block_align(
/*============*/
	const byte*	ptr)	/*!< in: pointer to a frame */
{
	ulint		i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_block_t*	block;

		block = buf_block_align_instance(
			buf_pool_from_array(i), ptr);
		if (block) {
			return(block);
		}
	}

	/* The block should always be found. */
	ut_error;
	return(NULL);
}

/********************************************************************//**
Find out if a pointer belongs to a buf_block_t. It can be a pointer to
the buf_block_t itself or a member of it. This functions checks one of
the buffer pool instances.
@return	TRUE if ptr belongs to a buf_block_t struct */
static
ibool
buf_pointer_is_block_field_instance(
/*================================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	const void*	ptr)		/*!< in: pointer not dereferenced */
{
	const buf_chunk_t*		chunk	= buf_pool->chunks;
	const buf_chunk_t* const	echunk	= chunk + buf_pool->n_chunks;

	/* TODO: protect buf_pool->chunks with a mutex (it will
	currently remain constant after buf_pool_init()) */
	while (chunk < echunk) {
		if (ptr >= (void *)chunk->blocks
		    && ptr < (void *)(chunk->blocks + chunk->size)) {

			return(TRUE);
		}

		chunk++;
	}

	return(FALSE);
}

/********************************************************************//**
Find out if a pointer belongs to a buf_block_t. It can be a pointer to
the buf_block_t itself or a member of it
@return	TRUE if ptr belongs to a buf_block_t struct */
UNIV_INTERN
ibool
buf_pointer_is_block_field(
/*=======================*/
	const void*	ptr)	/*!< in: pointer not dereferenced */
{
	ulint	i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		ibool	found;

		found = buf_pointer_is_block_field_instance(
			buf_pool_from_array(i), ptr);
		if (found) {
			return(TRUE);
		}
	}

	return(FALSE);
}

/********************************************************************//**
Find out if a buffer block was created by buf_chunk_init().
@return	TRUE if "block" has been added to buf_pool->free by buf_chunk_init() */
static
ibool
buf_block_is_uncompressed(
/*======================*/
	buf_pool_t*		buf_pool,	/*!< in: buffer pool instance */
	const buf_block_t*	block)		/*!< in: pointer to block,
						not dereferenced */
{
	//ut_ad(buf_pool_mutex_own(buf_pool));

	if (UNIV_UNLIKELY((((ulint) block) % sizeof *block) != 0)) {
		/* The pointer should be aligned. */
		return(FALSE);
	}

	return(buf_pointer_is_block_field_instance(buf_pool, (void *)block));
}

/********************************************************************//**
This is the general function used to get access to a database page.
@return	pointer to the block or NULL */
UNIV_INTERN
buf_block_t*
buf_page_get_gen(
/*=============*/
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	ulint		offset,	/*!< in: page number */
	ulint		rw_latch,/*!< in: RW_S_LATCH, RW_X_LATCH, RW_NO_LATCH */
	buf_block_t*	guess,	/*!< in: guessed block or NULL */
	ulint		mode,	/*!< in: BUF_GET, BUF_GET_IF_IN_POOL,
				BUF_PEEK_IF_IN_POOL, BUF_GET_NO_LATCH, or
				BUF_GET_IF_IN_POOL_OR_WATCH */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mini-transaction */
{
	buf_block_t*	block;
	ulint		fold;
	unsigned	access_time;
	ulint		fix_type;
	ibool		must_read;
	ulint		retries = 0;
	mutex_t*	block_mutex = NULL;
	trx_t*		trx = NULL;
	ulint		sec;
	ulint		ms;
	ib_uint64_t	start_time;
	ib_uint64_t	finish_time;
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);

	ut_ad(mtr);
	ut_ad(mtr->state == MTR_ACTIVE);
	ut_ad((rw_latch == RW_S_LATCH)
	      || (rw_latch == RW_X_LATCH)
	      || (rw_latch == RW_NO_LATCH));
#ifdef UNIV_DEBUG
	switch (mode) {
	case BUF_GET_NO_LATCH:
		ut_ad(rw_latch == RW_NO_LATCH);
		break;
	case BUF_GET:
	case BUF_GET_IF_IN_POOL:
	case BUF_PEEK_IF_IN_POOL:
	case BUF_GET_IF_IN_POOL_OR_WATCH:
	case BUF_GET_POSSIBLY_FREED:
		break;
	default:
		ut_error;
	}
#endif /* UNIV_DEBUG */
	ut_ad(zip_size == fil_space_get_zip_size(space));
	ut_ad(ut_is_2pow(zip_size));
#ifndef UNIV_LOG_DEBUG
	ut_ad(!ibuf_inside(mtr)
	      || ibuf_page_low(space, zip_size, offset,
			       FALSE, file, line, NULL));
#endif
	if (innobase_get_slow_log()) {
		trx = innobase_get_trx();
	}
	buf_pool->stat.n_page_gets++;
	fold = buf_page_address_fold(space, offset);
loop:
	block = guess;
	//buf_pool_mutex_enter(buf_pool);

	if (block) {
		block_mutex = buf_page_get_mutex_enter((buf_page_t*)block);

		/* If the guess is a compressed page descriptor that
		has been allocated by buf_page_alloc_descriptor(),
		it may have been freed by buf_relocate(). */

		if (!block_mutex) {
			block = guess = NULL;
		} else if (!buf_block_is_uncompressed(buf_pool, block)
		    || offset != block->page.offset
		    || space != block->page.space
		    || buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE) {

			mutex_exit(block_mutex);

			block = guess = NULL;
		} else {
			ut_ad(!block->page.in_zip_hash);
			ut_ad(block->page.in_page_hash);
		}
	}

	if (block == NULL) {
		rw_lock_s_lock(&buf_pool->page_hash_latch);
		block = (buf_block_t*) buf_page_hash_get_low(
			buf_pool, space, offset, fold);
		if (block) {
			if (UNIV_UNLIKELY(block->page.space_was_being_deleted)) {
				/* This page is obsoleted, should discard and retry */
				rw_lock_s_unlock(&buf_pool->page_hash_latch);

				mutex_enter(&buf_pool->LRU_list_mutex);
				block_mutex = buf_page_get_mutex_enter((buf_page_t*)block);

				if (UNIV_UNLIKELY(!block_mutex)) {
					mutex_exit(&buf_pool->LRU_list_mutex);
					goto loop;
				}

				buf_LRU_free_block((buf_page_t*)block, TRUE, TRUE);

				mutex_exit(&buf_pool->LRU_list_mutex);
				mutex_exit(block_mutex);
				block_mutex = NULL;

				goto loop;
			}

			block_mutex = buf_page_get_mutex_enter((buf_page_t*)block);
			ut_a(block_mutex);
		}
		rw_lock_s_unlock(&buf_pool->page_hash_latch);
	}

loop2:
	if (block && buf_pool_watch_is_sentinel(buf_pool, &block->page)) {
		mutex_exit(block_mutex);
		block = NULL;
	}

	if (block == NULL) {
		/* Page not in buf_pool: needs to be read from file */

		if (mode == BUF_GET_IF_IN_POOL_OR_WATCH) {
			block = (buf_block_t*) buf_pool_watch_set(
				space, offset, fold);

			if (UNIV_LIKELY_NULL(block)) {
				block_mutex = buf_page_get_mutex((buf_page_t*)block);
				ut_a(block_mutex);
				ut_ad(mutex_own(block_mutex));
				goto got_block;
			}
		}

		//buf_pool_mutex_exit(buf_pool);

		if (mode == BUF_GET_IF_IN_POOL
		    || mode == BUF_PEEK_IF_IN_POOL
		    || mode == BUF_GET_IF_IN_POOL_OR_WATCH) {

			return(NULL);
		}

		if (buf_read_page(space, zip_size, offset, trx)) {
			buf_read_ahead_random(space, zip_size, offset,
					      ibuf_inside(mtr), trx);

			retries = 0;
		} else if (retries < BUF_PAGE_READ_MAX_RETRIES) {
			++retries;
		} else {
			fprintf(stderr, "InnoDB: Error: Unable"
				" to read tablespace %lu page no"
				" %lu into the buffer pool after"
				" %lu attempts\n"
				"InnoDB: The most probable cause"
				" of this error may be that the"
				" table has been corrupted.\n"
				"InnoDB: You can try to fix this"
				" problem by using"
				" innodb_force_recovery.\n"
				"InnoDB: Please see reference manual"
				" for more details.\n"
				"InnoDB: Aborting...\n",
				space, offset,
				BUF_PAGE_READ_MAX_RETRIES);

			ut_error;
		}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		ut_a(++buf_dbg_counter % 37 || buf_validate());
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
		goto loop;
	}

got_block:
	ut_ad(page_zip_get_size(&block->page.zip) == zip_size);

	must_read = buf_block_get_io_fix(block) == BUF_IO_READ;

	if (must_read && (mode == BUF_GET_IF_IN_POOL
			  || mode == BUF_PEEK_IF_IN_POOL)) {

		/* The page is being read to buffer pool,
		but we cannot wait around for the read to
		complete. */
		//buf_pool_mutex_exit(buf_pool);
		mutex_exit(block_mutex);

		return(NULL);
	}

	if (srv_pass_corrupt_table <= 1) {
		if (block->page.is_corrupt) {
			mutex_exit(block_mutex);
			return(NULL);
		}
	}

	switch (buf_block_get_state(block)) {
		buf_page_t*	bpage;
		ibool		success;

	case BUF_BLOCK_FILE_PAGE:
		if (block_mutex == &buf_pool->zip_mutex) {
			/* it is wrong mutex... */
			mutex_exit(block_mutex);
			goto loop;
		}
		break;

	case BUF_BLOCK_ZIP_PAGE:
	case BUF_BLOCK_ZIP_DIRTY:
		ut_ad(block_mutex == &buf_pool->zip_mutex);
		bpage = &block->page;
		/* Protect bpage->buf_fix_count. */
		//mutex_enter(&buf_pool->zip_mutex);

		if (bpage->buf_fix_count
		    || buf_page_get_io_fix(bpage) != BUF_IO_NONE) {
			/* This condition often occurs when the buffer
			is not buffer-fixed, but I/O-fixed by
			buf_page_init_for_read(). */
			//mutex_exit(&buf_pool->zip_mutex);
wait_until_unfixed:
			/* The block is buffer-fixed or I/O-fixed.
			Try again later. */
			//buf_pool_mutex_exit(buf_pool);
			mutex_exit(block_mutex);
			os_thread_sleep(WAIT_FOR_READ);

			goto loop;
		}

		/* Allocate an uncompressed page. */
		//buf_pool_mutex_exit(buf_pool);
		//mutex_exit(&buf_pool->zip_mutex);
		mutex_exit(block_mutex);

		block = buf_LRU_get_free_block(buf_pool);
		ut_a(block);
		block_mutex = &block->mutex;

		//buf_pool_mutex_enter(buf_pool);
		mutex_enter(&buf_pool->LRU_list_mutex);
		rw_lock_x_lock(&buf_pool->page_hash_latch);
		mutex_enter(block_mutex);

		{
			buf_page_t*	hash_bpage;

			hash_bpage = buf_page_hash_get_low(
				buf_pool, space, offset, fold);

			if (UNIV_UNLIKELY(bpage != hash_bpage)) {
				/* The buf_pool->page_hash was modified
				while buf_pool->mutex was released.
				Free the block that was allocated. */

				buf_LRU_block_free_non_file_page(block, TRUE);
				mutex_exit(block_mutex);

				block = (buf_block_t*) hash_bpage;
				if (block) {
					block_mutex = buf_page_get_mutex_enter((buf_page_t*)block);
					ut_a(block_mutex);
				}
				rw_lock_x_unlock(&buf_pool->page_hash_latch);
				mutex_exit(&buf_pool->LRU_list_mutex);
				goto loop2;
			}
		}

		mutex_enter(&buf_pool->zip_mutex);

		if (UNIV_UNLIKELY
		    (bpage->buf_fix_count
		     || buf_page_get_io_fix(bpage) != BUF_IO_NONE)) {

			mutex_exit(&buf_pool->zip_mutex);
			/* The block was buffer-fixed or I/O-fixed
			while buf_pool->mutex was not held by this thread.
			Free the block that was allocated and try again.
			This should be extremely unlikely. */

			buf_LRU_block_free_non_file_page(block, TRUE);
			//mutex_exit(&block->mutex);

			rw_lock_x_unlock(&buf_pool->page_hash_latch);
			mutex_exit(&buf_pool->LRU_list_mutex);
			goto wait_until_unfixed;
		}

		/* Move the compressed page from bpage to block,
		and uncompress it. */

		buf_relocate(bpage, &block->page);

		rw_lock_x_unlock(&buf_pool->page_hash_latch);

		buf_block_init_low(block);
		block->lock_hash_val = lock_rec_hash(space, offset);

		UNIV_MEM_DESC(&block->page.zip.data,
			      page_zip_get_size(&block->page.zip), block);

		if (buf_page_get_state(&block->page)
		    == BUF_BLOCK_ZIP_PAGE) {
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
			UT_LIST_REMOVE(zip_list, buf_pool->zip_clean,
				       &block->page);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
			ut_ad(!block->page.in_flush_list);
		} else {
			/* Relocate buf_pool->flush_list. */
			buf_flush_relocate_on_flush_list(bpage,
							 &block->page);
		}

		/* Buffer-fix, I/O-fix, and X-latch the block
		for the duration of the decompression.
		Also add the block to the unzip_LRU list. */
		block->page.state = BUF_BLOCK_FILE_PAGE;

		/* Insert at the front of unzip_LRU list */
		buf_unzip_LRU_add_block(block, FALSE);

		mutex_exit(&buf_pool->LRU_list_mutex);

		block->page.buf_fix_count = 1;
		buf_block_set_io_fix(block, BUF_IO_READ);
		rw_lock_x_lock_inline(&block->lock, 0, file, line);

		UNIV_MEM_INVALID(bpage, sizeof *bpage);

		mutex_exit(block_mutex);
		mutex_exit(&buf_pool->zip_mutex);

		buf_pool_mutex_enter(buf_pool);
		buf_pool->n_pend_unzip++;
		buf_pool_mutex_exit(buf_pool);

		//buf_pool_mutex_exit(buf_pool);

		buf_page_free_descriptor(bpage);

		/* Decompress the page and apply buffered operations
		while not holding buf_pool->mutex or block->mutex. */
		success = buf_zip_decompress(block, srv_use_checksums);
		ut_a(success);

		if (UNIV_LIKELY(!recv_no_ibuf_operations)) {
			ibuf_merge_or_delete_for_page(block, space, offset,
						      zip_size, TRUE);
		}

		/* Unfix and unlatch the block. */
		//buf_pool_mutex_enter(buf_pool);
		block_mutex = &block->mutex;
		mutex_enter(block_mutex);
		block->page.buf_fix_count--;
		buf_block_set_io_fix(block, BUF_IO_NONE);

		buf_pool_mutex_enter(buf_pool);
		buf_pool->n_pend_unzip--;
		buf_pool_mutex_exit(buf_pool);
		rw_lock_x_unlock(&block->lock);

		break;

	case BUF_BLOCK_ZIP_FREE:
	case BUF_BLOCK_NOT_USED:
	case BUF_BLOCK_READY_FOR_USE:
	case BUF_BLOCK_MEMORY:
	case BUF_BLOCK_REMOVE_HASH:
		ut_error;
		break;
	}

	ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);

	//mutex_enter(&block->mutex);
#if UNIV_WORD_SIZE == 4
	/* On 32-bit systems, there is no padding in buf_page_t.  On
	other systems, Valgrind could complain about uninitialized pad
	bytes. */
	UNIV_MEM_ASSERT_RW(&block->page, sizeof block->page);
#endif
#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
	if ((mode == BUF_GET_IF_IN_POOL || mode == BUF_GET_IF_IN_POOL_OR_WATCH)
	    && ibuf_debug) {
		/* Try to evict the block from the buffer pool, to use the
		insert buffer (change buffer) as much as possible. */
		ulint	page_no	= buf_block_get_page_no(block);

		if (buf_LRU_free_block(&block->page, TRUE, FALSE)) {
			mutex_exit(block_mutex);
			if (mode == BUF_GET_IF_IN_POOL_OR_WATCH) {
				/* Set the watch, as it would have
				been set if the page were not in the
				buffer pool in the first place. */
				block = (buf_block_t*) buf_pool_watch_set(
					space, offset, fold);

				if (UNIV_LIKELY_NULL(block)) {
					block_mutex = buf_page_get_mutex((buf_page_t*)block);
					ut_a(block_mutex);
					ut_ad(mutex_own(block_mutex));

					/* The page entered the buffer
					pool for some reason. Try to
					evict it again. */
					goto got_block;
				}
			}
			//buf_pool_mutex_exit(buf_pool);
			fprintf(stderr,
				"innodb_change_buffering_debug evict %u %u\n",
				(unsigned) space, (unsigned) offset);
			return(NULL);
		} else if (UNIV_UNLIKELY(buf_block_get_state(block)
					 != BUF_BLOCK_FILE_PAGE
				|| (buf_block_get_page_no(block) != page_no)
				|| (buf_block_get_space(block) != space))) {

				/* buf_LRU_free_block temporarily releases the
				block mutex, and now block points to something
				else. */
				mutex_exit(block_mutex);
				block = NULL;
				goto loop2;

		} else if (buf_flush_page_try(buf_pool, block)) {
			fprintf(stderr,
				"innodb_change_buffering_debug flush %u %u\n",
				(unsigned) space, (unsigned) offset);
			guess = block;
			goto loop;
		}

		/* Failed to evict the page; change it directly */
	}
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

	buf_block_buf_fix_inc(block, file, line);
#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	ut_a(mode == BUF_GET_POSSIBLY_FREED
	     || !block->page.file_page_was_freed);
#endif
	//mutex_exit(&block->mutex);

	/* Check if this is the first access to the page */

	access_time = buf_page_is_accessed(&block->page);

	//buf_pool_mutex_exit(buf_pool);
	mutex_exit(block_mutex);

	if (UNIV_LIKELY(mode != BUF_PEEK_IF_IN_POOL)) {
		buf_page_set_accessed_make_young(&block->page, access_time);
	}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(++buf_dbg_counter % 5771 || buf_validate());
	ut_a(block->page.buf_fix_count > 0);
	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	switch (rw_latch) {
	case RW_NO_LATCH:
		if (must_read) {
			/* Let us wait until the read operation
			completes */

			if (innobase_get_slow_log() && trx && trx->take_stats)
			{
				ut_usectime(&sec, &ms);
				start_time = (ib_uint64_t)sec * 1000000 + ms;
			} else {
				start_time = 0;
			}
			for (;;) {
				enum buf_io_fix	io_fix;

				mutex_enter(&block->mutex);
				io_fix = buf_block_get_io_fix(block);
				mutex_exit(&block->mutex);

				if (io_fix == BUF_IO_READ) {

					os_thread_sleep(WAIT_FOR_READ);
				} else {
					break;
				}
			}
			if (innobase_get_slow_log() && trx && trx->take_stats && start_time)
			{
				ut_usectime(&sec, &ms);
				finish_time = (ib_uint64_t)sec * 1000000 + ms;
				trx->io_reads_wait_timer += (ulint)(finish_time - start_time);
			}
		}

		fix_type = MTR_MEMO_BUF_FIX;
		break;

	case RW_S_LATCH:
		rw_lock_s_lock_inline(&(block->lock), 0, file, line);

		fix_type = MTR_MEMO_PAGE_S_FIX;
		break;

	default:
		ut_ad(rw_latch == RW_X_LATCH);
		rw_lock_x_lock_inline(&(block->lock), 0, file, line);

		fix_type = MTR_MEMO_PAGE_X_FIX;
		break;
	}

	mtr_memo_push(mtr, block, fix_type);

	if (UNIV_LIKELY(mode != BUF_PEEK_IF_IN_POOL) && !access_time) {
		/* In the case of a first access, try to apply linear
		read-ahead */

		buf_read_ahead_linear(space, zip_size, offset,
				      ibuf_inside(mtr), trx);
	}

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(buf_block_get_space(block),
			    buf_block_get_page_no(block)) == 0);
#endif
	if (innobase_get_slow_log()) {
		_increment_page_get_statistics(block, trx);
	}

	return(block);
}

/********************************************************************//**
This is the general function used to get optimistic access to a database
page.
@return	TRUE if success */
UNIV_INTERN
ibool
buf_page_optimistic_get(
/*====================*/
	ulint		rw_latch,/*!< in: RW_S_LATCH, RW_X_LATCH */
	buf_block_t*	block,	/*!< in: guessed buffer block */
	ib_uint64_t	modify_clock,/*!< in: modify clock value if mode is
				..._GUESS_ON_CLOCK */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mini-transaction */
{
	buf_pool_t*	buf_pool;
	unsigned	access_time;
	ibool		success;
	ulint		fix_type;
	trx_t*		trx = NULL;

	ut_ad(block);
	ut_ad(mtr);
	ut_ad(mtr->state == MTR_ACTIVE);
	ut_ad((rw_latch == RW_S_LATCH) || (rw_latch == RW_X_LATCH));

	mutex_enter(&block->mutex);

	if (UNIV_UNLIKELY(buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE)) {

		mutex_exit(&block->mutex);

		return(FALSE);
	}

	buf_block_buf_fix_inc(block, file, line);

	mutex_exit(&block->mutex);

	/* Check if this is the first access to the page.
	We do a dirty read on purpose, to avoid mutex contention.
	This field is only used for heuristic purposes; it does not
	affect correctness. */

	access_time = buf_page_is_accessed(&block->page);
	buf_page_set_accessed_make_young(&block->page, access_time);

	ut_ad(!ibuf_inside(mtr)
	      || ibuf_page(buf_block_get_space(block),
			   buf_block_get_zip_size(block),
			   buf_block_get_page_no(block), NULL));

	if (rw_latch == RW_S_LATCH) {
		success = rw_lock_s_lock_nowait(&(block->lock),
						file, line);
		fix_type = MTR_MEMO_PAGE_S_FIX;
	} else {
		success = rw_lock_x_lock_func_nowait_inline(&(block->lock),
							    file, line);
		fix_type = MTR_MEMO_PAGE_X_FIX;
	}

	if (UNIV_UNLIKELY(!success)) {
		mutex_enter(&block->mutex);
		buf_block_buf_fix_dec(block);
		mutex_exit(&block->mutex);

		return(FALSE);
	}

	if (UNIV_UNLIKELY(modify_clock != block->modify_clock)) {
		buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

		if (rw_latch == RW_S_LATCH) {
			rw_lock_s_unlock(&(block->lock));
		} else {
			rw_lock_x_unlock(&(block->lock));
		}

		mutex_enter(&block->mutex);
		buf_block_buf_fix_dec(block);
		mutex_exit(&block->mutex);

		return(FALSE);
	}

	mtr_memo_push(mtr, block, fix_type);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(++buf_dbg_counter % 5771 || buf_validate());
	ut_a(block->page.buf_fix_count > 0);
	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	ut_a(block->page.file_page_was_freed == FALSE);
#endif
	if (innobase_get_slow_log()) {
		trx = innobase_get_trx();
	}

	if (UNIV_UNLIKELY(!access_time)) {
		/* In the case of a first access, try to apply linear
		read-ahead */

		buf_read_ahead_linear(buf_block_get_space(block),
				      buf_block_get_zip_size(block),
				      buf_block_get_page_no(block),
				      ibuf_inside(mtr), trx);
	}

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(buf_block_get_space(block),
			    buf_block_get_page_no(block)) == 0);
#endif
	buf_pool = buf_pool_from_block(block);
	buf_pool->stat.n_page_gets++;

	if (innobase_get_slow_log()) {
		_increment_page_get_statistics(block, trx);
	}
	return(TRUE);
}

/********************************************************************//**
This is used to get access to a known database page, when no waiting can be
done. For example, if a search in an adaptive hash index leads us to this
frame.
@return	TRUE if success */
UNIV_INTERN
ibool
buf_page_get_known_nowait(
/*======================*/
	ulint		rw_latch,/*!< in: RW_S_LATCH, RW_X_LATCH */
	buf_block_t*	block,	/*!< in: the known page */
	ulint		mode,	/*!< in: BUF_MAKE_YOUNG or BUF_KEEP_OLD */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mini-transaction */
{
	buf_pool_t*	buf_pool;
	ibool		success;
	ulint		fix_type;
	trx_t*		trx = NULL;

	ut_ad(mtr);
	ut_ad(mtr->state == MTR_ACTIVE);
	ut_ad((rw_latch == RW_S_LATCH) || (rw_latch == RW_X_LATCH));

	mutex_enter(&block->mutex);

	if (buf_block_get_state(block) == BUF_BLOCK_REMOVE_HASH) {
		/* Another thread is just freeing the block from the LRU list
		of the buffer pool: do not try to access this page; this
		attempt to access the page can only come through the hash
		index because when the buffer block state is ..._REMOVE_HASH,
		we have already removed it from the page address hash table
		of the buffer pool. */

		mutex_exit(&block->mutex);

		return(FALSE);
	}

	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);

	buf_block_buf_fix_inc(block, file, line);

	mutex_exit(&block->mutex);

	buf_pool = buf_pool_from_block(block);

	if (mode == BUF_MAKE_YOUNG && buf_page_peek_if_too_old(&block->page)) {
		//buf_pool_mutex_enter(buf_pool);
		mutex_enter(&buf_pool->LRU_list_mutex);
		buf_LRU_make_block_young(&block->page);
		//buf_pool_mutex_exit(buf_pool);
		mutex_exit(&buf_pool->LRU_list_mutex);
	} else if (!buf_page_is_accessed(&block->page)) {
		/* Above, we do a dirty read on purpose, to avoid
		mutex contention.  The field buf_page_t::access_time
		is only used for heuristic purposes.  Writes to the
		field must be protected by mutex, however. */
		ulint	time_ms = ut_time_ms();

		//buf_pool_mutex_enter(buf_pool);
		mutex_enter(&block->mutex);
		buf_page_set_accessed(&block->page, time_ms);
		//buf_pool_mutex_exit(buf_pool);
		mutex_exit(&block->mutex);
	}

	ut_ad(!ibuf_inside(mtr) || mode == BUF_KEEP_OLD);

	if (rw_latch == RW_S_LATCH) {
		success = rw_lock_s_lock_nowait(&(block->lock),
						file, line);
		fix_type = MTR_MEMO_PAGE_S_FIX;
	} else {
		success = rw_lock_x_lock_func_nowait_inline(&(block->lock),
							    file, line);
		fix_type = MTR_MEMO_PAGE_X_FIX;
	}

	if (!success) {
		mutex_enter(&block->mutex);
		buf_block_buf_fix_dec(block);
		mutex_exit(&block->mutex);

		return(FALSE);
	}

	mtr_memo_push(mtr, block, fix_type);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(++buf_dbg_counter % 5771 || buf_validate());
	ut_a(block->page.buf_fix_count > 0);
	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	ut_a(mode == BUF_KEEP_OLD || !block->page.file_page_was_freed);
#endif

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a((mode == BUF_KEEP_OLD)
	     || (ibuf_count_get(buf_block_get_space(block),
				buf_block_get_page_no(block)) == 0));
#endif
	buf_pool->stat.n_page_gets++;

	if (innobase_get_slow_log()) {
		trx = innobase_get_trx();
		_increment_page_get_statistics(block, trx);
	}

	return(TRUE);
}

/*******************************************************************//**
Given a tablespace id and page number tries to get that page. If the
page is not in the buffer pool it is not loaded and NULL is returned.
Suitable for using when holding the kernel mutex.
@return	pointer to a page or NULL */
UNIV_INTERN
const buf_block_t*
buf_page_try_get_func(
/*==================*/
	ulint		space_id,/*!< in: tablespace id */
	ulint		page_no,/*!< in: page number */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mini-transaction */
{
	buf_block_t*	block;
	ibool		success;
	ulint		fix_type;
	buf_pool_t*	buf_pool = buf_pool_get(space_id, page_no);

	ut_ad(mtr);
	ut_ad(mtr->state == MTR_ACTIVE);

	//buf_pool_mutex_enter(buf_pool);
	rw_lock_s_lock(&buf_pool->page_hash_latch);
	block = buf_block_hash_get(buf_pool, space_id, page_no);

	if (!block || buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE) {
		//buf_pool_mutex_exit(buf_pool);
		rw_lock_s_unlock(&buf_pool->page_hash_latch);
		return(NULL);
	}

	ut_ad(!buf_pool_watch_is_sentinel(buf_pool, &block->page));

	mutex_enter(&block->mutex);
	//buf_pool_mutex_exit(buf_pool);
	rw_lock_s_unlock(&buf_pool->page_hash_latch);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
	ut_a(buf_block_get_space(block) == space_id);
	ut_a(buf_block_get_page_no(block) == page_no);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	buf_block_buf_fix_inc(block, file, line);
	mutex_exit(&block->mutex);

	fix_type = MTR_MEMO_PAGE_S_FIX;
	success = rw_lock_s_lock_nowait(&block->lock, file, line);

	if (!success) {
		/* Let us try to get an X-latch. If the current thread
		is holding an X-latch on the page, we cannot get an
		S-latch. */

		fix_type = MTR_MEMO_PAGE_X_FIX;
		success = rw_lock_x_lock_func_nowait_inline(&block->lock,
							    file, line);
	}

	if (!success) {
		mutex_enter(&block->mutex);
		buf_block_buf_fix_dec(block);
		mutex_exit(&block->mutex);

		return(NULL);
	}

	mtr_memo_push(mtr, block, fix_type);
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(++buf_dbg_counter % 5771 || buf_validate());
	ut_a(block->page.buf_fix_count > 0);
	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	ut_a(block->page.file_page_was_freed == FALSE);
#endif /* UNIV_DEBUG_FILE_ACCESSES || UNIV_DEBUG */
	buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

	buf_pool->stat.n_page_gets++;

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(buf_block_get_space(block),
			    buf_block_get_page_no(block)) == 0);
#endif

	return(block);
}

/********************************************************************//**
Initialize some fields of a control block. */
UNIV_INLINE
void
buf_page_init_low(
/*==============*/
	buf_page_t*	bpage)	/*!< in: block to init */
{
	bpage->flush_type = BUF_FLUSH_LRU;
	bpage->io_fix = BUF_IO_NONE;
	bpage->buf_fix_count = 0;
	bpage->freed_page_clock = 0;
	bpage->access_time = 0;
	bpage->newest_modification = 0;
	bpage->oldest_modification = 0;
	HASH_INVALIDATE(bpage, hash);
	bpage->is_corrupt = FALSE;
#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
	bpage->file_page_was_freed = FALSE;
#endif /* UNIV_DEBUG_FILE_ACCESSES || UNIV_DEBUG */
}

/********************************************************************//**
Inits a page to the buffer buf_pool. */
static __attribute__((nonnull))
void
buf_page_init(
/*==========*/
	buf_pool_t*	buf_pool,/*!< in/out: buffer pool */
	ulint		space,	/*!< in: space id */
	ulint		offset,	/*!< in: offset of the page within space
				in units of a page */
	ulint		fold,	/*!< in: buf_page_address_fold(space,offset) */
	buf_block_t*	block)	/*!< in/out: block to init */
{
	buf_page_t*	hash_page;

	ut_ad(buf_pool == buf_pool_get(space, offset));
	//ut_ad(buf_pool_mutex_own(buf_pool));
#ifdef UNIV_SYNC_DEBUG
	ut_ad(rw_lock_own(&buf_pool->page_hash_latch, RW_LOCK_EX));
#endif
	ut_ad(mutex_own(&(block->mutex)));
	ut_a(buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE);

	/* Set the state of the block */
	buf_block_set_file_page(block, space, offset);

#ifdef UNIV_DEBUG_VALGRIND
	if (!space) {
		/* Silence valid Valgrind warnings about uninitialized
		data being written to data files.  There are some unused
		bytes on some pages that InnoDB does not initialize. */
		UNIV_MEM_VALID(block->frame, UNIV_PAGE_SIZE);
	}
#endif /* UNIV_DEBUG_VALGRIND */

	buf_block_init_low(block);

	block->lock_hash_val = lock_rec_hash(space, offset);

	buf_page_init_low(&block->page);

	/* Insert into the hash table of file pages */

	hash_page = buf_page_hash_get_low(buf_pool, space, offset, fold);

	if (UNIV_LIKELY(!hash_page)) {
	} else if (buf_pool_watch_is_sentinel(buf_pool, hash_page)) {
		/* Preserve the reference count. */
		ulint	buf_fix_count;

		mutex_enter(&buf_pool->zip_mutex);
		buf_fix_count = hash_page->buf_fix_count;
		ut_a(buf_fix_count > 0);
		block->page.buf_fix_count += buf_fix_count;
		buf_pool_watch_remove(buf_pool, fold, hash_page);
		mutex_exit(&buf_pool->zip_mutex);
	} else {
		fprintf(stderr,
			"InnoDB: Error: page %lu %lu already found"
			" in the hash table: %p, %p\n",
			(ulong) space,
			(ulong) offset,
			(const void*) hash_page, (const void*) block);
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		mutex_exit(&block->mutex);
		//buf_pool_mutex_exit(buf_pool);
		rw_lock_x_unlock(&buf_pool->page_hash_latch);
		buf_print();
		buf_LRU_print();
		buf_validate();
		buf_LRU_validate();
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
		ut_error;
	}

	ut_ad(!block->page.in_zip_hash);
	ut_ad(!block->page.in_page_hash);
	ut_d(block->page.in_page_hash = TRUE);
	HASH_INSERT(buf_page_t, hash, buf_pool->page_hash,
		    fold, &block->page);
}

/********************************************************************//**
Function which inits a page for read to the buffer buf_pool. If the page is
(1) already in buf_pool, or
(2) if we specify to read only ibuf pages and the page is not an ibuf page, or
(3) if the space is deleted or being deleted,
then this function does nothing.
Sets the io_fix flag to BUF_IO_READ and sets a non-recursive exclusive lock
on the buffer frame. The io-handler must take care that the flag is cleared
and the lock released later.
@return	pointer to the block or NULL */
UNIV_INTERN
buf_page_t*
buf_page_init_for_read(
/*===================*/
	ulint*		err,	/*!< out: DB_SUCCESS or DB_TABLESPACE_DELETED */
	ulint		mode,	/*!< in: BUF_READ_IBUF_PAGES_ONLY, ... */
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size, or 0 */
	ibool		unzip,	/*!< in: TRUE=request uncompressed page */
	ib_int64_t	tablespace_version,
				/*!< in: prevents reading from a wrong
				version of the tablespace in case we have done
				DISCARD + IMPORT */
	ulint		offset)	/*!< in: page number */
{
	buf_block_t*	block;
	buf_page_t*	bpage	= NULL;
	buf_page_t*	watch_page;
	mtr_t		mtr;
	ulint		fold;
	ibool		lru	= FALSE;
	void*		data;
	buf_pool_t*	buf_pool = buf_pool_get(space, offset);

	ut_ad(buf_pool);

	*err = DB_SUCCESS;

	if (mode == BUF_READ_IBUF_PAGES_ONLY) {
		/* It is a read-ahead within an ibuf routine */

		ut_ad(!ibuf_bitmap_page(zip_size, offset));

		ibuf_mtr_start(&mtr);

		if (!recv_no_ibuf_operations
		    && !ibuf_page(space, zip_size, offset, &mtr)) {

			ibuf_mtr_commit(&mtr);

			return(NULL);
		}
	} else {
		ut_ad(mode == BUF_READ_ANY_PAGE);
	}

	if (zip_size && UNIV_LIKELY(!unzip)
	    && UNIV_LIKELY(!recv_recovery_is_on())) {
		block = NULL;
	} else {
		block = buf_LRU_get_free_block(buf_pool);
		ut_ad(block);
		ut_ad(buf_pool_from_block(block) == buf_pool);
	}

	fold = buf_page_address_fold(space, offset);

retry:
	//buf_pool_mutex_enter(buf_pool);
	mutex_enter(&buf_pool->LRU_list_mutex);
	rw_lock_x_lock(&buf_pool->page_hash_latch);

	watch_page = buf_page_hash_get_low(buf_pool, space, offset, fold);

	if (UNIV_UNLIKELY(watch_page && watch_page->space_was_being_deleted)) {
		mutex_t*	block_mutex = buf_page_get_mutex_enter(watch_page);

		/* This page is obsoleted, should discard and retry */
		rw_lock_x_unlock(&buf_pool->page_hash_latch);
		ut_a(block_mutex);

		buf_LRU_free_block(watch_page, TRUE, TRUE);

		mutex_exit(&buf_pool->LRU_list_mutex);
		mutex_exit(block_mutex);

		goto retry;
	}

	if (watch_page && !buf_pool_watch_is_sentinel(buf_pool, watch_page)) {
		/* The page is already in the buffer pool. */
		watch_page = NULL;
err_exit:
		if (block) {
			mutex_enter(&block->mutex);
			mutex_exit(&buf_pool->LRU_list_mutex);
			rw_lock_x_unlock(&buf_pool->page_hash_latch);
			buf_LRU_block_free_non_file_page(block, FALSE);
			mutex_exit(&block->mutex);
		}
		else {
			mutex_exit(&buf_pool->LRU_list_mutex);
			rw_lock_x_unlock(&buf_pool->page_hash_latch);
		}

		bpage = NULL;
		goto func_exit;
	}

	if (fil_tablespace_deleted_or_being_deleted_in_mem(
		    space, tablespace_version)) {
		/* The page belongs to a space which has been
		deleted or is being deleted. */
		*err = DB_TABLESPACE_DELETED;

		goto err_exit;
	}

	if (block) {
		bpage = &block->page;
		mutex_enter(&block->mutex);

		ut_ad(buf_pool_from_bpage(bpage) == buf_pool);

		buf_page_init(buf_pool, space, offset, fold, block);

		rw_lock_x_unlock(&buf_pool->page_hash_latch);

		/* The block must be put to the LRU list, to the old blocks */
		buf_LRU_add_block(bpage, TRUE/* to old blocks */);

		/* We set a pass-type x-lock on the frame because then
		the same thread which called for the read operation
		(and is running now at this point of code) can wait
		for the read to complete by waiting for the x-lock on
		the frame; if the x-lock were recursive, the same
		thread would illegally get the x-lock before the page
		read is completed.  The x-lock is cleared by the
		io-handler thread. */

		rw_lock_x_lock_gen(&block->lock, BUF_IO_READ);
		buf_page_set_io_fix(bpage, BUF_IO_READ);

		if (UNIV_UNLIKELY(zip_size)) {
			page_zip_set_size(&block->page.zip, zip_size);

			/* buf_pool->mutex may be released and
			reacquired by buf_buddy_alloc().  Thus, we
			must release block->mutex in order not to
			break the latching order in the reacquisition
			of buf_pool->mutex.  We also must defer this
			operation until after the block descriptor has
			been added to buf_pool->LRU and
			buf_pool->page_hash. */
			mutex_exit(&block->mutex);
			data = buf_buddy_alloc(buf_pool, zip_size, &lru, FALSE);
			mutex_enter(&block->mutex);
			block->page.zip.data = data;

			/* To maintain the invariant
			block->in_unzip_LRU_list
			== buf_page_belongs_to_unzip_LRU(&block->page)
			we have to add this block to unzip_LRU
			after block->page.zip.data is set. */
			ut_ad(buf_page_belongs_to_unzip_LRU(&block->page));
			buf_unzip_LRU_add_block(block, TRUE);
		}

		mutex_exit(&buf_pool->LRU_list_mutex);
		mutex_exit(&block->mutex);
	} else {
		/* The compressed page must be allocated before the
		control block (bpage), in order to avoid the
		invocation of buf_buddy_relocate_block() on
		uninitialized data. */
		data = buf_buddy_alloc(buf_pool, zip_size, &lru, TRUE);

		/* If buf_buddy_alloc() allocated storage from the LRU list,
		it released and reacquired buf_pool->mutex.  Thus, we must
		check the page_hash again, as it may have been modified. */
		if (UNIV_UNLIKELY(lru)) {

			watch_page = buf_page_hash_get_low(
				buf_pool, space, offset, fold);

			if (watch_page
			    && !buf_pool_watch_is_sentinel(buf_pool,
				   			   watch_page)) {

				/* The block was added by some other thread. */
				watch_page = NULL;
				buf_buddy_free(buf_pool, data, zip_size, TRUE);

				mutex_exit(&buf_pool->LRU_list_mutex);
				rw_lock_x_unlock(&buf_pool->page_hash_latch);

				bpage = NULL;
				goto func_exit;
			}
		}

		bpage = buf_page_alloc_descriptor();

		/* Initialize the buf_pool pointer. */
		bpage->buf_pool_index = buf_pool_index(buf_pool);

		page_zip_des_init(&bpage->zip);
		page_zip_set_size(&bpage->zip, zip_size);
		bpage->zip.data = data;

		mutex_enter(&buf_pool->zip_mutex);
		UNIV_MEM_DESC(bpage->zip.data,
			      page_zip_get_size(&bpage->zip), bpage);

		buf_page_init_low(bpage);

		bpage->state	= BUF_BLOCK_ZIP_PAGE;
		bpage->space	= space;
		bpage->offset	= offset;
		bpage->space_was_being_deleted = FALSE;

#ifdef UNIV_DEBUG
		bpage->in_page_hash = FALSE;
		bpage->in_zip_hash = FALSE;
		bpage->in_flush_list = FALSE;
		bpage->in_free_list = FALSE;
#endif /* UNIV_DEBUG */
		bpage->in_LRU_list = FALSE;

		ut_d(bpage->in_page_hash = TRUE);

		if (UNIV_LIKELY_NULL(watch_page)) {
			/* Preserve the reference count. */
			ulint	buf_fix_count = watch_page->buf_fix_count;
			ut_a(buf_fix_count > 0);
			bpage->buf_fix_count += buf_fix_count;
			ut_ad(buf_pool_watch_is_sentinel(buf_pool, watch_page));
			buf_pool_watch_remove(buf_pool, fold, watch_page);
		}

		HASH_INSERT(buf_page_t, hash, buf_pool->page_hash, fold,
			    bpage);

		rw_lock_x_unlock(&buf_pool->page_hash_latch);

		/* The block must be put to the LRU list, to the old blocks */
		buf_LRU_add_block(bpage, TRUE/* to old blocks */);
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		buf_LRU_insert_zip_clean(bpage);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

		mutex_exit(&buf_pool->LRU_list_mutex);

		buf_page_set_io_fix(bpage, BUF_IO_READ);

		mutex_exit(&buf_pool->zip_mutex);
	}

	buf_pool_mutex_enter(buf_pool);
	buf_pool->n_pend_reads++;
	buf_pool_mutex_exit(buf_pool);
func_exit:
	//buf_pool_mutex_exit(buf_pool);

	if (mode == BUF_READ_IBUF_PAGES_ONLY) {

		ibuf_mtr_commit(&mtr);
	}

	ut_ad(!bpage || buf_page_in_file(bpage));
	return(bpage);
}

/********************************************************************//**
Initializes a page to the buffer buf_pool. The page is usually not read
from a file even if it cannot be found in the buffer buf_pool. This is one
of the functions which perform to a block a state transition NOT_USED =>
FILE_PAGE (the other is buf_page_get_gen).
@return	pointer to the block, page bufferfixed */
UNIV_INTERN
buf_block_t*
buf_page_create(
/*============*/
	ulint	space,	/*!< in: space id */
	ulint	offset,	/*!< in: offset of the page within space in units of
			a page */
	ulint	zip_size,/*!< in: compressed page size, or 0 */
	mtr_t*	mtr)	/*!< in: mini-transaction handle */
{
	buf_frame_t*	frame;
	buf_block_t*	block;
	ulint		fold;
	buf_block_t*	free_block	= NULL;
	ulint		time_ms		= ut_time_ms();
	buf_pool_t*	buf_pool 	= buf_pool_get(space, offset);

	ut_ad(mtr);
	ut_ad(mtr->state == MTR_ACTIVE);
	ut_ad(space || !zip_size);

	free_block = buf_LRU_get_free_block(buf_pool);

	fold = buf_page_address_fold(space, offset);

retry:
	//buf_pool_mutex_enter(buf_pool);
	mutex_enter(&buf_pool->LRU_list_mutex);
	rw_lock_x_lock(&buf_pool->page_hash_latch);

	block = (buf_block_t*) buf_page_hash_get_low(
		buf_pool, space, offset, fold);

	if (UNIV_UNLIKELY(block && block->page.space_was_being_deleted)) {
		mutex_t*	block_mutex = buf_page_get_mutex_enter((buf_page_t*)block);

		/* This page is obsoleted, should discard and retry */
		rw_lock_x_unlock(&buf_pool->page_hash_latch);
		ut_a(block_mutex);

		buf_LRU_free_block((buf_page_t*)block, TRUE, TRUE);

		mutex_exit(&buf_pool->LRU_list_mutex);
		mutex_exit(block_mutex);

		goto retry;
	}

	if (block
	    && buf_page_in_file(&block->page)
	    && !buf_pool_watch_is_sentinel(buf_pool, &block->page)) {
#ifdef UNIV_IBUF_COUNT_DEBUG
		ut_a(ibuf_count_get(space, offset) == 0);
#endif
#if defined UNIV_DEBUG_FILE_ACCESSES || defined UNIV_DEBUG
		block->page.file_page_was_freed = FALSE;
#endif /* UNIV_DEBUG_FILE_ACCESSES || UNIV_DEBUG */

		/* Page can be found in buf_pool */
		//buf_pool_mutex_exit(buf_pool);
		mutex_exit(&buf_pool->LRU_list_mutex);
		rw_lock_x_unlock(&buf_pool->page_hash_latch);

		buf_block_free(free_block);

		return(buf_page_get_with_no_latch(space, zip_size,
						  offset, mtr));
	}

	/* If we get here, the page was not in buf_pool: init it there */

#ifdef UNIV_DEBUG
	if (buf_debug_prints) {
		fprintf(stderr, "Creating space %lu page %lu to buffer\n",
			(ulong) space, (ulong) offset);
	}
#endif /* UNIV_DEBUG */

	block = free_block;

	mutex_enter(&block->mutex);

	buf_page_init(buf_pool, space, offset, fold, block);
	rw_lock_x_unlock(&buf_pool->page_hash_latch);

	/* The block must be put to the LRU list */
	buf_LRU_add_block(&block->page, FALSE);

	buf_block_buf_fix_inc(block, __FILE__, __LINE__);
	buf_pool->stat.n_pages_created++;

	if (zip_size) {
		void*	data;
		ibool	lru;

		/* Prevent race conditions during buf_buddy_alloc(),
		which may release and reacquire buf_pool->mutex,
		by IO-fixing and X-latching the block. */

		buf_page_set_io_fix(&block->page, BUF_IO_READ);
		rw_lock_x_lock(&block->lock);

		page_zip_set_size(&block->page.zip, zip_size);
		mutex_exit(&block->mutex);
		/* buf_pool->mutex may be released and reacquired by
		buf_buddy_alloc().  Thus, we must release block->mutex
		in order not to break the latching order in
		the reacquisition of buf_pool->mutex.  We also must
		defer this operation until after the block descriptor
		has been added to buf_pool->LRU and buf_pool->page_hash. */
		data = buf_buddy_alloc(buf_pool, zip_size, &lru, FALSE);
		mutex_enter(&block->mutex);
		block->page.zip.data = data;

		/* To maintain the invariant
		block->in_unzip_LRU_list
		== buf_page_belongs_to_unzip_LRU(&block->page)
		we have to add this block to unzip_LRU after
		block->page.zip.data is set. */
		ut_ad(buf_page_belongs_to_unzip_LRU(&block->page));
		buf_unzip_LRU_add_block(block, FALSE);

		buf_page_set_io_fix(&block->page, BUF_IO_NONE);
		rw_lock_x_unlock(&block->lock);
	}

	buf_page_set_accessed(&block->page, time_ms);

	//buf_pool_mutex_exit(buf_pool);
	mutex_exit(&buf_pool->LRU_list_mutex);

	mtr_memo_push(mtr, block, MTR_MEMO_BUF_FIX);

	mutex_exit(&block->mutex);

	/* Delete possible entries for the page from the insert buffer:
	such can exist if the page belonged to an index which was dropped */

	ibuf_merge_or_delete_for_page(NULL, space, offset, zip_size, TRUE);

	/* Flush pages from the end of the LRU list if necessary */
	buf_flush_free_margin(buf_pool, FALSE);

	frame = block->frame;

	memset(frame + FIL_PAGE_PREV, 0xff, 4);
	memset(frame + FIL_PAGE_NEXT, 0xff, 4);
	mach_write_to_2(frame + FIL_PAGE_TYPE, FIL_PAGE_TYPE_ALLOCATED);

	/* Reset to zero the file flush lsn field in the page; if the first
	page of an ibdata file is 'created' in this function into the buffer
	pool then we lose the original contents of the file flush lsn stamp.
	Then InnoDB could in a crash recovery print a big, false, corruption
	warning if the stamp contains an lsn bigger than the ib_logfile lsn. */

	memset(frame + FIL_PAGE_FILE_FLUSH_LSN, 0, 8);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(++buf_dbg_counter % 357 || buf_validate());
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(buf_block_get_space(block),
			    buf_block_get_page_no(block)) == 0);
#endif
	return(block);
}

/********************************************************************//**
Mark a table with the specified space pointed by bpage->space corrupted.
Also remove the bpage from LRU list.
@return TRUE if successful */
static
ibool
buf_mark_space_corrupt(
/*===================*/
	buf_page_t*	bpage)	/*!< in: pointer to the block in question */
{
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
	const ibool	uncompressed = (buf_page_get_state(bpage)
					== BUF_BLOCK_FILE_PAGE);
	ulint		space = bpage->space;
	ibool		ret = TRUE;

	/* First unfix and release lock on the bpage */
	//buf_pool_mutex_enter(buf_pool);
	mutex_enter(&buf_pool->LRU_list_mutex);
	rw_lock_x_lock(&buf_pool->page_hash_latch);
	mutex_enter(buf_page_get_mutex(bpage));
	ut_ad(buf_page_get_io_fix(bpage) == BUF_IO_READ);
	ut_ad(bpage->buf_fix_count == 0);

	/* Set BUF_IO_NONE before we remove the block from LRU list */
	buf_page_set_io_fix(bpage, BUF_IO_NONE);

	if (uncompressed) {
		rw_lock_x_unlock_gen(
			&((buf_block_t*) bpage)->lock,
			BUF_IO_READ);
	}

	/* Find the table with specified space id, and mark it corrupted */
	if (dict_set_corrupted_by_space(space)) {
		buf_LRU_free_one_page(bpage);
	} else {
		ret = FALSE;
	}

	buf_pool_mutex_enter(buf_pool);
	ut_ad(buf_pool->n_pend_reads > 0);
	buf_pool->n_pend_reads--;
	buf_pool_mutex_exit(buf_pool);

	mutex_exit(buf_page_get_mutex(bpage));
	//buf_pool_mutex_exit(buf_pool);
	mutex_exit(&buf_pool->LRU_list_mutex);
	rw_lock_x_unlock(&buf_pool->page_hash_latch);

	return(ret);
}

/********************************************************************//**
Completes an asynchronous read or write request of a file page to or from
the buffer pool.
@return TRUE if successful */
UNIV_INTERN
ibool
buf_page_io_complete(
/*=================*/
	buf_page_t*	bpage)	/*!< in: pointer to the block in question */
{
	enum buf_io_fix	io_type;
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
	const ibool	uncompressed = (buf_page_get_state(bpage)
					== BUF_BLOCK_FILE_PAGE);
	ibool		have_LRU_mutex = FALSE;
	mutex_t*	block_mutex;

	ut_a(buf_page_in_file(bpage));

	/* We do not need protect io_fix here by mutex to read
	it because this is the only function where we can change the value
	from BUF_IO_READ or BUF_IO_WRITE to some other value, and our code
	ensures that this is the only thread that handles the i/o for this
	block. */

	io_type = buf_page_get_io_fix(bpage);
	ut_ad(io_type == BUF_IO_READ || io_type == BUF_IO_WRITE);

	if (io_type == BUF_IO_READ) {
		ulint	read_page_no;
		ulint	read_space_id;
		byte*	frame;

		if (buf_page_get_zip_size(bpage)) {
			frame = bpage->zip.data;
			buf_pool->n_pend_unzip++;
			if (uncompressed
			    && !buf_zip_decompress((buf_block_t*) bpage,
						   FALSE)) {

				buf_pool->n_pend_unzip--;
				goto corrupt;
			}
			buf_pool->n_pend_unzip--;
		} else {
			ut_a(uncompressed);
			frame = ((buf_block_t*) bpage)->frame;
		}

		/* If this page is not uninitialized and not in the
		doublewrite buffer, then the page number and space id
		should be the same as in block. */
		read_page_no = mach_read_from_4(frame + FIL_PAGE_OFFSET);
		read_space_id = mach_read_from_4(
			frame + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

		if ((bpage->space == TRX_SYS_SPACE
		     || (srv_doublewrite_file && bpage->space == TRX_DOUBLEWRITE_SPACE))
		    && trx_doublewrite_page_inside(bpage->offset)) {

			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Error: reading page %lu\n"
				"InnoDB: which is in the"
				" doublewrite buffer!\n",
				(ulong) bpage->offset);
		} else if (!read_space_id && !read_page_no) {
			/* This is likely an uninitialized page. */
		} else if ((bpage->space
			    && bpage->space != read_space_id)
			   || bpage->offset != read_page_no) {
			/* We did not compare space_id to read_space_id
			if bpage->space == 0, because the field on the
			page may contain garbage in MySQL < 4.1.1,
			which only supported bpage->space == 0. */

			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: Error: space id and page n:o"
				" stored in the page\n"
				"InnoDB: read in are %lu:%lu,"
				" should be %lu:%lu!\n",
				(ulong) read_space_id, (ulong) read_page_no,
				(ulong) bpage->space,
				(ulong) bpage->offset);
		}

		if (!srv_pass_corrupt_table || !bpage->is_corrupt) {
		/* From version 3.23.38 up we store the page checksum
		to the 4 first bytes of the page end lsn field */

		if (buf_page_is_corrupted(frame,
					  buf_page_get_zip_size(bpage))) {
corrupt:
			fprintf(stderr,
				"InnoDB: Database page corruption on disk"
				" or a failed\n"
				"InnoDB: file read of page %lu.\n"
				"InnoDB: You may have to recover"
				" from a backup.\n",
				(ulong) bpage->offset);
			buf_page_print(frame, buf_page_get_zip_size(bpage),
				       BUF_PAGE_PRINT_NO_CRASH);
			fprintf(stderr,
				"InnoDB: Database page corruption on disk"
				" or a failed\n"
				"InnoDB: file read of page %lu.\n"
				"InnoDB: You may have to recover"
				" from a backup.\n",
				(ulong) bpage->offset);
			fputs("InnoDB: It is also possible that"
			      " your operating\n"
			      "InnoDB: system has corrupted its"
			      " own file cache\n"
			      "InnoDB: and rebooting your computer"
			      " removes the\n"
			      "InnoDB: error.\n"
			      "InnoDB: If the corrupt page is an index page\n"
			      "InnoDB: you can also try to"
			      " fix the corruption\n"
			      "InnoDB: by dumping, dropping,"
			      " and reimporting\n"
			      "InnoDB: the corrupt table."
			      " You can use CHECK\n"
			      "InnoDB: TABLE to scan your"
			      " table for corruption.\n"
			      "InnoDB: See also "
			      REFMAN "forcing-innodb-recovery.html\n"
			      "InnoDB: about forcing recovery.\n", stderr);

			if (srv_pass_corrupt_table && !trx_sys_sys_space(bpage->space)
			    && bpage->space < SRV_LOG_SPACE_FIRST_ID) {
				trx_t*	trx;

				fprintf(stderr,
					"InnoDB: space %u will be treated as corrupt.\n",
					bpage->space);
				fil_space_set_corrupt(bpage->space);

				trx = innobase_get_trx();
				if (trx && trx->dict_operation_lock_mode == RW_X_LATCH) {
					dict_table_set_corrupt_by_space(bpage->space, FALSE);
				} else {
					dict_table_set_corrupt_by_space(bpage->space, TRUE);
				}
				bpage->is_corrupt = TRUE;
			} else
			if (srv_force_recovery < SRV_FORCE_IGNORE_CORRUPT) {
				/* If page space id is larger than TRX_SYS_SPACE
				(0), we will attempt to mark the corresponding
				table as corrupted instead of crashing server */
				if (bpage->space > TRX_SYS_SPACE
				    && buf_mark_space_corrupt(bpage)) {
					return(FALSE);
				} else {
					fputs("InnoDB: Ending processing"
					      " because of"
					      " a corrupt database page.\n",
					      stderr);
					ut_error;
				}
			}
		}
		} /**/

		if (recv_recovery_is_on()) {
			/* Pages must be uncompressed for crash recovery. */
			ut_a(uncompressed);
			recv_recover_page(TRUE, (buf_block_t*) bpage);
		}

		if (uncompressed && !recv_no_ibuf_operations) {
			ibuf_merge_or_delete_for_page(
				/* Delete possible entries, if bpage is_corrupt */
				(srv_pass_corrupt_table && bpage->is_corrupt) ? NULL :
				(buf_block_t*) bpage, bpage->space,
				bpage->offset, buf_page_get_zip_size(bpage),
				(srv_pass_corrupt_table && bpage->is_corrupt) ? FALSE :
				TRUE);
		}
	}

	if (io_type == BUF_IO_WRITE
	    && (
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		buf_page_get_state(bpage) == BUF_BLOCK_ZIP_DIRTY ||
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
		buf_page_get_flush_type(bpage) == BUF_FLUSH_LRU)) {
		/* to keep consistency at buf_LRU_insert_zip_clean() */
		have_LRU_mutex = TRUE; /* optimistic */
	}
retry_mutex:
	if (have_LRU_mutex)
		mutex_enter(&buf_pool->LRU_list_mutex);
	block_mutex = buf_page_get_mutex_enter(bpage);
	ut_a(block_mutex);
	if (io_type == BUF_IO_WRITE
	    && (buf_page_get_state(bpage) == BUF_BLOCK_ZIP_DIRTY
		|| buf_page_get_flush_type(bpage) == BUF_FLUSH_LRU)
	    && !have_LRU_mutex) {
		mutex_exit(block_mutex);
		have_LRU_mutex = TRUE;
		goto retry_mutex;
	}
	buf_pool_mutex_enter(buf_pool);

#ifdef UNIV_IBUF_COUNT_DEBUG
	if (io_type == BUF_IO_WRITE || uncompressed) {
		/* For BUF_IO_READ of compressed-only blocks, the
		buffered operations will be merged by buf_page_get_gen()
		after the block has been uncompressed. */
		ut_a(ibuf_count_get(bpage->space, bpage->offset) == 0);
	}
#endif
	/* Because this thread which does the unlocking is not the same that
	did the locking, we use a pass value != 0 in unlock, which simply
	removes the newest lock debug record, without checking the thread
	id. */

	buf_page_set_io_fix(bpage, BUF_IO_NONE);

	switch (io_type) {
	case BUF_IO_READ:
		/* NOTE that the call to ibuf may have moved the ownership of
		the x-latch to this OS thread: do not let this confuse you in
		debugging! */

		ut_a(!have_LRU_mutex);
		ut_ad(buf_pool->n_pend_reads > 0);
		buf_pool->n_pend_reads--;
		buf_pool->stat.n_pages_read++;

		if (uncompressed) {
			rw_lock_x_unlock_gen(&((buf_block_t*) bpage)->lock,
					     BUF_IO_READ);
		}

		break;

	case BUF_IO_WRITE:
		/* Write means a flush operation: call the completion
		routine in the flush system */

		buf_flush_write_complete(bpage);

		if (have_LRU_mutex)
			mutex_exit(&buf_pool->LRU_list_mutex);

		if (uncompressed) {
			rw_lock_s_unlock_gen(&((buf_block_t*) bpage)->lock,
					     BUF_IO_WRITE);
		}

		buf_pool->stat.n_pages_written++;

		break;

	default:
		ut_error;
	}

#ifdef UNIV_DEBUG
	if (buf_debug_prints) {
		fprintf(stderr, "Has %s page space %lu page no %lu\n",
			io_type == BUF_IO_READ ? "read" : "written",
			(ulong) buf_page_get_space(bpage),
			(ulong) buf_page_get_page_no(bpage));
	}
#endif /* UNIV_DEBUG */

	buf_pool_mutex_exit(buf_pool);
	mutex_exit(block_mutex);

	return(TRUE);
}

/********************************************************************//**
*/
UNIV_INTERN
buf_block_t*
buf_page_from_array(
/*================*/
	buf_pool_t*	buf_pool,
	ulint		n_block)
{
	ulint		n_chunks, offset;
	buf_chunk_t*	chunk;

	ut_a(n_block < buf_pool->curr_size);

	chunk = buf_pool->chunks;
	offset = n_block;

	for (n_chunks = buf_pool->n_chunks; n_chunks--; chunk++) {
		if (offset < chunk->size) {
			return(&chunk->blocks[offset]);
		}

		offset -= chunk->size;
	}

	ut_error;

	return(NULL);
}

/*********************************************************************//**
Asserts that all file pages in the buffer are in a replaceable state.
@return	TRUE */
static
ibool
buf_all_freed_instance(
/*===================*/
	buf_pool_t*	buf_pool)	/*!< in: buffer pool instancce */
{
	ulint		i;
	buf_chunk_t*	chunk;

	ut_ad(buf_pool);

	//buf_pool_mutex_enter(buf_pool);
	mutex_enter(&buf_pool->LRU_list_mutex);
	rw_lock_x_lock(&buf_pool->page_hash_latch);

	chunk = buf_pool->chunks;

	for (i = buf_pool->n_chunks; i--; chunk++) {

		const buf_block_t* block = buf_chunk_not_freed(chunk);

		if (UNIV_LIKELY_NULL(block)) {
			fprintf(stderr,
				"Page %lu %lu still fixed or dirty\n",
				(ulong) block->page.space,
				(ulong) block->page.offset);
			ut_error;
		}
	}

	//buf_pool_mutex_exit(buf_pool);
	mutex_exit(&buf_pool->LRU_list_mutex);
	rw_lock_x_unlock(&buf_pool->page_hash_latch);

	return(TRUE);
}

/*********************************************************************//**
Invalidates file pages in one buffer pool instance */
static
void
buf_pool_invalidate_instance(
/*=========================*/
	buf_pool_t*	buf_pool)	/*!< in: buffer pool instance */
{
	ibool		freed;
	enum buf_flush	i;

	buf_pool_mutex_enter(buf_pool);

	for (i = BUF_FLUSH_LRU; i < BUF_FLUSH_N_TYPES; i++) {

		/* As this function is called during startup and
		during redo application phase during recovery, InnoDB
		is single threaded (apart from IO helper threads) at
		this stage. No new write batch can be in intialization
		stage at this point. */
		ut_ad(buf_pool->init_flush[i] == FALSE);

		/* However, it is possible that a write batch that has
		been posted earlier is still not complete. For buffer
		pool invalidation to proceed we must ensure there is NO
		write activity happening. */
		if (buf_pool->n_flush[i] > 0) {
			buf_pool_mutex_exit(buf_pool);
			buf_flush_wait_batch_end(buf_pool, i);
			buf_pool_mutex_enter(buf_pool);
		}
	}

	buf_pool_mutex_exit(buf_pool);

	ut_ad(buf_all_freed_instance(buf_pool));

	freed = TRUE;

	while (freed) {
		freed = buf_LRU_search_and_free_block(buf_pool, 100);
	}

	//buf_pool_mutex_enter(buf_pool);
	mutex_enter(&buf_pool->LRU_list_mutex);

	ut_ad(UT_LIST_GET_LEN(buf_pool->LRU) == 0);
	ut_ad(UT_LIST_GET_LEN(buf_pool->unzip_LRU) == 0);

	buf_pool->freed_page_clock = 0;
	buf_pool->LRU_old = NULL;
	buf_pool->LRU_old_len = 0;
	buf_pool->LRU_flush_ended = 0;

	memset(&buf_pool->stat, 0x00, sizeof(buf_pool->stat));
	buf_refresh_io_stats(buf_pool);

	//buf_pool_mutex_exit(buf_pool);
	mutex_exit(&buf_pool->LRU_list_mutex);
}

/*********************************************************************//**
Invalidates the file pages in the buffer pool when an archive recovery is
completed. All the file pages buffered must be in a replaceable state when
this function is called: not latched and not modified. */
UNIV_INTERN
void
buf_pool_invalidate(void)
/*=====================*/
{
	ulint   i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_invalidate_instance(buf_pool_from_array(i));
	}
}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/*********************************************************************//**
Validates data in one buffer pool instance
@return	TRUE */
static
ibool
buf_pool_validate_instance(
/*=======================*/
	buf_pool_t*	buf_pool)	/*!< in: buffer pool instance */
{
	buf_page_t*	b;
	buf_chunk_t*	chunk;
	ulint		i;
	ulint		n_single_flush	= 0;
	ulint		n_lru_flush	= 0;
	ulint		n_list_flush	= 0;
	ulint		n_lru		= 0;
	ulint		n_flush		= 0;
	ulint		n_free		= 0;
	ulint		n_zip		= 0;

	ut_ad(buf_pool);

	//buf_pool_mutex_enter(buf_pool);
	mutex_enter(&buf_pool->LRU_list_mutex);
	rw_lock_x_lock(&buf_pool->page_hash_latch);
	/* for keep the new latch order, it cannot validate correctly... */

	chunk = buf_pool->chunks;

	/* Check the uncompressed blocks. */

	for (i = buf_pool->n_chunks; i--; chunk++) {

		ulint		j;
		buf_block_t*	block = chunk->blocks;

		for (j = chunk->size; j--; block++) {

			mutex_enter(&block->mutex);

			switch (buf_block_get_state(block)) {
			case BUF_BLOCK_ZIP_FREE:
			case BUF_BLOCK_ZIP_PAGE:
			case BUF_BLOCK_ZIP_DIRTY:
				/* These should only occur on
				zip_clean, zip_free[], or flush_list. */
				ut_error;
				break;

			case BUF_BLOCK_FILE_PAGE:
				ut_a(buf_page_hash_get(buf_pool,
						       buf_block_get_space(
							       block),
						       buf_block_get_page_no(
							       block))
				     == &block->page);

#ifdef UNIV_IBUF_COUNT_DEBUG
				ut_a(buf_page_get_io_fix(&block->page)
				     == BUF_IO_READ
				     || !ibuf_count_get(buf_block_get_space(
								block),
							buf_block_get_page_no(
								block)));
#endif
				switch (buf_page_get_io_fix(&block->page)) {
				case BUF_IO_NONE:
					break;

				case BUF_IO_WRITE:
					switch (buf_page_get_flush_type(
							&block->page)) {
					case BUF_FLUSH_LRU:
						n_lru_flush++;
						ut_a(rw_lock_is_locked(
							     &block->lock,
							     RW_LOCK_SHARED));
						break;
					case BUF_FLUSH_LIST:
						n_list_flush++;
						break;
					case BUF_FLUSH_SINGLE_PAGE:
						n_single_flush++;
						break;
					default:
						ut_error;
					}

					break;

				case BUF_IO_READ:

					ut_a(rw_lock_is_locked(&block->lock,
							       RW_LOCK_EX));
					break;

				case BUF_IO_PIN:
					break;
				}

				n_lru++;
				break;

			case BUF_BLOCK_NOT_USED:
				n_free++;
				break;

			case BUF_BLOCK_READY_FOR_USE:
			case BUF_BLOCK_MEMORY:
			case BUF_BLOCK_REMOVE_HASH:
				/* do nothing */
				break;
			}

			mutex_exit(&block->mutex);
		}
	}

	mutex_enter(&buf_pool->zip_mutex);

	/* Check clean compressed-only blocks. */

	for (b = UT_LIST_GET_FIRST(buf_pool->zip_clean); b;
	     b = UT_LIST_GET_NEXT(zip_list, b)) {
		ut_a(buf_page_get_state(b) == BUF_BLOCK_ZIP_PAGE);
		switch (buf_page_get_io_fix(b)) {
		case BUF_IO_NONE:
		case BUF_IO_PIN:
			/* All clean blocks should be I/O-unfixed. */
			break;
		case BUF_IO_READ:
			/* In buf_LRU_free_block(), we temporarily set
			b->io_fix = BUF_IO_READ for a newly allocated
			control block in order to prevent
			buf_page_get_gen() from decompressing the block. */
			break;
		default:
			ut_error;
			break;
		}

		/* It is OK to read oldest_modification here because
		we have acquired buf_pool->zip_mutex above which acts
		as the 'block->mutex' for these bpages. */
		ut_a(!b->oldest_modification);
		ut_a(buf_page_hash_get(buf_pool, b->space, b->offset) == b);

		n_lru++;
		n_zip++;
	}

	/* Check dirty blocks. */

	buf_flush_list_mutex_enter(buf_pool);
	for (b = UT_LIST_GET_FIRST(buf_pool->flush_list); b;
	     b = UT_LIST_GET_NEXT(flush_list, b)) {
		ut_ad(b->in_flush_list);
		ut_a(b->oldest_modification);
		n_flush++;

		switch (buf_page_get_state(b)) {
		case BUF_BLOCK_ZIP_DIRTY:
			n_lru++;
			n_zip++;
			switch (buf_page_get_io_fix(b)) {
			case BUF_IO_NONE:
			case BUF_IO_READ:
			case BUF_IO_PIN:
				break;
			case BUF_IO_WRITE:
				switch (buf_page_get_flush_type(b)) {
				case BUF_FLUSH_LRU:
					n_lru_flush++;
					break;
				case BUF_FLUSH_LIST:
					n_list_flush++;
					break;
				case BUF_FLUSH_SINGLE_PAGE:
					n_single_flush++;
					break;
				default:
					ut_error;
				}
				break;
			}
			break;
		case BUF_BLOCK_FILE_PAGE:
			/* uncompressed page */
			break;
		case BUF_BLOCK_ZIP_FREE:
		case BUF_BLOCK_ZIP_PAGE:
		case BUF_BLOCK_NOT_USED:
		case BUF_BLOCK_READY_FOR_USE:
		case BUF_BLOCK_MEMORY:
		case BUF_BLOCK_REMOVE_HASH:
			ut_error;
			break;
		}
		ut_a(buf_page_hash_get(buf_pool, b->space, b->offset) == b);
	}

	ut_a(UT_LIST_GET_LEN(buf_pool->flush_list) == n_flush);

	buf_flush_list_mutex_exit(buf_pool);

	mutex_exit(&buf_pool->zip_mutex);

	if (n_lru + n_free > buf_pool->curr_size + n_zip) {
		fprintf(stderr, "n LRU %lu, n free %lu, pool %lu zip %lu\n",
			(ulong) n_lru, (ulong) n_free,
			(ulong) buf_pool->curr_size, (ulong) n_zip);
		ut_error;
	}

	ut_a(UT_LIST_GET_LEN(buf_pool->LRU) == n_lru);
	/* because of latching order with block->mutex, we cannot get needed mutexes before that */
/*
	if (UT_LIST_GET_LEN(buf_pool->free) != n_free) {
		fprintf(stderr, "Free list len %lu, free blocks %lu\n",
			(ulong) UT_LIST_GET_LEN(buf_pool->free),
			(ulong) n_free);
		ut_error;
	}

	ut_a(buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE] == n_single_flush);
	ut_a(buf_pool->n_flush[BUF_FLUSH_LIST] == n_list_flush);
	ut_a(buf_pool->n_flush[BUF_FLUSH_LRU] == n_lru_flush);
*/

	//buf_pool_mutex_exit(buf_pool);
	mutex_exit(&buf_pool->LRU_list_mutex);
	rw_lock_x_unlock(&buf_pool->page_hash_latch);

	ut_a(buf_LRU_validate());
	ut_a(buf_flush_validate(buf_pool));

	return(TRUE);
}

/*********************************************************************//**
Validates the buffer buf_pool data structure.
@return	TRUE */
UNIV_INTERN
ibool
buf_validate(void)
/*==============*/
{
	ulint	i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_pool_validate_instance(buf_pool);
	}
	return(TRUE);
}

#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

#if defined UNIV_DEBUG_PRINT || defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/*********************************************************************//**
Prints info of the buffer buf_pool data structure for one instance. */
static
void
buf_print_instance(
/*===============*/
	buf_pool_t*	buf_pool)
{
	index_id_t*	index_ids;
	ulint*		counts;
	ulint		size;
	ulint		i;
	ulint		j;
	index_id_t	id;
	ulint		n_found;
	buf_chunk_t*	chunk;
	dict_index_t*	index;

	ut_ad(buf_pool);

	size = buf_pool->curr_size;

	index_ids = mem_alloc(size * sizeof *index_ids);
	counts = mem_alloc(sizeof(ulint) * size);

	//buf_pool_mutex_enter(buf_pool);
	mutex_enter(&buf_pool->LRU_list_mutex);
	mutex_enter(&buf_pool->free_list_mutex);
	buf_flush_list_mutex_enter(buf_pool);

	fprintf(stderr,
		"buf_pool size %lu\n"
		"database pages %lu\n"
		"free pages %lu\n"
		"modified database pages %lu\n"
		"n pending decompressions %lu\n"
		"n pending reads %lu\n"
		"n pending flush LRU %lu list %lu single page %lu\n"
		"pages made young %lu, not young %lu\n"
		"pages read %lu, created %lu, written %lu\n",
		(ulong) size,
		(ulong) UT_LIST_GET_LEN(buf_pool->LRU),
		(ulong) UT_LIST_GET_LEN(buf_pool->free),
		(ulong) UT_LIST_GET_LEN(buf_pool->flush_list),
		(ulong) buf_pool->n_pend_unzip,
		(ulong) buf_pool->n_pend_reads,
		(ulong) buf_pool->n_flush[BUF_FLUSH_LRU],
		(ulong) buf_pool->n_flush[BUF_FLUSH_LIST],
		(ulong) buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE],
		(ulong) buf_pool->stat.n_pages_made_young,
		(ulong) buf_pool->stat.n_pages_not_made_young,
		(ulong) buf_pool->stat.n_pages_read,
		(ulong) buf_pool->stat.n_pages_created,
		(ulong) buf_pool->stat.n_pages_written);

	buf_flush_list_mutex_exit(buf_pool);

	/* Count the number of blocks belonging to each index in the buffer */

	n_found = 0;

	chunk = buf_pool->chunks;

	for (i = buf_pool->n_chunks; i--; chunk++) {
		buf_block_t*	block		= chunk->blocks;
		ulint		n_blocks	= chunk->size;

		for (; n_blocks--; block++) {
			const buf_frame_t* frame = block->frame;

			if (fil_page_get_type(frame) == FIL_PAGE_INDEX) {

				id = btr_page_get_index_id(frame);

				/* Look for the id in the index_ids array */
				j = 0;

				while (j < n_found) {

					if (index_ids[j] == id) {
						counts[j]++;

						break;
					}
					j++;
				}

				if (j == n_found) {
					n_found++;
					index_ids[j] = id;
					counts[j] = 1;
				}
			}
		}
	}

	//buf_pool_mutex_exit(buf_pool);
	mutex_exit(&buf_pool->LRU_list_mutex);
	mutex_exit(&buf_pool->free_list_mutex);

	for (i = 0; i < n_found; i++) {
		index = dict_index_get_if_in_cache(index_ids[i]);

		fprintf(stderr,
			"Block count for index %llu in buffer is about %lu",
			(ullint) index_ids[i],
			(ulong) counts[i]);

		if (index) {
			putc(' ', stderr);
			dict_index_name_print(stderr, NULL, index);
		}

		putc('\n', stderr);
	}

	mem_free(index_ids);
	mem_free(counts);

	ut_a(buf_pool_validate_instance(buf_pool));
}

/*********************************************************************//**
Prints info of the buffer buf_pool data structure. */
UNIV_INTERN
void
buf_print(void)
/*===========*/
{
	ulint   i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);
		buf_print_instance(buf_pool);
	}
}
#endif /* UNIV_DEBUG_PRINT || UNIV_DEBUG || UNIV_BUF_DEBUG */

#ifdef UNIV_DEBUG
/*********************************************************************//**
Returns the number of latched pages in the buffer pool.
@return	number of latched pages */
UNIV_INTERN
ulint
buf_get_latched_pages_number_instance(
/*==================================*/
	buf_pool_t*	buf_pool)	/*!< in: buffer pool instance */
{
	buf_page_t*	b;
	ulint		i;
	buf_chunk_t*	chunk;
	ulint		fixed_pages_number = 0;

	//buf_pool_mutex_enter(buf_pool);

	chunk = buf_pool->chunks;

	for (i = buf_pool->n_chunks; i--; chunk++) {
		buf_block_t*	block;
		ulint		j;

		block = chunk->blocks;

		for (j = chunk->size; j--; block++) {
			if (buf_block_get_state(block)
			    != BUF_BLOCK_FILE_PAGE) {

				continue;
			}

			mutex_enter(&block->mutex);

			if (block->page.buf_fix_count != 0
			    || buf_page_get_io_fix(&block->page)
			    != BUF_IO_NONE) {
				fixed_pages_number++;
			}

			mutex_exit(&block->mutex);
		}
	}

	mutex_enter(&buf_pool->zip_mutex);

	/* Traverse the lists of clean and dirty compressed-only blocks. */

	for (b = UT_LIST_GET_FIRST(buf_pool->zip_clean); b;
	     b = UT_LIST_GET_NEXT(zip_list, b)) {
		ut_a(buf_page_get_state(b) == BUF_BLOCK_ZIP_PAGE);
		ut_a(buf_page_get_io_fix(b) != BUF_IO_WRITE);

		if (b->buf_fix_count != 0
		    || buf_page_get_io_fix(b) != BUF_IO_NONE) {
			fixed_pages_number++;
		}
	}

	buf_flush_list_mutex_enter(buf_pool);
	for (b = UT_LIST_GET_FIRST(buf_pool->flush_list); b;
	     b = UT_LIST_GET_NEXT(flush_list, b)) {
		ut_ad(b->in_flush_list);

		switch (buf_page_get_state(b)) {
		case BUF_BLOCK_ZIP_DIRTY:
			if (b->buf_fix_count != 0
			    || buf_page_get_io_fix(b) != BUF_IO_NONE) {
				fixed_pages_number++;
			}
			break;
		case BUF_BLOCK_FILE_PAGE:
			/* uncompressed page */
			break;
		case BUF_BLOCK_ZIP_FREE:
		case BUF_BLOCK_ZIP_PAGE:
		case BUF_BLOCK_NOT_USED:
		case BUF_BLOCK_READY_FOR_USE:
		case BUF_BLOCK_MEMORY:
		case BUF_BLOCK_REMOVE_HASH:
			ut_error;
			break;
		}
	}

	buf_flush_list_mutex_exit(buf_pool);
	mutex_exit(&buf_pool->zip_mutex);
	//buf_pool_mutex_exit(buf_pool);

	return(fixed_pages_number);
}

/*********************************************************************//**
Returns the number of latched pages in all the buffer pools.
@return	number of latched pages */
UNIV_INTERN
ulint
buf_get_latched_pages_number(void)
/*==============================*/
{
	ulint	i;
	ulint	total_latched_pages = 0;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		total_latched_pages += buf_get_latched_pages_number_instance(
			buf_pool);
	}

	return(total_latched_pages);
}

#endif /* UNIV_DEBUG */

/*********************************************************************//**
Returns the number of pending buf pool ios.
@return	number of pending I/O operations */
UNIV_INTERN
ulint
buf_get_n_pending_ios(void)
/*=======================*/
{
	ulint	i;
	ulint	pend_ios = 0;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		pend_ios +=
			buf_pool->n_pend_reads
			+ buf_pool->n_flush[BUF_FLUSH_LRU]
			+ buf_pool->n_flush[BUF_FLUSH_LIST]
			+ buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE];
	}

	return(pend_ios);
}

/*********************************************************************//**
Returns the ratio in percents of modified pages in the buffer pool /
database pages in the buffer pool.
@return	modified page percentage ratio */
UNIV_INTERN
ulint
buf_get_modified_ratio_pct(void)
/*============================*/
{
	ulint		ratio;
	ulint		lru_len = 0;
	ulint		free_len = 0;
	ulint		flush_list_len = 0;

	buf_get_total_list_len(&lru_len, &free_len, &flush_list_len);

	ratio = (100 * flush_list_len) / (1 + lru_len + free_len);

	/* 1 + is there to avoid division by zero */

	return(ratio);
}

/*******************************************************************//**
Aggregates a pool stats information with the total buffer pool stats  */
static
void
buf_stats_aggregate_pool_info(
/*==========================*/
	buf_pool_info_t*	total_info,	/*!< in/out: the buffer pool
						info to store aggregated
						result */
	const buf_pool_info_t*	pool_info)	/*!< in: individual buffer pool
						stats info */
{
	ut_a(total_info && pool_info);

	/* Nothing to copy if total_info is the same as pool_info */
	if (total_info == pool_info) {
		return;
	}

	total_info->pool_size += pool_info->pool_size;
	total_info->pool_size_bytes += pool_info->pool_size_bytes;
	total_info->lru_len += pool_info->lru_len;
	total_info->old_lru_len += pool_info->old_lru_len;
	total_info->free_list_len += pool_info->free_list_len;
	total_info->flush_list_len += pool_info->flush_list_len;
	total_info->n_pend_unzip += pool_info->n_pend_unzip;
	total_info->n_pend_reads += pool_info->n_pend_reads;
	total_info->n_pending_flush_lru += pool_info->n_pending_flush_lru;
	total_info->n_pending_flush_list += pool_info->n_pending_flush_list;
	total_info->n_pending_flush_single_page +=
		 pool_info->n_pending_flush_single_page;
	total_info->n_pages_made_young += pool_info->n_pages_made_young;
	total_info->n_pages_not_made_young += pool_info->n_pages_not_made_young;
	total_info->n_pages_read += pool_info->n_pages_read;
	total_info->n_pages_created += pool_info->n_pages_created;
	total_info->n_pages_written += pool_info->n_pages_written;
	total_info->n_page_gets += pool_info->n_page_gets;
	total_info->n_ra_pages_read_rnd += pool_info->n_ra_pages_read_rnd;
	total_info->n_ra_pages_read += pool_info->n_ra_pages_read;
	total_info->n_ra_pages_evicted += pool_info->n_ra_pages_evicted;
	total_info->page_made_young_rate += pool_info->page_made_young_rate;
	total_info->page_not_made_young_rate +=
		pool_info->page_not_made_young_rate;
	total_info->pages_read_rate += pool_info->pages_read_rate;
	total_info->pages_created_rate += pool_info->pages_created_rate;
	total_info->pages_written_rate += pool_info->pages_written_rate;
	total_info->n_page_get_delta += pool_info->n_page_get_delta;
	total_info->page_read_delta += pool_info->page_read_delta;
	total_info->young_making_delta += pool_info->young_making_delta;
	total_info->not_young_making_delta += pool_info->not_young_making_delta;
	total_info->pages_readahead_rnd_rate += pool_info->pages_readahead_rnd_rate;
	total_info->pages_readahead_rate += pool_info->pages_readahead_rate;
	total_info->pages_evicted_rate += pool_info->pages_evicted_rate;
	total_info->unzip_lru_len += pool_info->unzip_lru_len;
	total_info->io_sum += pool_info->io_sum;
	total_info->io_cur += pool_info->io_cur;
	total_info->unzip_sum += pool_info->unzip_sum;
	total_info->unzip_cur += pool_info->unzip_cur;
}
/*******************************************************************//**
Collect buffer pool stats information for a buffer pool. Also
record aggregated stats if there are more than one buffer pool
in the server */
UNIV_INTERN
void
buf_stats_get_pool_info(
/*====================*/
	buf_pool_t*		buf_pool,	/*!< in: buffer pool */
	ulint			pool_id,	/*!< in: buffer pool ID */
	buf_pool_info_t*	all_pool_info)	/*!< in/out: buffer pool info
						to fill */
{
	buf_pool_info_t*        pool_info;
	time_t			current_time;
	double			time_elapsed;

	/* Find appropriate pool_info to store stats for this buffer pool */
	pool_info = &all_pool_info[pool_id];

	mutex_enter(&buf_pool->LRU_list_mutex);
	mutex_enter(&buf_pool->free_list_mutex);
	buf_pool_mutex_enter(buf_pool);
	buf_flush_list_mutex_enter(buf_pool);

	pool_info->pool_unique_id = pool_id;

	pool_info->pool_size = buf_pool->curr_size;

	pool_info->pool_size_bytes = buf_pool->curr_pool_size;

	pool_info->lru_len = UT_LIST_GET_LEN(buf_pool->LRU);

	pool_info->old_lru_len = buf_pool->LRU_old_len;

	pool_info->free_list_len = UT_LIST_GET_LEN(buf_pool->free);

	pool_info->flush_list_len = UT_LIST_GET_LEN(buf_pool->flush_list);

	pool_info->n_pend_unzip = UT_LIST_GET_LEN(buf_pool->unzip_LRU);

	pool_info->n_pend_reads = buf_pool->n_pend_reads;

	pool_info->n_pending_flush_lru =
		 (buf_pool->n_flush[BUF_FLUSH_LRU]
		  + buf_pool->init_flush[BUF_FLUSH_LRU]);

	pool_info->n_pending_flush_list =
		 (buf_pool->n_flush[BUF_FLUSH_LIST]
		  + buf_pool->init_flush[BUF_FLUSH_LIST]);

	pool_info->n_pending_flush_single_page =
		 buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE];

	buf_flush_list_mutex_exit(buf_pool);

	current_time = time(NULL);
	time_elapsed = 0.001 + difftime(current_time,
					buf_pool->last_printout_time);

	pool_info->n_pages_made_young = buf_pool->stat.n_pages_made_young;

	pool_info->n_pages_not_made_young =
		buf_pool->stat.n_pages_not_made_young;

	pool_info->n_pages_read = buf_pool->stat.n_pages_read;

	pool_info->n_pages_created = buf_pool->stat.n_pages_created;

	pool_info->n_pages_written = buf_pool->stat.n_pages_written;

	pool_info->n_page_gets = buf_pool->stat.n_page_gets;

	pool_info->n_ra_pages_read_rnd = buf_pool->stat.n_ra_pages_read_rnd;
	pool_info->n_ra_pages_read = buf_pool->stat.n_ra_pages_read;

	pool_info->n_ra_pages_evicted = buf_pool->stat.n_ra_pages_evicted;

	pool_info->page_made_young_rate =
		 (buf_pool->stat.n_pages_made_young
		  - buf_pool->old_stat.n_pages_made_young) / time_elapsed;

	pool_info->page_not_made_young_rate =
		 (buf_pool->stat.n_pages_not_made_young
		  - buf_pool->old_stat.n_pages_not_made_young) / time_elapsed;

	pool_info->pages_read_rate =
		(buf_pool->stat.n_pages_read
		  - buf_pool->old_stat.n_pages_read) / time_elapsed;

	pool_info->pages_created_rate =
		(buf_pool->stat.n_pages_created
		 - buf_pool->old_stat.n_pages_created) / time_elapsed;

	pool_info->pages_written_rate =
		(buf_pool->stat.n_pages_written
		 - buf_pool->old_stat.n_pages_written) / time_elapsed;

	pool_info->n_page_get_delta = buf_pool->stat.n_page_gets
				      - buf_pool->old_stat.n_page_gets;

	if (pool_info->n_page_get_delta) {
		pool_info->page_read_delta = buf_pool->stat.n_pages_read
					     - buf_pool->old_stat.n_pages_read;

		pool_info->young_making_delta =
			buf_pool->stat.n_pages_made_young
			- buf_pool->old_stat.n_pages_made_young;

		pool_info->not_young_making_delta =
			buf_pool->stat.n_pages_not_made_young
			- buf_pool->old_stat.n_pages_not_made_young;
	}
	pool_info->pages_readahead_rnd_rate =
		 (buf_pool->stat.n_ra_pages_read_rnd
		  - buf_pool->old_stat.n_ra_pages_read_rnd) / time_elapsed;


	pool_info->pages_readahead_rate =
		 (buf_pool->stat.n_ra_pages_read
		  - buf_pool->old_stat.n_ra_pages_read) / time_elapsed;

	pool_info->pages_evicted_rate =
		(buf_pool->stat.n_ra_pages_evicted
		 - buf_pool->old_stat.n_ra_pages_evicted) / time_elapsed;

	pool_info->unzip_lru_len = UT_LIST_GET_LEN(buf_pool->unzip_LRU);

	pool_info->io_sum = buf_LRU_stat_sum.io;

	pool_info->io_cur = buf_LRU_stat_cur.io;

	pool_info->unzip_sum = buf_LRU_stat_sum.unzip;

	pool_info->unzip_cur = buf_LRU_stat_cur.unzip;

	buf_refresh_io_stats(buf_pool);
	mutex_exit(&buf_pool->LRU_list_mutex);
	mutex_exit(&buf_pool->free_list_mutex);
	buf_pool_mutex_exit(buf_pool);
}

/*********************************************************************//**
Prints info of the buffer i/o. */
UNIV_INTERN
void
buf_print_io_instance(
/*==================*/
	buf_pool_info_t*pool_info,	/*!< in: buffer pool info */
	FILE*		file)		/*!< in/out: buffer where to print */
{
	ut_ad(pool_info);

	fprintf(file,
		"Buffer pool size        %lu\n"
		"Buffer pool size, bytes %lu\n"
		"Free buffers            %lu\n"
		"Database pages          %lu\n"
		"Old database pages      %lu\n"
		"Modified db pages       %lu\n"
		"Pending reads %lu\n"
		"Pending writes: LRU %lu, flush list %lu, single page %lu\n",
		pool_info->pool_size,
		pool_info->pool_size_bytes,
		pool_info->free_list_len,
		pool_info->lru_len,
		pool_info->old_lru_len,
		pool_info->flush_list_len,
		pool_info->n_pend_reads,
		pool_info->n_pending_flush_lru,
		pool_info->n_pending_flush_list,
		pool_info->n_pending_flush_single_page);

	fprintf(file,
		"Pages made young %lu, not young %lu\n"
		"%.2f youngs/s, %.2f non-youngs/s\n"
		"Pages read %lu, created %lu, written %lu\n"
		"%.2f reads/s, %.2f creates/s, %.2f writes/s\n",
		pool_info->n_pages_made_young,
		pool_info->n_pages_not_made_young,
		pool_info->page_made_young_rate,
		pool_info->page_not_made_young_rate,
		pool_info->n_pages_read,
		pool_info->n_pages_created,
		pool_info->n_pages_written,
		pool_info->pages_read_rate,
		pool_info->pages_created_rate,
		pool_info->pages_written_rate);

	if (pool_info->n_page_get_delta) {
		fprintf(file,
			"Buffer pool hit rate %lu / 1000,"
			" young-making rate %lu / 1000 not %lu / 1000\n",
			(ulong) (1000 - (1000 * pool_info->page_read_delta
					 / pool_info->n_page_get_delta)),
			(ulong) (1000 * pool_info->young_making_delta
				 / pool_info->n_page_get_delta),
			(ulong) (1000 * pool_info->not_young_making_delta
				 / pool_info->n_page_get_delta));
	} else {
		fputs("No buffer pool page gets since the last printout\n",
		      file);
	}

	/* Statistics about read ahead algorithm */
	fprintf(file, "Pages read ahead %.2f/s,"
		" evicted without access %.2f/s,"
		" Random read ahead %.2f/s\n",

		pool_info->pages_readahead_rate,
		pool_info->pages_evicted_rate,
		pool_info->pages_readahead_rnd_rate);

	/* Print some values to help us with visualizing what is
	happening with LRU eviction. */
	fprintf(file,
		"LRU len: %lu, unzip_LRU len: %lu\n"
		"I/O sum[%lu]:cur[%lu], unzip sum[%lu]:cur[%lu]\n",
		pool_info->lru_len, pool_info->unzip_lru_len,
		pool_info->io_sum, pool_info->io_cur,
		pool_info->unzip_sum, pool_info->unzip_cur);
}

/*********************************************************************//**
Prints info of the buffer i/o. */
UNIV_INTERN
void
buf_print_io(
/*=========*/
	FILE*	file)	/*!< in/out: buffer where to print */
{
	ulint			i;
	buf_pool_info_t*	pool_info;
	buf_pool_info_t*	pool_info_total;

	/* If srv_buf_pool_instances is greater than 1, allocate
	one extra buf_pool_info_t, the last one stores
	aggregated/total values from all pools */
	if (srv_buf_pool_instances > 1) {
		pool_info = (buf_pool_info_t*) mem_zalloc((
			srv_buf_pool_instances + 1) * sizeof *pool_info);

		pool_info_total = &pool_info[srv_buf_pool_instances];
	} else {
		ut_a(srv_buf_pool_instances == 1);
		pool_info_total = pool_info = (buf_pool_info_t*) mem_zalloc(
			sizeof *pool_info)
	}

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		/* Fetch individual buffer pool info and calculate
		aggregated stats along the way */
		buf_stats_get_pool_info(buf_pool, i, pool_info);

		/* If we have more than one buffer pool, store
		the aggregated stats  */
		if (srv_buf_pool_instances > 1) {
			buf_stats_aggregate_pool_info(pool_info_total,
						      &pool_info[i]);
		}
	}

	/* Print the aggreate buffer pool info */
	buf_print_io_instance(pool_info_total, file);

	/* If there are more than one buffer pool, print each individual pool
	info */
	if (srv_buf_pool_instances > 1) {
		fputs("----------------------\n"
		"INDIVIDUAL BUFFER POOL INFO\n"
		"----------------------\n", file);

		for (i = 0; i < srv_buf_pool_instances; i++) {
			fprintf(file, "---BUFFER POOL %lu\n", i);
			buf_print_io_instance(&pool_info[i], file);
		}
	}

	mem_free(pool_info);
}

/**********************************************************************//**
Refreshes the statistics used to print per-second averages. */
UNIV_INTERN
void
buf_refresh_io_stats(
/*=================*/
	buf_pool_t*	buf_pool)	/*!< in: buffer pool instance */
{
	buf_pool->last_printout_time = ut_time();
	buf_pool->old_stat = buf_pool->stat;
}

/**********************************************************************//**
Refreshes the statistics used to print per-second averages. */
UNIV_INTERN
void
buf_refresh_io_stats_all(void)
/*==========================*/
{
	ulint		i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_refresh_io_stats(buf_pool);
	}
}

/**********************************************************************//**
Check if all pages in all buffer pools are in a replacable state.
@return FALSE if not */
UNIV_INTERN
ibool
buf_all_freed(void)
/*===============*/
{
	ulint	i;

	for (i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		if (!buf_all_freed_instance(buf_pool)) {
			return(FALSE);
		}
 	}

	return(TRUE);
}

/*********************************************************************//**
Checks that there currently are no pending i/o-operations for the buffer
pool.
@return	number of pending i/o */
UNIV_INTERN
ulint
buf_pool_check_num_pending_io(void)
/*===============================*/
{
	ulint		i;
	ulint		pending_io = 0;

	buf_pool_mutex_enter_all();

	for (i = 0; i < srv_buf_pool_instances; i++) {
		const buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		pending_io += buf_pool->n_pend_reads
			      + buf_pool->n_flush[BUF_FLUSH_LRU]
			      + buf_pool->n_flush[BUF_FLUSH_LIST]
			      + buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE];

	}

	buf_pool_mutex_exit_all();

	return(pending_io);
}

#if 0
Code currently not used
/*********************************************************************//**
Gets the current length of the free list of buffer blocks.
@return	length of the free list */
UNIV_INTERN
ulint
buf_get_free_list_len(void)
/*=======================*/
{
	ulint	len;

	//buf_pool_mutex_enter(buf_pool);
	mutex_enter(&buf_pool->free_list_mutex);

	len = UT_LIST_GET_LEN(buf_pool->free);

	//buf_pool_mutex_exit(buf_pool);
	mutex_exit(&buf_pool->free_list_mutex);

	return(len);
}
#endif

#else /* !UNIV_HOTBACKUP */
/********************************************************************//**
Inits a page to the buffer buf_pool, for use in ibbackup --restore. */
UNIV_INTERN
void
buf_page_init_for_backup_restore(
/*=============================*/
	ulint		space,	/*!< in: space id */
	ulint		offset,	/*!< in: offset of the page within space
				in units of a page */
	ulint		zip_size,/*!< in: compressed page size in bytes
				or 0 for uncompressed pages */
	buf_block_t*	block)	/*!< in: block to init */
{
	block->page.state	= BUF_BLOCK_FILE_PAGE;
	block->page.space	= space;
	block->page.offset	= offset;

	page_zip_des_init(&block->page.zip);

	/* We assume that block->page.data has been allocated
	with zip_size == UNIV_PAGE_SIZE. */
	ut_ad(zip_size <= UNIV_PAGE_SIZE);
	ut_ad(ut_is_2pow(zip_size));
	page_zip_set_size(&block->page.zip, zip_size);
	if (zip_size) {
		block->page.zip.data = block->frame + UNIV_PAGE_SIZE;
	}
}
#endif /* !UNIV_HOTBACKUP */
