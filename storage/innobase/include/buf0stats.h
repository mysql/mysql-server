/*****************************************************************************

Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

/** @file include/buf0stats.h
 Buffer pool stats

 Created May 22, 2015 Vasil Dimov
 *******************************************************/

#ifndef buf0stats_h
#define buf0stats_h

#include "univ.i"

#include "dict0types.h"        /* index_id_t, DICT_IBUF_ID_MIN */
#include "fsp0sysspace.h"      /* srv_tmp_space */
#include "ibuf0ibuf.h"         /* IBUF_SPACE_ID */
#include "ut0lock_free_hash.h" /* ut_lock_free_hash_t */
#include "ut0new.h"            /* ut::new_withkey(), ut::delete_() */

/** Per index buffer pool statistics - contains how many pages for each index
are cached in the buffer pool(s). This is a key,value store where the key is
the index id and the value is the number of pages in the buffer pool that
belong to this index. */
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

    m_store->inc(id.conv_to_int());
  }

  /** Decrement the number of pages for a given index with 1.
  @param[in]    id      id of the index whose count to decrement */
  void dec(const index_id_t &id) {
    if (should_skip(id)) {
      return;
    }

    m_store->dec(id.conv_to_int());
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
  /** Assess if we should skip a page from accounting.
  @param[in]    id      index_id of the page
  @return true if it should not be accounted */
  bool should_skip(const index_id_t &id) {
    const bool is_temp = fsp_is_system_temporary(id.m_space_id);

    return (id.is_ibuf() || is_temp ||
            (id.m_index_id & 0xFFFFFFFF00000000ULL) != 0);
  }

  /** (key, value) storage. */
  ut_lock_free_hash_t *m_store;
};

/** Container for how many pages from each index are contained in the buffer
pool(s). */
extern buf_stat_per_index_t *buf_stat_per_index;

#endif /* buf0stats_h */
