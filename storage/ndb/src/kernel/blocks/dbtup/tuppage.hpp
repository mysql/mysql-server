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

#ifndef __NDB_TUP_PAGE_HPP
#define __NDB_TUP_PAGE_HPP

#include <ndb_types.h>
#include "../diskpage.hpp"

struct Tup_page 
{
  struct File_formats::Page_header m_page_header;
  Uint32 m_restart_seq;
  Uint32 page_state;
  union {
    Uint32 next_page;
    Uint32 nextList;
  };
  union {
    Uint32 prev_page;
    Uint32 prevList;
  };
  Uint32 first_cluster_page;
  Uint32 last_cluster_page;
  Uint32 next_cluster_page;
  Uint32 prev_cluster_page;
  Uint32 frag_page_id;
  Uint32 physical_page_id;
  Uint32 free_space;
  Uint32 next_free_index;
  Uint32 list_index; // free space in page bits/list, 0x8000 means not in free
  Uint32 uncommitted_used_space;
  Uint32 m_page_no;
  Uint32 m_file_no;
  Uint32 m_table_id;
  Uint32 m_fragment_id;
  Uint32 m_extent_no;
  Uint32 m_extent_info_ptr;
  Uint32 unused_ph[9];

  STATIC_CONST( DATA_WORDS = File_formats::NDB_PAGE_SIZE_WORDS - 32 );
  
  Uint32 m_data[DATA_WORDS];
};

struct Tup_fixsize_page
{
  struct File_formats::Page_header m_page_header;
  Uint32 m_restart_seq;
  Uint32 page_state;
  Uint32 next_page;
  Uint32 prev_page;
  Uint32 first_cluster_page;
  Uint32 last_cluster_page;
  Uint32 next_cluster_page;
  Uint32 prev_cluster_page;
  Uint32 frag_page_id;
  Uint32 physical_page_id;
  Uint32 free_space;
  Uint32 next_free_index;
  Uint32 list_index;
  Uint32 uncommitted_used_space;
  Uint32 m_page_no;
  Uint32 m_file_no;
  Uint32 m_table_id;
  Uint32 m_fragment_id;
  Uint32 m_extent_no;
  Uint32 m_extent_info_ptr;
  Uint32 unused_ph[9];

  STATIC_CONST( DATA_WORDS = File_formats::NDB_PAGE_SIZE_WORDS - 32 );
  
  Uint32 m_data[DATA_WORDS];
  
  Uint32* get_ptr(Uint32 page_idx, Uint32 rec_size){
    assert(page_idx + rec_size <= DATA_WORDS);
    return m_data + page_idx;
  }
  
  /**
   * Alloc record from page
   *   return page_idx
   **/
  Uint32 alloc_record();
  Uint32 alloc_record(Uint32 page_idx);
  Uint32 free_record(Uint32 page_idx);
};

struct Tup_varsize_page
{
  struct File_formats::Page_header m_page_header;
  Uint32 m_restart_seq;
  Uint32 page_state;
  Uint32 next_page;
  Uint32 prev_page;
  Uint32 first_cluster_page;
  Uint32 last_cluster_page;
  Uint32 next_cluster_page;
  Uint32 prev_cluster_page;
  Uint32 frag_page_id;
  Uint32 physical_page_id;
  Uint32 free_space;
  Uint32 next_free_index;
  Uint32 list_index;
  Uint32 uncommitted_used_space;
  Uint32 m_page_no;
  Uint32 m_file_no;
  Uint32 m_table_id;
  Uint32 m_fragment_id;
  Uint32 m_extent_no;
  Uint32 m_extent_info_ptr;
  Uint32 high_index; // size of index + 1
  Uint32 insert_pos;
  Uint32 unused_ph[7];
  
  STATIC_CONST( DATA_WORDS = File_formats::NDB_PAGE_SIZE_WORDS - 32 );
  STATIC_CONST( CHAIN = 0x8000 );
  
  Uint32 m_data[DATA_WORDS];
  
  void init();
  
  Uint32* get_free_space_ptr() { 
    return m_data+insert_pos; 
  }
  
  Uint32 largest_frag_size() const { 
    return DATA_WORDS - (high_index + insert_pos); 
  }
  
  Uint32 *get_index_ptr(Uint32 page_idx) {
    assert(page_idx < high_index);
    return (m_data + (DATA_WORDS - page_idx));
  }
  
  Uint32 get_index_word(Uint32 page_idx) const {
    assert(page_idx < high_index);
    return * (m_data + (DATA_WORDS - page_idx));
  }

  /**
   * Alloc record from page, return page_idx
   *   temp is used when having to reorg page before allocating
   */
  Uint32 alloc_record(Uint32 size, Tup_varsize_page* temp, Uint32 chain);
  
  /**
   * Free record from page
   */
  Uint32 free_record(Uint32 page_idx, Uint32 chain);

  void reorg(Tup_varsize_page* temp);
  void rebuild_index(Uint32* ptr);
  
  /**
   * Check if one can grow tuple wo/ reorg
   */
  bool is_space_behind_entry(Uint32 page_index, Uint32 growth_len) const {
    Uint32 idx= get_index_word(page_index); 
    Uint32 pos= idx & 0xFFFF;
    Uint32 len= (idx >> 16) & ~CHAIN;
    if ((pos + len == insert_pos) && 
	(insert_pos + growth_len < DATA_WORDS - high_index))
      return true;
    return false;
  }
  
  void grow_entry(Uint32 page_index, Uint32 growth_len) {
    assert(free_space >= growth_len);
    
    Uint32 *pos= get_index_ptr(page_index);
    Uint32 idx= *pos;
    Uint32 size= (idx >> 16) + growth_len;
    *pos= (idx & 0xFFFF) + (size << 16);
    assert((idx & 0xFFFF) + ((idx >> 16) & ~CHAIN) == insert_pos);
    insert_pos+= growth_len;
    free_space-= growth_len;
  }
  
  void shrink_entry(Uint32 page_index, Uint32 new_size){
    Uint32 *pos= get_index_ptr(page_index);
    Uint32 idx= *pos;
    *pos= (idx & (CHAIN << 16 | 0xFFFF)) + (new_size << 16);
    Uint32 old_size= (idx >> 16) & ~CHAIN;
    
    assert(old_size >= new_size);
    Uint32 shrink = old_size - new_size;
#ifdef VM_TRACE
    memset(m_data + (idx & 0xFFFF) + new_size, 0xF1, 4 * shrink);
#endif
    free_space+= shrink;
    if(insert_pos == ((idx & 0xFFFF) + old_size))
      insert_pos -= shrink;
  }
  
  Uint32* get_ptr(Uint32 page_idx) {
    return m_data + (get_index_word(page_idx) & 0xFFFF);
  }
  
  void set_entry_offset(Uint32 page_idx, Uint32 offset){
    Uint32 *pos= get_index_ptr(page_idx);
    *pos = (* pos & 0xFFFF0000) + offset;
  }
  
  Uint32 get_entry_len(Uint32 page_idx) const {
    return get_index_word(page_idx) >> 16;
  }
  
  void set_entry_len(Uint32 page_idx, Uint32 len) {
    Uint32 *pos= get_index_ptr(page_idx);
    *pos = (len << 16) + (*pos & (CHAIN << 16 | 0xFFFF));
  }
};

NdbOut& operator<< (NdbOut& out, const Tup_varsize_page& page);
NdbOut& operator<< (NdbOut& out, const Tup_fixsize_page& page);

#endif
