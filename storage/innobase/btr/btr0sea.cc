/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.
Copyright (c) 2008, Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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

/** Flag storing if the search system is in enabled state. While it is false,
the AHI data structures can't have new entries added, they can only be
removed. It is changed to false while having all AHI latches X-latched, so any
section that adds entries to AHI data structures must have at least one S-latch.
All changes to this flag are protected by the btr_search_enable_mutex. */
std::atomic_bool btr_search_enabled = true;

/** A value that basically stores the same as btr_search_enabled, but is not
atomic and thus can be used as SYSVAR. */
bool srv_btr_search_enabled = true;

/** Protects changes of btr_search_enabled flag. */
static ib_mutex_t btr_search_enabled_mutex;

/** Number of adaptive hash index partition. */
ulong btr_ahi_parts = 8;
ut::fast_modulo_t btr_ahi_parts_fast_modulo(8);

#ifdef UNIV_SEARCH_PERF_STAT
/** Number of successful adaptive hash index lookups */
ulint btr_search_n_succ = 0;
/** Number of failed adaptive hash index lookups */
ulint btr_search_n_hash_fail = 0;
#endif /* UNIV_SEARCH_PERF_STAT */

/* It is not unique_ptr, as destroying it at exit() will destroy its rw_lock
after the PFS is deinitialized. */
btr_search_sys_t *btr_search_sys;

/** If the number of records on the page divided by this parameter
would have been successfully accessed using a hash index, the index
is then built on the page, assuming the global limit has been reached */
constexpr uint32_t BTR_SEARCH_PAGE_BUILD_LIMIT = 16;

/** The global limit for consecutive potentially successful hash searches,
before hash index building is started */
constexpr uint32_t BTR_SEARCH_BUILD_LIMIT = 100;

/** Compute a value to seed the hash value of a record.
@param[in]      index   Index structure
@return hash value for seed */
static ulint btr_hash_seed_for_record(const dict_index_t *index) {
  ut_ad(index != nullptr);

  return btr_search_hash_index_id(index);
}

/** Get the hash-table based on index attributes.
A table is selected from an array of tables using pair of index-id, space-id.
@param[in]      index   index handler
@return hash table */
static inline hash_table_t *btr_get_search_table(const dict_index_t *index) {
  /* One can't use the returned table if these latches are not taken. Any resize
  of the AHI that is run in meantime will delete it. Note that btr_ahi_parts
  can't change once AHI is initialized. */
  ut_ad(rw_lock_own_flagged(btr_get_search_latch(index),
                            RW_LOCK_FLAG_S | RW_LOCK_FLAG_X));
  return btr_get_search_part(index).hash_table;
}

/** Determine the number of accessed key fields.
@param[in]      prefix_info     prefix information to get number of fields from
@return number of complete or incomplete fields */
[[nodiscard]] inline uint16_t btr_search_get_n_fields(
    btr_search_prefix_info_t prefix_info) {
  return prefix_info.n_fields + (prefix_info.n_bytes > 0 ? 1 : 0);
}

/** Determine the number of accessed key fields.
@param[in]      cursor          b-tree cursor
@return number of complete or incomplete fields */
[[nodiscard]] inline uint16_t btr_search_get_n_fields(const btr_cur_t *cursor) {
  return btr_search_get_n_fields(cursor->ahi.prefix_info);
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
@param[in] index    index handler
*/
static inline void btr_search_check_free_space_in_heap(dict_index_t *index) {
  if (!btr_search_enabled) {
    return;
  }
  ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_S));
  ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));

  auto &free_block_for_heap = btr_get_search_part(index).free_block_for_heap;

  const bool no_free_block = free_block_for_heap.load() == nullptr;

  /* We can't do this check and alloc a block from Buffer pool only when needed
  while inserting new nodes to AHI hash table, as in case the eviction is needed
  to free up a block from LRU, the AHI latches may be required to complete the
  page eviction. The execution can reach the following path: buf_block_alloc ->
  buf_LRU_get_free_block -> buf_LRU_scan_and_free_block ->
  buf_LRU_free_from_common_LRU_list -> buf_LRU_free_page ->
  btr_search_drop_page_hash_index */
  if (no_free_block) {
    const auto block = buf_block_alloc(nullptr);
    buf_block_t *expected = nullptr;
    ut_ad(block != nullptr);

    if (!free_block_for_heap.compare_exchange_strong(expected, block)) {
      /* Someone must have set the free_block in meantime, return the allocated
      block to pool. */
      buf_block_free(block);
    }
  }
}

void btr_search_sys_create(ulint hash_size) {
  /* Copy the initial SYSVAR value. While the Server is starting, the updater
  for SYSVARs is not called to set their initial value. */
  btr_search_enabled = srv_btr_search_enabled;
  btr_search_sys = ut::new_withkey<btr_search_sys_t>(
      ut::make_psi_memory_key(mem_key_ahi), hash_size);
  mutex_create(LATCH_ID_AHI_ENABLED, &btr_search_enabled_mutex);
}

btr_search_sys_t::btr_search_sys_t(size_t hash_size) {
  using part_type = btr_search_sys_t::search_part_t;
  parts = ut::make_unique_aligned<part_type[]>(
      ut::make_psi_memory_key(mem_key_ahi), alignof(part_type), btr_ahi_parts);
  static_assert(alignof(part_type) >= ut::INNODB_CACHE_LINE_SIZE);
  /* It is written only from one thread during server initialization, so it is
  safe. */
  btr_ahi_parts_fast_modulo = ut::fast_modulo_t{btr_ahi_parts};

  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    parts[i].initialize(hash_size);
  }
}

void btr_search_sys_t::search_part_t::initialize(size_t hash_size) {
  /* Step-1: Init latches. */
  rw_lock_create(btr_search_latch_key, &latch, LATCH_ID_BTR_SEARCH);

  /* Step-2: Allocate hash tables. */
  hash_table = ib_create((hash_size / btr_ahi_parts), LATCH_ID_HASH_TABLE_MUTEX,
                         0, MEM_HEAP_FOR_BTR_SEARCH);
  hash_table->heap->free_block_ptr = &free_block_for_heap;

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
  hash_table->adaptive = true;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
}

