/*
   Copyright (c) 2006, 2023, Oracle and/or its affiliates.

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

#ifndef WOPOOL_HPP
#define WOPOOL_HPP

#include "Pool.hpp"

#define JAM_FILE_ID 303


struct WOPage
{
  static constexpr Uint32 WOPAGE_WORDS = GLOBAL_PAGE_SIZE_WORDS - 2;

  Uint32 m_type_id;
  Uint32 m_ref_count;
  Uint32 m_data[WOPAGE_WORDS];
};

/**
 * Write Once Pool
 */
template<typename T>
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
  typedef T Type;
  WOPool();
  
  void init(const Record_info& ri, const Pool_context& pc);
  bool seize(Ptr<T>&);
  void release(Ptr<T>);
  void * getPtr(Uint32 i) const;
  
private:  
  void seize_in_page(Ptr<T>&);
  bool seize_new_page(Ptr<T>&);
  void release_not_current(Ptr<T>);

  [[noreturn]] void handle_invalid_release(Ptr<T>);
  [[noreturn]] void handle_invalid_get_ptr(Uint32 i) const;
  [[noreturn]] void handle_inconsistent_release(Ptr<T>);
};

template<typename T>
inline
void
WOPool<T>::seize_in_page(Ptr<T>& ptr)
{
  Uint32 pos = m_current_pos;
  WOPage *pageP = m_current_page;
  Uint32 magic_pos = pos + m_record_info.m_offset_magic;
  Uint32 type_id = ~(Uint32)m_record_info.m_type_id;
  Uint32 size = m_record_info.m_size;
  Uint16 ref_count = m_current_ref_count;

  assert(pos + size < WOPage::WOPAGE_WORDS);
  ptr.i = (m_current_page_no << POOL_RECORD_BITS) + pos;
  Uint32* const p = (pageP->m_data + pos);
  ptr.p = reinterpret_cast<T*>(p); // TODO dynamic_cast?
  pageP->m_data[magic_pos] = type_id;
  m_current_pos = pos + size;
  m_current_ref_count = ref_count + 1;
}

template<typename T>
inline
bool
WOPool<T>::seize(Ptr<T>& ptr)
{
  if (likely(m_current_pos + m_record_info.m_size < WOPage::WOPAGE_WORDS))
  {
    seize_in_page(ptr);
    return true;
  }
  return seize_new_page(ptr);
}

template<typename T>
inline
void
WOPool<T>::release(Ptr<T> ptr)
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

template<typename T>
inline
void*
WOPool<T>::getPtr(Uint32 i) const
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

#include "WOPool.cpp"

#undef JAM_FILE_ID

#endif
