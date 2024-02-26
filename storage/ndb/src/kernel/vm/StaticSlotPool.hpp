/*
   Copyright (c) 2018, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef STATICSLOTPOOL_HPP
#define STATICSLOTPOOL_HPP

#include "util/require.h"
#include "debugger/EventLogger.hpp"
#include "portlib/ndb_prefetch.h"
#include "vm/IntrusiveList.hpp"
#include "vm/ndbd_malloc_impl.hpp"
#include "vm/Slot.hpp"

#define JAM_FILE_ID 508


/**
 * StaticSlotPool
 *
 * The StaticSlotPool is intended to be a fast pool of records, still based on
 * pages.
 * 
 * The pool have a fixed size set by calling init() methods once.  At which
 * point a number of consecutive pages are allocated.
 *
 * This pool never shrink or release any pages.
 *
 * Pool keep a free list of records, this is setup by repeated calls to
 * startup() method.  Records are picked and put back at free list head (LIFO).
 *
 * First record on page is aligned to 8 words within page.
 */

class StaticSlotPool
{
 public:
  friend class LocalSlotPool<StaticSlotPool>;
  class Page;
  StaticSlotPool();
  void init(Uint32 type_id,
            Uint32 slot_size,
            Uint32* min_recs,
            const Pool_context& pool_ctx);
  bool startup(Uint32* startup_count_ptr, Uint32 slot_size);
  void free(Uint32 type_id, const Pool_context& pool_ctx);
  bool release(Ptr<Slot> p, Uint32 slot_size);
  bool seize(Ptr<Slot>& p, Uint32 slot_size);
  Slot* getPtr(Uint32 i, Uint32 slot_size) const;
  void getPtr(Ptr<Slot>& p, Uint32 slot_size) const;
  bool getValidPtr(Ptr<Slot>& p, Uint32 magic, Uint32 slot_size) const;
  bool getUncheckedPtrRO(Ptr<Slot>& p, Uint32 slot_size) const;
  bool getUncheckedPtrRW(Ptr<Slot>& p, Uint32 slot_size) const;
  Uint32 getSize() const { return m_slot_count; }
  Uint32 getUncheckedPtrs(Uint32* from,
                          Ptr<Slot> ptrs[],
                          Uint32 cnt,
                          Uint32 slot_size) const;
  bool may_shrink() const;
  bool rearrange_free_list_and_shrink(Uint32* max_shrinks,
                                      Uint32 /* slot_size */);

  static Uint64 getMemoryNeed(Uint32 slot_size, Uint32 entry_count);

 private:
  typedef LocalSLList<LocalSlotPool<StaticSlotPool> > Slot_list;

  Page* m_page_base;
  Slot_list::Head m_free_list;
  Uint32 m_slot_count;
};

class StaticSlotPool::Page
{
  friend class StaticSlotPool;

 private:
  static constexpr Uint32 WORDS_PER_PAGE = 8192;
  static constexpr Uint32 HEADER_WORDS = 8;
  static constexpr Uint32 DATA_WORDS_PER_PAGE = (WORDS_PER_PAGE - HEADER_WORDS);
  static constexpr Uint32 DATA_BYTE_OFFSET = HEADER_WORDS * sizeof(Uint32);
  Uint32 m_magic;
  Uint32 m_page_id;
  Uint32 m_reserved[6];
  Uint32 m_data[DATA_WORDS_PER_PAGE];

  static void static_asserts()
  {
    static_assert(sizeof(Page) == WORDS_PER_PAGE * sizeof(Uint32));
  }
};

inline bool StaticSlotPool::seize(Ptr<Slot>& p, Uint32 slot_size)
{
  LocalSlotPool<StaticSlotPool> pool(this, slot_size);
  Slot_list free_list(pool, m_free_list);

  if (likely(free_list.removeFirst(p)))
  {
    return true;
  }

  return false;
}

inline bool StaticSlotPool::release(Ptr<Slot> p, Uint32 slot_size)
{
  const Uint32 slot_count = m_slot_count;
  LocalSlotPool<StaticSlotPool> pool(this, slot_size);
  Slot_list free_list(pool, m_free_list);
  if (likely(p.i < slot_count))
  {
    free_list.addFirst(p);
    return true;
  }
  return false;
}

inline Slot* StaticSlotPool::getPtr(Uint32 i, Uint32 slot_size) const
{
  const Uint32 slot_count = m_slot_count;
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  Uint32 page_number = i / slots_per_page;
  Uint32 page_index = i % slots_per_page;
  Page* page = m_page_base + page_number;

  if (unlikely(i >= slot_count))
  {
    return NULL;
  }

  Slot* p = reinterpret_cast<Slot*>(&page->m_data[page_index * slot_size]);
  if (unlikely(!Magic::match(p->m_magic, Slot::TYPE_ID)))
  {
    g_eventLogger->info("Magic::match failed in %s: "
                        "type_id %08x rg %u tid %u: "
                        "slot_size %u: ptr.i %u: ptr.p %p: "
                        "magic %08x expected %08x",
                        __func__,
                        Slot::TYPE_ID,
                        GET_RG(Slot::TYPE_ID),
                        GET_TID(Slot::TYPE_ID),
                        slot_size,
                        page_index,
                        p,
                        p->m_magic,
                        Magic::make(Slot::TYPE_ID));
    require(Magic::match(p->m_magic, Slot::TYPE_ID));
  }
  return p;
}