void btr_search_sys_resize(ulint hash_size) {
  /* Step-1: Lock all search latches in exclusive mode. */
  btr_search_x_lock_all(UT_LOCATION_HERE);

  if (btr_search_enabled) {
    btr_search_x_unlock_all();

    ib::error(ER_IB_MSG_45) << "btr_search_sys_resize failed because"
                               " hash index hash table is not empty.";
    ut_d(ut_error);
    ut_o(return);
  }

  /* Step-2: Recreate hash tables with new size. */
  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    auto &part = btr_search_sys->parts[i];
    mem_heap_free(part.hash_table->heap);
    ut::delete_(part.hash_table);

    part.hash_table =
        ib_create((hash_size / btr_ahi_parts), LATCH_ID_HASH_TABLE_MUTEX, 0,
                  MEM_HEAP_FOR_BTR_SEARCH);
    part.hash_table->heap->free_block_ptr = &part.free_block_for_heap;

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
    part.hash_table->adaptive = true;
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
  }

  /* Step-3: Unlock all search latches from exclusive mode. */
  btr_search_x_unlock_all();
}

void btr_search_sys_free() {
  if (btr_search_sys == nullptr) {
    return;
  }

  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    auto &part = btr_search_sys->parts[i];
    mem_heap_free(part.hash_table->heap);
    ut::delete_(part.hash_table);
  }

  ut::delete_(btr_search_sys);
  btr_search_sys = nullptr;

  mutex_destroy(&btr_search_enabled_mutex);
}

void btr_search_await_no_reference(dict_table_t *table, dict_index_t *index,
                                   bool force) {
  ut_ad(dict_sys_mutex_own());

  uint sleep_counter = 0;

  /* We always create search info whether adaptive hash index is enabled or not.
   */
  ut_ad(index->search_info);

  while (index->search_info->ref_count.load() != 0 &&
         (force || srv_shutdown_state.load() < SRV_SHUTDOWN_CLEANUP)) {
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    sleep_counter++;

    if (sleep_counter % 500 == 0) {
      ib::error(ER_IB_LONG_AHI_DISABLE_WAIT, sleep_counter / 100,
                index->search_info->ref_count.load(), index->name(),
                table->name.m_name);
    }
    /* To avoid a hang here we commit suicide if the ref_count doesn't drop to
    zero in 600 seconds. */
    ut_a(sleep_counter < 60000);
  }
}

/** Wait for every index in the specified table to have all references from AHI
dropped. This can only be called while the AHI is being disabled. The last fact
causes that no new references to indexes can be added from AHI, so the reference
count will monotonically drop to zero.
@param[in,out]  table   table handler */
static void btr_search_await_no_reference(dict_table_t *table) {
  ut_ad(dict_sys_mutex_own());
  ut_ad(mutex_own(&btr_search_enabled_mutex));

  for (auto index = table->first_index(); index != nullptr;
       index = index->next()) {
    btr_search_await_no_reference(table, index, false);
  }
}

bool btr_search_disable() {
  mutex_enter(&btr_search_enabled_mutex);
  if (!btr_search_enabled) {
    mutex_exit(&btr_search_enabled_mutex);
    return false;
  }

  btr_search_x_lock_all(UT_LOCATION_HERE);

  ut_a(btr_search_enabled);

  btr_search_enabled = false;
  srv_btr_search_enabled = false;
  btr_search_x_unlock_all();

  /* Clear AHI info for all non-private blocks from Buffer Pool. */
  buf_pool_clear_hash_index();

  dict_sys_mutex_enter();
  /* Wait for every index in the data dictionary cache to have no references to
  AHI. After the buf_pool_clear_hash_index() is called, there might be some
  blocks that are being evicted by buf_LRU_free_page() and they are in
  BUF_BLOCK_REMOVE_HASH state. We will wait for them to be removed from AHI. */
  for (auto table : dict_sys->table_LRU) {
    btr_search_await_no_reference(table);
  }

  for (auto table : dict_sys->table_non_LRU) {
    btr_search_await_no_reference(table);
  }

  dict_sys_mutex_exit();

  /* Clear the adaptive hash index. */
  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    const auto hash_table = btr_search_sys->parts[i].hash_table;
    hash_table_clear(hash_table);
    mem_heap_empty(hash_table->heap);
  }

  mutex_exit(&btr_search_enabled_mutex);

  return true;
}

void btr_search_enable() {
  os_rmb;
  /* Don't allow enabling AHI if buffer pool resize is happening.
  Ignore it silently. */
  if (srv_buf_pool_old_size != srv_buf_pool_size) return;

  /* We need to synchronize with any threads that are in the middle of
  btr_search_disable() - they must first clear all structures before we can
  re-enable AHI again. */
  mutex_enter(&btr_search_enabled_mutex);
  btr_search_enabled = true;
  srv_btr_search_enabled = true;
  mutex_exit(&btr_search_enabled_mutex);
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
  info->prefix_info = {0, 1, true};

  return info;
}

