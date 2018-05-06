/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include <sys/types.h>

#include "my_compiler.h"

/** @file include/buf0lru.h
 The database buffer pool LRU replacement algorithm

 Created 11/5/1995 Heikki Tuuri
 *******************************************************/

#ifndef buf0lru_h
#define buf0lru_h

#include "buf0types.h"
#include "univ.i"
#ifndef UNIV_HOTBACKUP
#include "ut0byte.h"

// Forward declaration
struct trx_t;

/** Returns TRUE if less than 25 % of the buffer pool is available. This can be
 used in heuristics to prevent huge transactions eating up the whole buffer
 pool for their locks.
 @return true if less than 25 % of buffer pool left */
ibool buf_LRU_buf_pool_running_out(void);

/*#######################################################################
These are low-level functions
#########################################################################*/

/** Minimum LRU list length for which the LRU_old pointer is defined */
#define BUF_LRU_OLD_MIN_LEN 512 /* 8 megabytes of 16k pages */
#endif                          /* !UNIV_HOTBACKUP */

/** Flushes all dirty pages or removes all pages belonging
 to a given tablespace. A PROBLEM: if readahead is being started, what
 guarantees that it will not try to read in pages after this operation
 has completed? */
void buf_LRU_flush_or_remove_pages(
    space_id_t id,           /*!< in: space id */
    buf_remove_t buf_remove, /*!< in: remove or flush strategy */
    const trx_t *trx);       /*!< to check if the operation must
                             be interrupted */

#ifndef UNIV_HOTBACKUP
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Insert a compressed block into buf_pool->zip_clean in the LRU order.
@param[in]	bpage	pointer to the block in question */
void buf_LRU_insert_zip_clean(buf_page_t *bpage);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

/** Try to free a block.  If bpage is a descriptor of a compressed-only
page, the descriptor object will be freed as well.
NOTE: this function may temporarily release and relock the
buf_page_get_get_mutex(). Furthermore, the page frame will no longer be
accessible via bpage. If this function returns true, it will also release
the LRU list mutex.
The caller must hold the LRU list and buf_page_get_mutex() mutexes.
@param[in]	bpage	block to be freed
@param[in]	zip	true if should remove also the compressed page of
                        an uncompressed page
@return true if freed, false otherwise. */
bool buf_LRU_free_page(buf_page_t *bpage, bool zip)
    MY_ATTRIBUTE((warn_unused_result));

/** Try to free a replaceable block.
@param[in,out]	buf_pool	buffer pool instance
@param[in]	scan_all	scan whole LRU list if ture, otherwise scan
                                only BUF_LRU_SEARCH_SCAN_THRESHOLD blocks
@return true if found and freed */
bool buf_LRU_scan_and_free_block(buf_pool_t *buf_pool, bool scan_all)
    MY_ATTRIBUTE((warn_unused_result));

/** Returns a free block from the buf_pool.  The block is taken off the
free list.  If it is empty, returns NULL.
@param[in]	buf_pool	buffer pool instance
@return a free control block, or NULL if the buf_block->free list is empty */
buf_block_t *buf_LRU_get_free_only(buf_pool_t *buf_pool);

/** Returns a free block from the buf_pool. The block is taken off the
free list. If free list is empty, blocks are moved from the end of the
LRU list to the free list.
This function is called from a user thread when it needs a clean
block to read in a page. Note that we only ever get a block from
the free list. Even when we flush a page or find a page in LRU scan
we put it to free list to be used.
* iteration 0:
  * get a block from free list, success:done
  * if buf_pool->try_LRU_scan is set
    * scan LRU up to srv_LRU_scan_depth to find a clean block
    * the above will put the block on free list
    * success:retry the free list
  * flush one dirty page from tail of LRU to disk
    * the above will put the block on free list
    * success: retry the free list
* iteration 1:
  * same as iteration 0 except:
    * scan whole LRU list
    * scan LRU list even if buf_pool->try_LRU_scan is not set
* iteration > 1:
  * same as iteration 1 but sleep 10ms
@param[in,out]	buf_pool	buffer pool instance
@return the free control block, in state BUF_BLOCK_READY_FOR_USE */
buf_block_t *buf_LRU_get_free_block(buf_pool_t *buf_pool)
    MY_ATTRIBUTE((warn_unused_result));

/** Determines if the unzip_LRU list should be used for evicting a victim
instead of the general LRU list.
@param[in,out]	buf_pool	buffer pool instance
@return true if should use unzip_LRU */
ibool buf_LRU_evict_from_unzip_LRU(buf_pool_t *buf_pool);

/** Puts a block back to the free list.
@param[in]	block	block must not contain a file page */
void buf_LRU_block_free_non_file_page(buf_block_t *block);

/** Adds a block to the LRU list. Please make sure that the page_size is
 already set when invoking the function, so that we can get correct
 page_size from the buffer page when adding a block into LRU */
