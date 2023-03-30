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

#ifndef RWPOOL_HPP
#define RWPOOL_HPP

#include "ndbd_exit_codes.h"
#include "NdbOut.hpp"
#include "Pool.hpp"

#include <EventLogger.hpp>

#define JAM_FILE_ID 311


struct RWPage
{
  static constexpr Uint32 RWPAGE_WORDS = GLOBAL_PAGE_SIZE_WORDS - 4;

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
template<typename T>
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
  typedef T Type;
  RWPool();
  
  void init(const Record_info& ri, const Pool_context& pc);
  bool seize(Ptr<T>&);
  void release(Ptr<T>);
  void * getPtr(Uint32 i) const;
  void * getPtr(const Record_info&ri, Uint32 i) const;
  
  static constexpr Uint32 WORDS_PER_PAGE = RWPage::RWPAGE_WORDS;

private:  
  [[noreturn]] void handle_invalid_release(Ptr<T>);
  [[noreturn]] void handle_invalid_get_ptr(Uint32 i) const;
};

template<typename T>
inline
void*
RWPool<T>::getPtr(Uint32 i) const
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

template<typename T>
inline
void*
RWPool<T>::getPtr(const Record_info &ri, Uint32 i) const
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

#include "RWPool.cpp"

#undef JAM_FILE_ID

#endif
