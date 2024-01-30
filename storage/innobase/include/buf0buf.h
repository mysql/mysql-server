/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/buf0buf.h
 The database buffer pool high-level routines

 Created 11/5/1995 Heikki Tuuri
 *******************************************************/

#ifndef buf0buf_h
#define buf0buf_h

#include "buf0types.h"
#include "fil0fil.h"
#include "hash0hash.h"
#include "mtr0types.h"
#include "os0proc.h"
#include "page0types.h"
#include "srv0shutdown.h"
#include "srv0srv.h"
#include "univ.i"
#include "ut0byte.h"
#include "ut0rbt.h"

#include "buf/buf.h"

#include <ostream>

// Forward declaration
struct fil_addr_t;

/** @name Modes for buf_page_get_gen */
/** @{ */
enum class Page_fetch {
  /** Get always */
  NORMAL,

  /** Same as NORMAL, but hint that the fetch is part of a large scan.
  Try not to flood the buffer pool with pages that may not be accessed again
  any time soon. */
  SCAN,

  /** get if in pool */
  IF_IN_POOL,

  /** get if in pool, do not make the block young in the LRU list */
  PEEK_IF_IN_POOL,

  /** get and bufferfix, but set no latch; we have separated this case, because
  it is error-prone programming not to set a latch, and it  should be used with
  care */
  NO_LATCH,

  /** Get the page only if it's in the buffer pool, if not then set a watch on
  the page. */
  IF_IN_POOL_OR_WATCH,

  /** Like Page_fetch::NORMAL, but do not mind if the file page has been
  freed. */
  POSSIBLY_FREED,

  /** Like Page_fetch::POSSIBLY_FREED, but do not initiate read ahead. */
  POSSIBLY_FREED_NO_READ_AHEAD,
};
/** @} */

/** @name Modes for buf_page_get_known_nowait */

/** @{ */
enum class Cache_hint {
  /** Move the block to the start of the LRU list if there is a danger that the
  block would drift out of the buffer  pool*/
  MAKE_YOUNG = 51,

  /** Preserve the current LRU position of the block. */
  KEEP_OLD = 52
};

/** @} */

/** Number of bits to representing a buffer pool ID */
constexpr ulint MAX_BUFFER_POOLS_BITS = 6;

/** The maximum number of buffer pools that can be defined */
constexpr ulint MAX_BUFFER_POOLS = (1 << MAX_BUFFER_POOLS_BITS);

/** Maximum number of concurrent buffer pool watches */
#define BUF_POOL_WATCH_SIZE (srv_n_purge_threads + 1)

/** The maximum number of page_hash locks */
constexpr ulint MAX_PAGE_HASH_LOCKS = 1024;

/** The buffer pools of the database */
extern buf_pool_t *buf_pool_ptr;

#ifdef UNIV_HOTBACKUP
/** first block, for --apply-log */
extern buf_block_t *back_block1;
/** second block, for page reorganize */
extern buf_block_t *back_block2;
#endif /* UNIV_HOTBACKUP */

/** @brief States of a control block
@see buf_page_t

The enumeration values must be 0..7. */
enum buf_page_state : uint8_t {
  /** A sentinel for the buffer pool watch, element of buf_pool->watch[] */
  BUF_BLOCK_POOL_WATCH,
  /** Contains a clean compressed page */
  BUF_BLOCK_ZIP_PAGE,
  /** Contains a compressed page that is in the buf_pool->flush_list */
  BUF_BLOCK_ZIP_DIRTY,

  /** Is in the free list; must be after the BUF_BLOCK_ZIP_ constants for
  compressed-only pages @see buf_block_state_valid() */
  BUF_BLOCK_NOT_USED,

  /** When buf_LRU_get_free_block returns a block, it is in this state */
  BUF_BLOCK_READY_FOR_USE,

  /** Contains a buffered file page */
  BUF_BLOCK_FILE_PAGE,

  /** Contains some main memory object */
  BUF_BLOCK_MEMORY,

  /** Hash index should be removed before putting to the free list */
  BUF_BLOCK_REMOVE_HASH
};

/** This structure defines information we will fetch from each buffer pool. It
will be used to print table IO stats */
struct buf_pool_info_t {
  /* General buffer pool info */

  /** Buffer Pool ID */
  ulint pool_unique_id;
  /** Buffer Pool size in pages */
  ulint pool_size;
  /** Length of buf_pool->LRU */
  ulint lru_len;
  /** buf_pool->LRU_old_len */
  ulint old_lru_len;
  /** Length of buf_pool->free list */
  ulint free_list_len;
  /** Length of buf_pool->flush_list */
  ulint flush_list_len;
  /** buf_pool->n_pend_unzip, pages pending decompress */
  ulint n_pend_unzip;
  /** buf_pool->n_pend_reads, pages pending read */
  ulint n_pend_reads;
  /** Number of pages pending flush of given type */
  std::array<size_t, BUF_FLUSH_N_TYPES> n_pending_flush;
  /** number of pages made young */
  ulint n_pages_made_young;
  /** number of pages not made young */
  ulint n_pages_not_made_young;
  /** buf_pool->n_pages_read */
  ulint n_pages_read;
  /** buf_pool->n_pages_created */
  ulint n_pages_created;
  /** buf_pool->n_pages_written */
  ulint n_pages_written;
  /** buf_pool->n_page_gets */
  ulint n_page_gets;
  /** buf_pool->n_ra_pages_read_rnd, number of pages readahead */
  ulint n_ra_pages_read_rnd;
  /** buf_pool->n_ra_pages_read, number of pages readahead */
  ulint n_ra_pages_read;
  /** buf_pool->n_ra_pages_evicted, number of readahead pages evicted without
  access */
  ulint n_ra_pages_evicted;
  /** num of buffer pool page gets since last printout */
  ulint n_page_get_delta;

  /* Buffer pool access stats */
  double page_made_young_rate;     /*!< page made young rate in pages
                                   per second */
  double page_not_made_young_rate; /*!< page not made young rate
                                  in pages per second */
  double pages_read_rate;          /*!< num of pages read per second */
  double pages_created_rate;       /*!< num of pages create per second */
  double pages_written_rate;       /*!< num of  pages written per second */
  ulint page_read_delta;           /*!< num of pages read since last
                                   printout */
  ulint young_making_delta;        /*!< num of pages made young since
                                   last printout */
  ulint not_young_making_delta;    /*!< num of pages not make young since
                                   last printout */

  /* Statistics about read ahead algorithm.  */
  double pages_readahead_rnd_rate; /*!< random readahead rate in pages per
                                  second */
  double pages_readahead_rate;     /*!< readahead rate in pages per
                                   second */
  double pages_evicted_rate;       /*!< rate of readahead page evicted
                                   without access, in pages per second */

  /* Stats about LRU eviction */
  ulint unzip_lru_len; /*!< length of buf_pool->unzip_LRU
                       list */
  /* Counters for LRU policy */
  ulint io_sum;    /*!< buf_LRU_stat_sum.io */
  ulint io_cur;    /*!< buf_LRU_stat_cur.io, num of IO
                   for current interval */
  ulint unzip_sum; /*!< buf_LRU_stat_sum.unzip */
  ulint unzip_cur; /*!< buf_LRU_stat_cur.unzip, num
                   pages decompressed in current
                   interval */
};

/** The occupied bytes of lists in all buffer pools */
struct buf_pools_list_size_t {
  ulint LRU_bytes;        /*!< LRU size in bytes */
  ulint unzip_LRU_bytes;  /*!< unzip_LRU size in bytes */
  ulint flush_list_bytes; /*!< flush_list size in bytes */
};

#ifndef UNIV_HOTBACKUP
/** Creates the buffer pool.
@param[in]  total_size    Size of the total pool in bytes.
@param[in]  n_instances   Number of buffer pool instances to create.
@return DB_SUCCESS if success, DB_ERROR if not enough memory or error */
dberr_t buf_pool_init(ulint total_size, ulint n_instances);

/** Frees the buffer pool at shutdown.  This must not be invoked before
 freeing all mutexes. */
void buf_pool_free_all();

/** Determines if a block is intended to be withdrawn. The caller must ensure
that there was a sufficient memory barrier to read curr_size and old_size.
@param[in]      buf_pool        buffer pool instance
@param[in]      block           pointer to control block
@retval true    if will be withdrawn */
bool buf_block_will_withdrawn(buf_pool_t *buf_pool, const buf_block_t *block);

/** Determines if a frame is intended to be withdrawn. The caller must ensure
that there was a sufficient memory barrier to read curr_size and old_size.
@param[in]      buf_pool        buffer pool instance
@param[in]      ptr             pointer to a frame
@retval true    if will be withdrawn */
bool buf_frame_will_withdrawn(buf_pool_t *buf_pool, const byte *ptr);

/** This is the thread for resizing buffer pool. It waits for an event and
when waked up either performs a resizing and sleeps again. */
void buf_resize_thread();

/** Checks if innobase_should_madvise_buf_pool() value has changed since we've
last check and if so, then updates buf_pool_should_madvise and calls madvise
for all chunks in all srv_buf_pool_instances.
@see buf_pool_should_madvise comment for a longer explanation. */
void buf_pool_update_madvise();

/** Clears the adaptive hash index on all pages in the buffer pool. */
void buf_pool_clear_hash_index(void);

/** Gets the current size of buffer buf_pool in bytes.
 @return size in bytes */
static inline ulint buf_pool_get_curr_size(void);
/** Gets the current size of buffer buf_pool in frames.
 @return size in pages */
static inline ulint buf_pool_get_n_pages(void);

/** @return true if buffer pool resize is in progress. */
bool is_buffer_pool_resize_in_progress();

#endif /* !UNIV_HOTBACKUP */

/** Gets the smallest oldest_modification lsn among all of the earliest
added pages in flush lists. In other words - takes the last dirty page
from each flush list, and calculates minimum oldest_modification among
all of them. Does not acquire global lock for the whole process, so the
result might come from inconsistent view on flush lists.

@note Note that because of the relaxed order in each flush list, this
functions no longer returns the smallest oldest_modification among all
of the dirty pages. If you wanted to have a safe lsn, which is smaller
than every oldest_modification, you would need to use another function:
        buf_pool_get_oldest_modification_lwm().

Returns zero if there were no dirty pages (flush lists were empty).

@return minimum oldest_modification of last pages from flush lists,
        zero if flush lists were empty */
lsn_t buf_pool_get_oldest_modification_approx(void);

/** Gets a safe low watermark for oldest_modification. It's guaranteed
that there were no dirty pages with smaller oldest_modification in the
whole flush lists.

Returns zero if flush lists were empty, be careful in such case, because
taking the newest lsn is probably not a good idea. If you wanted to rely
on some lsn in such case, you would need to follow pattern:

        dpa_lsn = buf_flush_list_added->smallest_not_added_lsn();

        lwm_lsn = buf_pool_get_oldest_modification_lwm();

        if (lwm_lsn == 0) lwm_lsn = dpa_lsn;

The order is important to avoid race conditions.

@remarks
It's guaranteed that the returned value will not be smaller than the
last checkpoint lsn. It's not guaranteed that the returned value is
the maximum possible. It's just the best effort for the low cost.
It basically takes result of buf_pool_get_oldest_modification_approx()
and subtracts maximum possible lag introduced by relaxed order in
flush lists (srv_buf_flush_list_added_size).

@return safe low watermark for oldest_modification of dirty pages,
        or zero if flush lists were empty; if non-zero, it is then
        guaranteed not to be at block boundary (and it points to lsn
        inside data fragment of block) */
lsn_t buf_pool_get_oldest_modification_lwm(void);

#ifndef UNIV_HOTBACKUP

/** Allocates a buf_page_t descriptor. This function must succeed. In case
 of failure we assert in this function. */
static inline buf_page_t *buf_page_alloc_descriptor(void)
    MY_ATTRIBUTE((malloc));

/** Free a buf_page_t descriptor.
@param[in]  bpage  bpage descriptor to free */
void buf_page_free_descriptor(buf_page_t *bpage);

/** Allocates a buffer block.
 @return own: the allocated block, in state BUF_BLOCK_MEMORY */
buf_block_t *buf_block_alloc(
    buf_pool_t *buf_pool); /*!< in: buffer pool instance,
                           or NULL for round-robin selection
                           of the buffer pool */
/** Frees a buffer block which does not contain a file page. */
static inline void buf_block_free(
    buf_block_t *block); /*!< in, own: block to be freed */
#endif                   /* !UNIV_HOTBACKUP */

/** Copies contents of a buffer frame to a given buffer.
@param[in]      buf     buffer to copy to
@param[in]      frame   buffer frame
@return buf */
static inline byte *buf_frame_copy(byte *buf, const buf_frame_t *frame);

#ifndef UNIV_HOTBACKUP
/** This is the general function used to get optimistic access to a database
page.
@param[in]      rw_latch        RW_S_LATCH, RW_X_LATCH
@param[in,out]  block           Guessed block
@param[in]      modify_clock    Modify clock value
@param[in]      fetch_mode      Fetch mode
@param[in]      file            File name
@param[in]      line            Line where called
@param[in,out]  mtr             Mini-transaction
@return true if success */
bool buf_page_optimistic_get(ulint rw_latch, buf_block_t *block,
                             uint64_t modify_clock, Page_fetch fetch_mode,
                             const char *file, ulint line, mtr_t *mtr);

