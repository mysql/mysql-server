/*
   Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef PAGEPOOL_HPP
#define PAGEPOOL_HPP

#include "Pool.hpp"
#include "ndb_limits.h"

#define JAM_FILE_ID 502

/**
 * TransientPagePool
 *
 * Page pool with logical page numbers from 0 and up.
 *
 * About 0.1% of page numbers are unusable, due to words reserved for header
 * on page map pages.
 *
 * Pages are added and removed at top only, so in case lots of pages are
 * unused the memory is kept within pool until top pages are released.
 *
 * Due to this, the pool should only be used in cases there no pages are
 * expected to have indefinite life time.
 *
 * The page map, mapping logical page number to physical page id numbers
 * can handle slightly less than 2^26 pages.
 */

class TransientPagePool {
 public:
  class MapPage;
  class Page;
  class PageMap;

  // Allow unit tests to access private data members by using class Test.
  friend class Test;

  TransientPagePool();
  TransientPagePool(Uint32 type_id, Ndbd_mem_manager *mem_manager);
  void init(Uint32 type_id, Ndbd_mem_manager *mem_manager);

  bool seize(Ptr<Page> &p);
  bool release(Uint32 i);
  bool release(Ptr<Page> p);
  bool getPtr(Ptr<Page> &p) const;
  bool getUncheckedPtr(Ptr<Page> &p) const;
  bool getValidPtr(Ptr<Page> &p) const;
  Uint32 getTopPageNumber() const;
  bool canRelease(Uint32 i) const;

  static Uint64 getMemoryNeed(Uint32 pages);

 private:
  /* Page map methods */
  static bool is_valid_index(Uint32 index);
  static Uint32 get_next_index(Uint32 index);
  static Uint32 get_prev_index(Uint32 index);
  static bool on_same_map_page(Uint32 index1, Uint32 index2);
  // Uint32 get_next_indexes(Uint32 index, Uint32 indexes[], Uint32 n) const;
  bool set(Uint32 index, Uint32 value);
  bool clear(Uint32 index);
  Uint32 get(Uint32 index) const;
  Uint32 get_valid(Uint32 i) const;
  bool shrink();

  Ndbd_mem_manager *m_mem_manager;
  /** PAGE MAP **/
  MapPage *m_root_page;
  Uint32 m_top;
  Uint32 m_type_id;
};

class TransientPagePool::MapPage {
 public:
  static constexpr Uint32 PAGE_ID_GAP = 8;
  static constexpr Uint32 PAGE_WORDS = 8192 - PAGE_ID_GAP;
  static constexpr Uint32 VALUE_INDEX_BITS = 13;
  static constexpr Uint32 VALUE_INDEX_MASK = (1U << VALUE_INDEX_BITS) - 1;
  static constexpr Uint32 NO_VALUE = 0;

  /*
   * Biggest page id supported by one MapPage.
   */
  static constexpr Uint32 MAX_PAGE_ID_1L = PAGE_WORDS - 1;
  /*
   * Biggest page id supported by two levels of MapPage.
   */
  static constexpr Uint32 MAX_PAGE_ID_2L =
      (MAX_PAGE_ID_1L) + (MAX_PAGE_ID_1L)*8192;

  MapPage(Uint32 magic);
  Uint32 get(Uint32 i) const;
  void set(Uint32 i, Uint32 v);

 private:
  Uint32 m_magic;
  Uint32 m_reserved[7];
  Uint32 m_values[PAGE_WORDS];
};

class TransientPagePool::Page {
  friend class TransientPagePool;

 private:
  Uint32 m_magic;
  Uint32 m_page_id;
  Uint32 m_padding[8192 - 2];
};

inline Uint32 TransientPagePool::getTopPageNumber() const { return m_top; }

inline bool TransientPagePool::canRelease(Uint32 i) const {
  return (i != RNIL) && (i == m_top) && (m_top > 0);
}

inline bool TransientPagePool::release(Ptr<Page> p) { return release(p.i); }

inline bool TransientPagePool::on_same_map_page(Uint32 index1, Uint32 index2) {
  return (((index1 ^ index2) >> MapPage::VALUE_INDEX_BITS) == 0);
}

#undef JAM_FILE_ID

#endif
