/*****************************************************************************

Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "buf0block_hint.h"
#include "buf0buf.h"
namespace buf {

void Block_hint::store(buf_block_t *block) {
  ut_ad(block->page.buf_fix_count > 0);
  m_block = block;
  m_page_id = block->page.id;
}

void Block_hint::clear() { m_block = nullptr; }

void Block_hint::buffer_fix_block_if_still_valid() {
  /* We need to check if m_block points to one of chunks. For this to be
  meaningful we need to prevent freeing memory while we check, and until we
  buffer-fix the block. For this purpose it is enough to latch any of the many
  latches taken by buf_resize().
  However, for buffer-fixing to be meaningful, the block has to contain a page
  (as opposed to being already empty, which might mean that buf_pool_resize()
  can proceed and free it once we free the s-latch), so we confirm that the
  block contains a page. However, it is not sufficient to check that this is
  just any page, because just after we check it could get freed, unless we
  have a latch which prevents this. This is tricky because page_hash latches
  are sharded by page_id and we don't know the page_id until we look into the
  block. To solve this chicken-and-egg problem somewhat, we latch the shard
  for the m_page_id and compare block->page.id to it - so if is equal then we
  can be reasonably sure that we have the correct latch.
  There is still a theoretical problem here, where other threads might try
  to modify the m_block->page.id while we are comparing it, but the chance of
  accidentally causing the old space_id == m_page_id.m_space and the new
  page_no == m_page_id.m_page_no is minimal as compilers emit a single 8-byte
  comparison instruction to compare both at the same time atomically, and f()
  will probably double-check the block->page.id again, anyway.
  Finally, assuming that we have correct hash cell latched, we should check if
  the state of the block is BUF_BLOCK_FILE_PAGE before buffer-fixing the block,
  as otherwise we risk buffer-fixing and operating on a block, which is already
  meant to be freed. In particular, buf_LRU_free_page() first calls
  buf_LRU_block_remove_hashed() under hash cell latch protection to change the
  state to BUF_BLOCK_REMOVE_HASH and then releases the latch. Later it calls
  buf_LRU_block_free_hashed_page() without any latch to change the state to
  BUF_BLOCK_MEMORY and reset the page's id, which means buf_resize() can free it
  regardless of our buffer-fixing. */
  if (m_block != nullptr) {
    const buf_pool_t *const pool = buf_pool_get(m_page_id);
    rw_lock_t *latch = buf_page_hash_lock_get(pool, m_page_id);
    rw_lock_s_lock(latch, UT_LOCATION_HERE);
    /* If not own buf_pool_mutex, page_hash can be changed. */
    latch = buf_page_hash_lock_s_confirm(latch, pool, m_page_id);
    if (buf_is_block_in_instance(pool, m_block) &&
        m_page_id == m_block->page.id &&
        buf_block_get_state(m_block) == BUF_BLOCK_FILE_PAGE) {
      buf_block_buf_fix_inc(m_block, UT_LOCATION_HERE);
    } else {
      clear();
    }
    rw_lock_s_unlock(latch);
  }
}
void Block_hint::buffer_unfix_block_if_needed(buf_block_t *block) {
  if (block != nullptr) {
    buf_block_buf_fix_dec(block);
  }
}
}  // namespace buf
