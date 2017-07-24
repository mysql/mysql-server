/*****************************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.
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
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file buf/buf0buf.cc
The database buffer buf_pool

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#include "ha_prototypes.h"

#include "page0size.h"
#include "buf0buf.h"
#ifdef UNIV_NONINL
#include "buf0buf.ic"
#endif
#ifdef UNIV_INNOCHECKSUM
#include "string.h"
#include "mach0data.h"
#endif /* UNIV_INNOCHECKSUM */
#ifndef UNIV_INNOCHECKSUM
#include "mem0mem.h"
#include "btr0btr.h"
#include "fil0fil.h"
#include "fsp0sysspace.h"
#ifndef UNIV_HOTBACKUP
#include "buf0buddy.h"
#include "lock0lock.h"
#include "sync0rw.h"
#include "btr0sea.h"
#include "ibuf0ibuf.h"
#include "trx0undo.h"
#include "trx0purge.h"
#include "log0log.h"
#include "dict0stats_bg.h"
#endif /* !UNIV_HOTBACKUP */
#include "srv0srv.h"
#include "srv0start.h"
#include "dict0dict.h"
#include "log0recv.h"
#include "srv0mon.h"
#include "fsp0sysspace.h"
#endif /* !UNIV_INNOCHECKSUM */
#include "page0zip.h"
#include "buf0checksum.h"
#include "sync0sync.h"
#include "buf0dump.h"
#include "ut0new.h"
#include <new>
#include <map>
#include <sstream>

my_bool  srv_numa_interleave = FALSE;

#ifdef HAVE_LIBNUMA
#include <numa.h>
#include <numaif.h>

struct set_numa_interleave_t
{
	set_numa_interleave_t()
	{
		if (srv_numa_interleave) {

			ib::info() << "Setting NUMA memory policy to"
				" MPOL_INTERLEAVE";
			if (set_mempolicy(MPOL_INTERLEAVE,
					  numa_all_nodes_ptr->maskp,
					  numa_all_nodes_ptr->size) != 0) {

				ib::warn() << "Failed to set NUMA memory"
					" policy to MPOL_INTERLEAVE: "
					<< strerror(errno);
			}
		}
	}

	~set_numa_interleave_t()
	{
		if (srv_numa_interleave) {

			ib::info() << "Setting NUMA memory policy to"
				" MPOL_DEFAULT";
			if (set_mempolicy(MPOL_DEFAULT, NULL, 0) != 0) {
				ib::warn() << "Failed to set NUMA memory"
					" policy to MPOL_DEFAULT: "
					<< strerror(errno);
			}
		}
	}
};

#define NUMA_MEMPOLICY_INTERLEAVE_IN_SCOPE set_numa_interleave_t scoped_numa
#else
#define NUMA_MEMPOLICY_INTERLEAVE_IN_SCOPE
#endif /* HAVE_LIBNUMA */

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
the buddy allocator (buf0buddy.cc) to keep track of currently unused
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

#if (!(defined(UNIV_HOTBACKUP) || defined(UNIV_INNOCHECKSUM)))
/** Value in microseconds */
static const int WAIT_FOR_READ	= 100;
static const int WAIT_FOR_WRITE = 100;
/** Number of attempts made to read in a page in the buffer pool */
static const ulint	BUF_PAGE_READ_MAX_RETRIES = 100;
/** Number of pages to read ahead */
static const ulint	BUF_READ_AHEAD_PAGES = 64;
/** The maximum portion of the buffer pool that can be used for the
read-ahead buffer.  (Divide buf_pool size by this amount) */
static const ulint	BUF_READ_AHEAD_PORTION = 32;

/** The buffer pools of the database */
buf_pool_t*	buf_pool_ptr;

/** true when resizing buffer pool is in the critical path. */
volatile bool	buf_pool_resizing;

/** true when withdrawing buffer pool pages might cause page relocation */
volatile bool	buf_pool_withdrawing;

/** the clock is incremented every time a pointer to a page may become obsolete;
if the withdrwa clock has not changed, the pointer is still valid in buffer
pool. if changed, the pointer might not be in buffer pool any more. */
volatile ulint	buf_withdraw_clock;

/** Map of buffer pool chunks by its first frame address
This is newly made by initialization of buffer pool and buf_resize_thread.
Currently, no need mutex protection for update. */
typedef std::map<
	const byte*,
	buf_chunk_t*,
	std::less<const byte*>,
	ut_allocator<std::pair<const byte* const, buf_chunk_t*> > >
	buf_pool_chunk_map_t;

static buf_pool_chunk_map_t*			buf_chunk_map_reg;

/** Chunk map to be used to lookup.
The map pointed by this should not be updated */
static buf_pool_chunk_map_t*	buf_chunk_map_ref = NULL;

#ifdef UNIV_DEBUG
/** Disable resizing buffer pool to make assertion code not expensive. */
my_bool			buf_disable_resize_buffer_pool_debug = TRUE;
#endif /* UNIV_DEBUG */

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** This is used to insert validation operations in execution
in the debug version */
static ulint	buf_dbg_counter	= 0;
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

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

/** Macro to determine whether the read of write counter is used depending
on the io_type */
#define MONITOR_RW_COUNTER(io_type, counter)		\
	((io_type == BUF_IO_READ)			\
	 ? (counter##_READ)				\
	 : (counter##_WRITTEN))

/** Registers a chunk to buf_pool_chunk_map
@param[in]	chunk	chunk of buffers */
static
void
buf_pool_register_chunk(
	buf_chunk_t*	chunk)
{
	buf_chunk_map_reg->insert(buf_pool_chunk_map_t::value_type(
		chunk->blocks->frame, chunk));
}

/********************************************************************//**
Gets the smallest oldest_modification lsn for any page in the pool. Returns
zero if all modified pages have been flushed to disk.
@return oldest modification in pool, zero if none */
lsn_t
buf_pool_get_oldest_modification(void)
/*==================================*/
{
	lsn_t		lsn = 0;
	lsn_t		oldest_lsn = 0;

	/* When we traverse all the flush lists we don't want another
	thread to add a dirty page to any flush list. */
	log_flush_order_mutex_enter();

	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_flush_list_mutex_enter(buf_pool);

		buf_page_t*	bpage;

		/* We don't let log-checkpoint halt because pages from system
		temporary are not yet flushed to the disk. Anyway, object
		residing in system temporary doesn't generate REDO logging. */
		for (bpage = UT_LIST_GET_LAST(buf_pool->flush_list);
		     bpage != NULL
			&& fsp_is_system_temporary(bpage->id.space());
		     bpage = UT_LIST_GET_PREV(list, bpage)) {
			/* Do nothing. */
		}

		if (bpage != NULL) {
			ut_ad(bpage->in_flush_list);
			lsn = bpage->oldest_modification;
		}

		buf_flush_list_mutex_exit(buf_pool);

		if (!oldest_lsn || oldest_lsn > lsn) {
			oldest_lsn = lsn;
		}
	}

	log_flush_order_mutex_exit();

	/* The returned answer may be out of date: the flush_list can
	change after the mutex has been released. */

	return(oldest_lsn);
}

/********************************************************************//**
Get total buffer pool statistics. */
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
Get total list size in bytes from all buffer pools. */
void
buf_get_total_list_size_in_bytes(
/*=============================*/
	buf_pools_list_size_t*	buf_pools_list_size)	/*!< out: list sizes
							in all buffer pools */
{
	ut_ad(buf_pools_list_size);
	memset(buf_pools_list_size, 0, sizeof(*buf_pools_list_size));

	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);
		/* We don't need mutex protection since this is
		for statistics purpose */
		buf_pools_list_size->LRU_bytes += buf_pool->stat.LRU_bytes;
		buf_pools_list_size->unzip_LRU_bytes +=
			UT_LIST_GET_LEN(buf_pool->unzip_LRU) * UNIV_PAGE_SIZE;
		buf_pools_list_size->flush_list_bytes +=
			buf_pool->stat.flush_list_bytes;
	}
}

/********************************************************************//**
Get total buffer pool statistics. */
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
#endif /* !UNIV_HOTBACKUP && !UNIV_INNOCHECKSUM */

/** Checks if a page contains only zeroes.
@param[in]	read_buf	database page
@param[in]	page_size	page size
@return true if page is filled with zeroes */
bool
buf_page_is_zeroes(
	const byte*		read_buf,
	const page_size_t&	page_size)
{
	for (ulint i = 0; i < page_size.logical(); i++) {
		if (read_buf[i] != 0) {
			return(false);
		}
	}
	return(true);
}

/** Checks if the page is in crc32 checksum format.
@param[in]	read_buf		database page
@param[in]	checksum_field1		new checksum field
@param[in]	checksum_field2		old checksum field
@param[in]	page_no			page number of given read_buf
@param[in]	is_log_enabled		true if log option is enabled
@param[in]	log_file		file pointer to log_file
@param[in]	curr_algo		current checksum algorithm
@param[in]	use_legacy_big_endian   use legacy big endian algorithm
@return true if the page is in crc32 checksum format. */
UNIV_INLINE
bool
buf_page_is_checksum_valid_crc32(
	const byte*			read_buf,
	ulint				checksum_field1,
	ulint				checksum_field2,
#ifdef UNIV_INNOCHECKSUM
	uintmax_t			page_no,
	bool				is_log_enabled,
	FILE*				log_file,
	const srv_checksum_algorithm_t	curr_algo,
#endif /* UNIV_INNOCHECKSUM */
	bool				use_legacy_big_endian)
{
	const uint32_t	crc32 = buf_calc_page_crc32(read_buf,
						    use_legacy_big_endian);

#ifdef UNIV_INNOCHECKSUM
	if (is_log_enabled
	    && curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32) {
		fprintf(log_file, "page::%" PRIuMAX ";"
			" crc32 calculated = %u;"
			" recorded checksum field1 = " ULINTPF
			" recorded checksum field2 = " ULINTPF
			"\n", page_no, crc32,
			checksum_field1, checksum_field2);
	}
#endif /* UNIV_INNOCHECKSUM */

	if (checksum_field1 != checksum_field2) {
		return(false);
	}

	if (checksum_field1 == crc32) {
		return(true);
	}

	return(false);
}

/** Checks if the page is in innodb checksum format.
@param[in]	read_buf	database page
@param[in]	checksum_field1	new checksum field
@param[in]	checksum_field2	old checksum field
@param[in]	page_no		page number of given read_buf
@param[in]	is_log_enabled	true if log option is enabled
@param[in]	log_file	file pointer to log_file
@param[in]	curr_algo	current checksum algorithm
@return true if the page is in innodb checksum format. */
UNIV_INLINE
bool
buf_page_is_checksum_valid_innodb(
	const byte*			read_buf,
	ulint				checksum_field1,
	ulint				checksum_field2
#ifdef UNIV_INNOCHECKSUM
	,uintmax_t			page_no,
	bool				is_log_enabled,
	FILE*				log_file,
	const srv_checksum_algorithm_t	curr_algo
#endif /* UNIV_INNOCHECKSUM */
	)
{
	/* There are 2 valid formulas for
	checksum_field2 (old checksum field) which algo=innodb could have
	written to the page:

	1. Very old versions of InnoDB only stored 8 byte lsn to the
	start and the end of the page.

	2. Newer InnoDB versions store the old formula checksum
	(buf_calc_page_old_checksum()). */

	ulint	old_checksum = buf_calc_page_old_checksum(read_buf);
	ulint	new_checksum = buf_calc_page_new_checksum(read_buf);

#ifdef UNIV_INNOCHECKSUM
	if (is_log_enabled
	    && curr_algo == SRV_CHECKSUM_ALGORITHM_INNODB) {
		fprintf(log_file, "page::%" PRIuMAX ";"
			" old style: calculated = " ULINTPF
			"; recorded = " ULINTPF "\n",
			page_no, old_checksum,
			checksum_field2);
		fprintf(log_file, "page::%" PRIuMAX ";"
			" new style: calculated = " ULINTPF
			"; crc32 = %u; recorded = " ULINTPF "\n",
			page_no, new_checksum,
			buf_calc_page_crc32(read_buf), checksum_field1);
	}

	if (is_log_enabled
	    && curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB) {
		fprintf(log_file, "page::%" PRIuMAX ";"
			" old style: calculated = " ULINTPF
			"; recorded checksum = " ULINTPF "\n",
			page_no, old_checksum,
			checksum_field2);
		fprintf(log_file, "page::%" PRIuMAX ";"
			" new style: calculated = " ULINTPF
			"; recorded checksum  = " ULINTPF "\n",
			page_no, new_checksum,
			checksum_field1);
	}
#endif /* UNIV_INNOCHECKSUM */

	if (checksum_field2 != mach_read_from_4(read_buf + FIL_PAGE_LSN)
	    && checksum_field2 != old_checksum) {
		return(false);
	}

	/* old field is fine, check the new field */

	/* InnoDB versions < 4.0.14 and < 4.1.1 stored the space id
	(always equal to 0), to FIL_PAGE_SPACE_OR_CHKSUM */

	if (checksum_field1 != 0 && checksum_field1 != new_checksum) {
		return(false);
	}

	return(true);
}

/** Checks if the page is in none checksum format.
@param[in]	read_buf	database page
@param[in]	checksum_field1	new checksum field
@param[in]	checksum_field2	old checksum field
@param[in]	page_no		page number of given read_buf
@param[in]	is_log_enabled	true if log option is enabled
@param[in]	log_file	file pointer to log_file
@param[in]	curr_algo	current checksum algorithm
@return true if the page is in none checksum format. */
UNIV_INLINE
bool
buf_page_is_checksum_valid_none(
	const byte*			read_buf,
	ulint				checksum_field1,
	ulint				checksum_field2
#ifdef	UNIV_INNOCHECKSUM
	,uintmax_t			page_no,
	bool				is_log_enabled,
	FILE*				log_file,
	const srv_checksum_algorithm_t	curr_algo
#endif	/* UNIV_INNOCHECKSUM */
	)
{

#ifdef UNIV_INNOCHECKSUM
	if (is_log_enabled
	    && curr_algo == SRV_CHECKSUM_ALGORITHM_STRICT_NONE) {
		fprintf(log_file,
			"page::%" PRIuMAX
			"; none checksum: calculated = %lu;"
			" recorded checksum_field1 = " ULINTPF
			" recorded checksum_field2 = " ULINTPF "\n",
			page_no, BUF_NO_CHECKSUM_MAGIC,
			checksum_field1, checksum_field2);
	}
#endif /* UNIV_INNOCHECKSUM */

	return(checksum_field1 == checksum_field2
	       && checksum_field1 == BUF_NO_CHECKSUM_MAGIC);
}

/** Checks if a page is corrupt.
@param[in]	check_lsn	true if we need to check and complain about
the LSN
@param[in]	read_buf	database page
@param[in]	page_size	page size
@param[in]	skip_checksum	if true, skip checksum
@param[in]	page_no		page number of given read_buf
@param[in]	strict_check	true if strict-check option is enabled
@param[in]	is_log_enabled	true if log option is enabled
@param[in]	log_file	file pointer to log_file
@return TRUE if corrupted */
ibool
buf_page_is_corrupted(
	bool			check_lsn,
	const byte*		read_buf,
	const page_size_t&	page_size,
	bool			skip_checksum
#ifdef UNIV_INNOCHECKSUM
	,uintmax_t		page_no,
	bool			strict_check,
	bool			is_log_enabled,
	FILE*			log_file
#endif /* UNIV_INNOCHECKSUM */
)
{
	ulint		checksum_field1;
	ulint		checksum_field2;

	if (!page_size.is_compressed()
	    && memcmp(read_buf + FIL_PAGE_LSN + 4,
		      read_buf + page_size.logical()
		      - FIL_PAGE_END_LSN_OLD_CHKSUM + 4, 4)) {

		/* Stored log sequence numbers at the start and the end
		of page do not match */

		return(TRUE);
	}

#if !defined(UNIV_HOTBACKUP) && !defined(UNIV_INNOCHECKSUM)
	if (check_lsn && recv_lsn_checks_on) {
		lsn_t		current_lsn;
		const lsn_t	page_lsn
			= mach_read_from_8(read_buf + FIL_PAGE_LSN);

		/* Since we are going to reset the page LSN during the import
		phase it makes no sense to spam the log with error messages. */

		if (log_peek_lsn(&current_lsn) && current_lsn < page_lsn) {

			const ulint	space_id = mach_read_from_4(
				read_buf + FIL_PAGE_SPACE_ID);
			const ulint	page_no = mach_read_from_4(
				read_buf + FIL_PAGE_OFFSET);

			ib::error() << "Page " << page_id_t(space_id, page_no)
				<< " log sequence number " << page_lsn
				<< " is in the future! Current system"
				<< " log sequence number "
				<< current_lsn << ".";

			ib::error() << "Your database may be corrupt or"
				" you may have copied the InnoDB"
				" tablespace but not the InnoDB"
				" log files. "
				<< FORCE_RECOVERY_MSG;

		}
	}
#endif /* !UNIV_HOTBACKUP && !UNIV_INNOCHECKSUM */

	/* Check whether the checksum fields have correct values */

	if (srv_checksum_algorithm == SRV_CHECKSUM_ALGORITHM_NONE
	    || skip_checksum) {
		return(FALSE);
	}

	if (page_size.is_compressed()) {
#ifdef UNIV_INNOCHECKSUM
		return(!page_zip_verify_checksum(read_buf,
						 page_size.physical(),
						 page_no, strict_check,
						 is_log_enabled, log_file));
#else
		return(!page_zip_verify_checksum(read_buf,
						 page_size.physical()));
#endif /* UNIV_INNOCHECKSUM */
	}

	checksum_field1 = mach_read_from_4(
		read_buf + FIL_PAGE_SPACE_OR_CHKSUM);

	checksum_field2 = mach_read_from_4(
		read_buf + page_size.logical() - FIL_PAGE_END_LSN_OLD_CHKSUM);

#if FIL_PAGE_LSN % 8
#error "FIL_PAGE_LSN must be 64 bit aligned"
#endif

	/* declare empty pages non-corrupted */
	if (checksum_field1 == 0
	    && checksum_field2 == 0
	    && *reinterpret_cast<const ib_uint64_t*>(
		    read_buf + FIL_PAGE_LSN) == 0) {

		/* make sure that the page is really empty */

		ulint	i;

		for (i = 0; i < page_size.logical(); ++i) {

			/* The FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID has been
			repurposed for page compression. It can be
			set for uncompressed empty pages. */

			if ((i < FIL_PAGE_FILE_FLUSH_LSN
			     || i >= FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID)
			    && read_buf[i] != 0) {

				break;
			}
		}
#ifdef UNIV_INNOCHECKSUM
		if (i >= page_size.logical()) {
			if (is_log_enabled) {
				fprintf(log_file, "Page::%" PRIuMAX
					" is empty and uncorrupted\n",
					page_no);
			}
			return(FALSE);
		}
#else
		return(i < page_size.logical());
#endif /* UNIV_INNOCHECKSUM */
	}

#ifndef UNIV_INNOCHECKSUM
	const page_id_t	page_id(mach_read_from_4(
					read_buf + FIL_PAGE_SPACE_ID),
				mach_read_from_4(
					read_buf + FIL_PAGE_OFFSET));
#endif /* UNIV_INNOCHECKSUM */

	DBUG_EXECUTE_IF("buf_page_import_corrupt_failure", return(TRUE); );
	const srv_checksum_algorithm_t	curr_algo =
		static_cast<srv_checksum_algorithm_t>(srv_checksum_algorithm);

	bool	legacy_checksum_checked = false;

	switch (curr_algo) {
	case SRV_CHECKSUM_ALGORITHM_CRC32:
	case SRV_CHECKSUM_ALGORITHM_STRICT_CRC32:

		if (buf_page_is_checksum_valid_crc32(read_buf,
			checksum_field1, checksum_field2,
#ifdef UNIV_INNOCHECKSUM
			page_no, is_log_enabled, log_file, curr_algo,
#endif /* UNIV_INNOCHECKSUM */
			false)) {
			return(FALSE);
		}

		if (buf_page_is_checksum_valid_none(read_buf,
			checksum_field1, checksum_field2
#ifdef UNIV_INNOCHECKSUM
			, page_no, is_log_enabled, log_file, curr_algo)) {
#else /* UNIV_INNOCHECKSUM */
		)) {
			if (curr_algo
			    == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32) {
				page_warn_strict_checksum(
					curr_algo,
					SRV_CHECKSUM_ALGORITHM_NONE,
					page_id);
			}
#endif /* UNIV_INNOCHECKSUM */

#ifdef UNIV_INNOCHECKSUM
			if (is_log_enabled) {

				fprintf(log_file, "page::%" PRIuMAX ";"
					" old style: calculated = " ULINTPF ";"
					" recorded = " ULINTPF "\n", page_no,
					buf_calc_page_old_checksum(read_buf),
					checksum_field2);
				fprintf(log_file, "page::%" PRIuMAX ";"
					" new style: calculated = " ULINTPF ";"
					" crc32 = %u; recorded = " ULINTPF "\n",
					page_no,
					buf_calc_page_new_checksum(read_buf),
					buf_calc_page_crc32(read_buf),
					checksum_field1);
			}
#endif /* UNIV_INNOCHECKSUM */
			return(FALSE);
		}

		/* We need to check whether the stored checksum matches legacy
		big endian checksum or Innodb checksum. We optimize the order
		based on earlier results. if earlier we have found pages
		matching legacy big endian checksum, we try to match it first.
		Otherwise we check innodb checksum first. */
		if (legacy_big_endian_checksum) {
			if (buf_page_is_checksum_valid_crc32(read_buf,
				checksum_field1, checksum_field2,
#ifdef UNIV_INNOCHECKSUM
				page_no, is_log_enabled, log_file, curr_algo,
#endif /* UNIV_INNOCHECKSUM */
				true)) {

				return(FALSE);
			}
			legacy_checksum_checked = true;
		}

		if (buf_page_is_checksum_valid_innodb(read_buf,
			checksum_field1, checksum_field2
#ifdef UNIV_INNOCHECKSUM
			, page_no, is_log_enabled, log_file, curr_algo)) {
#else /* UNIV_INNOCHECKSUM */
		)) {
			if (curr_algo
			    == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32) {
				page_warn_strict_checksum(
					curr_algo,
					SRV_CHECKSUM_ALGORITHM_INNODB,
					page_id);
			}
#endif /* UNIV_INNOCHECKSUM */
			return(FALSE);
		}

		/* If legacy checksum is not checked, do it now. */
		if (!legacy_checksum_checked && buf_page_is_checksum_valid_crc32(
			read_buf, checksum_field1, checksum_field2,
#ifdef UNIV_INNOCHECKSUM
			page_no, is_log_enabled, log_file, curr_algo,
#endif /* UNIV_INNOCHECKSUM */
			true)) {

				legacy_big_endian_checksum = true;
				return(FALSE);
		}

#ifdef UNIV_INNOCHECKSUM
		if (is_log_enabled) {
			fprintf(log_file, "Fail; page %" PRIuMAX
				" invalid (fails crc32 checksum)\n",
				page_no);
		}