/** Updates the search info of an index about hash successes. NOTE that info
is NOT protected by any semaphore, to save CPU time! Do not assume its fields
are consistent.
@param[in]      cursor  cursor which was just positioned */
static void btr_search_info_update_hash(btr_cur_t *cursor) {
  dict_index_t *index = cursor->index;
  int cmp;

  ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_S));
  ut_ad(!rw_lock_own(btr_get_search_latch(index), RW_LOCK_X));

  if (dict_index_is_ibuf(index)) {
    /* So many deletes are performed on an insert buffer tree
    that we do not consider a hash index useful on it: */

    return;
  }

  const uint16_t n_unique =
      static_cast<uint16_t>(dict_index_get_n_unique_in_tree(index));
  const auto info = index->search_info;
  if (info->n_hash_potential != 0) {
    const auto prefix_info = info->prefix_info.load();

    /* Test if the search would have succeeded using the recommended
    hash prefix */

    /* If AHI uses all unique columns as a key, then each record is in its own
    equal-prefix-group, so it doesn't matter if we use left_side or not. Such
    a cache is only useful for searches with the whole unique part of the key
    specified in the query. */

    ut_a(prefix_info.n_fields <= n_unique);
    ut_ad(cursor->up_match <= n_unique);
    ut_ad(cursor->low_match <= n_unique);
    if (prefix_info.n_fields == n_unique &&
        std::max(cursor->up_match, cursor->low_match) == n_unique) {
      info->n_hash_potential++;

      return;
    }

    /* The search in B-tree has stopped at two consecutive tuples 'low' and
    'up', and we'd like the search in AHI to also find one of them. First, it
    means that one of them needs to have same first prefix_info.n_fields fields
    and n_bytes of next field equal to the sought tuple. In other words
    `low_matches_prefix||up_matches_prefix`. But, AHI keeps only one record from
    each equal-prefix-group of records, either the left-most or right-most of
    the group, depending on `prefix_info.left_side`. So if both
    `low_matches_prefix` and `up_matches_prefix` are true, it means there's no
    group boundary between them, and even if one of them is at the boundary it's
    "by accident" and the procedure for recommending a prefix length would not
    choose such a short prefix, as it tries to pick a prefix length which would
    create the boundary between low and up. What we want is that if we cache
    the left-most record from each group, then up matches, and low not, so that
    up is at the boundary, and would get cached. And the opposite if we cache
    right-most. */
    const bool low_matches_prefix =
        0 >= ut_pair_cmp(prefix_info.n_fields, prefix_info.n_bytes,
                         cursor->low_match, cursor->low_bytes);
    const bool up_matches_prefix =
        0 >= ut_pair_cmp(prefix_info.n_fields, prefix_info.n_bytes,
                         cursor->up_match, cursor->up_bytes);
    if (prefix_info.left_side ? (!low_matches_prefix && up_matches_prefix)
                              : (low_matches_prefix && !up_matches_prefix)) {
      info->n_hash_potential++;

      return;
    }
  }
  /* We have to set a new recommendation; skip the hash analysis
  for a while to avoid unnecessary CPU time usage when there is no
  chance for success */

  info->hash_analysis = 0;

  cmp = ut_pair_cmp(cursor->up_match, cursor->up_bytes, cursor->low_match,
                    cursor->low_bytes);
  if (cmp == 0) {
    info->n_hash_potential = 0;

    /* For extra safety, we set some sensible values here */
    info->prefix_info = {0, 1, true};
  } else if (cmp > 0) {
    info->n_hash_potential = 1;

    ut_ad(cursor->up_match <= n_unique);
    if (cursor->up_match == n_unique) {
      info->prefix_info = {0, n_unique, true};

    } else if (cursor->low_match < cursor->up_match) {
      info->prefix_info = {0, static_cast<uint16_t>(cursor->low_match + 1),
                           true};
    } else {
      info->prefix_info = {static_cast<uint32_t>(cursor->low_bytes + 1),
                           static_cast<uint16_t>(cursor->low_match), true};
    }
  } else {
    info->n_hash_potential = 1;

    ut_ad(cursor->low_match <= n_unique);
    if (cursor->low_match == n_unique) {
      info->prefix_info = {0, n_unique, false};
    } else if (cursor->low_match > cursor->up_match) {
      info->prefix_info = {0, static_cast<uint16_t>(cursor->up_match + 1),
                           false};
    } else {
      info->prefix_info = {static_cast<uint32_t>(cursor->up_bytes + 1),
                           static_cast<uint16_t>(cursor->up_match), false};
    }
  }
}

/** Update the block search info on hash successes. NOTE that info and
block->n_hash_helps, ahi.prefix_info are NOT protected by any
semaphore, to save CPU time! Do not assume the fields are consistent.
@return true if building a (new) hash index on the block is recommended
@param[in,out]  block   buffer block
@param[in]      cursor  cursor */
static bool btr_search_update_block_hash_info(buf_block_t *block,
                                              const btr_cur_t *cursor) {
  ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_S));
  ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_X));
  ut_ad(rw_lock_own_flagged(&block->lock, RW_LOCK_FLAG_S | RW_LOCK_FLAG_X));

  const auto info = cursor->index->search_info;
  info->last_hash_succ = false;

  ut_a(buf_block_state_valid(block));
  ut_ad(info->magic_n == BTR_SEARCH_MAGIC_N);

  if (block->n_hash_helps > 0 && info->n_hash_potential > 0 &&
      block->ahi.recommended_prefix_info.load() == info->prefix_info.load()) {
    /* The current index's prefix info is already used as recommendation for
    this block's prefix. */
    if (block->ahi.index &&
        block->ahi.prefix_info.load() == info->prefix_info.load()) {
      /* The recommended prefix is what is actually being used in this block.
      The search would presumably have succeeded using the hash index. */

      info->last_hash_succ = true;
    }

    block->n_hash_helps++;
  } else {
    block->n_hash_helps = 1;
    block->ahi.recommended_prefix_info = info->prefix_info.load();
  }

#ifdef UNIV_DEBUG
  if (cursor->index->table->does_not_fit_in_memory) {
    block->n_hash_helps = 0;
  }
#endif /* UNIV_DEBUG */

  if (info->n_hash_potential >= BTR_SEARCH_BUILD_LIMIT &&
      block->n_hash_helps >
          page_get_n_recs(block->frame) / BTR_SEARCH_PAGE_BUILD_LIMIT) {
    if (!block->ahi.index ||
        block->n_hash_helps > 2 * page_get_n_recs(block->frame) ||
        block->ahi.recommended_prefix_info.load() !=
            block->ahi.prefix_info.load()) {
      /* Build a new hash index on the page if:
      - the block is not yet in AHI, or
      - we queried 2 times the number of records on this page successfully (TODO
      explain the reason why it is needed), or
      - the recommendation differs from what prefix info is currently used in
      block for hashing in AHI. */

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
@param[in]      block   buffer block where cursor positioned
@param[in]      cursor  cursor */
static void btr_search_update_hash_ref(buf_block_t *block,
                                       const btr_cur_t *cursor) {
  ut_ad(cursor->flag == BTR_CUR_HASH_FAIL);
  ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_X));
  ut_ad(rw_lock_own_flagged(&(block->lock), RW_LOCK_FLAG_S | RW_LOCK_FLAG_X));
  ut_ad(page_align(btr_cur_get_rec(cursor)) == buf_block_get_frame(block));
  block->ahi.validate();

  const auto index = block->ahi.index.load();
  const auto block_prefix_info = block->ahi.prefix_info.load();

  if (!index) {
    return;
  }

  ut_ad(block->page.id.space() == index->space);
  ut_a(index == cursor->index);
  ut_a(!dict_index_is_ibuf(index));

  const auto info = index->search_info;
  /* Dirty read without latch, will be repeated after we take the x-latch, which
  we take after we have the hash value ready, to reduce time consumed under the
  latch. If the current index's prefix info is different than current block's
  prefix info used in AHI, then the block will have to be removed (and
  reinserted) from AHI very soon. It does not make sense to update any records
  using outdated prefix info. Note that only records folded using the block's
  current prefix info can be in AHI. */
  if (info->n_hash_potential > 0 &&
      block_prefix_info == info->prefix_info.load()) {
    const auto rec = btr_cur_get_rec(cursor);

    if (!page_rec_is_user_rec(rec)) {
      return;
    }

    const auto hash_value = rec_hash(
        rec, Rec_offsets{}.compute(rec, index), block_prefix_info.n_fields,
        block_prefix_info.n_bytes, btr_hash_seed_for_record(index), index);

    btr_search_check_free_space_in_heap(cursor->index);

    if (!btr_search_x_lock_nowait(cursor->index, UT_LOCATION_HERE)) {
      return;
    }
    /* After we acquire AHI latch we re-check the AHI is enabled, and was not
    disabled and re-enabled in meantime (the block's index would be reset to
    nullptr then, and later maybe even re-inserted to AHI again in case we don't
    have the block->lock X-latched). The block's prefix info will be current and
    we check if it still matches the prefix info we used to fold the record. If
    it does not match, we can't add the entry to hash table, as it would never
    be deleted and would corrupt the AHI. */
    if (btr_search_enabled && block->ahi.index != nullptr) {
      ut_ad(block->ahi.index == index);
      if (info->n_hash_potential > 0 &&
          block_prefix_info == block->ahi.prefix_info.load()) {
        const auto hash_table = btr_get_search_table(index);
        ha_insert_for_hash(hash_table, hash_value, block, rec);
      }
    }
    btr_search_x_unlock(cursor->index);
  }
}

