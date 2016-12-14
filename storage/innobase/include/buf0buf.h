/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/buf0buf.h
The database buffer pool high-level routines

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0buf_h
#define buf0buf_h

#include "univ.i"
#include "fil0fil.h"
#include "mtr0types.h"
#include "buf0types.h"
#ifndef UNIV_INNOCHECKSUM
#include "hash0hash.h"
#include "ut0byte.h"
#include "page0types.h"
#ifndef UNIV_HOTBACKUP
#include "ut0rbt.h"
#include "os0proc.h"
#include "log0log.h"
#include "srv0srv.h"
#include <ostream>

// Forward declaration
struct fil_addr_t;

/** @name Modes for buf_page_get_gen */
/* @{ */
#define BUF_GET			10	/*!< get always */
#define	BUF_GET_IF_IN_POOL	11	/*!< get if in pool */
#define BUF_PEEK_IF_IN_POOL	12	/*!< get if in pool, do not make
					the block young in the LRU list */
#define BUF_GET_NO_LATCH	14	/*!< get and bufferfix, but
					set no latch; we have
					separated this case, because
					it is error-prone programming
					not to set a latch, and it
					should be used with care */
#define BUF_GET_IF_IN_POOL_OR_WATCH	15
					/*!< Get the page only if it's in the
					buffer pool, if not then set a watch
					on the page. */
#define BUF_GET_POSSIBLY_FREED		16
					/*!< Like BUF_GET, but do not mind
					if the file page has been freed. */
/* @} */
/** @name Modes for buf_page_get_known_nowait */
/* @{ */
#define BUF_MAKE_YOUNG	51		/*!< Move the block to the
					start of the LRU list if there
					is a danger that the block
					would drift out of the buffer
					pool*/
#define BUF_KEEP_OLD	52		/*!< Preserve the current LRU
					position of the block. */
/* @} */

#define MAX_BUFFER_POOLS_BITS	6	/*!< Number of bits to representing
					a buffer pool ID */

#define MAX_BUFFER_POOLS 	(1 << MAX_BUFFER_POOLS_BITS)
					/*!< The maximum number of buffer
					pools that can be defined */

#define BUF_POOL_WATCH_SIZE		(srv_n_purge_threads + 1)
					/*!< Maximum number of concurrent
					buffer pool watches */
#define MAX_PAGE_HASH_LOCKS	1024	/*!< The maximum number of
					page_hash locks */

extern	buf_pool_t*	buf_pool_ptr;	/*!< The buffer pools
					of the database */

extern	volatile bool	buf_pool_withdrawing; /*!< true when withdrawing buffer
					pool pages might cause page relocation */

extern	volatile ulint	buf_withdraw_clock; /*!< the clock is incremented
					every time a pointer to a page may
					become obsolete */

#ifdef UNIV_DEBUG
extern my_bool	buf_disable_resize_buffer_pool_debug; /*!< if TRUE, resizing
					buffer pool is not allowed. */
#endif /* UNIV_DEBUG */
#else /* !UNIV_HOTBACKUP */
extern buf_block_t*	back_block1;	/*!< first block, for --apply-log */
extern buf_block_t*	back_block2;	/*!< second block, for page reorganize */
#endif /* !UNIV_HOTBACKUP */
#endif /* !UNIV_INNOCHECKSUM */

/** Magic value to use instead of checksums when they are disabled */
#define BUF_NO_CHECKSUM_MAGIC 0xDEADBEEFUL

#ifndef UNIV_INNOCHECKSUM
/** @brief States of a control block
@see buf_page_t

The enumeration values must be 0..7. */
enum buf_page_state {
	BUF_BLOCK_POOL_WATCH,		/*!< a sentinel for the buffer pool
					watch, element of buf_pool->watch[] */
	BUF_BLOCK_ZIP_PAGE,		/*!< contains a clean
					compressed page */
	BUF_BLOCK_ZIP_DIRTY,		/*!< contains a compressed
					page that is in the
					buf_pool->flush_list */

	BUF_BLOCK_NOT_USED,		/*!< is in the free list;
					must be after the BUF_BLOCK_ZIP_
					constants for compressed-only pages
					@see buf_block_state_valid() */
	BUF_BLOCK_READY_FOR_USE,	/*!< when buf_LRU_get_free_block
					returns a block, it is in this state */
	BUF_BLOCK_FILE_PAGE,		/*!< contains a buffered file page */
	BUF_BLOCK_MEMORY,		/*!< contains some main memory
					object */
	BUF_BLOCK_REMOVE_HASH		/*!< hash index should be removed
					before putting to the free list */
};


/** This structure defines information we will fetch from each buffer pool. It
will be used to print table IO stats */
struct buf_pool_info_t{
	/* General buffer pool info */
	ulint	pool_unique_id;		/*!< Buffer Pool ID */
	ulint	pool_size;		/*!< Buffer Pool size in pages */
	ulint	lru_len;		/*!< Length of buf_pool->LRU */
	ulint	old_lru_len;		/*!< buf_pool->LRU_old_len */
	ulint	free_list_len;		/*!< Length of buf_pool->free list */
	ulint	flush_list_len;		/*!< Length of buf_pool->flush_list */
	ulint	n_pend_unzip;		/*!< buf_pool->n_pend_unzip, pages
					pending decompress */
	ulint	n_pend_reads;		/*!< buf_pool->n_pend_reads, pages
					pending read */
	ulint	n_pending_flush_lru;	/*!< Pages pending flush in LRU */
	ulint	n_pending_flush_single_page;/*!< Pages pending to be
					flushed as part of single page
					flushes issued by various user
					threads */
	ulint	n_pending_flush_list;	/*!< Pages pending flush in FLUSH
					LIST */
	ulint	n_pages_made_young;	/*!< number of pages made young */
	ulint	n_pages_not_made_young;	/*!< number of pages not made young */
	ulint	n_pages_read;		/*!< buf_pool->n_pages_read */
	ulint	n_pages_created;	/*!< buf_pool->n_pages_created */
	ulint	n_pages_written;	/*!< buf_pool->n_pages_written */
	ulint	n_page_gets;		/*!< buf_pool->n_page_gets */
	ulint	n_ra_pages_read_rnd;	/*!< buf_pool->n_ra_pages_read_rnd,
					number of pages readahead */
	ulint	n_ra_pages_read;	/*!< buf_pool->n_ra_pages_read, number
					of pages readahead */
	ulint	n_ra_pages_evicted;	/*!< buf_pool->n_ra_pages_evicted,
					number of readahead pages evicted
					without access */
	ulint	n_page_get_delta;	/*!< num of buffer pool page gets since
					last printout */

	/* Buffer pool access stats */
	double	page_made_young_rate;	/*!< page made young rate in pages
					per second */
	double	page_not_made_young_rate;/*!< page not made young rate
					in pages per second */
	double	pages_read_rate;	/*!< num of pages read per second */
	double	pages_created_rate;	/*!< num of pages create per second */
	double	pages_written_rate;	/*!< num of  pages written per second */
	ulint	page_read_delta;	/*!< num of pages read since last
					printout */
	ulint	young_making_delta;	/*!< num of pages made young since
					last printout */
	ulint	not_young_making_delta;	/*!< num of pages not make young since
					last printout */

	/* Statistics about read ahead algorithm.  */
	double	pages_readahead_rnd_rate;/*!< random readahead rate in pages per
					second */
	double	pages_readahead_rate;	/*!< readahead rate in pages per
					second */
	double	pages_evicted_rate;	/*!< rate of readahead page evicted
					without access, in pages per second */

	/* Stats about LRU eviction */
	ulint	unzip_lru_len;		/*!< length of buf_pool->unzip_LRU
					list */
	/* Counters for LRU policy */
	ulint	io_sum;			/*!< buf_LRU_stat_sum.io */
	ulint	io_cur;			/*!< buf_LRU_stat_cur.io, num of IO
					for current interval */
	ulint	unzip_sum;		/*!< buf_LRU_stat_sum.unzip */
	ulint	unzip_cur;		/*!< buf_LRU_stat_cur.unzip, num
					pages decompressed in current
					interval */
};

/** The occupied bytes of lists in all buffer pools */
struct buf_pools_list_size_t {
	ulint	LRU_bytes;		/*!< LRU size in bytes */
	ulint	unzip_LRU_bytes;	/*!< unzip_LRU size in bytes */
	ulint	flush_list_bytes;	/*!< flush_list size in bytes */
};

/** Page identifier. */
class page_id_t {
public:

	/** Constructor from (space, page_no).
	@param[in]	space	tablespace id
	@param[in]	page_no	page number */
	page_id_t(ulint space, ulint page_no)
		:
		m_space(static_cast<ib_uint32_t>(space)),
		m_page_no(static_cast<ib_uint32_t>(page_no)),
		m_fold(ULINT_UNDEFINED)
	{
		ut_ad(space <= 0xFFFFFFFFU);
		ut_ad(page_no <= 0xFFFFFFFFU);
	}

	/** Retrieve the tablespace id.
	@return tablespace id */
	inline ib_uint32_t space() const
	{
		return(m_space);
	}

	/** Retrieve the page number.
	@return page number */
	inline ib_uint32_t page_no() const
	{
		return(m_page_no);
	}

	/** Retrieve the fold value.
	@return fold value */
	inline ulint fold() const
	{
		/* Initialize m_fold if it has not been initialized yet. */
		if (m_fold == ULINT_UNDEFINED) {
			m_fold = (m_space << 20) + m_space + m_page_no;
			ut_ad(m_fold != ULINT_UNDEFINED);
		}

		return(m_fold);
	}

	/** Copy the values from a given page_id_t object.
	@param[in]	src	page id object whose values to fetch */
	inline void copy_from(const page_id_t& src)
	{
		m_space = src.space();
		m_page_no = src.page_no();
		m_fold = src.fold();
	}

	/** Reset the values from a (space, page_no).
	@param[in]	space	tablespace id
	@param[in]	page_no	page number */
	inline void reset(ulint space, ulint page_no)
	{
		m_space = static_cast<ib_uint32_t>(space);
		m_page_no = static_cast<ib_uint32_t>(page_no);
		m_fold = ULINT_UNDEFINED;

		ut_ad(space <= 0xFFFFFFFFU);
		ut_ad(page_no <= 0xFFFFFFFFU);
	}

