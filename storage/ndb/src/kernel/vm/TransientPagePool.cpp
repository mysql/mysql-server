/*
   Copyright (c) 2017, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "TransientPagePool.hpp"
#include "Pool.hpp"
#include "debugger/EventLogger.hpp"
#include "ndb_limits.h"
#include "ndbd_malloc_impl.hpp"
#include "util/require.h"

#define JAM_FILE_ID 503

TransientPagePool::TransientPagePool()
    : m_mem_manager(NULL), m_root_page(NULL), m_top(RNIL), m_type_id(0) {}

TransientPagePool::TransientPagePool(Uint32 type_id,
                                     Ndbd_mem_manager *mem_manager)
    : m_mem_manager(NULL), m_root_page(NULL), m_top(RNIL), m_type_id(0) {
  init(type_id, mem_manager);
}

void TransientPagePool::init(Uint32 type_id, Ndbd_mem_manager *mem_manager) {
  assert(m_mem_manager == NULL);
  assert(m_root_page == NULL);
  assert(m_top == RNIL);
  assert(m_type_id == 0);

  m_type_id = type_id;
  m_mem_manager = mem_manager;

  /**
   * Try allocate one root page, one second level map page.
   */
  Uint32 page_count = 2;
  Uint32 page_number;
  m_mem_manager->alloc_pages(m_type_id, &page_number, &page_count, 1,
                             Ndbd_mem_manager::NDB_ZONE_LE_32);

  if (unlikely(page_count == 0)) {
    return;
  }

  void *p = m_mem_manager->get_page(page_number);
  m_root_page = new (p) MapPage(m_type_id);

  if (unlikely(page_count == 1)) {
    return;
  }

  (void)new (m_root_page + 1) MapPage(m_type_id);
  m_root_page->set(0, page_number + 1);
}

bool TransientPagePool::seize(Ptr<Page> &p) {
  Uint32 index = get_next_index(m_top);
  if (unlikely(index == RNIL)) {
    return false;
  }
  Uint32 page_number;
  void *vpage = m_mem_manager->alloc_page(m_type_id, &page_number,
                                          Ndbd_mem_manager::NDB_ZONE_LE_32);
  if (unlikely(vpage == NULL)) {
    return false;
  }
  require(page_number != MapPage::NO_VALUE);
  assert(page_number < RNIL);
  if (unlikely(!set(index, page_number))) {
    m_mem_manager->release_page(m_type_id, page_number);
    return false;
  }
  p.i = index;
  p.p = new (vpage) Page;
  assert(m_type_id != 0);
  p.p->m_magic = Magic::make(m_type_id);
  p.p->m_page_id = index;
  return true;
}

bool TransientPagePool::release(Uint32 i) {
  assert(i == m_top);
  Uint32 page_number = get(i);
  require(page_number != MapPage::NO_VALUE);
  assert(page_number < RNIL);
  require(clear(i));
  m_mem_manager->release_page(m_type_id, page_number);
  shrink();
  return true;
}

bool TransientPagePool::getPtr(Ptr<Page> &p) const {
  if (unlikely(!getUncheckedPtr(p))) {
    return false;
  }
  if (unlikely(!(p.p != NULL && Magic::match(p.p->m_magic, m_type_id)))) {
    g_eventLogger->info(
        "Magic::match failed in %s: "
        "type_id %08x rg %u tid %u: "
        "slot_size -: ptr.i %u: ptr.p %p: "
        "magic %08x expected %08x",
        __func__, m_type_id, GET_RG(m_type_id), GET_TID(m_type_id), p.i, p.p,
        p.p->m_magic, Magic::make(m_type_id));
    require(p.p != NULL && Magic::match(p.p->m_magic, m_type_id));
  }
  return true;
}

Uint64 TransientPagePool::getMemoryNeed(Uint32 pages) {
  const Uint64 map_pages =
      1 + (pages + MapPage::PAGE_WORDS - 1) / MapPage::PAGE_WORDS;
  return map_pages * sizeof(MapPage);
}

bool TransientPagePool::getUncheckedPtr(Ptr<Page> &p) const {
  if (unlikely(p.i == RNIL)) {
    p.p = NULL;
    return false;
  }
  Uint32 page_number = get_valid(p.i);
  if (unlikely(page_number == MapPage::NO_VALUE)) {
    return false;
  }
  assert(page_number < RNIL);
  void *page = m_mem_manager->get_page(page_number);
  p.p = static_cast<Page *>(page);
  return true;
}

bool TransientPagePool::getValidPtr(Ptr<Page> &p) const {
  if (unlikely(!getUncheckedPtr(p))) {
    return false;
  }
  if (unlikely(p.p == NULL)) {
    return false;
  }
  return Magic::match(p.p->m_magic, m_type_id);
}

