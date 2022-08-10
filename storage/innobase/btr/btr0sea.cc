/*****************************************************************************

Copyright (c) 1996, 2022, Oracle and/or its affiliates.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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

/** @file btr/btr0sea.cc
 The index tree adaptive search

 Created 2/17/1996 Heikki Tuuri
 *************************************************************************/

#include "btr0sea.h"

#include <sys/types.h>

#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0pcur.h"
#include "buf0buf.h"
#include "ha0ha.h"

#include "page0cur.h"
#include "page0page.h"
#include "srv0mon.h"
#include "sync0sync.h"

#include <scope_guard.h>

/** Is search system enabled.
Search system is protected by array of latches. */
bool btr_search_enabled = true;

/** Number of adaptive hash index partition. */
ulong btr_ahi_parts = 8;
ut::fast_modulo_t btr_ahi_parts_fast_modulo(8);

#ifdef UNIV_SEARCH_PERF_STAT
/** Number of successful adaptive hash index lookups */
ulint btr_search_n_succ = 0;
/** Number of failed adaptive hash index lookups */
ulint btr_search_n_hash_fail = 0;
#endif /* UNIV_SEARCH_PERF_STAT */

/** padding to prevent other memory update
hotspots from residing on the same memory
cache line as btr_search_latches */
byte btr_sea_pad1[64];

/** The latches protecting the adaptive search system: this latches protects the
(1) positions of records on those pages where a hash index has been built.
NOTE: It does not protect values of non-ordering fields within a record from
being updated in-place! We can use fact (1) to perform unique searches to
indexes. We will allocate the latches from dynamic memory to get it to the
same DRAM page as other hotspot semaphores */
rw_lock_t **btr_search_latches;

/** padding to prevent other memory update hotspots from residing on
the same memory cache line */
byte btr_sea_pad2[64];

/** The adaptive hash index */
btr_search_sys_t *btr_search_sys;

/** If the number of records on the page divided by this parameter
would have been successfully accessed using a hash index, the index
is then built on the page, assuming the global limit has been reached */
constexpr uint32_t BTR_SEARCH_PAGE_BUILD_LIMIT = 16;

/** The global limit for consecutive potentially successful hash searches,
before hash index building is started */
constexpr uint32_t BTR_SEARCH_BUILD_LIMIT = 100;

/** Compute the hash value of an index identifier.
@param[in]	index	Pointer to index descriptor.
@return hash value */
static uint64_t btr_search_hash_index_id(const dict_index_t *index) {
  return ut::hash_uint64_pair(index->id, index->space);
}

/** Determine the number of accessed key fields.
@param[in]      n_fields        number of complete fields
@param[in]      n_bytes         number of bytes in an incomplete last field
@return number of complete or incomplete fields */
[[nodiscard]] inline ulint btr_search_get_n_fields(ulint n_fields,
                                                   ulint n_bytes) {
  return (n_fields + (n_bytes > 0 ? 1 : 0));
}

/** Determine the number of accessed key fields.
@param[in]      cursor          b-tree cursor
@return number of complete or incomplete fields */
[[nodiscard]] inline ulint btr_search_get_n_fields(const btr_cur_t *cursor) {
  return (btr_search_get_n_fields(cursor->n_fields, cursor->n_bytes));
}

/** Builds a hash index on a page with the block's recommended parameters. If
the page already has a hash index with different parameters, the old hash index
is removed. This function checks if n_fields and n_bytes are sensible, and does
not build a hash index if not.
@param[in,out]  index   index for which to build
@param[in,out]  block   index page, s-/x- latched.
@param[in]      update  specifies if the page should be only added to index
                        (false) or possibly updated if any hash entries are
                        already added for the records this page has (true) */
static void btr_search_build_page_hash_index(dict_index_t *index,
                                             buf_block_t *block, bool update);

/** Checks that there is a free buffer frame allocated for hash table heap in
the btr search system. If not, allocates a free frame for the heap. This
function should be called before reserving any btr search mutex, if the intended
operation might add nodes to the search system hash table. The heap frame will
allow to do some insertions to the AHI hash table, but does not guarantee
anything, i.e. there may be a space in frame only for a part of the nodes to
insert or some other concurrent operation on AHI could consume the frame's
memory before we latch the AHI.
@param[in] index    index handler */
static inline void btr_search_check_free_space_in_heap(dict_index_t *index) {
  hash_table_t *table;
  mem_heap_t *heap;

  if (!btr_search_enabled) {
    return;
  }
  ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_S));
  ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));

  table = btr_get_search_table(index);

  heap = table->heap;

  /* We can't do this check and alloc a block from Buffer pool only when needed
  while inserting new nodes to AHI hash table, as in case the eviction is needed
  to free up a block from LRU, the AHI latches may be required to complete the
  page eviction. The execution can reach the following path: buf_block_alloc ->
  buf_LRU_get_free_block -> buf_LRU_scan_and_free_block ->
  buf_LRU_free_from_common_LRU_list -> buf_LRU_free_page ->
  btr_search_drop_page_hash_index */
  if (heap->free_block == nullptr) {
    const auto block = buf_block_alloc(nullptr);
    void *expected = nullptr;
    ut_ad(block != nullptr);
    if (!heap->free_block.compare_exchange_strong(expected, block)) {
      /* Someone must have set the free_block in meantime, return the allocated
      block to pool. */
      buf_block_free(block);
    }
  }
}

void btr_search_sys_create(ulint hash_size) {
  /* Search System is divided into n parts.
  Each part controls access to distinct set of hash cells from hash table
  through its own latch. */

  /* Step-1: Allocate latches (1 per part). */
  btr_search_latches = reinterpret_cast<rw_lock_t **>(
      ut::malloc_withkey(ut::make_psi_memory_key(mem_key_ahi),
                         sizeof(rw_lock_t *) * btr_ahi_parts));
  /* It is written only from one thread during server initialization, so it is
  safe. */
  btr_ahi_parts_fast_modulo = ut::fast_modulo_t{btr_ahi_parts};

  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    btr_search_latches[i] = reinterpret_cast<rw_lock_t *>(ut::malloc_withkey(
        ut::make_psi_memory_key(mem_key_ahi), sizeof(rw_lock_t)));

    rw_lock_create(btr_search_latch_key, btr_search_latches[i],
                   SYNC_SEARCH_SYS);
  }

  /* Step-2: Allocate hash tables. */
  btr_search_sys = reinterpret_cast<btr_search_sys_t *>(ut::malloc_withkey(
      ut::make_psi_memory_key(mem_key_ahi), sizeof(btr_search_sys_t)));

  btr_search_sys->hash_tables = reinterpret_cast<hash_table_t **>(
      ut::malloc_withkey(ut::make_psi_memory_key(mem_key_ahi),
                         sizeof(hash_table_t *) * btr_ahi_parts));

  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    btr_search_sys->hash_tables[i] =
        ib_create((hash_size / btr_ahi_parts), LATCH_ID_HASH_TABLE_MUTEX, 0,
                  MEM_HEAP_FOR_BTR_SEARCH);

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
    btr_search_sys->hash_tables[i]->adaptive = true;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
  }
}

