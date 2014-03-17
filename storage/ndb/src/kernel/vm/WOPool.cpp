/*
   Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "WOPool.hpp"
#include <ndbd_exit_codes.h>
#include <NdbOut.hpp>

#define JAM_FILE_ID 294


WOPool::WOPool() 
{
  memset(this, 0, sizeof(* this));
  m_current_pos = WOPage::WOPAGE_WORDS;
}

void
WOPool::init(const Record_info& ri, const Pool_context& pc)
{
  m_ctx = pc;
  m_record_info = ri;
  m_record_info.m_size = ((ri.m_size + 3) >> 2); // Align to word boundary
  m_record_info.m_offset_magic = ((ri.m_offset_magic + 3) >> 2);
  m_memroot = (WOPage*)m_ctx.get_memroot();
#ifdef VM_TRACE
  ndbout_c("WOPool::init(%x, %d)",ri.m_type_id, m_record_info.m_size);
#endif
}

bool
WOPool::seize_new_page(Ptr<void>& ptr)
{
  WOPage* page;
  Uint32 page_no = RNIL;
  if ((page = (WOPage*)m_ctx.alloc_page(m_record_info.m_type_id, &page_no)))
  {
    if (m_current_page)
    {
      m_current_page->m_ref_count = m_current_ref_count;
    }
    
    m_current_pos = 0;
    m_current_ref_count = 0;
    m_current_page_no = page_no;
    m_current_page = page;
    page->m_type_id = m_record_info.m_type_id;
    seize_in_page(ptr);
    return true;
  }
  return false;
}

void
WOPool::release_not_current(Ptr<void> ptr)
{
  WOPage* page = (WOPage*)(UintPtr(ptr.p) & ~(GLOBAL_PAGE_SIZE - 1));
  Uint32 cnt = page->m_ref_count;
  Uint32 type = page->m_type_id;
  Uint32 ri_type = m_record_info.m_type_id;
  if (likely(cnt && type == ri_type))
  {
    if (cnt == 1)
    {
      m_ctx.release_page(ri_type, ptr.i >> POOL_RECORD_BITS);
      return;
    }
    page->m_ref_count = cnt - 1;
    return;
  }
  
  handle_inconsistent_release(ptr);
}

void
WOPool::handle_invalid_release(Ptr<void> ptr)
{
  char buf[255];

  Uint32 pos = ptr.i & POOL_RECORD_MASK;
  Uint32 pageI = ptr.i >> POOL_RECORD_BITS;
  Uint32 * record_ptr_p = (Uint32*)ptr.p;
  Uint32 * record_ptr_i = (m_memroot+pageI)->m_data + pos;
  
  Uint32 magic = * (record_ptr_p + m_record_info.m_offset_magic);
  BaseString::snprintf(buf, sizeof(buf),
	   "Invalid memory release: ptr (%x %p %p) magic: (%.8x %.8x) memroot: %p page: %x",
	   ptr.i, ptr.p, record_ptr_i, magic, m_record_info.m_type_id,
	   m_memroot,
	   (m_memroot+pageI)->m_type_id);
  
  m_ctx.handleAbort(NDBD_EXIT_PRGERR, buf);
}

void
WOPool::handle_invalid_get_ptr(Uint32 ptrI)
{
  char buf[255];

  Uint32 pos = ptrI & POOL_RECORD_MASK;
  Uint32 pageI = ptrI >> POOL_RECORD_BITS;
  Uint32 * record_ptr_i = (m_memroot+pageI)->m_data + pos;
  
  Uint32 magic = * (record_ptr_i + m_record_info.m_offset_magic);
  BaseString::snprintf(buf, sizeof(buf),
	   "Invalid memory access: ptr (%x %p) magic: (%.8x %.8x) memroot: %p page: %x",
	   ptrI, record_ptr_i, magic, m_record_info.m_type_id,
	   m_memroot,
	   (m_memroot+pageI)->m_type_id);
  
  m_ctx.handleAbort(NDBD_EXIT_PRGERR, buf);
}

void
WOPool::handle_inconsistent_release(Ptr<void> ptr)
{
  WOPage* page = (WOPage*)(UintPtr(ptr.p) & ~(GLOBAL_PAGE_SIZE - 1));
  Uint32 cnt = page->m_ref_count;
  Uint32 type = page->m_type_id;
  Uint32 ri_type = m_record_info.m_type_id;

  char buf[255];
  
  BaseString::snprintf(buf, sizeof(buf),
	   "Memory corruption: ptr (%x %p) page (%d %x %x)",
	   ptr.i, ptr.p, cnt, type, ri_type);
  
  m_ctx.handleAbort(NDBD_EXIT_PRGERR, buf);
}
