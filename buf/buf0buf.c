/*   Innobase relational database engine; Copyright (C) 2001 Innobase Oy
     
     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License 2
     as published by the Free Software Foundation in June 1991.
     
     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.
     
     You should have received a copy of the GNU General Public License 2
     along with this program (in file COPYING); if not, write to the Free
     Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */
/******************************************************
The database buffer buf_pool

(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#include "buf0buf.h"

#ifdef UNIV_NONINL
#include "buf0buf.ic"
#endif

#include "mem0mem.h"
#include "btr0btr.h"
#include "fil0fil.h"
#include "lock0lock.h"
#include "btr0sea.h"
#include "ibuf0ibuf.h"
#include "dict0dict.h"
#include "log0recv.h"
#include "log0log.h"
#include "trx0undo.h"
#include "srv0srv.h"
#include "page0zip.h"

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
These locks can be locked and unlocked without owning the buf_pool mutex.
The OS events in the buf_pool struct can be waited for without owning the
buf_pool mutex.

The buf_pool mutex is a hot-spot in main memory, causing a lot of
memory bus traffic on multiprocessor systems when processors
alternately access the mutex. On our Pentium, the mutex is accessed
maybe every 10 microseconds. We gave up the solution to have mutexes
for each control block, for instance, because it seemed to be
complicated.

A solution to reduce mutex contention of the buf_pool mutex is to
create a separate mutex for the page hash table. On Pentium,
accessing the hash table takes 2 microseconds, about half
of the total buf_pool mutex hold time.

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

There are several lists of control blocks. The free list contains
blocks which are currently not used.

The LRU-list contains all the blocks holding a file page
except those for which the bufferfix count is non-zero.
The pages are in the LRU list roughly in the order of the last
access to the page, so that the oldest pages are at the end of the
list. We also keep a pointer to near the end of the LRU list,
which we can use when we want to artificially age a page in the
buf_pool. This is used if we know that some page is not needed
again for some time: we insert the block right after the pointer,
causing it to be replaced sooner than would noramlly be the case.
Currently this aging mechanism is used for read-ahead mechanism
of pages, and it can also be used when there is a scan of a full
table which cannot fit in the memory. Putting the pages near the
of the LRU list, we make sure that most of the buf_pool stays in the
main memory, undisturbed.

The chain of modified blocks contains the blocks
holding file pages that have been modified in the memory
but not written to disk yet. The block with the oldest modification
which has not yet been written to disk is at the end of the chain.

		Loading a file page
		-------------------

First, a victim block for replacement has to be found in the
buf_pool. It is taken from the free list or searched for from the
end of the LRU-list. An exclusive lock is reserved for the frame,
the io_fix field is set in the block fixing the block in buf_pool,
and the io-operation for loading the page is queued. The io-handler thread
releases the X-lock on the frame and resets the io_fix field
when the io operation completes.

A thread may request the above operation using the buf_page_get-
function. It may then continue to request a lock on the frame.
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

		AWE implementation
		------------------

By a 'block' we mean the buffer header of type buf_block_t. By a 'page'
we mean the physical 16 kB memory area allocated from RAM for that block.
By a 'frame' we mean a 16 kB area in the virtual address space of the
process, in the frame_mem of buf_pool.

We can map pages to the frames of the buffer pool.

1) A buffer block allocated to use as a non-data page, e.g., to the lock
table, is always mapped to a frame.
2) A bufferfixed or io-fixed data page is always mapped to a frame.
3) When we need to map a block to frame, we look from the list
awe_LRU_free_mapped and try to unmap its last block, but note that
bufferfixed or io-fixed pages cannot be unmapped.
4) For every frame in the buffer pool there is always a block whose page is
mapped to it. When we create the buffer pool, we map the first elements
in the free list to the frames.
5) When we have AWE enabled, we disable adaptive hash indexes.
*/

buf_pool_t*	buf_pool = NULL; /* The buffer buf_pool of the database */

#ifdef UNIV_DEBUG
ulint		buf_dbg_counter	= 0; /* This is used to insert validation
					operations in excution in the
					debug version */
ibool		buf_debug_prints = FALSE; /* If this is set TRUE,
					the program prints info whenever
					read-ahead or flush occurs */
#endif /* UNIV_DEBUG */
/************************************************************************
Calculates a page checksum which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value on
32-bit and 64-bit architectures. */

ulint
buf_calc_page_new_checksum(
/*=======================*/
		       /* out: checksum */
	byte*    page) /* in: buffer page */
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

/************************************************************************
In versions < 4.0.14 and < 4.1.1 there was a bug that the checksum only
looked at the first few bytes of the page. This calculates that old
checksum. 
NOTE: we must first store the new formula checksum to
FIL_PAGE_SPACE_OR_CHKSUM before calculating and storing this old checksum
because this takes that field as an input! */

ulint
buf_calc_page_old_checksum(
/*=======================*/
		       /* out: checksum */
	byte*    page) /* in: buffer page */
{
  	ulint checksum;
  	
  	checksum = ut_fold_binary(page, FIL_PAGE_FILE_FLUSH_LSN);

  	checksum = checksum & 0xFFFFFFFFUL;

  	return(checksum);
}

/************************************************************************
Checks if a page is corrupt. */

ibool
buf_page_is_corrupted(
/*==================*/
				/* out: TRUE if corrupted */
	byte*	read_buf)	/* in: a database page */
{
	ulint	checksum;
	ulint	old_checksum;
	ulint	checksum_field;
	ulint	old_checksum_field;
#ifndef UNIV_HOTBACKUP
	dulint	current_lsn;
#endif
	if (mach_read_from_4(read_buf + FIL_PAGE_LSN + 4)
	     != mach_read_from_4(read_buf + UNIV_PAGE_SIZE
				- FIL_PAGE_END_LSN_OLD_CHKSUM + 4)) {

		/* Stored log sequence numbers at the start and the end
		of page do not match */

		return(TRUE);
	}

#ifndef UNIV_HOTBACKUP
	if (recv_lsn_checks_on && log_peek_lsn(&current_lsn)) {
		if (ut_dulint_cmp(current_lsn,
				  mach_read_from_8(read_buf + FIL_PAGE_LSN))
				 < 0) {
			ut_print_timestamp(stderr);

			fprintf(stderr,
"  InnoDB: Error: page %lu log sequence number %lu %lu\n"
"InnoDB: is in the future! Current system log sequence number %lu %lu.\n"
"InnoDB: Your database may be corrupt or you may have copied the InnoDB\n"
"InnoDB: tablespace but not the InnoDB log files. See\n"
"http://dev.mysql.com/doc/mysql/en/backing-up.html for more information.\n",
		        (ulong) mach_read_from_4(read_buf + FIL_PAGE_OFFSET),
			(ulong) ut_dulint_get_high(
				mach_read_from_8(read_buf + FIL_PAGE_LSN)),
			(ulong) ut_dulint_get_low(
				mach_read_from_8(read_buf + FIL_PAGE_LSN)),
			(ulong) ut_dulint_get_high(current_lsn),
			(ulong) ut_dulint_get_low(current_lsn));
		}
	}
#endif
  
  /* If we use checksums validation, make additional check before returning
  TRUE to ensure that the checksum is not equal to BUF_NO_CHECKSUM_MAGIC which
  might be stored by InnoDB with checksums disabled.
     Otherwise, skip checksum calculation and return FALSE */
  
  if (srv_use_checksums) {
    old_checksum = buf_calc_page_old_checksum(read_buf); 

    old_checksum_field = mach_read_from_4(read_buf + UNIV_PAGE_SIZE
					- FIL_PAGE_END_LSN_OLD_CHKSUM);

    /* There are 2 valid formulas for old_checksum_field:
	  1. Very old versions of InnoDB only stored 8 byte lsn to the start
	  and the end of the page.
	  2. Newer InnoDB versions store the old formula checksum there. */
	
    if (old_checksum_field != mach_read_from_4(read_buf + FIL_PAGE_LSN)
        && old_checksum_field != old_checksum
        && old_checksum_field != BUF_NO_CHECKSUM_MAGIC) {

      return(TRUE);
    }

    checksum = buf_calc_page_new_checksum(read_buf);
    checksum_field = mach_read_from_4(read_buf + FIL_PAGE_SPACE_OR_CHKSUM);

    /* InnoDB versions < 4.0.14 and < 4.1.1 stored the space id
	  (always equal to 0), to FIL_PAGE_SPACE_SPACE_OR_CHKSUM */

    if (checksum_field != 0 && checksum_field != checksum
        && checksum_field != BUF_NO_CHECKSUM_MAGIC) {

      return(TRUE);
    }
  }
  
	return(FALSE);
}