void btr_search_sys_resize(ulint hash_size) {
  /* Step-1: Lock all search latches in exclusive mode. */
  btr_search_x_lock_all(UT_LOCATION_HERE);

  if (btr_search_enabled) {
    btr_search_x_unlock_all();

    ib::error(ER_IB_MSG_45) << "btr_search_sys_resize failed because"
                               " hash index hash table is not empty.";
    ut_d(ut_error);
    ut_o(return );
  }

  /* Step-2: Recreate hash tables with new size. */
  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    mem_heap_free(btr_search_sys->hash_tables[i]->heap);
    ut::delete_(btr_search_sys->hash_tables[i]);

    btr_search_sys->hash_tables[i] =
        ib_create((hash_size / btr_ahi_parts), LATCH_ID_HASH_TABLE_MUTEX, 0,
                  MEM_HEAP_FOR_BTR_SEARCH);

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
    btr_search_sys->hash_tables[i]->adaptive = true;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
  }

  /* Step-3: Unlock all search latches from exclusive mode. */
  btr_search_x_unlock_all();
}

void btr_search_sys_free() {
  if (btr_search_sys == nullptr) {
    ut_ad(btr_search_latches == nullptr);
    return;
  }

  ut_ad(btr_search_latches != nullptr);

  /* Step-1: Release the hash tables. */
  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    mem_heap_free(btr_search_sys->hash_tables[i]->heap);
    ut::delete_(btr_search_sys->hash_tables[i]);
  }

  ut::free(btr_search_sys->hash_tables);
  ut::free(btr_search_sys);
  btr_search_sys = nullptr;

  /* Step-2: Release all allocates latches. */
  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    rw_lock_free(btr_search_latches[i]);
    ut::free(btr_search_latches[i]);
  }

  ut::free(btr_search_latches);
  btr_search_latches = nullptr;
}

/** Set index->ref_count = 0 on all indexes of a table.
@param[in,out]  table   table handler */
static void btr_search_disable_ref_count(dict_table_t *table) {
  dict_index_t *index;

  ut_ad(dict_sys_mutex_own());

  for (index = table->first_index(); index != nullptr; index = index->next()) {
    ut_ad(rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));

    index->search_info->ref_count = 0;
  }
}

void btr_search_disable(bool need_mutex) {
  if (need_mutex) {
    dict_sys_mutex_enter();
  }

  ut_ad(dict_sys_mutex_own());
  btr_search_x_lock_all(UT_LOCATION_HERE);

  if (!btr_search_enabled) {
    if (need_mutex) {
      dict_sys_mutex_exit();
    }

    btr_search_x_unlock_all();
    return;
  }

  btr_search_enabled = false;

  /* Clear the index->search_info->ref_count of every index in
  the data dictionary cache. */
  for (auto table : dict_sys->table_LRU) {
    btr_search_disable_ref_count(table);
  }

  for (auto table : dict_sys->table_non_LRU) {
    btr_search_disable_ref_count(table);
  }

  if (need_mutex) {
    dict_sys_mutex_exit();
  }

  /* Set all block->index = NULL. */
  buf_pool_clear_hash_index();

  /* Clear the adaptive hash index. */
  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    hash_table_clear(btr_search_sys->hash_tables[i]);
    mem_heap_empty(btr_search_sys->hash_tables[i]->heap);
  }

  btr_search_x_unlock_all();
}

void btr_search_enable() {
  os_rmb;
  /* Don't allow enabling AHI if buffer pool resize is happening.
  Ignore it silently.  */
  if (srv_buf_pool_old_size != srv_buf_pool_size) return;

  btr_search_x_lock_all(UT_LOCATION_HERE);
  btr_search_enabled = true;
  btr_search_x_unlock_all();
}

btr_search_t *btr_search_info_create(mem_heap_t *heap) {
  btr_search_t *info;

  info = (btr_search_t *)mem_heap_alloc(heap, sizeof(btr_search_t));

  ut_d(info->magic_n = BTR_SEARCH_MAGIC_N);

  info->ref_count = 0;
  info->root_guess = nullptr;

  info->hash_analysis = 0;
  info->n_hash_potential = 0;

  info->last_hash_succ = false;

#ifdef UNIV_SEARCH_PERF_STAT
  info->n_hash_succ = 0;
  info->n_hash_fail = 0;
  info->n_patt_succ = 0;
  info->n_searches = 0;
#endif /* UNIV_SEARCH_PERF_STAT */

  /* Set some sensible values */
  info->n_fields = 1;
  info->n_bytes = 0;

  info->left_side = true;

  return (info);
}

size_t btr_search_info_get_ref_count(const btr_search_t *info) {
  if (!btr_search_enabled) {
    return 0;
  }

  ut_ad(info);

  return info->ref_count;
}

/** Updates the search info of an index about hash successes. NOTE that info
is NOT protected by any semaphore, to save CPU time! Do not assume its fields
are consistent.
@param[in,out]  info    search info
@param[in]      cursor  cursor which was just positioned */
static void btr_search_info_update_hash(btr_search_t *info, btr_cur_t *cursor) {
  dict_index_t *index = cursor->index;
  ulint n_unique;
  int cmp;

  ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_S));
  ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));

  if (dict_index_is_ibuf(index)) {
    /* So many deletes are performed on an insert buffer tree
    that we do not consider a hash index useful on it: */

    return;
  }

  n_unique = dict_index_get_n_unique_in_tree(index);

  if (info->n_hash_potential == 0) {
    goto set_new_recomm;
  }

  /* Test if the search would have succeeded using the recommended
  hash prefix */

  if (info->n_fields >= n_unique && cursor->up_match >= n_unique) {
  increment_potential:
    info->n_hash_potential++;

    return;
  }

  cmp = ut_pair_cmp(info->n_fields, info->n_bytes, cursor->low_match,
                    cursor->low_bytes);

  if (info->left_side ? cmp <= 0 : cmp > 0) {
    goto set_new_recomm;
  }

  cmp = ut_pair_cmp(info->n_fields, info->n_bytes, cursor->up_match,
                    cursor->up_bytes);

  if (info->left_side ? cmp <= 0 : cmp > 0) {
    goto increment_potential;
  }

set_new_recomm:
  /* We have to set a new recommendation; skip the hash analysis
  for a while to avoid unnecessary CPU time usage when there is no
  chance for success */

  info->hash_analysis = 0;

  cmp = ut_pair_cmp(cursor->up_match, cursor->up_bytes, cursor->low_match,
                    cursor->low_bytes);
  if (cmp == 0) {
    info->n_hash_potential = 0;

    /* For extra safety, we set some sensible values here */

    info->n_fields = 1;
    info->n_bytes = 0;

    info->left_side = true;

  } else if (cmp > 0) {
    info->n_hash_potential = 1;

    if (cursor->up_match >= n_unique) {
      info->n_fields = n_unique;
      info->n_bytes = 0;

    } else if (cursor->low_match < cursor->up_match) {
      info->n_fields = cursor->low_match + 1;
      info->n_bytes = 0;
    } else {
      info->n_fields = cursor->low_match;
      info->n_bytes = cursor->low_bytes + 1;
    }

    info->left_side = true;
  } else {
    info->n_hash_potential = 1;

    if (cursor->low_match >= n_unique) {
      info->n_fields = n_unique;
      info->n_bytes = 0;
    } else if (cursor->low_match > cursor->up_match) {
      info->n_fields = cursor->up_match + 1;
      info->n_bytes = 0;
    } else {
      info->n_fields = cursor->up_match;
      info->n_bytes = cursor->up_bytes + 1;
    }

    info->left_side = false;
  }
}

