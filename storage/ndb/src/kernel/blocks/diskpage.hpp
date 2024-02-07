/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef __NDB_DISKPAGE_HPP
#define __NDB_DISKPAGE_HPP

#include <ndb_types.h>
#include <NdbOut.hpp>

#define JAM_FILE_ID 436

struct File_formats {
  static constexpr Uint32 NDB_PAGE_SIZE = 32768;
  static constexpr Uint32 NDB_DATA_PAGE_HEADER_SIZE = 128;
  static constexpr Uint32 NDB_PAGE_SIZE_WORDS = NDB_PAGE_SIZE >> 2;

  enum File_type { FT_Datafile = 0x1, FT_Undofile = 0x2 };

  struct Page_header {
    Uint32 m_page_lsn_hi;
    Uint32 m_page_lsn_lo;
    Uint32 m_page_type;
  };

  enum Page_type {
    PT_Unallocated = 0x0,
    PT_Extent_page = 0x1,
    PT_Tup_fixsize_page = 0x2,
    PT_Tup_varsize_page = 0x3,
    PT_Undopage = 0x4
  };

  struct Zero_page_header {
    char m_magic[8];
    Uint32 m_byte_order;
    Uint32 m_page_size;
    Uint32 m_ndb_version;
    Uint32 m_node_id;
    Uint32 m_file_type;
    Uint32 m_time;  // time(0)
    Zero_page_header() {}
    void init(File_type ft, Uint32 node_id, Uint32 version, Uint32 now);
    int validate(File_type ft, Uint32 node_id, Uint32 version, Uint32 now);
  };

  static constexpr Uint32 NDB_PAGE_HEADER_WORDS = sizeof(Page_header) >> 2;

  struct Datafile {
    struct Zero_page_v2 {
      struct Zero_page_header m_page_header;
      Uint32 m_file_no;  // Local_key
      Uint32 m_file_id;  // DICT id
      Uint32 m_tablespace_id;
      Uint32 m_tablespace_version;
      Uint32 m_data_pages;
      Uint32 m_extent_pages;
      Uint32 m_extent_size;
      Uint32 m_extent_count;
      Uint32 m_extent_headers_per_page;
      Uint32 m_extent_header_words;
      Uint32 m_extent_header_bits_per_page;
      Uint32 m_checksum;
    };

    struct Zero_page {
      struct Zero_page_header m_page_header;
      Uint32 m_file_no;  // Local_key
      Uint32 m_file_id;  // DICT id
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

    struct Extent_header {
      Uint32 m_table;
      union {
        Uint32 m_fragment_id;
        Uint32 m_next_free_extent;
      };
      Uint32 m_page_bitmask[1];  // (BitsPerPage*ExtentSize)/(32*PageSize)
      Extent_header() {}
    };

    static constexpr Uint32 EXTENT_HEADER_BITMASK_BITS_PER_PAGE = 4;
    static constexpr Uint32 EXTENT_HEADER_FIXED_WORDS =
        (sizeof(Extent_header) >> 2) - 1;

    struct Extent_header_v2 {
      Uint32 m_table;
      Uint32 m_create_table_version;
      union {
        Uint32 m_fragment_id;
        Uint32 m_next_free_extent;
      };
      Uint32 m_unused;           // For future possible use
      Uint32 m_page_bitmask[1];  // (BitsPerPage*ExtentSize)/(32*PageSize)
      Extent_header_v2() {}
    };

    static constexpr Uint32 EXTENT_HEADER_BITMASK_BITS_PER_PAGE_v2 = 4;
    static constexpr Uint32 EXTENT_HEADER_FIXED_WORDS_v2 =
        (sizeof(Extent_header_v2) >> 2) - 1;
    static Uint32 extent_header_words(Uint32 extent_size_in_pages, bool v2);
    static Uint32 extent_page_words(bool v2);

    struct Extent_data {
      Uint32 m_page_bitmask[1];  // (BitsPerPage*ExtentSize)/(32*PageSize)
      Uint32 get_free_bits(Uint32 page) const;
      Uint32 get_free_word_offset(Uint32 page) const;
      void update_free_bits(Uint32 page, Uint32 bit);
      bool check_free(Uint32 extent_size) const;
    };

    struct Extent_page {
      struct Page_header m_page_header;
      Extent_header m_extents[1];

      Extent_page() {}
      Extent_header *get_header(Uint32 extent_no, Uint32 extent_size, bool v2);
      Extent_data *get_extent_data(Uint32 extent_no, Uint32 extent_size,
                                   bool v2);
      Uint32 *get_table_id(Uint32 extent_no, Uint32 extent_size, bool v2);
      Uint32 *get_fragment_id(Uint32 extent_no, Uint32 extent_size, bool v2);
      Uint32 *get_next_free_extent(Uint32 extent_no, Uint32 extent_size,
                                   bool v2);
      Uint32 *get_create_table_version(Uint32 extent_no, Uint32 extent_size,
                                       bool v2);
    };

    static constexpr Uint32 EXTENT_PAGE_WORDS =
        NDB_PAGE_SIZE_WORDS - NDB_PAGE_HEADER_WORDS;

