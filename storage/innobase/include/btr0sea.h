/*****************************************************************************

Copyright (c) 1996, 2022, Oracle and/or its affiliates.

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

/** @file include/btr0sea.h
 The index tree adaptive search

 Created 2/17/1996 Heikki Tuuri
 *************************************************************************/

#ifndef btr0sea_h
#define btr0sea_h

#include "univ.i"

#include "btr0types.h"
#include "dict0dict.h"
#include "ha0ha.h"
#include "mtr0mtr.h"
#include "rem0rec.h"

/** Creates and initializes the adaptive search system at a database start.
@param[in]      hash_size       hash table size. */
void btr_search_sys_create(ulint hash_size);

/** Resize hash index hash table.
@param[in]      hash_size       hash index hash table size */
void btr_search_sys_resize(ulint hash_size);

/** Frees the adaptive search system at a database shutdown. */
void btr_search_sys_free();

/** Disable the adaptive hash search system and empty the index.
@param[in]      need_mutex      Need to acquire dict_sys->mutex */
void btr_search_disable(bool need_mutex);
/** Enable the adaptive hash search system. */
void btr_search_enable();

/** Returns search info for an index.
 @return search info; search mutex reserved */
static inline btr_search_t *btr_search_get_info(
    dict_index_t *index); /*!< in: index */

/** Creates and initializes a search info struct.
@param[in]      heap            heap where created.
@return own: search info struct */
btr_search_t *btr_search_info_create(mem_heap_t *heap);

/** Returns the value of ref_count.
@param[in]      info            search info
@return ref_count value. */
size_t btr_search_info_get_ref_count(const btr_search_t *info);

/** Updates the search info statistics following a search in B-tree that was
performed not using or not finding row with the AHI index. It may do nothing or
decide to try to update the searched record on which the supplied cursor in
positioned at, or add the whole page to AHI.
@param[in]      index   index of the cursor
@param[in]      cursor  cursor which was just positioned */
static inline void btr_search_info_update(dict_index_t *index,
                                          btr_cur_t *cursor);

/** Tries to guess the right search position based on the hash search info
of the index. Note that if mode is PAGE_CUR_LE, which is used in inserts,
and the function returns true, then cursor->up_match and cursor->low_match
both have sensible values.
@param[in,out]  index           Index
@param[in,out]  info            Index search info
@param[in]      tuple           Logical record
@param[in]      mode            PAGE_CUR_L, ....
@param[in]      latch_mode      BTR_SEARCH_LEAF, ...;
                                NOTE that only if has_search_latch is 0, we will
                                have a latch set on the cursor page, otherwise
                                we assume the caller uses his search latch
                                to protect the record!
@param[out]     cursor          Tree cursor
@param[in]      has_search_latch
                                Latch mode the caller currently has on
                                search system: RW_S/X_LATCH or 0
@param[in]      mtr             Mini-transaction
@return true if succeeded */
bool btr_search_guess_on_hash(dict_index_t *index, btr_search_t *info,
                              const dtuple_t *tuple, ulint mode,
                              ulint latch_mode, btr_cur_t *cursor,
                              ulint has_search_latch, mtr_t *mtr);

/** Moves or deletes hash entries for moved records. If new_page is already
hashed, then the hash index for page, if any, is dropped. If new_page is not
hashed, and page is hashed, then a new hash index is built to new_page with the
same parameters as page (this often happens when a page is split).
@param[in,out]  new_block       records are copied to this page.
@param[in,out]  block           index page from which record are copied, and the
                                copied records will be deleted from this page.
@param[in,out]  index           record descriptor */
void btr_search_move_or_delete_hash_entries(buf_block_t *new_block,
                                            buf_block_t *block,
                                            dict_index_t *index);

/** Drop any adaptive hash index entries that point to an index page.
@param[in,out]  block   block containing index page, s- or x-latched, or an
                        index page for which we know that
                        block->buf_fix_count == 0 or it is an index page which
                        has already been removed from the buf_pool->page_hash
                        i.e.: it is in state BUF_BLOCK_REMOVE_HASH */
void btr_search_drop_page_hash_index(buf_block_t *block);