inline bool TransientPagePool::is_valid_index(Uint32 index) {
  return (index <= MapPage::MAX_PAGE_ID_2L) &&
         ((index & MapPage::VALUE_INDEX_MASK) <= MapPage::MAX_PAGE_ID_1L);
}

inline Uint32 TransientPagePool::get_next_index(Uint32 index) {
  if (unlikely(index == RNIL)) return 0;
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  require(is_valid_index(index));
#endif
  if (likely((index & MapPage::VALUE_INDEX_MASK) != MapPage::MAX_PAGE_ID_1L))
    return index + 1;
  if (unlikely(index == MapPage::MAX_PAGE_ID_2L)) return RNIL;
  return index + 1 + MapPage::PAGE_ID_GAP;
}

inline Uint32 TransientPagePool::get_prev_index(Uint32 index) {
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  require(is_valid_index(index));
#endif
  if (likely((index & MapPage::VALUE_INDEX_MASK) != 0)) return index - 1;
  if (unlikely(index == 0)) return RNIL;
  return index - 1 - MapPage::PAGE_ID_GAP;
}

/*
Uint32 TransientPagePool::get_next_indexes(Uint32 index, Uint32 indexes[],
Uint32 n) const
{
  Uint32 i;
  for (i = 0; i < n; i++)
  {
    Uint32 next = get_next_index(index);
    if (unlikely(next == RNIL))
    {
      break;
    }
    indexes[i] = next;
    index = next;
  }
  return i;
}
*/
inline bool TransientPagePool::set(Uint32 index, Uint32 value) {
  require(value != MapPage::NO_VALUE);
  assert(value < RNIL);
  assert(index <= get_next_index(m_top));
  require(m_root_page != NULL);

  Uint32 high =
      (index >> MapPage::VALUE_INDEX_BITS) & MapPage::VALUE_INDEX_MASK;
  require(high < MapPage::PAGE_WORDS);

  Uint32 low = index & MapPage::VALUE_INDEX_MASK;
  require(low < MapPage::PAGE_WORDS);

  Uint32 leaf_page_id = m_root_page->get(high);
  MapPage *leaf_page;
  if (unlikely(leaf_page_id == MapPage::NO_VALUE)) {
    void *p = m_mem_manager->alloc_page(m_type_id, &leaf_page_id,
                                        Ndbd_mem_manager::NDB_ZONE_LE_32);
    if (unlikely(p == NULL)) {
      return false;
    }
    require(leaf_page_id != MapPage::NO_VALUE);
    assert(leaf_page_id < RNIL);
    leaf_page = new (p) MapPage(m_type_id);
    m_root_page->set(high, leaf_page_id);
  } else {
    assert(leaf_page_id < RNIL);
    void *page = m_mem_manager->get_page(leaf_page_id);
    leaf_page = static_cast<MapPage *>(page);
  }
  leaf_page->set(low, value);
  if (m_top == RNIL || index > m_top) {
    require(index == get_next_index(m_top));
    m_top = index;
  }
  return true;
}

inline bool TransientPagePool::clear(Uint32 index) {
  require(index == m_top);  // Can only clear from top
  require(m_top != RNIL && index <= m_top);
  require(m_root_page != NULL);
  Uint32 high =
      (index >> MapPage::VALUE_INDEX_BITS) & MapPage::VALUE_INDEX_MASK;
  require(high < MapPage::PAGE_WORDS);
  Uint32 leaf_page_id = m_root_page->get(high);
  require(leaf_page_id != MapPage::NO_VALUE);
  assert(leaf_page_id < RNIL);
  void *vpage = m_mem_manager->get_page(leaf_page_id);
  MapPage *leaf_page = static_cast<MapPage *>(vpage);
  Uint32 low = index & MapPage::VALUE_INDEX_MASK;
  require(low < MapPage::PAGE_WORDS);
  leaf_page->set(low, MapPage::NO_VALUE);
  return true;
}

inline Uint32 TransientPagePool::get(Uint32 index) const {
  require(m_top != RNIL && index <= m_top);
  require(m_root_page != NULL);
  Uint32 high =
      (index >> MapPage::VALUE_INDEX_BITS) & MapPage::VALUE_INDEX_MASK;
  require(high < MapPage::PAGE_WORDS);
  Uint32 leaf_page_id = m_root_page->get(high);
  require(leaf_page_id != MapPage::NO_VALUE);
  assert(leaf_page_id < RNIL);
  void *vpage = m_mem_manager->get_page(leaf_page_id);
  MapPage *leaf_page = static_cast<MapPage *>(vpage);
  Uint32 low = index & MapPage::VALUE_INDEX_MASK;
  require(low < MapPage::PAGE_WORDS);
  const Uint32 value = leaf_page->get(low);
  assert(value < RNIL);
  return value;
}