#endif /* UNIV_INNOCHECKSUM */
		return(TRUE);

	case SRV_CHECKSUM_ALGORITHM_INNODB:
	case SRV_CHECKSUM_ALGORITHM_STRICT_INNODB:

		if (buf_page_is_checksum_valid_innodb(read_buf,
			checksum_field1, checksum_field2
#ifdef UNIV_INNOCHECKSUM
			, page_no, is_log_enabled, log_file, curr_algo
#endif /* UNIV_INNOCHECKSUM */
		)) {
			return(FALSE);
		}

		if (buf_page_is_checksum_valid_none(read_buf,
			checksum_field1, checksum_field2
#ifdef UNIV_INNOCHECKSUM
			, page_no, is_log_enabled, log_file, curr_algo)) {
#else	/* UNIV_INNOCHECKSUM */
		)) {
			if (curr_algo
			    == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB) {
				page_warn_strict_checksum(
					curr_algo,
					SRV_CHECKSUM_ALGORITHM_NONE,
					page_id);
			}
#endif /* UNIV_INNOCHECKSUM */

#ifdef UNIV_INNOCHECKSUM
			if (is_log_enabled) {
				fprintf(log_file, "page::%" PRIuMAX ";"
					" old style: calculated = " ULINTPF ";"
					" recorded = " ULINTPF "\n", page_no,
					buf_calc_page_old_checksum(read_buf),
					checksum_field2);
				fprintf(log_file, "page::%" PRIuMAX ";"
					" new style: calculated = " ULINTPF ";"
					" crc32 = %u; recorded = " ULINTPF "\n",
					page_no,
					buf_calc_page_new_checksum(read_buf),
					buf_calc_page_crc32(read_buf),
					checksum_field1);
			}
#endif /* UNIV_INNOCHECKSUM */
			return(FALSE);
		}

#ifdef UNIV_INNOCHECKSUM
		if (buf_page_is_checksum_valid_crc32(read_buf,
			checksum_field1, checksum_field2,
			page_no, is_log_enabled, log_file, curr_algo, false)
		    || buf_page_is_checksum_valid_crc32(read_buf,
			checksum_field1, checksum_field2,
			page_no, is_log_enabled, log_file, curr_algo, true)) {
#else /* UNIV_INNOCHECKSUM */
		if (buf_page_is_checksum_valid_crc32(read_buf,
			checksum_field1, checksum_field2, false)
		    || buf_page_is_checksum_valid_crc32(read_buf,
			checksum_field1, checksum_field2, true)) {

			if (curr_algo
			    == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB) {
				page_warn_strict_checksum(
					curr_algo,
					SRV_CHECKSUM_ALGORITHM_CRC32,
					page_id);
			}
#endif /* UNIV_INNOCHECKSUM */

			return(FALSE);
		}

#ifdef UNIV_INNOCHECKSUM
		if (is_log_enabled) {
			fprintf(log_file, "Fail; page %" PRIuMAX
				" invalid (fails innodb checksum)\n",
				page_no);
		}
#endif /* UNIV_INNOCHECKSUM */
		return(TRUE);

	case SRV_CHECKSUM_ALGORITHM_STRICT_NONE:

		if (buf_page_is_checksum_valid_none(read_buf,
			checksum_field1, checksum_field2
#ifdef UNIV_INNOCHECKSUM
			, page_no, is_log_enabled, log_file, curr_algo
#endif /* UNIV_INNOCHECKSUM */
		)) {
			return(false);
		}

#ifdef UNIV_INNOCHECKSUM
		if (buf_page_is_checksum_valid_crc32(read_buf,
			checksum_field1, checksum_field2,
			page_no, is_log_enabled, log_file, curr_algo, false)
		    || buf_page_is_checksum_valid_crc32(read_buf,
			checksum_field1, checksum_field2,
			page_no, is_log_enabled, log_file, curr_algo, true)) {
#else /* UNIV_INNOCHECKSUM */
		if (buf_page_is_checksum_valid_crc32(read_buf,
			checksum_field1, checksum_field2, false)
		    || buf_page_is_checksum_valid_crc32(read_buf,
			checksum_field1, checksum_field2, true)) {

			page_warn_strict_checksum(
				curr_algo,
				SRV_CHECKSUM_ALGORITHM_CRC32,
				page_id);
#endif /* UNIV_INNOCHECKSUM */
			return(FALSE);
		}

		if (buf_page_is_checksum_valid_innodb(read_buf,
			checksum_field1, checksum_field2
#ifdef UNIV_INNOCHECKSUM
			, page_no, is_log_enabled, log_file, curr_algo)) {
#else /* UNIV_INNOCHECKSUM */
		)) {
			page_warn_strict_checksum(
				curr_algo,
				SRV_CHECKSUM_ALGORITHM_INNODB,
				page_id);
#endif /* UNIV_INNOCHECKSUM */
			return(FALSE);
		}

#ifdef UNIV_INNOCHECKSUM
		if (is_log_enabled) {
			fprintf(log_file, "Fail; page %" PRIuMAX
				" invalid (fails none checksum)\n",
				page_no);
		}
#endif /* UNIV_INNOCHECKSUM */
		return(TRUE);

	case SRV_CHECKSUM_ALGORITHM_NONE:
		/* should have returned FALSE earlier */
		break;
	/* no default so the compiler will emit a warning if new enum
	is added and not handled here */
	}

	ut_error;
	return(FALSE);
}

#ifndef UNIV_INNOCHECKSUM

