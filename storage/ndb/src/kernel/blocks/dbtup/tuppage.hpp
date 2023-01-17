/*
   Copyright (c) 2005, 2023, Oracle and/or its affiliates.

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

#ifndef __NDB_TUP_PAGE_HPP
#define __NDB_TUP_PAGE_HPP

#include "util/require.h"
#include <pc.hpp>
#include <ndb_types.h>
#include "../diskpage.hpp"
#include <Bitmask.hpp>
#include <portlib/ndb_prefetch.h>
#define JAM_FILE_ID 419


struct Tup_page 
{
  Tup_page() {}
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
  Uint32 unused_cluster_page[3];
  Uint32 m_gci;
  Uint32 frag_page_id;
  Uint32 physical_page_id;
  Uint32 free_space;
  Uint32 next_free_index;
  /**
   * list_index used by disk pages and varsized pages.
   * free space in page bits/list, 0x8000 means not in free
   */
  Uint32 list_index;
  Uint32 uncommitted_used_space;
  Uint32 m_page_no;
  Uint32 m_file_no;
  Uint32 m_table_id;
  Uint32 m_fragment_id;
  Uint32 m_extent_no;
  Uint32 m_extent_info_ptr;
  Uint32 unused_high_index; // size of index + 1
  Uint32 unused_insert_pos;
  Uint32 m_flags; /* Currently only LCP_SKIP flag in bit 0 */
  Uint32 m_ndb_version;
  Uint32 m_create_table_version;
  Uint32 m_change_map[4];

  static constexpr Uint32 HEADER_WORDS = 32;
  static constexpr Uint32 DATA_WORDS = File_formats::NDB_PAGE_SIZE_WORDS -
                                       HEADER_WORDS;
  
  Uint32 m_data[DATA_WORDS];

  static constexpr Uint32 LCP_SKIP_FLAG = 1;

  bool is_page_to_skip_lcp() const
  {
    if (m_flags & LCP_SKIP_FLAG)
    {
      return true;
    }
    return false;
  }
  void set_page_to_skip_lcp()
  {
    m_flags |= LCP_SKIP_FLAG;
  }
  void clear_page_to_skip_lcp()
  {
    m_flags &= (~LCP_SKIP_FLAG);
  }
};