inline Uint32 TransientPagePool::get_valid(Uint32 index) const {
  if (unlikely(m_top == RNIL || index > m_top)) {
    return MapPage::NO_VALUE;
  }
  if (unlikely(m_root_page == NULL)) {
    return MapPage::NO_VALUE;
  }

  Uint32 high =
      (index >> MapPage::VALUE_INDEX_BITS) & MapPage::VALUE_INDEX_MASK;
  if (unlikely(high >= MapPage::PAGE_WORDS)) {
    return MapPage::NO_VALUE;
  }

  Uint32 leaf_page_id = m_root_page->get(high);
  if (unlikely(leaf_page_id == MapPage::NO_VALUE)) {
    return MapPage::NO_VALUE;
  }

  Uint32 low = index & MapPage::VALUE_INDEX_MASK;
  if (unlikely(low >= MapPage::PAGE_WORDS)) {
    return MapPage::NO_VALUE;
  }

  void *vpage = m_mem_manager->get_page(leaf_page_id);
  MapPage *leaf_page = static_cast<MapPage *>(vpage);
  return leaf_page->get(low);
}

/*
 * Return true if a map page was removed and the there are a new top that may
 * be removed.
 */
inline bool TransientPagePool::shrink() {
  if (unlikely(m_root_page == NULL || m_top == RNIL)) {
    return false;
  }

  Uint32 index = m_top;
  Uint32 new_top = get_prev_index(index);

  Uint32 high =
      (index >> MapPage::VALUE_INDEX_BITS) & MapPage::VALUE_INDEX_MASK;
  require(high < MapPage::PAGE_WORDS);
  Uint32 leaf_page_id = m_root_page->get(high);
  require(leaf_page_id != MapPage::NO_VALUE);
  void *vpage = m_mem_manager->get_page(leaf_page_id);
  MapPage *leaf_page = static_cast<MapPage *>(vpage);

  Uint32 low = index & MapPage::VALUE_INDEX_MASK;
  require(low < MapPage::PAGE_WORDS);
  require(leaf_page->get(low) == MapPage::NO_VALUE);

  m_top = new_top;

  if (on_same_map_page(new_top, index)) return false;

  m_mem_manager->release_page(m_type_id, leaf_page_id);
  m_root_page->set(high, MapPage::NO_VALUE);

  if (new_top == RNIL) return false;
  return true;
}

inline TransientPagePool::MapPage::MapPage(Uint32 magic) {
  static_assert(NO_VALUE == 0);
  /* zero fill both m_reserved and m_values */
  memset(this, 0, sizeof(*this));
  require(magic != 0);
  m_magic = magic;
}

inline Uint32 TransientPagePool::MapPage::get(Uint32 i) const {
  require(i < PAGE_WORDS);
  return m_values[i];
}

inline void TransientPagePool::MapPage::set(Uint32 i, Uint32 v) {
  require(i < PAGE_WORDS);
  m_values[i] = v;
}

#ifdef TEST_TRANSIENTPAGEPOOL

#undef JAM_FILE_ID

#include "ndb_types.h"
#include "unittest/mytap/tap.h"

/*
 * Putting actual tests inside Test class to get access to private members of
 * TransientPagePool.
 * class Test is friend of TransientPagePool.
 */

class Test {
 public:
  Test();
};

Test::Test() {
  // RNIL indicates no pages mapped, first index is 0
  ok1(TransientPagePool::get_next_index(RNIL) == 0);
  ok1(TransientPagePool::get_next_index(0) == 1);
  // 8183 is last valid id on a map page, 8192 is the first id on next page
  ok1(TransientPagePool::get_next_index(8183) == 8192);
  ok1(TransientPagePool::get_next_index(8183 + 8192) == 16384);
  ok1(TransientPagePool::get_next_index(8182 + 8183 * 8192) ==
      (8183 + 8183 * 8192));
  // Last valid id is 8183 + 8183 * 8192, nothing after that.
  ok1(TransientPagePool::get_next_index(8183 + 8183 * 8192) == RNIL);

  // 0 is first valid page id, nothing before that.
  ok1(TransientPagePool::get_prev_index(0) == RNIL);
  ok1(TransientPagePool::get_prev_index(1) == 0);
  ok1(TransientPagePool::get_prev_index(8192) == 8183);
  ok1(TransientPagePool::get_prev_index(16384) == (8183 + 8192));
  ok1(TransientPagePool::get_prev_index(8183 + 8183 * 8192) ==
      (8182 + 8183 * 8192));
}

int main(int argc, char *argv[]) {
  plan(11);

  Test dummy;

  return exit_status();
}

#endif
