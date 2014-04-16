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

#include "RWPool.hpp"
#include <ndbd_exit_codes.h>
#include <NdbOut.hpp>

#define JAM_FILE_ID 278


#define REC_NIL GLOBAL_PAGE_SIZE_WORDS

RWPool::RWPool() 
{
  memset(this, 0, sizeof(* this));
  m_current_pos = RWPage::RWPAGE_WORDS;
  m_current_first_free = REC_NIL;
  m_first_free_page = RNIL;
}

void
RWPool::init(const Record_info& ri, const Pool_context& pc)
{
  m_ctx = pc;
  m_record_info = ri;
  m_record_info.m_size = ((ri.m_size + 3) >> 2); // Align to word boundary
  m_record_info.m_offset_magic = ((ri.m_offset_magic + 3) >> 2);
  m_record_info.m_offset_next_pool = ((ri.m_offset_next_pool + 3) >> 2);
  m_memroot = (RWPage*)m_ctx.get_memroot();
#ifdef VM_TRACE
  ndbout_c("RWPool::init(%x, %d)",ri.m_type_id, m_record_info.m_size);
#endif
}

bool
RWPool::seize(Ptr<void>& ptr)
{
  Uint32 pos = m_current_pos;
  Uint32 size = m_record_info.m_size;
  Uint32 off = m_record_info.m_offset_magic;
  RWPage *pageP = m_current_page;
  if (likely(m_current_first_free != REC_NIL))
  {
seize_free:
    pos = m_current_first_free;
    ptr.i = (m_current_page_no << POOL_RECORD_BITS) + pos;
    ptr.p = pageP->m_data + pos;
    pageP->m_data[pos+off] = ~(Uint32)m_record_info.m_type_id;
    m_current_ref_count++;
    m_current_first_free = pageP->m_data[pos+m_record_info.m_offset_next_pool];
    return true;
  }
  else if (pos + size < RWPage::RWPAGE_WORDS)
  {
seize_first:
    ptr.i = (m_current_page_no << POOL_RECORD_BITS) + pos;
    ptr.p = (pageP->m_data + pos);
    pageP->m_data[pos+off] = ~(Uint32)m_record_info.m_type_id;
    m_current_ref_count++;
    m_current_pos = pos + size;
    return true;
  }

  if (m_current_page)
  {
    m_current_page->m_first_free = REC_NIL;
    m_current_page->m_next_page = RNIL;
    m_current_page->m_prev_page = RNIL;
    m_current_page->m_type_id = m_record_info.m_type_id;
    m_current_page->m_ref_count = m_current_ref_count;
  }

  if (m_first_free_page != RNIL)
  {
    pageP = m_current_page = m_memroot + m_first_free_page;
    m_current_page_no = m_first_free_page;
    m_current_pos = RWPage::RWPAGE_WORDS;
    m_current_first_free = m_current_page->m_first_free;
    m_first_free_page = m_current_page->m_next_page;
    m_current_ref_count = m_current_page->m_ref_count;
    if (m_first_free_page != RNIL)
    {
      (m_memroot + m_first_free_page)->m_prev_page = RNIL;
    }
    goto seize_free;
  }

  m_current_ref_count = 0;
  
  RWPage* page;
  Uint32 page_no = RNIL;
  if ((page = (RWPage*)m_ctx.alloc_page(m_record_info.m_type_id, &page_no)))
  {
    pos = 0;
    m_current_page_no = page_no;
    pageP = m_current_page = page;
    m_current_first_free = REC_NIL;
    page->m_type_id = m_record_info.m_type_id;
    goto seize_first;
  }

  m_current_page = 0;
  m_current_page_no = RNIL;
  m_current_pos = RWPage::RWPAGE_WORDS;
  m_current_first_free = REC_NIL;
  
  return false;
}

void
RWPool::release(Ptr<void> ptr)
{
  Uint32 cur_page = m_current_page_no;
  Uint32 ptr_page = ptr.i >> POOL_RECORD_BITS;
  Uint32 *record_ptr = (Uint32*)ptr.p;
  Uint32 magic_val = * (record_ptr + m_record_info.m_offset_magic);
  
  if (likely(magic_val == ~(Uint32)m_record_info.m_type_id))
  {
    * (record_ptr + m_record_info.m_offset_magic) = 0;
    if (cur_page == ptr_page)
    {
      * (record_ptr + m_record_info.m_offset_next_pool) = m_current_first_free;
      assert(m_current_ref_count);
      m_current_ref_count--;
      m_current_first_free = ptr.i & POOL_RECORD_MASK;
      return;
    }

    // Cache miss on page...
    RWPage* page = m_memroot + ptr_page;
    Uint32 ref_cnt = page->m_ref_count;
    Uint32 ff = page->m_first_free;

    * (record_ptr + m_record_info.m_offset_next_pool) = ff;
    page->m_first_free = ptr.i & POOL_RECORD_MASK;
    page->m_ref_count = ref_cnt - 1;
    
    if (ff == REC_NIL)
    {
      /**
       * It was full...add to free page list
       */
      Uint32 ffp = m_first_free_page;
      if (ffp != RNIL)
      {
	RWPage* next = (m_memroot + ffp);
	assert(next->m_prev_page == RNIL);
	next->m_prev_page = ptr_page;
      }
      page->m_next_page = ffp;
      page->m_prev_page = RNIL;
      m_first_free_page = ptr_page;
      return;
    }
    else if(ref_cnt == 1)
    {
      /**
       * It's now empty...release it
       */
      Uint32 prev = page->m_prev_page;
      Uint32 next = page->m_next_page;
      if (prev != RNIL)
      {
	(m_memroot + prev)->m_next_page = next;
      }
      else
      {
	assert(m_first_free_page == ptr_page);
	m_first_free_page = next;
      }
      
      if (next != RNIL)
      {
	(m_memroot + next)->m_prev_page = prev;
      }
      m_ctx.release_page(m_record_info.m_type_id, ptr_page);
      return;
    }
    return;
  }
  handle_invalid_release(ptr);
}

void
RWPool::handle_invalid_release(Ptr<void> ptr)
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
RWPool::handle_invalid_get_ptr(Uint32 ptrI)
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