	/** Reset the page number only.
	@param[in]	page_no	page number */
	inline void set_page_no(ulint page_no)
	{
		m_page_no = static_cast<ib_uint32_t>(page_no);
		m_fold = ULINT_UNDEFINED;

		ut_ad(page_no <= 0xFFFFFFFFU);
	}

	/** Check if a given page_id_t object is equal to the current one.
	@param[in]	a	page_id_t object to compare
	@return true if equal */
	inline bool equals_to(const page_id_t& a) const
	{
		return(a.space() == m_space && a.page_no() == m_page_no);
	}

private:

	/** Tablespace id. */
	ib_uint32_t	m_space;

	/** Page number. */
	ib_uint32_t	m_page_no;

	/** A fold value derived from m_space and m_page_no,
	used in hashing. */
	mutable ulint	m_fold;

	/* Disable implicit copying. */
	void operator=(const page_id_t&);

	/** Declare the overloaded global operator<< as a friend of this
	class. Refer to the global declaration for further details.  Print
	the given page_id_t object.
	@param[in,out]	out	the output stream
	@param[in]	page_id	the page_id_t object to be printed
	@return the output stream */
        friend
        std::ostream&
        operator<<(
                std::ostream&           out,
                const page_id_t&        page_id);
};

/** Print the given page_id_t object.
@param[in,out]	out	the output stream
@param[in]	page_id	the page_id_t object to be printed
@return the output stream */
std::ostream&
operator<<(
	std::ostream&		out,
	const page_id_t&	page_id);

#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Acquire mutex on all buffer pool instances */
UNIV_INLINE
void
buf_pool_mutex_enter_all(void);
/*===========================*/

/********************************************************************//**
Release mutex on all buffer pool instances */
UNIV_INLINE
void
buf_pool_mutex_exit_all(void);
/*==========================*/

/********************************************************************//**
Creates the buffer pool.
@return DB_SUCCESS if success, DB_ERROR if not enough memory or error */
dberr_t
buf_pool_init(
/*=========*/
	ulint	size,		/*!< in: Size of the total pool in bytes */
	ulint	n_instances);	/*!< in: Number of instances */
/********************************************************************//**
Frees the buffer pool at shutdown.  This must not be invoked before
freeing all mutexes. */
void
buf_pool_free(
/*==========*/
	ulint	n_instances);	/*!< in: numbere of instances to free */

/** Determines if a block is intended to be withdrawn.
@param[in]	buf_pool	buffer pool instance
@param[in]	block		pointer to control block
@retval true	if will be withdrawn */
bool
buf_block_will_withdrawn(
	buf_pool_t*		buf_pool,
	const buf_block_t*	block);

/** Determines if a frame is intended to be withdrawn.
@param[in]	buf_pool	buffer pool instance
@param[in]	ptr		pointer to a frame
@retval true	if will be withdrawn */
bool
buf_frame_will_withdrawn(
	buf_pool_t*	buf_pool,
	const byte*	ptr);

/** Resize the buffer pool based on srv_buf_pool_size from
srv_buf_pool_old_size. */
void
buf_pool_resize();

/** This is the thread for resizing buffer pool. It waits for an event and
when waked up either performs a resizing and sleeps again.
@param[in]	arg	a dummy parameter required by os_thread_create.
@return	this function does not return, calls os_thread_exit()
*/
extern "C"
os_thread_ret_t
DECLARE_THREAD(buf_resize_thread)(
/*==============================*/
	void*	arg);				/*!< in: a dummy parameter
						required by os_thread_create */

/********************************************************************//**
Clears the adaptive hash index on all pages in the buffer pool. */
void
buf_pool_clear_hash_index(void);
/*===========================*/

/*********************************************************************//**
Gets the current size of buffer buf_pool in bytes.
@return size in bytes */
UNIV_INLINE
ulint
buf_pool_get_curr_size(void);
/*========================*/
/*********************************************************************//**
Gets the current size of buffer buf_pool in frames.
@return size in pages */
UNIV_INLINE
ulint
buf_pool_get_n_pages(void);
/*=======================*/
/********************************************************************//**
Gets the smallest oldest_modification lsn for any page in the pool. Returns
zero if all modified pages have been flushed to disk.
@return oldest modification in pool, zero if none */
lsn_t
buf_pool_get_oldest_modification(void);
/*==================================*/

/********************************************************************//**
Allocates a buf_page_t descriptor. This function must succeed. In case
of failure we assert in this function. */
UNIV_INLINE
buf_page_t*
buf_page_alloc_descriptor(void)
/*===========================*/
	MY_ATTRIBUTE((malloc));
/********************************************************************//**
Free a buf_page_t descriptor. */
UNIV_INLINE
void
buf_page_free_descriptor(
/*=====================*/
	buf_page_t*	bpage)	/*!< in: bpage descriptor to free. */
	MY_ATTRIBUTE((nonnull));

/********************************************************************//**
Allocates a buffer block.
@return own: the allocated block, in state BUF_BLOCK_MEMORY */
buf_block_t*
buf_block_alloc(
/*============*/
	buf_pool_t*	buf_pool);	/*!< in: buffer pool instance,
					or NULL for round-robin selection
					of the buffer pool */
/********************************************************************//**
Frees a buffer block which does not contain a file page. */
UNIV_INLINE
void
buf_block_free(
/*===========*/
	buf_block_t*	block);	/*!< in, own: block to be freed */
#endif /* !UNIV_HOTBACKUP */
/*********************************************************************//**
Copies contents of a buffer frame to a given buffer.
@return buf */
UNIV_INLINE
byte*
buf_frame_copy(
/*===========*/
	byte*			buf,	/*!< in: buffer to copy to */
	const buf_frame_t*	frame);	/*!< in: buffer frame */
#ifndef UNIV_HOTBACKUP
/**************************************************************//**
NOTE! The following macros should be used instead of buf_page_get_gen,
to improve debugging. Only values RW_S_LATCH and RW_X_LATCH are allowed
in LA! */
#define buf_page_get(ID, SIZE, LA, MTR)	\
	buf_page_get_gen(ID, SIZE, LA, NULL, BUF_GET, __FILE__, __LINE__, MTR)
/**************************************************************//**
Use these macros to bufferfix a page with no latching. Remember not to
read the contents of the page unless you know it is safe. Do not modify
the contents of the page! We have separated this case, because it is
error-prone programming not to set a latch, and it should be used
with care. */
#define buf_page_get_with_no_latch(ID, SIZE, MTR)	\
	buf_page_get_gen(ID, SIZE, RW_NO_LATCH, NULL, BUF_GET_NO_LATCH, \
			 __FILE__, __LINE__, MTR)
/********************************************************************//**
This is the general function used to get optimistic access to a database
page.
@return TRUE if success */
ibool
buf_page_optimistic_get(
/*====================*/
	ulint		rw_latch,/*!< in: RW_S_LATCH, RW_X_LATCH */
	buf_block_t*	block,	/*!< in: guessed block */
	ib_uint64_t	modify_clock,/*!< in: modify clock value */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr);	/*!< in: mini-transaction */
/********************************************************************//**
This is used to get access to a known database page, when no waiting can be
done.
@return TRUE if success */
ibool
buf_page_get_known_nowait(
/*======================*/
	ulint		rw_latch,/*!< in: RW_S_LATCH, RW_X_LATCH */
	buf_block_t*	block,	/*!< in: the known page */
	ulint		mode,	/*!< in: BUF_MAKE_YOUNG or BUF_KEEP_OLD */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr);	/*!< in: mini-transaction */

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
	mtr_t*			mtr);

/** Tries to get a page.
If the page is not in the buffer pool it is not loaded. Suitable for using
when holding the lock_sys_t::mutex.
@param[in]	page_id	page identifier
@param[in]	mtr	mini-transaction
@return the page if in buffer pool, NULL if not */
#define buf_page_try_get(page_id, mtr)	\
	buf_page_try_get_func((page_id), __FILE__, __LINE__, mtr);

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
	const page_size_t&	page_size);

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
	bool			dirty_with_no_latch = false);

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
	mtr_t*			mtr);

#else /* !UNIV_HOTBACKUP */

/** Inits a page to the buffer buf_pool, for use in mysqlbackup --restore.
@param[in]	page_id		page id
@param[in]	page_size	page size
@param[in,out]	block		block to init */
void
buf_page_init_for_backup_restore(
	const page_id_t&	page_id,
	const page_size_t&	page_size,
	buf_block_t*		block);

#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Releases a compressed-only page acquired with buf_page_get_zip(). */
UNIV_INLINE
void
buf_page_release_zip(
/*=================*/
	buf_page_t*	bpage);		/*!< in: buffer block */
/********************************************************************//**
Releases a latch, if specified. */
UNIV_INLINE
void
buf_page_release_latch(
/*=====================*/
	buf_block_t*	block,		/*!< in: buffer block */
	ulint		rw_latch);	/*!< in: RW_S_LATCH, RW_X_LATCH,
					RW_NO_LATCH */
/********************************************************************//**
Moves a page to the start of the buffer pool LRU list. This high-level
function can be used to prevent an important page from slipping out of
the buffer pool. */
void
buf_page_make_young(
/*================*/
	buf_page_t*	bpage);	/*!< in: buffer block of a file page */

/** Returns TRUE if the page can be found in the buffer pool hash table.
NOTE that it is possible that the page is not yet read from disk,
though.
@param[in]	page_id	page id
@return TRUE if found in the page hash table */
UNIV_INLINE
ibool
buf_page_peek(
	const page_id_t&	page_id);

#ifdef UNIV_DEBUG

/** Sets file_page_was_freed TRUE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated.
@param[in]	page_id	page id
@return control block if found in page hash table, otherwise NULL */
buf_page_t*
buf_page_set_file_page_was_freed(
	const page_id_t&	page_id);

/** Sets file_page_was_freed FALSE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated.
@param[in]	page_id	page id
@return control block if found in page hash table, otherwise NULL */
buf_page_t*
buf_page_reset_file_page_was_freed(
	const page_id_t&	page_id);