/** This is used to get access to a known database page, when no waiting can be
done.
@param[in] rw_latch RW_S_LATCH or RW_X_LATCH.
@param[in] block The known page.
@param[in] hint Cache_hint::MAKE_YOUNG or Cache_hint::KEEP_OLD
@param[in] file File name from where it was called.
@param[in] line Line from where it was called.
@param[in,out] mtr Mini-transaction covering the fetch
@return true if success */
bool buf_page_get_known_nowait(ulint rw_latch, buf_block_t *block,
                               Cache_hint hint, const char *file, ulint line,
                               mtr_t *mtr);

/** Given a tablespace id and page number tries to get that page. If the
page is not in the buffer pool it is not loaded and NULL is returned.
Suitable for using when holding the lock_sys latches (as it avoids deadlock).
@param[in]      page_id page Id
@param[in]      location Location where called
@param[in]      mtr     Mini-transaction
@return pointer to a page or NULL */
const buf_block_t *buf_page_try_get(const page_id_t &page_id,
                                    ut::Location location, mtr_t *mtr);

/** Get read access to a compressed page (usually of type
FIL_PAGE_TYPE_ZBLOB or FIL_PAGE_TYPE_ZBLOB2).
The page must be released with buf_page_release_zip().
NOTE: the page is not protected by any latch.  Mutual exclusion has to
be implemented at a higher level.  In other words, all possible
accesses to a given page through this function must be protected by
the same set of mutexes or latches.
@param[in]      page_id         page id
@param[in]      page_size       page size
@return pointer to the block */
buf_page_t *buf_page_get_zip(const page_id_t &page_id,
                             const page_size_t &page_size);

/** This is the general function used to get access to a database page.
@param[in]      page_id                 Page id
@param[in]      page_size               Page size
@param[in]      rw_latch                RW_S_LATCH, RW_X_LATCH, RW_NO_LATCH
@param[in]      guess                     Guessed block or NULL
@param[in]      mode                      Fetch mode.
@param[in]      location          Location from where this method was called.
@param[in]      mtr                         Mini-transaction
@param[in]      dirty_with_no_latch     Mark page as dirty even if page is being
                        pinned without any latch
@return pointer to the block or NULL */
buf_block_t *buf_page_get_gen(const page_id_t &page_id,
                              const page_size_t &page_size, ulint rw_latch,
                              buf_block_t *guess, Page_fetch mode,
                              ut::Location location, mtr_t *mtr,
                              bool dirty_with_no_latch = false);

/** NOTE! The following macros should be used instead of buf_page_get_gen,
 to improve debugging. Only values RW_S_LATCH and RW_X_LATCH are allowed
 in LA! */
inline buf_block_t *buf_page_get(const page_id_t &id, const page_size_t &size,
                                 ulint latch, ut::Location location,
                                 mtr_t *mtr) {
  return buf_page_get_gen(id, size, latch, nullptr, Page_fetch::NORMAL,
                          location, mtr);
}
/** Use these macros to bufferfix a page with no latching. Remember not to
 read the contents of the page unless you know it is safe. Do not modify
 the contents of the page! We have separated this case, because it is
 error-prone programming not to set a latch, and it should be used
 with care. */
inline buf_block_t *buf_page_get_with_no_latch(const page_id_t &id,
                                               const page_size_t &size,
                                               ut::Location location,
                                               mtr_t *mtr) {
  return buf_page_get_gen(id, size, RW_NO_LATCH, nullptr, Page_fetch::NO_LATCH,
                          location, mtr);
}

/** Initializes a page to the buffer buf_pool. The page is usually not read
from a file even if it cannot be found in the buffer buf_pool. This is one
of the functions which perform to a block a state transition NOT_USED =>
FILE_PAGE (the other is buf_page_get_gen). The page is latched by passed mtr.
@param[in]      page_id         Page id
@param[in]      page_size       Page size
@param[in]      rw_latch        RW_SX_LATCH, RW_X_LATCH
@param[in]      mtr             Mini-transaction
@return pointer to the block, page bufferfixed */
buf_block_t *buf_page_create(const page_id_t &page_id,
                             const page_size_t &page_size,
                             rw_lock_type_t rw_latch, mtr_t *mtr);

#else  /* !UNIV_HOTBACKUP */

/** Inits a page to the buffer buf_pool, for use in mysqlbackup --restore.
@param[in]      page_id         page id
@param[in]      page_size       page size
@param[in,out]  block           block to init */
void meb_page_init(const page_id_t &page_id, const page_size_t &page_size,
                   buf_block_t *block);
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/** Releases a compressed-only page acquired with buf_page_get_zip(). */
static inline void buf_page_release_zip(
    buf_page_t *bpage); /*!< in: buffer block */

/** Releases a latch, if specified.
@param[in]      block           buffer block
@param[in]      rw_latch        RW_S_LATCH, RW_X_LATCH, RW_NO_LATCH */
static inline void buf_page_release_latch(buf_block_t *block, ulint rw_latch);

/** Moves a page to the start of the buffer pool LRU list. This high-level
function can be used to prevent an important page from slipping out of
the buffer pool.
@param[in,out]  bpage   buffer block of a file page */
void buf_page_make_young(buf_page_t *bpage);

/** Moved a page to the end of the buffer pool LRU list so that it can be
flushed out at the earliest.
@param[in]      bpage   buffer block of a file page */
void buf_page_make_old(buf_page_t *bpage);

/** Returns true if the page can be found in the buffer pool hash table.
NOTE that it is possible that the page is not yet read from disk,
though.
@param[in]      page_id page id
@return true if found in the page hash table */
static inline bool buf_page_peek(const page_id_t &page_id);

#ifdef UNIV_DEBUG

/** Sets file_page_was_freed true if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated.
@param[in]      page_id page id
@return control block if found in page hash table, otherwise NULL */
buf_page_t *buf_page_set_file_page_was_freed(const page_id_t &page_id);

/** Sets file_page_was_freed false if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated.
@param[in]      page_id page id
@return control block if found in page hash table, otherwise NULL */
buf_page_t *buf_page_reset_file_page_was_freed(const page_id_t &page_id);

#endif /* UNIV_DEBUG */
/** Reads the freed_page_clock of a buffer block.
 @return freed_page_clock */
[[nodiscard]] static inline ulint buf_page_get_freed_page_clock(
    const buf_page_t *bpage); /*!< in: block */
/** Reads the freed_page_clock of a buffer block.
 @return freed_page_clock */
[[nodiscard]] static inline ulint buf_block_get_freed_page_clock(
    const buf_block_t *block); /*!< in: block */

/** Tells, for heuristics, if a block is still close enough to the MRU end of
the LRU list meaning that it is not in danger of getting evicted and also
implying that it has been accessed recently.
The page must be either buffer-fixed, either its page hash must be locked.
@param[in]      bpage   block
@return true if block is close to MRU end of LRU */
static inline bool buf_page_peek_if_young(const buf_page_t *bpage);

/** Recommends a move of a block to the start of the LRU list if there is
danger of dropping from the buffer pool.
NOTE: does not reserve the LRU list mutex.
@param[in]      bpage   block to make younger
@return true if should be made younger */
static inline bool buf_page_peek_if_too_old(const buf_page_t *bpage);

/** Gets the youngest modification log sequence number for a frame.
 Returns zero if not file page or no modification occurred yet.
 @return newest modification to page */
static inline lsn_t buf_page_get_newest_modification(
    const buf_page_t *bpage); /*!< in: block containing the
                              page frame */

/** Increment the modify clock.
The caller must
(1) own the buf_pool->mutex and block bufferfix count has to be zero,
(2) own X or SX latch on the block->lock, or
(3) operate on a thread-private temporary table
@param[in,out]  block   buffer block */
static inline void buf_block_modify_clock_inc(buf_block_t *block);

/** Increments the bufferfix count.
@param[in]      location location
@param[in,out]  block   block to bufferfix */
static inline void buf_block_buf_fix_inc_func(IF_DEBUG(ut::Location location, )
                                                  buf_block_t *block);

/** Increments the bufferfix count.
@param[in,out]  bpage   block to bufferfix
@return the count */
static inline ulint buf_block_fix(buf_page_t *bpage);

/** Increments the bufferfix count.
@param[in,out]  block   block to bufferfix
@return the count */
static inline ulint buf_block_fix(buf_block_t *block);

/** Decrements the bufferfix count.
@param[in,out]  bpage   block to bufferunfix
@return the remaining buffer-fix count */
static inline ulint buf_block_unfix(buf_page_t *bpage);

/** Decrements the bufferfix count.
@param[in,out]  block   block to bufferunfix
@return the remaining buffer-fix count */
static inline ulint buf_block_unfix(buf_block_t *block);

/** Unfixes the page, unlatches the page,
removes it from page_hash and removes it from LRU.
@param[in,out]  bpage   pointer to the block */
void buf_read_page_handle_error(buf_page_t *bpage);

/** Increments the bufferfix count.
@param[in,out]  b       block to bufferfix
@param[in]      l location where requested */
inline void buf_block_buf_fix_inc(buf_block_t *b,
                                  ut::Location l [[maybe_unused]]) {
  buf_block_buf_fix_inc_func(IF_DEBUG(l, ) b);
}
#else  /* !UNIV_HOTBACKUP */
static inline void buf_block_modify_clock_inc(buf_block_t *block) {}
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP

/** Gets the space id, page offset, and byte offset within page of a pointer
pointing to a buffer frame containing a file page.
@param[in]      ptr     pointer to a buffer frame
@param[out]     space   space id
@param[out]     addr    page offset and byte offset */
static inline void buf_ptr_get_fsp_addr(const void *ptr, space_id_t *space,
                                        fil_addr_t *addr);

#ifdef UNIV_DEBUG
/** Finds a block in the buffer pool that points to a
given compressed page. Used only to confirm that buffer pool does not contain a
given pointer, thus protected by zip_free_mutex.
@param[in]      buf_pool        buffer pool instance
@param[in]      data            pointer to compressed page
@return buffer block pointing to the compressed page, or NULL */
buf_block_t *buf_pool_contains_zip(buf_pool_t *buf_pool, const void *data);
#endif /* UNIV_DEBUG */

/***********************************************************************
FIXME_FTS: Gets the frame the pointer is pointing to. */
static inline buf_frame_t *buf_frame_align(
    /* out: pointer to frame */
    byte *ptr); /* in: pointer to a frame */

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Validates the buffer pool data structure.
 @return true */
bool buf_validate(void);
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */
#if defined UNIV_DEBUG_PRINT || defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
/** Prints info of the buffer pool data structure. */
void buf_print(void);
#endif /* UNIV_DEBUG_PRINT || UNIV_DEBUG || UNIV_BUF_DEBUG */
#endif /* !UNIV_HOTBACKUP */
enum buf_page_print_flags {
  /** Do not crash at the end of buf_page_print(). */
  BUF_PAGE_PRINT_NO_CRASH = 1,
  /** Do not print the full page dump. */
  BUF_PAGE_PRINT_NO_FULL = 2
};

/** Prints a page to stderr.
@param[in]      read_buf        a database page
@param[in]      page_size       page size
@param[in]      flags           0 or BUF_PAGE_PRINT_NO_CRASH or
BUF_PAGE_PRINT_NO_FULL */
void buf_page_print(const byte *read_buf, const page_size_t &page_size,
                    ulint flags);

/** Decompress a block.
 @return true if successful */
bool buf_zip_decompress(buf_block_t *block, /*!< in/out: block */
                        bool check); /*!< in: true=verify the page checksum */
#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
/** Returns the number of latched pages in the buffer pool.
 @return number of latched pages */
ulint buf_get_latched_pages_number(void);
#endif /* UNIV_DEBUG */
/** Returns the number of pending buf pool read ios.
 @return number of pending read I/O operations */
ulint buf_get_n_pending_read_ios(void);
/** Prints info of the buffer i/o. */
void buf_print_io(FILE *file); /*!< in: file where to print */
/** Collect buffer pool stats information for a buffer pool. Also
 record aggregated stats if there are more than one buffer pool
 in the server */
void buf_stats_get_pool_info(
    buf_pool_t *buf_pool,            /*!< in: buffer pool */
    ulint pool_id,                   /*!< in: buffer pool ID */
    buf_pool_info_t *all_pool_info); /*!< in/out: buffer pool info
                                     to fill */
/** Return the ratio in percents of modified pages in the buffer pool /
database pages in the buffer pool.
@return modified page percentage ratio */
double buf_get_modified_ratio_pct(void);
/** Refresh the statistics used to print per-second averages. */
void buf_refresh_io_stats_all();

/** Assert that all file pages in the buffer are in a replaceable state. */
void buf_assert_all_are_replaceable();

/** Computes number of pending I/O read operations for the buffer pool.
@return number of pending i/o reads */
size_t buf_pool_pending_io_reads_count();

/** Computes number of pending I/O write operations for the buffer pool.
@return number of pending i/o writes */
size_t buf_pool_pending_io_writes_count();

/** Waits until there are no pending I/O operations for the buffer pool.
Keep waiting in loop with sleeps, emitting information every minute.
This is used to avoid risk of some pending async IO (e.g. enqueued by
the linear read-ahead), which would involve ibuf merge and create new
redo records. */
void buf_pool_wait_for_no_pending_io();

/** Invalidates the file pages in the buffer pool when an archive recovery is
 completed. All the file pages buffered must be in a replaceable state when
 this function is called: not latched and not modified. */
void buf_pool_invalidate(void);

/*========================================================================
--------------------------- LOWER LEVEL ROUTINES -------------------------
=========================================================================*/

#ifdef UNIV_DEBUG
/** Adds latch level info for the rw-lock protecting the buffer frame. This
should be called in the debug version after a successful latching of a page if
we know the latching order level of the acquired latch.
@param[in]      block   buffer page where we have acquired latch
@param[in]      level   latching order level */
static inline void buf_block_dbg_add_level(buf_block_t *block,
                                           latch_level_t level);
#else                                         /* UNIV_DEBUG */
#define buf_block_dbg_add_level(block, level) /* nothing */
#endif                                        /* UNIV_DEBUG */

#endif /* !UNIV_HOTBACKUP */

/** Gets the state of a block.
 @return state */
static inline enum buf_page_state buf_page_get_state(
    const buf_page_t *bpage); /*!< in: pointer to the control block */
/** Gets the state of a block.
 @return state */
[[nodiscard]] static inline enum buf_page_state buf_block_get_state(
    const buf_block_t *block); /*!< in: pointer to the control block */

/** Sets the state of a block.
@param[in,out]  bpage   pointer to control block
@param[in]      state   state */
static inline void buf_page_set_state(buf_page_t *bpage,
                                      enum buf_page_state state);

/** Sets the state of a block.
@param[in,out]  block   pointer to control block
@param[in]      state   state */
static inline void buf_block_set_state(buf_block_t *block,
                                       enum buf_page_state state);

/** Determines if a block is mapped to a tablespace.
 @return true if mapped */
[[nodiscard]] static inline bool buf_page_in_file(
    const buf_page_t *bpage); /*!< in: pointer to control block */
#ifndef UNIV_HOTBACKUP
/** Determines if a block should be on unzip_LRU list.
 @return true if block belongs to unzip_LRU */
[[nodiscard]] static inline bool buf_page_belongs_to_unzip_LRU(
    const buf_page_t *bpage); /*!< in: pointer to control block */

/** Gets the mutex of a block.
 @return pointer to mutex protecting bpage */
[[nodiscard]] static inline BPageMutex *buf_page_get_mutex(
    const buf_page_t *bpage); /*!< in: pointer to control block */

/** Get the flush type of a page.
 @return flush type */
[[nodiscard]] static inline buf_flush_t buf_page_get_flush_type(
    const buf_page_t *bpage); /*!< in: buffer page */

/** Set the flush type of a page.
@param[in]      bpage           buffer page
@param[in]      flush_type      flush type */
static inline void buf_page_set_flush_type(buf_page_t *bpage,
                                           buf_flush_t flush_type);

/** Map a block to a file page.
@param[in,out]  block   pointer to control block
@param[in]      page_id page id */
static inline void buf_block_set_file_page(buf_block_t *block,
                                           const page_id_t &page_id);

/** Gets the io_fix state of a block.
 @return io_fix state */
[[nodiscard]] static inline enum buf_io_fix buf_page_get_io_fix(
    const buf_page_t *bpage); /*!< in: pointer to the control block */
/** Gets the io_fix state of a block.
 @return io_fix state */
[[nodiscard]] static inline enum buf_io_fix buf_block_get_io_fix(
    const buf_block_t *block); /*!< in: pointer to the control block */

/** Sets the io_fix state of a block.
@param[in,out]  bpage   control block
@param[in]      io_fix  io_fix state */
static inline void buf_page_set_io_fix(buf_page_t *bpage,
                                       enum buf_io_fix io_fix);

/** Sets the io_fix state of a block.
@param[in,out]  block   control block
@param[in]      io_fix  io_fix state */
static inline void buf_block_set_io_fix(buf_block_t *block,
                                        enum buf_io_fix io_fix);

/** Makes a block sticky. A sticky block implies that even after we release
the buf_pool->LRU_list_mutex and the block->mutex:
* it cannot be removed from the flush_list
* the block descriptor cannot be relocated
* it cannot be removed from the LRU list
Note that:
* the block can still change its position in the LRU list
* the next and previous pointers can change.
@param[in,out]  bpage   control block */
static inline void buf_page_set_sticky(buf_page_t *bpage);

/** Removes stickiness of a block. */
static inline void buf_page_unset_sticky(
    buf_page_t *bpage); /*!< in/out: control block */
/** Determine if a buffer block can be relocated in memory.  The block
 can be dirty, but it must not be I/O-fixed or bufferfixed. */
[[nodiscard]] static inline bool buf_page_can_relocate(
    const buf_page_t *bpage); /*!< control block being relocated */

/** Determine if a block has been flagged old.
@param[in]      bpage   control block
@return true if old */
[[nodiscard]] static inline bool buf_page_is_old(const buf_page_t *bpage);

/** Flag a block old.
@param[in,out]  bpage   control block
@param[in]      old     old */
static inline void buf_page_set_old(buf_page_t *bpage, bool old);

/** Determine the time of first access of a block in the buffer pool.
 @return Time of first access, zero if not accessed
 */
[[nodiscard]] static inline std::chrono::steady_clock::time_point
buf_page_is_accessed(const buf_page_t *bpage); /*!< in: control block */
/** Flag a block accessed. */
static inline void buf_page_set_accessed(
    buf_page_t *bpage); /*!< in/out: control block */

/** Gets the buf_block_t handle of a buffered file block if an uncompressed
page frame exists, or NULL. page frame exists, or NULL. The caller must hold
either the appropriate hash lock in any mode, either the LRU list mutex. Note:
even though bpage is not declared a const we don't update its value. It is safe
to make this pure.
@param[in]      bpage   control block, or NULL
@return control block, or NULL */
[[nodiscard]] static inline buf_block_t *buf_page_get_block(buf_page_t *bpage);
#ifdef UNIV_DEBUG
/** Gets a pointer to the memory frame of a block.
 @return pointer to the frame */
[[nodiscard]] static inline buf_frame_t *buf_block_get_frame(
    const buf_block_t *block); /*!< in: pointer to the control block */
#else                          /* UNIV_DEBUG */
#define buf_block_get_frame(block) (block)->frame
#endif /* UNIV_DEBUG */
#else  /* !UNIV_HOTBACKUP */
#define buf_block_get_frame(block) (block)->frame
#endif /* !UNIV_HOTBACKUP */

/** Get a buffer block from an adaptive hash index pointer.
This function does not return if the block is not identified.
@param[in]      ptr     pointer to within a page frame
@return pointer to block, never NULL */
buf_block_t *buf_block_from_ahi(const byte *ptr);

/** Find out if a block pointer points into one of currently used chunks of
the buffer pool. This is useful if you stored the pointer some time ago, and
want to dereference it now, and are afraid that buffer pool resize could free
the memory pointed by it. Thus calling this function requires holding at least
one of the latches which prevent freeing memory from buffer pool for the
duration of the call and until you pin the block in some other way, as otherwise
the result of this function might be obsolete by the time you dereference the
block (an s-latch on buf_page_hash_lock_get for any hash cell is enough).
@param  buf_pool    The buffer pool instance to search in.
@param  ptr         A pointer which you want to check. This function will not
                    dereference it.
@return true iff `block` points inside one of the chunks of the `buf_pool`
*/
bool buf_is_block_in_instance(const buf_pool_t *buf_pool,
                              const buf_block_t *ptr);

#ifndef UNIV_HOTBACKUP

/** Inits a page for read to the buffer buf_pool. If the page is
(1) already in buf_pool, or
(2) if we specify to read only ibuf pages and the page is not an ibuf page, or
(3) if the space is deleted or being deleted,
then this function does nothing.
Sets the io_fix flag to BUF_IO_READ and sets a non-recursive exclusive lock
on the buffer frame. The io-handler must take care that the flag is cleared
and the lock released later.
@param[in]      mode                    BUF_READ_IBUF_PAGES_ONLY, ...
@param[in]      page_id                 page id
@param[in]      page_size               page size
@param[in]      unzip                   true=request uncompressed page
@return pointer to the block or NULL */
buf_page_t *buf_page_init_for_read(ulint mode, const page_id_t &page_id,
                                   const page_size_t &page_size, bool unzip);

/** Completes an asynchronous read or write request of a file page to or from
the buffer pool.
@param[in]      bpage   pointer to the block in question
@param[in]      evict   whether or not to evict the page from LRU list
@param[in]      type    i/o request type for which this completion routine is
                        called.
@param[in]      node    file node in which the disk copy of the page exists.
@return true if successful */
bool buf_page_io_complete(buf_page_t *bpage, bool evict,
                          IORequest *type = nullptr,
                          fil_node_t *node = nullptr);

/** Free a stale page. Caller must hold the LRU mutex. Upon successful page
free the LRU mutex will be released.
@param[in,out]  buf_pool   Buffer pool the page belongs to.
@param[in,out]  bpage      Page to free.
@return true if page was freed. */
bool buf_page_free_stale(buf_pool_t *buf_pool, buf_page_t *bpage) noexcept;

/** Evict a page from the buffer pool.
@param[in]  page_id    page to be evicted.
@param[in]  page_size  page size of the tablespace.
@param[in]  dirty_is_ok if true, it is OK for the page to be dirty. */
void buf_page_force_evict(const page_id_t &page_id,
                          const page_size_t &page_size,
                          const bool dirty_is_ok = true) noexcept;

/** Free a stale page. Caller must be holding the hash_lock in S mode if
hash_lock parameter is not nullptr. The hash lock will be released upon return
always. Caller must hold the LRU mutex if and only if the hash_lock parameter
is nullptr. Upon unsuccessful page free the LRU mutex will not be released if
hash_lock is nullptr.
@param[in,out]  buf_pool   Buffer pool the page belongs to.
@param[in,out]  bpage      Page to free.
@param[in,out]  hash_lock  Hash lock covering the fetch from the hash table if
latched in S mode. nullptr otherwise.
@return true if page was freed. */
bool buf_page_free_stale(buf_pool_t *buf_pool, buf_page_t *bpage,
                         rw_lock_t *hash_lock) noexcept;

/** Free a stale page that is being written. The caller must be within the
page's write code path.
@param[in,out] bpage            Page to free.
@param[in] owns_sx_lock         SX lock on block->lock is set. */
void buf_page_free_stale_during_write(buf_page_t *bpage,
                                      bool owns_sx_lock = false) noexcept;

/** Calculates the index of a buffer pool to the buf_pool[] array.
 @return the position of the buffer pool in buf_pool[] */
[[nodiscard]] static inline ulint buf_pool_index(
    const buf_pool_t *buf_pool); /*!< in: buffer pool */
/** Returns the buffer pool instance given a page instance
 @return buf_pool */
static inline buf_pool_t *buf_pool_from_bpage(
    const buf_page_t *bpage); /*!< in: buffer pool page */
/** Returns the buffer pool instance given a block instance
 @return buf_pool */
static inline buf_pool_t *buf_pool_from_block(
    const buf_block_t *block); /*!< in: block */

/** Returns the buffer pool instance given a page id.
@param[in]      page_id page id
@return buffer pool */
static inline buf_pool_t *buf_pool_get(const page_id_t &page_id);

/** Returns the buffer pool instance given its array index
 @return buffer pool */
static inline buf_pool_t *buf_pool_from_array(
    ulint index); /*!< in: array index to get
                  buffer pool instance from */

/** Returns the control block of a file page, NULL if not found.
@param[in]      buf_pool        buffer pool instance
@param[in]      page_id         page id
@return block, NULL if not found */
static inline buf_page_t *buf_page_hash_get_low(buf_pool_t *buf_pool,
                                                const page_id_t &page_id);

/** Returns the control block of a file page, NULL if not found.
If the block is found and lock is not NULL then the appropriate
page_hash lock is acquired in the specified lock mode. Otherwise,
mode value is ignored. It is up to the caller to release the
lock. If the block is found and the lock is NULL then the page_hash
lock is released by this function.
@param[in]      buf_pool        buffer pool instance
@param[in]      page_id         page id
@param[in,out]  lock            lock of the page hash acquired if bpage is
found, NULL otherwise. If NULL is passed then the hash_lock is released by
this function.
@param[in]      lock_mode       RW_LOCK_X or RW_LOCK_S. Ignored if
lock == NULL
@param[in]      watch           if true, return watch sentinel also.
@return pointer to the bpage or NULL; if NULL, lock is also NULL or
a watch sentinel. */
static inline buf_page_t *buf_page_hash_get_locked(buf_pool_t *buf_pool,
                                                   const page_id_t &page_id,
                                                   rw_lock_t **lock,
                                                   ulint lock_mode,
                                                   bool watch = false);

/** Returns the control block of a file page, NULL if not found.
If the block is found and lock is not NULL then the appropriate
page_hash lock is acquired in the specified lock mode. Otherwise,
mode value is ignored. It is up to the caller to release the
lock. If the block is found and the lock is NULL then the page_hash
lock is released by this function.
@param[in]      buf_pool        buffer pool instance
@param[in]      page_id         page id
@param[in,out]  lock            lock of the page hash acquired if bpage is
found, NULL otherwise. If NULL is passed then the hash_lock is released by
this function.
@param[in]      lock_mode       RW_LOCK_X or RW_LOCK_S. Ignored if
lock == NULL
@return pointer to the block or NULL; if NULL, lock is also NULL. */
static inline buf_block_t *buf_block_hash_get_locked(buf_pool_t *buf_pool,
                                                     const page_id_t &page_id,
                                                     rw_lock_t **lock,
                                                     ulint lock_mode);

