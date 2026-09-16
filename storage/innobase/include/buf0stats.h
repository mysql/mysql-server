/*****************************************************************************

Copyright (c) 2015, 2026, Oracle and/or its affiliates.

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

/** @file include/buf0stats.h
 Buffer pool stats

 Created May 22, 2015 Vasil Dimov
 *******************************************************/

#ifndef buf0stats_h
#define buf0stats_h

#include "univ.i"

#include "btr0btr.h"           /* btr_page_get_index_id() */
#include "dict0types.h"        /* index_id_t, DICT_IBUF_ID_MIN */
#include "fil0fil.h"           /* fil_page_get_type() */
#include "fsp0sysspace.h"      /* srv_tmp_space */
#include "ibuf0ibuf.h"         /* IBUF_SPACE_ID */
#include "mach0data.h"         /* mach_read_from_4() */
#include "page0page.h"         /* page_is_leaf() */
#include "srv0srv.h"           /* srv_buf_pool_curr_size */
#include "ut0lock_free_hash.h" /* ut_lock_free_hash_t */
#include "ut0new.h"            /* ut::new_withkey(), ut::delete_() */

/** Per index buffer pool statistics.

This tracks the number of logical buffer pool file pages that currently satisfy
all of these predicates:
1. the page is present in the buffer pool page hash, not on the free list,
2. the FIL page type is FIL_PAGE_INDEX or FIL_PAGE_RTREE,
3. PAGE_LEVEL is 0, i.e. page_is_leaf() is true,
4. the page id's space id and PAGE_INDEX_ID identify the tracked index.

For compressed tables, the compressed image and the uncompressed frame are two
possible in-memory representations of the same buffer pool page. They are
counted once while the page id remains cached. Discarding only the uncompressed
frame must not decrement the count if the compressed descriptor remains cached.

This is a key,value store where the key is a packed index_id_t and the value is
the number of cached leaf pages that belong to the index. */
class buf_stat_per_index_t {
 public:
  /** Constructor. */
  buf_stat_per_index_t() {
    m_store = ut::new_withkey<ut_lock_free_hash_t>(
        ut::make_psi_memory_key(mem_key_buf_stat_per_index_t), 1024, true);
  }

  /** Destructor. */
  ~buf_stat_per_index_t() { ut::delete_(m_store); }

  /** Increment the number of pages for a given index with 1.
  @param[in]    id      id of the index whose count to increment */
  void inc(const index_id_t &id) {
    if (should_skip(id)) {
      return;
    }

    /* Compressed-only pages can be smaller than UNIV_PAGE_SIZE, so use the
    smallest compressed page size as a conservative logical-page bound. */
    ut_ad(get(id) <
          static_cast<uint64_t>(srv_buf_pool_curr_size / UNIV_ZIP_SIZE_MIN));

    m_store->inc(id.conv_to_int());
  }

  /** Set the count for given id to 0
  @param[in]    id      id of the index whose count will be reset */
  void reset(const index_id_t &id) {
    if (should_skip(id)) {
      return;
    }

    m_store->set(id.conv_to_int(), 0);
  }

  /** Decrement the number of pages for a given index with 1.
  @param[in]    id      id of the index whose count to decrement */
  void dec(const index_id_t &id) {
    if (should_skip(id)) {
      return;
    }

    ut_ad(get(id) > 0);

    m_store->dec(id.conv_to_int());
  }

  /** Decrement the page count if a frame currently satisfies the accounting
  predicate.
  @param[in]    frame           buffer frame */
  void dec_if_tracked_page(const page_t *frame) {
    change_if_tracked_page(frame, false);
  }

  /** Increment the page count if a frame currently satisfies the accounting
  predicate.
  @param[in]    frame           buffer frame */
  void inc_if_tracked_page(const page_t *frame) {
    change_if_tracked_page(frame, true);
  }

  /** Check whether a frame currently satisfies the accounting predicate.
  @param[in]    frame           buffer frame
  @return true if the frame is counted by this structure */
  bool is_tracked_page(const page_t *frame) {
    const ulint page_type = fil_page_get_type(frame);

    if ((page_type != FIL_PAGE_INDEX && page_type != FIL_PAGE_RTREE) ||
        !page_is_leaf(frame)) {
      return false;
    }

    const space_id_t space_id = get_space_id(frame);
    const index_id_t id(space_id, btr_page_get_index_id(frame));

    return !should_skip(id);
  }

  /** Get the number of pages in the buffer pool for a given index.
  @param[in]    id      id of the index whose pages to peek
  @return number of pages */
  uint64_t get(const index_id_t &id) {
    if (should_skip(id)) {
      return (0);
    }

    const int64_t ret = m_store->get(id.conv_to_int());

    if (ret == ut_lock_free_hash_t::NOT_FOUND) {
      /* If the index is not found in this structure,
      then 0 of its pages are in the buffer pool. */
      return (0);
    }

    return (static_cast<uint64_t>(ret >= 0 ? ret : 0));
  }

 private:
  /** Change the page count if a frame currently satisfies the accounting
  predicate.
  @param[in]    frame           buffer frame
  @param[in]    increment       true to increment, false to decrement */
  void change_if_tracked_page(const page_t *frame, bool increment) {
    if (is_tracked_page(frame)) {
      const space_id_t space_id = get_space_id(frame);
      const index_id_t id(space_id, btr_page_get_index_id(frame));

      if (increment) {
        inc(id);
      } else {
        dec(id);
      }
    }
  }

  /** Gets the space id from a page image. Do not use page_get_space_id() here:
  it asserts that the frame is page-aligned, but compressed page images are
  allocated by the buddy allocator and need not satisfy that assertion.
  @param[in]    frame   page image
  @return tablespace identifier from the FIL header */
  static space_id_t get_space_id(const page_t *frame) {
    return mach_read_from_4(frame + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
  }

  /** Assess if we should skip a page from accounting.
  @param[in]    id      index_id of the page
  @return true if it should not be accounted */
  bool should_skip(const index_id_t &id) {
    const bool is_temp = fsp_is_system_temporary(id.m_space_id);

    /* BTR_FREED_INDEX_ID marks an invalidated root page after freeing an index
    tree. It is not a real index and must not be counted if that root page is
    later flushed or evicted. index_id_t::conv_to_int() packs the space id into
    the high 32 bits and the index id into the low 32 bits. Skip index ids with
    high bits set to avoid collisions between index-id bits and space-id bits.
    This also means SDI and IBUF pages are not measured here because they use
    very high index ids; IBUF is named explicitly for readability. */
    return (id.is_ibuf() || is_temp || id.m_index_id == BTR_FREED_INDEX_ID ||
            (id.m_index_id & 0xFFFFFFFF00000000ULL) != 0);
  }

  /** (key, value) storage. */
  ut_lock_free_hash_t *m_store;
};

/** Container for how many pages from each index are contained in the buffer
pool(s). */
extern buf_stat_per_index_t *buf_stat_per_index;

#endif /* buf0stats_h */