void btr_search_info_update_slow(btr_cur_t *cursor) {
  ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_S));
  ut_ad(!rw_lock_own(btr_get_search_latch(cursor->index), RW_LOCK_X));

  const auto block = btr_cur_get_block(cursor);

  /* NOTE that the following two function calls do NOT protect
  info or block->ahi with any semaphore, to save CPU time!
  We cannot assume the fields are consistent when we return from
  those functions! */
  btr_search_info_update_hash(cursor);

#ifdef UNIV_SEARCH_PERF_STAT
  if (cursor->flag == BTR_CUR_HASH_FAIL) {
    btr_search_n_hash_fail++;
  }
#endif /* UNIV_SEARCH_PERF_STAT */

  if (btr_search_update_block_hash_info(block, cursor)) {
    /* Note that since we did not protect block->ahi with any semaphore, the
    values can be inconsistent. We have to check inside the function call that
    they make sense. */
    btr_search_build_page_hash_index(cursor->index, block, false);
  } else if (cursor->flag == BTR_CUR_HASH_FAIL) {
    /* Update the hash node reference, if appropriate. If
    btr_search_update_block_hash_info decided to build the index for this
    block, the record should be hashed correctly with the rest of the block's
    records. */
    btr_search_update_hash_ref(block, cursor);
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

bool btr_search_guess_on_hash(const dtuple_t *tuple, ulint mode,
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

  ut_ad(tuple && cursor && mtr);
  const auto index = cursor->index;
  ut_ad(index != nullptr);
  const auto info = index->search_info;
  ut_ad(info != nullptr);
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

  const auto prefix_info = info->prefix_info.load();

  cursor->ahi.prefix_info = prefix_info;

  if (dtuple_get_n_fields(tuple) < btr_search_get_n_fields(cursor)) {
    return false;
  }

  const auto hash_value =
      dtuple_hash(tuple, prefix_info.n_fields, prefix_info.n_bytes,
                  btr_hash_seed_for_record(index));

  cursor->ahi.ahi_hash_value = hash_value;

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
    latch_guard.release();
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
    return false;
  }

  buf_block_t *block = buf_block_from_ahi(rec);

  if (!has_search_latch) {
    if (!buf_page_get_known_nowait(latch_mode, block, Cache_hint::MAKE_YOUNG,
                                   __FILE__, __LINE__, mtr)) {
      return false;
    }

    /* Release the AHI S-latch. It is released after the
    buf_page_get_known_nowait which is latching the block, so no one else can
    remove it. Up to this point we have the AHI is S-latched and since we found
    an AHI entry that leads to this block, the entry can't be removed and thus
    the block must be still in the buffer pool. */
    latch_guard.reset();

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

void btr_search_drop_page_hash_index(buf_block_t *block, bool force) {
  for (;;) {
    /* Do a dirty check on block->index, return if the block is
    not in the adaptive hash index. */
    const auto index = block->ahi.index.load();

    if (index == nullptr) {
      return;
    }

    /* Is it safe to dereference the index here?
    If this method is called from a method that uses reference to the index, or
    a cursor on that index, it should not be freed until they finish. Such
    methods are:
    - btr_free_root,
    - btr_page_reorganize_low,
    - btr_page_empty,
    - btr_lift_page_up,
    - btr_compress,
    - btr_discard_only_page_on_level,
    - btr_discard_page,
    - btr_search_update_hash_on_move, which in turn is called from:
      - btr_root_raise_and_insert
      - btr_page_split_and_insert
      - btr_lift_page_up
      - page_copy_rec_list_end
      - page_copy_rec_list_start
    - btr_search_build_page_hash_index, which in turn is called from
      - btr_search_info_update_slow
      - btr_search_update_hash_on_move
    - page_zip_reorganize

    There is a btr_search_drop_page_hash_when_freed() that is calling this
    method, but it asserts:
    ut_ad(index->table->n_ref_count > 0 || dict_sys_mutex_own());
    This checks the table is opened or we have dict sys mutex, either of which
    should prevent index from being freed. Assuming the current thread is the
    one that keeps the table open, which we can't assure easily, but checking if
    the current thread keeps the current block S- or X-latched should make it
    almost certain.

    The last other call is from buf_LRU_free_page(). It does not hold index or
    any latches that could prevent the index from being freed. However, while
    the block is in AHI, it will prevent the index from being freed - the
    block.ahi.index->search_info->ref_count will be positive, and the
    dict_index_remove_from_cache_low() will assure it is 0 before the index is
    freed. The buf_LRU_free_page() first removes the block from all lists and
    hashes, so at the point this method is called, it will be held "privately" -
    no other thread will be able to reach it without iterating every block in
    chunks in the Buffer Pool. So, for index to be freed here after it was
    checked to not be nullptr, it would require some other thread to iterate
    entire Buffer Pool and remove this block from AHI. Something similar is done
    during btr_search_disable(), but in buf_pool_clear_hash_index() we latch the
    block's mutex to prevent block state change, and verify the state is not
    BUF_BLOCK_REMOVE_HASH. Note that the block that we currently process a part
    of buf_LRU_free_page() must be in this state. */
    ut_d(bool block_held_in_private =
             buf_block_get_state(block) == BUF_BLOCK_REMOVE_HASH);
    ut_d(bool index_is_open = index->table->n_ref_count > 0);
    ut_d(bool block_is_being_accessed = rw_lock_own_flagged(
             &block->lock, RW_LOCK_FLAG_S | RW_LOCK_FLAG_X));
    ut_d(bool index_cant_be_freed =
             (index_is_open && block_is_being_accessed) ||
             dict_sys_mutex_own());
    ut_ad(block_held_in_private || index_cant_be_freed);
    /* For now, the only usage of the `force` param is in buf_LRU_free_page()
    while the block state is BUF_BLOCK_REMOVE_HASH. */
    ut_ad(block_held_in_private == force);

    ut_ad(!btr_search_own_any(RW_LOCK_S));
    ut_ad(!btr_search_own_any(RW_LOCK_X));

    block->ahi.validate();
    const auto prefix_info = block->ahi.prefix_info.load();

    ut_ad(!index->disable_ahi);

    ut_ad(block->page.id.space() == index->space);
    ut_a(btr_page_get_index_id(block->frame) == index->id);
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

    ut_a(btr_search_get_n_fields(prefix_info) > 0);

    auto page = block->frame;
    const auto n_recs = page_get_n_recs(page);

    /* Calculate and cache fold values into an array for fast deletion
    from the hash index */

    auto hashes = ut::make_unique<uint64_t[]>(UT_NEW_THIS_FILE_PSI_KEY, n_recs);

    size_t n_cached = 0;

    const rec_t *rec = page_get_infimum_rec(page);
    rec = page_rec_get_next_low(rec, page_is_comp(page));

    const auto index_hash = btr_hash_seed_for_record(index);

    uint64_t prev_hash_value = 0;
    {
      Rec_offsets offsets;

      while (!page_rec_is_supremum(rec)) {
        const auto hash_value = rec_hash(
            rec,
            offsets.compute(rec, index, btr_search_get_n_fields(prefix_info)),
            prefix_info.n_fields, prefix_info.n_bytes, index_hash, index);

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

    btr_search_x_lock(index, UT_LOCATION_HERE);
    const auto critical_section_guard = create_scope_guard([block, index]() {
      block->ahi.validate();
      btr_search_x_unlock(index);
    });

    {
      const auto is_ahi_enabled = btr_search_enabled.load();
      /* We need to read the block->ahi.index after we read btr_search_enabled.
      If the block->ahi.index is nullptr, then it will stay so, because we have
      the X-latch on AHI part.
      If it is not null, then it will stay not-null unless all of the following
      conditions are met:
      - the AHI is being disabled,
      - the block is in LRU (i.e. the force == false).
      So, if we were to read block->ahi.index first, then after reading
      btr_search_enabled == true, we would not know if it wasn't false when the
      latched_index was read, and then the block index could be set to nullptr
      by the buf_pool_clear_hash_index and the AHI was enabled again before we
      read it. In such case we would remove a block from AHI that was not
      indexed in AHI (the block.index would be nullptr already). */
      const auto latched_index = block->ahi.index.load();

      if (latched_index == nullptr) {
        /* Index is already set to null and we have the X-latch on AHI part, the
        block's index can't change to non-null. Nothing to do here. */
        return;
      }

      if (!is_ahi_enabled) {
        /* So, the AHI is being disabled or was already disabled. */
        if (force) {
          /* We are during a call to buf_LRU_free_page(), so the block is held
          in private and it is in BUF_BLOCK_REMOVE_HASH state. We have to clear
          the index and update reference counts instead of
          buf_pool_clear_hash_index(). The buf_pool_clear_hash_index() is not
          clearing the index because it could lead to situation where during the
          call from buf_LRU_free_page() to this method, the index would be
          already not referenced by AHI and its structure memory freed. Please
          refer to "Is it safe to dereference the index here?" comment in this
          method for reference. */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
          block->ahi.n_pointers.store(0);
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
          /* It is important to have the index reset to nullptr after the
          n_pointers is set to 0, so it synchronizes correctly with check in
          buf_block_t::ahi_t::validate(). */
          btr_search_set_block_not_cached(block);
        }
        /* The block is a regular page and buf_pool_clear_hash_index() will
        clear the index and update reference count. Any attempt to do that in
        this thread may result in assertion failure in
        btr_search_set_block_not_cached() as it expects it is not called in
        parallel. */
        return;
      }

      ut_a(latched_index == index);
    }

    block->ahi.validate();
    if (block->ahi.prefix_info.load() == prefix_info) {
      const auto hash_table = btr_get_search_table(index);
      for (size_t i = 0; i < n_cached; i++) {
        ha_remove_a_node_to_page(hash_table, hashes[i], page);
      }

      btr_search_set_block_not_cached(block);
      MONITOR_ATOMIC_INC_VALUE(MONITOR_ADAPTIVE_HASH_ROW_REMOVED, n_cached);

      return;
    }
    /* Someone else has meanwhile built a new hash index on the page, with
    different parameters. We need to retry the process of removal. */
  }
}

void btr_search_set_block_not_cached(buf_block_t *block) {
  block->ahi.assert_empty();
  /* It is important to have the index reset to nullptr after the
  n_pointers is set to 0, so it synchronizes correctly with check in
  buf_block_t::ahi_t::validate(). */
  const auto old_index = block->ahi.index.exchange(nullptr);
  /* This only assures we have checked the index is not null and there is no
  other concurrent thread that had just set it to nullptr. This must be
  assured by callers. If not that, the above line could be just .store(nullptr);
  */
  ut_a(old_index != nullptr);

  /* This check validates assumptions described in latching protocol of the
  ahi_t::index field. */
  ut_ad((buf_block_get_state(block) == BUF_BLOCK_FILE_PAGE &&
         mutex_own(&btr_search_enabled_mutex) && !btr_search_enabled) ||
        (buf_block_get_state(block) == BUF_BLOCK_REMOVE_HASH &&
         !btr_search_enabled) ||
        (rw_lock_own(btr_get_search_latch(old_index), RW_LOCK_X) &&
         btr_search_enabled));

  /* This must be the last operation we do on the index or table
  structure. Once it is 0 it can get freed by any other thread. This
  operation must be at least memory order release to let any other writes
  be completed before any other thread start to free the index or table
  structure. */
  auto old_ref_count = old_index->search_info->ref_count.fetch_sub(1);
  ut_a(old_ref_count > 0);
  block->ahi.assert_empty();

  MONITOR_ATOMIC_INC(MONITOR_ADAPTIVE_HASH_PAGE_REMOVED);
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

    dict_index_t *index = block->ahi.index;
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
          reinterpret_cast<const buf_block_t *>(bpage)->ahi.index;

      /* index == nullptr means the page is no longer in AHI, so no need to
      attempt freeing it */
      if (block_index == nullptr) {
        continue;
      }
      /* pages IO fixed for read have index == nullptr */
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
  const auto info = index->search_info;
  if (index->disable_ahi || info->ref_count == 0) {
    return;
  }

  const dict_table_t *table = index->table;
  const page_size_t page_size(dict_table_page_size(table));

  while (true) {
    if (info->ref_count == 0) {
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
  ut_ad(rw_lock_own_flagged(&(block->lock), RW_LOCK_FLAG_S | RW_LOCK_FLAG_X));
  /* If update is specified, then this thread must hold the block X-latched so
  there are no other threads that could run any other
  btr_search_build_page_hash_index in parallel and thus to assure the AHI
  entries for the specified prefix_info are updated certainly. */
  ut_ad(!(update && !rw_lock_own(&(block->lock), RW_LOCK_X)));

  const auto page = buf_block_get_frame(block);
  const auto prefix_info = block->ahi.recommended_prefix_info.load();
  const auto n_fields_for_offsets = btr_search_get_n_fields(prefix_info);

  /* We could end up here after the btr_search_update_block_hash_info()
  returned true. This may have happened for a page that is already indexed
  in AHI and also even in case with matching current prefix parameters. In such
  case we will be trying to update all block's record entries in AHI. */
  if (block->ahi.index && block->ahi.prefix_info.load() != prefix_info) {
    btr_search_drop_page_hash_index(block);
  }

  /* Check that the values for hash index build are sensible */

  if (prefix_info.n_fields == 0 && prefix_info.n_bytes == 0) {
    return;
  }

  ut_ad(dict_index_get_n_unique_in_tree(index) >= n_fields_for_offsets);

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
  ut_ad(
      page_rec_is_supremum(rec) ||
      btr_search_get_n_fields(prefix_info) ==
          rec_offs_n_fields(offsets.compute(rec, index, n_fields_for_offsets)));

  const auto index_hash = btr_hash_seed_for_record(index);

  auto hash_value =
      rec_hash(rec, offsets.compute(rec, index, n_fields_for_offsets),
               prefix_info.n_fields, prefix_info.n_bytes, index_hash, index);

  size_t n_cached = 0;
  if (prefix_info.left_side) {
    hashes[n_cached] = hash_value;
    recs[n_cached] = rec;
    n_cached++;
  }

  for (;;) {
    const auto next_rec = page_rec_get_next(rec);

    if (page_rec_is_supremum(next_rec)) {
      if (!prefix_info.left_side) {
        hashes[n_cached] = hash_value;
        recs[n_cached] = rec;
        n_cached++;
      }

      break;
    }

    const auto next_hash_value = rec_hash(
        next_rec, offsets.compute(next_rec, index, n_fields_for_offsets),
        prefix_info.n_fields, prefix_info.n_bytes, index_hash, index);

    if (hash_value != next_hash_value) {
      /* Insert an entry into the hash index */

      if (prefix_info.left_side) {
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
    block->ahi.validate();
    btr_search_x_unlock(index);
  });
#else
  auto x_latch_guard =
      create_scope_guard([index]() { btr_search_x_unlock(index); });
#endif

  /* After we acquire AHI latch we re-check the AHI is enabled. If it was
  disabled and re-enabled in meantime then it is not a problem, the
  block->ahi.index will have to be nullptr then and we will just add it to AHI
  now. */
  if (!btr_search_enabled) {
    return;
  }

  /* Before we re-acquired the AHI latch, someone else might have already change
  them. In case the block is already indexed and the prefix parameters match,
  we will just update all record's entries. */
  if (block->ahi.index && block->ahi.prefix_info.load() != prefix_info) {
    /* This can't happen if we are holding X-latch on the block. And (thus) when
    this method is called with update. */
    ut_ad(!rw_lock_own(&(block->lock), RW_LOCK_X));
    ut_a(!update);
    return;
  }

  /* This counter is decremented every time we drop page
  hash index entries and is incremented here. Since we can
  rebuild hash index for a page that is already hashed, we
  have to take care not to increment the counter in that
  case. */
  if (block->ahi.index == nullptr) {
    block->ahi.assert_empty();
    index->search_info->ref_count++;
  }

  block->n_hash_helps = 0;

  block->ahi.prefix_info = prefix_info;
  block->ahi.index = index;

  const auto table = btr_get_search_table(index);
  for (size_t i = 0; i < n_cached; i++) {
    ha_insert_for_hash(table, hashes[i], block, recs[i]);
  }

  x_latch_guard.reset();

  MONITOR_ATOMIC_INC(MONITOR_ADAPTIVE_HASH_PAGE_ADDED);
}

void btr_search_update_hash_on_move(buf_block_t *new_block, buf_block_t *block,
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

  const auto new_block_index = new_block->ahi.index.load();
  const auto old_block_index = block->ahi.index.load();
  ut_a(!new_block_index || new_block_index == index);
  ut_a(!old_block_index || old_block_index == index);
  ut_a(!(new_block_index || old_block_index) || !dict_index_is_ibuf(index));

  /* This method assures that all moved entries from the old block have their
  AHI entries deleted or updated to point to the new_block. */

  /* Are there any outdated entries hashed in the old block? Or maybe none were
  moved? */
  if (old_block_index == nullptr ||
      page_get_n_recs(buf_block_get_frame(new_block)) == 0) {
    /* New block may have some records cached, but it's not a problem to not
    have all entries hashed in AHI. */
    return;
  }
  const auto recommended_settings =
      old_block_index->search_info->prefix_info.load();
  const auto old_settings = block->ahi.prefix_info.load();
  /* Will caching the new_block overwrite outdated entries, that is are the old
  and new block settings matching? And are the old block settings valuable
  enough to keep in cache? */
  if ((!new_block_index || new_block->ahi.prefix_info.load() == old_settings) &&
      (recommended_settings == old_settings)) {
    /* We need to set recommended prefix so it is used by the
    btr_search_build_page_hash_index method. Since we are holding X-latch on
    block->lock, no other thread can modify the recommendation. */
    new_block->ahi.recommended_prefix_info = old_settings;
    btr_search_build_page_hash_index(index, new_block, true);
  } else {
    /* We have to get rid of old entries and don't want to use the new block's
    entries - we forcefully drop all entries on the old block. */
    btr_search_drop_page_hash_index(block);
  }
}

void btr_search_update_hash_on_delete(btr_cur_t *cursor) {
  if (cursor->index->disable_ahi || !btr_search_enabled) {
    return;
  }

  const auto block = btr_cur_get_block(cursor);

  ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));

  block->ahi.validate();
  const auto index = block->ahi.index.load();

  if (!index) {
    return;
  }

  ut_ad(block->page.id.space() == index->space);
  ut_a(index == cursor->index);
  ut_a(!dict_index_is_ibuf(index));

  const auto rec = btr_cur_get_rec(cursor);

  /* Since we hold the X-latch on block's lock, the AHI prefix parameters
  can't be changed (such change require at least S-latch on block's lock)
  even if the AHI latches are not held. */
  const auto prefix_info = block->ahi.prefix_info.load();
  ut_a(btr_search_get_n_fields(prefix_info) > 0);

  const auto hash_value =
      rec_hash(rec, Rec_offsets{}.compute(rec, index), prefix_info.n_fields,
               prefix_info.n_bytes, btr_hash_seed_for_record(index), index);

  btr_search_x_lock(index, UT_LOCATION_HERE);
  const auto table = btr_get_search_table(index);
  block->ahi.validate();

  /* After we acquire AHI latch we re-check the AHI is enabled, and was not
  disabled and re-enabled in meantime (the block's index would be reset to
  nullptr then). */
  if (btr_search_enabled && block->ahi.index != nullptr) {
    ut_a(block->ahi.index == index);
    ut_a(block->ahi.prefix_info.load() == prefix_info);

    if (ha_search_and_delete_if_found(table, hash_value, rec)) {
      MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_REMOVED);
    } else {
      MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_REMOVE_NOT_FOUND);
    }

    block->ahi.validate();
  }

  btr_search_x_unlock(index);
}

void btr_search_update_hash_node_on_insert(btr_cur_t *cursor) {
  if (cursor->index->disable_ahi || !btr_search_enabled) {
    return;
  }

  const auto rec = btr_cur_get_rec(cursor);

  const auto block = btr_cur_get_block(cursor);

  ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));

  const auto index = block->ahi.index.load();

  if (!index) {
    return;
  }

  ut_a(cursor->index == index);
  ut_a(!dict_index_is_ibuf(index));

  const auto prefix_info = block->ahi.prefix_info.load();

  if (cursor->flag == BTR_CUR_HASH && !prefix_info.left_side &&
      cursor->ahi.prefix_info.equals_without_left_side(prefix_info)) {
    if (!btr_search_x_lock_nowait(index, UT_LOCATION_HERE)) {
      return;
    }
    /* After we acquire AHI latch we re-check the AHI is enabled, and was not
    disabled and re-enabled in meantime (the block's index would be reset to
    nullptr then). */
    if (btr_search_enabled && block->ahi.index != nullptr) {
      ut_ad(block->ahi.index == index);
      const auto table = btr_get_search_table(index);

      /* Since we hold the X-latch on block's lock, the AHI prefix parameters
      can't be changed (such change require at least S-latch on block's lock)
      even if the AHI latches are not held in meantime. */
      if (ha_search_and_update_if_found(table, cursor->ahi.ahi_hash_value, rec,
                                        block, page_rec_get_next(rec))) {
        MONITOR_INC(MONITOR_ADAPTIVE_HASH_ROW_UPDATED);
      }
    }

    block->ahi.validate();
    btr_search_x_unlock(index);
  } else {
    btr_search_update_hash_on_insert(cursor);
  }
}

void btr_search_update_hash_on_insert(btr_cur_t *cursor) {
  const rec_t *ins_rec;
  const rec_t *next_rec;
  uint64_t hash_value;
  uint64_t next_hash = 0;
  bool locked = false;

  if (cursor->index->disable_ahi || !btr_search_enabled) {
    return;
  }

  const auto block = btr_cur_get_block(cursor);

  ut_ad(rw_lock_own(&(block->lock), RW_LOCK_X));
  block->ahi.validate();

  const auto index = block->ahi.index.load();

  if (!index) {
    return;
  }

  const auto x_latch_guard = create_scope_guard([&locked, index]() {
    if (locked) {
      btr_search_x_unlock(index);
    }
  });
  btr_search_check_free_space_in_heap(index);

  ut_ad(block->page.id.space() == index->space);

  hash_table_t *table = nullptr;

  const auto rec = btr_cur_get_rec(cursor);

  ut_a(!index->disable_ahi);
  ut_a(index == cursor->index);
  ut_a(!dict_index_is_ibuf(index));

  /* Since we hold the X-latch on block's lock, the AHI prefix parameters
  can't be changed (such change require at least S-latch on block's lock)
  even if the AHI latches are not held. */
  const auto prefix_info = block->ahi.prefix_info.load();

  ins_rec = page_rec_get_next_const(rec);
  next_rec = page_rec_get_next_const(ins_rec);

  const auto index_hash = btr_hash_seed_for_record(index);
  const ulint n_offs = btr_search_get_n_fields(prefix_info);

  Rec_offsets offsets;
  const auto ins_hash =
      rec_hash(ins_rec, offsets.compute(ins_rec, index, n_offs),
               prefix_info.n_fields, prefix_info.n_bytes, index_hash, index);

  if (!page_rec_is_supremum(next_rec)) {
    next_hash =
        rec_hash(next_rec, offsets.compute(next_rec, index, n_offs),
                 prefix_info.n_fields, prefix_info.n_bytes, index_hash, index);
  }

  if (!page_rec_is_infimum(rec)) {
    hash_value =
        rec_hash(rec, offsets.compute(rec, index, n_offs), prefix_info.n_fields,
                 prefix_info.n_bytes, index_hash, index);
  } else {
    if (prefix_info.left_side) {
      locked = btr_search_x_lock_nowait(index, UT_LOCATION_HERE);

      /* After we acquire AHI latch we re-check the AHI is enabled, and was not
      disabled and re-enabled in meantime (the block's index would be reset to
      nullptr then). */
      if (!locked || !btr_search_enabled || block->ahi.index == nullptr) {
        return;
      }
      table = btr_get_search_table(index);

      ha_insert_for_hash(table, ins_hash, block, ins_rec);
    }

    goto check_next_rec;
  }

  if (hash_value != ins_hash) {
    if (!locked) {
      locked = btr_search_x_lock_nowait(index, UT_LOCATION_HERE);

      /* After we acquire AHI latch we re-check the AHI is enabled, and was not
      disabled and re-enabled in meantime (the block's index would be reset to
      nullptr then). */
      if (!locked || !btr_search_enabled || block->ahi.index == nullptr) {
        return;
      }
      table = btr_get_search_table(index);
    }

    if (!prefix_info.left_side) {
      ha_insert_for_hash(table, hash_value, block, rec);
    } else {
      ha_insert_for_hash(table, ins_hash, block, ins_rec);
    }
  }

check_next_rec:
  if (page_rec_is_supremum(next_rec)) {
    if (!prefix_info.left_side) {
      if (!locked) {
        locked = btr_search_x_lock_nowait(index, UT_LOCATION_HERE);

        /* After we acquire AHI latch we re-check the AHI is enabled, and was
          not disabled and re-enabled in meantime (the block's index would be
          reset to nullptr then). */
        if (!locked || !btr_search_enabled || block->ahi.index == nullptr) {
          return;
        }
        table = btr_get_search_table(index);
      }

      ha_insert_for_hash(table, ins_hash, block, ins_rec);
    }

    return;
  }

  if (ins_hash != next_hash) {
    if (!locked) {
      locked = btr_search_x_lock_nowait(index, UT_LOCATION_HERE);

      /* After we acquire AHI latch we re-check the AHI is enabled, and was not
      disabled and re-enabled in meantime (the block's index would be reset to
      nullptr then). */
      if (!locked || !btr_search_enabled || block->ahi.index == nullptr) {
        return;
      }
      table = btr_get_search_table(index);
    }

    if (!prefix_info.left_side) {
      ha_insert_for_hash(table, ins_hash, block, ins_rec);
    } else {
      ha_insert_for_hash(table, next_hash, block, next_rec);
    }
  }
}

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG

/** Validates the search system for given hash table.
@param[in]      part_id   Part ID of AHI, for which hash table is to be
                          validated.
@return true if OK */
static bool btr_search_hash_table_validate(ulint part_id) {
  ha_node_t *node;
  bool ok = true;
  ulint i;
  ulint cell_count;
  Rec_offsets offsets;

  mutex_enter(&btr_search_enabled_mutex);
  if (!btr_search_enabled) {
    mutex_exit(&btr_search_enabled_mutex);
    return true;
  }

  const auto &part = btr_search_sys->parts[part_id];

  ut_ad(part.hash_table->heap->free_block_ptr == &part.free_block_for_heap);

  /* How many cells to check before temporarily releasing
  search latches. */
  constexpr auto chunk_size = 10000;

  btr_search_x_lock_all(UT_LOCATION_HERE);

  cell_count = hash_get_n_cells(part.hash_table);

  for (i = 0; i < cell_count; i++) {
    /* We release search latches every once in a while to
    give other queries a chance to run. */
    if ((i != 0) && ((i % chunk_size) == 0)) {
      btr_search_x_unlock_all();
      std::this_thread::yield();
      btr_search_x_lock_all(UT_LOCATION_HERE);

      ulint curr_cell_count = hash_get_n_cells(part.hash_table);

      if (cell_count != curr_cell_count) {
        cell_count = curr_cell_count;

        if (i >= cell_count) {
          break;
        }
      }
    }

    node = (ha_node_t *)hash_get_nth_cell(part.hash_table, i)->node;

    for (; node != nullptr; node = node->next) {
      buf_block_t *const block = buf_block_from_ahi((byte *)node->data);
      const buf_block_t *hash_block;

      const auto buf_pool = buf_pool_from_bpage((buf_page_t *)block);
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

      const auto index = block->ahi.index.load();
      const auto prefix_info = block->ahi.prefix_info.load();

      ut_a(!dict_index_is_ibuf(index));
      ut_ad(block->page.id.space() == index->space);

      const auto offsets_array = offsets.compute(
          node->data, index, btr_search_get_n_fields(prefix_info));
      const auto hash_value =
          rec_hash(node->data, offsets_array, prefix_info.n_fields,
                   prefix_info.n_bytes, btr_hash_seed_for_record(index), index);

      if (node->hash_value != hash_value) {
        const page_t *page = block->frame;

        ok = false;

        ib::error(ER_IB_MSG_46)
            << "Error in an adaptive hash"
            << " index pointer to page "
            << page_id_t(page_get_space_id(page), page_get_page_no(page))
            << ", ptr mem address "
            << reinterpret_cast<const void *>(node->data) << ", index id "
            << index_id_t{index->space, index->id} << ", node hash "
            << node->hash_value << ", rec hash " << hash_value;

        fputs("InnoDB: Record ", stderr);
        rec_print_new(stderr, node->data, offsets_array);
        fprintf(stderr,
                "\nInnoDB: on that page."
                " Page mem address %p, is hashed %p,"
                " n fields %lu\n"
                "InnoDB: side %lu\n",
                (void *)page, (void *)block->ahi.index.load(),
                (ulong)prefix_info.n_fields, (ulong)prefix_info.left_side);
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

      ulint curr_cell_count = hash_get_n_cells(part.hash_table);

      if (cell_count != curr_cell_count) {
        cell_count = curr_cell_count;

        if (i >= cell_count) {
          break;
        }
      }
    }

    ulint end_index = std::min(i + chunk_size - 1, cell_count - 1);

    if (!ha_validate(part.hash_table, i, end_index)) {
      ok = false;
    }
  }

  btr_search_x_unlock_all();

  mutex_exit(&btr_search_enabled_mutex);

  return ok;
}

bool btr_search_validate() {
  for (ulint i = 0; i < btr_ahi_parts; ++i) {
    if (!btr_search_hash_table_validate(i)) {
      return false;
    }
  }

  return true;
}

#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */
