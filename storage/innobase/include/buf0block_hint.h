/*****************************************************************************

Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
#ifndef buf0block_hint_h
#define buf0block_hint_h
#include "buf0types.h"

namespace buf {
class Block_hint {
 public:
  /** Stores the pointer to the block, which is currently buffer-fixed.
  @param[in]  block   a pointer to a buffer-fixed block to be stored */
  void store(buf_block_t *block);

  /** Clears currently stored pointer. */
  void clear();

  /** Executes given function with the block pointer which was previously stored
  or with nullptr if the pointer is no longer valid, was cleared or not stored.
  @param[in]  f   The function to be executed. It will be passed the pointer.
                  If you wish to use the block pointer subsequently, you need to
                  ensure you buffer-fix it before returning from f.
  @return the return value of f
  */
  template <typename F>
  auto run_with_hint(F &&f) {
    buffer_fix_block_if_still_valid();
    /* m_block could be changed during f() call, so we use local variable to
    remember which block we need to unfix */
    buf_block_t *buffer_fixed_block = m_block;
    auto res = f(buffer_fixed_block);
    buffer_unfix_block_if_needed(buffer_fixed_block);
    return res;
  }

 private:
  /** The block pointer stored by store(). */
  buf_block_t *m_block{nullptr};
  /** If m_block is non-null, the m_block->page.id at time it was stored. */
  page_id_t m_page_id{0, 0};

  /** A helper function which checks if m_block is not a dangling pointer and
  still points to block with page with m_page_id and if so, buffer-fixes it,
  otherwise clear()s it */
  void buffer_fix_block_if_still_valid();

  /** A helper function which decrements block->buf_fix_count if it's non-null
  @param[in]  block   A pointer to a block or nullptr */
  static void buffer_unfix_block_if_needed(buf_block_t *block);
};

}  // namespace buf
#endif /* buf0hint_h*/