#endif /* UNIV_DEBUG */
/********************************************************************//**
Reads the freed_page_clock of a buffer block.
@return freed_page_clock */
UNIV_INLINE
ulint
buf_page_get_freed_page_clock(
/*==========================*/
	const buf_page_t*	bpage)	/*!< in: block */
	MY_ATTRIBUTE((warn_unused_result));
/********************************************************************//**
Reads the freed_page_clock of a buffer block.
@return freed_page_clock */
UNIV_INLINE
ulint
buf_block_get_freed_page_clock(
/*===========================*/
	const buf_block_t*	block)	/*!< in: block */
	MY_ATTRIBUTE((warn_unused_result));

/********************************************************************//**
Tells if a block is still close enough to the MRU end of the LRU list
meaning that it is not in danger of getting evicted and also implying
that it has been accessed recently.
Note that this is for heuristics only and does not reserve buffer pool
mutex.
@return TRUE if block is close to MRU end of LRU */
UNIV_INLINE
ibool
buf_page_peek_if_young(
/*===================*/
	const buf_page_t*	bpage);	/*!< in: block */
/********************************************************************//**
Recommends a move of a block to the start of the LRU list if there is danger
of dropping from the buffer pool. NOTE: does not reserve the buffer pool
mutex.
@return TRUE if should be made younger */
UNIV_INLINE
ibool
buf_page_peek_if_too_old(
/*=====================*/
	const buf_page_t*	bpage);	/*!< in: block to make younger */
/********************************************************************//**
Gets the youngest modification log sequence number for a frame.
Returns zero if not file page or no modification occurred yet.
@return newest modification to page */
UNIV_INLINE
lsn_t
buf_page_get_newest_modification(
/*=============================*/
	const buf_page_t*	bpage);	/*!< in: block containing the
					page frame */
/********************************************************************//**
Increments the modify clock of a frame by 1. The caller must (1) own the
buf_pool->mutex and block bufferfix count has to be zero, (2) or own an x-lock
on the block. */
UNIV_INLINE
void
buf_block_modify_clock_inc(
/*=======================*/
	buf_block_t*	block);	/*!< in: block */
/********************************************************************//**
Returns the value of the modify clock. The caller must have an s-lock
or x-lock on the block.
@return value */
UNIV_INLINE
ib_uint64_t
buf_block_get_modify_clock(
/*=======================*/
	buf_block_t*	block);	/*!< in: block */
/*******************************************************************//**
Increments the bufferfix count. */
UNIV_INLINE
void
buf_block_buf_fix_inc_func(
/*=======================*/
# ifdef UNIV_DEBUG
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line */
# endif /* UNIV_DEBUG */
	buf_block_t*	block)	/*!< in/out: block to bufferfix */
	MY_ATTRIBUTE((nonnull));

/** Increments the bufferfix count.
@param[in,out]	bpage	block to bufferfix
@return the count */
UNIV_INLINE
ulint
buf_block_fix(
	buf_page_t*	bpage);

/** Increments the bufferfix count.
@param[in,out]	block	block to bufferfix
@return the count */
UNIV_INLINE
ulint
buf_block_fix(
	buf_block_t*	block);

/** Decrements the bufferfix count.
@param[in,out]	bpage	block to bufferunfix
@return	the remaining buffer-fix count */
UNIV_INLINE
ulint
buf_block_unfix(
	buf_page_t*	bpage);
/** Decrements the bufferfix count.
@param[in,out]	block	block to bufferunfix
@return	the remaining buffer-fix count */
UNIV_INLINE
ulint
buf_block_unfix(
	buf_block_t*	block);

# ifdef UNIV_DEBUG
/** Increments the bufferfix count.
@param[in,out]	b	block to bufferfix
@param[in]	f	file name where requested
@param[in]	l	line number where requested */
# define buf_block_buf_fix_inc(b,f,l) buf_block_buf_fix_inc_func(f,l,b)
# else /* UNIV_DEBUG */
/** Increments the bufferfix count.
@param[in,out]	b	block to bufferfix
@param[in]	f	file name where requested
@param[in]	l	line number where requested */
# define buf_block_buf_fix_inc(b,f,l) buf_block_buf_fix_inc_func(b)
# endif /* UNIV_DEBUG */
#else /* !UNIV_HOTBACKUP */
# define buf_block_modify_clock_inc(block) ((void) 0)
#endif /* !UNIV_HOTBACKUP */
#endif /* !UNIV_INNOCHECKSUM */

/** Checks if a page contains only zeroes.
@param[in]	read_buf	database page
@param[in]	page_size	page size
@return true if page is filled with zeroes */
bool
buf_page_is_zeroes(
	const byte*		read_buf,
	const page_size_t&	page_size);

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
) MY_ATTRIBUTE((warn_unused_result));
#ifndef UNIV_INNOCHECKSUM
#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Gets the space id, page offset, and byte offset within page of a
pointer pointing to a buffer frame containing a file page. */
UNIV_INLINE
void
buf_ptr_get_fsp_addr(
/*=================*/
	const void*	ptr,	/*!< in: pointer to a buffer frame */
	ulint*		space,	/*!< out: space id */
	fil_addr_t*	addr);	/*!< out: page offset and byte offset */
/**********************************************************************//**
Gets the hash value of a block. This can be used in searches in the
lock hash table.
@return lock hash value */
UNIV_INLINE
ulint
buf_block_get_lock_hash_val(
/*========================*/
	const buf_block_t*	block)	/*!< in: block */
	MY_ATTRIBUTE((warn_unused_result));
#ifdef UNIV_DEBUG
/*********************************************************************//**
Finds a block in the buffer pool that points to a
given compressed page.
@return buffer block pointing to the compressed page, or NULL */
buf_block_t*
buf_pool_contains_zip(
/*==================*/
	buf_pool_t*	buf_pool,	/*!< in: buffer pool instance */
	const void*	data);		/*!< in: pointer to compressed page */
#endif /* UNIV_DEBUG */

/***********************************************************************
FIXME_FTS: Gets the frame the pointer is pointing to. */
UNIV_INLINE
buf_frame_t*
buf_frame_align(
/*============*/
                        /* out: pointer to frame */
        byte*   ptr);   /* in: pointer to a frame */


#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/*********************************************************************//**
Validates the buffer pool data structure.
@return TRUE */
ibool
buf_validate(void);
/*==============*/
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#if defined UNIV_DEBUG_PRINT || defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/*********************************************************************//**
Prints info of the buffer pool data structure. */
void
buf_print(void);
/*============*/
#endif /* UNIV_DEBUG_PRINT || UNIV_DEBUG || UNIV_BUF_DEBUG */
#endif /* !UNIV_HOTBACKUP */
enum buf_page_print_flags {
	/** Do not crash at the end of buf_page_print(). */
	BUF_PAGE_PRINT_NO_CRASH	= 1,
	/** Do not print the full page dump. */
	BUF_PAGE_PRINT_NO_FULL = 2
};

/** Prints a page to stderr.
@param[in]	read_buf	a database page
@param[in]	page_size	page size
@param[in]	flags		0 or BUF_PAGE_PRINT_NO_CRASH or
BUF_PAGE_PRINT_NO_FULL */
void
buf_page_print(
	const byte*		read_buf,
	const page_size_t&	page_size,
	ulint			flags);

/********************************************************************//**
Decompress a block.
@return TRUE if successful */
ibool
buf_zip_decompress(
/*===============*/
	buf_block_t*	block,	/*!< in/out: block */
	ibool		check);	/*!< in: TRUE=verify the page checksum */
#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
/*********************************************************************//**
Returns the number of latched pages in the buffer pool.
@return number of latched pages */
ulint
buf_get_latched_pages_number(void);
/*==============================*/
#endif /* UNIV_DEBUG */
/*********************************************************************//**
Returns the number of pending buf pool read ios.
@return number of pending read I/O operations */
ulint
buf_get_n_pending_read_ios(void);
/*============================*/
/*********************************************************************//**
Prints info of the buffer i/o. */
void
buf_print_io(
/*=========*/
	FILE*	file);	/*!< in: file where to print */
/*******************************************************************//**
Collect buffer pool stats information for a buffer pool. Also
record aggregated stats if there are more than one buffer pool
in the server */
void
buf_stats_get_pool_info(
/*====================*/
	buf_pool_t*		buf_pool,	/*!< in: buffer pool */
	ulint			pool_id,	/*!< in: buffer pool ID */
	buf_pool_info_t*	all_pool_info);	/*!< in/out: buffer pool info
						to fill */
/*********************************************************************//**
Returns the ratio in percents of modified pages in the buffer pool /
database pages in the buffer pool.
@return modified page percentage ratio */
double
buf_get_modified_ratio_pct(void);
/*============================*/
/**********************************************************************//**
Refreshes the statistics used to print per-second averages. */
void
buf_refresh_io_stats(
/*=================*/
	buf_pool_t*	buf_pool);	/*!< buffer pool instance */
/**********************************************************************//**
Refreshes the statistics used to print per-second averages. */
void
buf_refresh_io_stats_all(void);
/*=================*/
/*********************************************************************//**
Asserts that all file pages in the buffer are in a replaceable state.
@return TRUE */
ibool
buf_all_freed(void);
/*===============*/
/*********************************************************************//**
Checks that there currently are no pending i/o-operations for the buffer
pool.
@return number of pending i/o operations */
ulint
buf_pool_check_no_pending_io(void);
/*==============================*/
/*********************************************************************//**
Invalidates the file pages in the buffer pool when an archive recovery is
completed. All the file pages buffered must be in a replaceable state when
this function is called: not latched and not modified. */
void
buf_pool_invalidate(void);
/*=====================*/
#endif /* !UNIV_HOTBACKUP */

/*========================================================================
--------------------------- LOWER LEVEL ROUTINES -------------------------
=========================================================================*/

#ifdef UNIV_DEBUG
/*********************************************************************//**
Adds latch level info for the rw-lock protecting the buffer frame. This
should be called in the debug version after a successful latching of a
page if we know the latching order level of the acquired latch. */
UNIV_INLINE
void
buf_block_dbg_add_level(
/*====================*/
	buf_block_t*	block,	/*!< in: buffer page
				where we have acquired latch */
	latch_level_t	level);	/*!< in: latching order level */