struct Tup_fixsize_page
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
  Uint32 unused_cluster_page[3];
  Uint32 m_gci;
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
  Uint32 unused_high_index; // size of index + 1
  Uint32 unused_insert_pos;
  /**
   * Currently LCP_SKIP flag in bit 0 and
   * change map bits in bits 24-31 (4 kB per bit)
   */
  Uint32 m_flags;
  Uint32 m_ndb_version;
  Uint32 m_schema_version;
  Uint32 m_change_map[4];

  /**
   * Don't set/reset LCP_SKIP/LCP_DELETE flags
   * The LCP_SKIP and LCP_DELETE flags are alive also after the record has
   * been deleted. This is to track rows that have been scanned, LCP scans
   * also scans deleted rows to ensure that any deleted rows since last LCP
   * are tracked.
   */
  static constexpr Uint32 FREE_RECORD = 0xeeffffff;
  static constexpr Uint32 HEADER_WORDS = 32;
  static constexpr Uint32 DATA_WORDS = File_formats::NDB_PAGE_SIZE_WORDS -
                                       HEADER_WORDS;
  static constexpr Uint32 FIRST_BIT_CHANGE_MAP = 24;
  static constexpr Uint32 PAGE_CHANGED_WHILE_LCP_SCAN_BIT = 23;
  static constexpr Uint32 PAGE_IS_BEING_LCP_SCANNED_BIT = 22;
  
  Uint32 m_data[DATA_WORDS];
  
  Uint32* get_ptr(Uint32 page_idx, Uint32 rec_size)
  {
    require(page_idx + rec_size <= DATA_WORDS);
    return m_data + page_idx;
  }
  Uint32 get_next_large_idx(Uint32 idx, Uint32 size)
  {
    /* First move idx to next 1024 word boundary */ 
    Uint32 new_idx = ((idx + 1024) / 1024) * 1024;
    /* Next move idx forward to size word boundary */
    new_idx = ((new_idx + size - 1) / size) * size;
    return new_idx;
  }
  Uint32 get_next_small_idx(Uint32 idx, Uint32 size)
  {
    /* First move idx to next 64 word boundary */ 
    Uint32 new_idx = ((idx + 64) / 64) * 64;
    /* Next move idx forward to size word boundary */
    new_idx = ((new_idx + size - 1) / size) * size;
    return new_idx;
  }
  bool get_and_clear_change_while_lcp_scan()
  {
    Uint32 flags = m_flags;
    Uint32 bit_pos = Tup_fixsize_page::PAGE_CHANGED_WHILE_LCP_SCAN_BIT;
    Uint32 flags_bit = 1 << bit_pos;
    bool bit_set = ((flags & flags_bit) != 0);
    Uint32 flags_clear_val = ~flags_bit;
    Uint32 flags_new_val = flags & flags_clear_val;
    m_flags = flags_new_val;
    return bit_set;
  }
  void set_change_while_lcp_scan()
  {
    Uint32 flags = m_flags;
    Uint32 bit_pos = Tup_fixsize_page::PAGE_CHANGED_WHILE_LCP_SCAN_BIT;
    Uint32 flags_bit = 1 << bit_pos;
    Uint32 new_flags = flags | flags_bit;
    m_flags = new_flags;
  }
  bool get_page_being_lcp_scanned()
  {
    Uint32 flags = m_flags;
    Uint32 bit_pos = Tup_fixsize_page::PAGE_IS_BEING_LCP_SCANNED_BIT;
    Uint32 flags_bit = 1 << bit_pos;
    bool bit_set = ((flags & flags_bit) != 0);
    return bit_set;
  }
  void set_page_being_lcp_scanned()
  {
    Uint32 flags = m_flags;
    Uint32 bit_pos = Tup_fixsize_page::PAGE_IS_BEING_LCP_SCANNED_BIT;
    Uint32 flags_bit = 1 << bit_pos;
    Uint32 new_flags = flags | flags_bit;
    m_flags = new_flags;
  }
  void clear_page_being_lcp_scanned()
  {
    Uint32 flags = m_flags;
    Uint32 bit_pos = Tup_fixsize_page::PAGE_IS_BEING_LCP_SCANNED_BIT;
    Uint32 flags_bit = 1 << bit_pos;
    Uint32 flags_clear_val = ~flags_bit;
    Uint32 flags_new_val = flags & flags_clear_val;
    m_flags = flags_new_val;
  }
  void prefetch_change_map()
  {
    NDB_PREFETCH_WRITE(&frag_page_id);
    NDB_PREFETCH_WRITE(&m_flags);
  }
  void clear_small_change_map()
  {
    m_change_map[0] = 0;
    m_change_map[1] = 0;
    m_change_map[2] = 0;
    m_change_map[3] = 0;
  }
  void clear_large_change_map()
  {
    Uint32 map_val = m_flags;
    map_val <<= 8;
    map_val >>= 8;
    m_flags = map_val;
  }
  void set_all_change_map()
  {
    m_change_map[0] = 0xFFFFFFFF;
    m_change_map[1] = 0xFFFFFFFF;
    m_change_map[2] = 0xFFFFFFFF;
    m_change_map[3] = 0xFFFFFFFF;
    Uint32 map_val = 0xFF;
    map_val <<= Tup_fixsize_page::FIRST_BIT_CHANGE_MAP;
    Uint32 flags = m_flags;
    flags |= map_val;
    m_flags = flags;
  }
  void verify_small_map_clear(Uint32 bit_pos)
  {
    /**
     * Verify that also small change map is zero when the large
     * map is zero.
     */
    Uint32 i = (bit_pos - Tup_fixsize_page::FIRST_BIT_CHANGE_MAP) / 2;
    Uint32 small_bit_map = m_change_map[i];
    if ((bit_pos & 1) == 0)
    {
      small_bit_map &= 0xFFFF;
    }
    else
    {
      small_bit_map >>= 16;
    }
    require(small_bit_map == 0);
  }
  void verify_small_map_not_clear(Uint32 bit_pos)
  {
    /**
     * Verify that also small change map is not zero when the large
     * map is not zero.
     */
    Uint32 i = (bit_pos - Tup_fixsize_page::FIRST_BIT_CHANGE_MAP) / 2;
    Uint32 small_bit_map = m_change_map[i];
    if ((bit_pos & 1) == 0)
    {
      small_bit_map &= 0xFFFF;
    }
    else
    {
      small_bit_map >>= 16;
    }
    require(small_bit_map != 0);
  }
  void set_change_maps(Uint32 page_index)
  {
    if (unlikely(get_page_being_lcp_scanned()))
    {
      set_change_while_lcp_scan();
      return;
    }
    assert(page_index < Tup_fixsize_page::DATA_WORDS);
    Uint32 *map_ptr = &m_change_map[0];
    /**
     * Each bit maps a 64 word region, the starting word is
     * used as the word to calculate the map index based on.
     */
    Uint32 map_id = page_index / 64;
    Uint32 idx = map_id / 32;
    Uint32 bit_pos = map_id & 31;
    assert(idx < 4);
    Uint32 map_val = map_ptr[idx];
    Uint32 map_set_val = 1 << bit_pos;
    map_val |= map_set_val;
    map_ptr[idx] = map_val;
    /**
     * Also set the change map with only 8 bits, one bit per
     * 4 kB.
     */
    Uint32 large_map_idx = Tup_fixsize_page::FIRST_BIT_CHANGE_MAP +
                           (page_index >> 10);
    assert(large_map_idx <= 31);
    map_set_val = 1 << large_map_idx;
    m_flags |= map_set_val;
    verify_small_map_not_clear(large_map_idx);
  }
  void clear_large_change_map(Uint32 page_index)
  {
    assert(page_index < Tup_fixsize_page::DATA_WORDS);
    Uint32 map_val = m_flags;
    Uint32 bit_pos = Tup_fixsize_page::FIRST_BIT_CHANGE_MAP +
                     (page_index >> 10);
    assert(bit_pos <= 31);
    Uint32 map_get_val = 1 << bit_pos;
    Uint32 map_clear_val = ~map_get_val;
    Uint32 map_new_val = map_val & map_clear_val;
    m_flags = map_new_val;
    verify_small_map_clear(bit_pos);
  }
  bool get_large_change_map(Uint32 page_index)
  {
    /**
     * Get the large change map bit.
     * If the bit is set, we will not reset it yet to ensure
     * that the page bits are always in a consistent state.
     * Instead we will reset it when the last small change
     * map bit is reset.
     */
    assert(page_index < Tup_fixsize_page::DATA_WORDS);
    Uint32 map_val = m_flags;
    Uint32 bit_pos = Tup_fixsize_page::FIRST_BIT_CHANGE_MAP +
                     (page_index >> 10);
    assert(bit_pos <= 31);
    Uint32 map_get_val = 1 << bit_pos;
    bool bit_set = ((map_get_val & map_val) != 0);
    if (!bit_set)
    {
      verify_small_map_clear(bit_pos);
    }
    else
    {
      verify_small_map_not_clear(bit_pos);
    }
    return bit_set;
  }
  bool get_and_clear_change_maps(Uint32 page_index)
  {
    assert(page_index < Tup_fixsize_page::DATA_WORDS);
    Uint32 *map_ptr = &m_change_map[0];
    Uint32 map_id = page_index / 64;
    Uint32 idx = map_id / 32;
    assert(idx < 4);
    Uint32 bit_pos = map_id & 31;
    Uint32 map_val = map_ptr[idx];
    Uint32 map_get_val = 1 << bit_pos;
    Uint32 map_clear_val = ~map_get_val;
    Uint32 map_new_val = map_val & map_clear_val;
    map_ptr[idx] = map_new_val;

    /**
     * Ensure that large map is cleared when we clear the
     * last bit in the small change map corresponding to
     * the large bit.
     *
     * Only necessary to perform this check when we actually
     * changed a bit in the small map.
     */
    bool any_change = ((map_get_val & map_val) != 0);
    if (any_change)
    {
      Uint32 small_bit_map = map_new_val;
      if (bit_pos < 16)
      {
        small_bit_map &= 0xFFFF;
      }
      else
      {
        small_bit_map >>= 16;
      }
      if (small_bit_map == 0)
      {
        clear_large_change_map(page_index);
      }
    }
    return any_change;
  }
  bool get_any_changes()
  {
    Uint32 map_val = m_flags;
    map_val >>= Tup_fixsize_page::FIRST_BIT_CHANGE_MAP;
#ifdef VM_TRACE
    if (map_val == 0)
    {
      Uint32 sum_small_maps =
        m_change_map[0] + m_change_map[1] + m_change_map[2] + m_change_map[3];
      require(sum_small_maps == 0);
    }
#endif
    return (map_val != 0);
  }
  bool verify_change_maps(EmulatedJamBuffer *jamBuf)
  {
    for (Uint32 i = 0; i < 4; i++)
    {
      Uint32 small_map = m_change_map[i];
      Uint32 bit_pos = 2 * i + Tup_fixsize_page::FIRST_BIT_CHANGE_MAP;
      Uint32 bit_val = m_flags & (1 << bit_pos);
      if (bit_val != 0)
      {
        Uint32 small_bit_map = small_map & 0xFFFF;
        if (small_bit_map == 0)
        {
          thrjamDebug(jamBuf);
          thrjamLineDebug(jamBuf, (Uint16(i)));
          return false;
        }
      }
      else
      {
        Uint32 small_bit_map = small_map & 0xFFFF;
        if (small_bit_map != 0)
        {
          thrjamDebug(jamBuf);
          thrjamLineDebug(jamBuf, (Uint16(i)));
          thrjamLineDebug(jamBuf, (Uint16(small_bit_map)));
          return false;
        }
      }
      bit_pos = 2 * i + Tup_fixsize_page::FIRST_BIT_CHANGE_MAP + 1;
      bit_val = m_flags & (1 << bit_pos);
      if (bit_val != 0)
      {
        Uint32 small_bit_map = small_map >> 16;
        if (small_bit_map == 0)
        {
          thrjamDebug(jamBuf);
          thrjamLineDebug(jamBuf, (Uint16(i)));
          return false;
        }
      }
      else
      {
        Uint32 small_bit_map = small_map >> 16;
        if (small_bit_map != 0)
        {
          thrjamDebug(jamBuf);
          thrjamLineDebug(jamBuf, (Uint16(i)));
          thrjamLineDebug(jamBuf, (Uint16(small_bit_map)));
          return false;
        }
      }
    }
    return true;
  }
  Uint32 get_num_changes()
  {
    Uint32 bit_count = 0;
    Uint32 map_val;
    map_val = m_change_map[0];
    bit_count += BitmaskImpl::count_bits(map_val);
    map_val = m_change_map[1];
    bit_count += BitmaskImpl::count_bits(map_val);
    map_val = m_change_map[2];
    bit_count += BitmaskImpl::count_bits(map_val);
    map_val = m_change_map[3];
    bit_count += BitmaskImpl::count_bits(map_val);
    return bit_count;
  }
  void clear_max_gci()
  {
    m_gci = 0;
  }
  Uint32 get_max_gci()
  {
    return m_gci;
  }
  void set_max_gci(Uint32 gci)
  {
    if (gci > m_gci)
      m_gci = gci;
  }

  /**
   * Alloc record from page
   *   return page_idx
   **/
  Tup_fixsize_page() {}
  Uint32 alloc_record();
  Uint32 alloc_record(Uint32 page_idx);
  Uint32 free_record(Uint32 page_idx);
};