/** Update the block search info on hash successes. NOTE that info and
block->n_hash_helps, n_fields, n_bytes, left_side are NOT protected by any
semaphore, to save CPU time! Do not assume the fields are consistent.
@return true if building a (new) hash index on the block is recommended
@param[in,out]  info    search info
@param[in,out]  block   buffer block
@param[in]      cursor  cursor */
static bool btr_search_update_block_hash_info(btr_search_t *info,
                                              buf_block_t *block,
                                              const btr_cur_t *cursor) {
  ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_S));
  ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_X));
  ut_ad(rw_lock_own(&block->lock, RW_LOCK_S) ||
        rw_lock_own(&block->lock, RW_LOCK_X));

  info->last_hash_succ = false;

  ut_a(buf_block_state_valid(block));
  ut_ad(info->magic_n == BTR_SEARCH_MAGIC_N);

  if ((block->n_hash_helps > 0) && (info->n_hash_potential > 0) &&
      (block->n_fields == info->n_fields) &&
      (block->n_bytes == info->n_bytes) &&
      (block->left_side == info->left_side)) {
    if ((block->index) && (block->curr_n_fields == info->n_fields) &&
        (block->curr_n_bytes == info->n_bytes) &&
        (block->curr_left_side == info->left_side)) {
      /* The search would presumably have succeeded using
      the hash index */

      info->last_hash_succ = true;
    }

    block->n_hash_helps++;
  } else {
    block->n_hash_helps = 1;
    block->n_fields = info->n_fields;
    block->n_bytes = info->n_bytes;
    block->left_side = info->left_side;
  }

#ifdef UNIV_DEBUG
  if (cursor->index->table->does_not_fit_in_memory) {
    block->n_hash_helps = 0;
  }
#endif /* UNIV_DEBUG */

  if (info->n_hash_potential >= BTR_SEARCH_BUILD_LIMIT &&
      block->n_hash_helps >
          page_get_n_recs(block->frame) / BTR_SEARCH_PAGE_BUILD_LIMIT) {
    if ((!block->index) ||
        (block->n_hash_helps > 2 * page_get_n_recs(block->frame)) ||
        (block->n_fields != block->curr_n_fields) ||
        (block->n_bytes != block->curr_n_bytes) ||
        (block->left_side != block->curr_left_side)) {
      /* Build a new hash index on the page */

      return true;
    }
  }

  return false;
}

/** Updates a hash node reference when it has been unsuccessfully used in a
search which could have succeeded with the used hash parameters. This can
happen because when building a hash index for a page, we do not check
what happens at page boundaries, and therefore there can be misleading
hash nodes. Also, collisions in the hash value can lead to misleading
references. This function lazily fixes these imperfections in the hash
index.
@param[in]      info    search info
@param[in]      block   buffer block where cursor positioned
@param[in]      cursor  cursor */
static void btr_search_update_hash_ref(const btr_search_t *info,
                                       buf_block_t *block,
                                       const btr_cur_t *cursor) {
  ut_ad(cursor->flag == BTR_CUR_HASH_FAIL);
  ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_X));
  ut_ad(rw_lock_own(&(block->lock), RW_LOCK_S) ||
        rw_lock_own(&(block->lock), RW_LOCK_X));
  ut_ad(page_align(btr_cur_get_rec(cursor)) == buf_block_get_frame(block));
  assert_block_ahi_valid(block);

  const auto index = block->index;

  if (!index) {
    return;
  }

  ut_ad(block->page.id.space() == index->space);
  ut_a(index == cursor->index);
  ut_a(!dict_index_is_ibuf(index));

  auto is_current_info_indexed = [block, info] {
    return (info->n_hash_potential > 0) &&
           (block->curr_n_fields == info->n_fields) &&
           (block->curr_n_bytes == info->n_bytes) &&
           (block->curr_left_side == info->left_side);
  };

  /* Dirty read without latch, will be repeated after we take the x-latch, which
  we take after we have the hash value ready to reduce time consumed under
  the latch. */
  if (is_current_info_indexed()) {
    const auto rec = btr_cur_get_rec(cursor);

    if (!page_rec_is_user_rec(rec)) {
      return;
    }

    const auto hash_value =
        rec_hash(rec, Rec_offsets{}.compute(rec, index), block->curr_n_fields,
                 block->curr_n_bytes, btr_search_hash_index_id(index), index);
    auto hash_table = btr_get_search_table(index);
    btr_search_check_free_space_in_heap(cursor->index);

    if (!btr_search_x_lock_nowait(cursor->index, UT_LOCATION_HERE)) {
      return;
    }
    if (is_current_info_indexed()) {
      ha_insert_for_hash(hash_table, hash_value, block, rec);
    }
    btr_search_x_unlock(cursor->index);
  }
}

void btr_search_info_update_slow(btr_search_t *info, btr_cur_t *cursor) {
  ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_S));
  ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_X));

  const auto block = btr_cur_get_block(cursor);

  /* NOTE that the following two function calls do NOT protect
  info or block->n_fields etc. with any semaphore, to save CPU time!
  We cannot assume the fields are consistent when we return from
  those functions! */

  btr_search_info_update_hash(info, cursor);

#ifdef UNIV_SEARCH_PERF_STAT
  if (cursor->flag == BTR_CUR_HASH_FAIL) {
    btr_search_n_hash_fail++;
  }
#endif /* UNIV_SEARCH_PERF_STAT */

  if (btr_search_update_block_hash_info(info, block, cursor)) {
    /* Note that since we did not protect block->n_fields etc.
    with any semaphore, the values can be inconsistent. We have
    to check inside the function call that they make sense. */
    btr_search_build_page_hash_index(cursor->index, block, false);
  } else if (cursor->flag == BTR_CUR_HASH_FAIL) {
    /* Update the hash node reference, if appropriate. If
    btr_search_update_block_hash_info decided to build the index for this
    block, the record should be hashed correctly with the rest of the block's
    records. */
    btr_search_update_hash_ref(info, block, cursor);
  }
}

/** Checks if a guessed position for a tree cursor is right. Note that if
mode is PAGE_CUR_LE, which is used in inserts, and the function returns
true, then cursor->up_match and cursor->low_match both have sensible values.
@param[in,out]  cursor  Guess cursor position
@param[in]      can_only_compare_to_cursor_rec
                        If we do not have a latch on the page of cursor, but a
                        latch corresponding search system, then ONLY the columns
                        of the record UNDER the cursor are protected, not the
                        next or previous record in the chain: we cannot look at
                        the next or previous record to check our guess!
@param[in]      tuple   Data tuple
@param[in]      mode    PAGE_CUR_L, PAGE_CUR_LE, PAGE_CUR_G, PAGE_CUR_GE
@param[in]      mtr     Mini-transaction
@return true if success */
static bool btr_search_check_guess(btr_cur_t *cursor,
                                   bool can_only_compare_to_cursor_rec,
                                   const dtuple_t *tuple, ulint mode,
                                   mtr_t *mtr) {
  rec_t *rec;
  ulint match;

  const auto n_unique = dict_index_get_n_unique_in_tree(cursor->index);

  rec = btr_cur_get_rec(cursor);

  ut_ad(page_rec_is_user_rec(rec));

  match = 0;

  Rec_offsets offsets;
  {
    const auto cmp =
        tuple->compare(rec, cursor->index,
                       offsets.compute(rec, cursor->index, n_unique), &match);

    if (mode == PAGE_CUR_GE) {
      if (cmp > 0) {
        return false;
      }

      cursor->up_match = match;

      if (match >= n_unique) {
        return true;
      }
    } else if (mode == PAGE_CUR_LE) {
      if (cmp < 0) {
        return false;
      }

      cursor->low_match = match;

    } else if (mode == PAGE_CUR_G) {
      if (cmp >= 0) {
        return false;
      }
    } else if (mode == PAGE_CUR_L) {
      if (cmp <= 0) {
        return false;
      }
    }

    if (can_only_compare_to_cursor_rec) {
      /* Since we could not determine if our guess is right just by
      looking at the record under the cursor, return false */
      return false;
    }
  }

  match = 0;

  if ((mode == PAGE_CUR_G) || (mode == PAGE_CUR_GE)) {
    rec_t *prev_rec;

    ut_ad(!page_rec_is_infimum(rec));

    prev_rec = page_rec_get_prev(rec);

    if (page_rec_is_infimum(prev_rec)) {
      return btr_page_get_prev(page_align(prev_rec), mtr) == FIL_NULL;
    }

    const auto cmp = tuple->compare(
        prev_rec, cursor->index,
        offsets.compute(prev_rec, cursor->index, n_unique), &match);
    bool success;
    if (mode == PAGE_CUR_GE) {
      success = cmp > 0;
    } else {
      success = cmp >= 0;
    }

    return success;
  } else {
    rec_t *next_rec;

    ut_ad(!page_rec_is_supremum(rec));

    next_rec = page_rec_get_next(rec);

    if (page_rec_is_supremum(next_rec)) {
      if (btr_page_get_next(page_align(next_rec), mtr) == FIL_NULL) {
        cursor->up_match = 0;
        return true;
      }

      return false;
    }

    const auto cmp = tuple->compare(
        next_rec, cursor->index,
        offsets.compute(next_rec, cursor->index, n_unique), &match);
    bool success;
    if (mode == PAGE_CUR_LE) {
      success = cmp < 0;
      cursor->up_match = match;
    } else {
      success = cmp <= 0;
    }
    return success;
  }
}

bool btr_search_guess_on_hash(dict_index_t *index, btr_search_t *info,
                              const dtuple_t *tuple, ulint mode,
                              ulint latch_mode, btr_cur_t *cursor,
                              ulint has_search_latch, mtr_t *mtr) {
  const rec_t *rec;
#ifdef notdefined
  btr_cur_t cursor2;
  btr_pcur_t pcur;
#endif

  if (!btr_search_enabled) {
    return false;
  }

  ut_ad(index && info && tuple && cursor && mtr);
  ut_ad(!dict_index_is_ibuf(index));
  ut_ad((latch_mode == BTR_SEARCH_LEAF) || (latch_mode == BTR_MODIFY_LEAF));

  /* Not supported for spatial index */
  ut_ad(!dict_index_is_spatial(index));

  /* If we decide to return before doing actual hash search, we will return with
  the following state of the cursor. */
  cursor->flag = BTR_CUR_HASH_NOT_ATTEMPTED;
  /* Note that, for efficiency, the struct info may not be protected by
   any latch here! */

  if (info->n_hash_potential == 0) {
    return false;
  }

  cursor->n_fields = info->n_fields;
  cursor->n_bytes = info->n_bytes;

  if (dtuple_get_n_fields(tuple) < btr_search_get_n_fields(cursor)) {
    return false;
  }

  const auto hash_value = dtuple_hash(tuple, cursor->n_fields, cursor->n_bytes,
                                      btr_search_hash_index_id(index));

  cursor->hash_value = hash_value;

  if (!has_search_latch) {
    if (!btr_search_s_lock_nowait(index, UT_LOCATION_HERE)) {
      return false;
    }
  }

  auto latch_guard =
      create_scope_guard([index]() { btr_search_s_unlock(index); });
  if (!has_search_latch) {
    if (!btr_search_enabled) {
      return false;
    }
  } else {
    /* If we had a latch, then the guard is not needed. */
    latch_guard.commit();
  }

  ut_ad(rw_lock_get_writer(btr_get_search_latch(index)) != RW_LOCK_X);
  ut_ad(rw_lock_get_reader_count(btr_get_search_latch(index)) > 0);

  rec =
      (rec_t *)ha_search_and_get_data(btr_get_search_table(index), hash_value);

  /* We did the hash search. If we decide to return before successfully
  verifying the search is correct, we will return with the following state of
  the cursor. */
  cursor->flag = BTR_CUR_HASH_FAIL;

#ifdef UNIV_SEARCH_PERF_STAT
  info->n_hash_fail++;
#endif /* UNIV_SEARCH_PERF_STAT */

  info->last_hash_succ = false;

  if (rec == nullptr) {
    return (false);
  }

  buf_block_t *block = buf_block_from_ahi(rec);

  if (!has_search_latch) {
    if (!buf_page_get_known_nowait(latch_mode, block, Cache_hint::MAKE_YOUNG,
                                   __FILE__, __LINE__, mtr)) {
      return false;
    }

    /* Release the AHI S-latch. */
    latch_guard.rollback();

    buf_block_dbg_add_level(block, SYNC_TREE_NODE_FROM_HASH);
  }

  if (buf_block_get_state(block) != BUF_BLOCK_FILE_PAGE) {
    ut_ad(buf_block_get_state(block) == BUF_BLOCK_REMOVE_HASH);

    if (!has_search_latch) {
      btr_leaf_page_release(block, latch_mode, mtr);
    }

    return false;
  }

  ut_ad(page_rec_is_user_rec(rec));

  btr_cur_position(index, (rec_t *)rec, block, cursor);

  /* Check the validity of the guess within the page */

  /* If we only have the latch on search system, not on the
  page, it only protects the columns of the record the cursor
  is positioned on. We cannot look at the next of the previous
  record to determine if our guess for the cursor position is
  right. */
  if (index->space != block->page.id.space() ||
      index->id != btr_page_get_index_id(block->frame) ||
      !btr_search_check_guess(cursor, has_search_latch, tuple, mode, mtr)) {
    if (!has_search_latch) {
      btr_leaf_page_release(block, latch_mode, mtr);
    }

    return false;
  }

  if (info->n_hash_potential < BTR_SEARCH_BUILD_LIMIT + 5) {
    info->n_hash_potential++;
  }

#ifdef notdefined
  /* These lines of code can be used in a debug version to check
  the correctness of the searched cursor position: */

  info->last_hash_succ = false;

  /* Currently, does not work if the following fails: */
  ut_ad(!has_search_latch);

  btr_leaf_page_release(block, latch_mode, mtr);

  btr_cur_search_to_nth_level(index, 0, tuple, mode, latch_mode, &cursor2, 0,
                              mtr);

  if (mode == PAGE_CUR_GE && page_rec_is_supremum(btr_cur_get_rec(&cursor2))) {
    /* If mode is PAGE_CUR_GE, then the binary search
    in the index tree may actually take us to the supremum
    of the previous page */

    info->last_hash_succ = false;

    pcur.open_on_user_rec(index, tuple, mode, latch_mode, mtr,
                          UT_LOCATION_HERE);

    ut_ad(pcur.get_rec() == btr_cur_get_rec(cursor));
  } else {
    ut_ad(btr_cur_get_rec(&cursor2) == btr_cur_get_rec(cursor));
  }

  /* NOTE that it is theoretically possible that the above assertions
  fail if the page of the cursor gets removed from the buffer pool
  meanwhile! Thus it might not be a bug. */
#endif

  info->last_hash_succ = true;
  cursor->flag = BTR_CUR_HASH;

#ifdef UNIV_SEARCH_PERF_STAT
  /* Revert the accounting we did for the hash search failure that was prepared
  above. */
  info->n_hash_fail--;

  info->n_hash_succ++;
  btr_search_n_succ++;
#endif
  if (!has_search_latch && buf_page_peek_if_too_old(&block->page)) {
    buf_page_make_young(&block->page);
  }

  /* Increment the page get statistics though we did not really
  fix the page: for user info only */

  {
    buf_pool_t *buf_pool = buf_pool_from_bpage(&block->page);

    Counter::inc(buf_pool->stat.m_n_page_gets, block->page.id.page_no());
  }

  return true;
}

