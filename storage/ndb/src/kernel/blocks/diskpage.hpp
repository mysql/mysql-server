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

#ifndef __NDB_DISKPAGE_HPP
#define __NDB_DISKPAGE_HPP

#include <ndb_types.h>

struct File_formats 
{
  STATIC_CONST( NDB_PAGE_SIZE = 32768 );
  STATIC_CONST( NDB_PAGE_SIZE_WORDS = NDB_PAGE_SIZE >> 2);
  
  enum File_type
  {
    FT_Datafile = 0x1,
    FT_Undofile = 0x2
  };

  struct Page_header 
  {
    Uint32 m_page_lsn_hi;
    Uint32 m_page_lsn_lo;
    Uint32 m_page_type;
  };

  enum Page_type 
  {
    PT_Unallocated        = 0x0,
    PT_Extent_page        = 0x1,
    PT_Tup_fixsize_page   = 0x2,
    PT_Tup_varsize_page   = 0x3,
    PT_Undopage           = 0x4
  };

  struct Zero_page_header
  {
    char   m_magic[8];
    Uint32 m_byte_order;
    Uint32 m_page_size;
    Uint32 m_ndb_version;
    Uint32 m_node_id;
    Uint32 m_file_type;
    Uint32 m_time; // time(0)
    Zero_page_header() {}
    void init(File_type ft, Uint32 node_id, Uint32 version, Uint32 now);
    int validate(File_type ft, Uint32 node_id, Uint32 version, Uint32 now);
  };
  
  STATIC_CONST( NDB_PAGE_HEADER_WORDS = sizeof(Page_header) >> 2);
  
  struct Datafile 
  {
    struct Zero_page 
    {
      struct Zero_page_header m_page_header;
      Uint32 m_file_no; // Local_key
      Uint32 m_file_id; // DICT id
      Uint32 m_tablespace_id;
      Uint32 m_tablespace_version;
      Uint32 m_data_pages;
      Uint32 m_extent_pages;
      Uint32 m_extent_size;
      Uint32 m_extent_count;
      Uint32 m_extent_headers_per_page;
      Uint32 m_extent_header_words;
      Uint32 m_extent_header_bits_per_page;
    };
    
    struct Extent_header 
    {
      Uint32 m_table;
      union 
      {
	Uint32 m_fragment_id;
	Uint32 m_next_free_extent;
      };
      Extent_header() {}
      Uint32 m_page_bitmask[1]; // (BitsPerPage*ExtentSize)/(32*PageSize)
      Uint32 get_free_bits(Uint32 page) const;
      Uint32 get_free_word_offset(Uint32 page) const;
      void update_free_bits(Uint32 page, Uint32 bit);
      bool check_free(Uint32 extent_size) const ;
    };
    
    STATIC_CONST( EXTENT_HEADER_BITMASK_BITS_PER_PAGE = 4 );
    STATIC_CONST( EXTENT_HEADER_FIXED_WORDS = (sizeof(Extent_header)>>2) - 1);
    static Uint32 extent_header_words(Uint32 extent_size_in_pages);
    
    struct Extent_page
    {
      struct Page_header m_page_header;
      Extent_header m_extents[1];
      
      Extent_page() {}
      Extent_header* get_header(Uint32 extent_no, Uint32 extent_size);
    };
    
    STATIC_CONST( EXTENT_PAGE_WORDS = NDB_PAGE_SIZE_WORDS - NDB_PAGE_HEADER_WORDS );
    
    struct Data_page 
    {
      struct Page_header m_page_header;
    };
  };

  struct Undofile 
  {
    struct Zero_page
    {
      struct Zero_page_header m_page_header;
      Uint32 m_file_id;
      Uint32 m_logfile_group_id;
      Uint32 m_logfile_group_version;
      Uint32 m_undo_pages;
    };
    struct Undo_page 
    {
      struct Page_header m_page_header;
      Uint32 m_words_used;
      Uint32 m_data[1];
    };

    struct Undo_entry
    {
      Uint32 m_file_no;
      Uint32 m_page_no;
      struct 
      {
	Uint32 m_len_offset;
	Uint32 m_data[1];
      } m_changes[1];
      Uint32 m_length; // [ 16-bit type | 16 bit length of entry ]
    };

    enum Undo_type {
      UNDO_LCP_FIRST  = 1 // First LCP record with specific lcp id
      ,UNDO_LCP = 2       // LCP Start
      
      /**
       * TUP Undo record
       */
      ,UNDO_TUP_ALLOC  = 3
      ,UNDO_TUP_UPDATE = 4
      ,UNDO_TUP_FREE   = 5
      ,UNDO_TUP_CREATE = 6
      ,UNDO_TUP_DROP   = 7
      ,UNDO_TUP_ALLOC_EXTENT = 8
      ,UNDO_TUP_FREE_EXTENT  = 9
      
      ,UNDO_END        = 0x7FFF 
      ,UNDO_NEXT_LSN   = 0x8000
    };

    struct Undo_lcp
    {
      Uint32 m_lcp_id;
      Uint32 m_type_length; // 16 bit type, 16 bit length
    };
  };
  STATIC_CONST( UNDO_PAGE_WORDS = NDB_PAGE_SIZE_WORDS - NDB_PAGE_HEADER_WORDS - 1);
};


/**
 * Compute size of extent header in words
 */
inline Uint32 
File_formats::Datafile::extent_header_words(Uint32 extent_size_in_pages)
{
  return EXTENT_HEADER_FIXED_WORDS + 
    ((extent_size_in_pages * EXTENT_HEADER_BITMASK_BITS_PER_PAGE + 31) >> 5);
}

inline
File_formats::Datafile::Extent_header*
File_formats::Datafile::Extent_page::get_header(Uint32 no, Uint32 extent_size)
{
  Uint32 * tmp = (Uint32*)m_extents;
  tmp += no*File_formats::Datafile::extent_header_words(extent_size);
  return (Extent_header*)tmp;
}

inline
Uint32
File_formats::Datafile::Extent_header::get_free_bits(Uint32 page) const
{
  return ((m_page_bitmask[page >> 3] >> ((page & 7) << 2))) & 15;
}

inline
Uint32
File_formats::Datafile::Extent_header::get_free_word_offset(Uint32 page) const
{
  return page >> 3;
}

inline
void
File_formats::Datafile::Extent_header::update_free_bits(Uint32 page, 
							Uint32 bit)
{
  Uint32 shift = (page & 7) << 2;
  Uint32 mask = (15 << shift);
  Uint32 org = m_page_bitmask[page >> 3];
  m_page_bitmask[page >> 3] = (org & ~mask) | (bit << shift);
}

inline
bool
File_formats::Datafile::Extent_header::check_free(Uint32 extent_size) const 
{
  Uint32 words = (extent_size * EXTENT_HEADER_BITMASK_BITS_PER_PAGE + 31) >> 5;
  Uint32 sum = 0;
  for(; words; words--)
    sum |= m_page_bitmask[words-1];

  if(sum & 0x3333)
    return false;
  
  return true;
}

#include <NdbOut.hpp>
NdbOut& operator<<(NdbOut& out, const File_formats::Zero_page_header&);
NdbOut& operator<<(NdbOut& out, const File_formats::Datafile::Zero_page&);
NdbOut& operator<<(NdbOut& out, const File_formats::Undofile::Zero_page&);

#endif