/** Prints a page to stderr.
@param[in]	read_buf	a database page
@param[in]	page_size	page size
@param[in]	flags		0 or BUF_PAGE_PRINT_NO_CRASH or
BUF_PAGE_PRINT_NO_FULL */
void
buf_page_print(
	const byte*		read_buf,
	const page_size_t&	page_size,
	ulint			flags)
{
#ifndef UNIV_HOTBACKUP
	dict_index_t*	index;
#endif /* !UNIV_HOTBACKUP */

	if (!(flags & BUF_PAGE_PRINT_NO_FULL)) {

		ib::info() << "Page dump in ascii and hex ("
			<< page_size.physical() << " bytes):";

		ut_print_buf(stderr, read_buf, page_size.physical());
		fputs("\nInnoDB: End of page dump\n", stderr);
	}

	if (page_size.is_compressed()) {
		/* Print compressed page. */
		ib::info() << "Compressed page type ("
			<< fil_page_get_type(read_buf)
			<< "); stored checksum in field1 "
			<< mach_read_from_4(
				read_buf + FIL_PAGE_SPACE_OR_CHKSUM)
			<< "; calculated checksums for field1: "
			<< buf_checksum_algorithm_name(
				SRV_CHECKSUM_ALGORITHM_CRC32)
			<< " "
			<< page_zip_calc_checksum(
				read_buf, page_size.physical(),
				SRV_CHECKSUM_ALGORITHM_CRC32)
			<< "/"
			<< page_zip_calc_checksum(
				read_buf, page_size.physical(),
				SRV_CHECKSUM_ALGORITHM_CRC32, true)
			<< ", "
			<< buf_checksum_algorithm_name(
				SRV_CHECKSUM_ALGORITHM_INNODB)
			<< " "
			<< page_zip_calc_checksum(
				read_buf, page_size.physical(),
				SRV_CHECKSUM_ALGORITHM_INNODB)
			<< ", "
			<< buf_checksum_algorithm_name(
				SRV_CHECKSUM_ALGORITHM_NONE)
			<< " "
			<< page_zip_calc_checksum(
				read_buf, page_size.physical(),
				SRV_CHECKSUM_ALGORITHM_NONE)
			<< "; page LSN "
			<< mach_read_from_8(read_buf + FIL_PAGE_LSN)
			<< "; page number (if stored to page"
			<< " already) "
			<< mach_read_from_4(read_buf + FIL_PAGE_OFFSET)
			<< "; space id (if stored to page already) "
			<< mach_read_from_4(
				read_buf + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);

	} else {
		const uint32_t	crc32 = buf_calc_page_crc32(read_buf);

		const uint32_t	crc32_legacy = buf_calc_page_crc32(read_buf,
								   true);

		ib::info() << "Uncompressed page, stored checksum in field1 "
			<< mach_read_from_4(
				read_buf + FIL_PAGE_SPACE_OR_CHKSUM)
			<< ", calculated checksums for field1: "
			<< buf_checksum_algorithm_name(
				SRV_CHECKSUM_ALGORITHM_CRC32) << " "
			<< crc32 << "/" << crc32_legacy
			<< ", "
			<< buf_checksum_algorithm_name(
				SRV_CHECKSUM_ALGORITHM_INNODB) << " "
			<< buf_calc_page_new_checksum(read_buf)
			<< ", "
			<< buf_checksum_algorithm_name(
				SRV_CHECKSUM_ALGORITHM_NONE) << " "
			<< BUF_NO_CHECKSUM_MAGIC
			<< ", stored checksum in field2 "
			<< mach_read_from_4(read_buf + page_size.logical()
					    - FIL_PAGE_END_LSN_OLD_CHKSUM)
			<< ", calculated checksums for field2: "
			<< buf_checksum_algorithm_name(
				SRV_CHECKSUM_ALGORITHM_CRC32) << " "
			<< crc32 << "/" << crc32_legacy
			<< ", "
			<< buf_checksum_algorithm_name(
				SRV_CHECKSUM_ALGORITHM_INNODB) << " "
			<< buf_calc_page_old_checksum(read_buf)
			<< ", "
			<< buf_checksum_algorithm_name(
				SRV_CHECKSUM_ALGORITHM_NONE) << " "
			<< BUF_NO_CHECKSUM_MAGIC
			<< ",  page LSN "
			<< mach_read_from_4(read_buf + FIL_PAGE_LSN)
			<< " "
			<< mach_read_from_4(read_buf + FIL_PAGE_LSN + 4)
			<< ", low 4 bytes of LSN at page end "
			<< mach_read_from_4(read_buf + page_size.logical()
					    - FIL_PAGE_END_LSN_OLD_CHKSUM + 4)
			<< ", page number (if stored to page already) "
			<< mach_read_from_4(read_buf + FIL_PAGE_OFFSET)
			<< ", space id (if created with >= MySQL-4.1.1"
			   " and stored already) "
			<< mach_read_from_4(
				read_buf + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
	}

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
	case FIL_PAGE_RTREE:
		index_id = btr_page_get_index_id(read_buf);
		fprintf(stderr,
			"InnoDB: Page may be an index page where"
			" index id is " IB_ID_FMT "\n",
			index_id);
#ifndef UNIV_HOTBACKUP
		index = dict_index_find_on_id_low(index_id);
		if (index) {
			ib::info()
				<< "Index " << index_id
				<< " is " << index->name
				<< " in table " << index->table->name;
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
extern mysql_pfs_key_t	buffer_block_mutex_key;

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
	buf_block_t*    block;
	ulint		num_to_register;

	block = chunk->blocks;

	num_to_register = ut_min(
		chunk->size, PFS_MAX_BUFFER_MUTEX_LOCK_REGISTER);

	for (ulint i = 0; i < num_to_register; i++) {
#  ifdef UNIV_PFS_MUTEX
		BPageMutex*	mutex;

		mutex = &block->mutex;
		mutex->pfs_add(buffer_block_mutex_key);
#  endif /* UNIV_PFS_MUTEX */

		rw_lock_t*	rwlock;

#  ifdef UNIV_PFS_RWLOCK
		rwlock = &block->lock;
		ut_a(!rwlock->pfs_psi);
		rwlock->pfs_psi = (PSI_server)
			? PSI_server->init_rwlock(
				buf_block_lock_key.m_value, rwlock)
			: NULL;

#   ifdef UNIV_DEBUG
		rwlock = &block->debug_latch;
		ut_a(!rwlock->pfs_psi);
		rwlock->pfs_psi = (PSI_server)
			? PSI_server->init_rwlock(
				buf_block_debug_latch_key.m_value, rwlock)
			: NULL;
#   endif /* UNIV_DEBUG */

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
	UNIV_MEM_DESC(frame, UNIV_PAGE_SIZE);

	/* This function should only be executed at database startup or by
	buf_pool_resize(). Either way, adaptive hash index must not exist. */
	assert_block_ahi_empty_on_init(block);

	block->frame = frame;

	block->page.buf_pool_index = buf_pool_index(buf_pool);
	block->page.state = BUF_BLOCK_NOT_USED;
	block->page.buf_fix_count = 0;
	block->page.io_fix = BUF_IO_NONE;
	block->page.flush_observer = NULL;

	block->modify_clock = 0;

	ut_d(block->page.file_page_was_freed = FALSE);

	block->index = NULL;
	block->made_dirty_with_no_latch = false;
	block->skip_flush_check = false;

	ut_d(block->page.in_page_hash = FALSE);
	ut_d(block->page.in_zip_hash = FALSE);
	ut_d(block->page.in_flush_list = FALSE);
	ut_d(block->page.in_free_list = FALSE);
	ut_d(block->page.in_LRU_list = FALSE);
	ut_d(block->in_unzip_LRU_list = FALSE);
	ut_d(block->in_withdraw_list = FALSE);

	page_zip_des_init(&block->page.zip);

	mutex_create(LATCH_ID_BUF_BLOCK_MUTEX, &block->mutex);

#if defined PFS_SKIP_BUFFER_MUTEX_RWLOCK || defined PFS_GROUP_BUFFER_SYNC
	/* If PFS_SKIP_BUFFER_MUTEX_RWLOCK is defined, skip registration
	of buffer block rwlock with performance schema.

	If PFS_GROUP_BUFFER_SYNC is defined, skip the registration
	since buffer block rwlock will be registered later in
	pfs_register_buffer_block(). */

	rw_lock_create(PFS_NOT_INSTRUMENTED, &block->lock, SYNC_LEVEL_VARYING);

	ut_d(rw_lock_create(
			PFS_NOT_INSTRUMENTED,
			&block->debug_latch, SYNC_NO_ORDER_CHECK));

#else /* PFS_SKIP_BUFFER_MUTEX_RWLOCK || PFS_GROUP_BUFFER_SYNC */

	rw_lock_create(buf_block_lock_key, &block->lock, SYNC_LEVEL_VARYING);

	ut_d(rw_lock_create(
			buf_block_debug_latch_key,
			&block->debug_latch, SYNC_NO_ORDER_CHECK));

#endif /* PFS_SKIP_BUFFER_MUTEX_RWLOCK || PFS_GROUP_BUFFER_SYNC */

	block->lock.is_block_lock = 1;

	ut_ad(rw_lock_validate(&(block->lock)));
}

/********************************************************************//**
Allocates a chunk of buffer frames.
@return chunk, or NULL on failure */
static
buf_chunk_t*
buf_chunk_init(
/*===========*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	buf_chunk_t*	chunk,		/*!< out: chunk of buffers */
	ulint		mem_size)	/*!< in: requested size in bytes */
{
	buf_block_t*	block;
	byte*		frame;
	ulint		i;

	/* Round down to a multiple of page size,
	although it already should be. */
	mem_size = ut_2pow_round(mem_size, UNIV_PAGE_SIZE);
	/* Reserve space for the block descriptors. */
	mem_size += ut_2pow_round((mem_size / UNIV_PAGE_SIZE) * (sizeof *block)
				  + (UNIV_PAGE_SIZE - 1), UNIV_PAGE_SIZE);

	DBUG_EXECUTE_IF("ib_buf_chunk_init_fails", return(NULL););

	chunk->mem = buf_pool->allocator.allocate_large(mem_size,
							&chunk->mem_pfx);

	if (UNIV_UNLIKELY(chunk->mem == NULL)) {

		return(NULL);
	}

#ifdef HAVE_LIBNUMA
	if (srv_numa_interleave) {
		int	st = mbind(chunk->mem, chunk->mem_size(),
				   MPOL_INTERLEAVE,
				   numa_all_nodes_ptr->maskp,
				   numa_all_nodes_ptr->size,
				   MPOL_MF_MOVE);
		if (st != 0) {
			ib::warn() << "Failed to set NUMA memory policy of"
				" buffer pool page frames to MPOL_INTERLEAVE"
				" (error: " << strerror(errno) << ").";
		}
	}
#endif /* HAVE_LIBNUMA */


	/* Allocate the block descriptors from
	the start of the memory block. */
	chunk->blocks = (buf_block_t*) chunk->mem;

	/* Align a pointer to the first frame.  Note that when
	os_large_page_size is smaller than UNIV_PAGE_SIZE,
	we may allocate one fewer block than requested.  When
	it is bigger, we may allocate more blocks than requested. */

	frame = (byte*) ut_align(chunk->mem, UNIV_PAGE_SIZE);
	chunk->size = chunk->mem_pfx.m_size / UNIV_PAGE_SIZE
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

	/* Init block structs and assign frames for them. Then we
	assign the frames to the first blocks (we already mapped the
	memory above). */

	block = chunk->blocks;

	for (i = chunk->size; i--; ) {

		buf_block_init(buf_pool, block, frame);
		UNIV_MEM_INVALID(block->frame, UNIV_PAGE_SIZE);

		/* Add the block to the free list */
		UT_LIST_ADD_LAST(buf_pool->free, &block->page);

		ut_d(block->page.in_free_list = TRUE);
		ut_ad(buf_pool_from_block(block) == buf_pool);

		block++;
		frame += UNIV_PAGE_SIZE;
	}

	buf_pool_register_chunk(chunk);

#ifdef PFS_GROUP_BUFFER_SYNC
	pfs_register_buffer_block(chunk);
#endif /* PFS_GROUP_BUFFER_SYNC */
	return(chunk);
}

#ifdef UNIV_DEBUG
/*********************************************************************//**
Finds a block in the given buffer chunk that points to a
given compressed page.
@return buffer block pointing to the compressed page, or NULL */
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
@return buffer block pointing to the compressed page, or NULL */
buf_block_t*
buf_pool_contains_zip(
/*==================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	const void*	data)		/*!< in: pointer to compressed page */
{
	ulint		n;
	buf_chunk_t*	chunk = buf_pool->chunks;

	ut_ad(buf_pool);
	ut_ad(buf_pool_mutex_own(buf_pool));
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
@return address of a non-free block, or NULL if all freed */
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
		case BUF_BLOCK_POOL_WATCH:
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
			buf_page_mutex_enter(block);
			ready = buf_flush_ready_for_replace(&block->page);
			buf_page_mutex_exit(block);

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
	srv_buf_pool_base_size = srv_buf_pool_size;

	buf_pool_mutex_exit_all();
}

/********************************************************************//**
Initialize a buffer pool instance.
@return DB_SUCCESS if all goes well. */
ulint
buf_pool_init_instance(
/*===================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	ulint		buf_pool_size,	/*!< in: size in bytes */
	ulint		instance_no)	/*!< in: id of the instance */
{
	ulint		i;
	ulint		chunk_size;
	buf_chunk_t*	chunk;

	ut_ad(buf_pool_size % srv_buf_pool_chunk_unit == 0);

	/* 1. Initialize general fields
	------------------------------- */
	mutex_create(LATCH_ID_BUF_POOL, &buf_pool->mutex);

	mutex_create(LATCH_ID_BUF_POOL_ZIP, &buf_pool->zip_mutex);

	new(&buf_pool->allocator)
		ut_allocator<unsigned char>(mem_key_buf_buf_pool);

	buf_pool_mutex_enter(buf_pool);

	if (buf_pool_size > 0) {
		buf_pool->n_chunks
			= buf_pool_size / srv_buf_pool_chunk_unit;
		chunk_size = srv_buf_pool_chunk_unit;

		buf_pool->chunks =
			reinterpret_cast<buf_chunk_t*>(ut_zalloc_nokey(
				buf_pool->n_chunks * sizeof(*chunk)));
		buf_pool->chunks_old = NULL;

		UT_LIST_INIT(buf_pool->LRU, &buf_page_t::LRU);
		UT_LIST_INIT(buf_pool->free, &buf_page_t::list);
		UT_LIST_INIT(buf_pool->withdraw, &buf_page_t::list);
		buf_pool->withdraw_target = 0;
		UT_LIST_INIT(buf_pool->flush_list, &buf_page_t::list);
		UT_LIST_INIT(buf_pool->unzip_LRU, &buf_block_t::unzip_LRU);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		UT_LIST_INIT(buf_pool->zip_clean, &buf_page_t::list);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

		for (i = 0; i < UT_ARR_SIZE(buf_pool->zip_free); ++i) {
			UT_LIST_INIT(
				buf_pool->zip_free[i], &buf_buddy_free_t::list);
		}

		buf_pool->curr_size = 0;
		chunk = buf_pool->chunks;

		do {
			if (!buf_chunk_init(buf_pool, chunk, chunk_size)) {
				while (--chunk >= buf_pool->chunks) {
					buf_block_t*	block = chunk->blocks;

					for (i = chunk->size; i--; block++) {
						mutex_free(&block->mutex);
						rw_lock_free(&block->lock);

						ut_d(rw_lock_free(
							&block->debug_latch));
					}

					buf_pool->allocator.deallocate_large(
						chunk->mem, &chunk->mem_pfx);
				}
				ut_free(buf_pool->chunks);
				buf_pool_mutex_exit(buf_pool);

				return(DB_ERROR);
			}

			buf_pool->curr_size += chunk->size;
		} while (++chunk < buf_pool->chunks + buf_pool->n_chunks);

		buf_pool->instance_no = instance_no;
		buf_pool->read_ahead_area =
			ut_min(BUF_READ_AHEAD_PAGES,
			       ut_2_power_up(buf_pool->curr_size /
					     BUF_READ_AHEAD_PORTION));
		buf_pool->curr_pool_size = buf_pool->curr_size * UNIV_PAGE_SIZE;

		buf_pool->old_size = buf_pool->curr_size;
		buf_pool->n_chunks_new = buf_pool->n_chunks;

		/* Number of locks protecting page_hash must be a
		power of two */
		srv_n_page_hash_locks = static_cast<ulong>(
			 ut_2_power_up(srv_n_page_hash_locks));
		ut_a(srv_n_page_hash_locks != 0);
		ut_a(srv_n_page_hash_locks <= MAX_PAGE_HASH_LOCKS);

		buf_pool->page_hash = ib_create(
			2 * buf_pool->curr_size,
			LATCH_ID_HASH_TABLE_RW_LOCK,
			srv_n_page_hash_locks, MEM_HEAP_FOR_PAGE_HASH);

		buf_pool->page_hash_old = NULL;

		buf_pool->zip_hash = hash_create(2 * buf_pool->curr_size);

		buf_pool->last_printout_time = ut_time();
	}
	/* 2. Initialize flushing fields
	-------------------------------- */

	mutex_create(LATCH_ID_FLUSH_LIST, &buf_pool->flush_list_mutex);

	for (i = BUF_FLUSH_LRU; i < BUF_FLUSH_N_TYPES; i++) {
		buf_pool->no_flush[i] = os_event_create(0);
	}

	buf_pool->watch = (buf_page_t*) ut_zalloc_nokey(
		sizeof(*buf_pool->watch) * BUF_POOL_WATCH_SIZE);
	for (i = 0; i < BUF_POOL_WATCH_SIZE; i++) {
		buf_pool->watch[i].buf_pool_index = buf_pool->instance_no;
	}

	/* All fields are initialized by ut_zalloc_nokey(). */

	buf_pool->try_LRU_scan = TRUE;

	/* Initialize the hazard pointer for flush_list batches */
	new(&buf_pool->flush_hp)
		FlushHp(buf_pool, &buf_pool->flush_list_mutex);

	/* Initialize the hazard pointer for LRU batches */
	new(&buf_pool->lru_hp) LRUHp(buf_pool, &buf_pool->mutex);

	/* Initialize the iterator for LRU scan search */
	new(&buf_pool->lru_scan_itr) LRUItr(buf_pool, &buf_pool->mutex);

	/* Initialize the iterator for single page scan search */
	new(&buf_pool->single_scan_itr) LRUItr(buf_pool, &buf_pool->mutex);

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
	buf_page_t*	prev_bpage = 0;

	mutex_free(&buf_pool->mutex);
	mutex_free(&buf_pool->zip_mutex);
	mutex_free(&buf_pool->flush_list_mutex);

	for (bpage = UT_LIST_GET_LAST(buf_pool->LRU);
	     bpage != NULL;
	     bpage = prev_bpage) {

		prev_bpage = UT_LIST_GET_PREV(LRU, bpage);
		buf_page_state	state = buf_page_get_state(bpage);

		ut_ad(buf_page_in_file(bpage));
		ut_ad(bpage->in_LRU_list);

		if (state != BUF_BLOCK_FILE_PAGE) {
			/* We must not have any dirty block except
			when doing a fast shutdown. */
			ut_ad(state == BUF_BLOCK_ZIP_PAGE
			      || srv_fast_shutdown == 2);
			buf_page_free_descriptor(bpage);
		}
	}

	ut_free(buf_pool->watch);
	buf_pool->watch = NULL;

	chunks = buf_pool->chunks;
	chunk = chunks + buf_pool->n_chunks;

	while (--chunk >= chunks) {
		buf_block_t*	block = chunk->blocks;

		for (ulint i = chunk->size; i--; block++) {
			mutex_free(&block->mutex);
			rw_lock_free(&block->lock);

			ut_d(rw_lock_free(&block->debug_latch));
		}

		buf_pool->allocator.deallocate_large(
			chunk->mem, &chunk->mem_pfx);
	}

	for (ulint i = BUF_FLUSH_LRU; i < BUF_FLUSH_N_TYPES; ++i) {
		os_event_destroy(buf_pool->no_flush[i]);
	}

	ut_free(buf_pool->chunks);
	ha_clear(buf_pool->page_hash);
	hash_table_free(buf_pool->page_hash);
	hash_table_free(buf_pool->zip_hash);

	buf_pool->allocator.~ut_allocator();
}

/********************************************************************//**
Creates the buffer pool.
@return DB_SUCCESS if success, DB_ERROR if not enough memory or error */
dberr_t
buf_pool_init(
/*==========*/
	ulint	total_size,	/*!< in: size of the total pool in bytes */
	ulint	n_instances)	/*!< in: number of instances */
{
	ulint		i;
	const ulint	size	= total_size / n_instances;

	ut_ad(n_instances > 0);
	ut_ad(n_instances <= MAX_BUFFER_POOLS);
	ut_ad(n_instances == srv_buf_pool_instances);

	NUMA_MEMPOLICY_INTERLEAVE_IN_SCOPE;

	buf_pool_resizing = false;
	buf_pool_withdrawing = false;
	buf_withdraw_clock = 0;

	buf_pool_ptr = (buf_pool_t*) ut_zalloc_nokey(
		n_instances * sizeof *buf_pool_ptr);

	buf_chunk_map_reg = UT_NEW_NOKEY(buf_pool_chunk_map_t());

	for (i = 0; i < n_instances; i++) {
		buf_pool_t*	ptr	= &buf_pool_ptr[i];

		if (buf_pool_init_instance(ptr, size, i) != DB_SUCCESS) {

			/* Free all the instances created so far. */
			buf_pool_free(i);

			return(DB_ERROR);
		}
	}

	buf_chunk_map_ref = buf_chunk_map_reg;

	buf_pool_set_sizes();
	buf_LRU_old_ratio_update(100 * 3/ 8, FALSE);

	btr_search_sys_create(buf_pool_get_curr_size() / sizeof(void*) / 64);

	return(DB_SUCCESS);
}

/********************************************************************//**
Frees the buffer pool at shutdown.  This must not be invoked before
freeing all mutexes. */
void
buf_pool_free(
/*==========*/
	ulint	n_instances)	/*!< in: numbere of instances to free */
{
	for (ulint i = 0; i < n_instances; i++) {
		buf_pool_free_instance(buf_pool_from_array(i));
	}

	UT_DELETE(buf_chunk_map_reg);
	buf_chunk_map_reg = buf_chunk_map_ref = NULL;

	ut_free(buf_pool_ptr);
	buf_pool_ptr = NULL;
}

/** Reallocate a control block.
@param[in]	buf_pool	buffer pool instance
@param[in]	block		pointer to control block
@retval false	if failed because of no free blocks. */
static
bool
buf_page_realloc(
	buf_pool_t*	buf_pool,
	buf_block_t*	block)
{
	buf_block_t*	new_block;

	ut_ad(buf_pool_withdrawing);
	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);

	new_block = buf_LRU_get_free_only(buf_pool);

	if (new_block == NULL) {
		return(false); /* free_list was not enough */
	}

	rw_lock_t*	hash_lock = buf_page_hash_lock_get(buf_pool, block->page.id);

	rw_lock_x_lock(hash_lock);
	mutex_enter(&block->mutex);

	if (buf_page_can_relocate(&block->page)) {
		mutex_enter(&new_block->mutex);

		memcpy(new_block->frame, block->frame, UNIV_PAGE_SIZE);
		memcpy(&new_block->page, &block->page, sizeof block->page);

		/* relocate LRU list */
		ut_ad(block->page.in_LRU_list);
		ut_ad(!block->page.in_zip_hash);
		ut_d(block->page.in_LRU_list = FALSE);

		buf_LRU_adjust_hp(buf_pool, &block->page);

		buf_page_t*	prev_b = UT_LIST_GET_PREV(LRU, &block->page);
		UT_LIST_REMOVE(buf_pool->LRU, &block->page);

		if (prev_b != NULL) {
			UT_LIST_INSERT_AFTER(buf_pool->LRU, prev_b, &new_block->page);
		} else {
			UT_LIST_ADD_FIRST(buf_pool->LRU, &new_block->page);
		}

		if (buf_pool->LRU_old == &block->page) {
			buf_pool->LRU_old = &new_block->page;
		}

		ut_ad(new_block->page.in_LRU_list);

		/* relocate unzip_LRU list */
		if (block->page.zip.data != NULL) {
			ut_ad(block->in_unzip_LRU_list);
			ut_d(new_block->in_unzip_LRU_list = TRUE);
			UNIV_MEM_DESC(&new_block->page.zip.data,
				      page_zip_get_size(&new_block->page.zip));

			buf_block_t*	prev_block = UT_LIST_GET_PREV(unzip_LRU, block);
			UT_LIST_REMOVE(buf_pool->unzip_LRU, block);

			ut_d(block->in_unzip_LRU_list = FALSE);
			block->page.zip.data = NULL;
			page_zip_set_size(&block->page.zip, 0);

			if (prev_block != NULL) {
				UT_LIST_INSERT_AFTER(buf_pool->unzip_LRU, prev_block, new_block);
			} else {
				UT_LIST_ADD_FIRST(buf_pool->unzip_LRU, new_block);
			}
		} else {
			ut_ad(!block->in_unzip_LRU_list);
			ut_d(new_block->in_unzip_LRU_list = FALSE);
		}

		/* relocate buf_pool->page_hash */
		ut_ad(block->page.in_page_hash);
		ut_ad(&block->page == buf_page_hash_get_low(buf_pool,
							    block->page.id));
		ut_d(block->page.in_page_hash = FALSE);
		ulint	fold = block->page.id.fold();
		ut_ad(fold == new_block->page.id.fold());
		HASH_DELETE(buf_page_t, hash, buf_pool->page_hash, fold, (&block->page));
		HASH_INSERT(buf_page_t, hash, buf_pool->page_hash, fold, (&new_block->page));

		ut_ad(new_block->page.in_page_hash);

		buf_block_modify_clock_inc(block);
		memset(block->frame + FIL_PAGE_OFFSET, 0xff, 4);
		memset(block->frame + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, 0xff, 4);
		UNIV_MEM_INVALID(block->frame, UNIV_PAGE_SIZE);
		buf_block_set_state(block, BUF_BLOCK_REMOVE_HASH);
		block->page.id.reset(ULINT32_UNDEFINED, ULINT32_UNDEFINED);

		/* Relocate buf_pool->flush_list. */
		if (block->page.oldest_modification) {
			buf_flush_relocate_on_flush_list(
				&block->page, &new_block->page);
		}

		/* set other flags of buf_block_t */

		/* This code should only be executed by buf_pool_resize(),
		while the adaptive hash index is disabled. */
		assert_block_ahi_empty(block);
		assert_block_ahi_empty_on_init(new_block);
		ut_ad(!block->index);
		new_block->index	= NULL;
		new_block->n_hash_helps	= 0;
		new_block->n_fields	= 1;
		new_block->left_side	= TRUE;

		new_block->lock_hash_val = block->lock_hash_val;
		ut_ad(new_block->lock_hash_val == lock_rec_hash(
			new_block->page.id.space(),
			new_block->page.id.page_no()));

		rw_lock_x_unlock(hash_lock);
		mutex_exit(&new_block->mutex);

		/* free block */
		buf_block_set_state(block, BUF_BLOCK_MEMORY);
		buf_LRU_block_free_non_file_page(block);

		mutex_exit(&block->mutex);
	} else {
		rw_lock_x_unlock(hash_lock);
		mutex_exit(&block->mutex);

		/* free new_block */
		mutex_enter(&new_block->mutex);
		buf_LRU_block_free_non_file_page(new_block);
		mutex_exit(&new_block->mutex);
	}

	return(true); /* free_list was enough */
}

/** Sets the global variable that feeds MySQL's innodb_buffer_pool_resize_status
to the specified string. The format and the following parameters are the
same as the ones used for printf(3).
@param[in]	fmt	format
@param[in]	...	extra parameters according to fmt */
static
void
buf_resize_status(
	const char*	fmt,
	...)
{
	va_list	ap;

	va_start(ap, fmt);

	ut_vsnprintf(
		export_vars.innodb_buffer_pool_resize_status,
		sizeof(export_vars.innodb_buffer_pool_resize_status),
		fmt, ap);

	va_end(ap);

	ib::info() << export_vars.innodb_buffer_pool_resize_status;
}

/** Determines if a block is intended to be withdrawn.
@param[in]	buf_pool	buffer pool instance
@param[in]	block		pointer to control block
@retval true	if will be withdrawn */
bool
buf_block_will_withdrawn(
	buf_pool_t*		buf_pool,
	const buf_block_t*	block)
{
	ut_ad(buf_pool->curr_size < buf_pool->old_size);
	ut_ad(!buf_pool_resizing || buf_pool_mutex_own(buf_pool));

	const buf_chunk_t*	chunk
		= buf_pool->chunks + buf_pool->n_chunks_new;
	const buf_chunk_t*	echunk
		= buf_pool->chunks + buf_pool->n_chunks;

	while (chunk < echunk) {
		if (block >= chunk->blocks
		    && block < chunk->blocks + chunk->size) {
			return(true);
		}
		++chunk;
	}

	return(false);
}

/** Determines if a frame is intended to be withdrawn.
@param[in]	buf_pool	buffer pool instance
@param[in]	ptr		pointer to a frame
@retval true	if will be withdrawn */
bool
buf_frame_will_withdrawn(
	buf_pool_t*	buf_pool,
	const byte*	ptr)
{
	ut_ad(buf_pool->curr_size < buf_pool->old_size);
	ut_ad(!buf_pool_resizing || buf_pool_mutex_own(buf_pool));

	const buf_chunk_t*	chunk
		= buf_pool->chunks + buf_pool->n_chunks_new;
	const buf_chunk_t*	echunk
		= buf_pool->chunks + buf_pool->n_chunks;

	while (chunk < echunk) {
		if (ptr >= chunk->blocks->frame
		    && ptr < (chunk->blocks + chunk->size - 1)->frame
			     + UNIV_PAGE_SIZE) {
			return(true);
		}
		++chunk;
	}

	return(false);
}

/** Withdraw the buffer pool blocks from end of the buffer pool instance
until withdrawn by buf_pool->withdraw_target.
@param[in]	buf_pool	buffer pool instance
@retval true	if retry is needed */
static
bool
buf_pool_withdraw_blocks(
	buf_pool_t*	buf_pool)
{
	buf_block_t*	block;
	ulint		loop_count = 0;
	ulint		i = buf_pool_index(buf_pool);

	ib::info() << "buffer pool " << i
		<< " : start to withdraw the last "
		<< buf_pool->withdraw_target << " blocks.";

	/* Minimize buf_pool->zip_free[i] lists */
	buf_pool_mutex_enter(buf_pool);
	buf_buddy_condense_free(buf_pool);
	buf_pool_mutex_exit(buf_pool);

	while (UT_LIST_GET_LEN(buf_pool->withdraw)
	       < buf_pool->withdraw_target) {

		/* try to withdraw from free_list */
		ulint	count1 = 0;

		buf_pool_mutex_enter(buf_pool);
		block = reinterpret_cast<buf_block_t*>(
			UT_LIST_GET_FIRST(buf_pool->free));
		while (block != NULL
		       && UT_LIST_GET_LEN(buf_pool->withdraw)
			  < buf_pool->withdraw_target) {
			ut_ad(block->page.in_free_list);
			ut_ad(!block->page.in_flush_list);
			ut_ad(!block->page.in_LRU_list);
			ut_a(!buf_page_in_file(&block->page));

			buf_block_t*	next_block;
			next_block = reinterpret_cast<buf_block_t*>(
				UT_LIST_GET_NEXT(
					list, &block->page));

			if (buf_block_will_withdrawn(buf_pool, block)) {
				/* This should be withdrawn */
				UT_LIST_REMOVE(
					buf_pool->free,
					&block->page);
				UT_LIST_ADD_LAST(
					buf_pool->withdraw,
					&block->page);
				ut_d(block->in_withdraw_list = TRUE);
				count1++;
			}

			block = next_block;
		}
		buf_pool_mutex_exit(buf_pool);

		/* reserve free_list length */
		if (UT_LIST_GET_LEN(buf_pool->withdraw)
		    < buf_pool->withdraw_target) {
			ulint	scan_depth;
			ulint	n_flushed = 0;

			/* cap scan_depth with current LRU size. */
			buf_pool_mutex_enter(buf_pool);
			scan_depth = UT_LIST_GET_LEN(buf_pool->LRU);
			buf_pool_mutex_exit(buf_pool);

			scan_depth = ut_min(
				ut_max(buf_pool->withdraw_target
				       - UT_LIST_GET_LEN(buf_pool->withdraw),
				       static_cast<ulint>(srv_LRU_scan_depth)),
				scan_depth);

			buf_flush_do_batch(buf_pool, BUF_FLUSH_LRU,
				scan_depth, 0, &n_flushed);
			buf_flush_wait_batch_end(buf_pool, BUF_FLUSH_LRU);

			if (n_flushed) {
				MONITOR_INC_VALUE_CUMULATIVE(
					MONITOR_LRU_BATCH_FLUSH_TOTAL_PAGE,
					MONITOR_LRU_BATCH_FLUSH_COUNT,
					MONITOR_LRU_BATCH_FLUSH_PAGES,
					n_flushed);
			}
		}

		/* relocate blocks/buddies in withdrawn area */
		ulint	count2 = 0;

		buf_pool_mutex_enter(buf_pool);
		buf_page_t*	bpage;
		bpage = UT_LIST_GET_FIRST(buf_pool->LRU);
		while (bpage != NULL) {
			BPageMutex*	block_mutex;
			buf_page_t*	next_bpage;

			block_mutex = buf_page_get_mutex(bpage);
			mutex_enter(block_mutex);

			next_bpage = UT_LIST_GET_NEXT(LRU, bpage);

			if (bpage->zip.data != NULL
			    && buf_frame_will_withdrawn(
				buf_pool,
				static_cast<byte*>(bpage->zip.data))) {

				if (buf_page_can_relocate(bpage)) {
					mutex_exit(block_mutex);
					buf_pool_mutex_exit_forbid(buf_pool);
					if(!buf_buddy_realloc(
						buf_pool, bpage->zip.data,
						page_zip_get_size(
							&bpage->zip))) {

						/* failed to allocate block */
						buf_pool_mutex_exit_allow(
							buf_pool);
						break;
					}
					buf_pool_mutex_exit_allow(buf_pool);
					mutex_enter(block_mutex);
					count2++;
				}
				/* NOTE: if the page is in use,
				not reallocated yet */
			}

			if (buf_page_get_state(bpage)
			    == BUF_BLOCK_FILE_PAGE
			    && buf_block_will_withdrawn(
				buf_pool,
				reinterpret_cast<buf_block_t*>(bpage))) {

				if (buf_page_can_relocate(bpage)) {
					mutex_exit(block_mutex);
					buf_pool_mutex_exit_forbid(buf_pool);
					if(!buf_page_realloc(
						buf_pool,
						reinterpret_cast<buf_block_t*>(
							bpage))) {
						/* failed to allocate block */
						buf_pool_mutex_exit_allow(
							buf_pool);
						break;
					}
					buf_pool_mutex_exit_allow(buf_pool);
					count2++;
				} else {
					mutex_exit(block_mutex);
				}
				/* NOTE: if the page is in use,
				not reallocated yet */
			} else {
				mutex_exit(block_mutex);
			}

			bpage = next_bpage;
		}
		buf_pool_mutex_exit(buf_pool);

		buf_resize_status(
			"buffer pool %lu : withdrawing blocks. (%lu/%lu)",
			i, UT_LIST_GET_LEN(buf_pool->withdraw),
			buf_pool->withdraw_target);

		ib::info() << "buffer pool " << i << " : withdrew "
			<< count1 << " blocks from free list."
			<< " Tried to relocate " << count2 << " pages ("
			<< UT_LIST_GET_LEN(buf_pool->withdraw) << "/"
			<< buf_pool->withdraw_target << ").";

		if (++loop_count >= 10) {
			/* give up for now.
			retried after user threads paused. */

			ib::info() << "buffer pool " << i
				<< " : will retry to withdraw later.";

			/* need retry later */
			return(true);
		}
	}

	/* confirm withdrawn enough */
	const buf_chunk_t*	chunk
		= buf_pool->chunks + buf_pool->n_chunks_new;
	const buf_chunk_t*	echunk
		= buf_pool->chunks + buf_pool->n_chunks;

	while (chunk < echunk) {
		block = chunk->blocks;
		for (ulint j = chunk->size; j--; block++) {
			/* If !=BUF_BLOCK_NOT_USED block in the
			withdrawn area, it means corruption
			something */
			ut_a(buf_block_get_state(block)
				== BUF_BLOCK_NOT_USED);
			ut_ad(block->in_withdraw_list);
		}
		++chunk;
	}

	ib::info() << "buffer pool " << i << " : withdrawn target "
		<< UT_LIST_GET_LEN(buf_pool->withdraw) << " blocks.";

	/* retry is not needed */
	++buf_withdraw_clock;
	os_wmb;

	return(false);
}

/** resize page_hash and zip_hash for a buffer pool instance.
@param[in]	buf_pool	buffer pool instance */
static
void
buf_pool_resize_hash(
	buf_pool_t*	buf_pool)
{
	hash_table_t*	new_hash_table;

	ut_ad(buf_pool->page_hash_old == NULL);

	/* recreate page_hash */
	new_hash_table = ib_recreate(
		buf_pool->page_hash, 2 * buf_pool->curr_size);

	for (ulint i = 0; i < hash_get_n_cells(buf_pool->page_hash); i++) {
		buf_page_t*	bpage;

		bpage = static_cast<buf_page_t*>(
			HASH_GET_FIRST(
				buf_pool->page_hash, i));

		while (bpage) {
			buf_page_t*	prev_bpage = bpage;
			ulint		fold;

			bpage = static_cast<buf_page_t*>(
				HASH_GET_NEXT(
					hash, prev_bpage));

			fold = prev_bpage->id.fold();

			HASH_DELETE(buf_page_t, hash,
				buf_pool->page_hash, fold,
				prev_bpage);

			HASH_INSERT(buf_page_t, hash,
				new_hash_table, fold,
				prev_bpage);
		}
	}

	buf_pool->page_hash_old = buf_pool->page_hash;
	buf_pool->page_hash = new_hash_table;

	/* recreate zip_hash */
	new_hash_table = hash_create(2 * buf_pool->curr_size);

	for (ulint i = 0; i < hash_get_n_cells(buf_pool->zip_hash); i++) {
		buf_page_t*	bpage;

		bpage = static_cast<buf_page_t*>(
			HASH_GET_FIRST(buf_pool->zip_hash, i));

		while (bpage) {
			buf_page_t*	prev_bpage = bpage;
			ulint		fold;

			bpage = static_cast<buf_page_t*>(
				HASH_GET_NEXT(
					hash, prev_bpage));

			fold = BUF_POOL_ZIP_FOLD(
				reinterpret_cast<buf_block_t*>(
					prev_bpage));

			HASH_DELETE(buf_page_t, hash,
				buf_pool->zip_hash, fold,
				prev_bpage);

			HASH_INSERT(buf_page_t, hash,
				new_hash_table, fold,
				prev_bpage);
		}
	}

	hash_table_free(buf_pool->zip_hash);
	buf_pool->zip_hash = new_hash_table;
}

#ifndef DBUG_OFF
/** This is a debug routine to inject an memory allocation failure error. */
static
void
buf_pool_resize_chunk_make_null(buf_chunk_t** new_chunks)
{
	static int count = 0;

	if (count == 1) {
		ut_free(*new_chunks);
		*new_chunks = NULL;
	}

	count++;
}
#endif // DBUG_OFF

/** Resize the buffer pool based on srv_buf_pool_size from
srv_buf_pool_old_size. */
void
buf_pool_resize()
{
	buf_pool_t*	buf_pool;
	ulint		new_instance_size;
	bool		warning = false;

	NUMA_MEMPOLICY_INTERLEAVE_IN_SCOPE;

	ut_ad(!buf_pool_resizing);
	ut_ad(!buf_pool_withdrawing);
	ut_ad(srv_buf_pool_chunk_unit > 0);

	new_instance_size = srv_buf_pool_size / srv_buf_pool_instances;
	new_instance_size /= UNIV_PAGE_SIZE;

	buf_resize_status("Resizing buffer pool from " ULINTPF " to "
			  ULINTPF " (unit=" ULINTPF ").",
			  srv_buf_pool_old_size, srv_buf_pool_size,
			  srv_buf_pool_chunk_unit);

	/* set new limit for all buffer pool for resizing */
	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool = buf_pool_from_array(i);
		buf_pool_mutex_enter(buf_pool);

		ut_ad(buf_pool->curr_size == buf_pool->old_size);
		ut_ad(buf_pool->n_chunks_new == buf_pool->n_chunks);
		ut_ad(UT_LIST_GET_LEN(buf_pool->withdraw) == 0);
		ut_ad(buf_pool->flush_rbt == NULL);

		buf_pool->curr_size = new_instance_size;

		buf_pool->n_chunks_new = new_instance_size * UNIV_PAGE_SIZE
			/ srv_buf_pool_chunk_unit;

		buf_pool_mutex_exit(buf_pool);
	}

	/* disable AHI if needed */
	bool	btr_search_disabled = false;

	buf_resize_status("Disabling adaptive hash index.");

	btr_search_s_lock_all();
	if (btr_search_enabled) {
		btr_search_s_unlock_all();
		btr_search_disabled = true;
	} else {
		btr_search_s_unlock_all();
	}

	btr_search_disable(true);

	if (btr_search_disabled) {
		ib::info() << "disabled adaptive hash index.";
	}

	/* set withdraw target */
	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool = buf_pool_from_array(i);
		if (buf_pool->curr_size < buf_pool->old_size) {
			ulint	withdraw_target = 0;

			const buf_chunk_t*	chunk
				= buf_pool->chunks + buf_pool->n_chunks_new;
			const buf_chunk_t*	echunk
				= buf_pool->chunks + buf_pool->n_chunks;

			while (chunk < echunk) {
				withdraw_target += chunk->size;
				++chunk;
			}

			ut_ad(buf_pool->withdraw_target == 0);
			buf_pool->withdraw_target = withdraw_target;
			buf_pool_withdrawing = true;
		}
	}

	buf_resize_status("Withdrawing blocks to be shrunken.");

	ib_time_t	withdraw_started = ut_time();
	ulint		message_interval = 60;
	ulint		retry_interval = 1;

withdraw_retry:
	bool	should_retry_withdraw = false;

	/* wait for the number of blocks fit to the new size (if needed)*/
	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool = buf_pool_from_array(i);
		if (buf_pool->curr_size < buf_pool->old_size) {

			should_retry_withdraw |=
				buf_pool_withdraw_blocks(buf_pool);
		}
	}

	if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
		/* abort to resize for shutdown. */
		buf_pool_withdrawing = false;
		return;
	}

	/* abort buffer pool load */
	buf_load_abort();

	if (should_retry_withdraw
	    && ut_difftime(ut_time(), withdraw_started) >= message_interval) {

		if (message_interval > 900) {
			message_interval = 1800;
		} else {
			message_interval *= 2;
		}

		lock_mutex_enter();
		trx_sys_mutex_enter();
		bool	found = false;
		for (trx_t* trx = UT_LIST_GET_FIRST(trx_sys->mysql_trx_list);
		     trx != NULL;
		     trx = UT_LIST_GET_NEXT(mysql_trx_list, trx)) {
			if (trx->state != TRX_STATE_NOT_STARTED
			    && trx->mysql_thd != NULL
			    && ut_difftime(withdraw_started,
					   trx->start_time) > 0) {
				if (!found) {
					ib::warn() <<
						"The following trx might hold"
						" the blocks in buffer pool to"
					        " be withdrawn. Buffer pool"
						" resizing can complete only"
						" after all the transactions"
						" below release the blocks.";
					found = true;
				}

				lock_trx_print_wait_and_mvcc_state(
					stderr, trx);
			}
		}
		trx_sys_mutex_exit();
		lock_mutex_exit();

		withdraw_started = ut_time();
	}

	if (should_retry_withdraw) {
		ib::info() << "Will retry to withdraw " << retry_interval
			<< " seconds later.";
		os_thread_sleep(retry_interval * 1000000);

		if (retry_interval > 5) {
			retry_interval = 10;
		} else {
			retry_interval *= 2;
		}

		goto withdraw_retry;
	}

	buf_pool_withdrawing = false;

	buf_resize_status("Latching whole of buffer pool.");

#ifndef DBUG_OFF
	{
		bool	should_wait = true;

		while (should_wait) {
			should_wait = false;
			DBUG_EXECUTE_IF(
				"ib_buf_pool_resize_wait_before_resize",
				should_wait = true; os_thread_sleep(10000););
		}
	}
#endif /* !DBUG_OFF */

	if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
		return;
	}

	/* Indicate critical path */
	buf_pool_resizing = true;

	/* Acquire all buf_pool_mutex/hash_lock */
	for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
		buf_pool_t*	buf_pool = buf_pool_from_array(i);

		buf_pool_mutex_enter(buf_pool);
	}
	for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
		buf_pool_t*	buf_pool = buf_pool_from_array(i);

		hash_lock_x_all(buf_pool->page_hash);
	}

	buf_chunk_map_reg = UT_NEW_NOKEY(buf_pool_chunk_map_t());

	/* add/delete chunks */
	for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
		buf_pool_t*	buf_pool = buf_pool_from_array(i);
		buf_chunk_t*	chunk;
		buf_chunk_t*	echunk;

		buf_resize_status("buffer pool %lu :"
			" resizing with chunks %lu to %lu.",
			i, buf_pool->n_chunks, buf_pool->n_chunks_new);

		if (buf_pool->n_chunks_new < buf_pool->n_chunks) {
			/* delete chunks */
			chunk = buf_pool->chunks
				+ buf_pool->n_chunks_new;
			echunk = buf_pool->chunks + buf_pool->n_chunks;

			ulint	sum_freed = 0;

			while (chunk < echunk) {
				buf_block_t*	block = chunk->blocks;

				for (ulint j = chunk->size;
				     j--; block++) {
					mutex_free(&block->mutex);
					rw_lock_free(&block->lock);

					ut_d(rw_lock_free(
						&block->debug_latch));
				}

				buf_pool->allocator.deallocate_large(
					chunk->mem, &chunk->mem_pfx);

				sum_freed += chunk->size;

				++chunk;
			}

			/* discard withdraw list */
			UT_LIST_INIT(buf_pool->withdraw,
				     &buf_page_t::list);
			buf_pool->withdraw_target = 0;

			ib::info() << "buffer pool " << i << " : "
				<< buf_pool->n_chunks - buf_pool->n_chunks_new
				<< " chunks (" << sum_freed
				<< " blocks) were freed.";

			buf_pool->n_chunks = buf_pool->n_chunks_new;
		}

		{
			/* reallocate buf_pool->chunks */
			const ulint	new_chunks_size
				= buf_pool->n_chunks_new * sizeof(*chunk);

			buf_chunk_t*	new_chunks
				= reinterpret_cast<buf_chunk_t*>(
					ut_zalloc_nokey_nofatal(new_chunks_size));

			DBUG_EXECUTE_IF("buf_pool_resize_chunk_null",
				buf_pool_resize_chunk_make_null(&new_chunks););

			if (new_chunks == NULL) {
				ib::error() << "buffer pool " << i
					<< " : failed to allocate"
					" the chunk array.";
				buf_pool->n_chunks_new
					= buf_pool->n_chunks;
				warning = true;
				buf_pool->chunks_old = NULL;
				goto calc_buf_pool_size;
			}

			ulint	n_chunks_copy = ut_min(buf_pool->n_chunks_new,
						       buf_pool->n_chunks);

			memcpy(new_chunks, buf_pool->chunks,
			       n_chunks_copy * sizeof(*chunk));

			for (ulint j = 0; j < n_chunks_copy; j++) {
				buf_pool_register_chunk(&new_chunks[j]);
			}

			buf_pool->chunks_old = buf_pool->chunks;
			buf_pool->chunks = new_chunks;
		}


		if (buf_pool->n_chunks_new > buf_pool->n_chunks) {
			/* add chunks */
			chunk = buf_pool->chunks + buf_pool->n_chunks;
			echunk = buf_pool->chunks
				+ buf_pool->n_chunks_new;

			ulint	sum_added = 0;
			ulint	n_chunks = buf_pool->n_chunks;

			while (chunk < echunk) {
				ulong	unit = srv_buf_pool_chunk_unit;

				if (!buf_chunk_init(buf_pool, chunk, unit)) {

					ib::error() << "buffer pool " << i
						<< " : failed to allocate"
						" new memory.";

					warning = true;

					buf_pool->n_chunks_new
						= n_chunks;

					break;
				}

				sum_added += chunk->size;

				++n_chunks;
				++chunk;
			}

			ib::info() << "buffer pool " << i << " : "
				<< buf_pool->n_chunks_new - buf_pool->n_chunks
				<< " chunks (" << sum_added
				<< " blocks) were added.";

			buf_pool->n_chunks = n_chunks;
		}
calc_buf_pool_size:

		/* recalc buf_pool->curr_size */
		ulint	new_size = 0;

		chunk = buf_pool->chunks;
		do {
			new_size += chunk->size;
		} while (++chunk < buf_pool->chunks
				   + buf_pool->n_chunks);

		buf_pool->curr_size = new_size;
		buf_pool->n_chunks_new = buf_pool->n_chunks;

		if (buf_pool->chunks_old) {
			ut_free(buf_pool->chunks_old);
			buf_pool->chunks_old = NULL;
		}
	}

	buf_pool_chunk_map_t*	chunk_map_old = buf_chunk_map_ref;
	buf_chunk_map_ref = buf_chunk_map_reg;

	/* set instance sizes */
	{
		ulint	curr_size = 0;

		for (ulint i = 0; i < srv_buf_pool_instances; i++) {
			buf_pool = buf_pool_from_array(i);

			ut_ad(UT_LIST_GET_LEN(buf_pool->withdraw) == 0);

			buf_pool->read_ahead_area =
				ut_min(BUF_READ_AHEAD_PAGES,
				       ut_2_power_up(buf_pool->curr_size /
						      BUF_READ_AHEAD_PORTION));
			buf_pool->curr_pool_size
				= buf_pool->curr_size * UNIV_PAGE_SIZE;
			curr_size += buf_pool->curr_pool_size;
			buf_pool->old_size = buf_pool->curr_size;
		}
		srv_buf_pool_curr_size = curr_size;
		innodb_set_buf_pool_size(buf_pool_size_align(curr_size));
	}

	const bool	new_size_too_diff
		= srv_buf_pool_base_size > srv_buf_pool_size * 2
			|| srv_buf_pool_base_size * 2 < srv_buf_pool_size;

	/* Normalize page_hash and zip_hash,
	if the new size is too different */
	if (!warning && new_size_too_diff) {

		buf_resize_status("Resizing hash tables.");

		for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
			buf_pool_t*	buf_pool = buf_pool_from_array(i);

			buf_pool_resize_hash(buf_pool);

			ib::info() << "buffer pool " << i
				<< " : hash tables were resized.";
		}
	}

	/* Release all buf_pool_mutex/page_hash */
	for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
		buf_pool_t*	buf_pool = buf_pool_from_array(i);

		hash_unlock_x_all(buf_pool->page_hash);
		buf_pool_mutex_exit(buf_pool);

		if (buf_pool->page_hash_old != NULL) {
			hash_table_free(buf_pool->page_hash_old);
			buf_pool->page_hash_old = NULL;
		}
	}

	UT_DELETE(chunk_map_old);

	buf_pool_resizing = false;

	/* Normalize other components, if the new size is too different */
	if (!warning && new_size_too_diff) {
		srv_buf_pool_base_size = srv_buf_pool_size;

		buf_resize_status("Resizing also other hash tables.");

		/* normalize lock_sys */
		srv_lock_table_size = 5 * (srv_buf_pool_size / UNIV_PAGE_SIZE);
		lock_sys_resize(srv_lock_table_size);

		/* normalize btr_search_sys */
		btr_search_sys_resize(
			buf_pool_get_curr_size() / sizeof(void*) / 64);

		/* normalize dict_sys */
		dict_resize();

		ib::info() << "Resized hash tables at lock_sys,"
			" adaptive hash index, dictionary.";
	}

	/* normalize ibuf->max_size */
	ibuf_max_size_update(srv_change_buffer_max_size);

	if (srv_buf_pool_old_size != srv_buf_pool_size) {

		ib::info() << "Completed to resize buffer pool from "
			<< srv_buf_pool_old_size
			<< " to " << srv_buf_pool_size << ".";
		srv_buf_pool_old_size = srv_buf_pool_size;
	}

	/* enable AHI if needed */
	if (btr_search_disabled) {
		btr_search_enable();
		ib::info() << "Re-enabled adaptive hash index.";
	}

	char	now[32];

	ut_sprintf_timestamp(now);
	if (!warning) {
		buf_resize_status("Completed resizing buffer pool at %s.",
			now);
	} else {
		buf_resize_status("Resizing buffer pool failed,"
			" finished resizing at %s.", now);
	}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(buf_validate());
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	return;
}

/** This is the thread for resizing buffer pool. It waits for an event and
when waked up either performs a resizing and sleeps again.
@param[in]	arg	a dummy parameter required by os_thread_create.
@return	this function does not return, calls os_thread_exit()
*/
extern "C"
os_thread_ret_t
DECLARE_THREAD(buf_resize_thread)(
	void*	arg MY_ATTRIBUTE((unused)))
{
	my_thread_init();

	srv_buf_resize_thread_active = true;

	while (srv_shutdown_state == SRV_SHUTDOWN_NONE) {
		os_event_wait(srv_buf_resize_event);
		os_event_reset(srv_buf_resize_event);

		if (srv_shutdown_state != SRV_SHUTDOWN_NONE) {
			break;
		}

		buf_pool_mutex_enter_all();
		if (srv_buf_pool_old_size == srv_buf_pool_size) {
			buf_pool_mutex_exit_all();
			std::ostringstream sout;
			sout << "Size did not change (old size = new size = "
				<< srv_buf_pool_size << ". Nothing to do.";
			buf_resize_status(sout.str().c_str());

			/* nothing to do */
			continue;
		}
		buf_pool_mutex_exit_all();

		buf_pool_resize();
	}

	srv_buf_resize_thread_active = false;

	my_thread_end();
	os_thread_exit();

	OS_THREAD_DUMMY_RETURN;
}

/********************************************************************//**
Clears the adaptive hash index on all pages in the buffer pool. */
void
buf_pool_clear_hash_index(void)
/*===========================*/
{
	ulint	p;

	ut_ad(btr_search_own_all(RW_LOCK_X));
	ut_ad(!buf_pool_resizing);
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
				assert_block_ahi_valid(block);

				/* We can set block->index = NULL
				and block->n_pointers = 0
				when btr_search_own_all(RW_LOCK_X);
				see the comments in buf0buf.h */

				if (!index) {
					continue;
				}

				ut_ad(buf_block_get_state(block)
                                      == BUF_BLOCK_FILE_PAGE);
# if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
				block->n_pointers = 0;
# endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
				block->index = NULL;
			}
		}
	}
}

/********************************************************************//**
Relocate a buffer control block.  Relocates the block on the LRU list
and in buf_pool->page_hash.  Does not relocate bpage->list.
The caller must take care of relocating bpage->list. */
static
void
buf_relocate(
/*=========*/
	buf_page_t*	bpage,	/*!< in/out: control block being relocated;
				buf_page_get_state(bpage) must be
				BUF_BLOCK_ZIP_DIRTY or BUF_BLOCK_ZIP_PAGE */
	buf_page_t*	dpage)	/*!< in/out: destination control block */
{
	buf_page_t*	b;
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);

	ut_ad(buf_pool_mutex_own(buf_pool));
	ut_ad(buf_page_hash_lock_held_x(buf_pool, bpage));
	ut_ad(mutex_own(buf_page_get_mutex(bpage)));
	ut_a(buf_page_get_io_fix(bpage) == BUF_IO_NONE);
	ut_a(bpage->buf_fix_count == 0);
	ut_ad(bpage->in_LRU_list);
	ut_ad(!bpage->in_zip_hash);
	ut_ad(bpage->in_page_hash);
	ut_ad(bpage == buf_page_hash_get_low(buf_pool, bpage->id));

	ut_ad(!buf_pool_watch_is_sentinel(buf_pool, bpage));
#ifdef UNIV_DEBUG
	switch (buf_page_get_state(bpage)) {
	case BUF_BLOCK_POOL_WATCH:
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

	/* Important that we adjust the hazard pointer before
	removing bpage from LRU list. */
	buf_LRU_adjust_hp(buf_pool, bpage);

	ut_d(bpage->in_LRU_list = FALSE);
	ut_d(bpage->in_page_hash = FALSE);

	/* relocate buf_pool->LRU */
	b = UT_LIST_GET_PREV(LRU, bpage);
	UT_LIST_REMOVE(buf_pool->LRU, bpage);

	if (b != NULL) {
		UT_LIST_INSERT_AFTER(buf_pool->LRU, b, dpage);
	} else {
		UT_LIST_ADD_FIRST(buf_pool->LRU, dpage);
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

        ut_d(CheckInLRUList::validate(buf_pool));

	/* relocate buf_pool->page_hash */
	ulint	fold = bpage->id.fold();
	ut_ad(fold == dpage->id.fold());
	HASH_DELETE(buf_page_t, hash, buf_pool->page_hash, fold, bpage);
	HASH_INSERT(buf_page_t, hash, buf_pool->page_hash, fold, dpage);
}

/** Hazard Pointer implementation. */

/** Set current value
@param bpage	buffer block to be set as hp */
void
HazardPointer::set(buf_page_t* bpage)
{
	ut_ad(mutex_own(m_mutex));
	ut_ad(!bpage || buf_pool_from_bpage(bpage) == m_buf_pool);
	ut_ad(!bpage || buf_page_in_file(bpage));

	m_hp = bpage;
}

/** Checks if a bpage is the hp
@param bpage    buffer block to be compared
@return true if it is hp */

bool
HazardPointer::is_hp(const buf_page_t* bpage)
{
	ut_ad(mutex_own(m_mutex));
	ut_ad(!m_hp || buf_pool_from_bpage(m_hp) == m_buf_pool);
	ut_ad(!bpage || buf_pool_from_bpage(bpage) == m_buf_pool);

	return(bpage == m_hp);
}

/** Adjust the value of hp. This happens when some other thread working
on the same list attempts to remove the hp from the list.
@param bpage	buffer block to be compared */

void
FlushHp::adjust(const buf_page_t* bpage)
{
	ut_ad(bpage != NULL);

	/** We only support reverse traversal for now. */
	if (is_hp(bpage)) {
		m_hp = UT_LIST_GET_PREV(list, m_hp);
	}

	ut_ad(!m_hp || m_hp->in_flush_list);
}

/** Adjust the value of hp. This happens when some other thread working
on the same list attempts to remove the hp from the list.
@param bpage	buffer block to be compared */

void
LRUHp::adjust(const buf_page_t* bpage)
{
	ut_ad(bpage);

	/** We only support reverse traversal for now. */
	if (is_hp(bpage)) {
		m_hp = UT_LIST_GET_PREV(LRU, m_hp);
	}

	ut_ad(!m_hp || m_hp->in_LRU_list);
}

/** Selects from where to start a scan. If we have scanned too deep into
the LRU list it resets the value to the tail of the LRU list.
@return buf_page_t from where to start scan. */

buf_page_t*
LRUItr::start()
{
	ut_ad(mutex_own(m_mutex));

	if (!m_hp || m_hp->old) {
		m_hp = UT_LIST_GET_LAST(m_buf_pool->LRU);
	}

	return(m_hp);
}

/** Determine if a block is a sentinel for a buffer pool watch.
@param[in]	buf_pool	buffer pool instance
@param[in]	bpage		block
@return TRUE if a sentinel for a buffer pool watch, FALSE if not */
ibool
buf_pool_watch_is_sentinel(
	const buf_pool_t*	buf_pool,
	const buf_page_t*	bpage)
{
	/* We must also own the appropriate hash lock. */
	ut_ad(buf_page_hash_lock_held_s_or_x(buf_pool, bpage));
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
	return(TRUE);
}

/** Add watch for the given page to be read in. Caller must have
appropriate hash_lock for the bpage. This function may release the
hash_lock and reacquire it.
@param[in]	page_id		page id
@param[in,out]	hash_lock	hash_lock currently latched
@return NULL if watch set, block if the page is in the buffer pool */
buf_page_t*
buf_pool_watch_set(
	const page_id_t&	page_id,
	rw_lock_t**		hash_lock)
{
	buf_page_t*	bpage;
	ulint		i;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);

	ut_ad(*hash_lock == buf_page_hash_lock_get(buf_pool, page_id));

	ut_ad(rw_lock_own(*hash_lock, RW_LOCK_X));

	bpage = buf_page_hash_get_low(buf_pool, page_id);

	if (bpage != NULL) {
page_found:
		if (!buf_pool_watch_is_sentinel(buf_pool, bpage)) {
			/* The page was loaded meanwhile. */
			return(bpage);
		}

		/* Add to an existing watch. */
		buf_block_fix(bpage);
		return(NULL);
	}

	/* From this point this function becomes fairly heavy in terms
	of latching. We acquire the buf_pool mutex as well as all the
	hash_locks. buf_pool mutex is needed because any changes to
	the page_hash must be covered by it and hash_locks are needed
	because we don't want to read any stale information in
	buf_pool->watch[]. However, it is not in the critical code path
	as this function will be called only by the purge thread. */


	/* To obey latching order first release the hash_lock. */
	rw_lock_x_unlock(*hash_lock);

	buf_pool_mutex_enter(buf_pool);
	hash_lock_x_all(buf_pool->page_hash);

	/* If not own buf_pool_mutex, page_hash can be changed. */
	*hash_lock = buf_page_hash_lock_get(buf_pool, page_id);

	/* We have to recheck that the page
	was not loaded or a watch set by some other
	purge thread. This is because of the small
	time window between when we release the
	hash_lock to acquire buf_pool mutex above. */

	bpage = buf_page_hash_get_low(buf_pool, page_id);
	if (UNIV_LIKELY_NULL(bpage)) {
		buf_pool_mutex_exit(buf_pool);
		hash_unlock_x_all_but(buf_pool->page_hash, *hash_lock);
		goto page_found;
	}

	/* The maximum number of purge threads should never exceed
	BUF_POOL_WATCH_SIZE. So there is no way for purge thread
	instance to hold a watch when setting another watch. */
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
			bpage->id.copy_from(page_id);
			bpage->buf_fix_count = 1;

			ut_d(bpage->in_page_hash = TRUE);
			HASH_INSERT(buf_page_t, hash, buf_pool->page_hash,
				    page_id.fold(), bpage);

			buf_pool_mutex_exit(buf_pool);
			/* Once the sentinel is in the page_hash we can
			safely release all locks except just the
			relevant hash_lock */
			hash_unlock_x_all_but(buf_pool->page_hash,
						*hash_lock);

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
	return(NULL);
}

/** Remove the sentinel block for the watch before replacing it with a
real block. buf_page_watch_clear() or buf_page_watch_occurred() will notice
that the block has been replaced with the real block.
@param[in,out]	buf_pool	buffer pool instance
@param[in,out]	watch		sentinel for watch
@return reference count, to be added to the replacement block */
static
void
buf_pool_watch_remove(
	buf_pool_t*	buf_pool,
	buf_page_t*	watch)
{
#ifdef UNIV_DEBUG
	/* We must also own the appropriate hash_bucket mutex. */
	rw_lock_t* hash_lock = buf_page_hash_lock_get(buf_pool, watch->id);
	ut_ad(rw_lock_own(hash_lock, RW_LOCK_X));
#endif /* UNIV_DEBUG */

	ut_ad(buf_pool_mutex_own(buf_pool));

	HASH_DELETE(buf_page_t, hash, buf_pool->page_hash, watch->id.fold(),
		    watch);
	ut_d(watch->in_page_hash = FALSE);
	watch->buf_fix_count = 0;
	watch->state = BUF_BLOCK_POOL_WATCH;
}

/** Stop watching if the page has been read in.
buf_pool_watch_set(same_page_id) must have returned NULL before.
@param[in]	page_id	page id */
void
buf_pool_watch_unset(
	const page_id_t&	page_id)
{
	buf_page_t*	bpage;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);

	/* We only need to have buf_pool mutex in case where we end
	up calling buf_pool_watch_remove but to obey latching order
	we acquire it here before acquiring hash_lock. This should
	not cause too much grief as this function is only ever
	called from the purge thread. */
	buf_pool_mutex_enter(buf_pool);

	rw_lock_t*	hash_lock = buf_page_hash_lock_get(buf_pool, page_id);
	rw_lock_x_lock(hash_lock);

	/* The page must exist because buf_pool_watch_set()
	increments buf_fix_count. */
	bpage = buf_page_hash_get_low(buf_pool, page_id);

	if (buf_block_unfix(bpage) == 0
	    && buf_pool_watch_is_sentinel(buf_pool, bpage)) {
		buf_pool_watch_remove(buf_pool, bpage);
	}

	buf_pool_mutex_exit(buf_pool);
	rw_lock_x_unlock(hash_lock);
}

/** Check if the page has been read in.
This may only be called after buf_pool_watch_set(same_page_id)
has returned NULL and before invoking buf_pool_watch_unset(same_page_id).
@param[in]	page_id	page id
@return FALSE if the given page was not read in, TRUE if it was */
ibool
buf_pool_watch_occurred(
	const page_id_t&	page_id)
{
	ibool		ret;
	buf_page_t*	bpage;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);
	rw_lock_t*	hash_lock = buf_page_hash_lock_get(buf_pool, page_id);

	rw_lock_s_lock(hash_lock);

	/* If not own buf_pool_mutex, page_hash can be changed. */
	hash_lock = buf_page_hash_lock_s_confirm(hash_lock, buf_pool, page_id);

	/* The page must exist because buf_pool_watch_set()
	increments buf_fix_count. */
	bpage = buf_page_hash_get_low(buf_pool, page_id);

	ret = !buf_pool_watch_is_sentinel(buf_pool, bpage);
	rw_lock_s_unlock(hash_lock);

	return(ret);
}

/********************************************************************//**
Moves a page to the start of the buffer pool LRU list. This high-level
function can be used to prevent an important page from slipping out of
the buffer pool. */
void
buf_page_make_young(
/*================*/
	buf_page_t*	bpage)	/*!< in: buffer block of a file page */
{
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);

	buf_pool_mutex_enter(buf_pool);

	ut_a(buf_page_in_file(bpage));

	buf_LRU_make_block_young(bpage);

	buf_pool_mutex_exit(buf_pool);
}

/********************************************************************//**
Moves a page to the start of the buffer pool LRU list if it is too old.
This high-level function can be used to prevent an important page from
slipping out of the buffer pool. */
static
void
buf_page_make_young_if_needed(
/*==========================*/
	buf_page_t*	bpage)		/*!< in/out: buffer block of a
					file page */
{
#ifdef UNIV_DEBUG
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
	ut_ad(!buf_pool_mutex_own(buf_pool));
#endif /* UNIV_DEBUG */
	ut_a(buf_page_in_file(bpage));

	if (buf_page_peek_if_too_old(bpage)) {
		buf_page_make_young(bpage);
	}
}

#ifdef UNIV_DEBUG

/** Sets file_page_was_freed TRUE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated.
@param[in]	page_id	page id
@return control block if found in page hash table, otherwise NULL */
buf_page_t*
buf_page_set_file_page_was_freed(
	const page_id_t&	page_id)
{
	buf_page_t*	bpage;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);
	rw_lock_t*	hash_lock;

	bpage = buf_page_hash_get_s_locked(buf_pool, page_id, &hash_lock);

	if (bpage) {
		BPageMutex*	block_mutex = buf_page_get_mutex(bpage);
		ut_ad(!buf_pool_watch_is_sentinel(buf_pool, bpage));
		mutex_enter(block_mutex);
		rw_lock_s_unlock(hash_lock);
		/* bpage->file_page_was_freed can already hold
		when this code is invoked from dict_drop_index_tree() */
		bpage->file_page_was_freed = TRUE;
		mutex_exit(block_mutex);
	}

	return(bpage);
}

/** Sets file_page_was_freed FALSE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated.
@param[in]	page_id	page id
@return control block if found in page hash table, otherwise NULL */
buf_page_t*
buf_page_reset_file_page_was_freed(
	const page_id_t&	page_id)
{
	buf_page_t*	bpage;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);
	rw_lock_t*	hash_lock;

	bpage = buf_page_hash_get_s_locked(buf_pool, page_id, &hash_lock);
	if (bpage) {
		BPageMutex*	block_mutex = buf_page_get_mutex(bpage);
		ut_ad(!buf_pool_watch_is_sentinel(buf_pool, bpage));
		mutex_enter(block_mutex);
		rw_lock_s_unlock(hash_lock);
		bpage->file_page_was_freed = FALSE;
		mutex_exit(block_mutex);
	}

	return(bpage);
}
#endif /* UNIV_DEBUG */

/** Attempts to discard the uncompressed frame of a compressed page.
The caller should not be holding any mutexes when this function is called.
@param[in]	page_id	page id
@return TRUE if successful, FALSE otherwise. */
static
void
buf_block_try_discard_uncompressed(
	const page_id_t&	page_id)
{
	buf_page_t*	bpage;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);

	/* Since we need to acquire buf_pool mutex to discard
	the uncompressed frame and because page_hash mutex resides
	below buf_pool mutex in sync ordering therefore we must
	first release the page_hash mutex. This means that the
	block in question can move out of page_hash. Therefore
	we need to check again if the block is still in page_hash. */
	buf_pool_mutex_enter(buf_pool);

	bpage = buf_page_hash_get(buf_pool, page_id);

	if (bpage) {
		buf_LRU_free_page(bpage, false);
	}

	buf_pool_mutex_exit(buf_pool);
}

/** Get read access to a compressed page (usually of type
FIL_PAGE_TYPE_ZBLOB or FIL_PAGE_TYPE_ZBLOB2).
The page must be released with buf_page_release_zip().
NOTE: the page is not protected by any latch.  Mutual exclusion has to
be implemented at a higher level.  In other words, all possible
accesses to a given page through this function must be protected by
the same set of mutexes or latches.
@param[in]	page_id		page id
@param[in]	page_size	page size
@return pointer to the block */
buf_page_t*
buf_page_get_zip(
	const page_id_t&	page_id,
	const page_size_t&	page_size)
{
	buf_page_t*	bpage;
	BPageMutex*	block_mutex;
	rw_lock_t*	hash_lock;
	ibool		discard_attempted = FALSE;
	ibool		must_read;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);

	buf_pool->stat.n_page_gets++;

	for (;;) {
lookup:

		/* The following call will also grab the page_hash
		mutex if the page is found. */
		bpage = buf_page_hash_get_s_locked(buf_pool, page_id,
						   &hash_lock);
		if (bpage) {
			ut_ad(!buf_pool_watch_is_sentinel(buf_pool, bpage));
			break;
		}

		/* Page not in buf_pool: needs to be read from file */

		ut_ad(!hash_lock);
		buf_read_page(page_id, page_size);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		ut_a(++buf_dbg_counter % 5771 || buf_validate());
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
	}

	ut_ad(buf_page_hash_lock_held_s(buf_pool, bpage));

	if (!bpage->zip.data) {
		/* There is no compressed page. */
err_exit:
		rw_lock_s_unlock(hash_lock);
		return(NULL);
	}

	ut_ad(!buf_pool_watch_is_sentinel(buf_pool, bpage));

	switch (buf_page_get_state(bpage)) {
	case BUF_BLOCK_POOL_WATCH:
	case BUF_BLOCK_NOT_USED:
	case BUF_BLOCK_READY_FOR_USE:
	case BUF_BLOCK_MEMORY:
	case BUF_BLOCK_REMOVE_HASH:
		ut_error;

	case BUF_BLOCK_ZIP_PAGE:
	case BUF_BLOCK_ZIP_DIRTY:
		buf_block_fix(bpage);
		block_mutex = &buf_pool->zip_mutex;
		mutex_enter(block_mutex);
		goto got_block;
	case BUF_BLOCK_FILE_PAGE:
		/* Discard the uncompressed page frame if possible. */
		if (!discard_attempted) {
			rw_lock_s_unlock(hash_lock);
			buf_block_try_discard_uncompressed(page_id);
			discard_attempted = TRUE;
			goto lookup;
		}

		buf_block_buf_fix_inc((buf_block_t*) bpage,
				      __FILE__, __LINE__);

		block_mutex = &((buf_block_t*) bpage)->mutex;

		mutex_enter(block_mutex);

		goto got_block;
	}

	ut_error;
	goto err_exit;

got_block:
	must_read = buf_page_get_io_fix(bpage) == BUF_IO_READ;

	rw_lock_s_unlock(hash_lock);

	ut_ad(!bpage->file_page_was_freed);

	buf_page_set_accessed(bpage);

	mutex_exit(block_mutex);

	buf_page_make_young_if_needed(bpage);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(++buf_dbg_counter % 5771 || buf_validate());
	ut_a(bpage->buf_fix_count > 0);
	ut_a(buf_page_in_file(bpage));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	if (must_read) {
		/* Let us wait until the read operation
		completes */

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
	}

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(page_id) == 0);
#endif /* UNIV_IBUF_COUNT_DEBUG */

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
	/* No adaptive hash index entries may point to a previously
	unused (and now freshly allocated) block. */
	assert_block_ahi_empty_on_init(block);
	block->index		= NULL;
	block->made_dirty_with_no_latch = false;
	block->skip_flush_check = false;

	block->n_hash_helps	= 0;
	block->n_fields		= 1;
	block->n_bytes		= 0;
	block->left_side	= TRUE;
}
#endif /* !UNIV_HOTBACKUP */

/********************************************************************//**
Decompress a block.
@return TRUE if successful */
ibool
buf_zip_decompress(
/*===============*/
	buf_block_t*	block,	/*!< in/out: block */
	ibool		check)	/*!< in: TRUE=verify the page checksum */
{
	const byte*	frame = block->page.zip.data;
	ulint		size = page_zip_get_size(&block->page.zip);

	ut_ad(block->page.size.is_compressed());
	ut_a(block->page.id.space() != 0);

	if (UNIV_UNLIKELY(check && !page_zip_verify_checksum(frame, size))) {

		ib::error() << "Compressed page checksum mismatch "
			<< block->page.id << "): stored: "
			<< mach_read_from_4(frame + FIL_PAGE_SPACE_OR_CHKSUM)
			<< ", crc32: "
			<< page_zip_calc_checksum(
				frame, size, SRV_CHECKSUM_ALGORITHM_CRC32)
			<< "/"
			<< page_zip_calc_checksum(
				frame, size, SRV_CHECKSUM_ALGORITHM_CRC32,
				true)
			<< " innodb: "
			<< page_zip_calc_checksum(
				frame, size, SRV_CHECKSUM_ALGORITHM_INNODB)
			<< ", none: "
			<< page_zip_calc_checksum(
				frame, size, SRV_CHECKSUM_ALGORITHM_NONE);

		return(FALSE);
	}

	switch (fil_page_get_type(frame)) {
	case FIL_PAGE_INDEX:
	case FIL_PAGE_RTREE:
		if (page_zip_decompress(&block->page.zip,
					block->frame, TRUE)) {
			return(TRUE);
		}

		ib::error() << "Unable to decompress space "
			<< block->page.id.space()
			<< " page " << block->page.id.page_no();

		return(FALSE);

	case FIL_PAGE_TYPE_ALLOCATED:
	case FIL_PAGE_INODE:
	case FIL_PAGE_IBUF_BITMAP:
	case FIL_PAGE_TYPE_FSP_HDR:
	case FIL_PAGE_TYPE_XDES:
	case FIL_PAGE_TYPE_ZBLOB:
	case FIL_PAGE_TYPE_ZBLOB2:
		/* Copy to uncompressed storage. */
		memcpy(block->frame, frame, block->page.size.physical());
		return(TRUE);
	}

	ib::error() << "Unknown compressed page type "
		<< fil_page_get_type(frame);

	return(FALSE);
}

#ifndef UNIV_HOTBACKUP
/** Get a buffer block from an adaptive hash index pointer.
This function does not return if the block is not identified.
@param[in]	ptr	pointer to within a page frame
@return pointer to block, never NULL */
buf_block_t*
buf_block_from_ahi(const byte* ptr)
{
	buf_pool_chunk_map_t::iterator it;

	buf_pool_chunk_map_t*	chunk_map = buf_chunk_map_ref;
	ut_ad(buf_chunk_map_ref == buf_chunk_map_reg);
	ut_ad(!buf_pool_resizing);

	buf_chunk_t*	chunk;
	it = chunk_map->upper_bound(ptr);

	ut_a(it != chunk_map->begin());

	if (it == chunk_map->end()) {
		chunk = chunk_map->rbegin()->second;
	} else {
		chunk = (--it)->second;
	}

	ulint		offs = ptr - chunk->blocks->frame;

	offs >>= UNIV_PAGE_SIZE_SHIFT;

	ut_a(offs < chunk->size);

	buf_block_t*	block = &chunk->blocks[offs];

	/* The function buf_chunk_init() invokes buf_block_init() so that
	block[n].frame == block->frame + n * UNIV_PAGE_SIZE.  Check it. */
	ut_ad(block->frame == page_align(ptr));
	/* Read the state of the block without holding a mutex.
	A state transition from BUF_BLOCK_FILE_PAGE to
	BUF_BLOCK_REMOVE_HASH is possible during this execution. */
	ut_d(const buf_page_state state = buf_block_get_state(block));
	ut_ad(state == BUF_BLOCK_FILE_PAGE || state == BUF_BLOCK_REMOVE_HASH);
	return(block);
}

