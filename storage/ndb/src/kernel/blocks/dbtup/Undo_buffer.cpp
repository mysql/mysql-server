/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "Undo_buffer.hpp"
#define DBTUP_C
#include "Dbtup.hpp"

#define JAM_FILE_ID 429


#if ZPAGE_STATE_POS != 0
#error "PROBLEM!"
#endif

struct UndoPage
{
  Uint32 m_words_used;
  Uint32 m_ref_count;
  Uint32 m_data[GLOBAL_PAGE_SIZE_WORDS-2];
  
  STATIC_CONST( DATA_WORDS = GLOBAL_PAGE_SIZE_WORDS-2 );
};

#if defined VM_TRACE || defined ERROR_INSERT
#define SAFE_UB
#endif

static
inline
UndoPage* 
get_page(Ndbd_mem_manager* mm, Uint32 no)
{
  return ((UndoPage*)mm->get_memroot()) + no;
}

Undo_buffer::Undo_buffer(Ndbd_mem_manager* mm)
{
  m_mm = mm;
  m_first_free = RNIL;
  assert(sizeof(UndoPage) == 4*GLOBAL_PAGE_SIZE_WORDS);
}

Uint32 *
Undo_buffer::alloc_copy_tuple(Local_key* dst, Uint32 words)
{
  UndoPage* page;
  assert(words);
#ifdef SAFE_UB
  words += 2; // header + footer
#endif
  if (m_first_free == RNIL)
  {
    page= (UndoPage*)m_mm->alloc_page(RG_DATAMEM, 
                                      &m_first_free,
                                      Ndbd_mem_manager::NDB_ZONE_ANY);
    if(page == 0)
      return 0;
    page->m_words_used= 0;
    page->m_ref_count= 0;
  }
  
  page = get_page(m_mm, m_first_free);
  Uint32 pos= page->m_words_used;

  if (words + pos > UndoPage::DATA_WORDS)
  {
    m_first_free= RNIL;
    return alloc_copy_tuple(dst, words);
  }
  
  dst->m_page_no = m_first_free;
  dst->m_page_idx = pos;
  
  page->m_ref_count++;
  page->m_words_used = pos + words;
#ifdef SAFE_UB
  page->m_data[pos] = words;    // header
  page->m_data[pos + words - 1] = m_first_free + pos;
  pos ++;
#endif
  return page->m_data + pos;
}

void
Undo_buffer::shrink_copy_tuple(Local_key* key, Uint32 words)
{
  assert(key->m_page_no == m_first_free);
  UndoPage* page= get_page(m_mm, key->m_page_no); 
  assert(page->m_words_used >= words);
  page->m_words_used -= words;
}

void
Undo_buffer::free_copy_tuple(Local_key* key)
{
  UndoPage* page= get_page(m_mm, key->m_page_no);
  Uint32 cnt= page->m_ref_count;
  assert(cnt);

  page->m_ref_count= cnt - 1;
  
  if (cnt - 1 == 0)
  {
    page->m_words_used= 0;
    if (m_first_free == key->m_page_no)
    {
      //ndbout_c("resetting page");
    }
    else 
    {
      //ndbout_c("returning page");
      m_mm->release_page(RG_DATAMEM, key->m_page_no);
    }
  }
  key->setNull();
}

Uint32 *
Undo_buffer::get_ptr(const Local_key* key)
{
  UndoPage* page = get_page(m_mm, key->m_page_no);
  Uint32 * ptr = page->m_data + key->m_page_idx;
#ifdef SAFE_UB
  Uint32 words = * ptr;
  Uint32 check = ptr[words - 1];
  if (unlikely(! ((check == key->m_page_no + key->m_page_idx))))
  {
    abort();
  }
  ptr++;
#endif
  return ptr;
}