/* There are four different ways we can try to get a bpage or block
from the page hash:
1) Caller already holds the appropriate page hash lock: in the case call
buf_page_hash_get_low() function.
2) Caller wants to hold page hash lock in x-mode
3) Caller wants to hold page hash lock in s-mode
4) Caller doesn't want to hold page hash lock */
inline buf_page_t *buf_page_hash_get_s_locked(buf_pool_t *b,
                                              const page_id_t &page_id,
                                              rw_lock_t **l) {
  return buf_page_hash_get_locked(b, page_id, l, RW_LOCK_S);
}
inline buf_page_t *buf_page_hash_get_x_locked(buf_pool_t *b,
                                              const page_id_t &page_id,
                                              rw_lock_t **l) {
  return buf_page_hash_get_locked(b, page_id, l, RW_LOCK_X);
}
inline buf_page_t *buf_page_hash_get(buf_pool_t *b, const page_id_t &page_id) {
  return buf_page_hash_get_locked(b, page_id, nullptr, 0);
}
inline buf_page_t *buf_page_get_also_watch(buf_pool_t *b,
                                           const page_id_t &page_id) {
  return buf_page_hash_get_locked(b, page_id, nullptr, 0, true);
}

inline buf_block_t *buf_block_hash_get_s_locked(buf_pool_t *b,
                                                const page_id_t &page_id,
                                                rw_lock_t **l) {
  return buf_block_hash_get_locked(b, page_id, l, RW_LOCK_S);
}
inline buf_block_t *buf_block_hash_get_x_locked(buf_pool_t *b,
                                                const page_id_t &page_id,
                                                rw_lock_t **l) {
  return buf_block_hash_get_locked(b, page_id, l, RW_LOCK_X);
}
inline buf_block_t *buf_block_hash_get(buf_pool_t *b,
                                       const page_id_t &page_id) {
  return buf_block_hash_get_locked(b, page_id, nullptr, 0);
}

/** Gets the current length of the free list of buffer blocks.
 @return length of the free list */
ulint buf_get_free_list_len(void);

/** Determine if a block is a sentinel for a buffer pool watch.
@param[in]      buf_pool        buffer pool instance
@param[in]      bpage           block
@return true if a sentinel for a buffer pool watch, false if not */
[[nodiscard]] bool buf_pool_watch_is_sentinel(const buf_pool_t *buf_pool,
                                              const buf_page_t *bpage);

/** Stop watching if the page has been read in.
buf_pool_watch_set(same_page_id) must have returned NULL before.
@param[in]      page_id page id */
void buf_pool_watch_unset(const page_id_t &page_id);

/** Check if the page has been read in.
This may only be called after buf_pool_watch_set(same_page_id)
has returned NULL and before invoking buf_pool_watch_unset(same_page_id).
@param[in]      page_id page id
@return false if the given page was not read in, true if it was */
[[nodiscard]] bool buf_pool_watch_occurred(const page_id_t &page_id);

/** Get total buffer pool statistics.
@param[out] LRU_len Length of all lru lists
@param[out] free_len Length of all free lists
@param[out] flush_list_len Length of all flush lists */
void buf_get_total_list_len(ulint *LRU_len, ulint *free_len,
                            ulint *flush_list_len);

/** Get total list size in bytes from all buffer pools. */
void buf_get_total_list_size_in_bytes(
    buf_pools_list_size_t *buf_pools_list_size); /*!< out: list sizes
                                                 in all buffer pools */
/** Get total buffer pool statistics. */
void buf_get_total_stat(
    buf_pool_stat_t *tot_stat); /*!< out: buffer pool stats */

/** Get the nth chunk's buffer block in the specified buffer pool.
@param[in]      buf_pool        buffer pool instance
@param[in]      n               nth chunk in the buffer pool
@param[in]      chunk_size      chunk_size
@return the nth chunk's buffer block. */
static inline buf_block_t *buf_get_nth_chunk_block(const buf_pool_t *buf_pool,
                                                   ulint n, ulint *chunk_size);

/** Calculate aligned buffer pool size based on srv_buf_pool_chunk_unit,
if needed.
@param[in]      size    size in bytes
@return aligned size */
static inline ulint buf_pool_size_align(ulint size);

/** Adjust the proposed chunk unit size so that it satisfies all invariants
@param[in]      size    proposed size of buffer pool chunk unit in bytes
@return adjusted size which meets invariants */
ulonglong buf_pool_adjust_chunk_unit(ulonglong size);

/** Calculate the checksum of a page from compressed table and update the
page.
@param[in,out]  page              page to update
@param[in]      size              compressed page size
@param[in]      lsn               LSN to stamp on the page
@param[in]      skip_lsn_check    true to skip check for lsn (in DEBUG) */
void buf_flush_update_zip_checksum(buf_frame_t *page, ulint size, lsn_t lsn,
                                   bool skip_lsn_check);

/** Return how many more pages must be added to the withdraw list to reach the
withdraw target of the currently ongoing buffer pool resize.
@param[in]      buf_pool        buffer pool instance
@return page count to be withdrawn or zero if the target is already achieved or
if the buffer pool is not currently being resized. */
static inline ulint buf_get_withdraw_depth(buf_pool_t *buf_pool);

#endif /* !UNIV_HOTBACKUP */

/** The common buffer control block structure
for compressed and uncompressed frames */

/** Number of bits used for buffer page states. */
constexpr uint32_t BUF_PAGE_STATE_BITS = 3;

template <typename T>
class copyable_atomic_t : public std::atomic<T> {
 public:
  copyable_atomic_t(const copyable_atomic_t<T> &other)
      : std::atomic<T>(other.load(std::memory_order_relaxed)) {}
};

using buf_fix_count_atomic_t = copyable_atomic_t<uint32_t>;
class buf_page_t {
 public:
  /** Copy constructor.
  @param[in] other       Instance to copy from. */
  buf_page_t(const buf_page_t &other)
      : id(other.id),
        size(other.size),
        buf_fix_count(other.buf_fix_count),
        io_fix(other.io_fix),
        state(other.state),
        flush_type(other.flush_type),
        buf_pool_index(other.buf_pool_index),
#ifndef UNIV_HOTBACKUP
        hash(other.hash),
#endif /* !UNIV_HOTBACKUP */
        list(other.list),
        newest_modification(other.newest_modification),
        oldest_modification(other.oldest_modification),
        LRU(other.LRU),
        zip(other.zip)
#ifndef UNIV_HOTBACKUP
        ,
        m_flush_observer(other.m_flush_observer),
        m_space(other.m_space),
        freed_page_clock(other.freed_page_clock),
        m_version(other.m_version),
        access_time(other.access_time),
        m_dblwr_id(other.m_dblwr_id),
        old(other.old)
#ifdef UNIV_DEBUG
        ,
        file_page_was_freed(other.file_page_was_freed),
        in_flush_list(other.in_flush_list),
        in_free_list(other.in_free_list),
        in_LRU_list(other.in_LRU_list),
        in_page_hash(other.in_page_hash),
        in_zip_hash(other.in_zip_hash)
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */
  {
#ifndef UNIV_HOTBACKUP
    m_space->inc_ref();
#endif /* !UNIV_HOTBACKUP */
  }

 public:
  /** Check if the given ptr lies in a memory block of type BUF_BLOCK_MEMORY.
  This is checked by looking at the FIL_PAGE_LSN.  If the FIL_PAGE_LSN is zero,
  then the block state is assumed to be BUF_BLOCK_MEMORY.
  @return true if the FIL_PAGE_LSN is zero, false otherwise. */
  [[nodiscard]] static bool is_memory(const page_t *const ptr) noexcept;

  /** Check if the state of this page is BUF_BLOCK_MEMORY.
  @return true if the state is BUF_BLOCK_MEMORY, or false. */
  [[nodiscard]] bool is_memory() const noexcept {
    return state == BUF_BLOCK_MEMORY;
  }

#ifndef UNIV_HOTBACKUP
  /** Set the doublewrite buffer ID.
  @param[in]  batch_id  Double write batch ID that flushed the page. */
  void set_dblwr_batch_id(uint16_t batch_id) { m_dblwr_id = batch_id; }

  /** @return the double write batch id, or uint16_t max if undefined. */
  [[nodiscard]] uint16_t get_dblwr_batch_id() const { return (m_dblwr_id); }

  /** Retrieve the tablespace id.
  @return tablespace id */
  [[nodiscard]] space_id_t space() const noexcept { return id.space(); }

  /** Retrieve the page number.
  @return page number */
  [[nodiscard]] page_no_t page_no() const noexcept { return id.page_no(); }

  /** Checks if this space reference saved during last page ID initialization
  was deleted or truncated since.
  @return true when space reference stored leads was deleted or truncated and
  this page should be discarded. Result is up to date until the fil_shard mutex
  is released. */
  inline bool is_stale() const {
    ut_a(m_space != nullptr);
    ut_a(id.space() == m_space->id);
    ut_a(m_version <= m_space->get_current_version());
    if (m_version == m_space->get_current_version()) {
      ut_a(!m_space->is_deleted());
      return false;
    } else {
      return true;
    }
  }

  /** Checks if this space reference saved during last page ID initialization
  was deleted or truncated since.
  @return true when space reference stored leads was deleted or truncated and
  this page should be discarded. When false is returned, the status of stale is
  checked to be guaranteed. */
  inline bool was_stale() const {
    ut_a(m_space != nullptr);
    ut_a(id.space() == m_space->id);
    /* If the the version is OK, then the space must not be deleted.
    However, version is modified before the deletion flag is set, so reading
    these values need to be executed in reversed order. The atomic reads
    cannot be relaxed for it to work. */
    bool was_not_deleted = m_space->was_not_deleted();
    if (m_version == m_space->get_recent_version()) {
      ut_a(was_not_deleted);
      return false;
    } else {
      return true;
    }
  }

  /** Retrieve the tablespace object if one was available during page ID
  initialization. The returned object is safe to use as long as this buf_page_t
  object is not changed. Caller should have a IO fix, buffer fix, mutex or any
  other mean to assure the page will not be freed. After that is released the
  space object may be freed.
  @return tablespace object */
  inline fil_space_t *get_space() const { return m_space; }

  /** Set the stored page id to a new value. This is used only on a buffer
  block with BUF_BLOCK_MEMORY state.
  @param[in]  page_id  the new value of the page id. */
  void set_page_id(const page_id_t page_id) {
    ut_ad(state == BUF_BLOCK_MEMORY);
    id = page_id;
  }

  /** Set the page size to a new value. This can be used during initialization
  of a newly allocated buffer page.
  @param[in]  page_size  the new value of the page size. */
  void set_page_size(const page_size_t &page_size) {
    ut_ad(state == BUF_BLOCK_MEMORY);
    size = page_size;
  }

  /** Sets stored page ID to the new value. Handles space object reference
  count.
  @param[in]    new_page_id  new page ID to be set. */
  inline void reset_page_id(page_id_t new_page_id) {
    if (m_space != nullptr) {
      /* If we reach this line through a call chain:
      srv_shutdown -> buf_pool_free_all -> buf_pool_free_instance ->
      buf_page_free_descriptor, then we are already past the fil system
      shutdown, and all fil_space_t objects were already freed. */
      if (srv_shutdown_state.load() != SRV_SHUTDOWN_EXIT_THREADS) {
        m_space->dec_ref();
      }
    }
    id = new_page_id;
    space_id_changed();
  }

  /** Sets stored value to invalid/empty value. Handles space object reference
  count. */
  inline void reset_page_id() {
    reset_page_id(page_id_t(UINT32_UNDEFINED, UINT32_UNDEFINED));
  }

 private:
  /** Updates new space reference and acquires "reference count latch" and the
  current version of the space object. */
  inline void space_id_changed() {
    m_space = nullptr;
    m_version = 0;
    if (id.space() != UINT32_UNDEFINED) {
      m_space = fil_space_get(id.space());
      /* There could be non-existent tablespace while importing it */
      if (m_space) {
        m_space->inc_ref();
        /* We don't have a way to check the MDL locks, which are guarding the
        version number, so we don't use get_current_version(). */
        m_version = m_space->get_recent_version();
      }
    }
  }

 public:
  /** @return the flush observer instance. */
  Flush_observer *get_flush_observer() noexcept { return m_flush_observer; }

  /** Set the flush observer for the page.
  @param[in] flush_observer     The flush observer to set. */
  void set_flush_observer(Flush_observer *flush_observer) noexcept {
    /* Don't allow to set flush observer from non-null to null, or from one
    observer to another. */
    ut_a(m_flush_observer == nullptr || m_flush_observer == flush_observer);
    m_flush_observer = flush_observer;
  }

  /** Remove the flush observer. */
  void reset_flush_observer() noexcept { m_flush_observer = nullptr; }
#endif /* !UNIV_HOTBACKUP */

  /** @return the LSN of the latest modification. */
  lsn_t get_newest_lsn() const noexcept { return newest_modification; }

  /** @return the LSN of the first modification since the last time
  it was clean. */
  lsn_t get_oldest_lsn() const noexcept { return oldest_modification; }

  /** @return true if the page is dirty. */
  bool is_dirty() const noexcept { return get_oldest_lsn() > 0; }

  /** Set the latest modification LSN.
  @param[in] lsn                Latest modification lSN. */
  void set_newest_lsn(lsn_t lsn) noexcept { newest_modification = lsn; }

  /** Set the LSN when the page is modified for the first time.
  @param[in] lsn                First modification LSN. */
  void set_oldest_lsn(lsn_t lsn) noexcept;