#else /* UNIV_DEBUG */
# define buf_block_dbg_add_level(block, level) /* nothing */
#endif /* UNIV_DEBUG */
/*********************************************************************//**
Gets the state of a block.
@return state */
UNIV_INLINE
enum buf_page_state
buf_page_get_state(
/*===============*/
	const buf_page_t*	bpage);	/*!< in: pointer to the control block */
/*********************************************************************//**
Gets the state of a block.
@return state */
UNIV_INLINE
enum buf_page_state
buf_block_get_state(
/*================*/
	const buf_block_t*	block)	/*!< in: pointer to the control block */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Sets the state of a block. */
UNIV_INLINE
void
buf_page_set_state(
/*===============*/
	buf_page_t*		bpage,	/*!< in/out: pointer to control block */
	enum buf_page_state	state);	/*!< in: state */
/*********************************************************************//**
Sets the state of a block. */
UNIV_INLINE
void
buf_block_set_state(
/*================*/
	buf_block_t*		block,	/*!< in/out: pointer to control block */
	enum buf_page_state	state);	/*!< in: state */
/*********************************************************************//**
Determines if a block is mapped to a tablespace.
@return TRUE if mapped */
UNIV_INLINE
ibool
buf_page_in_file(
/*=============*/
	const buf_page_t*	bpage)	/*!< in: pointer to control block */
	MY_ATTRIBUTE((warn_unused_result));
#ifndef UNIV_HOTBACKUP
/*********************************************************************//**
Determines if a block should be on unzip_LRU list.
@return TRUE if block belongs to unzip_LRU */
UNIV_INLINE
ibool
buf_page_belongs_to_unzip_LRU(
/*==========================*/
	const buf_page_t*	bpage)	/*!< in: pointer to control block */
	MY_ATTRIBUTE((warn_unused_result));

/*********************************************************************//**
Gets the mutex of a block.
@return pointer to mutex protecting bpage */
UNIV_INLINE
BPageMutex*
buf_page_get_mutex(
/*===============*/
	const buf_page_t*	bpage)	/*!< in: pointer to control block */
	MY_ATTRIBUTE((warn_unused_result));

/*********************************************************************//**
Get the flush type of a page.
@return flush type */
UNIV_INLINE
buf_flush_t
buf_page_get_flush_type(
/*====================*/
	const buf_page_t*	bpage)	/*!< in: buffer page */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Set the flush type of a page. */
UNIV_INLINE
void
buf_page_set_flush_type(
/*====================*/
	buf_page_t*	bpage,		/*!< in: buffer page */
	buf_flush_t	flush_type);	/*!< in: flush type */

/** Map a block to a file page.
@param[in,out]	block	pointer to control block
@param[in]	page_id	page id */
UNIV_INLINE
void
buf_block_set_file_page(
	buf_block_t*		block,
	const page_id_t&	page_id);

/*********************************************************************//**
Gets the io_fix state of a block.
@return io_fix state */
UNIV_INLINE
enum buf_io_fix
buf_page_get_io_fix(
/*================*/
	const buf_page_t*	bpage)	/*!< in: pointer to the control block */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Gets the io_fix state of a block.
@return io_fix state */
UNIV_INLINE
enum buf_io_fix
buf_block_get_io_fix(
/*================*/
	const buf_block_t*	block)	/*!< in: pointer to the control block */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Sets the io_fix state of a block. */
UNIV_INLINE
void
buf_page_set_io_fix(
/*================*/
	buf_page_t*	bpage,	/*!< in/out: control block */
	enum buf_io_fix	io_fix);/*!< in: io_fix state */
/*********************************************************************//**
Sets the io_fix state of a block. */
UNIV_INLINE
void
buf_block_set_io_fix(
/*=================*/
	buf_block_t*	block,	/*!< in/out: control block */
	enum buf_io_fix	io_fix);/*!< in: io_fix state */
/*********************************************************************//**
Makes a block sticky. A sticky block implies that even after we release
the buf_pool->mutex and the block->mutex:
* it cannot be removed from the flush_list
* the block descriptor cannot be relocated
* it cannot be removed from the LRU list
Note that:
* the block can still change its position in the LRU list
* the next and previous pointers can change. */
UNIV_INLINE
void
buf_page_set_sticky(
/*================*/
	buf_page_t*	bpage);	/*!< in/out: control block */
/*********************************************************************//**
Removes stickiness of a block. */
UNIV_INLINE
void
buf_page_unset_sticky(
/*==================*/
	buf_page_t*	bpage);	/*!< in/out: control block */
/********************************************************************//**
Determine if a buffer block can be relocated in memory.  The block
can be dirty, but it must not be I/O-fixed or bufferfixed. */
UNIV_INLINE
ibool
buf_page_can_relocate(
/*==================*/
	const buf_page_t*	bpage)	/*!< control block being relocated */
	MY_ATTRIBUTE((warn_unused_result));

/*********************************************************************//**
Determine if a block has been flagged old.
@return TRUE if old */
UNIV_INLINE
ibool
buf_page_is_old(
/*============*/
	const buf_page_t*	bpage)	/*!< in: control block */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Flag a block old. */
UNIV_INLINE
void
buf_page_set_old(
/*=============*/
	buf_page_t*	bpage,	/*!< in/out: control block */
	ibool		old);	/*!< in: old */
/*********************************************************************//**
Determine the time of first access of a block in the buffer pool.
@return ut_time_ms() at the time of first access, 0 if not accessed */
UNIV_INLINE
unsigned
buf_page_is_accessed(
/*=================*/
	const buf_page_t*	bpage)	/*!< in: control block */
	MY_ATTRIBUTE((warn_unused_result));
/*********************************************************************//**
Flag a block accessed. */
UNIV_INLINE
void
buf_page_set_accessed(
/*==================*/
	buf_page_t*	bpage)		/*!< in/out: control block */
	MY_ATTRIBUTE((nonnull));
/*********************************************************************//**
Gets the buf_block_t handle of a buffered file block if an uncompressed
page frame exists, or NULL. Note: even though bpage is not declared a
const we don't update its value.
@return control block, or NULL */
UNIV_INLINE
buf_block_t*
buf_page_get_block(
/*===============*/
	buf_page_t*	bpage)	/*!< in: control block, or NULL */
	MY_ATTRIBUTE((warn_unused_result));
#endif /* !UNIV_HOTBACKUP */
#ifdef UNIV_DEBUG
/*********************************************************************//**
Gets a pointer to the memory frame of a block.
@return pointer to the frame */
UNIV_INLINE
buf_frame_t*
buf_block_get_frame(
/*================*/
	const buf_block_t*	block)	/*!< in: pointer to the control block */
	MY_ATTRIBUTE((warn_unused_result));
#else /* UNIV_DEBUG */
# define buf_block_get_frame(block) (block)->frame
#endif /* UNIV_DEBUG */
/*********************************************************************//**
Gets the compressed page descriptor corresponding to an uncompressed page
if applicable. */
#define buf_block_get_page_zip(block) \
	((block)->page.zip.data ? &(block)->page.zip : NULL)
#ifndef UNIV_HOTBACKUP

/** Get a buffer block from an adaptive hash index pointer.
This function does not return if the block is not identified.
@param[in]	ptr	pointer to within a page frame
@return pointer to block, never NULL */
buf_block_t*
buf_block_from_ahi(const byte* ptr);

/********************************************************************//**
Find out if a pointer belongs to a buf_block_t. It can be a pointer to
the buf_block_t itself or a member of it
@return TRUE if ptr belongs to a buf_block_t struct */
ibool
buf_pointer_is_block_field(
/*=======================*/
	const void*		ptr);	/*!< in: pointer not
					dereferenced */
/** Find out if a pointer corresponds to a buf_block_t::mutex.
@param m in: mutex candidate
@return TRUE if m is a buf_block_t::mutex */
#define buf_pool_is_block_mutex(m)			\
	buf_pointer_is_block_field((const void*)(m))
/** Find out if a pointer corresponds to a buf_block_t::lock.
@param l in: rw-lock candidate
@return TRUE if l is a buf_block_t::lock */
#define buf_pool_is_block_lock(l)			\
	buf_pointer_is_block_field((const void*)(l))

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
	ibool			unzip);

/********************************************************************//**
Completes an asynchronous read or write request of a file page to or from
the buffer pool.
@return true if successful */
bool
buf_page_io_complete(
/*=================*/
	buf_page_t*	bpage,	/*!< in: pointer to the block in question */
	bool		evict = false);/*!< in: whether or not to evict
				the page from LRU list. */
/********************************************************************//**
Calculates the index of a buffer pool to the buf_pool[] array.
@return the position of the buffer pool in buf_pool[] */
UNIV_INLINE
ulint
buf_pool_index(
/*===========*/
	const buf_pool_t*	buf_pool)	/*!< in: buffer pool */
	MY_ATTRIBUTE((warn_unused_result));
/******************************************************************//**
Returns the buffer pool instance given a page instance
@return buf_pool */
UNIV_INLINE
buf_pool_t*
buf_pool_from_bpage(
/*================*/
	const buf_page_t*	bpage); /*!< in: buffer pool page */
/******************************************************************//**
Returns the buffer pool instance given a block instance
@return buf_pool */
UNIV_INLINE
buf_pool_t*
buf_pool_from_block(
/*================*/
	const buf_block_t*	block); /*!< in: block */

/** Returns the buffer pool instance given a page id.
@param[in]	page_id	page id
@return buffer pool */
UNIV_INLINE
buf_pool_t*
buf_pool_get(
	const page_id_t&	page_id);

/******************************************************************//**
Returns the buffer pool instance given its array index
@return buffer pool */
UNIV_INLINE
buf_pool_t*
buf_pool_from_array(
/*================*/
	ulint	index);		/*!< in: array index to get
				buffer pool instance from */

/** Returns the control block of a file page, NULL if not found.
@param[in]	buf_pool	buffer pool instance
@param[in]	page_id		page id
@return block, NULL if not found */
UNIV_INLINE
buf_page_t*
buf_page_hash_get_low(
	buf_pool_t*		buf_pool,
	const page_id_t&	page_id);

