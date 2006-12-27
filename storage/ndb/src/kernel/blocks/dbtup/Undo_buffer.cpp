/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "Undo_buffer.hpp"
#define DBTUP_C
#include "Dbtup.hpp"

#if ZPAGE_STATE_POS != 0
#error "PROBLEM!"
#endif

struct UndoPage
{
  File_formats::Page_header m_page_header;
  Uint32 _tupdata1;
  Uint32 m_state; // Used by buddy alg
  Uint32 m_words_used;
  Uint32 m_ref_count;
  Uint32 m_data[GLOBAL_PAGE_SIZE_WORDS-4-(sizeof(File_formats::Page_header)>>2)];
  
  STATIC_CONST( DATA_WORDS = GLOBAL_PAGE_SIZE_WORDS-4-(sizeof(File_formats::Page_header)>>2) );
};

Undo_buffer::Undo_buffer(Dbtup* tup)
{
  m_tup= tup;
  m_first_free= RNIL;
}

Uint32 *
Undo_buffer::alloc_copy_tuple(Local_key* dst, Uint32 words)
{
  UndoPage* page;
  assert(words);
  if(m_first_free == RNIL)
  {
    Uint32 count;
    m_tup->allocConsPages(1, count, m_first_free);
    if(count == 0)
      return 0;
    page= (UndoPage*)m_tup->c_page_pool.getPtr(m_first_free);
    page->m_state= ~ZFREE_COMMON;
    page->m_words_used= 0;
    page->m_ref_count= 0;
  }
  
  page= (UndoPage*)m_tup->c_page_pool.getPtr(m_first_free);
  
  Uint32 pos= page->m_words_used;
  if(words + pos > UndoPage::DATA_WORDS)
  {
    m_first_free= RNIL;
    return alloc_copy_tuple(dst, words);
  }
  
  dst->m_page_no = m_first_free;
  dst->m_page_idx = pos;
  
  page->m_ref_count++;
  page->m_words_used = pos + words;
  return page->m_data + pos;
}

void
Undo_buffer::shrink_copy_tuple(Local_key* key, Uint32 words)
{
  assert(key->m_page_no == m_first_free);
  UndoPage* page= (UndoPage*)m_tup->c_page_pool.getPtr(key->m_page_no); 
  assert(page->m_words_used >= words);
  page->m_words_used -= words;
}

void
Undo_buffer::free_copy_tuple(Local_key* key)
{
  UndoPage* page= (UndoPage*)m_tup->c_page_pool.getPtr(key->m_page_no);
  Uint32 cnt= page->m_ref_count;
  assert(cnt);

  page->m_ref_count= cnt - 1;
  
  if(cnt - 1 == 0)
  {
    page->m_words_used= 0;
    if(m_first_free == key->m_page_no)
    {
      //ndbout_c("resetting page");
    }
    else 
    {
      //ndbout_c("returning page");
      m_tup->returnCommonArea(key->m_page_no, 1);
    }
  }
  key->setNull();
}

Uint32 *
Undo_buffer::get_ptr(Local_key* key)
{
  return ((UndoPage*)(m_tup->c_page_pool.getPtr(key->m_page_no)))->m_data+key->m_page_idx;
}
  
