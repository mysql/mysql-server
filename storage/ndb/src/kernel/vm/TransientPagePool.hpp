/*
   Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PAGEPOOL_HPP
#define PAGEPOOL_HPP

#include "ndb_limits.h"
#include "Pool.hpp"

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

class TransientPagePool
{
public:
  class MapPage;
  class Page;
  class PageMap;

  TransientPagePool();
  TransientPagePool(Uint32 type_id,
                    Ndbd_mem_manager* mem_manager);
  void init(Uint32 type_id, Ndbd_mem_manager* mem_manager);

  bool seize(Ptr<Page>& p);
  bool release(Uint32 i);
  bool release(Ptr<Page> p);
  bool getPtr(Ptr<Page>& p) const;
  bool getUncheckedPtr(Ptr<Page>& p) const;
  bool getValidPtr(Ptr<Page>& p) const;
  Uint32 getTopPageNumber() const;
  bool canRelease(Uint32 i) const;

  static Uint64 getMemoryNeed(Uint32 pages);
private:
  /* Page map methods */
  Uint32 get_next_index(Uint32 index) const;
  //Uint32 get_next_indexes(Uint32 index, Uint32 indexes[], Uint32 n) const;
  bool set(Uint32 index, Uint32 value);
  bool clear(Uint32 index);
  Uint32 get(Uint32 index) const;
  Uint32 get_valid(Uint32 i) const;
  bool shrink();

  Ndbd_mem_manager* m_mem_manager;
  /** PAGE MAP **/
  MapPage* m_root_page;
  Uint32 m_top;
  Uint32 m_type_id;
};

class TransientPagePool::MapPage
{
public:
  STATIC_CONST( PAGE_WORDS = 8192 - 8 );
  STATIC_CONST( VALUE_INDEX_BITS = 13 );
  STATIC_CONST( VALUE_INDEX_MASK = (1U << VALUE_INDEX_BITS) - 1 );
  STATIC_CONST( NO_VALUE = 0 );

  MapPage(Uint32 magic);
  Uint32 get(Uint32 i) const;
  void set(Uint32 i, Uint32 v);
private:

  Uint32 m_magic;
  Uint32 m_reserved[7];
  Uint32 m_values[PAGE_WORDS];
};

class TransientPagePool::Page
{
  friend class TransientPagePool;
private:
  Uint32 m_magic;
  Uint32 m_page_id;
  Uint32 m_padding[8192 - 2];
};

inline Uint32 TransientPagePool::getTopPageNumber() const
{
  return m_top;
}

inline bool TransientPagePool::canRelease(Uint32 i) const
{
  return (i != RNIL) && (i == m_top) && (m_top > 0);
}

inline bool TransientPagePool::release(Ptr<Page> p)
{
  return release(p.i);
}

#undef JAM_FILE_ID

#endif
