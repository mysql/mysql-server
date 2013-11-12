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

#ifndef RWPOOL_HPP
#define RWPOOL_HPP

#include "Pool.hpp"

#define JAM_FILE_ID 311


struct RWPage
{
  STATIC_CONST( RWPAGE_WORDS = GLOBAL_PAGE_SIZE_WORDS - 4 );

  Uint32 m_type_id;
  Uint16 m_first_free;
  Uint16 m_ref_count;
  Uint32 m_next_page;
  Uint32 m_prev_page;
  Uint32 m_data[RWPAGE_WORDS];
};

/**
 * Read Write  Pool
 */
struct RWPool
{
  Record_info m_record_info;
  RWPage* m_memroot;
  RWPage* m_current_page;
  Pool_context m_ctx;
  Uint32 m_first_free_page;
  Uint32 m_current_page_no;
  Uint16 m_current_pos;
  Uint16 m_current_first_free;
  Uint16 m_current_ref_count;
public:
  RWPool();
  
  void init(const Record_info& ri, const Pool_context& pc);
  bool seize(Ptr<void>&);
  void release(Ptr<void>);
  void * getPtr(Uint32 i);
  void * getPtr(const Record_info&ri, Uint32 i);
  
  STATIC_CONST( WORDS_PER_PAGE = RWPage::RWPAGE_WORDS );

private:  
  void handle_invalid_release(Ptr<void>) ATTRIBUTE_NORETURN;
  void handle_invalid_get_ptr(Uint32 i) ATTRIBUTE_NORETURN;
};

inline
void*
RWPool::getPtr(Uint32 i)
{
  Uint32 page_no = i >> POOL_RECORD_BITS;
  Uint32 page_idx = i & POOL_RECORD_MASK;
  RWPage * page = m_memroot + page_no;
  Uint32 * record = page->m_data + page_idx;
  Uint32 magic_val = * (record + m_record_info.m_offset_magic);
  if (likely(magic_val == ~(Uint32)m_record_info.m_type_id))
  {
    return record;
  }
  handle_invalid_get_ptr(i);
  return 0;                                     /* purify: deadcode */
}

inline
void*
RWPool::getPtr(const Record_info &ri, Uint32 i)
{
  Uint32 page_no = i >> POOL_RECORD_BITS;
  Uint32 page_idx = i & POOL_RECORD_MASK;
  RWPage * page = m_memroot + page_no;
  Uint32 * record = page->m_data + page_idx;
  Uint32 magic_val = * (record + ri.m_offset_magic);
  if (likely(magic_val == ~(Uint32)ri.m_type_id))
  {
    return record;
  }
  handle_invalid_get_ptr(i);
  return 0;                                     /* purify: deadcode */
}


#undef JAM_FILE_ID

#endif