struct Tup_varsize_page
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
  Uint32 unused_cluster_page[3];
  Uint32 m_gci;
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
  Uint32 m_flags; /* Currently only LCP_SKIP flag in bit 0 */
  Uint32 m_ndb_version;
  Uint32 m_schema_version;
  Uint32 m_change_map[4];
  
  static constexpr Uint32 HEADER_WORDS = 32;
  static constexpr Uint32 DATA_WORDS = File_formats::NDB_PAGE_SIZE_WORDS -
                                       HEADER_WORDS;
  static constexpr Uint32 CHAIN = 0x80000000;
  static constexpr Uint32 FREE = 0x40000000;
  static constexpr Uint32 LEN_MASK = 0x3FFF8000;
  static constexpr Uint32 POS_MASK = 0x00007FFF;
  static constexpr Uint32 LEN_SHIFT = 15;
  static constexpr Uint32 POS_SHIFT = 0;
  static constexpr Uint32 END_OF_FREE_LIST = POS_MASK;

  static constexpr Uint32 NEXT_MASK = POS_MASK;
  static constexpr Uint32 NEXT_SHIFT = POS_SHIFT;
  static constexpr Uint32 PREV_MASK = LEN_MASK;
  static constexpr Uint32 PREV_SHIFT = LEN_SHIFT;
  
  Uint32 m_data[DATA_WORDS];
  
  Tup_varsize_page() {}
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
   * Alloc page_idx from page, return page_idx
   *   temp is used when having to reorg page before allocating
   */
  Uint32 alloc_record(Uint32 page_idx, Uint32 size, Tup_varsize_page* temp);
  
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
    Uint32 pos= (idx & POS_MASK) >> POS_SHIFT;
    Uint32 len= (idx & LEN_MASK) >> LEN_SHIFT;
    if ((pos + len == insert_pos) && 
	(insert_pos + growth_len < DATA_WORDS - high_index))
      return true;
    return false;
  }
  
  void grow_entry(Uint32 page_index, Uint32 growth_len) {
    assert(free_space >= growth_len);

    Uint32 *pos= get_index_ptr(page_index);
    Uint32 idx= *pos;
    assert(! (idx & FREE));
    assert((((idx & POS_MASK) >> POS_SHIFT) + ((idx & LEN_MASK) >> LEN_SHIFT))
	   == insert_pos);
    
    * pos= idx + (growth_len << LEN_SHIFT);
    insert_pos+= growth_len;
    free_space-= growth_len;
  }
  
  void shrink_entry(Uint32 page_index, Uint32 new_size){
    Uint32 *pos= get_index_ptr(page_index);
    Uint32 idx= *pos;
    Uint32 old_pos = (idx & POS_MASK) >> POS_SHIFT;
    Uint32 old_size = (idx & LEN_MASK) >> LEN_SHIFT;

    assert( ! (idx & FREE));
    assert(old_size >= new_size);

    * pos= (idx & ~LEN_MASK) + (new_size << LEN_SHIFT);
    Uint32 shrink = old_size - new_size;
#ifdef VM_TRACE
    memset(m_data + old_pos + new_size, 0xF1, 4 * shrink);
#endif
    free_space+= shrink;
    if(insert_pos == (old_pos + old_size))
      insert_pos -= shrink;
  }
  
  Uint32* get_ptr(Uint32 page_idx) {
    return m_data + ((get_index_word(page_idx) & POS_MASK) >> POS_SHIFT);
  }
  
  void set_entry_offset(Uint32 page_idx, Uint32 offset){
    Uint32 *pos= get_index_ptr(page_idx);
    * pos = (* pos & ~POS_MASK) + (offset << POS_SHIFT);
  }
  
  void set_entry_len(Uint32 page_idx, Uint32 len) {
    Uint32 *pos= get_index_ptr(page_idx);
    * pos = (*pos & ~LEN_MASK) + (len << LEN_SHIFT);
  }

  Uint32 get_entry_len(Uint32 page_idx) const {
    return (get_index_word(page_idx) & LEN_MASK) >> LEN_SHIFT;
  }
  
  Uint32 get_entry_chain(Uint32 page_idx) const {
    return get_index_word(page_idx) & CHAIN;
  }

  bool is_free(Uint32 page_idx) const
  {
    return ((get_index_word(page_idx) & FREE) != 0) ? true : false;
  }

  bool is_empty() const
  {
    return high_index == 1;
  }
};

NdbOut& operator<< (NdbOut& out, const Tup_varsize_page& page);
NdbOut& operator<< (NdbOut& out, const Tup_fixsize_page& page);


#undef JAM_FILE_ID

#endif
