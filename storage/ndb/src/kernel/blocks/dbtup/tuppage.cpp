/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include "tuppage.hpp"
#include "Dbtup.hpp"

Uint32
Tup_fixsize_page::alloc_record()
{
  assert(free_space);
  Uint32 page_idx = next_free_index;
  assert(page_idx + 1 < DATA_WORDS);

  Uint32 prev = m_data[page_idx] >> 16;
  Uint32 next = m_data[page_idx] & 0xFFFF;

  assert(prev == 0xFFFF);
  assert(m_data[page_idx + 1] == Dbtup::Tuple_header::FREE);
  
  m_data[page_idx + 1] = 0;
  if (next != 0xFFFF)
  {
    assert(free_space > 1);
    Uint32 nextP = m_data[next];
    assert((nextP >> 16) == page_idx);
    m_data[next] = 0xFFFF0000 | (nextP & 0xFFFF);
  }
  else
  {
    assert(free_space == 1);
  }
  
  next_free_index = next;
  free_space--;
  return page_idx;
}

Uint32
Tup_fixsize_page::alloc_record(Uint32 page_idx)
{
  assert(page_idx + 1 < DATA_WORDS);
  if (likely(free_space && m_data[page_idx + 1] == Dbtup::Tuple_header::FREE))
  {
    Uint32 prev = m_data[page_idx] >> 16;
    Uint32 next = m_data[page_idx] & 0xFFFF;
    
    assert(prev != 0xFFFF || (next_free_index == page_idx));
    if (prev == 0xFFFF)
    {
      next_free_index = next;
    }
    else
    {
      Uint32 prevP = m_data[prev];
      m_data[prev] = (prevP & 0xFFFF0000) | next;
    }

    if (next != 0xFFFF)
    {
      Uint32 nextP = m_data[next];
      m_data[next] = (prev << 16) | (nextP & 0xFFFF);
    }
    free_space --;
    m_data[page_idx + 1] = 0;
    return page_idx;
  }
  return ~0;
}

Uint32
Tup_fixsize_page::free_record(Uint32 page_idx)
{
  Uint32 next = next_free_index;
  
  assert(page_idx + 1 < DATA_WORDS);
  assert(m_data[page_idx + 1] != Dbtup::Tuple_header::FREE);

  if (next == 0xFFFF)
  {
    assert(free_space == 0);
  }
  else
  {
    assert(free_space);
    assert(next + 1 < DATA_WORDS);
    Uint32 nextP = m_data[next];
    assert((nextP >> 16) == 0xFFFF);
    m_data[next] = (page_idx << 16) | (nextP & 0xFFFF);
    assert(m_data[next + 1] == Dbtup::Tuple_header::FREE);
  }

  next_free_index = page_idx;
  m_data[page_idx] = 0xFFFF0000 | next;
  m_data[page_idx + 1] = Dbtup::Tuple_header::FREE;
  
  return ++free_space;
}

void
Tup_varsize_page::init()
{
  free_space= DATA_WORDS - 1;
  high_index= 1;
  insert_pos= 0;
  next_free_index= 0xFFFF;
  m_page_header.m_page_type = File_formats::PT_Tup_varsize_page;
}

Uint32
Tup_varsize_page::alloc_record(Uint32 alloc_size, 
			       Tup_varsize_page* temp, Uint32 chain)
{
  assert(free_space >= alloc_size);
  Uint32 largest_size= DATA_WORDS - (insert_pos + high_index);
  if (alloc_size >= largest_size) {
    /*
      We can't fit this segment between the insert position and the end of
      the index entries. We will pack the page so that all free space
      exists between the insert position and the end of the index entries.
    */
    reorg(temp);
    largest_size= DATA_WORDS - (insert_pos + high_index);
  }
  assert(largest_size > alloc_size);

  Uint32 page_idx;
  if (next_free_index == 0xFFFF) {
    /*
      We are out of free index slots. We will extend the array of free
      slots
    */
    page_idx= high_index++;
    free_space--;
  } else {
    // Pick an empty slot among the index entries
    page_idx= next_free_index;
    assert((get_index_word(page_idx) & 0xFFFF0000) == 0);
    next_free_index= get_index_word(page_idx);
  }

  assert(chain == 0 || chain == CHAIN);
  * get_index_ptr(page_idx) = insert_pos + ((chain + alloc_size) << 16);
  
  insert_pos += alloc_size;
  free_space -= alloc_size;
  //ndbout_c("%p->alloc_record(%d%s) -> %d", this,alloc_size, (chain ? " CHAIN" : ""),page_idx);
  return page_idx;
}
  