    struct Extent_page_v2 {
      struct Page_header m_page_header;
      Uint32 m_checksum;
      Uint32 m_ndb_version;
      Uint32 m_unused[4];
      Extent_header_v2 m_extents[1];

      Extent_page_v2() {}
      Extent_header_v2 *get_header_v2(Uint32 extent_no, Uint32 extent_size);
    };

    static constexpr Uint32 EXTENT_PAGE_WORDS_v2 =
        NDB_PAGE_SIZE_WORDS - NDB_PAGE_HEADER_WORDS - 6;

    struct Data_page {
      struct Page_header m_page_header;
    };
  };

  struct Undofile {
    struct Zero_page {
      struct Zero_page_header m_page_header;
      Uint32 m_file_id;
      Uint32 m_logfile_group_id;
      Uint32 m_logfile_group_version;
      Uint32 m_undo_pages;
    };
    struct Zero_page_v2 {
      struct Zero_page_header m_page_header;
      Uint32 m_file_id;
      Uint32 m_logfile_group_id;
      Uint32 m_logfile_group_version;
      Uint32 m_undo_pages;
      Uint32 m_checksum;
    };
    struct Undo_page {
      struct Page_header m_page_header;
      Uint32 m_words_used;
      Uint32 m_data[1];
    };
    struct Undo_page_v2 {
      struct Page_header m_page_header;
      Uint32 m_words_used;
      Uint32 m_checksum;
      Uint32 m_ndb_version;
      Uint32 m_unused[6];
      Uint32 m_data[1];
    };

    struct Undo_entry {
      Uint32 m_file_no;
      Uint32 m_page_no;
      struct {
        Uint32 m_len_offset;
        Uint32 m_data[1];
      } m_changes[1];
      Uint32 m_length;  // [ 16-bit type | 16 bit length of entry ]
    };

    enum Undo_type {
      /**
       * We have replaced UNDO_LCP and UNDO_LCP_FIRST by UNDO_LOCAL_LCP
       * and UNDO_LOCAL_LCP_FIRST. We keep the old ones to be able to
       * restore old versions. When reading UNDO_LCP and UNDO_LCP_FIRST
       * we will always assume that local LCP id is 0.
       */
      UNDO_LCP_FIRST = 1  // First LCP record with specific lcp id
      ,
      UNDO_LCP = 2  // LCP Start
      ,
      UNDO_LOCAL_LCP = 10  // LCP start with local LCP id
      ,
      UNDO_LOCAL_LCP_FIRST = 11  // First LCP start with local LCP id

      /**
       * TUP Undo record
       */
      ,
      UNDO_TUP_ALLOC = 3,
      UNDO_TUP_UPDATE = 4,
      UNDO_TUP_FREE = 5,
      UNDO_TUP_CREATE = 6,
      UNDO_TUP_DROP = 7,
      UNDO_TUP_ALLOC_EXTENT = 8,
      UNDO_TUP_FREE_EXTENT = 9,
      UNDO_TUP_FIRST_UPDATE_PART = 12,
      UNDO_TUP_UPDATE_PART = 13,
      UNDO_TUP_FREE_PART = 14

      ,
      UNDO_END = 0x7FFF,
      UNDO_NEXT_LSN = 0x8000
    };