void buf_LRU_add_block(
    buf_page_t *bpage, /*!< in: control block */
    ibool old);        /*!< in: TRUE if should be put to the old
                       blocks in the LRU list, else put to the
                       start; if the LRU list is very short, added to
                       the start regardless of this parameter */

/** Adds a block to the LRU list of decompressed zip pages.
@param[in]	block	control block
@param[in]	old	TRUE if should be put to the end of the list,
                        else put to the start */
void buf_unzip_LRU_add_block(buf_block_t *block, ibool old);

/** Moves a block to the start of the LRU list.
@param[in]	bpage	control block */
void buf_LRU_make_block_young(buf_page_t *bpage);

/** Updates buf_pool->LRU_old_ratio.
 @return updated old_pct */
uint buf_LRU_old_ratio_update(
    uint old_pct,  /*!< in: Reserve this percentage of
                   the buffer pool for "old" blocks. */
    ibool adjust); /*!< in: TRUE=adjust the LRU list;
                   FALSE=just assign buf_pool->LRU_old_ratio
                   during the initialization of InnoDB */
/** Update the historical stats that we are collecting for LRU eviction
 policy at the end of each interval. */
void buf_LRU_stat_update(void);

/** Remove one page from LRU list and put it to free list. The caller must hold
the LRU list and block mutexes and have page hash latched in X. The latch and
the block mutexes will be released.
@param[in,out]	bpage		block, must contain a file page and
                                be in a state where it can be freed; there
                                may or may not be a hash index to the page
@param[in]	zip		true if should remove also the compressed page
                                of an uncompressed page
@param[in]	ignore_content	true if should ignore page content, since it
                                could be not initialized */
void buf_LRU_free_one_page(buf_page_t *bpage, bool zip, bool ignore_content);

/** Adjust LRU hazard pointers if needed. */
void buf_LRU_adjust_hp(buf_pool_t *buf_pool, /*!< in: buffer pool instance */
                       const buf_page_t *bpage); /*!< in: control block */

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Validates the LRU list.
 @return true */
ibool buf_LRU_validate(void);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#if defined UNIV_DEBUG_PRINT || defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Prints the LRU list. */
void buf_LRU_print(void);
#endif /* UNIV_DEBUG_PRINT || UNIV_DEBUG || UNIV_BUF_DEBUG */

/** @name Heuristics for detecting index scan @{ */
/** The denominator of buf_pool->LRU_old_ratio. */
#define BUF_LRU_OLD_RATIO_DIV 1024
/** Maximum value of buf_pool->LRU_old_ratio.
@see buf_LRU_old_adjust_len
@see buf_pool->LRU_old_ratio_update */
#define BUF_LRU_OLD_RATIO_MAX BUF_LRU_OLD_RATIO_DIV
/** Minimum value of buf_pool->LRU_old_ratio.
@see buf_LRU_old_adjust_len
@see buf_pool->LRU_old_ratio_update
The minimum must exceed
(BUF_LRU_OLD_TOLERANCE + 5) * BUF_LRU_OLD_RATIO_DIV / BUF_LRU_OLD_MIN_LEN. */
#define BUF_LRU_OLD_RATIO_MIN 51

#if BUF_LRU_OLD_RATIO_MIN >= BUF_LRU_OLD_RATIO_MAX
#error "BUF_LRU_OLD_RATIO_MIN >= BUF_LRU_OLD_RATIO_MAX"
#endif
#if BUF_LRU_OLD_RATIO_MAX > BUF_LRU_OLD_RATIO_DIV
#error "BUF_LRU_OLD_RATIO_MAX > BUF_LRU_OLD_RATIO_DIV"
#endif

/** Move blocks to "new" LRU list only if the first access was at
least this many milliseconds ago.  Not protected by any mutex or latch. */
extern uint buf_LRU_old_threshold_ms;
/* @} */

/** @brief Statistics for selecting the LRU list for eviction.

These statistics are not 'of' LRU but 'for' LRU.  We keep count of I/O
and page_zip_decompress() operations.  Based on the statistics we decide
if we want to evict from buf_pool->unzip_LRU or buf_pool->LRU. */
struct buf_LRU_stat_t {
  ulint io;    /**< Counter of buffer pool I/O operations. */
  ulint unzip; /**< Counter of page_zip_decompress operations. */
};

/** Current operation counters.  Not protected by any mutex.
Cleared by buf_LRU_stat_update(). */
extern buf_LRU_stat_t buf_LRU_stat_cur;

/** Running sum of past values of buf_LRU_stat_cur.
Updated by buf_LRU_stat_update(). Accesses protected by memory barriers. */
extern buf_LRU_stat_t buf_LRU_stat_sum;

/** Increments the I/O counter in buf_LRU_stat_cur. */
#define buf_LRU_stat_inc_io() buf_LRU_stat_cur.io++
/** Increments the page_zip_decompress() counter in buf_LRU_stat_cur. */
#define buf_LRU_stat_inc_unzip() buf_LRU_stat_cur.unzip++

#include "buf0lru.ic"

#endif /* !UNIV_HOTBACKUP */

#endif