Uint32
Tup_varsize_page::free_record(Uint32 page_idx, Uint32 chain)
{
  //ndbout_c("%p->free_record(%d%s)", this, page_idx, (chain ? " CHAIN": ""));
  Uint32 *index_ptr= get_index_ptr(page_idx);
  Uint32 index_word= * index_ptr;
  Uint32 entry_pos= index_word & 0xFFFF;
  Uint32 entry_len= (index_word >> 16) & ~CHAIN;
  assert(chain == 0 || chain == CHAIN);
  assert(!(((index_word >> 16) ^ chain) & 0x8000));
#ifdef VM_TRACE
  memset(m_data + entry_pos, 0xF2, 4*entry_len);
#endif
  if (page_idx + 1 == high_index) {
    /*
      We are removing the last in the entry list. We could potentially
      have several free entries also before this. To take that into account
      we will rebuild the free list and thus compress it and update the
      free space accordingly.
    */
    rebuild_index(index_ptr);
  } else {
    * index_ptr= next_free_index;
    next_free_index= page_idx;
  }
  
  free_space+= entry_len;
  // If we're the "last" entry, decrease insert_pos
  insert_pos -= (entry_pos + entry_len == insert_pos ? entry_len : 0);
  
  return free_space;
}

void
Tup_varsize_page::rebuild_index(Uint32* index_ptr)
{
  Uint32 empty= 1;
  Uint32 *end= m_data + DATA_WORDS;

  /**
   * Scan until you find first non empty index pos
   */
  for(index_ptr++; index_ptr < end; index_ptr++)
    if((* index_ptr >> 16) == 0)
      empty++;
    else
      break;
  
  if(index_ptr == end)
  {
    // Totally free page
    high_index = 1;
    free_space += empty;
    next_free_index= 0xFFFF;
    return;
  }

  Uint32 next= 0xFFFF;
  high_index -= empty;
  for(index_ptr++; index_ptr < end; index_ptr++)
  {
    if((* index_ptr >> 16) == 0)
    {
      * index_ptr= next;
      next= (end - index_ptr);
    }
  }
  
  free_space += empty;
  next_free_index= next;
}

void
Tup_varsize_page::reorg(Tup_varsize_page* copy_page)
{
  Uint32 new_insert_pos= 0;
  Uint32 old_insert_pos= insert_pos;

  // Copy key data part of page to a temporary page.
  memcpy(copy_page->m_data, m_data, 4*old_insert_pos);
  assert(high_index > 0);
  Uint32* index_ptr= get_index_ptr(high_index-1);
  Uint32 *end_of_page= m_data + DATA_WORDS;
  for (; index_ptr < end_of_page; index_ptr++)
  {
    Uint32 index_word= * index_ptr;
    Uint32 entry_len= (index_word >> 16) & ~CHAIN;
    if (entry_len != 0) {
      /*
	We found an index item that needs to be packed. 
	We will update the index entry and copy the data to the page.
      */
      Uint32 entry_pos= index_word & 0xffff;
      assert(entry_pos + entry_len <= old_insert_pos);
      assert(new_insert_pos + entry_len <= old_insert_pos);
      * index_ptr= new_insert_pos + (index_word & 0xFFFF0000);
      memcpy(m_data+new_insert_pos, copy_page->m_data+entry_pos, 4*entry_len);
      
      new_insert_pos += entry_len;
    }
  }
  insert_pos= new_insert_pos;
}

NdbOut&
operator<< (NdbOut& out, const Tup_varsize_page& page)
{
  out << "[ Varpage " << &page << ": free: " << page.free_space
      << " (" << (page.DATA_WORDS - (page.insert_pos + page.high_index + 1)) << ")"
      << " insert_pos: " << page.insert_pos 
      << " high_index: " << page.high_index
      << " index: " << flush;
  
  const Uint32 *index_ptr= page.m_data+page.DATA_WORDS-1;
  for(Uint32 i = 1; i<page.high_index; i++, index_ptr--)
  {
    out << " [ " << i;
    if(*index_ptr >> 16)
      out << " pos: " << ((*index_ptr) & 0xFFFF)
	  << " len: " << ((*index_ptr >> 16) & ~page.CHAIN)
	  << (((* index_ptr >> 16) & page.CHAIN) ? " CHAIN " : " ") 
	  << "]" << flush;
    else
      out << " FREE ]" << flush;
  }
  
  out << " free list: " << flush;
  Uint32 next= page.next_free_index;
  while(next != 0xFFFF)
  {
    out << next << " " << flush;
    next= * (page.m_data+page.DATA_WORDS-next);
  }
  out << "]";
  return out;
}

NdbOut&
operator<< (NdbOut& out, const Tup_fixsize_page& page)
{
  out << "[ Fixpage " << &page 
      << ": frag_page: " << page.frag_page_id 
      << " page_no: " << page.m_page_no 
      << " file_no: " << page.m_file_no
      << " table: " << page.m_table_id
      << " fragment: " << page.m_fragment_id 
      << " uncommitted_used_space: " << page.uncommitted_used_space 
      << " free: " << page.free_space;
  
  out << " free list: " << hex << page.next_free_index << " " << flush;
  Uint32 startTuple = page.next_free_index >> 16;
  
#if 0
  Uint32 cnt = 0;
  Uint32 next= startTuple;
  while((next & 0xFFFF) != 0xFFFF)
  {
    cnt++;
    out << dec << "(" << (next & 0xFFFF) << " " << hex << next << ") " << flush;
    assert(page.m_data[(next & 0xFFFF) + 1] == Dbtup::Tuple_header::FREE);
    next= * (page.m_data + ( next & 0xFFFF ));
  }
  assert(cnt == page.free_space);
#endif
  out << "]";
  return out;
}