  /** Set page to clean state. */
  void set_clean() noexcept { set_oldest_lsn(0); }

  /** @name General fields
  None of these bit-fields must be modified without holding
  buf_page_get_mutex() [buf_block_t::mutex or
  buf_pool->zip_mutex], since they can be stored in the same
  machine word.  */
  /** @{ */

  /** Page id. */
  page_id_t id;

  /** Page size. */
  page_size_t size;

  /** Count of how many fold this block is currently bufferfixed. */
  buf_fix_count_atomic_t buf_fix_count;

 private:
  /** Type of pending I/O operation.
  Modified under protection of buf_page_get_mutex(this).
  Read under protection of rules described in @see Buf_io_fix_latching_rules */
  copyable_atomic_t<buf_io_fix> io_fix;

#ifdef UNIV_DEBUG
 public:
  /** Checks if io_fix has any of the known enum values.
  @param[in]  io_fix  the value to test
  @return true iff io_fix has any of the known enum values
  */
  static bool is_correct_io_fix_value(buf_io_fix io_fix) {
    switch (io_fix) {
      case BUF_IO_NONE:
      case BUF_IO_READ:
      case BUF_IO_WRITE:
      case BUF_IO_PIN:
        return true;
    }
    return false;
  }

 private:
  /** Checks if io_fix has any of the known enum values.
  @return true iff io_fix has any of the known enum values
  */
  bool has_correct_io_fix_value() const {
    return is_correct_io_fix_value(io_fix);
  }
  /* Helper debug-only functions related latching rules are moved to a separate
  class so that this header doesn't have to pull in Stateful_latching_rules.*/
  class Latching_rules_helpers;
  friend class Latching_rules_helpers;

  /* Helper debug-only class used to track which thread is currently responsible
  for performing I/O operation on this page. There's at most one such thread and
  the responsibility might be passed from one to another during async I/O. This
  is used to prove correctness of io_fix state transitions and checking it
  without a latch in the io_completion threads. */
  class io_responsibility_t {
    /** The thread responsible for I/O on this page, or an impossible value if
    no thread is currently responsible for I/O*/
    std::thread::id responsible_thread{std::thread().get_id()};

   public:
    /** Checks if there is any thread responsible for I/O on this page now.
    @return true iff there is a thread responsible for I/O on this page.*/
    bool someone_is_responsible() const {
      return responsible_thread != std::thread().get_id();
    }

    /** Checks if the current thread is responsible for I/O on this page now.
    @return true iff the current thread is responsible for I/O on this page.*/
    bool current_thread_is_responsible() const {
      return responsible_thread == std::this_thread::get_id();
    }

    /** Called by the thread responsible for I/O on this page to release its
    responsibility. */
    void release() {
      ut_a(current_thread_is_responsible());
      responsible_thread = std::thread().get_id();
    }

    /** Called by the thread which becomes responsible for I/O on this page to
    indicate that it takes the responsibility. */
    void take() {
      ut_a(!someone_is_responsible());
      responsible_thread = std::this_thread::get_id();
    }
  };
  /** Tracks which thread is responsible for I/O on this page. */
  io_responsibility_t io_responsibility;

 public:
  /** Checks if there is any thread responsible for I/O on this page now.
  @return true iff there is a thread responsible for I/O on this page.*/
  bool someone_has_io_responsibility() const {
    return io_responsibility.someone_is_responsible();
  }

  /** Checks if the current thread is responsible for I/O on this page now.
  @return true iff the current thread is responsible for I/O on this page.*/
  bool current_thread_has_io_responsibility() const {
    return io_responsibility.current_thread_is_responsible();
  }

  /** Called by the thread responsible for I/O on this page to release its
  responsibility. */
  void release_io_responsibility() { io_responsibility.release(); }

  /** Called by the thread which becomes responsible for I/O on this page to
  indicate that it takes the responsibility. */
  void take_io_responsibility() {
    ut_ad(mutex_own(buf_page_get_mutex(this)) ||
          io_fix.load(std::memory_order_relaxed) == BUF_IO_WRITE ||
          io_fix.load(std::memory_order_relaxed) == BUF_IO_READ);
    io_responsibility.take();
  }
#endif /* UNIV_DEBUG */
 private:
  /** Retrieves a value of io_fix without requiring or acquiring any latches.
  Note that this implies that the value might be stale unless caller establishes
  happens-before relation in some other way.
  This is a low-level function which shouldn't be used directly, but
  rather via wrapper methods which check if proper latches are taken or via one
  of the many `was_io_fix_something()` methods with name explicitly warning the
  developer about the uncertainty involved.
  @return the value of io_fix at some moment "during" the call */
  buf_io_fix get_io_fix_snapshot() const {
    ut_ad(has_correct_io_fix_value());
    return io_fix.load(std::memory_order_relaxed);
  }

 public:
  /** This is called only when having full ownership of the page object and no
  other thread can reach it. This currently happens during buf_pool_create(),
  buf_pool_resize() (which latch quite a lot) or from fil_tablespace_iterate()
  which creates a fake, private block which is not really a part of the buffer
  pool.
  Therefore we allow this function to set io_fix without checking for any
  latches.
  Please use set_io_fix(BUF_IO_NONE) to change state in a regular situation. */
  void init_io_fix() {
    io_fix.store(BUF_IO_NONE, std::memory_order_relaxed);
    /* This is only needed because places which call init_io_fix() do not call
    buf_page_t's constructor */
    ut_d(new (&io_responsibility) io_responsibility_t{});
  }

  /** This is called only when having full ownership of the page object and no
  other thread can reach it. This currently happens during buf_page_init_low()
  under buf_page_get_mutex(this), on a previously initialized page for reuse,
  yet should be treated as initialization of the field, not a state transition.
  Please use set_io_fix(BUF_IO_NONE) to change state in a regular situation. */
  void reinit_io_fix() {
    ut_ad(io_fix.load(std::memory_order_relaxed) == BUF_IO_NONE);
    ut_ad(!someone_has_io_responsibility());
    io_fix.store(BUF_IO_NONE, std::memory_order_relaxed);
  }

  /** Sets io_fix to specified value.
  Assumes the caller holds buf_page_get_mutex(this).
  Might require additional latches depending on particular state transition.
  Calls take_io_responsibility() or release_io_responsibility() as needed.
  @see Buf_io_fix_latching_rules for specific rules. */
  void set_io_fix(buf_io_fix io_fix);

  /** Retrieves the current value of io_fix.
  Assumes the caller holds buf_page_get_mutex(this).
  @return the current value of io_fix */
  buf_io_fix get_io_fix() const {
    ut_ad(mutex_own(buf_page_get_mutex(this)));
    return get_io_fix_snapshot();
  }

  /** Checks if the current value of io_fix is BUF_IO_WRITE.
  Assumes the caller holds buf_page_get_mutex(this) or some other latches which
  prevent state transition from/to BUF_IO_WRITE.
  @see Buf_io_fix_latching_rules for specific rules.
  @return true iff the current value of io_fix == BUF_IO_WRITE */
  bool is_io_fix_write() const;

  /** Checks if the current value of io_fix is BUF_IO_READ.
  Assumes the caller holds buf_page_get_mutex(this) or some other latches which
  prevent state transition from/to BUF_IO_READ.
  @see Buf_io_fix_latching_rules for specific rules.
  @return true iff the current value of io_fix == BUF_IO_READ */
  bool is_io_fix_read() const;

  /** Assuming that io_fix is either BUF_IO_READ or BUF_IO_WRITE determines
  which of the two it is. Additionally it assumes the caller holds
  buf_page_get_mutex(this) or some other latches which prevent state transition
  from BUF_IO_READ or from BUF_IO_WRITE to another state.
  @see Buf_io_fix_latching_rules for specific rules.
  @return true iff the current value of io_fix == BUF_IO_READ */
  bool is_io_fix_read_as_opposed_to_write() const;

  /** Checks if io_fix is BUF_IO_READ without requiring or acquiring any
  latches.
  Note that this implies calling this function twice in a row could produce
  different results.
  @return true iff io_fix equal to BUF_IO_READ was noticed*/
  bool was_io_fix_read() const { return get_io_fix_snapshot() == BUF_IO_READ; }

  /** Checks if io_fix is BUF_IO_FIX or BUF_IO_READ or BUF_IO_WRITE without
  requiring or acquiring any latches.
  Note that this implies calling this function twice in a row could produce
  different results.
  @return true iff io_fix not equal to BUF_IO_NONE was noticed */
  bool was_io_fixed() const { return get_io_fix_snapshot() != BUF_IO_NONE; }

  /** Checks if io_fix is BUF_IO_NONE without requiring or acquiring any
  latches.
  Note that this implies calling this function twice in a row could produce
  different results.
  Please, prefer this function over !was_io_fixed() to avoid the misleading
  interpretation as "not(Exists time such that io_fix(time))", while in fact we
  want and get "Exists time such that !io_fix(time)".
  @return true iff io_fix equal to BUF_IO_NONE was noticed */
  bool was_io_fix_none() const { return get_io_fix_snapshot() == BUF_IO_NONE; }

  /** Block state. @see buf_page_in_file */
  buf_page_state state;

  /** If this block is currently being flushed to disk, this tells
  the flush_type.  @see buf_flush_t */
  buf_flush_t flush_type;

  /** Index number of the buffer pool that this block belongs to */
  uint8_t buf_pool_index;

  static_assert(MAX_BUFFER_POOLS <= 64,
                "MAX_BUFFER_POOLS > 64; redefine buf_pool_index");

  /** @} */
#ifndef UNIV_HOTBACKUP
  /** Node used in chaining to buf_pool->page_hash or buf_pool->zip_hash */
  buf_page_t *hash;
#endif /* !UNIV_HOTBACKUP */

  /** @name Page flushing fields
  All these are protected by buf_pool->mutex. */
  /** @{ */

  /** Based on state, this is a list node, protected by the corresponding list
  mutex, in one of the following lists in buf_pool:

  - BUF_BLOCK_NOT_USED: free, withdraw
  - BUF_BLOCK_FILE_PAGE:        flush_list
  - BUF_BLOCK_ZIP_DIRTY:        flush_list
  - BUF_BLOCK_ZIP_PAGE: zip_clean

  The node pointers are protected by the corresponding list mutex.

  The contents of the list node is undefined if !in_flush_list &&
  state == BUF_BLOCK_FILE_PAGE, or if state is one of
  BUF_BLOCK_MEMORY,
  BUF_BLOCK_REMOVE_HASH or
  BUF_BLOCK_READY_IN_USE. */

  UT_LIST_NODE_T(buf_page_t) list;

 private:
  /** The flush LSN, LSN when this page was written to the redo log. For
  non redo logged pages this is set using: buf_flush_borrow_lsn() */
  lsn_t newest_modification;

  /** log sequence number of the youngest modification to this block, zero
  if not modified. Protected by block mutex */
  lsn_t oldest_modification;

 public:
  /** log sequence number of the START of the log entry written of the oldest
  modification to this block which has not yet been flushed on disk; zero if
  all modifications are on disk.  Writes to this field must be covered by both
  block->mutex and buf_pool->flush_list_mutex. Hence reads can happen while
  holding any one of the two mutexes */
  /** @} */

  /** @name LRU replacement algorithm fields
  These fields are protected by both buf_pool->LRU_list_mutex and the
  block mutex. */
  /** @{ */

  /** node of the LRU list */
  UT_LIST_NODE_T(buf_page_t) LRU;

  /** compressed page; zip.data (but not the data it points to) is
  protected by buf_pool->zip_mutex; state == BUF_BLOCK_ZIP_PAGE and
  zip.data == NULL means an active buf_pool->watch */
  page_zip_des_t zip;

#ifndef UNIV_HOTBACKUP
  /** Flush observer instance. */
  Flush_observer *m_flush_observer{};

  /** Tablespace instance that this page belongs to. */
  fil_space_t *m_space{};

  /** The value of buf_pool->freed_page_clock when this block was the last
  time put to the head of the LRU list; a thread is allowed to read this
  for heuristic purposes without holding any mutex or latch */
  uint32_t freed_page_clock;

  /** @} */
  /** Version of fil_space_t when the page was updated. It can also be viewed as
   the truncation number. */
  uint32_t m_version{};

  /** Time of first access, or 0 if the block was never accessed in the
  buffer pool. Protected by block mutex */
  std::chrono::steady_clock::time_point access_time;

 private:
  /** Double write instance ordinal value during writes. This is used
  by IO completion (writes) to select the double write instance.*/
  uint16_t m_dblwr_id{};

 public:
  /** true if the block is in the old blocks in buf_pool->LRU_old */
  bool old;

#ifdef UNIV_DEBUG
  /** This is set to true when fsp frees a page in buffer pool;
  protected by buf_pool->zip_mutex or buf_block_t::mutex. */
  bool file_page_was_freed;

  /** true if in buf_pool->flush_list; when buf_pool->flush_list_mutex
  is free, the following should hold:
    in_flush_list == (state == BUF_BLOCK_FILE_PAGE ||
                      state == BUF_BLOCK_ZIP_DIRTY)
  Writes to this field must be covered by both buf_pool->flush_list_mutex
  and block->mutex. Hence reads can happen while holding any one of the
  two mutexes */
  bool in_flush_list;

  /** true if in buf_pool->free; when buf_pool->free_list_mutex is free, the
  following should hold: in_free_list == (state == BUF_BLOCK_NOT_USED) */
  bool in_free_list;