void btr_search_drop_page_hash_index(buf_block_t *block) {
  const auto index_id = btr_page_get_index_id(block->frame);
  const auto ahi_slot = btr_get_search_slot(index_id, block->page.id.space());
  const auto latch = btr_search_latches[ahi_slot];
  ut::unique_ptr<uint64_t[]> hashes;
  size_t n_cached;
  byte *page;

  for (;;) {
    /* Do a dirty check on block->index, return if the block is
    not in the adaptive hash index. */
    const auto index = block->index;
    /* This debug check uses a dirty read that could theoretically cause
    false positives while buf_pool_clear_hash_index() is executing. */
    assert_block_ahi_valid(block);

    if (index == nullptr) {
      return;
    }

    ut_ad(block->page.buf_fix_count == 0 ||
          buf_block_get_state(block) == BUF_BLOCK_REMOVE_HASH ||
          rw_lock_own(&block->lock, RW_LOCK_S) ||
          rw_lock_own(&block->lock, RW_LOCK_X));

    /* We must not dereference index here, because it could be freed
    if (index->table->n_ref_count == 0 && !dict_sys_mutex_own()).
    Determine the ahi_slot based on the block contents. */
    ut_ad(!btr_search_own_any(RW_LOCK_S));
    ut_ad(!btr_search_own_any(RW_LOCK_X));

    rw_lock_s_lock(latch, UT_LOCATION_HERE);
    assert_block_ahi_valid(block);

    const auto n_fields = block->curr_n_fields;
    const auto n_bytes = block->curr_n_bytes;
    /* The index associated with a block must remain the
    same, because we are holding block->lock or the block is
    not accessible by other threads (BUF_BLOCK_REMOVE_HASH),
    or the index is not accessible to other threads
    (buf_fix_count == 0 when DROP TABLE or similar is executing
    buf_LRU_drop_page_hash_for_tablespace()). However, if we don't have AHI
    latch then the prefix parameters can change or the index can be dropped for
    the page. */
    const auto latched_index = block->index;
    rw_lock_s_unlock(latch);

    if (latched_index == nullptr) {
      return;
    }
    ut_ad(latched_index == index);

    ut_ad(!index->disable_ahi);
    ut_ad(btr_search_enabled);

    ut_ad(block->page.id.space() == index->space);
    ut_a(index_id == index->id);
    ut_a(!dict_index_is_ibuf(index));
#ifdef UNIV_DEBUG
    switch (dict_index_get_online_status(index)) {
      case ONLINE_INDEX_CREATION:
        /* The index is being created (bulk loaded). */
      case ONLINE_INDEX_COMPLETE:
        /* The index has been published. */
      case ONLINE_INDEX_ABORTED:
        /* Either the index creation was aborted due to an
        error observed by InnoDB (in which case there should
        not be any adaptive hash index entries), or it was
        completed and then flagged aborted in
        rollback_inplace_alter_table(). */
      case ONLINE_INDEX_ABORTED_DROPPED:
        /* Since dropping the indexes are delayed to post_ddl,
        this status is similar to ONLINE_INDEX_ABORTED. */
        break;
      default:
        ut_error;
    }
#endif /* UNIV_DEBUG */

    /* NOTE: The AHI fields of block must not be accessed after
    releasing search latch, as the index page might only be s-latched! */

    ut_a(n_fields > 0 || n_bytes > 0);

    page = block->frame;
    const auto n_recs = page_get_n_recs(page);

    /* Calculate and cache fold values into an array for fast deletion
    from the hash index */

    hashes = ut::make_unique<uint64_t[]>(UT_NEW_THIS_FILE_PSI_KEY, n_recs);

    n_cached = 0;

    const rec_t *rec = page_get_infimum_rec(page);
    rec = page_rec_get_next_low(rec, page_is_comp(page));

    const auto index_hash = btr_search_hash_index_id(index);

    uint64_t prev_hash_value = 0;
    {
      Rec_offsets offsets;

      while (!page_rec_is_supremum(rec)) {
        const auto hash_value = rec_hash(
            rec,
            offsets.compute(rec, index,
                            btr_search_get_n_fields(n_fields, n_bytes)),
            n_fields, n_bytes, index_hash, index);

        if (hash_value != prev_hash_value || prev_hash_value == 0) {
          /* The fold identifies a single hash chain to possibly contain the
          record. We will use it after this iteration over the page's records
          to remove any entries from that chain that point to the page. */
          hashes[n_cached] = hash_value;
          n_cached++;
        }
        rec = page_rec_get_next_low(rec, page_rec_is_comp(rec));
        prev_hash_value = hash_value;
      }
    }

    rw_lock_x_lock(latch, UT_LOCATION_HERE);

    if (UNIV_UNLIKELY(!block->index)) {
      /* Someone else has meanwhile dropped the hash index. */
      assert_block_ahi_valid(block);
      rw_lock_x_unlock(latch);
      return;
    }

    ut_a(block->index == index);

    if (block->curr_n_fields == n_fields && block->curr_n_bytes == n_bytes) {
      break;
    }
    /* Someone else has meanwhile built a new hash index on the page, with
    different parameters */
    rw_lock_x_unlock(latch);

    continue;
  }

  for (size_t i = 0; i < n_cached; i++) {
    ha_remove_a_node_to_page(btr_search_sys->hash_tables[ahi_slot], hashes[i],
                             page);
  }

  const auto info = btr_search_get_info(block->index);
  {
    /* This must be the last operation we do on the index or table
    structure. Once it is 0 it can get freed by any other thread. This
    operation must be at least memory order release to let any other writes
    be completed before any other thread start to free the index or table
    structure. */
    auto old_ref_count = info->ref_count.fetch_sub(1);
    ut_a(old_ref_count > 0);
  }
  block->index = nullptr;

  MONITOR_ATOMIC_INC(MONITOR_ADAPTIVE_HASH_PAGE_REMOVED);
  MONITOR_ATOMIC_INC_VALUE(MONITOR_ADAPTIVE_HASH_ROW_REMOVED, n_cached);

  assert_block_ahi_valid(block);
  rw_lock_x_unlock(latch);
}

void btr_search_drop_page_hash_when_freed(const page_id_t &page_id,
                                          const page_size_t &page_size) {
  buf_block_t *block;
  mtr_t mtr;

  ut_d(export_vars.innodb_ahi_drop_lookups++);

  mtr_start(&mtr);

  /* If the caller has a latch on the page, then the caller must
  have a x-latch on the page and it must have already dropped
  the hash index for the page. Because of the x-latch that we
  are possibly holding, we cannot s-latch the page, but must
  (recursively) x-latch it, even though we are only reading. */

  block = buf_page_get_gen(page_id, page_size, RW_X_LATCH, nullptr,
                           Page_fetch::PEEK_IF_IN_POOL, UT_LOCATION_HERE, &mtr);

  if (block) {
    /* If AHI is still valid, page can't be in free state.
    AHI is dropped when page is freed. */
    ut_ad(!block->page.file_page_was_freed);

    buf_block_dbg_add_level(block, SYNC_TREE_NODE_FROM_HASH);

    dict_index_t *index = block->index;
    if (index != nullptr) {
      /* In all our callers, the table handle should
      be open, or we should be in the process of
      dropping the table (preventing eviction). */
      ut_ad(index->table->n_ref_count > 0 || dict_sys_mutex_own());
      btr_search_drop_page_hash_index(block);
    }
  }

  mtr_commit(&mtr);
}