/************************************************************************
Prints a page to stderr. */

void
buf_page_print(
/*===========*/
	byte*	read_buf)	/* in: a database page */
{
	dict_index_t*	index;
	ulint		checksum;
	ulint		old_checksum;

	ut_print_timestamp(stderr);
	fprintf(stderr, "  InnoDB: Page dump in ascii and hex (%lu bytes):\n",
		(ulint)UNIV_PAGE_SIZE);
	ut_print_buf(stderr, read_buf, UNIV_PAGE_SIZE);
	fputs("InnoDB: End of page dump\n", stderr);

	checksum = srv_use_checksums ?
    buf_calc_page_new_checksum(read_buf) : BUF_NO_CHECKSUM_MAGIC;
	old_checksum = srv_use_checksums ?
    buf_calc_page_old_checksum(read_buf) : BUF_NO_CHECKSUM_MAGIC;

	ut_print_timestamp(stderr);
	fprintf(stderr, 
"  InnoDB: Page checksum %lu, prior-to-4.0.14-form checksum %lu\n"
"InnoDB: stored checksum %lu, prior-to-4.0.14-form stored checksum %lu\n",
			(ulong) checksum, (ulong) old_checksum,
			(ulong) mach_read_from_4(read_buf + FIL_PAGE_SPACE_OR_CHKSUM),
			(ulong) mach_read_from_4(read_buf + UNIV_PAGE_SIZE
					- FIL_PAGE_END_LSN_OLD_CHKSUM));
	fprintf(stderr,
"InnoDB: Page lsn %lu %lu, low 4 bytes of lsn at page end %lu\n"
"InnoDB: Page number (if stored to page already) %lu,\n"
"InnoDB: space id (if created with >= MySQL-4.1.1 and stored already) %lu\n",
		(ulong) mach_read_from_4(read_buf + FIL_PAGE_LSN),
		(ulong) mach_read_from_4(read_buf + FIL_PAGE_LSN + 4),
		(ulong) mach_read_from_4(read_buf + UNIV_PAGE_SIZE
					- FIL_PAGE_END_LSN_OLD_CHKSUM + 4),
		(ulong) mach_read_from_4(read_buf + FIL_PAGE_OFFSET),
		(ulong) mach_read_from_4(read_buf + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID));

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

	if (fil_page_get_type(read_buf) == FIL_PAGE_INDEX) {
	    	fprintf(stderr,
"InnoDB: Page may be an index page where index id is %lu %lu\n",
			(ulong) ut_dulint_get_high(btr_page_get_index_id(read_buf)),
			(ulong) ut_dulint_get_low(btr_page_get_index_id(read_buf)));

		/* If the code is in ibbackup, dict_sys may be uninitialized,
		i.e., NULL */

		if (dict_sys != NULL) {

		        index = dict_index_find_on_id_low(
					btr_page_get_index_id(read_buf));
		        if (index) {
				fputs("InnoDB: (", stderr);
				dict_index_name_print(stderr, NULL, index);
				fputs(")\n", stderr);
			}
		}
	} else if (fil_page_get_type(read_buf) == FIL_PAGE_INODE) {
		fputs("InnoDB: Page may be an 'inode' page\n", stderr);
	} else if (fil_page_get_type(read_buf) == FIL_PAGE_IBUF_FREE_LIST) {
		fputs("InnoDB: Page may be an insert buffer free list page\n",
			stderr);
	}
}