/** Drop any adaptive hash index entries that may point to an index
page that may be in the buffer pool, when a page is evicted from the
buffer pool or freed in a file segment.
@param[in]      page_id         page id
@param[in]      page_size       page size */
void btr_search_drop_page_hash_when_freed(const page_id_t &page_id,
                                          const page_size_t &page_size);

/** Drop any adaptive hash index entries for a table.
@param[in,out]  table   to drop indexes of this table */
void btr_drop_ahi_for_table(dict_table_t *table);

/** Drop any adaptive hash index entries for a index.
@param[in,out]  index   to drop hash indexes for this index */
void btr_drop_ahi_for_index(const dict_index_t *index);

/** Updates the page hash index when a single record is inserted on a page.
@param[in]      cursor  cursor which was positioned to the place to insert using
                        btr_cur_search_, and the new record has been inserted
                        next to the cursor. */
void btr_search_update_hash_node_on_insert(btr_cur_t *cursor);

/** Updates the page hash index when a single record is inserted on a page.
@param[in,out]  cursor  cursor which was positioned to the place to insert using
                        btr_cur_search_..., and the new record has been inserted
                        next to the cursor. */
void btr_search_update_hash_on_insert(btr_cur_t *cursor);

/** Updates the page hash index when a single record is deleted from a page.
@param[in]      cursor  cursor which was positioned on the record to delete
using btr_cur_search_, the record is not yet deleted. */
void btr_search_update_hash_on_delete(btr_cur_t *cursor);

/** Validates the search system.
@return true if ok */
bool btr_search_validate();

/** X-Lock the search latch (corresponding to given index)
@param[in] index        index handler
@param[in] location     source location */
static inline void btr_search_x_lock(const dict_index_t *index,
                                     ut::Location location);

/** X-Lock the search latch (corresponding to given index), does not block.
@param[in]      index           index handler
@param[in]      location        source location
@return true if the latch could was acquired.*/
[[nodiscard]] static inline bool btr_search_x_lock_nowait(
    const dict_index_t *index, ut::Location location);

/** X-Unlock the search latch (corresponding to given index)
@param[in]      index   index handler */
static inline void btr_search_x_unlock(const dict_index_t *index);

/** Lock all search latches in exclusive mode.
@param[in] location source location */
static inline void btr_search_x_lock_all(ut::Location location);

/** Unlock all search latches from exclusive mode. */
static inline void btr_search_x_unlock_all();

/** S-Lock the search latch (corresponding to given index)
@param[in] index        index handler
@param[in] location     source location */
static inline void btr_search_s_lock(const dict_index_t *index,
                                     ut::Location location);

/** S-Lock the search latch (corresponding to given index), does not block.
@param[in]      index           index handler
@param[in]      location        source location
@return true if the latch could was acquired.*/
[[nodiscard]] static inline bool btr_search_s_lock_nowait(
    const dict_index_t *index, ut::Location location);

/** S-Unlock the search latch (corresponding to given index)
@param[in]      index   index handler */
static inline void btr_search_s_unlock(const dict_index_t *index);

/** Lock all search latches in shared mode.
@param[in]      location        source location */
static inline void btr_search_s_lock_all(ut::Location location);

#ifdef UNIV_DEBUG
/** Check if thread owns all the search latches.
@param[in]      mode    lock mode check
@retval true if owns all of them
@retval false if does not own some of them */
[[nodiscard]] static inline bool btr_search_own_all(ulint mode);

/** Check if thread owns any of the search latches.
@param[in]      mode    lock mode check
@retval true if owns any of them
@retval false if owns no search latch */
[[nodiscard]] static inline bool btr_search_own_any(ulint mode);
#endif /* UNIV_DEBUG */

/** Unlock all search latches from shared mode. */
static inline void btr_search_s_unlock_all();

/** Get the adaptive hash search index slot ID for a b-tree specified by its IDs
of index and space.
@param[in] index_id Index of the b-tree index
@param[in] space_id Index of the tablespace the index is in.
@return Index of the slot for btr_search_sys->hash_tables and btr_search_latches
arrays. */
static inline size_t btr_get_search_slot(const space_index_t index_id,
                                         const space_id_t space_id);

/** Get the latch based on index attributes.
A latch is selected from an array of latches using pair of index-id, space-id.
@param[in]      index   index handler
@return latch */
static inline rw_lock_t *btr_get_search_latch(const dict_index_t *index);

/** Get the hash-table based on index attributes.
A table is selected from an array of tables using pair of index-id, space-id.
@param[in]      index   index handler
@return hash table */
static inline hash_table_t *btr_get_search_table(const dict_index_t *index);

/** The search info struct in an index */
struct btr_search_t {
  /** Number of blocks in this index tree that have search index built i.e.
  block->index points to this index. */
  std::atomic<ulint> ref_count;

  /** @{ The following fields are not protected by any latch.
  Unfortunately, this means that they must be aligned to the machine word, i.e.,
  they cannot be turned into bit-fields. */

  /** the root page frame when it was last time fetched, or NULL. */
  buf_block_t *root_guess;
  /** when this exceeds BTR_SEARCH_HASH_ANALYSIS, the hash analysis starts; this
  is reset if no success noticed. */
  ulint hash_analysis;
  /** true if the last search would have succeeded, or did succeed, using the
  hash index; NOTE that the value here is not exact: it is not calculated for
  every search, and the calculation itself is not always accurate! */
  bool last_hash_succ;
  /** number of consecutive searches which would have succeeded, or did succeed,
  using the hash index; the range is 0 .. BTR_SEARCH_BUILD_LIMIT + 5. */
  ulint n_hash_potential;
  /** @} */

  /**---------------------- @{ */
  /** recommended prefix length for hash search: number of full fields */
  ulint n_fields;
  /** recommended prefix: number of bytes in an incomplete field
  @see BTR_PAGE_MAX_REC_SIZE */
  ulint n_bytes;
  /** true or false, depending on whether the leftmost record of several records
  with the same prefix should be indexed in the hash index */
  bool left_side;
  /*---------------------- @} */
#ifdef UNIV_SEARCH_PERF_STAT
  /** number of successful hash searches so far. */
  std::atomic<ulint> n_hash_succ;
  /** number of failed hash searches */
  std::atomic<ulint> n_hash_fail;
  /** number of successful pattern searches thus far */
  std::atomic<ulint> n_patt_succ;
  /** number of searches */
  std::atomic<ulint> n_searches;
#endif /* UNIV_SEARCH_PERF_STAT */
#ifdef UNIV_DEBUG
  /** magic number @see BTR_SEARCH_MAGIC_N */
  ulint magic_n;
#endif /* UNIV_DEBUG */
};

#ifdef UNIV_DEBUG
/** value of btr_search_t::magic_n, used in assertions */
constexpr uint32_t BTR_SEARCH_MAGIC_N = 1112765;
#endif /* UNIV_DEBUG */

/** The hash index system */
struct btr_search_sys_t {
  /** the adaptive hash tables, mapping dtuple_hash values to rec_t pointers on
  index pages */
  hash_table_t **hash_tables;
};

/** Latches protecting access to adaptive hash index. */
extern rw_lock_t **btr_search_latches;

/** The adaptive hash index */
extern btr_search_sys_t *btr_search_sys;

#ifdef UNIV_SEARCH_PERF_STAT
/** Number of successful adaptive hash index lookups */
extern ulint btr_search_n_succ;
/** Number of failed adaptive hash index lookups */
extern ulint btr_search_n_hash_fail;
#endif /* UNIV_SEARCH_PERF_STAT */

/** After change in n_fields or n_bytes in info, this many rounds are waited
before starting the hash analysis again: this is to save CPU time when there
is no hope in building a hash index. */
constexpr uint32_t BTR_SEARCH_HASH_ANALYSIS = 17;

/** Limit of consecutive searches for trying a search shortcut on the search
pattern */
constexpr uint32_t BTR_SEARCH_ON_PATTERN_LIMIT = 3;

/** Limit of consecutive searches for trying a search shortcut using
the hash index */
constexpr uint32_t BTR_SEARCH_ON_HASH_LIMIT = 3;

#include "btr0sea.ic"

#endif