inline void StaticSlotPool::getPtr(Ptr<Slot>& p, Uint32 slot_size) const
{
  p.p = getPtr(p.i, slot_size);
}

inline bool StaticSlotPool::getValidPtr(Ptr<Slot>& p,
                                        Uint32 magic,
                                        Uint32 slot_size) const
{
  const Uint32 slot_count = m_slot_count;
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  Uint32 page_index = p.i % slots_per_page;
  Uint32 page_number = p.i / slots_per_page;
  Page* page = m_page_base;

  if (unlikely(p.i >= slot_count))
  {
    return false;
  }

  page += page_number;
  p.p = reinterpret_cast<Slot*>(&page->m_data[page_index * slot_size]);
  if (unlikely(p.p->m_magic != magic))
  {
    return false;  // Bad magic
  }
  return true;
}

inline bool StaticSlotPool::getUncheckedPtrRO(Ptr<Slot>& p,
                                        Uint32 slot_size) const
{
  const Uint32 slot_count = m_slot_count;
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  Uint32 page_index = p.i % slots_per_page;
  Uint32 page_number = p.i / slots_per_page;
  Page* page = m_page_base;

  if (unlikely(p.i >= slot_count))
  {
    return false;
  }

  page += page_number;
  p.p = reinterpret_cast<Slot*>(&page->m_data[page_index * slot_size]);

  NDB_PREFETCH_READ(p.p);

  return true;
}

inline bool StaticSlotPool::getUncheckedPtrRW(Ptr<Slot>& p,
                                        Uint32 slot_size) const
{
  const Uint32 slot_count = m_slot_count;
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  Uint32 page_index = p.i % slots_per_page;
  Uint32 page_number = p.i / slots_per_page;
  Page* page = m_page_base;

  if (unlikely(p.i >= slot_count))
  {
    return false;
  }

  page += page_number;
  p.p = reinterpret_cast<Slot*>(&page->m_data[page_index * slot_size]);

  NDB_PREFETCH_WRITE(p.p);

  return true;
}

inline StaticSlotPool::StaticSlotPool() : m_page_base(NULL), m_slot_count(0)
{
  m_free_list.init();
}

inline void StaticSlotPool::free(Uint32 type_id, const Pool_context& pool_ctx)
{
  if (m_page_base == NULL)
  {
    require(m_slot_count == 0);
    return;
  }

  Ndbd_mem_manager* mem_manager = pool_ctx.get_mem_manager();
  const Uint32 page_number =
      m_page_base - reinterpret_cast<Page*>(mem_manager->get_memroot());
  mem_manager->release_pages(type_id, page_number, m_slot_count);
  m_free_list.init();
  m_page_base = NULL;
  m_slot_count = 0;
}

inline Uint32 StaticSlotPool::getUncheckedPtrs(Uint32* from,
                                               Ptr<Slot> ptrs[],
                                               Uint32 cnt,
                                               Uint32 slot_size) const
{
  const Uint32 slot_count = m_slot_count;
  Uint32 index = *from;
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  Uint32 page_number = index / slots_per_page;
  Uint32 page_index = index % slots_per_page;
  require(index != RNIL);

  Page* page = m_page_base + page_number;
  if (unlikely(index >= slot_count))
  {
    *from = RNIL;
    return 0;
  }

  Uint32 ptrs_cnt = 0;
  Uint32* slot_ptr = &page->m_data[page_index * slot_size];
  for (; ptrs_cnt < cnt && page_index < slots_per_page;
       page_index++, index++, ptrs_cnt++, slot_ptr += slot_size)
  {
    ptrs[ptrs_cnt].i = index;
    ptrs[ptrs_cnt].p = reinterpret_cast<Slot*>(slot_ptr);
  }
  if (unlikely(page_index == slots_per_page))
  {
    if (unlikely(index >= slot_count))
    {
      index = RNIL;
    }
  }
  *from = index;
  return ptrs_cnt;
}

inline Uint64 StaticSlotPool::getMemoryNeed(Uint32 slot_size,
                                            Uint32 entry_count)
{
  const Uint32 slots_per_page = Page::WORDS_PER_PAGE / slot_size;
  const Uint32 pages = (entry_count + slots_per_page - 1) / slots_per_page;
  return pages * sizeof(Page);
}

inline bool StaticSlotPool::may_shrink() const { return false; }

inline bool StaticSlotPool::rearrange_free_list_and_shrink(
    Uint32* max_shrinks, Uint32 /* slot_size */)
{
  *max_shrinks = 0;
  return false;
}

#undef JAM_FILE_ID

#endif