  /** true if the page is in the LRU list; used in debugging */
  bool in_LRU_list;

  /** true if in buf_pool->page_hash */
  bool in_page_hash;

  /** true if in buf_pool->zip_hash */
  bool in_zip_hash;
#endif /* UNIV_DEBUG */

#endif /* !UNIV_HOTBACKUP */
};

/** Structure used by AHI to contain information on record prefixes to be
considered in hash index subsystem. It is meant for using as a single 64bit
atomic value, thus it needs to be aligned properly. */
struct alignas(alignof(uint64_t)) btr_search_prefix_info_t {
  /** recommended prefix: number of bytes in an incomplete field
  @see BTR_PAGE_MAX_REC_SIZE */
  uint32_t n_bytes;
  /** recommended prefix length for hash search: number of full fields */
  uint16_t n_fields;
  /** true or false, depending on whether the leftmost record of several records
  with the same prefix should be indexed in the hash index */
  bool left_side;

  bool equals_without_left_side(const btr_search_prefix_info_t &other) const {
    return n_bytes == other.n_bytes && n_fields == other.n_fields;
  }

  bool operator==(const btr_search_prefix_info_t &other) const {
    return n_bytes == other.n_bytes && n_fields == other.n_fields &&
           left_side == other.left_side;
  }

  bool operator!=(const btr_search_prefix_info_t &other) const {
    return !(*this == other);
  }
};

/** The buffer control block structure */
struct buf_block_t {
  /** @name General fields */
  /** @{ */

  /** page information; this must be the first field, so
  that buf_pool->page_hash can point to buf_page_t or buf_block_t */
  buf_page_t page;

#ifndef UNIV_HOTBACKUP
  /** read-write lock of the buffer frame */
  BPageLock lock;

#ifdef UNIV_DEBUG
  /** Check if the buffer block was freed.
  @return true if the block was freed, false otherwise. */
  bool was_freed() const { return page.file_page_was_freed; }
#endif /* UNIV_DEBUG */

#endif /* UNIV_HOTBACKUP */

  /** pointer to buffer frame which is of size UNIV_PAGE_SIZE, and aligned
  to an address divisible by UNIV_PAGE_SIZE */
  byte *frame;

  /** Determine whether the page is in new-style compact format.
  @return true  if the page is in compact format
  @return false if it is in old-style format */
  bool is_compact() const;

  /** node of the decompressed LRU list; a block is in the unzip_LRU list if
  page.state == BUF_BLOCK_FILE_PAGE and page.zip.data != NULL. Protected by
  both LRU_list_mutex and the block mutex. */
  UT_LIST_NODE_T(buf_block_t) unzip_LRU;
#ifdef UNIV_DEBUG
  /** true if the page is in the decompressed LRU list; used in debugging */
  bool in_unzip_LRU_list;

  bool in_withdraw_list;
#endif /* UNIV_DEBUG */

  /** @} */

  /** Structure that holds most AHI-related fields. */
  struct ahi_t {
   public:
    /** Recommended prefix info for hash search. It is atomically copied
    from the index's current recommendation for the prefix info and should
    eventually get to the block's actual prefix info used. It is used to decide
    when the n_hash_helps should be reset. It is modified only while having S-
    or X- latch on block's lock. */
    std::atomic<btr_search_prefix_info_t> recommended_prefix_info;
    /** Prefix info that was used for building hash index. It cannot be modified
    while there are any record entries added in the AHI. It's invariant that all
    records added to AHI from this block were folded using this prefix info. It
    may only be modified when we are holding the appropriate X-latch in
    btr_search_sys->parts[]->latch. Also, it happens that it is modified
    to not-empty value only when the block is held in private or the block's
    lock is S- or X-latched. This implies that the field's non-empty value may
    be read and use reliably when the appropriate
    btr_search_sys->parts[]->latch S-latch or X-latch is being held, or
    the block's lock is X-latched. */
    std::atomic<btr_search_prefix_info_t> prefix_info;
    static_assert(decltype(prefix_info)::is_always_lock_free);

    /** Index for which the adaptive hash index has been created, or nullptr if
    the page does not exist in the index. Note that it does not guarantee that
    the AHI index is complete, though: there may have been hash collisions etc.
    It may be modified:
    - to nullptr if btr_search_enabled is false and block's mutex is held and
    block's state is BUF_BLOCK_FILE_PAGE and btr_search_enabled_mutex is
    owned, protecting the btr_search_enabled from being changed,
    - to nullptr if btr_search_enabled is false and block is held in private in
    BUF_BLOCK_REMOVE_HASH state in buf_LRU_free_page().
    - to any value under appropriate X-latch in btr_search_sys->parts[]->latch
    if btr_search_enabled is true (and setting btr_search_enabled to false in
    turn is protected by having all btr_search_sys->parts[]->latch X-latched).
    */
    std::atomic<dict_index_t *> index;

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
    /** Used in debugging. The number of pointers in the adaptive hash index
    pointing to this frame; Modified under appropriate X-latch in
    btr_search_sys->parts[]->latch. */
    std::atomic<uint16_t> n_pointers;

    inline void validate() const {
      /* These fields are read without holding any AHI latches. Adding or
      removing a block from AHI requires having only an appropriate AHI part
      X-latched. If we have at least S-latched the correct AHI part (for which
      we would need at least S-latch on block for the block's index to not be
      changed in meantime) this check is certain. If we don't have necessary AHI
      latches, then:
      - it can't happen that the check fails while the block is removed from
      AHI. Both btr_search_drop_page_hash_index() and
      buf_pool_clear_hash_index() will first make the n_pointers be 0 and then
      set index to nullptr. As the index is an atomic variable, so if we
      synchronize with a reset to nullptr which is sequenced after the reset of
      n_pointers, we should see the n_pointers set to 0 here.
      - it can happen that the check fails while the block is added to the AHI
      right after we read the index is nullptr. In such case, if the n_pointers
      is not 0, we double check the index member. It can still be nullptr, if
      the block is removed after reading the n_pointers, but that should be near
      impossible. */
      ut_a(this->index.load() != nullptr || this->n_pointers.load() == 0 ||
           this->index.load() != nullptr);
    }

    inline void assert_empty() const { ut_a(this->n_pointers.load() == 0); }

    inline void assert_empty_on_init() const {
      UNIV_MEM_VALID(&this->n_pointers, sizeof(this->n_pointers));
      assert_empty();
    }
#else
    inline void validate() const {}

    inline void assert_empty() const {}

    inline void assert_empty_on_init() const {}
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
  } ahi;

  /** Counter which controls how many times the current prefix recommendation
  would help in searches. If it is helpful enough, it will be used as the
  actual prefix to build hash for this block. It is modified similarly as
  recommended_prefix_info, that is only while having S- or X- latch on block's
  lock. Because it is modified concurrently, it may not have fully reliable
  count, but it is enough for this use case.
  Mind the n_hash_helps is AHI-related, and should be in the ahi_t struct above,
  but having it outside causes the made_dirty_with_no_latch to occupy the common
  8byte aligned 8byte long space, so basically it saves us 8bytes of the object
  that is used in high volumes. */
  std::atomic<uint32_t> n_hash_helps;
  /** true if block has been made dirty without acquiring X/SX latch as the
  block belongs to temporary tablespace and block is always accessed by a
  single thread. */
  bool made_dirty_with_no_latch;

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
  /** @name Debug fields */
  /** @{ */
  /** In the debug version, each thread which bufferfixes the block acquires
  an s-latch here; so we can use the debug utilities in sync0rw */
  rw_lock_t debug_latch;
  /** @} */
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

  /** @name Optimistic search field */
  /** @{ */

  /** This clock is incremented every time a pointer to a record on the page
  may become obsolete; this is used in the optimistic cursor positioning: if
  the modify clock has not changed, we know that the pointer is still valid;
  this field may be changed if the thread (1) owns the LRU list mutex and the
  page is not bufferfixed, or (2) the thread has an x-latch on the block,
  or (3) the block must belong to an intrinsic table */
  uint64_t modify_clock;

  /** @} */

  /** mutex protecting this block: state (also protected by the buffer
  pool mutex), io_fix, buf_fix_count, and accessed; we introduce this
  new mutex in InnoDB-5.1 to relieve contention on the buffer pool mutex */
  BPageMutex mutex;

  /** Get the modified clock (version) value.
  @param[in] single_threaded    Thread can only be written to or read by a
                                single thread
  @return the modified clock vlue. */
  uint64_t get_modify_clock(IF_DEBUG(bool single_threaded)) const noexcept {
#if defined(UNIV_DEBUG) && !defined(UNIV_LIBRARY) && !defined(UNIV_HOTBACKUP)
    /* No block latch is acquired when blocks access is guaranteed to be
    in single threaded mode. */
    constexpr auto mode = RW_LOCK_FLAG_X | RW_LOCK_FLAG_SX | RW_LOCK_FLAG_S;
    ut_ad(single_threaded || rw_lock_own_flagged(&lock, mode));
#endif /* UNIV_DEBUG && !UNIV_LIBRARY */

    return modify_clock;
  }

  /** Get the page number and space id of the current buffer block.
  @return page number of the current buffer block. */
  const page_id_t &get_page_id() const { return page.id; }

  /** Get the page number of the current buffer block.
  @return page number of the current buffer block. */
  page_no_t get_page_no() const { return (page.id.page_no()); }

  /** Get the next page number of the current buffer block.
  @return next page number of the current buffer block. */
  page_no_t get_next_page_no() const {
    return (mach_read_from_4(frame + FIL_PAGE_NEXT));
  }

  /** Get the prev page number of the current buffer block.
  @return prev page number of the current buffer block. */
  page_no_t get_prev_page_no() const {
    return (mach_read_from_4(frame + FIL_PAGE_PREV));
  }

  /** Get the page type of the current buffer block.
  @return page type of the current buffer block. */
  page_type_t get_page_type() const {
    return (mach_read_from_2(frame + FIL_PAGE_TYPE));
  }

#ifndef UNIV_HOTBACKUP
  /** Mark the frame with jumbled page_id, while initiating i/o read
  (BUF_IO_READ).*/
  void mark_for_read_io() {
    memset(frame, 0x00, page.size.logical());
    mach_write_to_4(frame + FIL_PAGE_SPACE_ID, page.page_no());
    mach_write_to_4(frame + FIL_PAGE_OFFSET, page.space());
  }
#endif /* UNIV_HOTBACKUP */

  uint16_t get_page_level() const;
  bool is_leaf() const;
  bool is_root() const;
  bool is_index_page() const;

  /** Check if this index page is empty.  An index page is considered empty
  if the next record of an infimum record is supremum record.  Presence of
  del-marked records will make the page non-empty.
  @return true if this index page is empty. */
  bool is_empty() const;

  /** Get the page type of the current buffer block as string.
  @return page type of the current buffer block as string. */
  [[nodiscard]] const char *get_page_type_str() const noexcept;

  /** Gets the compressed page descriptor corresponding to an uncompressed page
  if applicable.
  @return page descriptor or nullptr. */
  page_zip_des_t *get_page_zip() noexcept {
    return page.zip.data != nullptr ? &page.zip : nullptr;
  }

  /** Const version.
  @return page descriptor or nullptr. */
  page_zip_des_t const *get_page_zip() const noexcept {
    return page.zip.data != nullptr ? &page.zip : nullptr;
  }

  [[nodiscard]] bool is_memory() const noexcept { return page.is_memory(); }
};

inline bool buf_block_t::is_root() const {
  return ((get_next_page_no() == FIL_NULL) && (get_prev_page_no() == FIL_NULL));
}

inline bool buf_block_t::is_leaf() const { return get_page_level() == 0; }

inline bool buf_block_t::is_index_page() const {
  return get_page_type() == FIL_PAGE_INDEX;
}

/** Check if a buf_block_t object is in a valid state
@param block buffer block
@return true if valid */
inline bool buf_block_state_valid(buf_block_t *block) {
  return buf_block_get_state(block) >= BUF_BLOCK_NOT_USED &&
         buf_block_get_state(block) <= BUF_BLOCK_REMOVE_HASH;
}

/** Compute the hash value for blocks in buf_pool->zip_hash. */
/** @{ */
static inline uint64_t buf_pool_hash_zip_frame(void *ptr) {
  return ut::hash_uint64(reinterpret_cast<uintptr_t>(ptr) >>
                         UNIV_PAGE_SIZE_SHIFT);
}
static inline uint64_t buf_pool_hash_zip(buf_block_t *b) {
  return buf_pool_hash_zip_frame(b->frame);
}
/** @} */

/** A "Hazard Pointer" class used to iterate over page lists
inside the buffer pool. A hazard pointer is a buf_page_t pointer
which we intend to iterate over next and we want it remain valid
even after we release the buffer pool mutex. */
class HazardPointer {
 public:
  /** Constructor
  @param buf_pool buffer pool instance
  @param mutex  mutex that is protecting the hp. */
  HazardPointer(const buf_pool_t *buf_pool, const ib_mutex_t *mutex)
      : m_buf_pool(buf_pool) IF_DEBUG(, m_mutex(mutex)), m_hp() {}

  /** Destructor */
  virtual ~HazardPointer() = default;

  /** Get current value */
  buf_page_t *get() const {
    ut_ad(mutex_own(m_mutex));
    return (m_hp);
  }

  /** Set current value
  @param bpage  buffer block to be set as hp */
  void set(buf_page_t *bpage);

