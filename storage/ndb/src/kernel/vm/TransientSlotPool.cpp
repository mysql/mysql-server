/*
   Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "record_types.hpp"
#include "IntrusiveList.hpp"
#include "TransientPagePool.hpp"
#include "TransientSlotPool.hpp"

#define JAM_FILE_ID 505

TransientSlotPool::TransientSlotPool()
: m_page_pool(NULL),
  m_type_id(0),
//  m_slot_size(0),
  m_use_count(0),
//  m_use_high(0),
  m_may_shrink(false)//,
//  m_shrink_level(0),
//  m_static_initialized_slots(0)
{
  m_free_list.init();
}


void TransientSlotPool::init(Uint32 type_id,
                             Uint32 slot_size,
                             Uint32* min_recs,
                             const Pool_context& pool_ctx)
{
//  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;

  m_page_pool = new TransientPagePool(type_id,
                                      pool_ctx.get_mem_manager());
  m_type_id = type_id;
*min_recs = 0;
  // m_slot_size = slot_size;
  // m_shrink_level = m_static.getSize() + (slots_per_page + 1) / 2;
  // require(expand());
}


TransientSlotPool::~TransientSlotPool()
{
  if (m_page_pool)
  {
    delete m_page_pool;
  }
}


bool TransientSlotPool::expand(Uint32 slot_size)
{
  assert(m_free_list.isEmpty());
  Ptr<TransientPagePool::Page> lpage;
  if (unlikely(!m_page_pool->seize(lpage)))
  {
    return false;
  }
  Ptr<Page> page;
  page.i = lpage.i;
  page.p = reinterpret_cast<Page*>(lpage.p);

  page.p->m_use_count = 0;
  m_may_shrink = m_page_pool->getTopPageNumber() > 0;

  // Add first slot to free list
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  LocalSlotPool<TransientSlotPool> pool(this, slot_size);
  Ptr<Type> free_record;
  void *pv = static_cast<void*>(&page.p->m_data[0]);
  free_record.p = new (pv) Type;
  free_record.i = slots_per_page * page.i;
  Slot_list free_list(pool, m_free_list);
  free_list.addLast(free_record);
  page.p->m_first_in_free_array = 1;

  return true;
}

Uint64 TransientSlotPool::getMemoryNeed(Uint32 slot_size, Uint32 entry_count)
{
  const Uint64 entries_per_page = (Page::DATA_WORDS_PER_PAGE / slot_size);
  const Uint64 data_pages =
      ((entry_count + entries_per_page - 1) / entries_per_page);
  return data_pages * sizeof(Page) + TransientPagePool::getMemoryNeed(data_pages);
}

Uint32 TransientSlotPool::getUncheckedPtrs(Uint32* from,
                                           Ptr<Type> ptrs[],
                                           Uint32 cnt,
                                           Uint32 slot_size) const
{
  Uint32 index = *from;
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  Uint32 page_number = index / slots_per_page;
  Uint32 page_index = index % slots_per_page;
  require(index != RNIL);

  Ptr<TransientPagePool::Page> lpage;
  lpage.i = page_number;
  if (unlikely(!m_page_pool->getUncheckedPtr(lpage)))
  {
    Uint32 top_page = m_page_pool->getTopPageNumber();
    if (top_page == RNIL || page_number >= top_page)
    {
      index = RNIL;
    }
    else
    {
      index = index - page_index + slots_per_page;
    }
    *from = index;
    return 0;
  }
  Page* page = reinterpret_cast<Page*>(lpage.p);
  const Uint32 end_index = page->m_first_in_free_array;
  Uint32 *slot_ptr = &page->m_data[page_index * slot_size];
  Uint32 ptrs_cnt = 0;
  for (; ptrs_cnt < cnt && page_index < end_index;
         page_index++, index++, ptrs_cnt++, slot_ptr += slot_size)
  {
    ptrs[ptrs_cnt].i = index;
    ptrs[ptrs_cnt].p = reinterpret_cast<Type*>(slot_ptr);
  }
  if (unlikely(page_index >= end_index))
  {
    Uint32 top_page = m_page_pool->getTopPageNumber();
    require(top_page != RNIL);
    if (page_number == top_page)
    {
      index = RNIL;
    }
    else if (page_index < slots_per_page)
    {
      require(page_number < top_page);
      require(page_index == end_index);
      index = index - page_index + slots_per_page;
    }
    else
    {
      require(page_number < top_page);
      require(end_index == slots_per_page);
    }
  }
  *from = index;
  return ptrs_cnt;
}

bool TransientSlotPool::rearrange_free_list_and_shrink(Uint32* max_shrinks, Uint32 slot_size)
{
  Uint32 free = getNoOfFree();
  if (free > 8)
  {
    free = 8;
  }
  if (free > 0)
  {
    Ptr<Type> ptr[8];
    Uint32 j = 0;
    for (Uint32 i = 0; i < free; i++)
    {
      if (likely(seize(ptr[j], slot_size)))
      {
        j++;
      }
    }
    while (j > 0)
    {
      j--;
      release(ptr[j], slot_size);
    }
    free--;
  }
  for (Uint32 shrink_count = 0; shrink_count < *max_shrinks; shrink_count++)
  {
    if (!shrink(slot_size))
    {
*max_shrinks = shrink_count;
      return false;
    }
  }
  return true;
}

bool TransientSlotPool::shrink(Uint32 slot_size)
{
  if(!may_shrink())
  {
    return false;
  }
  Uint32 page_number = m_page_pool->getTopPageNumber();
  require(m_page_pool->canRelease(page_number));
  Ptr<TransientPagePool::Page> lpage;
  lpage.i = page_number;
  require(m_page_pool->getPtr(lpage));
  Page* page = reinterpret_cast<Page*>(lpage.p);
  require(page->m_use_count == 0);

  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  Uint32 base_index = slots_per_page * lpage.i;

  {
    LocalSlotPool<TransientSlotPool> pool(this, slot_size);
    Slot_list free_list(pool, m_free_list);
    const Uint32 end_index = page->m_first_in_free_array;
    for (unsigned i = 0; i < end_index; i++)
    {
      Ptr<Type> p;
      p.p = reinterpret_cast<Type*>(&page->m_data[i * slot_size]);
      p.i = base_index + i;
      free_list.remove(p);
    }
  }
  require(m_page_pool->release(lpage));

  lpage.i = m_page_pool->getTopPageNumber();
  if (!m_page_pool->canRelease(lpage.i))
  {
    m_may_shrink = false;
    return false;
  }
  require(m_page_pool->getPtr(lpage));

  page = reinterpret_cast<Page*>(lpage.p);
  if (page->m_use_count != 0)
  {
    m_may_shrink = false;
    return false;
  }
  return true;
}

#if !INLINE_TRANSIENT_SLOT_POOL
#define inline
#include "TransientSlotPool.inline.hpp.inc"
#endif
