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

#include "vm/StaticSlotPool.hpp"

#include "my_compiler.h"
#include "ndb_global.h"
#include "ndb_types.h"
#include "vm/ndbd_malloc_impl.hpp"
#include "vm/Pool.hpp"
#include "vm/Slot.hpp"

#define JAM_FILE_ID 509

void StaticSlotPool::init(Uint32 type_id,
                          Uint32 slot_size,
                          Uint32* min_recs,
                          const Pool_context& pool_ctx)
{
  Ndbd_mem_manager* mem_manager = pool_ctx.get_mem_manager();
  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  const Uint32 min_pages = (*min_recs + slots_per_page - 1) / slots_per_page;

  Uint32 page_count = min_pages;
  Uint32 page_number;
  if (likely(page_count != 0))
  {
    mem_manager->alloc_pages(type_id,
                             &page_number,
                             &page_count,
                             1,
                             Ndbd_mem_manager::NDB_ZONE_LE_32);
  }

  if (unlikely(page_count == 0))
  {
    *min_recs = 0;
    return;
  }

  void* p = mem_manager->get_page(page_number);
  m_page_base = reinterpret_cast<Page*>(p);
  m_slot_count = page_count * slots_per_page;
  *min_recs = m_slot_count;
  for (Uint32 pagei = 0; pagei < page_count; pagei++)
  {
    Page* page = new (m_page_base + pagei) Page;
    page->m_magic = Magic::make(type_id);
    page->m_page_id = pagei;
  }
}

bool StaticSlotPool::startup(Uint32* initialized_slots_ptr,
                             Uint32 slot_size)
{
  const Uint32 initialized_slots = *initialized_slots_ptr;
  if (likely(initialized_slots == m_slot_count))
  {
    return false;
  }

  const Uint32 slots_per_page = Page::DATA_WORDS_PER_PAGE / slot_size;
  Ptr<Page> page;
  page.i = (m_slot_count - initialized_slots - 1) / slots_per_page;
  page.p = m_page_base + page.i;
  require(initialized_slots % slots_per_page == 0);

  LocalSlotPool<StaticSlotPool> pool(this, slot_size);
  Slot_list free_list(pool, m_free_list);
  Uint32 index = initialized_slots;
  for (Uint32 i = 0; i < slots_per_page; i++, index++)
  {
    void* pv = static_cast<void*>(&page.p->m_data[(slots_per_page - i - 1)* slot_size]);
    Ptr<Slot> free_record;
    free_record.p = new (pv) Slot;
    free_record.i = m_slot_count - index - 1;
    free_list.addFirst(free_record);
  }
  *initialized_slots_ptr = index;
  require(index <= m_slot_count);
  return true;
}