/********************************************************************//**
Find out if a pointer belongs to a buf_block_t. It can be a pointer to
the buf_block_t itself or a member of it. This functions checks one of
the buffer pool instances.
@return TRUE if ptr belongs to a buf_block_t struct */
static
ibool
buf_pointer_is_block_field_instance(
/*================================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	const void*	ptr)		/*!< in: pointer not dereferenced */
{
	const buf_chunk_t*		chunk	= buf_pool->chunks;
	const buf_chunk_t* const	echunk	= chunk + ut_min(
		buf_pool->n_chunks, buf_pool->n_chunks_new);

	/* TODO: protect buf_pool->chunks with a mutex (the older pointer will
	currently remain while during buf_pool_resize()) */
	while (chunk < echunk) {
		if (ptr >= (void*) chunk->blocks
		    && ptr < (void*) (chunk->blocks + chunk->size)) {

			return(TRUE);
		}

		chunk++;
	}

	return(FALSE);
}

/********************************************************************//**
Find out if a pointer belongs to a buf_block_t. It can be a pointer to
the buf_block_t itself or a member of it
@return TRUE if ptr belongs to a buf_block_t struct */
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
@return TRUE if "block" has been added to buf_pool->free by buf_chunk_init() */
static
ibool
buf_block_is_uncompressed(
/*======================*/
	buf_pool_t*		buf_pool,	/*!< in: buffer pool instance */
	const buf_block_t*	block)		/*!< in: pointer to block,
						not dereferenced */
{
	if ((((ulint) block) % sizeof *block) != 0) {
		/* The pointer should be aligned. */
		return(FALSE);
	}

	return(buf_pointer_is_block_field_instance(buf_pool, (void*) block));
}

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG
/********************************************************************//**
Return true if probe is enabled.
@return true if probe enabled. */
static
bool
buf_debug_execute_is_force_flush()
/*==============================*/
{
	DBUG_EXECUTE_IF("ib_buf_force_flush", return(true); );

	/* This is used during queisce testing, we want to ensure maximum
	buffering by the change buffer. */

	if (srv_ibuf_disable_background_merge) {
		return(true);
	}

	return(false);
}
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

/** Wait for the block to be read in.
@param[in]	block	The block to check */
static
void
buf_wait_for_read(
	buf_block_t*	block)
{
	/* Note:

	We are using the block->lock to check for IO state (and a dirty read).
	We set the IO_READ state under the protection of the hash_lock
	(and block->mutex). This is safe because another thread can only
	access the block (and check for IO state) after the block has been
	added to the page hashtable. */

	if (buf_block_get_io_fix(block) == BUF_IO_READ) {

		/* Wait until the read operation completes */

		BPageMutex*	mutex = buf_page_get_mutex(&block->page);

		for (;;) {
			buf_io_fix	io_fix;

			mutex_enter(mutex);

			io_fix = buf_block_get_io_fix(block);

			mutex_exit(mutex);

			if (io_fix == BUF_IO_READ) {
				/* Wait by temporaly s-latch */
				rw_lock_s_lock(&block->lock);
				rw_lock_s_unlock(&block->lock);
			} else {
				break;
			}
		}
	}
}

/** This is the general function used to get access to a database page.
@param[in]	page_id		page id
@param[in]	rw_latch	RW_S_LATCH, RW_X_LATCH, RW_NO_LATCH
@param[in]	guess		guessed block or NULL
@param[in]	mode		BUF_GET, BUF_GET_IF_IN_POOL,
BUF_PEEK_IF_IN_POOL, BUF_GET_NO_LATCH, or BUF_GET_IF_IN_POOL_OR_WATCH
@param[in]	file		file name
@param[in]	line		line where called
@param[in]	mtr		mini-transaction
@param[in]	dirty_with_no_latch
				mark page as dirty even if page
				is being pinned without any latch
@return pointer to the block or NULL */
buf_block_t*
buf_page_get_gen(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	ulint			rw_latch,
	buf_block_t*		guess,
	ulint			mode,
	const char*		file,
	ulint			line,
	mtr_t*			mtr,
	bool			dirty_with_no_latch)
{
	buf_block_t*	block;
	unsigned	access_time;
	rw_lock_t*	hash_lock;
	buf_block_t*	fix_block;
	ulint		retries = 0;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);

	ut_ad(mtr->is_active());
	ut_ad((rw_latch == RW_S_LATCH)
	      || (rw_latch == RW_X_LATCH)
	      || (rw_latch == RW_SX_LATCH)
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

	bool			found;
	const page_size_t&	space_page_size
		= fil_space_get_page_size(page_id.space(), &found);

	ut_ad(found);

	ut_ad(page_size.equals_to(space_page_size));
#endif /* UNIV_DEBUG */

	ut_ad(!ibuf_inside(mtr)
	      || ibuf_page_low(page_id, page_size, FALSE, file, line, NULL));

	buf_pool->stat.n_page_gets++;
	hash_lock = buf_page_hash_lock_get(buf_pool, page_id);
loop:
	block = guess;

	rw_lock_s_lock(hash_lock);

	/* If not own buf_pool_mutex, page_hash can be changed. */
	hash_lock = buf_page_hash_lock_s_confirm(hash_lock, buf_pool, page_id);

	if (block != NULL) {

		/* If the guess is a compressed page descriptor that
		has been allocated by buf_page_alloc_descriptor(),
		it may have been freed by buf_relocate(). */

		if (!buf_block_is_uncompressed(buf_pool, block)
		    || !page_id.equals_to(block->page.id)
		    || buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE) {

			/* Our guess was bogus or things have changed
			since. */
			block = guess = NULL;
		} else {
			ut_ad(!block->page.in_zip_hash);
		}
	}

	if (block == NULL) {
		block = (buf_block_t*) buf_page_hash_get_low(buf_pool, page_id);
	}

	if (!block || buf_pool_watch_is_sentinel(buf_pool, &block->page)) {
		rw_lock_s_unlock(hash_lock);
		block = NULL;
	}

	if (block == NULL) {
		/* Page not in buf_pool: needs to be read from file */

		if (mode == BUF_GET_IF_IN_POOL_OR_WATCH) {
			rw_lock_x_lock(hash_lock);

			/* If not own buf_pool_mutex,
			page_hash can be changed. */
			hash_lock = buf_page_hash_lock_x_confirm(
				hash_lock, buf_pool, page_id);

			block = (buf_block_t*) buf_pool_watch_set(
				page_id, &hash_lock);

			if (block) {
				/* We can release hash_lock after we
				increment the fix count to make
				sure that no state change takes place. */
				fix_block = block;

				if (fsp_is_system_temporary(page_id.space())) {
					/* For temporary tablespace,
					the mutex is being used for
					synchronization between user
					thread and flush thread,
					instead of block->lock. See
					buf_flush_page() for the flush
					thread counterpart. */

					BPageMutex*	fix_mutex
						= buf_page_get_mutex(
							&fix_block->page);
					mutex_enter(fix_mutex);
					buf_block_fix(fix_block);
					mutex_exit(fix_mutex);
				} else {
					buf_block_fix(fix_block);
				}

				/* Now safe to release page_hash mutex */
				rw_lock_x_unlock(hash_lock);
				goto got_block;
			}

			rw_lock_x_unlock(hash_lock);
		}

		if (mode == BUF_GET_IF_IN_POOL
		    || mode == BUF_PEEK_IF_IN_POOL
		    || mode == BUF_GET_IF_IN_POOL_OR_WATCH) {

			ut_ad(!rw_lock_own(hash_lock, RW_LOCK_X));
			ut_ad(!rw_lock_own(hash_lock, RW_LOCK_S));

			return(NULL);
		}

		if (buf_read_page(page_id, page_size)) {
			buf_read_ahead_random(page_id, page_size,
					      ibuf_inside(mtr));

			retries = 0;
		} else if (retries < BUF_PAGE_READ_MAX_RETRIES) {
			++retries;
			DBUG_EXECUTE_IF(
				"innodb_page_corruption_retries",
				retries = BUF_PAGE_READ_MAX_RETRIES;
			);
		} else {
			ib::fatal() << "Unable to read page " << page_id
				<< " into the buffer pool after "
				<< BUF_PAGE_READ_MAX_RETRIES << " attempts."
				" The most probable cause of this error may"
				" be that the table has been corrupted. Or,"
				" the table was compressed with with an"
				" algorithm that is not supported by this"
				" instance. If it is not a decompress failure,"
				" you can try to fix this problem by using"
				" innodb_force_recovery."
				" Please see " REFMAN " for more"
				" details. Aborting...";
		}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		ut_a(fsp_skip_sanity_check(page_id.space())
		     || ++buf_dbg_counter % 5771
		     || buf_validate());
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
		goto loop;
	} else {
		fix_block = block;
	}

	if (fsp_is_system_temporary(page_id.space())) {
		/* For temporary tablespace, the mutex is being used
		for synchronization between user thread and flush
		thread, instead of block->lock. See buf_flush_page()
		for the flush thread counterpart. */
		BPageMutex*	fix_mutex = buf_page_get_mutex(
			&fix_block->page);
		mutex_enter(fix_mutex);
		buf_block_fix(fix_block);
		mutex_exit(fix_mutex);
	} else {
		buf_block_fix(fix_block);
	}

	/* Now safe to release page_hash mutex */
	rw_lock_s_unlock(hash_lock);

got_block:

	if (mode == BUF_GET_IF_IN_POOL || mode == BUF_PEEK_IF_IN_POOL) {

		buf_page_t*	fix_page = &fix_block->page;
		BPageMutex*	fix_mutex = buf_page_get_mutex(fix_page);
		mutex_enter(fix_mutex);
		const bool	must_read
			= (buf_page_get_io_fix(fix_page) == BUF_IO_READ);
		mutex_exit(fix_mutex);

		if (must_read) {
			/* The page is being read to buffer pool,
			but we cannot wait around for the read to
			complete. */
			buf_block_unfix(fix_block);

			return(NULL);
		}
	}

	switch (buf_block_get_state(fix_block)) {
		buf_page_t*	bpage;

	case BUF_BLOCK_FILE_PAGE:
		bpage = &block->page;
		if (fsp_is_system_temporary(page_id.space())
		    && buf_page_get_io_fix(bpage) != BUF_IO_NONE) {
			/* This suggest that page is being flushed.
			Avoid returning reference to this page.
			Instead wait for flush action to complete.
			For normal page this sync is done using SX
			lock but for intrinsic there is no latching. */
			buf_block_unfix(fix_block);
			os_thread_sleep(WAIT_FOR_WRITE);
			goto loop;
		}
		break;

	case BUF_BLOCK_ZIP_PAGE:
	case BUF_BLOCK_ZIP_DIRTY:
		if (mode == BUF_PEEK_IF_IN_POOL) {
			/* This mode is only used for dropping an
			adaptive hash index.  There cannot be an
			adaptive hash index for a compressed-only
			page, so do not bother decompressing the page. */
			buf_block_unfix(fix_block);

			return(NULL);
		}

		bpage = &block->page;

		/* Note: We have already buffer fixed this block. */
		if (bpage->buf_fix_count > 1
		    || buf_page_get_io_fix(bpage) != BUF_IO_NONE) {

			/* This condition often occurs when the buffer
			is not buffer-fixed, but I/O-fixed by
			buf_page_init_for_read(). */
			buf_block_unfix(fix_block);

			/* The block is buffer-fixed or I/O-fixed.
			Try again later. */
			os_thread_sleep(WAIT_FOR_READ);

			goto loop;
		}

		/* Buffer-fix the block so that it cannot be evicted
		or relocated while we are attempting to allocate an
		uncompressed page. */

		block = buf_LRU_get_free_block(buf_pool);

		buf_pool_mutex_enter(buf_pool);

		/* If not own buf_pool_mutex, page_hash can be changed. */
		hash_lock = buf_page_hash_lock_get(buf_pool, page_id);

		rw_lock_x_lock(hash_lock);

		/* Buffer-fixing prevents the page_hash from changing. */
		ut_ad(bpage == buf_page_hash_get_low(buf_pool, page_id));

		buf_block_unfix(fix_block);

		buf_page_mutex_enter(block);
		mutex_enter(&buf_pool->zip_mutex);

		fix_block = block;

		if (bpage->buf_fix_count > 0
		    || buf_page_get_io_fix(bpage) != BUF_IO_NONE) {

			mutex_exit(&buf_pool->zip_mutex);
			/* The block was buffer-fixed or I/O-fixed while
			buf_pool->mutex was not held by this thread.
			Free the block that was allocated and retry.
			This should be extremely unlikely, for example,
			if buf_page_get_zip() was invoked. */

			buf_LRU_block_free_non_file_page(block);
			buf_pool_mutex_exit(buf_pool);
			rw_lock_x_unlock(hash_lock);
			buf_page_mutex_exit(block);

			/* Try again */
			goto loop;
		}

		/* Move the compressed page from bpage to block,
		and uncompress it. */

		/* Note: this is the uncompressed block and it is not
		accessible by other threads yet because it is not in
		any list or hash table */
		buf_relocate(bpage, &block->page);

		buf_block_init_low(block);

		/* Set after buf_relocate(). */
		block->page.buf_fix_count = 1;

		block->lock_hash_val = lock_rec_hash(page_id.space(),
						     page_id.page_no());

		UNIV_MEM_DESC(&block->page.zip.data,
			      page_zip_get_size(&block->page.zip));

		if (buf_page_get_state(&block->page) == BUF_BLOCK_ZIP_PAGE) {
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
			UT_LIST_REMOVE(buf_pool->zip_clean, &block->page);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
			ut_ad(!block->page.in_flush_list);
		} else {
			/* Relocate buf_pool->flush_list. */
			buf_flush_relocate_on_flush_list(bpage, &block->page);
		}

		/* Buffer-fix, I/O-fix, and X-latch the block
		for the duration of the decompression.
		Also add the block to the unzip_LRU list. */
		block->page.state = BUF_BLOCK_FILE_PAGE;

		/* Insert at the front of unzip_LRU list */
		buf_unzip_LRU_add_block(block, FALSE);

		buf_block_set_io_fix(block, BUF_IO_READ);
		rw_lock_x_lock_inline(&block->lock, 0, file, line);

		UNIV_MEM_INVALID(bpage, sizeof *bpage);

		rw_lock_x_unlock(hash_lock);
		buf_pool->n_pend_unzip++;
		mutex_exit(&buf_pool->zip_mutex);
		buf_pool_mutex_exit(buf_pool);

		access_time = buf_page_is_accessed(&block->page);

		buf_page_mutex_exit(block);

		buf_page_free_descriptor(bpage);

		/* Decompress the page while not holding
		buf_pool->mutex or block->mutex. */

		/* Page checksum verification is already done when
		the page is read from disk. Hence page checksum
		verification is not necessary when decompressing the page. */
		{
			bool	success = buf_zip_decompress(block, FALSE);
			ut_a(success);
		}

		if (!recv_no_ibuf_operations) {
			if (access_time) {
#ifdef UNIV_IBUF_COUNT_DEBUG
				ut_a(ibuf_count_get(page_id) == 0);
#endif /* UNIV_IBUF_COUNT_DEBUG */
			} else {
				ibuf_merge_or_delete_for_page(
					block, page_id, &page_size, TRUE);
			}
		}

		buf_pool_mutex_enter(buf_pool);

		buf_page_mutex_enter(fix_block);

		buf_block_set_io_fix(fix_block, BUF_IO_NONE);

		buf_page_mutex_exit(fix_block);

		--buf_pool->n_pend_unzip;

		buf_pool_mutex_exit(buf_pool);

		rw_lock_x_unlock(&block->lock);

		break;

	case BUF_BLOCK_POOL_WATCH:
	case BUF_BLOCK_NOT_USED:
	case BUF_BLOCK_READY_FOR_USE:
	case BUF_BLOCK_MEMORY:
	case BUF_BLOCK_REMOVE_HASH:
		ut_error;
		break;
	}

	ut_ad(block == fix_block);
	ut_ad(fix_block->page.buf_fix_count > 0);

	ut_ad(!rw_lock_own(hash_lock, RW_LOCK_X));
	ut_ad(!rw_lock_own(hash_lock, RW_LOCK_S));

	ut_ad(buf_block_get_state(fix_block) == BUF_BLOCK_FILE_PAGE);

#if defined UNIV_DEBUG || defined UNIV_IBUF_DEBUG

	if ((mode == BUF_GET_IF_IN_POOL || mode == BUF_GET_IF_IN_POOL_OR_WATCH)
	    && (ibuf_debug || buf_debug_execute_is_force_flush())) {

		/* Try to evict the block from the buffer pool, to use the
		insert buffer (change buffer) as much as possible. */

		buf_pool_mutex_enter(buf_pool);

		buf_block_unfix(fix_block);

		/* Now we are only holding the buf_pool->mutex,
		not block->mutex or hash_lock. Blocks cannot be
		relocated or enter or exit the buf_pool while we
		are holding the buf_pool->mutex. */

		if (buf_LRU_free_page(&fix_block->page, true)) {

			buf_pool_mutex_exit(buf_pool);

			/* If not own buf_pool_mutex,
			page_hash can be changed. */
			hash_lock = buf_page_hash_lock_get(buf_pool, page_id);

			rw_lock_x_lock(hash_lock);

			/* If not own buf_pool_mutex,
			page_hash can be changed. */
			hash_lock = buf_page_hash_lock_x_confirm(
				hash_lock, buf_pool, page_id);

			if (mode == BUF_GET_IF_IN_POOL_OR_WATCH) {
				/* Set the watch, as it would have
				been set if the page were not in the
				buffer pool in the first place. */
				block = (buf_block_t*) buf_pool_watch_set(
					page_id, &hash_lock);
			} else {
				block = (buf_block_t*) buf_page_hash_get_low(
					buf_pool, page_id);
			}

			rw_lock_x_unlock(hash_lock);

			if (block != NULL) {
				/* Either the page has been read in or
				a watch was set on that in the window
				where we released the buf_pool::mutex
				and before we acquire the hash_lock
				above. Try again. */
				guess = block;

				goto loop;
			}

			ib::info() << "innodb_change_buffering_debug evict "
				<< page_id;

			return(NULL);
		}

		buf_page_mutex_enter(fix_block);

		if (buf_flush_page_try(buf_pool, fix_block)) {

			ib::info() << "innodb_change_buffering_debug flush "
				<< page_id;

			guess = fix_block;

			goto loop;
		}

		buf_page_mutex_exit(fix_block);

		buf_block_fix(fix_block);

		/* Failed to evict the page; change it directly */

		buf_pool_mutex_exit(buf_pool);
	}
#endif /* UNIV_DEBUG || UNIV_IBUF_DEBUG */

	ut_ad(fix_block->page.buf_fix_count > 0);

#ifdef UNIV_DEBUG
	/* We have already buffer fixed the page, and we are committed to
	returning this page to the caller. Register for debugging.
	Avoid debug latching if page/block belongs to system temporary
	tablespace (Not much needed for table with single threaded access.). */
	if (!fsp_is_system_temporary(page_id.space())) {
		ibool   ret;
		ret = rw_lock_s_lock_nowait(
			&fix_block->debug_latch, file, line);
		ut_a(ret);
	}
#endif /* UNIV_DEBUG */

	/* While tablespace is reinited the indexes are already freed but the
	blocks related to it still resides in buffer pool. Trying to remove
	such blocks from buffer pool would invoke removal of AHI entries
	associated with these blocks. Logic to remove AHI entry will try to
	load the block but block is already in free state. Handle the said case
	with mode = BUF_PEEK_IF_IN_POOL that is invoked from
	"btr_search_drop_page_hash_when_freed". */
	ut_ad(mode == BUF_GET_POSSIBLY_FREED
	      || mode == BUF_PEEK_IF_IN_POOL
	      || !fix_block->page.file_page_was_freed);

	/* Check if this is the first access to the page */
	access_time = buf_page_is_accessed(&fix_block->page);

	/* This is a heuristic and we don't care about ordering issues. */
	if (access_time == 0) {
		buf_page_mutex_enter(fix_block);

		buf_page_set_accessed(&fix_block->page);

		buf_page_mutex_exit(fix_block);
	}

	if (mode != BUF_PEEK_IF_IN_POOL) {
		buf_page_make_young_if_needed(&fix_block->page);
	}

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(fsp_skip_sanity_check(page_id.space())
	     || ++buf_dbg_counter % 5771
	     || buf_validate());
	ut_a(buf_block_get_state(fix_block) == BUF_BLOCK_FILE_PAGE);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	/* We have to wait here because the IO_READ state was set
	under the protection of the hash_lock and not the block->mutex
	and block->lock. */
	buf_wait_for_read(fix_block);

	/* Mark block as dirty if requested by caller. If not requested (false)
	then we avoid updating the dirty state of the block and retain the
	original one. This is reason why ?
	Same block can be shared/pinned by 2 different mtrs. If first mtr
	set the dirty state to true and second mtr mark it as false the last
	updated dirty state is retained. Which means we can loose flushing of
	a modified block. */
	if (dirty_with_no_latch) {
		fix_block->made_dirty_with_no_latch = dirty_with_no_latch;
	}

	mtr_memo_type_t	fix_type;

	switch (rw_latch) {
	case RW_NO_LATCH:

		fix_type = MTR_MEMO_BUF_FIX;
		break;

	case RW_S_LATCH:
		rw_lock_s_lock_inline(&fix_block->lock, 0, file, line);

		fix_type = MTR_MEMO_PAGE_S_FIX;
		break;

	case RW_SX_LATCH:
		rw_lock_sx_lock_inline(&fix_block->lock, 0, file, line);

		fix_type = MTR_MEMO_PAGE_SX_FIX;
		break;

	default:
		ut_ad(rw_latch == RW_X_LATCH);
		rw_lock_x_lock_inline(&fix_block->lock, 0, file, line);

		fix_type = MTR_MEMO_PAGE_X_FIX;
		break;
	}

	mtr_memo_push(mtr, fix_block, fix_type);

	if (mode != BUF_PEEK_IF_IN_POOL && !access_time) {
		/* In the case of a first access, try to apply linear
		read-ahead */

		buf_read_ahead_linear(page_id, page_size, ibuf_inside(mtr));
	}

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(fix_block->page.id) == 0);
#endif

	ut_ad(!rw_lock_own(hash_lock, RW_LOCK_X));
	ut_ad(!rw_lock_own(hash_lock, RW_LOCK_S));

	return(fix_block);
}

/********************************************************************//**
This is the general function used to get optimistic access to a database
page.
@return TRUE if success */
ibool
buf_page_optimistic_get(
/*====================*/
	ulint		rw_latch,/*!< in: RW_S_LATCH, RW_X_LATCH */
	buf_block_t*	block,	/*!< in: guessed buffer block */
	ib_uint64_t	modify_clock,/*!< in: modify clock value */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mini-transaction */
{
	buf_pool_t*	buf_pool;
	unsigned	access_time;
	ibool		success;

	ut_ad(block);
	ut_ad(mtr);
	ut_ad(mtr->is_active());
	ut_ad((rw_latch == RW_S_LATCH) || (rw_latch == RW_X_LATCH));

	buf_page_mutex_enter(block);

	if (UNIV_UNLIKELY(buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE)) {

		buf_page_mutex_exit(block);

		return(FALSE);
	}

	buf_block_buf_fix_inc(block, file, line);

	access_time = buf_page_is_accessed(&block->page);

	buf_page_set_accessed(&block->page);

	buf_page_mutex_exit(block);

	buf_page_make_young_if_needed(&block->page);

	ut_ad(!ibuf_inside(mtr)
	      || ibuf_page(block->page.id, block->page.size, NULL));

	mtr_memo_type_t	fix_type;

	switch (rw_latch) {
	case RW_S_LATCH:
		success = rw_lock_s_lock_nowait(&block->lock, file, line);

		fix_type = MTR_MEMO_PAGE_S_FIX;
		break;
	case RW_X_LATCH:
		success = rw_lock_x_lock_func_nowait_inline(
			&block->lock, file, line);

		fix_type = MTR_MEMO_PAGE_X_FIX;
		break;
	default:
		ut_error; /* RW_SX_LATCH is not implemented yet */
	}

	if (!success) {
		buf_page_mutex_enter(block);
		buf_block_buf_fix_dec(block);
		buf_page_mutex_exit(block);

		return(FALSE);
	}

	if (modify_clock != block->modify_clock) {

		buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

		if (rw_latch == RW_S_LATCH) {
			rw_lock_s_unlock(&block->lock);
		} else {
			rw_lock_x_unlock(&block->lock);
		}

		buf_page_mutex_enter(block);
		buf_block_buf_fix_dec(block);
		buf_page_mutex_exit(block);

		return(FALSE);
	}

	mtr_memo_push(mtr, block, fix_type);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(fsp_skip_sanity_check(block->page.id.space())
	     || ++buf_dbg_counter % 5771
	     || buf_validate());
	ut_a(block->page.buf_fix_count > 0);
	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	ut_d(buf_page_mutex_enter(block));
	ut_ad(!block->page.file_page_was_freed);
	ut_d(buf_page_mutex_exit(block));

	if (!access_time) {
		/* In the case of a first access, try to apply linear
		read-ahead */
		buf_read_ahead_linear(block->page.id, block->page.size,
				      ibuf_inside(mtr));
	}

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(block->page.id) == 0);
#endif /* UNIV_IBUF_COUNT_DEBUG */

	buf_pool = buf_pool_from_block(block);
	buf_pool->stat.n_page_gets++;

	return(TRUE);
}

/********************************************************************//**
This is used to get access to a known database page, when no waiting can be
done. For example, if a search in an adaptive hash index leads us to this
frame.
@return TRUE if success */
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

	ut_ad(mtr->is_active());
	ut_ad((rw_latch == RW_S_LATCH) || (rw_latch == RW_X_LATCH));

	buf_page_mutex_enter(block);

	if (buf_block_get_state(block) == BUF_BLOCK_REMOVE_HASH) {
		/* Another thread is just freeing the block from the LRU list
		of the buffer pool: do not try to access this page; this
		attempt to access the page can only come through the hash
		index because when the buffer block state is ..._REMOVE_HASH,
		we have already removed it from the page address hash table
		of the buffer pool. */

		buf_page_mutex_exit(block);

		return(FALSE);
	}

	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);

	buf_block_buf_fix_inc(block, file, line);

	buf_page_set_accessed(&block->page);

	buf_page_mutex_exit(block);

	buf_pool = buf_pool_from_block(block);

	if (mode == BUF_MAKE_YOUNG) {
		buf_page_make_young_if_needed(&block->page);
	}

	ut_ad(!ibuf_inside(mtr) || mode == BUF_KEEP_OLD);

	mtr_memo_type_t	fix_type;

	switch (rw_latch) {
	case RW_S_LATCH:
		success = rw_lock_s_lock_nowait(&block->lock, file, line);
		fix_type = MTR_MEMO_PAGE_S_FIX;
		break;
	case RW_X_LATCH:
		success = rw_lock_x_lock_func_nowait_inline(
			&block->lock, file, line);

		fix_type = MTR_MEMO_PAGE_X_FIX;
		break;
	default:
		ut_error; /* RW_SX_LATCH is not implemented yet */
	}

	if (!success) {
		buf_page_mutex_enter(block);
		buf_block_buf_fix_dec(block);
		buf_page_mutex_exit(block);

		return(FALSE);
	}

	mtr_memo_push(mtr, block, fix_type);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(++buf_dbg_counter % 5771 || buf_validate());
	ut_a(block->page.buf_fix_count > 0);
	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

#ifdef UNIV_DEBUG
	if (mode != BUF_KEEP_OLD) {
		/* If mode == BUF_KEEP_OLD, we are executing an I/O
		completion routine.  Avoid a bogus assertion failure
		when ibuf_merge_or_delete_for_page() is processing a
		page that was just freed due to DROP INDEX, or
		deleting a record from SYS_INDEXES. This check will be
		skipped in recv_recover_page() as well. */

		buf_page_mutex_enter(block);
		ut_a(!block->page.file_page_was_freed);
		buf_page_mutex_exit(block);
	}
#endif /* UNIV_DEBUG */

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a((mode == BUF_KEEP_OLD) || ibuf_count_get(block->page.id) == 0);
#endif
	buf_pool->stat.n_page_gets++;

	return(TRUE);
}

/** Given a tablespace id and page number tries to get that page. If the
page is not in the buffer pool it is not loaded and NULL is returned.
Suitable for using when holding the lock_sys_t::mutex.
@param[in]	page_id	page id
@param[in]	file	file name
@param[in]	line	line where called
@param[in]	mtr	mini-transaction
@return pointer to a page or NULL */
const buf_block_t*
buf_page_try_get_func(
	const page_id_t&	page_id,
	const char*		file,
	ulint			line,
	mtr_t*			mtr)
{
	buf_block_t*	block;
	ibool		success;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);
	rw_lock_t*	hash_lock;

	ut_ad(mtr);
	ut_ad(mtr->is_active());

	block = buf_block_hash_get_s_locked(buf_pool, page_id, &hash_lock);

	if (!block || buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE) {
		if (block) {
			rw_lock_s_unlock(hash_lock);
		}
		return(NULL);
	}

	ut_ad(!buf_pool_watch_is_sentinel(buf_pool, &block->page));

	buf_page_mutex_enter(block);
	rw_lock_s_unlock(hash_lock);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
	ut_a(page_id.equals_to(block->page.id));
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	buf_block_buf_fix_inc(block, file, line);
	buf_page_mutex_exit(block);

	mtr_memo_type_t	fix_type = MTR_MEMO_PAGE_S_FIX;
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
		buf_page_mutex_enter(block);
		buf_block_buf_fix_dec(block);
		buf_page_mutex_exit(block);

		return(NULL);
	}

	mtr_memo_push(mtr, block, fix_type);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(fsp_skip_sanity_check(block->page.id.space())
	     || ++buf_dbg_counter % 5771
	     || buf_validate());
	ut_a(block->page.buf_fix_count > 0);
	ut_a(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

	ut_d(buf_page_mutex_enter(block));
	ut_d(ut_a(!block->page.file_page_was_freed));
	ut_d(buf_page_mutex_exit(block));

	buf_block_dbg_add_level(block, SYNC_NO_ORDER_CHECK);

	buf_pool->stat.n_page_gets++;

#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(block->page.id) == 0);
#endif /* UNIV_IBUF_COUNT_DEBUG */

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

	ut_d(bpage->file_page_was_freed = FALSE);
}

/** Inits a page to the buffer buf_pool.
@param[in,out]	buf_pool	buffer pool
@param[in]	page_id		page id
@param[in,out]	block		block to init */
static
void
buf_page_init(
	buf_pool_t*		buf_pool,
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	buf_block_t*		block)
{
	buf_page_t*	hash_page;

	ut_ad(buf_pool == buf_pool_get(page_id));
	ut_ad(buf_pool_mutex_own(buf_pool));

	ut_ad(buf_page_mutex_own(block));
	ut_a(buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE);

	ut_ad(rw_lock_own(buf_page_hash_lock_get(buf_pool, page_id),
			  RW_LOCK_X));

	/* Set the state of the block */
	buf_block_set_file_page(block, page_id);

#ifdef UNIV_DEBUG_VALGRIND
	if (is_system_tablespace(page_id.space())) {
		/* Silence valid Valgrind warnings about uninitialized
		data being written to data files.  There are some unused
		bytes on some pages that InnoDB does not initialize. */
		UNIV_MEM_VALID(block->frame, UNIV_PAGE_SIZE);
	}
#endif /* UNIV_DEBUG_VALGRIND */

	buf_block_init_low(block);

	block->lock_hash_val = lock_rec_hash(page_id.space(),
					     page_id.page_no());

	buf_page_init_low(&block->page);

	/* Insert into the hash table of file pages */

	hash_page = buf_page_hash_get_low(buf_pool, page_id);

	if (hash_page == NULL) {
		/* Block not found in hash table */
	} else if (buf_pool_watch_is_sentinel(buf_pool, hash_page)) {
		/* Preserve the reference count. */
		ib_uint32_t	buf_fix_count = hash_page->buf_fix_count;

		ut_a(buf_fix_count > 0);

		os_atomic_increment_uint32(&block->page.buf_fix_count,
					   buf_fix_count);

		buf_pool_watch_remove(buf_pool, hash_page);
	} else {

		ib::error() << "Page " << page_id
			<< " already found in the hash table: "
			<< hash_page << ", " << block;

		ut_d(buf_page_mutex_exit(block));
		ut_d(buf_pool_mutex_exit(buf_pool));
		ut_d(buf_print());
		ut_d(buf_LRU_print());
		ut_d(buf_validate());
		ut_d(buf_LRU_validate());
		ut_ad(0);
	}

	ut_ad(!block->page.in_zip_hash);
	ut_ad(!block->page.in_page_hash);
	ut_d(block->page.in_page_hash = TRUE);

	block->page.id.copy_from(page_id);
	block->page.size.copy_from(page_size);

	HASH_INSERT(buf_page_t, hash, buf_pool->page_hash,
		    page_id.fold(), &block->page);

	if (page_size.is_compressed()) {
		page_zip_set_size(&block->page.zip, page_size.physical());
	}
}

/** Inits a page for read to the buffer buf_pool. If the page is
(1) already in buf_pool, or
(2) if we specify to read only ibuf pages and the page is not an ibuf page, or
(3) if the space is deleted or being deleted,
then this function does nothing.
Sets the io_fix flag to BUF_IO_READ and sets a non-recursive exclusive lock
on the buffer frame. The io-handler must take care that the flag is cleared
and the lock released later.
@param[out]	err			DB_SUCCESS or DB_TABLESPACE_DELETED
@param[in]	mode			BUF_READ_IBUF_PAGES_ONLY, ...
@param[in]	page_id			page id
@param[in]	unzip			TRUE=request uncompressed page
@return pointer to the block or NULL */
buf_page_t*
buf_page_init_for_read(
	dberr_t*		err,
	ulint			mode,
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	ibool			unzip)
{
	buf_block_t*	block;
	buf_page_t*	bpage	= NULL;
	buf_page_t*	watch_page;
	rw_lock_t*	hash_lock;
	mtr_t		mtr;
	ibool		lru	= FALSE;
	void*		data;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);

	ut_ad(buf_pool);

	*err = DB_SUCCESS;

	if (mode == BUF_READ_IBUF_PAGES_ONLY) {
		/* It is a read-ahead within an ibuf routine */

		ut_ad(!ibuf_bitmap_page(page_id, page_size));

		ibuf_mtr_start(&mtr);

		if (!recv_no_ibuf_operations &&
		    !ibuf_page(page_id, page_size, &mtr)) {

			ibuf_mtr_commit(&mtr);

			return(NULL);
		}
	} else {
		ut_ad(mode == BUF_READ_ANY_PAGE);
	}

	if (page_size.is_compressed() && !unzip && !recv_recovery_is_on()) {
		block = NULL;
	} else {
		block = buf_LRU_get_free_block(buf_pool);
		ut_ad(block);
		ut_ad(buf_pool_from_block(block) == buf_pool);
	}

	buf_pool_mutex_enter(buf_pool);

	hash_lock = buf_page_hash_lock_get(buf_pool, page_id);
	rw_lock_x_lock(hash_lock);

	watch_page = buf_page_hash_get_low(buf_pool, page_id);
	if (watch_page && !buf_pool_watch_is_sentinel(buf_pool, watch_page)) {
		/* The page is already in the buffer pool. */
		watch_page = NULL;
		rw_lock_x_unlock(hash_lock);
		if (block) {
			buf_page_mutex_enter(block);
			buf_LRU_block_free_non_file_page(block);
			buf_page_mutex_exit(block);
		}

		bpage = NULL;
		goto func_exit;
	}

	if (block) {
		bpage = &block->page;

		buf_page_mutex_enter(block);

		ut_ad(buf_pool_from_bpage(bpage) == buf_pool);

		buf_page_init(buf_pool, page_id, page_size, block);

		/* Note: We are using the hash_lock for protection. This is
		safe because no other thread can lookup the block from the
		page hashtable yet. */

		buf_page_set_io_fix(bpage, BUF_IO_READ);

		rw_lock_x_unlock(hash_lock);

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

		if (page_size.is_compressed()) {
			/* buf_pool->mutex may be released and
			reacquired by buf_buddy_alloc().  Thus, we
			must release block->mutex in order not to
			break the latching order in the reacquisition
			of buf_pool->mutex.  We also must defer this
			operation until after the block descriptor has
			been added to buf_pool->LRU and
			buf_pool->page_hash. */
			buf_page_mutex_exit(block);
			data = buf_buddy_alloc(buf_pool, page_size.physical(),
					       &lru);
			buf_page_mutex_enter(block);
			block->page.zip.data = (page_zip_t*) data;

			/* To maintain the invariant
			block->in_unzip_LRU_list
			== buf_page_belongs_to_unzip_LRU(&block->page)
			we have to add this block to unzip_LRU
			after block->page.zip.data is set. */
			ut_ad(buf_page_belongs_to_unzip_LRU(&block->page));
			buf_unzip_LRU_add_block(block, TRUE);
		}

		buf_page_mutex_exit(block);
	} else {
		rw_lock_x_unlock(hash_lock);

		/* The compressed page must be allocated before the
		control block (bpage), in order to avoid the
		invocation of buf_buddy_relocate_block() on
		uninitialized data. */
		data = buf_buddy_alloc(buf_pool, page_size.physical(), &lru);

		rw_lock_x_lock(hash_lock);

		/* If buf_buddy_alloc() allocated storage from the LRU list,
		it released and reacquired buf_pool->mutex.  Thus, we must
		check the page_hash again, as it may have been modified. */
		if (UNIV_UNLIKELY(lru)) {

			watch_page = buf_page_hash_get_low(buf_pool, page_id);

			if (UNIV_UNLIKELY(watch_page
			    && !buf_pool_watch_is_sentinel(buf_pool,
							   watch_page))) {

				/* The block was added by some other thread. */
				rw_lock_x_unlock(hash_lock);
				watch_page = NULL;
				buf_buddy_free(buf_pool, data,
					       page_size.physical());

				bpage = NULL;
				goto func_exit;
			}
		}

		bpage = buf_page_alloc_descriptor();

		/* Initialize the buf_pool pointer. */
		bpage->buf_pool_index = buf_pool_index(buf_pool);

		page_zip_des_init(&bpage->zip);
		page_zip_set_size(&bpage->zip, page_size.physical());
		bpage->zip.data = (page_zip_t*) data;

		bpage->size.copy_from(page_size);

		mutex_enter(&buf_pool->zip_mutex);
		UNIV_MEM_DESC(bpage->zip.data, bpage->size.physical());

		buf_page_init_low(bpage);

		bpage->state = BUF_BLOCK_ZIP_PAGE;
		bpage->id.copy_from(page_id);
		bpage->flush_observer = NULL;

		ut_d(bpage->in_page_hash = FALSE);
		ut_d(bpage->in_zip_hash = FALSE);
		ut_d(bpage->in_flush_list = FALSE);
		ut_d(bpage->in_free_list = FALSE);
		ut_d(bpage->in_LRU_list = FALSE);

		ut_d(bpage->in_page_hash = TRUE);

		if (watch_page != NULL) {

			/* Preserve the reference count. */
			ib_uint32_t	buf_fix_count;

			buf_fix_count = watch_page->buf_fix_count;

			ut_a(buf_fix_count > 0);

			os_atomic_increment_uint32(
				&bpage->buf_fix_count, buf_fix_count);

			ut_ad(buf_pool_watch_is_sentinel(buf_pool, watch_page));
			buf_pool_watch_remove(buf_pool, watch_page);
		}

		HASH_INSERT(buf_page_t, hash, buf_pool->page_hash,
			    bpage->id.fold(), bpage);

		rw_lock_x_unlock(hash_lock);

		/* The block must be put to the LRU list, to the old blocks.
		The zip size is already set into the page zip */
		buf_LRU_add_block(bpage, TRUE/* to old blocks */);
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
		buf_LRU_insert_zip_clean(bpage);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

		buf_page_set_io_fix(bpage, BUF_IO_READ);

		mutex_exit(&buf_pool->zip_mutex);
	}

	buf_pool->n_pend_reads++;
func_exit:
	buf_pool_mutex_exit(buf_pool);

	if (mode == BUF_READ_IBUF_PAGES_ONLY) {

		ibuf_mtr_commit(&mtr);
	}

	ut_ad(!rw_lock_own(hash_lock, RW_LOCK_X));
	ut_ad(!rw_lock_own(hash_lock, RW_LOCK_S));
	ut_ad(!bpage || buf_page_in_file(bpage));

	return(bpage);
}

/** Initializes a page to the buffer buf_pool. The page is usually not read
from a file even if it cannot be found in the buffer buf_pool. This is one
of the functions which perform to a block a state transition NOT_USED =>
FILE_PAGE (the other is buf_page_get_gen).
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in]	mtr		mini-transaction
@return pointer to the block, page bufferfixed */
buf_block_t*
buf_page_create(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	mtr_t*			mtr)
{
	buf_frame_t*	frame;
	buf_block_t*	block;
	buf_block_t*	free_block	= NULL;
	buf_pool_t*	buf_pool = buf_pool_get(page_id);
	rw_lock_t*	hash_lock;

	ut_ad(mtr->is_active());
	ut_ad(page_id.space() != 0 || !page_size.is_compressed());

	free_block = buf_LRU_get_free_block(buf_pool);

	buf_pool_mutex_enter(buf_pool);

	hash_lock = buf_page_hash_lock_get(buf_pool, page_id);
	rw_lock_x_lock(hash_lock);

	block = (buf_block_t*) buf_page_hash_get_low(buf_pool, page_id);

	if (block
	    && buf_page_in_file(&block->page)
	    && !buf_pool_watch_is_sentinel(buf_pool, &block->page)) {

#ifdef UNIV_IBUF_COUNT_DEBUG
		ut_a(ibuf_count_get(page_id) == 0);
#endif /* UNIV_IBUF_COUNT_DEBUG */

		ut_d(block->page.file_page_was_freed = FALSE);

		/* Page can be found in buf_pool */
		buf_pool_mutex_exit(buf_pool);
		rw_lock_x_unlock(hash_lock);

		buf_block_free(free_block);

		return(buf_page_get_with_no_latch(page_id, page_size, mtr));
	}

	/* If we get here, the page was not in buf_pool: init it there */

	DBUG_PRINT("ib_buf", ("create page " UINT32PF ":" UINT32PF,
			      page_id.space(), page_id.page_no()));

	block = free_block;

	buf_page_mutex_enter(block);

	buf_page_init(buf_pool, page_id, page_size, block);

	rw_lock_x_unlock(hash_lock);

	/* The block must be put to the LRU list */
	buf_LRU_add_block(&block->page, FALSE);

	buf_block_buf_fix_inc(block, __FILE__, __LINE__);
	buf_pool->stat.n_pages_created++;

	if (page_size.is_compressed()) {
		void*	data;
		ibool	lru;

		/* Prevent race conditions during buf_buddy_alloc(),
		which may release and reacquire buf_pool->mutex,
		by IO-fixing and X-latching the block. */

		buf_page_set_io_fix(&block->page, BUF_IO_READ);
		rw_lock_x_lock(&block->lock);

		buf_page_mutex_exit(block);
		/* buf_pool->mutex may be released and reacquired by
		buf_buddy_alloc().  Thus, we must release block->mutex
		in order not to break the latching order in
		the reacquisition of buf_pool->mutex.  We also must
		defer this operation until after the block descriptor
		has been added to buf_pool->LRU and buf_pool->page_hash. */
		data = buf_buddy_alloc(buf_pool, page_size.physical(), &lru);
		buf_page_mutex_enter(block);
		block->page.zip.data = (page_zip_t*) data;

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

	buf_pool_mutex_exit(buf_pool);

	mtr_memo_push(mtr, block, MTR_MEMO_BUF_FIX);

	buf_page_set_accessed(&block->page);

	buf_page_mutex_exit(block);

	/* Delete possible entries for the page from the insert buffer:
	such can exist if the page belonged to an index which was dropped */
	ibuf_merge_or_delete_for_page(NULL, page_id, &page_size, TRUE);

	frame = block->frame;

	memset(frame + FIL_PAGE_PREV, 0xff, 4);
	memset(frame + FIL_PAGE_NEXT, 0xff, 4);
	mach_write_to_2(frame + FIL_PAGE_TYPE, FIL_PAGE_TYPE_ALLOCATED);

	/* These 8 bytes are also repurposed for PageIO compression and must
	be reset when the frame is assigned to a new page id. See fil0fil.h.

	FIL_PAGE_FILE_FLUSH_LSN is used on the following pages:
	(1) The first page of the InnoDB system tablespace (page 0:0)
	(2) FIL_RTREE_SPLIT_SEQ_NUM on R-tree pages .

	Therefore we don't transparently compress such pages. */

	memset(frame + FIL_PAGE_FILE_FLUSH_LSN, 0, 8);

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ut_a(++buf_dbg_counter % 5771 || buf_validate());
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#ifdef UNIV_IBUF_COUNT_DEBUG
	ut_a(ibuf_count_get(block->page.id) == 0);
#endif
	return(block);
}

/********************************************************************//**
Monitor the buffer page read/write activity, and increment corresponding
counter value if MONITOR_MODULE_BUF_PAGE (module_buf_page) module is
enabled. */
static
void
buf_page_monitor(
/*=============*/
	const buf_page_t*	bpage,	/*!< in: pointer to the block */
	enum buf_io_fix		io_type)/*!< in: io_fix types */
{
	const byte*	frame;
	monitor_id_t	counter;

	/* If the counter module is not turned on, just return */
	if (!MONITOR_IS_ON(MONITOR_MODULE_BUF_PAGE)) {
		return;
	}

	ut_a(io_type == BUF_IO_READ || io_type == BUF_IO_WRITE);

	frame = bpage->zip.data
		? bpage->zip.data
		: ((buf_block_t*) bpage)->frame;

	switch (fil_page_get_type(frame)) {
		ulint	level;

	case FIL_PAGE_INDEX:
	case FIL_PAGE_RTREE:
		level = btr_page_get_level_low(frame);

		/* Check if it is an index page for insert buffer */
		if (btr_page_get_index_id(frame)
		    == (index_id_t)(DICT_IBUF_ID_MIN + IBUF_SPACE_ID)) {
			if (level == 0) {
				counter = MONITOR_RW_COUNTER(
					io_type, MONITOR_INDEX_IBUF_LEAF_PAGE);
			} else {
				counter = MONITOR_RW_COUNTER(
					io_type,
					MONITOR_INDEX_IBUF_NON_LEAF_PAGE);
			}
		} else {
			if (level == 0) {
				counter = MONITOR_RW_COUNTER(
					io_type, MONITOR_INDEX_LEAF_PAGE);
			} else {
				counter = MONITOR_RW_COUNTER(
					io_type, MONITOR_INDEX_NON_LEAF_PAGE);
			}
		}
		break;

	case FIL_PAGE_UNDO_LOG:
		counter = MONITOR_RW_COUNTER(io_type, MONITOR_UNDO_LOG_PAGE);
		break;

	case FIL_PAGE_INODE:
		counter = MONITOR_RW_COUNTER(io_type, MONITOR_INODE_PAGE);
		break;

	case FIL_PAGE_IBUF_FREE_LIST:
		counter = MONITOR_RW_COUNTER(io_type,
					     MONITOR_IBUF_FREELIST_PAGE);
		break;

	case FIL_PAGE_IBUF_BITMAP:
		counter = MONITOR_RW_COUNTER(io_type,
					     MONITOR_IBUF_BITMAP_PAGE);
		break;

	case FIL_PAGE_TYPE_SYS:
		counter = MONITOR_RW_COUNTER(io_type, MONITOR_SYSTEM_PAGE);
		break;

	case FIL_PAGE_TYPE_TRX_SYS:
		counter = MONITOR_RW_COUNTER(io_type, MONITOR_TRX_SYSTEM_PAGE);
		break;

	case FIL_PAGE_TYPE_FSP_HDR:
		counter = MONITOR_RW_COUNTER(io_type, MONITOR_FSP_HDR_PAGE);
		break;

	case FIL_PAGE_TYPE_XDES:
		counter = MONITOR_RW_COUNTER(io_type, MONITOR_XDES_PAGE);
		break;

	case FIL_PAGE_TYPE_BLOB:
		counter = MONITOR_RW_COUNTER(io_type, MONITOR_BLOB_PAGE);
		break;

	case FIL_PAGE_TYPE_ZBLOB:
		counter = MONITOR_RW_COUNTER(io_type, MONITOR_ZBLOB_PAGE);
		break;

	case FIL_PAGE_TYPE_ZBLOB2:
		counter = MONITOR_RW_COUNTER(io_type, MONITOR_ZBLOB2_PAGE);
		break;

	default:
		counter = MONITOR_RW_COUNTER(io_type, MONITOR_OTHER_PAGE);
	}

	MONITOR_INC_NOCHECK(counter);
}

/********************************************************************//**
Mark a table with the specified space pointed by bpage->id.space() corrupted.
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
	ib_uint32_t	space = bpage->id.space();
	ibool		ret = TRUE;

	/* First unfix and release lock on the bpage */
	buf_pool_mutex_enter(buf_pool);
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

	mutex_exit(buf_page_get_mutex(bpage));

	/* Find the table with specified space id, and mark it corrupted */
	if (dict_set_corrupted_by_space(space)) {
		buf_LRU_free_one_page(bpage);
	} else {
		ret = FALSE;
	}

	ut_ad(buf_pool->n_pend_reads > 0);
	buf_pool->n_pend_reads--;

	buf_pool_mutex_exit(buf_pool);

	return(ret);
}

/********************************************************************//**
Completes an asynchronous read or write request of a file page to or from
the buffer pool.
@return true if successful */
bool
buf_page_io_complete(
/*=================*/
	buf_page_t*	bpage,	/*!< in: pointer to the block in question */
	bool		evict)	/*!< in: whether or not to evict the page
				from LRU list. */

{
	enum buf_io_fix	io_type;
	buf_pool_t*	buf_pool = buf_pool_from_bpage(bpage);
	const ibool	uncompressed = (buf_page_get_state(bpage)
					== BUF_BLOCK_FILE_PAGE);

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
		bool	compressed_page;

		if (bpage->size.is_compressed()) {
			frame = bpage->zip.data;
			buf_pool->n_pend_unzip++;

			if (uncompressed
			    && !buf_zip_decompress((buf_block_t*) bpage,
						   FALSE)) {

				buf_pool->n_pend_unzip--;
				compressed_page = false;
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

		if (bpage->id.space() == TRX_SYS_SPACE
		    && buf_dblwr_page_inside(bpage->id.page_no())) {

			ib::error() << "Reading page " << bpage->id
				<< ", which is in the doublewrite buffer!";

		} else if (read_space_id == 0 && read_page_no == 0) {
			/* This is likely an uninitialized page. */
		} else if ((bpage->id.space() != 0
			    && bpage->id.space() != read_space_id)
			   || bpage->id.page_no() != read_page_no) {
			/* We did not compare space_id to read_space_id
			if bpage->space == 0, because the field on the
			page may contain garbage in MySQL < 4.1.1,
			which only supported bpage->space == 0. */

			ib::error() << "Space id and page no stored in "
				"the page, read in are "
				<< page_id_t(read_space_id, read_page_no)
				<< ", should be " << bpage->id;
		}

		compressed_page = Compression::is_compressed_page(frame);

		/* If the decompress failed then the most likely case is
		that we are reading in a page for which this instance doesn't
		support the compression algorithm. */
		if (compressed_page) {

			Compression::meta_t	meta;

			Compression::deserialize_header(frame, &meta);

			ib::error()
				<< "Page " << bpage->id << " "
				<< "compressed with "
				<< Compression::to_string(meta) << " "
				<< "that is not supported by this instance";
		}

		/* From version 3.23.38 up we store the page checksum
		to the 4 first bytes of the page end lsn field */
		if (compressed_page
		    || buf_page_is_corrupted(
			    true, frame, bpage->size,
			    fsp_is_checksum_disabled(bpage->id.space()))) {

			/* Not a real corruption if it was triggered by
			error injection */
			DBUG_EXECUTE_IF(
				"buf_page_import_corrupt_failure",
				if (bpage->id.space() > TRX_SYS_SPACE
				    && !Tablespace::is_undo_tablespace(
					    bpage->id.space())
				    && buf_mark_space_corrupt(bpage)) {
					ib::info() << "Simulated IMPORT "
						"corruption";
					return(true);
				}
				goto page_not_corrupt;
				;);
corrupt:
			/* Compressed pages are basically gibberish avoid
			printing the contents. */
			if (!compressed_page) {

				ib::error()
					<< "Database page corruption on disk"
					" or a failed file read of page "
					<< bpage->id
					<< ". You may have to recover from "
					<< "a backup.";

				buf_page_print(
					frame, bpage->size,
					BUF_PAGE_PRINT_NO_CRASH);

				ib::info()
					<< "It is also possible that your"
				        " operating system has corrupted"
					" its own file cache and rebooting"
					" your computer removes the error."
					" If the corrupt page is an index page."
					" You can also try to fix the"
					" corruption by dumping, dropping,"
					" and reimporting the corrupt table."
					" You can use CHECK TABLE to scan"
					" your table for corruption. "
					<< FORCE_RECOVERY_MSG;
			}

			if (srv_force_recovery < SRV_FORCE_IGNORE_CORRUPT) {

				/* If page space id is larger than TRX_SYS_SPACE
				(0), we will attempt to mark the corresponding
				table as corrupted instead of crashing server */

				if (bpage->id.space() > TRX_SYS_SPACE
				    && buf_mark_space_corrupt(bpage)) {

					return(false);
				} else {
					ib::fatal()
						<< "Aborting because of a"
						" corrupt database page in"
						" the system tablespace. Or, "
						" there was a failure in"
						" tagging the tablespace "
						" as corrupt.";
				}
			}
		}

		DBUG_EXECUTE_IF("buf_page_import_corrupt_failure",
				page_not_corrupt:  bpage = bpage; );

		if (recv_recovery_is_on()) {
			/* Pages must be uncompressed for crash recovery. */
			ut_a(uncompressed);
			recv_recover_page(TRUE, (buf_block_t*) bpage);
		}

		/* If space is being truncated then avoid ibuf operation.
		During re-init we have already freed ibuf entries. */
		if (uncompressed
		    && !Compression::is_compressed_page(frame)
		    && !recv_no_ibuf_operations
		    && !Tablespace::is_undo_tablespace(bpage->id.space())
		    && bpage->id.space() != srv_tmp_space.space_id()
		    && !srv_is_tablespace_truncated(bpage->id.space())
		    && fil_page_get_type(frame) == FIL_PAGE_INDEX
		    && page_is_leaf(frame)) {

			ibuf_merge_or_delete_for_page(
				(buf_block_t*) bpage, bpage->id,
				&bpage->size, TRUE);
		}
	}

	buf_pool_mutex_enter(buf_pool);
	mutex_enter(buf_page_get_mutex(bpage));

#ifdef UNIV_IBUF_COUNT_DEBUG
	if (io_type == BUF_IO_WRITE || uncompressed) {
		/* For BUF_IO_READ of compressed-only blocks, the
		buffered operations will be merged by buf_page_get_gen()
		after the block has been uncompressed. */
		ut_a(ibuf_count_get(bpage->id) == 0);
	}
#endif
	/* Because this thread which does the unlocking is not the same that
	did the locking, we use a pass value != 0 in unlock, which simply
	removes the newest lock debug record, without checking the thread
	id. */

	buf_page_set_io_fix(bpage, BUF_IO_NONE);
	buf_page_monitor(bpage, io_type);

	switch (io_type) {
	case BUF_IO_READ:
		/* NOTE that the call to ibuf may have moved the ownership of
		the x-latch to this OS thread: do not let this confuse you in
		debugging! */

		ut_ad(buf_pool->n_pend_reads > 0);
		buf_pool->n_pend_reads--;
		buf_pool->stat.n_pages_read++;

		if (uncompressed) {
			rw_lock_x_unlock_gen(&((buf_block_t*) bpage)->lock,
					     BUF_IO_READ);
		}

		mutex_exit(buf_page_get_mutex(bpage));

		break;

	case BUF_IO_WRITE:
		/* Write means a flush operation: call the completion
		routine in the flush system */

		buf_flush_write_complete(bpage);

		if (uncompressed) {
			rw_lock_sx_unlock_gen(&((buf_block_t*) bpage)->lock,
					      BUF_IO_WRITE);
		}

		buf_pool->stat.n_pages_written++;

		/* We decide whether or not to evict the page from the
		LRU list based on the flush_type.
		* BUF_FLUSH_LIST: don't evict
		* BUF_FLUSH_LRU: always evict
		* BUF_FLUSH_SINGLE_PAGE: eviction preference is passed
		by the caller explicitly. */
		if (buf_page_get_flush_type(bpage) == BUF_FLUSH_LRU) {
			evict = true;
		}

		if (evict) {
			mutex_exit(buf_page_get_mutex(bpage));
			buf_LRU_free_page(bpage, true);
		} else {
			mutex_exit(buf_page_get_mutex(bpage));
		}

		break;

	default:
		ut_error;
	}

	DBUG_PRINT("ib_buf", ("%s page " UINT32PF ":" UINT32PF,
			      io_type == BUF_IO_READ ? "read" : "wrote",
			      bpage->id.space(), bpage->id.page_no()));

	buf_pool_mutex_exit(buf_pool);

	return(true);
}

/*********************************************************************//**
Asserts that all file pages in the buffer are in a replaceable state.
@return TRUE */
static
ibool
buf_all_freed_instance(
/*===================*/
	buf_pool_t*	buf_pool)	/*!< in: buffer pool instancce */
{
	ulint		i;
	buf_chunk_t*	chunk;

	ut_ad(buf_pool);

	buf_pool_mutex_enter(buf_pool);

	chunk = buf_pool->chunks;

	for (i = buf_pool->n_chunks; i--; chunk++) {

		const buf_block_t* block = buf_chunk_not_freed(chunk);

		if (UNIV_LIKELY_NULL(block)) {
			ib::fatal() << "Page " << block->page.id
				<< " still fixed or dirty";
		}
	}

	buf_pool_mutex_exit(buf_pool);

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
	ulint		i;

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
			buf_flush_t	type = static_cast<buf_flush_t>(i);

			buf_pool_mutex_exit(buf_pool);
			buf_flush_wait_batch_end(buf_pool, type);
			buf_pool_mutex_enter(buf_pool);
		}
	}

	buf_pool_mutex_exit(buf_pool);

	ut_ad(buf_all_freed_instance(buf_pool));

	buf_pool_mutex_enter(buf_pool);

	while (buf_LRU_scan_and_free_block(buf_pool, true)) {
	}

	ut_ad(UT_LIST_GET_LEN(buf_pool->LRU) == 0);
	ut_ad(UT_LIST_GET_LEN(buf_pool->unzip_LRU) == 0);

	buf_pool->freed_page_clock = 0;
	buf_pool->LRU_old = NULL;
	buf_pool->LRU_old_len = 0;

	memset(&buf_pool->stat, 0x00, sizeof(buf_pool->stat));
	buf_refresh_io_stats(buf_pool);

	buf_pool_mutex_exit(buf_pool);
}

/*********************************************************************//**
Invalidates the file pages in the buffer pool when an archive recovery is
completed. All the file pages buffered must be in a replaceable state when
this function is called: not latched and not modified. */
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
@return TRUE */
static
ibool
buf_pool_validate_instance(
/*=======================*/
	buf_pool_t*	buf_pool)	/*!< in: buffer pool instance */
{
	buf_page_t*	b;
	buf_chunk_t*	chunk;
	ulint		i;
	ulint		n_lru_flush	= 0;
	ulint		n_page_flush	= 0;
	ulint		n_list_flush	= 0;
	ulint		n_lru		= 0;
	ulint		n_flush		= 0;
	ulint		n_free		= 0;
	ulint		n_zip		= 0;

	ut_ad(buf_pool);

	buf_pool_mutex_enter(buf_pool);
	hash_lock_x_all(buf_pool->page_hash);

	chunk = buf_pool->chunks;

	/* Check the uncompressed blocks. */

	for (i = buf_pool->n_chunks; i--; chunk++) {

		ulint		j;
		buf_block_t*	block = chunk->blocks;

		for (j = chunk->size; j--; block++) {

			buf_page_mutex_enter(block);

			switch (buf_block_get_state(block)) {
			case BUF_BLOCK_POOL_WATCH:
			case BUF_BLOCK_ZIP_PAGE:
			case BUF_BLOCK_ZIP_DIRTY:
				/* These should only occur on
				zip_clean, zip_free[], or flush_list. */
				ut_error;
				break;

			case BUF_BLOCK_FILE_PAGE:
				ut_a(buf_page_hash_get_low(
						buf_pool, block->page.id)
				     == &block->page);

#ifdef UNIV_IBUF_COUNT_DEBUG
				ut_a(buf_page_get_io_fix(&block->page)
				     == BUF_IO_READ
				     || !ibuf_count_get(block->page.id));
#endif
				switch (buf_page_get_io_fix(&block->page)) {
				case BUF_IO_NONE:
					break;

				case BUF_IO_WRITE:
					switch (buf_page_get_flush_type(
							&block->page)) {
					case BUF_FLUSH_LRU:
						n_lru_flush++;
						goto assert_s_latched;
					case BUF_FLUSH_SINGLE_PAGE:
						n_page_flush++;
assert_s_latched:
						ut_a(rw_lock_is_locked(
							     &block->lock,
								     RW_LOCK_S)
						     || rw_lock_is_locked(
								&block->lock,
								RW_LOCK_SX));
						break;
					case BUF_FLUSH_LIST:
						n_list_flush++;
						break;
					default:
						ut_error;
					}

					break;

				case BUF_IO_READ:

					ut_a(rw_lock_is_locked(&block->lock,
							       RW_LOCK_X));
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

			buf_page_mutex_exit(block);
		}
	}

	mutex_enter(&buf_pool->zip_mutex);

	/* Check clean compressed-only blocks. */

	for (b = UT_LIST_GET_FIRST(buf_pool->zip_clean); b;
	     b = UT_LIST_GET_NEXT(list, b)) {
		ut_a(buf_page_get_state(b) == BUF_BLOCK_ZIP_PAGE);
		switch (buf_page_get_io_fix(b)) {
		case BUF_IO_NONE:
		case BUF_IO_PIN:
			/* All clean blocks should be I/O-unfixed. */
			break;
		case BUF_IO_READ:
			/* In buf_LRU_free_page(), we temporarily set
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
		ut_a(buf_page_hash_get_low(buf_pool, b->id) == b);
		n_lru++;
		n_zip++;
	}

	/* Check dirty blocks. */

	buf_flush_list_mutex_enter(buf_pool);
	for (b = UT_LIST_GET_FIRST(buf_pool->flush_list); b;
	     b = UT_LIST_GET_NEXT(list, b)) {
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
				case BUF_FLUSH_SINGLE_PAGE:
					n_page_flush++;
					break;
				case BUF_FLUSH_LIST:
					n_list_flush++;
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
		case BUF_BLOCK_POOL_WATCH:
		case BUF_BLOCK_ZIP_PAGE:
		case BUF_BLOCK_NOT_USED:
		case BUF_BLOCK_READY_FOR_USE:
		case BUF_BLOCK_MEMORY:
		case BUF_BLOCK_REMOVE_HASH:
			ut_error;
			break;
		}
		ut_a(buf_page_hash_get_low(buf_pool, b->id) == b);
	}

	ut_a(UT_LIST_GET_LEN(buf_pool->flush_list) == n_flush);

	hash_unlock_x_all(buf_pool->page_hash);
	buf_flush_list_mutex_exit(buf_pool);

	mutex_exit(&buf_pool->zip_mutex);

	if (buf_pool->curr_size == buf_pool->old_size
	    && n_lru + n_free > buf_pool->curr_size + n_zip) {

		ib::fatal() << "n_LRU " << n_lru << ", n_free " << n_free
			<< ", pool " << buf_pool->curr_size
			<< " zip " << n_zip << ". Aborting...";
	}

	ut_a(UT_LIST_GET_LEN(buf_pool->LRU) == n_lru);
	if (buf_pool->curr_size == buf_pool->old_size
	    && UT_LIST_GET_LEN(buf_pool->free) != n_free) {

		ib::fatal() << "Free list len "
			<< UT_LIST_GET_LEN(buf_pool->free)
			<< ", free blocks " << n_free << ". Aborting...";
	}

	ut_a(buf_pool->n_flush[BUF_FLUSH_LIST] == n_list_flush);
	ut_a(buf_pool->n_flush[BUF_FLUSH_LRU] == n_lru_flush);
	ut_a(buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE] == n_page_flush);

	buf_pool_mutex_exit(buf_pool);

	ut_a(buf_LRU_validate());
	ut_a(buf_flush_validate(buf_pool));

	return(TRUE);
}

/*********************************************************************//**
Validates the buffer buf_pool data structure.
@return TRUE */
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

	index_ids = static_cast<index_id_t*>(
		ut_malloc_nokey(size * sizeof *index_ids));

	counts = static_cast<ulint*>(ut_malloc_nokey(sizeof(ulint) * size));

	buf_pool_mutex_enter(buf_pool);
	buf_flush_list_mutex_enter(buf_pool);

	ib::info() << *buf_pool;

	buf_flush_list_mutex_exit(buf_pool);

	/* Count the number of blocks belonging to each index in the buffer */

	n_found = 0;

	chunk = buf_pool->chunks;

	for (i = buf_pool->n_chunks; i--; chunk++) {
		buf_block_t*	block		= chunk->blocks;
		ulint		n_blocks	= chunk->size;

		for (; n_blocks--; block++) {
			const buf_frame_t* frame = block->frame;

			if (fil_page_index_page_check(frame)) {

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

	buf_pool_mutex_exit(buf_pool);

	for (i = 0; i < n_found; i++) {
		index = dict_index_get_if_in_cache(index_ids[i]);

		if (!index) {
			ib::info() << "Block count for index "
				<< index_ids[i] << " in buffer is about "
				<< counts[i];
		} else {
			ib::info() << "Block count for index " << index_ids[i]
				<< " in buffer is about " << counts[i]
				<< ", index " << index->name
				<< " of table " << index->table->name;
		}
	}

	ut_free(index_ids);
	ut_free(counts);

	ut_a(buf_pool_validate_instance(buf_pool));
}

/*********************************************************************//**
Prints info of the buffer buf_pool data structure. */
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
@return number of latched pages */
ulint
buf_get_latched_pages_number_instance(
/*==================================*/
	buf_pool_t*	buf_pool)	/*!< in: buffer pool instance */
{
	buf_page_t*	b;
	ulint		i;
	buf_chunk_t*	chunk;
	ulint		fixed_pages_number = 0;

	buf_pool_mutex_enter(buf_pool);

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

			buf_page_mutex_enter(block);

			if (block->page.buf_fix_count != 0
			    || buf_page_get_io_fix(&block->page)
			    != BUF_IO_NONE) {
				fixed_pages_number++;
			}

			buf_page_mutex_exit(block);
		}
	}

	mutex_enter(&buf_pool->zip_mutex);

	/* Traverse the lists of clean and dirty compressed-only blocks. */

	for (b = UT_LIST_GET_FIRST(buf_pool->zip_clean); b;
	     b = UT_LIST_GET_NEXT(list, b)) {
		ut_a(buf_page_get_state(b) == BUF_BLOCK_ZIP_PAGE);
		ut_a(buf_page_get_io_fix(b) != BUF_IO_WRITE);

		if (b->buf_fix_count != 0
		    || buf_page_get_io_fix(b) != BUF_IO_NONE) {
			fixed_pages_number++;
		}
	}

	buf_flush_list_mutex_enter(buf_pool);
	for (b = UT_LIST_GET_FIRST(buf_pool->flush_list); b;
	     b = UT_LIST_GET_NEXT(list, b)) {
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
		case BUF_BLOCK_POOL_WATCH:
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
	buf_pool_mutex_exit(buf_pool);

	return(fixed_pages_number);
}

/*********************************************************************//**
Returns the number of latched pages in all the buffer pools.
@return number of latched pages */
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
Returns the number of pending buf pool read ios.
@return number of pending read I/O operations */
ulint
buf_get_n_pending_read_ios(void)
/*============================*/
{
	ulint	pend_ios = 0;

	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		pend_ios += buf_pool_from_array(i)->n_pend_reads;
	}

	return(pend_ios);
}

/*********************************************************************//**
Returns the ratio in percents of modified pages in the buffer pool /
database pages in the buffer pool.
@return modified page percentage ratio */
double
buf_get_modified_ratio_pct(void)
/*============================*/
{
	double		ratio;
	ulint		lru_len = 0;
	ulint		free_len = 0;
	ulint		flush_list_len = 0;

	buf_get_total_list_len(&lru_len, &free_len, &flush_list_len);

	ratio = static_cast<double>(100 * flush_list_len)
		/ (1 + lru_len + free_len);

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
	total_info->lru_len += pool_info->lru_len;
	total_info->old_lru_len += pool_info->old_lru_len;
	total_info->free_list_len += pool_info->free_list_len;
	total_info->flush_list_len += pool_info->flush_list_len;
	total_info->n_pend_unzip += pool_info->n_pend_unzip;
	total_info->n_pend_reads += pool_info->n_pend_reads;
	total_info->n_pending_flush_lru += pool_info->n_pending_flush_lru;
	total_info->n_pending_flush_list += pool_info->n_pending_flush_list;
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
void
buf_stats_get_pool_info(
/*====================*/
	buf_pool_t*		buf_pool,	/*!< in: buffer pool */
	ulint			pool_id,	/*!< in: buffer pool ID */
	buf_pool_info_t*	all_pool_info)	/*!< in/out: buffer pool info
						to fill */
{
	buf_pool_info_t*	pool_info;
	time_t			current_time;
	double			time_elapsed;

	/* Find appropriate pool_info to store stats for this buffer pool */
	pool_info = &all_pool_info[pool_id];

	buf_pool_mutex_enter(buf_pool);
	buf_flush_list_mutex_enter(buf_pool);

	pool_info->pool_unique_id = pool_id;

	pool_info->pool_size = buf_pool->curr_size;

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
		 (buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE]
		  + buf_pool->init_flush[BUF_FLUSH_SINGLE_PAGE]);

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
	buf_pool_mutex_exit(buf_pool);
}

/*********************************************************************//**
Prints info of the buffer i/o. */
void
buf_print_io_instance(
/*==================*/
	buf_pool_info_t*pool_info,	/*!< in: buffer pool info */
	FILE*		file)		/*!< in/out: buffer where to print */
{
	ut_ad(pool_info);

	fprintf(file,
		"Buffer pool size   " ULINTPF "\n"
		"Free buffers       " ULINTPF "\n"
		"Database pages     " ULINTPF "\n"
		"Old database pages " ULINTPF "\n"
		"Modified db pages  " ULINTPF "\n"
		"Pending reads      " ULINTPF "\n"
		"Pending writes: LRU " ULINTPF
		", flush list " ULINTPF
		", single page " ULINTPF "\n",
		pool_info->pool_size,
		pool_info->free_list_len,
		pool_info->lru_len,
		pool_info->old_lru_len,
		pool_info->flush_list_len,
		pool_info->n_pend_reads,
		pool_info->n_pending_flush_lru,
		pool_info->n_pending_flush_list,
		pool_info->n_pending_flush_single_page);

	fprintf(file,
		"Pages made young " ULINTPF
		", not young " ULINTPF "\n"
		"%.2f youngs/s, %.2f non-youngs/s\n"
		"Pages read " ULINTPF
		", created " ULINTPF
		", written " ULINTPF "\n"
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
		"LRU len: " ULINTPF ", unzip_LRU len: " ULINTPF "\n"
		"I/O sum[" ULINTPF "]:cur[" ULINTPF "], "
		"unzip sum[" ULINTPF "]:cur[" ULINTPF "]\n",
		pool_info->lru_len, pool_info->unzip_lru_len,
		pool_info->io_sum, pool_info->io_cur,
		pool_info->unzip_sum, pool_info->unzip_cur);
}

/*********************************************************************//**
Prints info of the buffer i/o. */
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
		pool_info = (buf_pool_info_t*) ut_zalloc_nokey((
			srv_buf_pool_instances + 1) * sizeof *pool_info);

		pool_info_total = &pool_info[srv_buf_pool_instances];
	} else {
		ut_a(srv_buf_pool_instances == 1);

		pool_info_total = pool_info =
			static_cast<buf_pool_info_t*>(
				ut_zalloc_nokey(sizeof *pool_info));
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
			fprintf(file, "---BUFFER POOL " ULINTPF "\n", i);
			buf_print_io_instance(&pool_info[i], file);
		}
	}

	ut_free(pool_info);
}

/**********************************************************************//**
Refreshes the statistics used to print per-second averages. */
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
void
buf_refresh_io_stats_all(void)
/*==========================*/
{
	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
		buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		buf_refresh_io_stats(buf_pool);
	}
}

/**********************************************************************//**
Check if all pages in all buffer pools are in a replacable state.
@return FALSE if not */
ibool
buf_all_freed(void)
/*===============*/
{
	for (ulint i = 0; i < srv_buf_pool_instances; i++) {
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
@return number of pending i/o */
ulint
buf_pool_check_no_pending_io(void)
/*==============================*/
{
	ulint		i;
	ulint		pending_io = 0;

	buf_pool_mutex_enter_all();

	for (i = 0; i < srv_buf_pool_instances; i++) {
		const buf_pool_t*	buf_pool;

		buf_pool = buf_pool_from_array(i);

		pending_io += buf_pool->n_pend_reads
			      + buf_pool->n_flush[BUF_FLUSH_LRU]
			      + buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE]
			      + buf_pool->n_flush[BUF_FLUSH_LIST];

	}

	buf_pool_mutex_exit_all();

	return(pending_io);
}

#if 0
Code currently not used
/*********************************************************************//**
Gets the current length of the free list of buffer blocks.
@return length of the free list */
ulint
buf_get_free_list_len(void)
/*=======================*/
{
	ulint	len;

	buf_pool_mutex_enter(buf_pool);

	len = UT_LIST_GET_LEN(buf_pool->free);

	buf_pool_mutex_exit(buf_pool);

	return(len);
}
#endif

#else /* !UNIV_HOTBACKUP */

/** Inits a page to the buffer buf_pool, for use in mysqlbackup --restore.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in,out]	block		block to init */
void
buf_page_init_for_backup_restore(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	buf_block_t*		block)
{
	block->page.state = BUF_BLOCK_FILE_PAGE;
	block->page.id = page_id;
	block->page.size.copy_from(page_size);

	page_zip_des_init(&block->page.zip);

	/* We assume that block->page.data has been allocated
	with page_size == univ_page_size. */
	if (page_size.is_compressed()) {
		page_zip_set_size(&block->page.zip, page_size.physical());
		block->page.zip.data = block->frame + page_size.logical();
	} else {
		page_zip_set_size(&block->page.zip, 0);
	}
}

#endif /* !UNIV_HOTBACKUP */

/** Print the given page_id_t object.
@param[in,out]	out	the output stream
@param[in]	page_id	the page_id_t object to be printed
@return the output stream */
std::ostream&
operator<<(
	std::ostream&		out,
	const page_id_t&	page_id)
{
	out << "[page id: space=" << page_id.m_space
		<< ", page number=" << page_id.m_page_no << "]";
	return(out);
}

/** Print the given buf_pool_t object.
@param[in,out]	out		the output stream
@param[in]	buf_pool	the buf_pool_t object to be printed
@return the output stream */
std::ostream&
operator<<(
	std::ostream&		out,
	const buf_pool_t&	buf_pool)
{
	out << "[buffer pool instance: "
		<< "buf_pool size=" << buf_pool.curr_size
		<< ", database pages=" << UT_LIST_GET_LEN(buf_pool.LRU)
		<< ", free pages=" << UT_LIST_GET_LEN(buf_pool.free)
		<< ", modified database pages="
		<< UT_LIST_GET_LEN(buf_pool.flush_list)
		<< ", n pending decompressions=" << buf_pool.n_pend_unzip
		<< ", n pending reads=" << buf_pool.n_pend_reads
		<< ", n pending flush LRU=" << buf_pool.n_flush[BUF_FLUSH_LRU]
		<< " list=" << buf_pool.n_flush[BUF_FLUSH_LIST]
		<< " single page=" << buf_pool.n_flush[BUF_FLUSH_SINGLE_PAGE]
		<< ", pages made young=" << buf_pool.stat.n_pages_made_young
		<< ", not young=" << buf_pool.stat.n_pages_not_made_young
		<< ", pages read=" << buf_pool.stat.n_pages_read
		<< ", created=" << buf_pool.stat.n_pages_created
		<< ", written=" << buf_pool.stat.n_pages_written << "]";
	return(out);
}
#endif /* !UNIV_INNOCHECKSUM */