    struct Undo_lcp {
      Uint32 m_lcp_id;
      Uint32 m_type_length;  // 16 bit type, 16 bit length
    };
  };
  static constexpr Uint32 UNDO_PAGE_WORDS =
      NDB_PAGE_SIZE_WORDS - NDB_PAGE_HEADER_WORDS - 1;
  static constexpr Uint32 UNDO_PAGE_WORDS_v2 =
      NDB_PAGE_SIZE_WORDS - NDB_PAGE_HEADER_WORDS - 9;
};

/**
 * Compute size of extent header in words
 */
inline Uint32 File_formats::Datafile::extent_header_words(
    Uint32 extent_size_in_pages, bool v2) {
  if (v2) {
    return EXTENT_HEADER_FIXED_WORDS_v2 +
           ((extent_size_in_pages * EXTENT_HEADER_BITMASK_BITS_PER_PAGE_v2 +
             31) >>
            5);
  } else {
    return EXTENT_HEADER_FIXED_WORDS +
           ((extent_size_in_pages * EXTENT_HEADER_BITMASK_BITS_PER_PAGE + 31) >>
            5);
  }
}

inline Uint32 File_formats::Datafile::extent_page_words(bool v2) {
  if (v2) {
    return File_formats::Datafile::EXTENT_PAGE_WORDS_v2;
  } else {
    return File_formats::Datafile::EXTENT_PAGE_WORDS;
  }
}

inline File_formats::Datafile::Extent_header *
File_formats::Datafile::Extent_page::get_header(Uint32 no, Uint32 extent_size,
                                                bool v2) {
  if (v2) {
    File_formats::Datafile::Extent_page_v2 *page_v2 =
        (File_formats::Datafile::Extent_page_v2 *)this;
    return (Extent_header *)page_v2->get_header_v2(no, extent_size);
  } else {
    Uint32 *tmp = (Uint32 *)m_extents;
    tmp += no * File_formats::Datafile::extent_header_words(extent_size, v2);
    return (Extent_header *)tmp;
  }
}

inline File_formats::Datafile::Extent_header_v2 *
File_formats::Datafile::Extent_page_v2::get_header_v2(Uint32 no,
                                                      Uint32 extent_size) {
  Uint32 *tmp = (Uint32 *)m_extents;
  tmp += no * File_formats::Datafile::extent_header_words(extent_size, true);
  return (Extent_header_v2 *)tmp;
}

inline Uint32 File_formats::Datafile::Extent_data::get_free_bits(
    Uint32 page) const {
  return ((m_page_bitmask[page >> 3] >> ((page & 7) << 2))) & 15;
}

inline Uint32 File_formats::Datafile::Extent_data::get_free_word_offset(
    Uint32 page) const {
  return page >> 3;
}

inline void File_formats::Datafile::Extent_data::update_free_bits(Uint32 page,
                                                                  Uint32 bit) {
  Uint32 shift = (page & 7) << 2;
  Uint32 mask = (15 << shift);
  Uint32 org = m_page_bitmask[page >> 3];
  m_page_bitmask[page >> 3] = (org & ~mask) | (bit << shift);
}

inline bool File_formats::Datafile::Extent_data::check_free(
    Uint32 extent_size) const {
  Uint32 words = (extent_size * EXTENT_HEADER_BITMASK_BITS_PER_PAGE + 31) >> 5;
  Uint32 sum = 0;
  for (; words; words--) sum |= m_page_bitmask[words - 1];

  if (sum & 0x3333) return false;

  return true;
}

inline File_formats::Datafile::Extent_data *
File_formats::Datafile::Extent_page::get_extent_data(Uint32 no,
                                                     Uint32 extent_size,
                                                     bool v2) {
  Extent_data *ret_data;
  if (v2) {
    File_formats::Datafile::Extent_page_v2 *page_v2 =
        (File_formats::Datafile::Extent_page_v2 *)this;
    Uint32 *tmp = (Uint32 *)page_v2->m_extents;
    tmp += no * File_formats::Datafile::extent_header_words(extent_size, v2);
    ret_data = (Extent_data *)&((Extent_header_v2 *)tmp)->m_page_bitmask[0];
  } else {
    Uint32 *tmp = (Uint32 *)m_extents;
    tmp += no * File_formats::Datafile::extent_header_words(extent_size, v2);
    ret_data = (Extent_data *)&((Extent_header *)tmp)->m_page_bitmask[0];
  }
  return ret_data;
}

inline Uint32 *File_formats::Datafile::Extent_page::get_table_id(
    Uint32 no, Uint32 extent_size, bool v2) {
  Uint32 *ret_data;
  if (v2) {
    File_formats::Datafile::Extent_page_v2 *page_v2 =
        (File_formats::Datafile::Extent_page_v2 *)this;
    File_formats::Datafile::Extent_header_v2 *header =
        page_v2->get_header_v2(no, extent_size);
    ret_data = &header->m_table;
  } else {
    File_formats::Datafile::Extent_header *header =
        get_header(no, extent_size, v2);
    ret_data = &header->m_table;
  }
  return ret_data;
}

inline Uint32 *File_formats::Datafile::Extent_page::get_fragment_id(
    Uint32 no, Uint32 extent_size, bool v2) {
  Uint32 *ret_data;
  if (v2) {
    File_formats::Datafile::Extent_page_v2 *page =
        (File_formats::Datafile::Extent_page_v2 *)this;
    File_formats::Datafile::Extent_header_v2 *header =
        page->get_header_v2(no, extent_size);
    ret_data = &header->m_fragment_id;
  } else {
    File_formats::Datafile::Extent_header *header =
        get_header(no, extent_size, v2);
    ret_data = &header->m_fragment_id;
  }
  return ret_data;
}

inline Uint32 *File_formats::Datafile::Extent_page::get_next_free_extent(
    Uint32 no, Uint32 extent_size, bool v2) {
  return get_fragment_id(no, extent_size, v2);
}

inline Uint32 *File_formats::Datafile::Extent_page::get_create_table_version(
    Uint32 no, Uint32 extent_size, bool v2) {
  Uint32 *ret_data;
  if (v2) {
    File_formats::Datafile::Extent_page_v2 *page =
        (File_formats::Datafile::Extent_page_v2 *)this;
    File_formats::Datafile::Extent_header_v2 *header =
        page->get_header_v2(no, extent_size);
    ret_data = &header->m_create_table_version;
  } else {
    ret_data = NULL;
  }
  return ret_data;
}

NdbOut &operator<<(NdbOut &out, const File_formats::Zero_page_header &);
NdbOut &operator<<(NdbOut &out, const File_formats::Datafile::Zero_page &);
NdbOut &operator<<(NdbOut &out, const File_formats::Undofile::Zero_page &);

NdbOut &operator<<(NdbOut &out, const File_formats::Datafile::Zero_page_v2 &);
NdbOut &operator<<(NdbOut &out, const File_formats::Undofile::Zero_page_v2 &);

#undef JAM_FILE_ID

#endif
