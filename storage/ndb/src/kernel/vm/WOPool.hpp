/*
   Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef WOPOOL_HPP
#define WOPOOL_HPP

#include "Pool.hpp"

#define JAM_FILE_ID 303


struct WOPage
{
  STATIC_CONST( WOPAGE_WORDS = GLOBAL_PAGE_SIZE_WORDS - 2 );

  Uint32 m_type_id;
  Uint32 m_ref_count;
  Uint32 m_data[WOPAGE_WORDS];
};

/**
 * Write Once Pool
 */
struct WOPool
{
  Record_info m_record_info;
  WOPage* m_memroot;
  WOPage* m_current_page;
  Pool_context m_ctx;
  Uint32 m_current_page_no;
  Uint16 m_current_pos;
  Uint16 m_current_ref_count;
public:
  WOPool();
  
  void init(const Record_info& ri, const Pool_context& pc);
  bool seize(Ptr<void>&);
  void release(Ptr<void>);
  void * getPtr(Uint32 i);
  
private:  
  void seize_in_page(Ptr<void>&);
  bool seize_new_page(Ptr<void>&);
  void release_not_current(Ptr<void>);

  void handle_invalid_release(Ptr<void>) ATTRIBUTE_NORETURN;
  void handle_invalid_get_ptr(Uint32 i) ATTRIBUTE_NORETURN;
  void handle_inconsistent_release(Ptr<void>) ATTRIBUTE_NORETURN;
};

inline
void
WOPool::seize_in_page(Ptr<void>& ptr)
{
  Uint32 pos = m_current_pos;
  WOPage *pageP = m_current_page;
  Uint32 magic_pos = pos + m_record_info.m_offset_magic;
  Uint32 type_id = ~(Uint32)m_record_info.m_type_id;
  Uint32 size = m_record_info.m_size;
  Uint16 ref_count = m_current_ref_count;

  assert(pos + size < WOPage::WOPAGE_WORDS);
  ptr.i = (m_current_page_no << POOL_RECORD_BITS) + pos;
  ptr.p = (pageP->m_data + pos);
  pageP->m_data[magic_pos] = type_id;
  m_current_pos = pos + size;
  m_current_ref_count = ref_count + 1;
}

inline
bool
WOPool::seize(Ptr<void>& ptr)
{
  if (likely(m_current_pos + m_record_info.m_size < WOPage::WOPAGE_WORDS))
  {
    seize_in_page(ptr);
    return true;
  }
  return seize_new_page(ptr);
}

inline
void
WOPool::release(Ptr<void> ptr)
{
  Uint32 cur_page = m_current_page_no;
  Uint32 ptr_page = ptr.i >> POOL_RECORD_BITS;
  Uint32 *magic_ptr = (((Uint32*)ptr.p)+m_record_info.m_offset_magic);
  Uint32 magic_val = *magic_ptr;
  
  if (likely(magic_val == ~(Uint32)m_record_info.m_type_id))
  {
    * magic_ptr = 0;
    if (cur_page == ptr_page)
    {
      if (m_current_ref_count == 1)
      {
	m_current_pos = 0;
      }
      m_current_ref_count--;
      return;
    }
    return release_not_current(ptr);
  }
  handle_invalid_release(ptr);
}

inline
void*
WOPool::getPtr(Uint32 i)
{
  Uint32 page_no = i >> POOL_RECORD_BITS;
  Uint32 page_idx = i & POOL_RECORD_MASK;
  WOPage * page = m_memroot + page_no;
  Uint32 * record = page->m_data + page_idx;
  Uint32 magic_val = * (record + m_record_info.m_offset_magic);
  if (likely(magic_val == ~(Uint32)m_record_info.m_type_id))
  {
    return record;
  }
  handle_invalid_get_ptr(i);
  return 0;                                     /* purify: deadcode */
}


#undef JAM_FILE_ID

#endif