static void btr_drop_next_batch(const page_size_t &page_size,
                                const dict_index_t **first,
                                const dict_index_t **last) {
  static constexpr unsigned batch_size = 1024;
  std::vector<page_id_t> to_drop;
  to_drop.reserve(batch_size);

  for (ulint i = 0; i < srv_buf_pool_instances; ++i) {
    to_drop.clear();
    buf_pool_t *buf_pool = buf_pool_from_array(i);
    mutex_enter(&buf_pool->LRU_list_mutex);
    const buf_page_t *prev;

    for (const buf_page_t *bpage = UT_LIST_GET_LAST(buf_pool->LRU);
         bpage != nullptr; bpage = prev) {
      prev = UT_LIST_GET_PREV(LRU, bpage);

      ut_a(buf_page_in_file(bpage));
      if (buf_page_get_state(bpage) != BUF_BLOCK_FILE_PAGE ||
          bpage->buf_fix_count > 0) {
        continue;
      }

      const dict_index_t *block_index =
          reinterpret_cast<const buf_block_t *>(bpage)->index;

      /* index == nullptr means the page is no longer in AHI, so no need to
      attempt freeing it */
      if (block_index == nullptr) {
        continue;
      }
      /* pages io fixed for read have index == nullptr */
      ut_ad(!bpage->was_io_fix_read());

      if (std::find(first, last, block_index) != last) {
        to_drop.emplace_back(bpage->id);
        if (to_drop.size() == batch_size) {
          break;
        }
      }
    }

    mutex_exit(&buf_pool->LRU_list_mutex);

    for (const page_id_t &page_id : to_drop) {
      btr_search_drop_page_hash_when_freed(page_id, page_size);
    }
  }
}

void btr_drop_ahi_for_table(dict_table_t *table) {
  const ulint len = UT_LIST_GET_LEN(table->indexes);

  if (len == 0) {
    return;
  }

  const dict_index_t *indexes[MAX_INDEXES];
  const page_size_t page_size(dict_table_page_size(table));

  for (;;) {
    ulint ref_count = 0;
    const dict_index_t **end = indexes;

    for (dict_index_t *index = table->first_index(); index != nullptr;
         index = index->next()) {
      if (ulint n_refs = index->search_info->ref_count) {
        ut_ad(!index->disable_ahi);
        ut_ad(index->is_committed());
        ref_count += n_refs;
        ut_ad(indexes + len > end);
        *end++ = index;
      }
    }

    ut_ad((indexes == end) == (ref_count == 0));

    if (ref_count == 0) {
      return;
    }

    btr_drop_next_batch(page_size, indexes, end);

    std::this_thread::yield();
  }
}

void btr_drop_ahi_for_index(const dict_index_t *index) {
  ut_ad(index->is_committed());

  if (index->disable_ahi || index->search_info->ref_count == 0) {
    return;
  }

  const dict_table_t *table = index->table;
  const page_size_t page_size(dict_table_page_size(table));

  while (true) {
    if (index->search_info->ref_count == 0) {
      return;
    }

    btr_drop_next_batch(page_size, &index, &index + 1);

    std::this_thread::yield();
  }
}

static void btr_search_build_page_hash_index(dict_index_t *index,
                                             buf_block_t *block, bool update) {
  if (index->disable_ahi || !btr_search_enabled) {
    return;
  }

  ut_ad(index);
  ut_ad(block->page.id.space() == index->space);
  ut_a(!dict_index_is_ibuf(index));

  ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));
  ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_S));
  ut_ad(rw_lock_own(&(block->lock), RW_LOCK_S) ||
        rw_lock_own(&(block->lock), RW_LOCK_X));

  btr_search_s_lock(index, UT_LOCATION_HERE);

  const auto table = btr_get_search_table(index);
  const auto page = buf_block_get_frame(block);
  const auto n_fields = block->n_fields.load();
  const auto n_bytes = block->n_bytes.load();
  const auto left_side = block->left_side.load();
  const auto n_fields_for_offsets = btr_search_get_n_fields(n_fields, n_bytes);

  /* We could end up here after the btr_search_update_block_hash_info()
  returned true. This may have happened for a page that is already indexed
  in AHI and also even in case with matching current prefix parameters. In such
  case we will be trying to update all block's record entries in AHI. */
  if (block->index &&
      ((block->curr_n_fields != n_fields) || (block->curr_n_bytes != n_bytes) ||
       (block->curr_left_side != left_side))) {
    btr_search_s_unlock(index);

    btr_search_drop_page_hash_index(block);
  } else {
    btr_search_s_unlock(index);
  }

  /* Check that the values for hash index build are sensible */

  if (n_fields == 0 && n_bytes == 0) {
    return;
  }

  if (dict_index_get_n_unique_in_tree(index) < n_fields_for_offsets) {
    return;
  }

  const auto n_recs = page_get_n_recs(page);

  if (n_recs == 0) {
    return;
  }

  /* Calculate and cache hash values and corresponding records into
  an array for fast insertion to the hash index */

  auto hashes = ut::make_unique<uint64_t[]>(UT_NEW_THIS_FILE_PSI_KEY, n_recs);
  auto recs = ut::make_unique<rec_t *[]>(UT_NEW_THIS_FILE_PSI_KEY, n_recs);

  ut_a(index->id == btr_page_get_index_id(page));

  auto rec = page_rec_get_next(page_get_infimum_rec(page));

  Rec_offsets offsets;
  ut_ad(page_rec_is_supremum(rec) ||
        n_fields + (n_bytes > 0) == rec_offs_n_fields(offsets.compute(
                                        rec, index, n_fields_for_offsets)));

  const auto index_hash = btr_search_hash_index_id(index);

  auto hash_value =
      rec_hash(rec, offsets.compute(rec, index, n_fields_for_offsets), n_fields,
               n_bytes, index_hash, index);

  size_t n_cached = 0;
  if (left_side) {
    hashes[n_cached] = hash_value;
    recs[n_cached] = rec;
    n_cached++;
  }

  for (;;) {
    const auto next_rec = page_rec_get_next(rec);

    if (page_rec_is_supremum(next_rec)) {
      if (!left_side) {
        hashes[n_cached] = hash_value;
        recs[n_cached] = rec;
        n_cached++;
      }

      break;
    }

    const auto next_hash_value = rec_hash(
        next_rec, offsets.compute(next_rec, index, n_fields_for_offsets),
        n_fields, n_bytes, index_hash, index);

    if (hash_value != next_hash_value) {
      /* Insert an entry into the hash index */

      if (left_side) {
        hashes[n_cached] = next_hash_value;
        recs[n_cached] = next_rec;
        n_cached++;
      } else {
        hashes[n_cached] = hash_value;
        recs[n_cached] = rec;
        n_cached++;
      }
    }

    rec = next_rec;
    hash_value = next_hash_value;
  }

  btr_search_check_free_space_in_heap(index);

  /* The AHI is supposed to be heuristic for speed-up. When adding a block
  to index, waiting here for the latch would defy the purpose. We will try
  to add the block to index next time. However, for updates this must
  succeed so the index doesn't contain wrong entries. */
  if (update) {
    btr_search_x_lock(index, UT_LOCATION_HERE);
  } else {
    if (!btr_search_x_lock_nowait(index, UT_LOCATION_HERE)) {
      return;
    }
  }
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
  auto x_latch_guard = create_scope_guard([block, index]() {
    assert_block_ahi_valid(block);
    btr_search_x_unlock(index);
  });
#else
  auto x_latch_guard =
      create_scope_guard([index]() { btr_search_x_unlock(index); });