/** Returns the control block of a file page, NULL if not found.
If the block is found and lock is not NULL then the appropriate
page_hash lock is acquired in the specified lock mode. Otherwise,
mode value is ignored. It is up to the caller to release the
lock. If the block is found and the lock is NULL then the page_hash
lock is released by this function.
@param[in]	buf_pool	buffer pool instance
@param[in]	page_id		page id
@param[in,out]	lock		lock of the page hash acquired if bpage is
found, NULL otherwise. If NULL is passed then the hash_lock is released by
this function.
@param[in]	lock_mode	RW_LOCK_X or RW_LOCK_S. Ignored if
lock == NULL
@param[in]	watch		if true, return watch sentinel also.
@return pointer to the bpage or NULL; if NULL, lock is also NULL or
a watch sentinel. */
UNIV_INLINE
buf_page_t*
buf_page_hash_get_locked(
	buf_pool_t*		buf_pool,
	const page_id_t&	page_id,
	rw_lock_t**		lock,
	ulint			lock_mode,
	bool			watch = false);

/** Returns the control block of a file page, NULL if not found.
If the block is found and lock is not NULL then the appropriate
page_hash lock is acquired in the specified lock mode. Otherwise,
mode value is ignored. It is up to the caller to release the
lock. If the block is found and the lock is NULL then the page_hash
lock is released by this function.
@param[in]	buf_pool	buffer pool instance
@param[in]	page_id		page id
@param[in,out]	lock		lock of the page hash acquired if bpage is
found, NULL otherwise. If NULL is passed then the hash_lock is released by
this function.
@param[in]	lock_mode	RW_LOCK_X or RW_LOCK_S. Ignored if
lock == NULL
@return pointer to the block or NULL; if NULL, lock is also NULL. */
UNIV_INLINE
buf_block_t*
buf_block_hash_get_locked(
	buf_pool_t*		buf_pool,
	const page_id_t&	page_id,
	rw_lock_t**		lock,
	ulint			lock_mode);

/* There are four different ways we can try to get a bpage or block
from the page hash:
1) Caller already holds the appropriate page hash lock: in the case call
buf_page_hash_get_low() function.
2) Caller wants to hold page hash lock in x-mode
3) Caller wants to hold page hash lock in s-mode
4) Caller doesn't want to hold page hash lock */
#define buf_page_hash_get_s_locked(b, page_id, l)		\
	buf_page_hash_get_locked(b, page_id, l, RW_LOCK_S)
#define buf_page_hash_get_x_locked(b, page_id, l)		\
	buf_page_hash_get_locked(b, page_id, l, RW_LOCK_X)
#define buf_page_hash_get(b, page_id)				\
	buf_page_hash_get_locked(b, page_id, NULL, 0)
#define buf_page_get_also_watch(b, page_id)			\
	buf_page_hash_get_locked(b, page_id, NULL, 0, true)

#define buf_block_hash_get_s_locked(b, page_id, l)		\
	buf_block_hash_get_locked(b, page_id, l, RW_LOCK_S)
#define buf_block_hash_get_x_locked(b, page_id, l)		\
	buf_block_hash_get_locked(b, page_id, l, RW_LOCK_X)
#define buf_block_hash_get(b, page_id)				\
	buf_block_hash_get_locked(b, page_id, NULL, 0)

/*********************************************************************//**
Gets the current length of the free list of buffer blocks.
@return length of the free list */
ulint
buf_get_free_list_len(void);
/*=======================*/

/********************************************************************//**
Determine if a block is a sentinel for a buffer pool watch.
@return TRUE if a sentinel for a buffer pool watch, FALSE if not */
ibool
buf_pool_watch_is_sentinel(
/*=======================*/
	const buf_pool_t*	buf_pool,	/*!< buffer pool instance */
	const buf_page_t*	bpage)		/*!< in: block */
	MY_ATTRIBUTE((nonnull, warn_unused_result));

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
MY_ATTRIBUTE((warn_unused_result));

/** Stop watching if the page has been read in.
buf_pool_watch_set(space,offset) must have returned NULL before.
@param[in]	page_id	page id */
void
buf_pool_watch_unset(
	const page_id_t&	page_id);

/** Check if the page has been read in.
This may only be called after buf_pool_watch_set(space,offset)
has returned NULL and before invoking buf_pool_watch_unset(space,offset).
@param[in]	page_id	page id
@return FALSE if the given page was not read in, TRUE if it was */
ibool
buf_pool_watch_occurred(
	const page_id_t&	page_id)
MY_ATTRIBUTE((warn_unused_result));

/********************************************************************//**
Get total buffer pool statistics. */
void
buf_get_total_list_len(
/*===================*/
	ulint*		LRU_len,	/*!< out: length of all LRU lists */
	ulint*		free_len,	/*!< out: length of all free lists */
	ulint*		flush_list_len);/*!< out: length of all flush lists */
/********************************************************************//**
Get total list size in bytes from all buffer pools. */
void
buf_get_total_list_size_in_bytes(
/*=============================*/
	buf_pools_list_size_t*	buf_pools_list_size);	/*!< out: list sizes
							in all buffer pools */
/********************************************************************//**
Get total buffer pool statistics. */
void
buf_get_total_stat(
/*===============*/
	buf_pool_stat_t*tot_stat);	/*!< out: buffer pool stats */
/*********************************************************************//**
Get the nth chunk's buffer block in the specified buffer pool.
@return the nth chunk's buffer block. */
UNIV_INLINE
buf_block_t*
buf_get_nth_chunk_block(
/*====================*/
	const buf_pool_t* buf_pool,	/*!< in: buffer pool instance */
	ulint		n,		/*!< in: nth chunk in the buffer pool */
	ulint*		chunk_size);	/*!< in: chunk size */

/** Verify the possibility that a stored page is not in buffer pool.
@param[in]	withdraw_clock	withdraw clock when stored the page
@retval true	if the page might be relocated */
UNIV_INLINE
bool
buf_pool_is_obsolete(
	ulint	withdraw_clock);

/** Calculate aligned buffer pool size based on srv_buf_pool_chunk_unit,
if needed.
@param[in]	size	size in bytes
@return	aligned size */
UNIV_INLINE
ulint
buf_pool_size_align(
	ulint	size);

/** Calculate the checksum of a page from compressed table and update the
page.
@param[in,out]	page	page to update
@param[in]	size	compressed page size
@param[in]	lsn	LSN to stamp on the page */
void
buf_flush_update_zip_checksum(
	buf_frame_t*	page,
	ulint		size,
	lsn_t		lsn);

#endif /* !UNIV_HOTBACKUP */

/** The common buffer control block structure
for compressed and uncompressed frames */

/** Number of bits used for buffer page states. */
#define BUF_PAGE_STATE_BITS	3

class buf_page_t {
public:
	/** @name General fields
	None of these bit-fields must be modified without holding
	buf_page_get_mutex() [buf_block_t::mutex or
	buf_pool->zip_mutex], since they can be stored in the same
	machine word.  Some of these fields are additionally protected
	by buf_pool->mutex. */
	/* @{ */

	/** Page id. Protected by buf_pool mutex. */
	page_id_t	id;

	/** Page size. Protected by buf_pool mutex. */
	page_size_t	size;

	/** Count of how manyfold this block is currently bufferfixed. */
	ib_uint32_t	buf_fix_count;

	/** type of pending I/O operation; also protected by
	buf_pool->mutex for writes only */
	buf_io_fix	io_fix;

	/** Block state. @see buf_page_in_file */
	buf_page_state	state;

#ifndef UNIV_HOTBACKUP
	unsigned	flush_type:2;	/*!< if this block is currently being
					flushed to disk, this tells the
					flush_type.
					@see buf_flush_t */
	unsigned	buf_pool_index:6;/*!< index number of the buffer pool
					that this block belongs to */
# if MAX_BUFFER_POOLS > 64
#  error "MAX_BUFFER_POOLS > 64; redefine buf_pool_index:6"
# endif
	/* @} */
#endif /* !UNIV_HOTBACKUP */
	page_zip_des_t	zip;		/*!< compressed page; zip.data
					(but not the data it points to) is
					also protected by buf_pool->mutex;
					state == BUF_BLOCK_ZIP_PAGE and
					zip.data == NULL means an active
					buf_pool->watch */
#ifndef UNIV_HOTBACKUP
	buf_page_t*	hash;		/*!< node used in chaining to
					buf_pool->page_hash or
					buf_pool->zip_hash */
#ifdef UNIV_DEBUG
	ibool		in_page_hash;	/*!< TRUE if in buf_pool->page_hash */
	ibool		in_zip_hash;	/*!< TRUE if in buf_pool->zip_hash */
#endif /* UNIV_DEBUG */

	/** @name Page flushing fields
	All these are protected by buf_pool->mutex. */
	/* @{ */

	UT_LIST_NODE_T(buf_page_t) list;
					/*!< based on state, this is a
					list node, protected either by
					buf_pool->mutex or by
					buf_pool->flush_list_mutex,
					in one of the following lists in
					buf_pool:

					- BUF_BLOCK_NOT_USED:	free, withdraw
					- BUF_BLOCK_FILE_PAGE:	flush_list
					- BUF_BLOCK_ZIP_DIRTY:	flush_list
					- BUF_BLOCK_ZIP_PAGE:	zip_clean

					If bpage is part of flush_list
					then the node pointers are
					covered by buf_pool->flush_list_mutex.
					Otherwise these pointers are
					protected by buf_pool->mutex.

					The contents of the list node
					is undefined if !in_flush_list
					&& state == BUF_BLOCK_FILE_PAGE,
					or if state is one of
					BUF_BLOCK_MEMORY,
					BUF_BLOCK_REMOVE_HASH or
					BUF_BLOCK_READY_IN_USE. */

#ifdef UNIV_DEBUG
	ibool		in_flush_list;	/*!< TRUE if in buf_pool->flush_list;
					when buf_pool->flush_list_mutex is
					free, the following should hold:
					in_flush_list
					== (state == BUF_BLOCK_FILE_PAGE
					    || state == BUF_BLOCK_ZIP_DIRTY)
					Writes to this field must be
					covered by both block->mutex
					and buf_pool->flush_list_mutex. Hence
					reads can happen while holding
					any one of the two mutexes */
	ibool		in_free_list;	/*!< TRUE if in buf_pool->free; when
					buf_pool->mutex is free, the following
					should hold: in_free_list
					== (state == BUF_BLOCK_NOT_USED) */
#endif /* UNIV_DEBUG */

	FlushObserver*	flush_observer;	/*!< flush observer */

	lsn_t		newest_modification;
					/*!< log sequence number of
					the youngest modification to
					this block, zero if not
					modified. Protected by block
					mutex */
	lsn_t		oldest_modification;
					/*!< log sequence number of
					the START of the log entry
					written of the oldest
					modification to this block
					which has not yet been flushed
					on disk; zero if all
					modifications are on disk.
					Writes to this field must be
					covered by both block->mutex
					and buf_pool->flush_list_mutex. Hence
					reads can happen while holding
					any one of the two mutexes */
	/* @} */
	/** @name LRU replacement algorithm fields
	These fields are protected by buf_pool->mutex only (not
	buf_pool->zip_mutex or buf_block_t::mutex). */
	/* @{ */

	UT_LIST_NODE_T(buf_page_t) LRU;
					/*!< node of the LRU list */
#ifdef UNIV_DEBUG
	ibool		in_LRU_list;	/*!< TRUE if the page is in
					the LRU list; used in
					debugging */
#endif /* UNIV_DEBUG */
	unsigned	old:1;		/*!< TRUE if the block is in the old
					blocks in buf_pool->LRU_old */
	unsigned	freed_page_clock:31;/*!< the value of
					buf_pool->freed_page_clock
					when this block was the last
					time put to the head of the
					LRU list; a thread is allowed
					to read this for heuristic
					purposes without holding any
					mutex or latch */
	/* @} */
	unsigned	access_time;	/*!< time of first access, or
					0 if the block was never accessed
					in the buffer pool. Protected by
					block mutex */
# ifdef UNIV_DEBUG
	ibool		file_page_was_freed;
					/*!< this is set to TRUE when
					fsp frees a page in buffer pool;
					protected by buf_pool->zip_mutex
					or buf_block_t::mutex. */
# endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */
};

/** The buffer control block structure */

struct buf_block_t{

	/** @name General fields */
	/* @{ */

	buf_page_t	page;		/*!< page information; this must
					be the first field, so that
					buf_pool->page_hash can point
					to buf_page_t or buf_block_t */
	byte*		frame;		/*!< pointer to buffer frame which
					is of size UNIV_PAGE_SIZE, and
					aligned to an address divisible by
					UNIV_PAGE_SIZE */
#ifndef UNIV_HOTBACKUP
	BPageLock	lock;		/*!< read-write lock of the buffer
					frame */
	UT_LIST_NODE_T(buf_block_t) unzip_LRU;
					/*!< node of the decompressed LRU list;
					a block is in the unzip_LRU list
					if page.state == BUF_BLOCK_FILE_PAGE
					and page.zip.data != NULL */
#ifdef UNIV_DEBUG
	ibool		in_unzip_LRU_list;/*!< TRUE if the page is in the
					decompressed LRU list;
					used in debugging */
	ibool		in_withdraw_list;
#endif /* UNIV_DEBUG */
	unsigned	lock_hash_val:32;/*!< hashed value of the page address
					in the record lock hash table;
					protected by buf_block_t::lock
					(or buf_block_t::mutex, buf_pool->mutex
				        in buf_page_get_gen(),
					buf_page_init_for_read()
					and buf_page_create()) */
	/* @} */
	/** @name Optimistic search field */
	/* @{ */

	ib_uint64_t	modify_clock;	/*!< this clock is incremented every
					time a pointer to a record on the
					page may become obsolete; this is
					used in the optimistic cursor
					positioning: if the modify clock has
					not changed, we know that the pointer
					is still valid; this field may be
					changed if the thread (1) owns the
					pool mutex and the page is not
					bufferfixed, or (2) the thread has an
					x-latch on the block */
	/* @} */
	/** @name Hash search fields (unprotected)
	NOTE that these fields are NOT protected by any semaphore! */
	/* @{ */

	ulint		n_hash_helps;	/*!< counter which controls building
					of a new hash index for the page */
	volatile ulint	n_bytes;	/*!< recommended prefix length for hash
					search: number of bytes in
					an incomplete last field */
	volatile ulint	n_fields;	/*!< recommended prefix length for hash
					search: number of full fields */
	volatile bool	left_side;	/*!< true or false, depending on
					whether the leftmost record of several
					records with the same prefix should be
					indexed in the hash index */
	/* @} */

	/** @name Hash search fields
	These 5 fields may only be modified when:
	we are holding the appropriate x-latch in btr_search_latches[], and
	one of the following holds:
	(1) the block state is BUF_BLOCK_FILE_PAGE, and
	we are holding an s-latch or x-latch on buf_block_t::lock, or
	(2) buf_block_t::buf_fix_count == 0, or
	(3) the block state is BUF_BLOCK_REMOVE_HASH.

	An exception to this is when we init or create a page
	in the buffer pool in buf0buf.cc.

	Another exception for buf_pool_clear_hash_index() is that
	assigning block->index = NULL (and block->n_pointers = 0)
	is allowed whenever btr_search_own_all(RW_LOCK_X).

	Another exception is that ha_insert_for_fold_func() may
	decrement n_pointers without holding the appropriate latch
	in btr_search_latches[]. Thus, n_pointers must be
	protected by atomic memory access.

	This implies that the fields may be read without race
	condition whenever any of the following hold:
	- the btr_search_latches[] s-latch or x-latch is being held, or
	- the block state is not BUF_BLOCK_FILE_PAGE or BUF_BLOCK_REMOVE_HASH,
	and holding some latch prevents the state from changing to that.

	Some use of assert_block_ahi_empty() or assert_block_ahi_valid()
	is prone to race conditions while buf_pool_clear_hash_index() is
	executing (the adaptive hash index is being disabled). Such use
	is explicitly commented. */

	/* @{ */

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	ulint		n_pointers;	/*!< used in debugging: the number of
					pointers in the adaptive hash index
					pointing to this frame;
					protected by atomic memory access
					or btr_search_own_all(). */
# define assert_block_ahi_empty(block)					\
	ut_a(os_atomic_increment_ulint(&(block)->n_pointers, 0) == 0)
# define assert_block_ahi_empty_on_init(block) do {			\
	UNIV_MEM_VALID(&(block)->n_pointers, sizeof (block)->n_pointers); \
	assert_block_ahi_empty(block);					\
} while (0)
# define assert_block_ahi_valid(block)					\
	ut_a((block)->index						\
	     || os_atomic_increment_ulint(&(block)->n_pointers, 0) == 0)
#else /* UNIV_AHI_DEBUG || UNIV_DEBUG */
# define assert_block_ahi_empty(block) /* nothing */
# define assert_block_ahi_empty_on_init(block) /* nothing */
# define assert_block_ahi_valid(block) /* nothing */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	unsigned	curr_n_fields:10;/*!< prefix length for hash indexing:
					number of full fields */
	unsigned	curr_n_bytes:15;/*!< number of bytes in hash
					indexing */
	unsigned	curr_left_side:1;/*!< TRUE or FALSE in hash indexing */
	dict_index_t*	index;		/*!< Index for which the
					adaptive hash index has been
					created, or NULL if the page
					does not exist in the
					index. Note that it does not
					guarantee that the index is
					complete, though: there may
					have been hash collisions,
					record deletions, etc. */
	/* @} */
	bool		made_dirty_with_no_latch;
					/*!< true if block has been made dirty
					without acquiring X/SX latch as the
					block belongs to temporary tablespace
					and block is always accessed by a
					single thread. */
	bool		skip_flush_check;
					/*!< Skip check in buf_dblwr_check_block
					during bulk load, protected by lock.*/
# ifdef UNIV_DEBUG
	/** @name Debug fields */
	/* @{ */
	rw_lock_t	debug_latch;	/*!< in the debug version, each thread
					which bufferfixes the block acquires
					an s-latch here; so we can use the
					debug utilities in sync0rw */
	/* @} */
# endif
	BPageMutex	mutex;		/*!< mutex protecting this block:
					state (also protected by the buffer
					pool mutex), io_fix, buf_fix_count,
					and accessed; we introduce this new
					mutex in InnoDB-5.1 to relieve
					contention on the buffer pool mutex */
#endif /* !UNIV_HOTBACKUP */
};

/** Check if a buf_block_t object is in a valid state
@param block buffer block
@return TRUE if valid */
#define buf_block_state_valid(block)				\
(buf_block_get_state(block) >= BUF_BLOCK_NOT_USED		\
 && (buf_block_get_state(block) <= BUF_BLOCK_REMOVE_HASH))

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Compute the hash fold value for blocks in buf_pool->zip_hash. */
/* @{ */
#define BUF_POOL_ZIP_FOLD_PTR(ptr) ((ulint) (ptr) / UNIV_PAGE_SIZE)
#define BUF_POOL_ZIP_FOLD(b) BUF_POOL_ZIP_FOLD_PTR((b)->frame)
#define BUF_POOL_ZIP_FOLD_BPAGE(b) BUF_POOL_ZIP_FOLD((buf_block_t*) (b))
/* @} */

/** A "Hazard Pointer" class used to iterate over page lists
inside the buffer pool. A hazard pointer is a buf_page_t pointer
which we intend to iterate over next and we want it remain valid
even after we release the buffer pool mutex. */
class HazardPointer {

public:
	/** Constructor
	@param buf_pool buffer pool instance
	@param mutex	mutex that is protecting the hp. */
	HazardPointer(const buf_pool_t* buf_pool, const ib_mutex_t* mutex)
		:
		m_buf_pool(buf_pool)
#ifdef UNIV_DEBUG
		, m_mutex(mutex)
#endif /* UNIV_DEBUG */
		, m_hp() {}

	/** Destructor */
	virtual ~HazardPointer() {}

	/** Get current value */
	buf_page_t* get() const
	{
		ut_ad(mutex_own(m_mutex));
		return(m_hp);
	}

	/** Set current value
	@param bpage	buffer block to be set as hp */
	void set(buf_page_t* bpage);

	/** Checks if a bpage is the hp
	@param bpage	buffer block to be compared
	@return true if it is hp */
	bool is_hp(const buf_page_t* bpage);

	/** Adjust the value of hp. This happens when some
	other thread working on the same list attempts to
	remove the hp from the list. Must be implemented
	by the derived classes.
	@param bpage	buffer block to be compared */
	virtual void adjust(const buf_page_t*) = 0;

protected:
	/** Disable copying */
	HazardPointer(const HazardPointer&);
	HazardPointer& operator=(const HazardPointer&);

	/** Buffer pool instance */
	const buf_pool_t*	m_buf_pool;

#ifdef UNIV_DEBUG
	/** mutex that protects access to the m_hp. */
	const ib_mutex_t*	m_mutex;
#endif /* UNIV_DEBUG */

	/** hazard pointer. */
	buf_page_t*		m_hp;
};

/** Class implementing buf_pool->flush_list hazard pointer */
class FlushHp: public HazardPointer {

public:
	/** Constructor
	@param buf_pool buffer pool instance
	@param mutex	mutex that is protecting the hp. */
	FlushHp(const buf_pool_t* buf_pool, const ib_mutex_t* mutex)
		:
		HazardPointer(buf_pool, mutex) {}

	/** Destructor */
	virtual ~FlushHp() {}

	/** Adjust the value of hp. This happens when some
	other thread working on the same list attempts to
	remove the hp from the list.
	@param bpage	buffer block to be compared */
	void adjust(const buf_page_t* bpage);
};

/** Class implementing buf_pool->LRU hazard pointer */
class LRUHp: public HazardPointer {

public:
	/** Constructor
	@param buf_pool buffer pool instance
	@param mutex	mutex that is protecting the hp. */
	LRUHp(const buf_pool_t* buf_pool, const ib_mutex_t* mutex)
		:
		HazardPointer(buf_pool, mutex) {}

	/** Destructor */
	virtual ~LRUHp() {}

	/** Adjust the value of hp. This happens when some
	other thread working on the same list attempts to
	remove the hp from the list.
	@param bpage	buffer block to be compared */
	void adjust(const buf_page_t* bpage);
};

/** Special purpose iterators to be used when scanning the LRU list.
The idea is that when one thread finishes the scan it leaves the
itr in that position and the other thread can start scan from
there */
class LRUItr: public LRUHp {

public:
	/** Constructor
	@param buf_pool buffer pool instance
	@param mutex	mutex that is protecting the hp. */
	LRUItr(const buf_pool_t* buf_pool, const ib_mutex_t* mutex)
		:
		LRUHp(buf_pool, mutex) {}

	/** Destructor */
	virtual ~LRUItr() {}

	/** Selects from where to start a scan. If we have scanned
	too deep into the LRU list it resets the value to the tail
	of the LRU list.
	@return buf_page_t from where to start scan. */
	buf_page_t* start();
};

/** Struct that is embedded in the free zip blocks */
struct buf_buddy_free_t {
	union {
		ulint	size;	/*!< size of the block */
		byte	bytes[FIL_PAGE_DATA];
				/*!< stamp[FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID]
				== BUF_BUDDY_FREE_STAMP denotes a free
				block. If the space_id field of buddy
				block != BUF_BUDDY_FREE_STAMP, the block
				is not in any zip_free list. If the
				space_id is BUF_BUDDY_FREE_STAMP then
				stamp[0] will contain the
				buddy block size. */
	} stamp;

	buf_page_t	bpage;	/*!< Embedded bpage descriptor */
	UT_LIST_NODE_T(buf_buddy_free_t) list;
				/*!< Node of zip_free list */
};

/** @brief The buffer pool statistics structure. */
struct buf_pool_stat_t{
	ulint	n_page_gets;	/*!< number of page gets performed;
				also successful searches through
				the adaptive hash index are
				counted as page gets; this field
				is NOT protected by the buffer
				pool mutex */
	ulint	n_pages_read;	/*!< number read operations */
	ulint	n_pages_written;/*!< number write operations */
	ulint	n_pages_created;/*!< number of pages created
				in the pool with no read */
	ulint	n_ra_pages_read_rnd;/*!< number of pages read in
				as part of random read ahead */
	ulint	n_ra_pages_read;/*!< number of pages read in
				as part of read ahead */
	ulint	n_ra_pages_evicted;/*!< number of read ahead
				pages that are evicted without
				being accessed */
	ulint	n_pages_made_young; /*!< number of pages made young, in
				calls to buf_LRU_make_block_young() */
	ulint	n_pages_not_made_young; /*!< number of pages not made
				young because the first access
				was not long enough ago, in
				buf_page_peek_if_too_old() */
	ulint	LRU_bytes;	/*!< LRU size in bytes */
	ulint	flush_list_bytes;/*!< flush_list size in bytes */
};

/** Statistics of buddy blocks of a given size. */
struct buf_buddy_stat_t {
	/** Number of blocks allocated from the buddy system. */
	ulint		used;
	/** Number of blocks relocated by the buddy system. */
	ib_uint64_t	relocated;
	/** Total duration of block relocations, in microseconds. */
	ib_uint64_t	relocated_usec;
};

/** @brief The buffer pool structure.

NOTE! The definition appears here only for other modules of this
directory (buf) to see it. Do not use from outside! */

struct buf_pool_t{

	/** @name General fields */
	/* @{ */
	BufPoolMutex	mutex;		/*!< Buffer pool mutex of this
					instance */
	BufPoolZipMutex	zip_mutex;	/*!< Zip mutex of this buffer
					pool instance, protects compressed
					only pages (of type buf_page_t, not
					buf_block_t */
	ulint		instance_no;	/*!< Array index of this buffer
					pool instance */
	ulint		curr_pool_size;	/*!< Current pool size in bytes */
	ulint		LRU_old_ratio;  /*!< Reserve this much of the buffer
					pool for "old" blocks */
#ifdef UNIV_DEBUG
	ulint		buddy_n_frames; /*!< Number of frames allocated from
					the buffer pool to the buddy system */
#endif
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	ulint		mutex_exit_forbidden; /*!< Forbid release mutex */
#endif
	ut_allocator<unsigned char>	allocator;	/*!< Allocator used for
					allocating memory for the the "chunks"
					member. */
	volatile ulint	n_chunks;	/*!< number of buffer pool chunks */
	volatile ulint	n_chunks_new;	/*!< new number of buffer pool chunks */
	buf_chunk_t*	chunks;		/*!< buffer pool chunks */
	buf_chunk_t*	chunks_old;	/*!< old buffer pool chunks to be freed
					after resizing buffer pool */
	ulint		curr_size;	/*!< current pool size in pages */
	ulint		old_size;	/*!< previous pool size in pages */
	ulint		read_ahead_area;/*!< size in pages of the area which
					the read-ahead algorithms read if
					invoked */
	hash_table_t*	page_hash;	/*!< hash table of buf_page_t or
					buf_block_t file pages,
					buf_page_in_file() == TRUE,
					indexed by (space_id, offset).
					page_hash is protected by an
					array of mutexes.
					Changes in page_hash are protected
					by buf_pool->mutex and the relevant
					page_hash mutex. Lookups can happen
					while holding the buf_pool->mutex or
					the relevant page_hash mutex. */
	hash_table_t*	page_hash_old;	/*!< old pointer to page_hash to be
					freed after resizing buffer pool */
	hash_table_t*	zip_hash;	/*!< hash table of buf_block_t blocks
					whose frames are allocated to the
					zip buddy system,
					indexed by block->frame */
	ulint		n_pend_reads;	/*!< number of pending read
					operations */
	ulint		n_pend_unzip;	/*!< number of pending decompressions */

	time_t		last_printout_time;
					/*!< when buf_print_io was last time
					called */
	buf_buddy_stat_t buddy_stat[BUF_BUDDY_SIZES_MAX + 1];
					/*!< Statistics of buddy system,
					indexed by block size */
	buf_pool_stat_t	stat;		/*!< current statistics */
	buf_pool_stat_t	old_stat;	/*!< old statistics */

	/* @} */

	/** @name Page flushing algorithm fields */

	/* @{ */

	FlushListMutex	flush_list_mutex;/*!< mutex protecting the
					flush list access. This mutex
					protects flush_list, flush_rbt
					and bpage::list pointers when
					the bpage is on flush_list. It
					also protects writes to
					bpage::oldest_modification and
					flush_list_hp */
	FlushHp			flush_hp;/*!< "hazard pointer"
					used during scan of flush_list
					while doing flush list batch.
					Protected by flush_list_mutex */
	UT_LIST_BASE_NODE_T(buf_page_t) flush_list;
					/*!< base node of the modified block
					list */
	ibool		init_flush[BUF_FLUSH_N_TYPES];
					/*!< this is TRUE when a flush of the
					given type is being initialized */
	ulint		n_flush[BUF_FLUSH_N_TYPES];
					/*!< this is the number of pending
					writes in the given flush type */
	os_event_t	no_flush[BUF_FLUSH_N_TYPES];
					/*!< this is in the set state
					when there is no flush batch
					of the given type running */
	ib_rbt_t*	flush_rbt;	/*!< a red-black tree is used
					exclusively during recovery to
					speed up insertions in the
					flush_list. This tree contains
					blocks in order of
					oldest_modification LSN and is
					kept in sync with the
					flush_list.
					Each member of the tree MUST
					also be on the flush_list.
					This tree is relevant only in
					recovery and is set to NULL
					once the recovery is over.
					Protected by flush_list_mutex */
	ulint		freed_page_clock;/*!< a sequence number used
					to count the number of buffer
					blocks removed from the end of
					the LRU list; NOTE that this
					counter may wrap around at 4
					billion! A thread is allowed
					to read this for heuristic
					purposes without holding any
					mutex or latch */
	ibool		try_LRU_scan;	/*!< Set to FALSE when an LRU
					scan for free block fails. This
					flag is used to avoid repeated
					scans of LRU list when we know
					that there is no free block
					available in the scan depth for
					eviction. Set to TRUE whenever
					we flush a batch from the
					buffer pool. Protected by the
					buf_pool->mutex */
	/* @} */

	/** @name LRU replacement algorithm fields */
	/* @{ */

	UT_LIST_BASE_NODE_T(buf_page_t) free;
					/*!< base node of the free
					block list */

	UT_LIST_BASE_NODE_T(buf_page_t) withdraw;
					/*!< base node of the withdraw
					block list. It is only used during
					shrinking buffer pool size, not to
					reuse the blocks will be removed */

	ulint		withdraw_target;/*!< target length of withdraw
					block list, when withdrawing */

	/** "hazard pointer" used during scan of LRU while doing
	LRU list batch.  Protected by buf_pool::mutex */
	LRUHp		lru_hp;

	/** Iterator used to scan the LRU list when searching for
	replacable victim. Protected by buf_pool::mutex. */
	LRUItr		lru_scan_itr;

	/** Iterator used to scan the LRU list when searching for
	single page flushing victim.  Protected by buf_pool::mutex. */
	LRUItr		single_scan_itr;

	UT_LIST_BASE_NODE_T(buf_page_t) LRU;
					/*!< base node of the LRU list */

	buf_page_t*	LRU_old;	/*!< pointer to the about
					LRU_old_ratio/BUF_LRU_OLD_RATIO_DIV
					oldest blocks in the LRU list;
					NULL if LRU length less than
					BUF_LRU_OLD_MIN_LEN;
					NOTE: when LRU_old != NULL, its length
					should always equal LRU_old_len */
	ulint		LRU_old_len;	/*!< length of the LRU list from
					the block to which LRU_old points
					onward, including that block;
					see buf0lru.cc for the restrictions
					on this value; 0 if LRU_old == NULL;
					NOTE: LRU_old_len must be adjusted
					whenever LRU_old shrinks or grows! */

	UT_LIST_BASE_NODE_T(buf_block_t) unzip_LRU;
					/*!< base node of the
					unzip_LRU list */

	/* @} */
	/** @name Buddy allocator fields
	The buddy allocator is used for allocating compressed page
	frames and buf_page_t descriptors of blocks that exist
	in the buffer pool only in compressed form. */
	/* @{ */
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
	UT_LIST_BASE_NODE_T(buf_page_t)	zip_clean;
					/*!< unmodified compressed pages */
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
	UT_LIST_BASE_NODE_T(buf_buddy_free_t) zip_free[BUF_BUDDY_SIZES_MAX];
					/*!< buddy free lists */

	buf_page_t*			watch;
					/*!< Sentinel records for buffer
					pool watches. Protected by
					buf_pool->mutex. */

#if BUF_BUDDY_LOW > UNIV_ZIP_SIZE_MIN
# error "BUF_BUDDY_LOW > UNIV_ZIP_SIZE_MIN"
#endif
	/* @} */
};

/** Print the given buf_pool_t object.
@param[in,out]	out		the output stream
@param[in]	buf_pool	the buf_pool_t object to be printed
@return the output stream */
std::ostream&
operator<<(
        std::ostream&		out,
        const buf_pool_t&	buf_pool);

/** @name Accessors for buf_pool->mutex.
Use these instead of accessing buf_pool->mutex directly. */
/* @{ */

/** Test if a buffer pool mutex is owned. */
#define buf_pool_mutex_own(b) mutex_own(&b->mutex)
/** Acquire a buffer pool mutex. */
#define buf_pool_mutex_enter(b) do {		\
	ut_ad(!(b)->zip_mutex.is_owned());	\
	mutex_enter(&(b)->mutex);		\
} while (0)

/** Test if flush list mutex is owned. */
#define buf_flush_list_mutex_own(b) mutex_own(&(b)->flush_list_mutex)

/** Acquire the flush list mutex. */
#define buf_flush_list_mutex_enter(b) do {	\
	mutex_enter(&(b)->flush_list_mutex);	\
} while (0)
/** Release the flush list mutex. */
# define buf_flush_list_mutex_exit(b) do {	\
	mutex_exit(&(b)->flush_list_mutex);	\
} while (0)


/** Test if block->mutex is owned. */
#define buf_page_mutex_own(b)	(b)->mutex.is_owned()

/** Acquire the block->mutex. */
#define buf_page_mutex_enter(b) do {			\
	mutex_enter(&(b)->mutex);			\
} while (0)

/** Release the trx->mutex. */
#define buf_page_mutex_exit(b) do {			\
	(b)->mutex.exit();				\
} while (0)


/** Get appropriate page_hash_lock. */
# define buf_page_hash_lock_get(buf_pool, page_id)	\
	hash_get_lock((buf_pool)->page_hash, (page_id).fold())

/** If not appropriate page_hash_lock, relock until appropriate. */
# define buf_page_hash_lock_s_confirm(hash_lock, buf_pool, page_id)\
	hash_lock_s_confirm(hash_lock, (buf_pool)->page_hash, (page_id).fold())

# define buf_page_hash_lock_x_confirm(hash_lock, buf_pool, page_id)\
	hash_lock_x_confirm(hash_lock, (buf_pool)->page_hash, (page_id).fold())

#ifdef UNIV_DEBUG
/** Test if page_hash lock is held in s-mode. */
# define buf_page_hash_lock_held_s(buf_pool, bpage)	\
	rw_lock_own(buf_page_hash_lock_get((buf_pool), (bpage)->id), RW_LOCK_S)

/** Test if page_hash lock is held in x-mode. */
# define buf_page_hash_lock_held_x(buf_pool, bpage)	\
	rw_lock_own(buf_page_hash_lock_get((buf_pool), (bpage)->id), RW_LOCK_X)

/** Test if page_hash lock is held in x or s-mode. */
# define buf_page_hash_lock_held_s_or_x(buf_pool, bpage)\
	(buf_page_hash_lock_held_s((buf_pool), (bpage))	\
	 || buf_page_hash_lock_held_x((buf_pool), (bpage)))

# define buf_block_hash_lock_held_s(buf_pool, block)	\
	buf_page_hash_lock_held_s((buf_pool), &(block)->page)

# define buf_block_hash_lock_held_x(buf_pool, block)	\
	buf_page_hash_lock_held_x((buf_pool), &(block)->page)

# define buf_block_hash_lock_held_s_or_x(buf_pool, block)	\
	buf_page_hash_lock_held_s_or_x((buf_pool), &(block)->page)
#else /* UNIV_DEBUG */
# define buf_page_hash_lock_held_s(b, p)	(TRUE)
# define buf_page_hash_lock_held_x(b, p)	(TRUE)
# define buf_page_hash_lock_held_s_or_x(b, p)	(TRUE)
# define buf_block_hash_lock_held_s(b, p)	(TRUE)
# define buf_block_hash_lock_held_x(b, p)	(TRUE)
# define buf_block_hash_lock_held_s_or_x(b, p)	(TRUE)
#endif /* UNIV_DEBUG */

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Forbid the release of the buffer pool mutex. */
# define buf_pool_mutex_exit_forbid(b) do {	\
	ut_ad(buf_pool_mutex_own(b));		\
	b->mutex_exit_forbidden++;		\
} while (0)
/** Allow the release of the buffer pool mutex. */
# define buf_pool_mutex_exit_allow(b) do {	\
	ut_ad(buf_pool_mutex_own(b));		\
	ut_a(b->mutex_exit_forbidden);	\
	b->mutex_exit_forbidden--;		\
} while (0)
/** Release the buffer pool mutex. */
# define buf_pool_mutex_exit(b) do {		\
	ut_a(!b->mutex_exit_forbidden);		\
	mutex_exit(&b->mutex);			\
} while (0)
#else
/** Forbid the release of the buffer pool mutex. */
# define buf_pool_mutex_exit_forbid(b) ((void) 0)
/** Allow the release of the buffer pool mutex. */
# define buf_pool_mutex_exit_allow(b) ((void) 0)
/** Release the buffer pool mutex. */
# define buf_pool_mutex_exit(b) mutex_exit(&b->mutex)
#endif
#endif /* !UNIV_HOTBACKUP */
/* @} */

/**********************************************************************
Let us list the consistency conditions for different control block states.

NOT_USED:	is in free list, not in LRU list, not in flush list, nor
		page hash table
READY_FOR_USE:	is not in free list, LRU list, or flush list, nor page
		hash table
MEMORY:		is not in free list, LRU list, or flush list, nor page
		hash table
FILE_PAGE:	space and offset are defined, is in page hash table
		if io_fix == BUF_IO_WRITE,
			pool: no_flush[flush_type] is in reset state,
			pool: n_flush[flush_type] > 0

		(1) if buf_fix_count == 0, then
			is in LRU list, not in free list
			is in flush list,
				if and only if oldest_modification > 0
			is x-locked,
				if and only if io_fix == BUF_IO_READ
			is s-locked,
				if and only if io_fix == BUF_IO_WRITE

		(2) if buf_fix_count > 0, then
			is not in LRU list, not in free list
			is in flush list,
				if and only if oldest_modification > 0
			if io_fix == BUF_IO_READ,
				is x-locked
			if io_fix == BUF_IO_WRITE,
				is s-locked

State transitions:

NOT_USED => READY_FOR_USE
READY_FOR_USE => MEMORY
READY_FOR_USE => FILE_PAGE
MEMORY => NOT_USED
FILE_PAGE => NOT_USED	NOTE: This transition is allowed if and only if
				(1) buf_fix_count == 0,
				(2) oldest_modification == 0, and
				(3) io_fix == 0.
*/

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Functor to validate the LRU list. */
struct	CheckInLRUList {
	void	operator()(const buf_page_t* elem) const
	{
		ut_a(elem->in_LRU_list);
	}

	static void validate(const buf_pool_t* buf_pool)
	{
		CheckInLRUList	check;
		ut_list_validate(buf_pool->LRU, check);
	}
};

/** Functor to validate the LRU list. */
struct	CheckInFreeList {
	void	operator()(const buf_page_t* elem) const
	{
		ut_a(elem->in_free_list);
	}

	static void validate(const buf_pool_t* buf_pool)
	{
		CheckInFreeList	check;
		ut_list_validate(buf_pool->free, check);
	}
};

struct	CheckUnzipLRUAndLRUList {
	void	operator()(const buf_block_t* elem) const
	{
                ut_a(elem->page.in_LRU_list);
                ut_a(elem->in_unzip_LRU_list);
	}

	static void validate(const buf_pool_t* buf_pool)
	{
		CheckUnzipLRUAndLRUList	check;
		ut_list_validate(buf_pool->unzip_LRU, check);
	}
};
#endif /* UNIV_DEBUG || defined UNIV_BUF_DEBUG */

#ifndef UNIV_NONINL
#include "buf0buf.ic"
#endif
#endif /* !UNIV_INNOCHECKSUM */

#endif