/************************************************************************
Initializes a buffer control block when the buf_pool is created. */
static
void
buf_block_init(
/*===========*/
	buf_block_t*	block,	/* in: pointer to control block */
	byte*		frame)	/* in: pointer to buffer frame, or NULL if in
				the case of AWE there is no frame */
{
	block->state = BUF_BLOCK_NOT_USED;
	
	block->frame = frame;

	block->awe_info = NULL;

	block->modify_clock = ut_dulint_zero;
	
	block->file_page_was_freed = FALSE;

	block->check_index_page_at_flush = FALSE;
	block->index = NULL;

	block->in_free_list = FALSE;
	block->in_LRU_list = FALSE;

	block->n_pointers = 0;

	page_zip_des_init(&block->page_zip);

	rw_lock_create(&(block->lock));
	ut_ad(rw_lock_validate(&(block->lock)));

#ifdef UNIV_SYNC_DEBUG
	rw_lock_create(&(block->debug_latch));
	rw_lock_set_level(&(block->debug_latch), SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */
}

/************************************************************************
Creates the buffer pool. */

buf_pool_t*
buf_pool_init(
/*==========*/
				/* out, own: buf_pool object, NULL if not
				enough memory or error */
	ulint	max_size,	/* in: maximum size of the buf_pool in
				blocks */
	ulint	curr_size,	/* in: current size to use, must be <=
				max_size, currently must be equal to
				max_size */
	ulint	n_frames)	/* in: number of frames; if AWE is used,
				this is the size of the address space window
				where physical memory pages are mapped; if
				AWE is not used then this must be the same
				as max_size */
{
	byte*		frame;
	ulint		i;
	buf_block_t*	block;
	
	ut_a(max_size == curr_size);
	ut_a(srv_use_awe || n_frames == max_size);
	
	if (n_frames > curr_size) {
	        fprintf(stderr,
"InnoDB: AWE: Error: you must specify in my.cnf .._awe_mem_mb larger\n"
"InnoDB: than .._buffer_pool_size. Now the former is %lu pages,\n"
"InnoDB: the latter %lu pages.\n", (ulong) curr_size, (ulong) n_frames);

		return(NULL);
	}

	buf_pool = mem_alloc(sizeof(buf_pool_t));

	/* 1. Initialize general fields
	   ---------------------------- */
	mutex_create(&(buf_pool->mutex));
	mutex_set_level(&(buf_pool->mutex), SYNC_BUF_POOL);

	mutex_enter(&(buf_pool->mutex));

	if (srv_use_awe) {
		/*----------------------------------------*/
		/* Allocate the virtual address space window, i.e., the
		buffer pool frames */

		buf_pool->frame_mem = os_awe_allocate_virtual_mem_window(
					UNIV_PAGE_SIZE * (n_frames + 1));
					
		/* Allocate the physical memory for AWE and the AWE info array
		for buf_pool */

		if ((curr_size % ((1024 * 1024) / UNIV_PAGE_SIZE)) != 0) {

		        fprintf(stderr,
"InnoDB: AWE: Error: physical memory must be allocated in full megabytes.\n"
"InnoDB: Trying to allocate %lu database pages.\n", 
			  (ulong) curr_size);

		        return(NULL);
		}

		if (!os_awe_allocate_physical_mem(&(buf_pool->awe_info),
			curr_size / ((1024 * 1024) / UNIV_PAGE_SIZE))) {

			return(NULL);
		}
		/*----------------------------------------*/
	} else {
		buf_pool->frame_mem = os_mem_alloc_large(
					UNIV_PAGE_SIZE * (n_frames + 1),
					TRUE, FALSE);
	}

	if (buf_pool->frame_mem == NULL) {

		return(NULL);
	}

	buf_pool->blocks = ut_malloc(sizeof(buf_block_t) * max_size);

	if (buf_pool->blocks == NULL) {

		return(NULL);
	}

	buf_pool->max_size = max_size;
	buf_pool->curr_size = curr_size;

	buf_pool->n_frames = n_frames;

	/* Align pointer to the first frame */

	frame = ut_align(buf_pool->frame_mem, UNIV_PAGE_SIZE);

	buf_pool->frame_zero = frame;
	buf_pool->high_end = frame + UNIV_PAGE_SIZE * n_frames;

	if (srv_use_awe) {
		/*----------------------------------------*/
		/* Map an initial part of the allocated physical memory to
		the window */

		os_awe_map_physical_mem_to_window(buf_pool->frame_zero,
				n_frames *
				(UNIV_PAGE_SIZE / OS_AWE_X86_PAGE_SIZE),
					buf_pool->awe_info);
		/*----------------------------------------*/
	}

	buf_pool->blocks_of_frames = ut_malloc(sizeof(void*) * n_frames);
	
	if (buf_pool->blocks_of_frames == NULL) {

		return(NULL);
	}

	/* Init block structs and assign frames for them; in the case of
	AWE there are less frames than blocks. Then we assign the frames
	to the first blocks (we already mapped the memory above). We also
	init the awe_info for every block. */

	for (i = 0; i < max_size; i++) {

		block = buf_pool_get_nth_block(buf_pool, i);

		if (i < n_frames) {
			frame = buf_pool->frame_zero + i * UNIV_PAGE_SIZE;
			*(buf_pool->blocks_of_frames + i) = block;
		} else {
			frame = NULL;
		}
		
		buf_block_init(block, frame);

		if (srv_use_awe) {
			/*----------------------------------------*/
			block->awe_info = buf_pool->awe_info
				+ i * (UNIV_PAGE_SIZE / OS_AWE_X86_PAGE_SIZE);
			/*----------------------------------------*/
		}
	}

	buf_pool->page_hash = hash_create(2 * max_size);

	buf_pool->n_pend_reads = 0;

	buf_pool->last_printout_time = time(NULL);

	buf_pool->n_pages_read = 0;
	buf_pool->n_pages_written = 0;
	buf_pool->n_pages_created = 0;
	buf_pool->n_pages_awe_remapped = 0;
	
	buf_pool->n_page_gets = 0;
	buf_pool->n_page_gets_old = 0;
	buf_pool->n_pages_read_old = 0;
	buf_pool->n_pages_written_old = 0;
	buf_pool->n_pages_created_old = 0;
	buf_pool->n_pages_awe_remapped_old = 0;
	
	/* 2. Initialize flushing fields
	   ---------------------------- */
	UT_LIST_INIT(buf_pool->flush_list);

	for (i = BUF_FLUSH_LRU; i <= BUF_FLUSH_LIST; i++) {
		buf_pool->n_flush[i] = 0;
		buf_pool->init_flush[i] = FALSE;
		buf_pool->no_flush[i] = os_event_create(NULL);
	}

	buf_pool->LRU_flush_ended = 0;

	buf_pool->ulint_clock = 1;
	buf_pool->freed_page_clock = 0;
	
	/* 3. Initialize LRU fields
	   ---------------------------- */
	UT_LIST_INIT(buf_pool->LRU);

	buf_pool->LRU_old = NULL;

	UT_LIST_INIT(buf_pool->awe_LRU_free_mapped);

	/* Add control blocks to the free list */
	UT_LIST_INIT(buf_pool->free);

	for (i = 0; i < curr_size; i++) {

		block = buf_pool_get_nth_block(buf_pool, i);

		if (block->frame) {
			/* Wipe contents of frame to eliminate a Purify
			warning */

#ifdef HAVE_purify
			memset(block->frame, '\0', UNIV_PAGE_SIZE);
#endif
			if (srv_use_awe) {
				/* Add to the list of blocks mapped to
				frames */
				
				UT_LIST_ADD_LAST(awe_LRU_free_mapped,
					buf_pool->awe_LRU_free_mapped, block);
			}
		}

		UT_LIST_ADD_LAST(free, buf_pool->free, block);
		block->in_free_list = TRUE;
	}

	mutex_exit(&(buf_pool->mutex));

	if (srv_use_adaptive_hash_indexes) {
	  	btr_search_sys_create(
			  curr_size * UNIV_PAGE_SIZE / sizeof(void*) / 64);
	} else {
	        /* Create only a small dummy system */
	        btr_search_sys_create(1000);
	}

	return(buf_pool);
}	

/************************************************************************
Maps the page of block to a frame, if not mapped yet. Unmaps some page
from the end of the awe_LRU_free_mapped. */

void
buf_awe_map_page_to_frame(
/*======================*/
	buf_block_t*	block,		/* in: block whose page should be
					mapped to a frame */
	ibool		add_to_mapped_list) /* in: TRUE if we in the case
					we need to map the page should also
					add the block to the
					awe_LRU_free_mapped list */
{
	buf_block_t*	bck;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(buf_pool->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(block);

	if (block->frame) {

		return;
	}

	/* Scan awe_LRU_free_mapped from the end and try to find a block
	which is not bufferfixed or io-fixed */

	bck = UT_LIST_GET_LAST(buf_pool->awe_LRU_free_mapped);

	while (bck) {	
		if (bck->state == BUF_BLOCK_FILE_PAGE
	    	    && (bck->buf_fix_count != 0 || bck->io_fix != 0)) {

			/* We have to skip this */
			bck = UT_LIST_GET_PREV(awe_LRU_free_mapped, bck);
		} else {
			/* We can map block to the frame of bck */

			os_awe_map_physical_mem_to_window(
				bck->frame,
				UNIV_PAGE_SIZE / OS_AWE_X86_PAGE_SIZE,
				block->awe_info);

			block->frame = bck->frame;

			*(buf_pool->blocks_of_frames
				+ (((ulint)(block->frame
						- buf_pool->frame_zero))
						>> UNIV_PAGE_SIZE_SHIFT))
				= block;
			
			bck->frame = NULL;
			UT_LIST_REMOVE(awe_LRU_free_mapped,
					buf_pool->awe_LRU_free_mapped,
					bck);

			if (add_to_mapped_list) {
				UT_LIST_ADD_FIRST(awe_LRU_free_mapped,
					buf_pool->awe_LRU_free_mapped,
					block);
			}

			buf_pool->n_pages_awe_remapped++;
			
			return;
		}
	}

	fprintf(stderr,
"InnoDB: AWE: Fatal error: cannot find a page to unmap\n"
"InnoDB: awe_LRU_free_mapped list length %lu\n",
		(ulong) UT_LIST_GET_LEN(buf_pool->awe_LRU_free_mapped));

	ut_a(0);
}

/************************************************************************
Allocates a buffer block. */
UNIV_INLINE
buf_block_t*
buf_block_alloc(void)
/*=================*/
				/* out, own: the allocated block; also if AWE
				is used it is guaranteed that the page is
				mapped to a frame */
{
	buf_block_t*	block;

	block = buf_LRU_get_free_block();

	return(block);
}

/************************************************************************
Moves to the block to the start of the LRU list if there is a danger
that the block would drift out of the buffer pool. */
UNIV_INLINE
void
buf_block_make_young(
/*=================*/
	buf_block_t*	block)	/* in: block to make younger */
{
	if (buf_pool->freed_page_clock >= block->freed_page_clock 
				+ 1 + (buf_pool->curr_size / 1024)) {

		/* There has been freeing activity in the LRU list:
		best to move to the head of the LRU list */

		buf_LRU_make_block_young(block);
	}
}

/************************************************************************
Moves a page to the start of the buffer pool LRU list. This high-level
function can be used to prevent an important page from from slipping out of
the buffer pool. */

void
buf_page_make_young(
/*=================*/
	buf_frame_t*	frame)	/* in: buffer frame of a file page */
{
	buf_block_t*	block;
	
	mutex_enter(&(buf_pool->mutex));

	block = buf_block_align(frame);

	ut_a(block->state == BUF_BLOCK_FILE_PAGE);

	buf_LRU_make_block_young(block);

	mutex_exit(&(buf_pool->mutex));
}

/************************************************************************
Frees a buffer block which does not contain a file page. */
UNIV_INLINE
void
buf_block_free(
/*===========*/
	buf_block_t*	block)	/* in, own: block to be freed */
{
	ut_a(block->state != BUF_BLOCK_FILE_PAGE);

	mutex_enter(&(buf_pool->mutex));

	buf_LRU_block_free_non_file_page(block);

	mutex_exit(&(buf_pool->mutex));
}

/*************************************************************************
Allocates a buffer frame. */

buf_frame_t*
buf_frame_alloc(void)
/*=================*/
				/* out: buffer frame */
{
	return(buf_block_alloc()->frame);
}

/*************************************************************************
Frees a buffer frame which does not contain a file page. */

void
buf_frame_free(
/*===========*/
	buf_frame_t*	frame)	/* in: buffer frame */
{
	buf_block_free(buf_block_align(frame));
}
	
/************************************************************************
Returns the buffer control block if the page can be found in the buffer
pool. NOTE that it is possible that the page is not yet read
from disk, though. This is a very low-level function: use with care! */

buf_block_t*
buf_page_peek_block(
/*================*/
			/* out: control block if found from page hash table,
			otherwise NULL; NOTE that the page is not necessarily
			yet read from disk! */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;

	mutex_enter_fast(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	mutex_exit(&(buf_pool->mutex));

	return(block);
}

/************************************************************************
Resets the check_index_page_at_flush field of a page if found in the buffer
pool. */

void
buf_reset_check_index_page_at_flush(
/*================================*/
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;

	mutex_enter_fast(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	if (block) {
		block->check_index_page_at_flush = FALSE;
	}
	
	mutex_exit(&(buf_pool->mutex));
}

/************************************************************************
Returns the current state of is_hashed of a page. FALSE if the page is
not in the pool. NOTE that this operation does not fix the page in the
pool if it is found there. */

ibool
buf_page_peek_if_search_hashed(
/*===========================*/
			/* out: TRUE if page hash index is built in search
			system */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;
	ibool		is_hashed;

	mutex_enter_fast(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	if (!block) {
		is_hashed = FALSE;
	} else {
		is_hashed = block->is_hashed;
	}

	mutex_exit(&(buf_pool->mutex));

	return(is_hashed);
}

/************************************************************************
Returns TRUE if the page can be found in the buffer pool hash table. NOTE
that it is possible that the page is not yet read from disk, though. */

ibool
buf_page_peek(
/*==========*/
			/* out: TRUE if found from page hash table,
			NOTE that the page is not necessarily yet read
			from disk! */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	if (buf_page_peek_block(space, offset)) {

		return(TRUE);
	}

	return(FALSE);
}

/************************************************************************
Sets file_page_was_freed TRUE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated. */

buf_block_t*
buf_page_set_file_page_was_freed(
/*=============================*/
			/* out: control block if found from page hash table,
			otherwise NULL */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;

	mutex_enter_fast(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	if (block) {
		block->file_page_was_freed = TRUE;
	}

	mutex_exit(&(buf_pool->mutex));

	return(block);
}

/************************************************************************
Sets file_page_was_freed FALSE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated. */

buf_block_t*
buf_page_reset_file_page_was_freed(
/*===============================*/
			/* out: control block if found from page hash table,
			otherwise NULL */
	ulint	space,	/* in: space id */
	ulint	offset)	/* in: page number */
{
	buf_block_t*	block;

	mutex_enter_fast(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	if (block) {
		block->file_page_was_freed = FALSE;
	}

	mutex_exit(&(buf_pool->mutex));

	return(block);
}

/************************************************************************
This is the general function used to get access to a database page. */

buf_frame_t*
buf_page_get_gen(
/*=============*/
				/* out: pointer to the frame or NULL */
	ulint		space,	/* in: space id */
	ulint		offset,	/* in: page number */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH, RW_NO_LATCH */
	buf_frame_t*	guess,	/* in: guessed frame or NULL */
	ulint		mode,	/* in: BUF_GET, BUF_GET_IF_IN_POOL,
				BUF_GET_NO_LATCH, BUF_GET_NOWAIT */
	const char*	file,	/* in: file name */
	ulint		line,	/* in: line where called */
	mtr_t*		mtr)	/* in: mini-transaction */
{
	buf_block_t*	block;
	ibool		accessed;
	ulint		fix_type;
	ibool		success;
	ibool		must_read;
	
	ut_ad(mtr);
	ut_ad((rw_latch == RW_S_LATCH)
	      || (rw_latch == RW_X_LATCH)
	      || (rw_latch == RW_NO_LATCH));
	ut_ad((mode != BUF_GET_NO_LATCH) || (rw_latch == RW_NO_LATCH));
	ut_ad((mode == BUF_GET) || (mode == BUF_GET_IF_IN_POOL)
	      || (mode == BUF_GET_NO_LATCH) || (mode == BUF_GET_NOWAIT));
#ifndef UNIV_LOG_DEBUG
	ut_ad(!ibuf_inside() || ibuf_page(space, offset));
#endif
	buf_pool->n_page_gets++;
loop:
	mutex_enter_fast(&(buf_pool->mutex));

	block = NULL;
	
	if (guess) {
		block = buf_block_align(guess);

		if ((offset != block->offset) || (space != block->space)
				|| (block->state != BUF_BLOCK_FILE_PAGE)) {

			block = NULL;
		}
	}

	if (block == NULL) {
		block = buf_page_hash_get(space, offset);
	}

	if (block == NULL) {
		/* Page not in buf_pool: needs to be read from file */

		mutex_exit(&(buf_pool->mutex));

		if (mode == BUF_GET_IF_IN_POOL) {

			return(NULL);
		}

		buf_read_page(space, offset);

#ifdef UNIV_DEBUG
		buf_dbg_counter++;

		if (buf_dbg_counter % 37 == 0) {
			ut_ad(buf_validate());
		}
#endif
		goto loop;
	}

	ut_a(block->state == BUF_BLOCK_FILE_PAGE);

	must_read = FALSE;
	
	if (block->io_fix == BUF_IO_READ) {

		must_read = TRUE;

		if (mode == BUF_GET_IF_IN_POOL) {

			/* The page is only being read to buffer */
			mutex_exit(&(buf_pool->mutex));

			return(NULL);
		}
	}		

	/* If AWE is enabled and the page is not mapped to a frame, then
	map it */

	if (block->frame == NULL) {
		ut_a(srv_use_awe);

		/* We set second parameter TRUE because the block is in the
		LRU list and we must put it to awe_LRU_free_mapped list once
		mapped to a frame */
		
		buf_awe_map_page_to_frame(block, TRUE);
	}
	
#ifdef UNIV_SYNC_DEBUG
	buf_block_buf_fix_inc_debug(block, file, line);
#else
	buf_block_buf_fix_inc(block);
#endif
	buf_block_make_young(block);

	/* Check if this is the first access to the page */

	accessed = block->accessed;

	block->accessed = TRUE;

#ifdef UNIV_DEBUG_FILE_ACCESSES
	ut_a(block->file_page_was_freed == FALSE);
#endif	
	mutex_exit(&(buf_pool->mutex));

#ifdef UNIV_DEBUG
	buf_dbg_counter++;

	if (buf_dbg_counter % 5771 == 0) {
		ut_ad(buf_validate());
	}
#endif
	ut_ad(block->buf_fix_count > 0);
	ut_ad(block->state == BUF_BLOCK_FILE_PAGE);

	if (mode == BUF_GET_NOWAIT) {
		if (rw_latch == RW_S_LATCH) {
			success = rw_lock_s_lock_func_nowait(&(block->lock),
								file, line);
			fix_type = MTR_MEMO_PAGE_S_FIX;
		} else {
			ut_ad(rw_latch == RW_X_LATCH);
			success = rw_lock_x_lock_func_nowait(&(block->lock),
					file, line);
			fix_type = MTR_MEMO_PAGE_X_FIX;
		}

		if (!success) {
			mutex_enter(&(buf_pool->mutex));

			block->buf_fix_count--;
#ifdef UNIV_SYNC_DEBUG
			rw_lock_s_unlock(&(block->debug_latch));
#endif			
			mutex_exit(&(buf_pool->mutex));

			return(NULL);
		}
	} else if (rw_latch == RW_NO_LATCH) {

		if (must_read) {
		        /* Let us wait until the read operation
			completes */

		        for (;;) {
			        mutex_enter(&(buf_pool->mutex));

		                if (block->io_fix == BUF_IO_READ) {

				        mutex_exit(&(buf_pool->mutex));
				  
				        /* Sleep 20 milliseconds */

				        os_thread_sleep(20000);
				} else {
				  
				       mutex_exit(&(buf_pool->mutex));

				       break;
				}
			}
		}

		fix_type = MTR_MEMO_BUF_FIX;
	} else if (rw_latch == RW_S_LATCH) {

		rw_lock_s_lock_func(&(block->lock), 0, file, line);

		fix_type = MTR_MEMO_PAGE_S_FIX;
	} else {
		rw_lock_x_lock_func(&(block->lock), 0, file, line);

		fix_type = MTR_MEMO_PAGE_X_FIX;
	}

	mtr_memo_push(mtr, block, fix_type);

	if (!accessed) {
		/* In the case of a first access, try to apply linear
		read-ahead */

		buf_read_ahead_linear(space, offset);
	}

#ifdef UNIV_IBUF_DEBUG
	ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
	return(block->frame);		
}

/************************************************************************
This is the general function used to get optimistic access to a database
page. */

ibool
buf_page_optimistic_get_func(
/*=========================*/
				/* out: TRUE if success */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH */
	buf_block_t*	block,	/* in: guessed buffer block */
	buf_frame_t*	guess,	/* in: guessed frame; note that AWE may move
				frames */
	dulint		modify_clock,/* in: modify clock value if mode is
				..._GUESS_ON_CLOCK */
	const char*	file,	/* in: file name */
	ulint		line,	/* in: line where called */
	mtr_t*		mtr)	/* in: mini-transaction */
{
	ibool		accessed;
	ibool		success;
	ulint		fix_type;

	ut_ad(mtr && block);
	ut_ad((rw_latch == RW_S_LATCH) || (rw_latch == RW_X_LATCH));
	
	mutex_enter(&(buf_pool->mutex));

	/* If AWE is used, block may have a different frame now, e.g., NULL */
	
	if (UNIV_UNLIKELY(block->state != BUF_BLOCK_FILE_PAGE)
			|| UNIV_UNLIKELY(block->frame != guess)) {
	exit_func:
		mutex_exit(&(buf_pool->mutex));

		return(FALSE);
	}

#ifdef UNIV_SYNC_DEBUG
	buf_block_buf_fix_inc_debug(block, file, line);
#else
	buf_block_buf_fix_inc(block);
#endif
	buf_block_make_young(block);

	/* Check if this is the first access to the page */

	accessed = block->accessed;

	block->accessed = TRUE;

	mutex_exit(&(buf_pool->mutex));

	ut_ad(!ibuf_inside() || ibuf_page(block->space, block->offset));

	if (rw_latch == RW_S_LATCH) {
		success = rw_lock_s_lock_func_nowait(&(block->lock),
								file, line);
		fix_type = MTR_MEMO_PAGE_S_FIX;
	} else {
		success = rw_lock_x_lock_func_nowait(&(block->lock),
								file, line);
		fix_type = MTR_MEMO_PAGE_X_FIX;
	}

	if (UNIV_UNLIKELY(!success)) {
		mutex_enter(&(buf_pool->mutex));
		
		block->buf_fix_count--;
#ifdef UNIV_SYNC_DEBUG
		rw_lock_s_unlock(&(block->debug_latch));
#endif
		goto exit_func;
	}

	if (UNIV_UNLIKELY(!UT_DULINT_EQ(modify_clock, block->modify_clock))) {
#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(block->frame, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */
		if (rw_latch == RW_S_LATCH) {
			rw_lock_s_unlock(&(block->lock));
		} else {
			rw_lock_x_unlock(&(block->lock));
		}

		mutex_enter(&(buf_pool->mutex));
		
		block->buf_fix_count--;
#ifdef UNIV_SYNC_DEBUG
		rw_lock_s_unlock(&(block->debug_latch));
#endif
		goto exit_func;
	}

	mtr_memo_push(mtr, block, fix_type);

#ifdef UNIV_DEBUG
	buf_dbg_counter++;

	if (buf_dbg_counter % 5771 == 0) {
		ut_ad(buf_validate());
	}
#endif
	ut_ad(block->buf_fix_count > 0);
	ut_ad(block->state == BUF_BLOCK_FILE_PAGE);

#ifdef UNIV_DEBUG_FILE_ACCESSES
	ut_a(block->file_page_was_freed == FALSE);
#endif
	if (UNIV_UNLIKELY(!accessed)) {
		/* In the case of a first access, try to apply linear
		read-ahead */

		buf_read_ahead_linear(buf_frame_get_space_id(guess),
					buf_frame_get_page_no(guess));
	}

#ifdef UNIV_IBUF_DEBUG
	ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
	buf_pool->n_page_gets++;

	return(TRUE);
}

/************************************************************************
This is used to get access to a known database page, when no waiting can be
done. For example, if a search in an adaptive hash index leads us to this
frame. */

ibool
buf_page_get_known_nowait(
/*======================*/
				/* out: TRUE if success */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH */
	buf_frame_t*	guess,	/* in: the known page frame */
	ulint		mode,	/* in: BUF_MAKE_YOUNG or BUF_KEEP_OLD */
	const char*	file,	/* in: file name */
	ulint		line,	/* in: line where called */
	mtr_t*		mtr)	/* in: mini-transaction */
{
	buf_block_t*	block;
	ibool		success;
	ulint		fix_type;

	ut_ad(mtr);
	ut_ad((rw_latch == RW_S_LATCH) || (rw_latch == RW_X_LATCH));
	
	mutex_enter(&(buf_pool->mutex));

	block = buf_block_align(guess);

	if (block->state == BUF_BLOCK_REMOVE_HASH) {
	        /* Another thread is just freeing the block from the LRU list
	        of the buffer pool: do not try to access this page; this
		attempt to access the page can only come through the hash
		index because when the buffer block state is ..._REMOVE_HASH,
		we have already removed it from the page address hash table
		of the buffer pool. */

	        mutex_exit(&(buf_pool->mutex));

		return(FALSE);
	}

	ut_a(block->state == BUF_BLOCK_FILE_PAGE);

#ifdef UNIV_SYNC_DEBUG
	buf_block_buf_fix_inc_debug(block, file, line);
#else
	buf_block_buf_fix_inc(block);
#endif
	if (mode == BUF_MAKE_YOUNG) {
		buf_block_make_young(block);
	}

	mutex_exit(&(buf_pool->mutex));

	ut_ad(!ibuf_inside() || (mode == BUF_KEEP_OLD));

	if (rw_latch == RW_S_LATCH) {
		success = rw_lock_s_lock_func_nowait(&(block->lock),
								file, line);
		fix_type = MTR_MEMO_PAGE_S_FIX;
	} else {
		success = rw_lock_x_lock_func_nowait(&(block->lock),
								file, line);
		fix_type = MTR_MEMO_PAGE_X_FIX;
	}
	
	if (!success) {
		mutex_enter(&(buf_pool->mutex));
		
		block->buf_fix_count--;
#ifdef UNIV_SYNC_DEBUG
		rw_lock_s_unlock(&(block->debug_latch));
#endif		
		mutex_exit(&(buf_pool->mutex));

		return(FALSE);
	}

	mtr_memo_push(mtr, block, fix_type);

#ifdef UNIV_DEBUG
	buf_dbg_counter++;

	if (buf_dbg_counter % 5771 == 0) {
		ut_ad(buf_validate());
	}
#endif
	ut_ad(block->buf_fix_count > 0);
	ut_ad(block->state == BUF_BLOCK_FILE_PAGE);
#ifdef UNIV_DEBUG_FILE_ACCESSES
	ut_a(block->file_page_was_freed == FALSE);
#endif

#ifdef UNIV_IBUF_DEBUG
	ut_a((mode == BUF_KEEP_OLD)
		|| (ibuf_count_get(block->space, block->offset) == 0));
#endif
	buf_pool->n_page_gets++;

	return(TRUE);
}

/************************************************************************
Inits a page to the buffer buf_pool, for use in ibbackup --restore. */

void
buf_page_init_for_backup_restore(
/*=============================*/
	ulint		space,	/* in: space id */
	ulint		offset,	/* in: offset of the page within space
				in units of a page */
	buf_block_t*	block)	/* in: block to init */
{
	/* Set the state of the block */
	block->magic_n		= BUF_BLOCK_MAGIC_N;

	block->state 		= BUF_BLOCK_FILE_PAGE;
	block->space 		= space;
	block->offset 		= offset;

	block->lock_hash_val	= 0;
	block->lock_mutex	= NULL;
	
	block->freed_page_clock = 0;

	block->newest_modification = ut_dulint_zero;
	block->oldest_modification = ut_dulint_zero;
	
	block->accessed		= FALSE;
	block->buf_fix_count 	= 0;
	block->io_fix		= 0;

	block->n_hash_helps	= 0;
	block->is_hashed	= FALSE;
	block->n_fields         = 1;
	block->n_bytes          = 0;
	block->side             = BTR_SEARCH_LEFT_SIDE;

	block->file_page_was_freed = FALSE;
}

/************************************************************************
Inits a page to the buffer buf_pool. */
static
void
buf_page_init(
/*==========*/
	ulint		space,	/* in: space id */
	ulint		offset,	/* in: offset of the page within space
				in units of a page */
	buf_block_t*	block)	/* in: block to init */
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(buf_pool->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_a(block->state != BUF_BLOCK_FILE_PAGE);

	/* Set the state of the block */
	block->magic_n		= BUF_BLOCK_MAGIC_N;

	block->state 		= BUF_BLOCK_FILE_PAGE;
	block->space 		= space;
	block->offset 		= offset;

	block->check_index_page_at_flush = FALSE;
	block->index		= NULL;
	
	block->lock_hash_val	= lock_rec_hash(space, offset);
	block->lock_mutex	= NULL;
	
	/* Insert into the hash table of file pages */

        if (buf_page_hash_get(space, offset)) {
                fprintf(stderr,
"InnoDB: Error: page %lu %lu already found from the hash table\n",
			(ulong) space,
			(ulong) offset);
#ifdef UNIV_DEBUG
                buf_print();
                buf_LRU_print();
                buf_validate();
                buf_LRU_validate();
#endif /* UNIV_DEBUG */
                ut_a(0);
        }

	HASH_INSERT(buf_block_t, hash, buf_pool->page_hash,
				buf_page_address_fold(space, offset), block);

	block->freed_page_clock = 0;

	block->newest_modification = ut_dulint_zero;
	block->oldest_modification = ut_dulint_zero;
	
	block->accessed		= FALSE;
	block->buf_fix_count 	= 0;
	block->io_fix		= 0;

	block->n_hash_helps	= 0;
	block->is_hashed	= FALSE;
	block->n_fields         = 1;
	block->n_bytes          = 0;
	block->side             = BTR_SEARCH_LEFT_SIDE;

	block->file_page_was_freed = FALSE;
}

/************************************************************************
Function which inits a page for read to the buffer buf_pool. If the page is
(1) already in buf_pool, or
(2) if we specify to read only ibuf pages and the page is not an ibuf page, or
(3) if the space is deleted or being deleted,
then this function does nothing.
Sets the io_fix flag to BUF_IO_READ and sets a non-recursive exclusive lock
on the buffer frame. The io-handler must take care that the flag is cleared
and the lock released later. This is one of the functions which perform the
state transition NOT_USED => FILE_PAGE to a block (the other is
buf_page_create). */ 

buf_block_t*
buf_page_init_for_read(
/*===================*/
				/* out: pointer to the block or NULL */
	ulint*		err,	/* out: DB_SUCCESS or DB_TABLESPACE_DELETED */
	ulint		mode,	/* in: BUF_READ_IBUF_PAGES_ONLY, ... */
	ulint		space,	/* in: space id */
	ib_longlong	tablespace_version,/* in: prevents reading from a wrong
				version of the tablespace in case we have done
				DISCARD + IMPORT */
	ulint		offset)	/* in: page number */
{
	buf_block_t*	block;
	mtr_t		mtr;

	ut_ad(buf_pool);

	*err = DB_SUCCESS;

	if (mode == BUF_READ_IBUF_PAGES_ONLY) {
		/* It is a read-ahead within an ibuf routine */

		ut_ad(!ibuf_bitmap_page(offset));
		ut_ad(ibuf_inside());
	
		mtr_start(&mtr);
	
		if (!ibuf_page_low(space, offset, &mtr)) {

			mtr_commit(&mtr);

			return(NULL);
		}
	} else {
		ut_ad(mode == BUF_READ_ANY_PAGE);
	}
	
	block = buf_block_alloc();

	ut_a(block);

	mutex_enter(&(buf_pool->mutex));

	if (fil_tablespace_deleted_or_being_deleted_in_mem(space,
							tablespace_version)) {
		*err = DB_TABLESPACE_DELETED;
	}

	if (*err == DB_TABLESPACE_DELETED
	    || NULL != buf_page_hash_get(space, offset)) {

		/* The page belongs to a space which has been deleted or is
		being deleted, or the page is already in buf_pool, return */

		mutex_exit(&(buf_pool->mutex));
		buf_block_free(block);

		if (mode == BUF_READ_IBUF_PAGES_ONLY) {

			mtr_commit(&mtr);
		}

		return(NULL);
	}

	ut_ad(block);
	
	buf_page_init(space, offset, block);

	/* The block must be put to the LRU list, to the old blocks */

	buf_LRU_add_block(block, TRUE); 	/* TRUE == to old blocks */
	
	block->io_fix = BUF_IO_READ;
	buf_pool->n_pend_reads++;
	
	/* We set a pass-type x-lock on the frame because then the same
	thread which called for the read operation (and is running now at
	this point of code) can wait for the read to complete by waiting
	for the x-lock on the frame; if the x-lock were recursive, the
	same thread would illegally get the x-lock before the page read
	is completed. The x-lock is cleared by the io-handler thread. */
	
	rw_lock_x_lock_gen(&(block->lock), BUF_IO_READ);
	
 	mutex_exit(&(buf_pool->mutex));

	if (mode == BUF_READ_IBUF_PAGES_ONLY) {

		mtr_commit(&mtr);
	}

	return(block);
}	

/************************************************************************
Initializes a page to the buffer buf_pool. The page is usually not read
from a file even if it cannot be found in the buffer buf_pool. This is one
of the functions which perform to a block a state transition NOT_USED =>
FILE_PAGE (the other is buf_page_init_for_read above). */

buf_frame_t*
buf_page_create(
/*============*/
			/* out: pointer to the frame, page bufferfixed */
	ulint	space,	/* in: space id */
	ulint	offset,	/* in: offset of the page within space in units of
			a page */
	mtr_t*	mtr)	/* in: mini-transaction handle */
{
	buf_frame_t*	frame;
	buf_block_t*	block;
	buf_block_t*	free_block	= NULL;
	
	ut_ad(mtr);

	free_block = buf_LRU_get_free_block();
	
	mutex_enter(&(buf_pool->mutex));

	block = buf_page_hash_get(space, offset);

	if (block != NULL) {
#ifdef UNIV_IBUF_DEBUG
		ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
		block->file_page_was_freed = FALSE;

		/* Page can be found in buf_pool */
		mutex_exit(&(buf_pool->mutex));

		buf_block_free(free_block);

		frame = buf_page_get_with_no_latch(space, offset, mtr);

		return(frame);
	}

	/* If we get here, the page was not in buf_pool: init it there */

#ifdef UNIV_DEBUG
	if (buf_debug_prints) {
		fprintf(stderr, "Creating space %lu page %lu to buffer\n",
			(ulong) space, (ulong) offset);
	}
#endif /* UNIV_DEBUG */

	block = free_block;
	
	buf_page_init(space, offset, block);

	/* The block must be put to the LRU list */
	buf_LRU_add_block(block, FALSE);
		
#ifdef UNIV_SYNC_DEBUG
	buf_block_buf_fix_inc_debug(block, __FILE__, __LINE__);
#else
	buf_block_buf_fix_inc(block);
#endif
	mtr_memo_push(mtr, block, MTR_MEMO_BUF_FIX);

	block->accessed = TRUE;
	
	buf_pool->n_pages_created++;

	mutex_exit(&(buf_pool->mutex));

	/* Delete possible entries for the page from the insert buffer:
	such can exist if the page belonged to an index which was dropped */

	ibuf_merge_or_delete_for_page(NULL, space, offset, TRUE);

	/* Flush pages from the end of the LRU list if necessary */
	buf_flush_free_margin();

	frame = block->frame;

	/* Reset to zero the file flush lsn field in the page; if the first
	page of an ibdata file is 'created' in this function into the buffer
	pool then we lose the original contents of the file flush lsn stamp.
	Then InnoDB could in a crash recovery print a big, false, corruption
	warning if the stamp contains an lsn bigger than the ib_logfile lsn. */

	memset(frame + FIL_PAGE_FILE_FLUSH_LSN, 0, 8);

#ifdef UNIV_DEBUG
	buf_dbg_counter++;

	if (buf_dbg_counter % 357 == 0) {
		ut_ad(buf_validate());
	}
#endif
#ifdef UNIV_IBUF_DEBUG
	ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
	return(frame);
}

/************************************************************************
Completes an asynchronous read or write request of a file page to or from
the buffer pool. */

void
buf_page_io_complete(
/*=================*/
	buf_block_t*	block)	/* in: pointer to the block in question */
{
	ulint		io_type;
	ulint		read_page_no;
	
	ut_ad(block);

	ut_a(block->state == BUF_BLOCK_FILE_PAGE);

	io_type = block->io_fix;

	if (io_type == BUF_IO_READ) {
		/* If this page is not uninitialized and not in the
		doublewrite buffer, then the page number should be the
		same as in block */

		read_page_no = mach_read_from_4((block->frame)
						+ FIL_PAGE_OFFSET);
		if (read_page_no != 0
			&& !trx_doublewrite_page_inside(read_page_no)
	    		&& read_page_no != block->offset) {

			fprintf(stderr,
"InnoDB: Error: page n:o stored in the page read in is %lu, should be %lu!\n",
				(ulong) read_page_no, (ulong) block->offset);
		}
		/* From version 3.23.38 up we store the page checksum
		   to the 4 first bytes of the page end lsn field */

		if (buf_page_is_corrupted(block->frame)) {
		  	fprintf(stderr,
		"InnoDB: Database page corruption on disk or a failed\n"
		"InnoDB: file read of page %lu.\n", (ulong) block->offset);
			  
			fputs(
		"InnoDB: You may have to recover from a backup.\n", stderr);

			buf_page_print(block->frame);

		  	fprintf(stderr,
		"InnoDB: Database page corruption on disk or a failed\n"
		"InnoDB: file read of page %lu.\n", (ulong) block->offset);
			fputs(
		"InnoDB: You may have to recover from a backup.\n", stderr);
			fputs(
		"InnoDB: It is also possible that your operating\n"
		"InnoDB: system has corrupted its own file cache\n"
		"InnoDB: and rebooting your computer removes the\n"
		"InnoDB: error.\n"
		"InnoDB: If the corrupt page is an index page\n"
		"InnoDB: you can also try to fix the corruption\n"
		"InnoDB: by dumping, dropping, and reimporting\n"
		"InnoDB: the corrupt table. You can use CHECK\n"
		"InnoDB: TABLE to scan your table for corruption.\n"
		"InnoDB: See also "
		"http://dev.mysql.com/doc/mysql/en/Forcing_recovery.html\n"
		"InnoDB: about forcing recovery.\n", stderr);
			  
			if (srv_force_recovery < SRV_FORCE_IGNORE_CORRUPT) {
				fputs(
	"InnoDB: Ending processing because of a corrupt database page.\n",
					stderr);
		  		exit(1);
		  	}
		}

		if (recv_recovery_is_on()) {
			recv_recover_page(FALSE, TRUE, block->frame,
						block->space, block->offset);
		}

		if (!recv_no_ibuf_operations) {
			ibuf_merge_or_delete_for_page(block->frame,
					block->space, block->offset, TRUE);
		}
	}
	
#ifdef UNIV_IBUF_DEBUG
	ut_a(ibuf_count_get(block->space, block->offset) == 0);
#endif
	mutex_enter(&(buf_pool->mutex));
	
	/* Because this thread which does the unlocking is not the same that
	did the locking, we use a pass value != 0 in unlock, which simply
	removes the newest lock debug record, without checking the thread
	id. */

	block->io_fix = 0;
	
	if (io_type == BUF_IO_READ) {
		/* NOTE that the call to ibuf may have moved the ownership of
		the x-latch to this OS thread: do not let this confuse you in
		debugging! */		
	
		ut_ad(buf_pool->n_pend_reads > 0);
		buf_pool->n_pend_reads--;
		buf_pool->n_pages_read++;

		rw_lock_x_unlock_gen(&(block->lock), BUF_IO_READ);

#ifdef UNIV_DEBUG
		if (buf_debug_prints) {
			fputs("Has read ", stderr);
		}
#endif /* UNIV_DEBUG */
	} else {
		ut_ad(io_type == BUF_IO_WRITE);

		/* Write means a flush operation: call the completion
		routine in the flush system */

		buf_flush_write_complete(block);

		rw_lock_s_unlock_gen(&(block->lock), BUF_IO_WRITE);

		buf_pool->n_pages_written++;

#ifdef UNIV_DEBUG
		if (buf_debug_prints) {
			fputs("Has written ", stderr);
		}
#endif /* UNIV_DEBUG */
	}
	
	mutex_exit(&(buf_pool->mutex));

#ifdef UNIV_DEBUG
	if (buf_debug_prints) {
		fprintf(stderr, "page space %lu page no %lu\n",
			(ulong) block->space, (ulong) block->offset);
	}
#endif /* UNIV_DEBUG */
}

/*************************************************************************
Invalidates the file pages in the buffer pool when an archive recovery is
completed. All the file pages buffered must be in a replaceable state when
this function is called: not latched and not modified. */

void
buf_pool_invalidate(void)
/*=====================*/
{
	ibool	freed;

	ut_ad(buf_all_freed());
	
	freed = TRUE;

	while (freed) {
		freed = buf_LRU_search_and_free_block(100);
	}
	
	mutex_enter(&(buf_pool->mutex));

	ut_ad(UT_LIST_GET_LEN(buf_pool->LRU) == 0);

	mutex_exit(&(buf_pool->mutex));
}

#ifdef UNIV_DEBUG
/*************************************************************************
Validates the buffer buf_pool data structure. */

ibool
buf_validate(void)
/*==============*/
{
	buf_block_t*	block;
	ulint		i;
	ulint		n_single_flush	= 0;
	ulint		n_lru_flush	= 0;
	ulint		n_list_flush	= 0;
	ulint		n_lru		= 0;
	ulint		n_flush		= 0;
	ulint		n_free		= 0;
	ulint		n_page		= 0;
	
	ut_ad(buf_pool);

	mutex_enter(&(buf_pool->mutex));

	for (i = 0; i < buf_pool->curr_size; i++) {

		block = buf_pool_get_nth_block(buf_pool, i);

		if (block->state == BUF_BLOCK_FILE_PAGE) {

			ut_a(buf_page_hash_get(block->space,
						block->offset) == block);
			n_page++;

#ifdef UNIV_IBUF_DEBUG
			ut_a((block->io_fix == BUF_IO_READ)
			     || ibuf_count_get(block->space, block->offset)
								== 0);
#endif
			if (block->io_fix == BUF_IO_WRITE) {

				if (block->flush_type == BUF_FLUSH_LRU) {
					n_lru_flush++;
					ut_a(rw_lock_is_locked(&(block->lock),
							RW_LOCK_SHARED));
				} else if (block->flush_type ==
						BUF_FLUSH_LIST) {
					n_list_flush++;
				} else if (block->flush_type ==
						BUF_FLUSH_SINGLE_PAGE) {
					n_single_flush++;
				} else {
					ut_error;
				}

			} else if (block->io_fix == BUF_IO_READ) {

				ut_a(rw_lock_is_locked(&(block->lock),
							RW_LOCK_EX));
			}
			
			n_lru++;

			if (ut_dulint_cmp(block->oldest_modification,
						ut_dulint_zero) > 0) {
					n_flush++;
			}	
		
		} else if (block->state == BUF_BLOCK_NOT_USED) {
			n_free++;
		}
 	}

	if (n_lru + n_free > buf_pool->curr_size) {
		fprintf(stderr, "n LRU %lu, n free %lu\n", (ulong) n_lru, (ulong) n_free);
		ut_error;
	}

	ut_a(UT_LIST_GET_LEN(buf_pool->LRU) == n_lru);
	if (UT_LIST_GET_LEN(buf_pool->free) != n_free) {
		fprintf(stderr, "Free list len %lu, free blocks %lu\n",
			(ulong) UT_LIST_GET_LEN(buf_pool->free), (ulong) n_free);
		ut_error;
	}
	ut_a(UT_LIST_GET_LEN(buf_pool->flush_list) == n_flush);

	ut_a(buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE] == n_single_flush);
	ut_a(buf_pool->n_flush[BUF_FLUSH_LIST] == n_list_flush);
	ut_a(buf_pool->n_flush[BUF_FLUSH_LRU] == n_lru_flush);
	
	mutex_exit(&(buf_pool->mutex));

	ut_a(buf_LRU_validate());
	ut_a(buf_flush_validate());

	return(TRUE);
}	

/*************************************************************************
Prints info of the buffer buf_pool data structure. */

void
buf_print(void)
/*===========*/
{
	dulint*		index_ids;
	ulint*		counts;
	ulint		size;
	ulint		i;
	ulint		j;
	dulint		id;
	ulint		n_found;
	buf_frame_t* 	frame;
	dict_index_t*	index;
	
	ut_ad(buf_pool);

	size = buf_pool->curr_size;

	index_ids = mem_alloc(sizeof(dulint) * size);
	counts = mem_alloc(sizeof(ulint) * size);

	mutex_enter(&(buf_pool->mutex));
	
	fprintf(stderr,
		"buf_pool size %lu\n"
		"database pages %lu\n"
		"free pages %lu\n"
		"modified database pages %lu\n"
		"n pending reads %lu\n"
		"n pending flush LRU %lu list %lu single page %lu\n"
		"pages read %lu, created %lu, written %lu\n",
		(ulong) size,
		(ulong) UT_LIST_GET_LEN(buf_pool->LRU),
		(ulong) UT_LIST_GET_LEN(buf_pool->free),
		(ulong) UT_LIST_GET_LEN(buf_pool->flush_list),
		(ulong) buf_pool->n_pend_reads,
		(ulong) buf_pool->n_flush[BUF_FLUSH_LRU],
		(ulong) buf_pool->n_flush[BUF_FLUSH_LIST],
		(ulong) buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE],
		(ulong) buf_pool->n_pages_read, buf_pool->n_pages_created,
		(ulong) buf_pool->n_pages_written);

	/* Count the number of blocks belonging to each index in the buffer */
	
	n_found = 0;

	for (i = 0; i < size; i++) {
		frame = buf_pool_get_nth_block(buf_pool, i)->frame;

		if (fil_page_get_type(frame) == FIL_PAGE_INDEX) {

			id = btr_page_get_index_id(frame);

			/* Look for the id in the index_ids array */
			j = 0;

			while (j < n_found) {

				if (ut_dulint_cmp(index_ids[j], id) == 0) {
					(counts[j])++;

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

	mutex_exit(&(buf_pool->mutex));

	for (i = 0; i < n_found; i++) {
		index = dict_index_get_if_in_cache(index_ids[i]);

		fprintf(stderr,
			"Block count for index %lu in buffer is about %lu",
		       (ulong) ut_dulint_get_low(index_ids[i]),
		       (ulong) counts[i]);

		if (index) {
			putc(' ', stderr);
			dict_index_name_print(stderr, NULL, index);
		}

		putc('\n', stderr);
	}
	
	mem_free(index_ids);
	mem_free(counts);

	ut_a(buf_validate());
}	
#endif /* UNIV_DEBUG */

/*************************************************************************
Returns the number of latched pages in the buffer pool. */

ulint
buf_get_latched_pages_number(void)
{
        buf_block_t* block;
        ulint i;
        ulint fixed_pages_number = 0;

        mutex_enter(&(buf_pool->mutex));

        for (i = 0; i < buf_pool->curr_size; i++) {

               block = buf_pool_get_nth_block(buf_pool, i);

               if (((block->buf_fix_count != 0) || (block->io_fix != 0)) &&
                    block->magic_n == BUF_BLOCK_MAGIC_N )
                       fixed_pages_number++;
        }

        mutex_exit(&(buf_pool->mutex));
        return fixed_pages_number;
}

/*************************************************************************
Returns the number of pending buf pool ios. */

ulint
buf_get_n_pending_ios(void)
/*=======================*/
{
	return(buf_pool->n_pend_reads
		+ buf_pool->n_flush[BUF_FLUSH_LRU]
		+ buf_pool->n_flush[BUF_FLUSH_LIST]
		+ buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE]);
}

/*************************************************************************
Returns the ratio in percents of modified pages in the buffer pool /
database pages in the buffer pool. */

ulint
buf_get_modified_ratio_pct(void)
/*============================*/
{
	ulint	ratio;

	mutex_enter(&(buf_pool->mutex));

	ratio = (100 * UT_LIST_GET_LEN(buf_pool->flush_list))
		     / (1 + UT_LIST_GET_LEN(buf_pool->LRU)
		        + UT_LIST_GET_LEN(buf_pool->free));

		       /* 1 + is there to avoid division by zero */   

	mutex_exit(&(buf_pool->mutex));

	return(ratio);
}

/*************************************************************************
Prints info of the buffer i/o. */

void
buf_print_io(
/*=========*/
	FILE*	file)	/* in/out: buffer where to print */
{
	time_t	current_time;
	double	time_elapsed;
	ulint	size;
	
	ut_ad(buf_pool);
	size = buf_pool->curr_size;

	mutex_enter(&(buf_pool->mutex));
	
	if (srv_use_awe) {
		fprintf(stderr,
		"AWE: Buffer pool memory frames                        %lu\n",
				(ulong) buf_pool->n_frames);
		
		fprintf(stderr,
		"AWE: Database pages and free buffers mapped in frames %lu\n",
				(ulong) UT_LIST_GET_LEN(buf_pool->awe_LRU_free_mapped));
	}
	fprintf(file,
		"Buffer pool size   %lu\n"
		"Free buffers       %lu\n"
		"Database pages     %lu\n"
		"Modified db pages  %lu\n"
		"Pending reads %lu\n"
		"Pending writes: LRU %lu, flush list %lu, single page %lu\n",
		(ulong) size,
		(ulong) UT_LIST_GET_LEN(buf_pool->free),
		(ulong) UT_LIST_GET_LEN(buf_pool->LRU),
		(ulong) UT_LIST_GET_LEN(buf_pool->flush_list),
		(ulong) buf_pool->n_pend_reads,
		(ulong) buf_pool->n_flush[BUF_FLUSH_LRU]
			+ buf_pool->init_flush[BUF_FLUSH_LRU],
		(ulong) buf_pool->n_flush[BUF_FLUSH_LIST]
			+ buf_pool->init_flush[BUF_FLUSH_LIST],
		(ulong) buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE]);

	current_time = time(NULL);
	time_elapsed = 0.001 + difftime(current_time,
						buf_pool->last_printout_time);
	buf_pool->last_printout_time = current_time;

	fprintf(file,
		"Pages read %lu, created %lu, written %lu\n"
		"%.2f reads/s, %.2f creates/s, %.2f writes/s\n",
		(ulong) buf_pool->n_pages_read,
		(ulong) buf_pool->n_pages_created,
		(ulong) buf_pool->n_pages_written,
		(buf_pool->n_pages_read - buf_pool->n_pages_read_old)
		/ time_elapsed,
		(buf_pool->n_pages_created - buf_pool->n_pages_created_old)
		/ time_elapsed,
		(buf_pool->n_pages_written - buf_pool->n_pages_written_old)
		/ time_elapsed);

	if (srv_use_awe) {
		fprintf(file, "AWE: %.2f page remaps/s\n",
		(buf_pool->n_pages_awe_remapped
				- buf_pool->n_pages_awe_remapped_old)
			/ time_elapsed);
	}
		
	if (buf_pool->n_page_gets > buf_pool->n_page_gets_old) {
		fprintf(file, "Buffer pool hit rate %lu / 1000\n",
       (ulong) (1000
		- ((1000 *
		    (buf_pool->n_pages_read - buf_pool->n_pages_read_old))
		/ (buf_pool->n_page_gets - buf_pool->n_page_gets_old))));
	} else {
		fputs("No buffer pool page gets since the last printout\n",
			file);
	}

	buf_pool->n_page_gets_old = buf_pool->n_page_gets;
	buf_pool->n_pages_read_old = buf_pool->n_pages_read;
	buf_pool->n_pages_created_old = buf_pool->n_pages_created;
	buf_pool->n_pages_written_old = buf_pool->n_pages_written;
	buf_pool->n_pages_awe_remapped_old = buf_pool->n_pages_awe_remapped;

	mutex_exit(&(buf_pool->mutex));
}

/**************************************************************************
Refreshes the statistics used to print per-second averages. */

void
buf_refresh_io_stats(void)
/*======================*/
{
        buf_pool->last_printout_time = time(NULL);
	buf_pool->n_page_gets_old = buf_pool->n_page_gets;
	buf_pool->n_pages_read_old = buf_pool->n_pages_read;
	buf_pool->n_pages_created_old = buf_pool->n_pages_created;
	buf_pool->n_pages_written_old = buf_pool->n_pages_written;
	buf_pool->n_pages_awe_remapped_old = buf_pool->n_pages_awe_remapped; 
}

/*************************************************************************
Checks that all file pages in the buffer are in a replaceable state. */

ibool
buf_all_freed(void)
/*===============*/
{
	buf_block_t*	block;
	ulint		i;
	
	ut_ad(buf_pool);

	mutex_enter(&(buf_pool->mutex));

	for (i = 0; i < buf_pool->curr_size; i++) {

		block = buf_pool_get_nth_block(buf_pool, i);

		if (block->state == BUF_BLOCK_FILE_PAGE) {

			if (!buf_flush_ready_for_replace(block)) {

				fprintf(stderr,
					"Page %lu %lu still fixed or dirty\n",
					(ulong) block->space, (ulong) block->offset);
			    	ut_error;
			}
		}
 	}

	mutex_exit(&(buf_pool->mutex));

	return(TRUE);
}	

/*************************************************************************
Checks that there currently are no pending i/o-operations for the buffer
pool. */

ibool
buf_pool_check_no_pending_io(void)
/*==============================*/
				/* out: TRUE if there is no pending i/o */
{
	ibool	ret;

	mutex_enter(&(buf_pool->mutex));

	if (buf_pool->n_pend_reads + buf_pool->n_flush[BUF_FLUSH_LRU]
				+ buf_pool->n_flush[BUF_FLUSH_LIST]
				+ buf_pool->n_flush[BUF_FLUSH_SINGLE_PAGE]) {
		ret = FALSE;
	} else {
		ret = TRUE;
	}

	mutex_exit(&(buf_pool->mutex));

	return(ret);
}

/*************************************************************************
Gets the current length of the free list of buffer blocks. */

ulint
buf_get_free_list_len(void)
/*=======================*/
{
	ulint	len;

	mutex_enter(&(buf_pool->mutex));

	len = UT_LIST_GET_LEN(buf_pool->free);

	mutex_exit(&(buf_pool->mutex));

	return(len);
}