#endif

  if (!btr_search_enabled) {
    return;
  }

  /* Before we re-acquired the AHI latch, someone else might have already change
  them. In case the block is already indexed and the prefix parameters match,
  we will just update all record's entries. */
  if (block->index &&
      ((block->curr_n_fields != n_fields) || (block->curr_n_bytes != n_bytes) ||
       (block->curr_left_side != left_side))) {
    return;
  }

  /* This counter is decremented every time we drop page
  hash index entries and is incremented here. Since we can
  rebuild hash index for a page that is already hashed, we
  have to take care not to increment the counter in that
  case. */
  if (block->index == nullptr) {
    assert_block_ahi_empty(block);
    index->search_info->ref_count++;
  }

  block->n_hash_helps = 0;

  block->curr_n_fields = n_fields;
  block->curr_n_bytes = n_bytes;
  block->curr_left_side = left_side;
  block->index = index;

  for (size_t i = 0; i < n_cached; i++) {
    ha_insert_for_hash(table, hashes[i], block, recs[i]);
  }

  x_latch_guard.rollback();

  MONITOR_ATOMIC_INC(MONITOR_ADAPTIVE_HASH_PAGE_ADDED);
}

void btr_search_move_or_delete_hash_entries(buf_block_t *new_block,
                                            buf_block_t *block,
                                            dict_index_t *index) {
  /* AHI is disabled for intrinsic table as it depends on index-id
  which is dynamically assigned for intrinsic table indexes and not
  through a centralized index generator. */
  if (index->disable_ahi || !btr_search_enabled) {
    return;
  }

  ut_ad(!index->table->is_intrinsic());

  ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));
  ut_ad(rw_lock_own(&(new_block->lock), RW_LOCK_X));

  btr_search_s_lock(index, UT_LOCATION_HERE);

  ut_a(!new_block->index || new_block->index == index);
  ut_a(!block->index || block->index == index);
  ut_a(!(new_block->index || block->index) || !dict_index_is_ibuf(index));
  assert_block_ahi_valid(block);
  assert_block_ahi_valid(new_block);

  if (new_block->index) {
    btr_search_s_unlock(index);

    btr_search_drop_page_hash_index(block);

    return;
  }

  if (block->index && page_get_n_recs(buf_block_get_frame(new_block)) != 0) {
    /* It is very important for the recommended prefix info in the new block to
    be exactly the same as the current prefix data in the old block. Only
    these parameters will allow the existing entries to be found and updated
    to a new row pointer. A failure to do so would result in a broken AHI
    entries. The new block's recommendation can't change as it requires at
    least S-latch on the block, and we are holding X latch for the new (and
    old) block. */
    new_block->n_fields = block->curr_n_fields;
    new_block->n_bytes = block->curr_n_bytes;
    new_block->left_side = block->curr_left_side;

    ut_a(new_block->n_fields > 0 || new_block->n_bytes > 0);

    btr_search_s_unlock(index);

    btr_search_build_page_hash_index(index, new_block, true);
    ut_ad(new_block->curr_n_fields == block->curr_n_fields);
    ut_ad(new_block->curr_n_bytes == block->curr_n_bytes);
    ut_ad(new_block->curr_left_side == block->curr_left_side);
    return;
  }

  btr_search_s_unlock(index);
}

void btr_search_update_hash_on_delete(btr_cur_t *cursor) {
  hash_table_t *table;
  buf_block_t *block;
  const rec_t *rec;
  dict_index_t *index;

  if (cursor->index->disable_ahi || !btr_search_enabled) {
    return;
  }

  block = btr_cur_get_block(cursor);

  ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));

  assert_block_ahi_valid(block);
  index = block->index;

  if (!index) {
    return;
  }

  ut_ad(block->page.id.space() == index->space);
  ut_a(index == cursor->index);
  ut_a(block->curr_n_fields > 0 || block->curr_n_bytes > 0);
  ut_a(!dict_index_is_ibuf(index));

  table = btr_get_search_table(index);

  rec = btr_cur_get_rec(cursor);

  /* Since we hold the X-latch on block's lock, the AHI prefix parameters
  can't be changed (such change require at least S-latch on block's lock)
  even if the AHI latches are not held. */
  const auto hash_value =
      rec_hash(rec, Rec_offsets{}.compute(rec, index), block->curr_n_fields,
               block->curr_n_bytes, btr_search_hash_index_id(index), index);
  btr_search_x_lock(index, UT_LOCATION_HERE);
  assert_block_ahi_valid(block);

  if (block->index) {
    ut_a(block->index == index);

    if (ha_search_and_delete_if_found(table, hash_value, rec)) {
      MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_REMOVED);
    } else {
      MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_REMOVE_NOT_FOUND);
    }

    assert_block_ahi_valid(block);
  }

  btr_search_x_unlock(index);
}

void btr_search_update_hash_node_on_insert(btr_cur_t *cursor) {
  hash_table_t *table;
  buf_block_t *block;
  dict_index_t *index;
  rec_t *rec;

  if (cursor->index->disable_ahi || !btr_search_enabled) {
    return;
  }

  rec = btr_cur_get_rec(cursor);

  block = btr_cur_get_block(cursor);

  ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));

  index = block->index;

  if (!index) {
    return;
  }

  ut_a(cursor->index == index);
  ut_a(!dict_index_is_ibuf(index));

  if (!btr_search_x_lock_nowait(index, UT_LOCATION_HERE)) {
    return;
  }

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
  auto x_latch_guard = create_scope_guard([index, block] {
    assert_block_ahi_valid(block);
    btr_search_x_unlock(index);
  });
#else
  auto x_latch_guard =
      create_scope_guard([index] { btr_search_x_unlock(index); });
#endif

  if (!block->index) {
    return;
  }

  ut_a(block->index == index);

  if ((cursor->flag == BTR_CUR_HASH) &&
      (cursor->n_fields == block->curr_n_fields) &&
      (cursor->n_bytes == block->curr_n_bytes) && !block->curr_left_side) {
    table = btr_get_search_table(index);

    /* Since we hold the X-latch on block's lock, the AHI prefix parameters
    can't be changed (such change require at least S-latch on block's lock)
    even if the AHI latches are not held in meantime. */
    if (ha_search_and_update_if_found(table, cursor->hash_value, rec, block,
                                      page_rec_get_next(rec))) {
      MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_UPDATED);
    }
  } else {
    x_latch_guard.rollback();

    btr_search_update_hash_on_insert(cursor);
  }
}

void btr_search_update_hash_on_insert(btr_cur_t *cursor) {
  hash_table_t *table;
  buf_block_t *block;
  dict_index_t *index;
  const rec_t *rec;
  const rec_t *ins_rec;
  const rec_t *next_rec;
  uint64_t hash_value;
  uint64_t ins_hash;
  uint64_t next_hash = 0;
  ulint n_fields;
  ulint n_bytes;
  bool locked = false;

  if (cursor->index->disable_ahi || !btr_search_enabled) {
    return;
  }

  block = btr_cur_get_block(cursor);

  ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));
  assert_block_ahi_valid(block);

  index = block->index;

  if (!index) {
    return;
  }

  btr_search_check_free_space_in_heap(index);

  ut_ad(block->page.id.space() == index->space);

  table = btr_get_search_table(index);

  rec = btr_cur_get_rec(cursor);

  ut_a(!index->disable_ahi);
  ut_a(index == cursor->index);
  ut_a(!dict_index_is_ibuf(index));

  /* Since we hold the X-latch on block's lock, the AHI prefix parameters
  can't be changed (such change require at least S-latch on block's lock)
  even if the AHI latches are not held. */
  n_fields = block->curr_n_fields;
  n_bytes = block->curr_n_bytes;
  auto left_side = block->curr_left_side;

  ins_rec = page_rec_get_next_const(rec);
  next_rec = page_rec_get_next_const(ins_rec);

  const auto index_hash = btr_search_hash_index_id(index);
  const ulint n_offs = btr_search_get_n_fields(n_fields, n_bytes);

  Rec_offsets offsets;
  ins_hash = rec_hash(ins_rec, offsets.compute(ins_rec, index, n_offs),
                      n_fields, n_bytes, index_hash, index);

  if (!page_rec_is_supremum(next_rec)) {
    next_hash = rec_hash(next_rec, offsets.compute(next_rec, index, n_offs),
                         n_fields, n_bytes, index_hash, index);
  }

  if (!page_rec_is_infimum(rec)) {
    hash_value = rec_hash(rec, offsets.compute(rec, index, n_offs), n_fields,
                          n_bytes, index_hash, index);
  } else {
    if (left_side) {
      locked = btr_search_x_lock_nowait(index, UT_LOCATION_HERE);

      if (!locked || !btr_search_enabled) {
        goto function_exit;
      }

      ha_insert_for_hash(table, ins_hash, block, ins_rec);
    }

    goto check_next_rec;
  }

  if (hash_value != ins_hash) {
    if (!locked) {
      locked = btr_search_x_lock_nowait(index, UT_LOCATION_HERE);

      if (!locked || !btr_search_enabled) {
        goto function_exit;
      }
    }

    if (!left_side) {
      ha_insert_for_hash(table, hash_value, block, rec);
    } else {
      ha_insert_for_hash(table, ins_hash, block, ins_rec);
    }
  }