  /** Checks if a bpage is the hp
  @param bpage  buffer block to be compared
  @return true if it is hp */
  bool is_hp(const buf_page_t *bpage);

  /** Adjust the value of hp. This happens when some
  other thread working on the same list attempts to
  remove the hp from the list. Must be implemented
  by the derived classes.
  @param bpage  buffer block to be compared */
  virtual void adjust(const buf_page_t *bpage) = 0;

  /** Adjust the value of hp for moving. This happens
  when some other thread working on the same list
  attempts to relocate the hp of the page.
  @param bpage  buffer block to be compared
  @param dpage  buffer block to be moved to */
  void move(const buf_page_t *bpage, buf_page_t *dpage);

 protected:
  /** Disable copying */
  HazardPointer(const HazardPointer &);
  HazardPointer &operator=(const HazardPointer &);

  /** Buffer pool instance */
  const buf_pool_t *m_buf_pool;

#ifdef UNIV_DEBUG
  /** mutex that protects access to the m_hp. */
  const ib_mutex_t *m_mutex;
#endif /* UNIV_DEBUG */

  /** hazard pointer. */
  buf_page_t *m_hp;
};

/** Class implementing buf_pool->flush_list hazard pointer */
class FlushHp : public HazardPointer {
 public:
  /** Constructor
  @param buf_pool buffer pool instance
  @param mutex  mutex that is protecting the hp. */
  FlushHp(const buf_pool_t *buf_pool, const ib_mutex_t *mutex)
      : HazardPointer(buf_pool, mutex) {}

  /** Destructor */
  ~FlushHp() override = default;

  /** Adjust the value of hp. This happens when some
  other thread working on the same list attempts to
  remove the hp from the list.
  @param bpage  buffer block to be compared */
  void adjust(const buf_page_t *bpage) override;
};

/** Class implementing buf_pool->LRU hazard pointer */
class LRUHp : public HazardPointer {
 public:
  /** Constructor
  @param buf_pool buffer pool instance
  @param mutex  mutex that is protecting the hp. */
  LRUHp(const buf_pool_t *buf_pool, const ib_mutex_t *mutex)
      : HazardPointer(buf_pool, mutex) {}

  /** Destructor */
  ~LRUHp() override = default;

  /** Adjust the value of hp. This happens when some
  other thread working on the same list attempts to
  remove the hp from the list.
  @param bpage  buffer block to be compared */
  void adjust(const buf_page_t *bpage) override;
};

/** Special purpose iterators to be used when scanning the LRU list.
The idea is that when one thread finishes the scan it leaves the
itr in that position and the other thread can start scan from
there */
class LRUItr : public LRUHp {
 public:
  /** Constructor
  @param buf_pool buffer pool instance
  @param mutex  mutex that is protecting the hp. */
  LRUItr(const buf_pool_t *buf_pool, const ib_mutex_t *mutex)
      : LRUHp(buf_pool, mutex) {}

  /** Destructor */
  ~LRUItr() override = default;

  /** Selects from where to start a scan. If we have scanned
  too deep into the LRU list it resets the value to the tail
  of the LRU list.
  @return buf_page_t from where to start scan. */
  buf_page_t *start();
};

/** Struct that is embedded in the free zip blocks */
struct buf_buddy_free_t {
  union {
    ulint size; /*!< size of the block */
    byte bytes[FIL_PAGE_DATA];
    /*!< stamp[FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID]
    == BUF_BUDDY_FREE_STAMP denotes a free
    block. If the space_id field of buddy
    block != BUF_BUDDY_FREE_STAMP, the block
    is not in any zip_free list. If the
    space_id is BUF_BUDDY_FREE_STAMP then
    stamp[0] will contain the
    buddy block size. */
  } stamp;

  buf_page_t bpage; /*!< Embedded bpage descriptor */
  UT_LIST_NODE_T(buf_buddy_free_t) list;
  /*!< Node of zip_free list */
};

/** @brief The buffer pool statistics structure. */
struct buf_pool_stat_t {
  using Shards = Counter::Shards<64>;

  /** Number of page gets performed; also successful searches through the
  adaptive hash index are counted as page gets; this field is NOT protected
  by the buffer pool mutex */
  Shards m_n_page_gets;

  /** Number of read operations. */
  std::atomic<uint64_t> n_pages_read;

  /** Number of write operations. */
  std::atomic<uint64_t> n_pages_written;

  /**  number of pages created in the pool with no read. */
  std::atomic<uint64_t> n_pages_created;

  /** Number of pages read in as part of random read ahead. */
  std::atomic<uint64_t> n_ra_pages_read_rnd;

  /** Number of pages read in as part of read ahead. */
  std::atomic<uint64_t> n_ra_pages_read;

  /** Number of read ahead pages that are evicted without being accessed.
  Protected by LRU_list_mutex. */
  uint64_t n_ra_pages_evicted;

  /** Number of pages made young, in calls to buf_LRU_make_block_young().
  Protected by LRU_list_mutex. */
  uint64_t n_pages_made_young;

  /** Number of pages not made young because the first access was not long
  enough ago, in buf_page_peek_if_too_old(). Not protected. */
  uint64_t n_pages_not_made_young;

  /** LRU size in bytes. Protected by LRU_list_mutex. */
  uint64_t LRU_bytes;

  /** Flush_list size in bytes.  Protected by flush_list_mutex */
  uint64_t flush_list_bytes;

  static void copy(buf_pool_stat_t &dst, const buf_pool_stat_t &src) noexcept {
    Counter::copy(dst.m_n_page_gets, src.m_n_page_gets);

    dst.n_pages_read.store(src.n_pages_read.load());

    dst.n_pages_written.store(src.n_pages_written.load());

    dst.n_pages_created.store(src.n_pages_created.load());

    dst.n_ra_pages_read_rnd.store(src.n_ra_pages_read_rnd.load());

    dst.n_ra_pages_read.store(src.n_ra_pages_read.load());

    dst.n_ra_pages_evicted = src.n_ra_pages_evicted;

    dst.n_pages_made_young = src.n_pages_made_young;

    dst.n_pages_not_made_young = src.n_pages_not_made_young;

    dst.LRU_bytes = src.LRU_bytes;

    dst.flush_list_bytes = src.flush_list_bytes;
  }

  void reset() {
    Counter::clear(m_n_page_gets);

    n_pages_read = 0;
    n_pages_written = 0;
    n_pages_created = 0;
    n_ra_pages_read_rnd = 0;
    n_ra_pages_read = 0;
    n_ra_pages_evicted = 0;
    n_pages_made_young = 0;
    n_pages_not_made_young = 0;
    LRU_bytes = 0;
    flush_list_bytes = 0;
  }
};

/** Statistics of buddy blocks of a given size. */
struct buf_buddy_stat_t {
  /** Number of blocks allocated from the buddy system. */
  std::atomic<ulint> used;
  /** Number of blocks relocated by the buddy system.
  Protected by buf_pool zip_free_mutex. */
  uint64_t relocated;
  /** Total duration of block relocations.
  Protected by buf_pool zip_free_mutex. */
  std::chrono::steady_clock::duration relocated_duration;

  struct snapshot_t {
    ulint used;
    uint64_t relocated;
    std::chrono::steady_clock::duration relocated_duration;
  };

  snapshot_t take_snapshot() {
    return {used.load(), relocated, relocated_duration};
  }
};

/** @brief The buffer pool structure.

NOTE! The definition appears here only for other modules of this
directory (buf) to see it. Do not use from outside! */

struct buf_pool_t {
  /** @name General fields */
  /** @{ */
  /** protects (de)allocation of chunks:
   - changes to chunks, n_chunks are performed while holding this latch,
   - reading buf_pool_should_madvise requires holding this latch for any
     buf_pool_t
   - writing to buf_pool_should_madvise requires holding these latches
     for all buf_pool_t-s */
  BufListMutex chunks_mutex;

  /** LRU list mutex */
  BufListMutex LRU_list_mutex;

  /** free and withdraw list mutex */
  BufListMutex free_list_mutex;

  /** buddy allocator mutex */
  BufListMutex zip_free_mutex;

  /** zip_hash mutex */
  BufListMutex zip_hash_mutex;

  /** Flush state protection mutex */
  ib_mutex_t flush_state_mutex;

  /** Zip mutex of this buffer pool instance, protects compressed only pages (of
  type buf_page_t, not buf_block_t */
  BufPoolZipMutex zip_mutex;

  /** Array index of this buffer pool instance */
  ulint instance_no;

  /** Current pool size in bytes */
  ulint curr_pool_size;

  /** Reserve this much of the buffer pool for "old" blocks */
  ulint LRU_old_ratio;
#ifdef UNIV_DEBUG
  /** Number of frames allocated from the buffer pool to the buddy system.
  Protected by zip_hash_mutex. */
  ulint buddy_n_frames;
#endif

  /** Number of buffer pool chunks */
  volatile ulint n_chunks;

  /** New number of buffer pool chunks */
  volatile ulint n_chunks_new;

  /** buffer pool chunks */
  buf_chunk_t *chunks;

  /** old buffer pool chunks to be freed after resizing buffer pool */
  buf_chunk_t *chunks_old;

  /** Current pool size in pages */
  ulint curr_size;

  /** Previous pool size in pages */
  ulint old_size;

  /** Size in pages of the area which the read-ahead algorithms read
  if invoked */
  page_no_t read_ahead_area;

  /** Hash table of buf_page_t or buf_block_t file pages, buf_page_in_file() ==
  true, indexed by (space_id, offset).  page_hash is protected by an array of
  mutexes. */
  hash_table_t *page_hash;

  /** Hash table of buf_block_t blocks whose frames are allocated to the zip
  buddy system, indexed by block->frame */
  hash_table_t *zip_hash;

  /** Number of pending read operations. Accessed atomically */
  std::atomic<ulint> n_pend_reads;

  /** number of pending decompressions.  Accessed atomically. */
  std::atomic<ulint> n_pend_unzip;

  /** when buf_print_io was last time called. Accesses not protected. */
  std::chrono::steady_clock::time_point last_printout_time;

  /** Statistics of buddy system, indexed by block size. Protected by zip_free
  mutex, except for the used field, which is also accessed atomically */
  buf_buddy_stat_t buddy_stat[BUF_BUDDY_SIZES_MAX + 1];

  /** Current statistics */
  buf_pool_stat_t stat;

  /** Old statistics */
  buf_pool_stat_t old_stat;

  /** @} */

  /** @name Page flushing algorithm fields */

  /** @{ */

  /** Mutex protecting the flush list access. This mutex protects flush_list,
  flush_rbt and bpage::list pointers when the bpage is on flush_list. It also
  protects writes to bpage::oldest_modification and flush_list_hp */
  BufListMutex flush_list_mutex;

  /** "Hazard pointer" used during scan of flush_list while doing flush list
  batch.  Protected by flush_list_mutex */
  FlushHp flush_hp;

  /** Entry pointer to scan the oldest page except for system temporary */
  FlushHp oldest_hp;

  /** Base node of the modified block list */
  UT_LIST_BASE_NODE_T(buf_page_t, list) flush_list;

  /** This is true when a flush of the given type is being initialized.
  Protected by flush_state_mutex. */
  bool init_flush[BUF_FLUSH_N_TYPES];

  /** This is the number of pending writes in the given flush type.  Protected
  by flush_state_mutex. */
  std::array<size_t, BUF_FLUSH_N_TYPES> n_flush;

  /** This is in the set state when there is no flush batch of the given type
  running. Protected by flush_state_mutex. */
  os_event_t no_flush[BUF_FLUSH_N_TYPES];

  /** A red-black tree is used exclusively during recovery to speed up
  insertions in the flush_list. This tree contains blocks in order of
  oldest_modification LSN and is kept in sync with the flush_list.  Each
  member of the tree MUST also be on the flush_list.  This tree is relevant
  only in recovery and is set to NULL once the recovery is over.  Protected
  by flush_list_mutex */
  ib_rbt_t *flush_rbt;

  /** A sequence number used to count the number of buffer blocks removed from
  the end of the LRU list; NOTE that this counter may wrap around at 4
  billion! A thread is allowed to read this for heuristic purposes without
  holding any mutex or latch. For non-heuristic purposes protected by
  LRU_list_mutex */
  ulint freed_page_clock;

  /** Set to false when an LRU scan for free block fails. This flag is used to
  avoid repeated scans of LRU list when we know that there is no free block
  available in the scan depth for eviction. Set to true whenever we flush a
  batch from the buffer pool. Accessed protected by memory barriers. */
  bool try_LRU_scan;

  /** Page Tracking start LSN. */
  lsn_t track_page_lsn;

  /** Check if the page modifications are tracked.
  @return true if page modifications are tracked, false otherwise. */
  bool is_tracking() { return track_page_lsn != LSN_MAX; }

  /** Maximum LSN for which write io has already started. */
  lsn_t max_lsn_io;

  /** @} */

  /** @name LRU replacement algorithm fields */
  /** @{ */

  /** Base node of the free block list */
  UT_LIST_BASE_NODE_T(buf_page_t, list) free;

  /** base node of the withdraw block list. It is only used during shrinking
  buffer pool size, not to reuse the blocks will be removed.  Protected by
  free_list_mutex */
  UT_LIST_BASE_NODE_T(buf_page_t, list) withdraw;

  /** Target length of withdraw block list, when withdrawing */
  ulint withdraw_target;

  /** "hazard pointer" used during scan of LRU while doing
  LRU list batch.  Protected by buf_pool::LRU_list_mutex */
  LRUHp lru_hp;

  /** Iterator used to scan the LRU list when searching for
  replaceable victim. Protected by buf_pool::LRU_list_mutex. */
  LRUItr lru_scan_itr;

  /** Iterator used to scan the LRU list when searching for
  single page flushing victim.  Protected by buf_pool::LRU_list_mutex. */
  LRUItr single_scan_itr;

  /** Base node of the LRU list */
  UT_LIST_BASE_NODE_T(buf_page_t, LRU) LRU;

  /** Pointer to the about LRU_old_ratio/BUF_LRU_OLD_RATIO_DIV oldest blocks in
  the LRU list; NULL if LRU length less than BUF_LRU_OLD_MIN_LEN; NOTE: when
  LRU_old != NULL, its length should always equal LRU_old_len */
  buf_page_t *LRU_old;

  /** Length of the LRU list from the block to which LRU_old points onward,
  including that block; see buf0lru.cc for the restrictions on this value; 0
  if LRU_old == NULL; NOTE: LRU_old_len must be adjusted whenever LRU_old
  shrinks or grows! */
  ulint LRU_old_len;

  /** Base node of the unzip_LRU list. The list is protected by the
  LRU_list_mutex. */
  UT_LIST_BASE_NODE_T(buf_block_t, unzip_LRU) unzip_LRU;

  /** @} */
  /** @name Buddy allocator fields
  The buddy allocator is used for allocating compressed page
  frames and buf_page_t descriptors of blocks that exist
  in the buffer pool only in compressed form. */
  /** @{ */
#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
  /** Unmodified compressed pages */
  UT_LIST_BASE_NODE_T(buf_page_t, list) zip_clean;
#endif /* UNIV_DEBUG || UNIV_BUF_DEBUG */

  /** Buddy free lists */
  UT_LIST_BASE_NODE_T(buf_buddy_free_t, list) zip_free[BUF_BUDDY_SIZES_MAX];

  /** Sentinel records for buffer pool watches. Scanning the array is protected
  by taking all page_hash latches in X. Updating or reading an individual
  watch page is protected by a corresponding individual page_hash latch. */
  buf_page_t *watch;

  /** A wrapper for buf_pool_t::allocator.alocate_large which also advices the
  OS that this chunk should not be dumped to a core file if that was requested.
  Emits a warning to the log and disables @@global.core_file if advising was
  requested but could not be performed, but still return true as the allocation
  itself succeeded.
  @param[in]      mem_size  number of bytes to allocate
  @param[in,out]  chunk     mem and mem_pfx fields of this chunk will be updated
                            to contain information about allocated memory region
  @return true iff allocated successfully */
  bool allocate_chunk(ulonglong mem_size, buf_chunk_t *chunk);

  /** A wrapper for buf_pool_t::allocator.deallocate_large which also advices
  the OS that this chunk can be dumped to a core file.
  Emits a warning to the log and disables @@global.core_file if advising was
  requested but could not be performed.
  @param[in]  chunk   mem and mem_pfx fields of this chunk will be used to
                      locate the memory region to free */
  void deallocate_chunk(buf_chunk_t *chunk);

  /** Advices the OS that all chunks in this buffer pool instance can be dumped
  to a core file.
  Emits a warning to the log if could not succeed.
  @return true iff succeeded, false if no OS support or failed */
  bool madvise_dump();

  /** Advices the OS that all chunks in this buffer pool instance should not
  be dumped to a core file.
  Emits a warning to the log if could not succeed.
  @return true iff succeeded, false if no OS support or failed */
  bool madvise_dont_dump();

  /** Checks if the batch is running, which is basically equivalent to
  !os_event_is_set(no_flush[type]) if you hold flush_state_mutex.
  It is used as source of truth to know when to set or reset this event.
  Caller should hold flush_state_mutex.
  @param[in]  flush_type  The type of the flush we are interested in
  @return Should no_flush[type] be in the "unset" state? */
  bool is_flushing(buf_flush_t flush_type) const {
    ut_ad(mutex_own(&flush_state_mutex));
    return init_flush[flush_type] || 0 < n_flush[flush_type];
  }

#ifndef UNIV_HOTBACKUP
  /** Executes change() which modifies fields protected by flush_state_mutex.
  If it caused a change to is_flushing(flush_type) then it sets or resets the
  no_flush[flush_type] to keep it in sync.
  @param[in]  flush_type  The type of the flush this change of state concerns
  @param[in]  change      A callback to execute within flush_state_mutex
  */
  template <typename F>
  void change_flush_state(buf_flush_t flush_type, F &&change) {
    mutex_enter(&flush_state_mutex);
    const bool was_set = !is_flushing(flush_type);
    ut_ad(was_set == os_event_is_set(no_flush[flush_type]));
    std::forward<F>(change)();
    const bool should_be_set = !is_flushing(flush_type);
    if (was_set && !should_be_set) {
      os_event_reset(no_flush[flush_type]);
    } else if (!was_set && should_be_set) {
      os_event_set(no_flush[flush_type]);
    }
    ut_ad(should_be_set == os_event_is_set(no_flush[flush_type]));
    mutex_exit(&flush_state_mutex);
  }
#endif /*! UNIV_HOTBACKUP */

  static_assert(BUF_BUDDY_LOW <= UNIV_ZIP_SIZE_MIN,
                "BUF_BUDDY_LOW > UNIV_ZIP_SIZE_MIN");
  /** @} */
};

/** Print the given buf_pool_t object.
@param[in,out]  out             the output stream
@param[in]      buf_pool        the buf_pool_t object to be printed
@return the output stream */
std::ostream &operator<<(std::ostream &out, const buf_pool_t &buf_pool);

/** @name Accessors for buffer pool mutexes
Use these instead of accessing buffer pool mutexes directly. */
/** @{ */

#ifndef UNIV_HOTBACKUP
/** Test if flush list mutex is owned. */
#define buf_flush_list_mutex_own(b) mutex_own(&(b)->flush_list_mutex)

/** Acquire the flush list mutex. */
#define buf_flush_list_mutex_enter(b)    \
  do {                                   \
    mutex_enter(&(b)->flush_list_mutex); \
  } while (0)
/** Release the flush list mutex. */
#define buf_flush_list_mutex_exit(b)    \
  do {                                  \
    mutex_exit(&(b)->flush_list_mutex); \
  } while (0)
/** Acquire the block->mutex. */
#define buf_page_mutex_enter(b) \
  do {                          \
    mutex_enter(&(b)->mutex);   \
  } while (0)

/** Release the block->mutex. */
#define buf_page_mutex_exit(b) \
  do {                         \
    (b)->mutex.exit();         \
  } while (0)

/** Get appropriate page_hash_lock. */
inline rw_lock_t *buf_page_hash_lock_get(const buf_pool_t *buf_pool,
                                         const page_id_t page_id) {
  return hash_get_lock(buf_pool->page_hash, page_id.hash());
}

/** If not appropriate page_hash_lock, relock until appropriate. */
inline rw_lock_t *buf_page_hash_lock_s_confirm(rw_lock_t *hash_lock,
                                               const buf_pool_t *buf_pool,
                                               const page_id_t page_id) {
  return hash_lock_s_confirm(hash_lock, buf_pool->page_hash, page_id.hash());
}

inline rw_lock_t *buf_page_hash_lock_x_confirm(rw_lock_t *hash_lock,
                                               buf_pool_t *buf_pool,
                                               const page_id_t &page_id) {
  return hash_lock_x_confirm(hash_lock, buf_pool->page_hash, page_id.hash());
}
#endif /* !UNIV_HOTBACKUP */

#if defined(UNIV_DEBUG) && !defined(UNIV_HOTBACKUP)
/** Test if page_hash lock is held in s-mode. */
inline ulint buf_page_hash_lock_held_s(const buf_pool_t *buf_pool,
                                       const buf_page_t *bpage) {
  return rw_lock_own(buf_page_hash_lock_get(buf_pool, bpage->id), RW_LOCK_S);
}

/** Test if page_hash lock is held in x-mode. */
inline ulint buf_page_hash_lock_held_x(const buf_pool_t *buf_pool,
                                       const buf_page_t *bpage) {
  return rw_lock_own(buf_page_hash_lock_get((buf_pool), (bpage)->id),
                     RW_LOCK_X);
}

/** Test if page_hash lock is held in x or s-mode. */
inline bool buf_page_hash_lock_held_s_or_x(const buf_pool_t *buf_pool,
                                           const buf_page_t *bpage) {
  return buf_page_hash_lock_held_s(buf_pool, bpage) ||
         buf_page_hash_lock_held_x(buf_pool, bpage);
}

inline bool buf_block_hash_lock_held_s(const buf_pool_t *buf_pool,
                                       const buf_block_t *block) {
  return buf_page_hash_lock_held_s(buf_pool, &block->page);
}

inline bool buf_block_hash_lock_held_x(const buf_pool_t *buf_pool,
                                       const buf_block_t *block) {
  return buf_page_hash_lock_held_x(buf_pool, &block->page);
}

inline bool buf_block_hash_lock_held_s_or_x(const buf_pool_t *buf_pool,
                                            const buf_block_t *block) {
  return buf_page_hash_lock_held_s_or_x(buf_pool, &block->page);
}
#else /* UNIV_DEBUG && !UNIV_HOTBACKUP */
#define buf_page_hash_lock_held_s(b, p) (true)
#define buf_page_hash_lock_held_x(b, p) (true)
#define buf_page_hash_lock_held_s_or_x(b, p) (true)
#define buf_block_hash_lock_held_s(b, p) (true)
#define buf_block_hash_lock_held_x(b, p) (true)
#define buf_block_hash_lock_held_s_or_x(b, p) (true)
#endif /* UNIV_DEBUG && !UNIV_HOTBACKUP */

/** @} */

/**********************************************************************
Let us list the consistency conditions for different control block states.

NOT_USED:       is in free list, not in LRU list, not in flush list, nor
                page hash table
READY_FOR_USE:  is not in free list, LRU list, or flush list, nor page
                hash table
MEMORY:         is not in free list, LRU list, or flush list, nor page
                hash table
FILE_PAGE:      space and offset are defined, is in page hash table
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
FILE_PAGE => NOT_USED   NOTE: This transition is allowed if and only if
                                (1) buf_fix_count == 0,
                                (2) oldest_modification == 0, and
                                (3) io_fix == 0.
*/

#if defined UNIV_DEBUG || defined UNIV_BUF_DEBUG
#ifndef UNIV_HOTBACKUP
/** Functor to validate the LRU list. */
struct CheckInLRUList {
  void operator()(const buf_page_t *elem) const { ut_a(elem->in_LRU_list); }

  static void validate(const buf_pool_t *buf_pool) {
    CheckInLRUList check;
    ut_list_validate(buf_pool->LRU, check);
  }
};

/** Functor to validate the LRU list. */
struct CheckInFreeList {
  void operator()(const buf_page_t *elem) const { ut_a(elem->in_free_list); }

  static void validate(const buf_pool_t *buf_pool) {
    CheckInFreeList check;
    ut_list_validate(buf_pool->free, check);
  }
};

struct CheckUnzipLRUAndLRUList {
  void operator()(const buf_block_t *elem) const {
    ut_a(elem->page.in_LRU_list);
    ut_a(elem->in_unzip_LRU_list);
  }

  static void validate(const buf_pool_t *buf_pool) {
    CheckUnzipLRUAndLRUList check;
    ut_list_validate(buf_pool->unzip_LRU, check);
  }
};
#endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_DEBUG || defined UNIV_BUF_DEBUG */

#ifndef UNIV_HOTBACKUP
/** Prepare a page before adding to the free list.
@param[in,out] bpage            Buffer page to prepare for freeing. */
inline void buf_page_prepare_for_free(buf_page_t *bpage) noexcept {
  bpage->reset_page_id();
}
#endif /* !UNIV_HOTBACKUP */

/** Gets the compressed page descriptor corresponding to an uncompressed
page if applicable.
@param[in] block                Get the zip descriptor for this block. */
inline page_zip_des_t *buf_block_get_page_zip(buf_block_t *block) noexcept {
  return block->get_page_zip();
}

/** Gets the compressed page descriptor corresponding to an uncompressed
page if applicable. Const version.
@param[in] block                Get the zip descriptor for this block.
@return page descriptor or nullptr. */
inline const page_zip_des_t *buf_block_get_page_zip(
    const buf_block_t *block) noexcept {
  return block->get_page_zip();
}

inline bool buf_page_in_memory(const buf_page_t *bpage) {
  switch (buf_page_get_state(bpage)) {
    case BUF_BLOCK_MEMORY:
      return true;
    default:
      break;
  }
  return false;
}

/** Verify the page contained by the block. If there is page type
mismatch then reset it to expected page type. Data files created
before MySQL 5.7 GA may contain garbage in the FIL_PAGE_TYPE field.
@param[in,out]  block       block that may possibly have invalid
                            FIL_PAGE_TYPE
@param[in]      type        Expected page type
@param[in,out]  mtr         Mini-transaction */
inline void buf_block_reset_page_type_on_mismatch(buf_block_t &block,
                                                  page_type_t type,
                                                  mtr_t &mtr) {
  byte *page = block.frame;
  page_type_t page_type = fil_page_get_type(page);
  if (page_type != type) {
    const page_id_t &page_id = block.page.id;
    fil_page_reset_type(page_id, page, type, &mtr);
  }
}

#include "buf0buf.ic"

#endif /* !buf0buf_h */
