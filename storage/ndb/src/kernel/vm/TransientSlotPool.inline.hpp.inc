/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifdef TRANSSLOTPOOL_INLINE_HPP_INC
#error TransientSlotPool.inline.hpp.inc included twice ?!
#endif

#define TRANSSLOTPOOL_INLINE_HPP_INC

#define SAVE_JAM_FILE_ID JAM_FILE_ID
//#undef JAM_FILE_ID

//#define JAM_FILE_ID 503

/**
 * Inline implementation
 */

inline
TransientSlotPool::Page*
TransientSlotPool::get_page_from_slot(Ptr<Type> p, Uint32 slot_size) const
{
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  const Uint32 page_index = p.i % slots_per_page;
  const size_t offset =
      Page::DATA_BYTE_OFFSET + page_index * slot_size * sizeof(Uint32);
  return reinterpret_cast<Page*>(reinterpret_cast<char*>(p.p) - offset);
}

inline bool TransientSlotPool::may_shrink() const
{
  return m_may_shrink;
}

inline bool TransientSlotPool::seize(Ptr<Type> &p, Uint32 slot_size)
{
  const Uint32 use_count = m_use_count;
  const Uint32 new_use_count = use_count + 1;
  if (m_free_list.isEmpty())
  {
    if (unlikely(!expand(slot_size)))
    {
      return false;
    }
  }

  LocalSlotPool<TransientSlotPool> pool(this, slot_size);
  Slot_list free_list(pool, m_free_list);
  require(free_list.removeFirst(p));
  m_use_count = new_use_count;
#if REARRANGE_ON_SEIZE
  if (p.i > new_use_count)
  {
     Ptr<Type> q;
    if (free_list.removeFirst(q))
    {
      if (q.i < p.i)
      {
        free_list.addLast(p);
        p = q;
      }
      else
      {
        free_list.addLast(q);
      }
    }
  }
#endif
  Page* page = get_page_from_slot(p, slot_size);
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  if (m_may_shrink &&
      page->m_use_count == 0 &&
      p.i >= slots_per_page * m_page_pool->getTopPageNumber())
  {
    m_may_shrink = false;
  }
  page->m_use_count++;
  const Uint32 page_use_count = page->m_use_count;
  if (page_use_count < slots_per_page &&
      page_use_count == page->m_first_in_free_array)
  {
    Ptr<Type> free_record;
    void *pv = static_cast<void*>(&page->m_data[page_use_count * slot_size]);
    free_record.p = new (pv) Type;
    const Uint32 pagei = p.i / slots_per_page;
    free_record.i = slots_per_page * pagei + page_use_count;
    free_list.addLast(free_record);
    page->m_first_in_free_array = page_use_count + 1;
  }
  return true;
}


inline void TransientSlotPool::release(Ptr<Type> p, Uint32 slot_size)
{
  const Uint32 use_count = m_use_count;
  const Uint32 new_use_count = use_count - 1;

  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  LocalSlotPool<TransientSlotPool> pool(this, slot_size);
  Slot_list free_list(pool, m_free_list);
  if (p.i <= use_count)
  {
    free_list.addFirst(p);
  } else {
    free_list.addLast(p);
  }
  m_use_count = new_use_count;
  Page* page = get_page_from_slot(p, slot_size);
  page->m_use_count--;
  // Below is not needed for fast records
  if (!m_may_shrink && page->m_use_count == 0)
  {
    Ptr<TransientPagePool::Page> lpage;
    lpage.i = p.i / slots_per_page;
    lpage.p = reinterpret_cast<TransientPagePool::Page*>(page);
    if (m_page_pool->canRelease(lpage.i))
    {
      m_may_shrink = true;
/* TODO(wl9756) move to TransientFastSlotPool
      if (getNoOfFree() > 4 * slots_per_page +  m_shrink_level)
      {
        shrink(slot_size);
        shrink(slot_size);
      }
*/
    }
  }
}

inline
TransientSlotPool::Type*
TransientSlotPool::getPtr(Uint32 i, Uint32 slot_size) const
{
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  Uint32 page_number = i / slots_per_page;
  Uint32 page_index = i % slots_per_page;

  Page* page;
  Ptr<TransientPagePool::Page> lpage;
  lpage.i = page_number;
  require(m_page_pool->getUncheckedPtr(lpage));
  page = reinterpret_cast<Page*>(lpage.p);

  Type* p = reinterpret_cast<Type*>(&page->m_data[page_index * slot_size]);
  require(Magic::match(p->m_magic, Type::TYPE_ID));
  return p;
}

inline void TransientSlotPool::getPtr(Ptr<Type>& p, Uint32 slot_size) const
{
  p.p = getPtr(p.i, slot_size);
}

inline
bool
TransientSlotPool::getValidPtr(Ptr<Type> &p, Uint32 magic, Uint32 slot_size) const
{
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  const Uint32 page_number = p.i / slots_per_page;
  const Uint32 page_index = p.i % slots_per_page;

  Ptr<TransientPagePool::Page> lpage;
  lpage.i = page_number;
  if (unlikely(!m_page_pool->getUncheckedPtr(lpage)))
  {
    return false;
  }
  Page* page = reinterpret_cast<Page*>(lpage.p);

  p.p = reinterpret_cast<Type*>(
            &page->m_data[page_index * slot_size]);
  if (unlikely(p.p->m_magic != magic))
  {
    return false;
  }
  return true;
}

inline
bool
TransientSlotPool::getUncheckedPtrRO(Ptr<Type> &p, Uint32 slot_size) const
{
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  const Uint32 page_number = p.i / slots_per_page;
  const Uint32 page_index = p.i % slots_per_page;

  Ptr<TransientPagePool::Page> lpage;
  lpage.i = page_number;
  if (unlikely(!m_page_pool->getUncheckedPtr(lpage)))
  {
    return false;
  }
  Page* page = reinterpret_cast<Page*>(lpage.p);

  p.p = reinterpret_cast<Type*>(
            &page->m_data[page_index * slot_size]);

  NDB_PREFETCH_READ(p.p);
  return true;
}

inline
bool
TransientSlotPool::getUncheckedPtrRW(Ptr<Type> &p, Uint32 slot_size) const
{
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  const Uint32 page_number = p.i / slots_per_page;
  const Uint32 page_index = p.i % slots_per_page;

  Ptr<TransientPagePool::Page> lpage;
  lpage.i = page_number;
  if (unlikely(!m_page_pool->getUncheckedPtr(lpage)))
  {
    return false;
  }
  Page* page = reinterpret_cast<Page*>(lpage.p);

  p.p = reinterpret_cast<Type*>(
            &page->m_data[page_index * slot_size]);

  NDB_PREFETCH_WRITE(p.p);
  return true;
}

// #undef JAM_FILE_ID
// #define JAM_FILE_ID SAVE_JAM_FILE_ID