check_next_rec:
  if (page_rec_is_supremum(next_rec)) {
    if (!left_side) {
      if (!locked) {
        locked = btr_search_x_lock_nowait(index, UT_LOCATION_HERE);

        if (!locked || !btr_search_enabled) {
          goto function_exit;
        }
      }

      ha_insert_for_hash(table, ins_hash, block, ins_rec);
    }

    goto function_exit;
  }

  if (ins_hash != next_hash) {
    if (!locked) {
      locked = btr_search_x_lock_nowait(index, UT_LOCATION_HERE);

      if (!locked || !btr_search_enabled) {
        goto function_exit;
      }
    }

    if (!left_side) {
      ha_insert_for_hash(table, ins_hash, block, ins_rec);
    } else {
      ha_insert_for_hash(table, next_hash, block, next_rec);
    }
  }

function_exit:
  if (locked) {
    btr_search_x_unlock(index);
  }
}

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG

/** Validates the search system for given hash table.
@param[in]      hash_table_id   hash table to validate
@return true if ok */
static bool btr_search_hash_table_validate(ulint hash_table_id) {
  ha_node_t *node;
  bool ok = true;
  ulint i;
  ulint cell_count;
  Rec_offsets offsets;

  if (!btr_search_enabled) {
    return true;
  }

  /* How many cells to check before temporarily releasing
  search latches. */
  ulint chunk_size = 10000;

  btr_search_x_lock_all(UT_LOCATION_HERE);

  cell_count = hash_get_n_cells(btr_search_sys->hash_tables[hash_table_id]);

  for (i = 0; i < cell_count; i++) {
    /* We release search latches every once in a while to
    give other queries a chance to run. */
    if ((i != 0) && ((i % chunk_size) == 0)) {
      btr_search_x_unlock_all();
      std::this_thread::yield();
      btr_search_x_lock_all(UT_LOCATION_HERE);

      ulint curr_cell_count =
          hash_get_n_cells(btr_search_sys->hash_tables[hash_table_id]);

      if (cell_count != curr_cell_count) {
        cell_count = curr_cell_count;

        if (i >= cell_count) {
          break;
        }
      }
    }

    node = (ha_node_t *)hash_get_nth_cell(
               btr_search_sys->hash_tables[hash_table_id], i)
               ->node;

    for (; node != nullptr; node = node->next) {
      buf_block_t *block = buf_block_from_ahi((byte *)node->data);
      const buf_block_t *hash_block;
      buf_pool_t *buf_pool;

      buf_pool = buf_pool_from_bpage((buf_page_t *)block);
      /* Prevent BUF_BLOCK_FILE_PAGE -> BUF_BLOCK_REMOVE_HASH
      transition until we lock the block mutex */
      mutex_enter(&buf_pool->LRU_list_mutex);

      if (UNIV_LIKELY(buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE)) {
        /* The space and offset are only valid
        for file blocks.  It is possible that
        the block is being freed
        (BUF_BLOCK_REMOVE_HASH, see the
        assertion and the comment below) */
        hash_block = buf_block_hash_get(buf_pool, block->page.id);
      } else {
        hash_block = nullptr;
      }

      if (hash_block) {
        ut_a(hash_block == block);
      } else {
        /* When a block is being freed,
        buf_LRU_free_page() first
        removes the block from
        buf_pool->page_hash by calling
        buf_LRU_block_remove_hashed_page().
        After that, it invokes
        buf_LRU_block_remove_hashed() to
        remove the block from
        btr_search_sys->hash_tables[i]. */

        ut_a(buf_block_get_state(block) == BUF_BLOCK_REMOVE_HASH);
      }

      mutex_enter(&block->mutex);
      mutex_exit(&buf_pool->LRU_list_mutex);

      ut_a(!dict_index_is_ibuf(block->index));
      ut_ad(block->page.id.space() == block->index->space);

      index_id_t page_index_id(block->page.id.space(),
                               btr_page_get_index_id(block->frame));

      const auto offsets_array = offsets.compute(
          node->data, block->index,
          btr_search_get_n_fields(block->curr_n_fields, block->curr_n_bytes));
      const auto hash_value = rec_hash(
          node->data, offsets_array, block->curr_n_fields, block->curr_n_bytes,
          btr_search_hash_index_id(block->index), block->index);

      if (node->hash_value != hash_value) {
        const page_t *page = block->frame;

        ok = false;

        ib::error(ER_IB_MSG_46)
            << "Error in an adaptive hash"
            << " index pointer to page "
            << page_id_t(page_get_space_id(page), page_get_page_no(page))
            << ", ptr mem address "
            << reinterpret_cast<const void *>(node->data) << ", index id "
            << page_index_id << ", node hash " << node->hash_value
            << ", rec hash " << hash_value;

        fputs("InnoDB: Record ", stderr);
        rec_print_new(stderr, node->data, offsets_array);
        fprintf(stderr,
                "\nInnoDB: on that page."
                " Page mem address %p, is hashed %p,"
                " n fields %lu\n"
                "InnoDB: side %lu\n",
                (void *)page, (void *)block->index, (ulong)block->curr_n_fields,
                (ulong)block->curr_left_side);
        ut_d(ut_error);
      }

      mutex_exit(&block->mutex);
    }
  }

  for (i = 0; i < cell_count; i += chunk_size) {
    /* We release search latches every once in a while to
    give other queries a chance to run. */
    if (i != 0) {
      btr_search_x_unlock_all();
      std::this_thread::yield();
      btr_search_x_lock_all(UT_LOCATION_HERE);

      ulint curr_cell_count =
          hash_get_n_cells(btr_search_sys->hash_tables[hash_table_id]);

      if (cell_count != curr_cell_count) {
        cell_count = curr_cell_count;

        if (i >= cell_count) {
          break;
        }
      }
    }

    ulint end_index = std::min(i + chunk_size - 1, cell_count - 1);

    if (!ha_validate(btr_search_sys->hash_tables[hash_table_id], i,
                     end_index)) {
      ok = false;
    }
  }

  btr_search_x_unlock_all();

  return ok;
}

bool btr_search_validate() {
  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    if (!btr_search_hash_table_validate(i)) {
      return (false);
    }
  }

  return (true);
}

#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */
