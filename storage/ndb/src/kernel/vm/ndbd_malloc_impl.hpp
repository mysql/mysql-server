/*
   Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDBD_MALLOC_IMPL_H
#define NDBD_MALLOC_IMPL_H

#include <kernel_types.h>
#include <Bitmask.hpp>
#include <assert.h>
#include "Pool.hpp"
#include <Vector.hpp>

#define JAM_FILE_ID 291


/**
 * 13 -> 8192 words -> 32768 bytes
 * 18 -> 262144 words -> 1M
 */
#define BMW_2LOG 13
#define BITMAP_WORDS (1 << BMW_2LOG)

#define BPP_2LOG (BMW_2LOG + 5)
#define SPACE_PER_BMP_2LOG ((2 + BMW_2LOG) + BPP_2LOG)

//#define BITMAP_WORDS GLOBAL_PAGE_SIZE_WORDS

struct Alloc_page 
{
  Uint32 m_data[BITMAP_WORDS];
};

struct InitChunk
{
  Uint32 m_cnt;
  Uint32 m_start;
  Alloc_page* m_ptr;
};

struct Free_page_data 
{
  Uint32 m_list;
  Uint32 m_next;
  Uint32 m_prev;
  Uint32 m_size;
};

#define FPD_2LOG 2

class Ndbd_mem_manager 
{
public:
  Ndbd_mem_manager();
  
  void set_resource_limit(const Resource_limit& rl);
  bool get_resource_limit(Uint32 id, Resource_limit& rl) const;
  bool get_resource_limit_nolock(Uint32 id, Resource_limit& rl) const;

  bool init(Uint32 *watchCounter, bool allow_alloc_less_than_requested = true);
  void map(Uint32 * watchCounter, bool memlock = false, Uint32 resources[] = 0);
  void* get_memroot() const;
  
  void dump() const ;
  
  enum AllocZone
  {
    NDB_ZONE_LO  = 0, // Only allocate with page_id < (1 << 13)
    NDB_ZONE_ANY = 1  // Allocate with any page_id
  };

  void* alloc_page(Uint32 type, Uint32* i, enum AllocZone);
  void release_page(Uint32 type, Uint32 i);
  
  void alloc_pages(Uint32 type, Uint32* i, Uint32 *cnt, Uint32 min = 1);
  void release_pages(Uint32 type, Uint32 i, Uint32 cnt);
  
  /**
   * Compute 2log of size 
   * @note size = 0     -> 0
   * @note size > 65536 -> 16
   */
  static Uint32 ndb_log2(Uint32 size);

private:
  void grow(Uint32 start, Uint32 cnt);

#define XX_RL_COUNT 10
  /**
   * Return pointer to free page data on page
   */
  static Free_page_data* get_free_page_data(Alloc_page*, Uint32 idx);
  Vector<Uint32> m_used_bitmap_pages;
  
  Uint32 m_buddy_lists[2][16];
  Resource_limit m_resource_limit[XX_RL_COUNT]; // RG_COUNT in record_types.hpp
  Alloc_page * m_base_page;
  
  void release_impl(Uint32 zone, Uint32 start, Uint32 cnt);  
  void insert_free_list(Uint32 zone, Uint32 start, Uint32 cnt);
  Uint32 remove_free_list(Uint32 zone, Uint32 start, Uint32 list);

  void set(Uint32 first, Uint32 last);
  void clear(Uint32 first, Uint32 last);
  void clear_and_set(Uint32 first, Uint32 last);
  Uint32 check(Uint32 first, Uint32 last);

  void alloc(AllocZone, Uint32* ret, Uint32 *pages, Uint32 min_requested);
  void alloc_impl(Uint32 zone, Uint32* ret, Uint32 *pages, Uint32 min);
  void release(Uint32 start, Uint32 cnt);

  /**
   * This is memory that has been allocated
   *   but not yet mapped (i.e it is not possible to get it using alloc_page(s)
   */
  Vector<InitChunk> m_unmapped_chunks;
};

inline
Free_page_data*
Ndbd_mem_manager::get_free_page_data(Alloc_page* ptr, Uint32 idx)
{
  assert(idx & ((1 << BPP_2LOG) - 1));
  assert((idx & ((1 << BPP_2LOG) - 1)) != ((1 << BPP_2LOG) - 1));
  
  return (Free_page_data*)
    (ptr->m_data + ((idx & ((BITMAP_WORDS >> FPD_2LOG) - 1)) << FPD_2LOG));
}

inline
void
Ndbd_mem_manager::set(Uint32 first, Uint32 last)
{
  Alloc_page * ptr = m_base_page;
#if ((SPACE_PER_BMP_2LOG < 32) && (SIZEOF_CHARP == 4)) || (SIZEOF_CHARP == 8)
  Uint32 bmp = first & ~((1 << BPP_2LOG) - 1);
  assert((first >> BPP_2LOG) == (last >> BPP_2LOG));
  assert(bmp < m_resource_limit[0].m_resource_id);

  first -= bmp;
  last -= bmp;
  ptr += bmp;
#endif
  BitmaskImpl::set(BITMAP_WORDS, ptr->m_data, first);
  BitmaskImpl::set(BITMAP_WORDS, ptr->m_data, last);
}

inline
void
Ndbd_mem_manager::clear(Uint32 first, Uint32 last)
{
  Alloc_page * ptr = m_base_page;
#if ((SPACE_PER_BMP_2LOG < 32) && (SIZEOF_CHARP == 4)) || (SIZEOF_CHARP == 8)
  Uint32 bmp = first & ~((1 << BPP_2LOG) - 1);
  assert((first >> BPP_2LOG) == (last >> BPP_2LOG));
  assert(bmp < m_resource_limit[0].m_resource_id);
  
  first -= bmp;
  last -= bmp;
  ptr += bmp;
#endif
  BitmaskImpl::clear(BITMAP_WORDS, ptr->m_data, first);
  BitmaskImpl::clear(BITMAP_WORDS, ptr->m_data, last);
}

inline
void
Ndbd_mem_manager::clear_and_set(Uint32 first, Uint32 last)
{
  Alloc_page * ptr = m_base_page;
#if ((SPACE_PER_BMP_2LOG < 32) && (SIZEOF_CHARP == 4)) || (SIZEOF_CHARP == 8)
  Uint32 bmp = first & ~((1 << BPP_2LOG) - 1);
  assert((first >> BPP_2LOG) == (last >> BPP_2LOG));
  assert(bmp < m_resource_limit[0].m_resource_id);
  
  first -= bmp;
  last -= bmp;
  ptr += bmp;
#endif
  BitmaskImpl::clear(BITMAP_WORDS, ptr->m_data, first);
  BitmaskImpl::clear(BITMAP_WORDS, ptr->m_data, last);
  BitmaskImpl::set(BITMAP_WORDS, ptr->m_data, last+1);
}

inline
Uint32
Ndbd_mem_manager::check(Uint32 first, Uint32 last)
{
  Uint32 ret = 0;
  Alloc_page * ptr = m_base_page;
#if ((SPACE_PER_BMP_2LOG < 32) && (SIZEOF_CHARP == 4)) || (SIZEOF_CHARP == 8)
  Uint32 bmp = first & ~((1 << BPP_2LOG) - 1);
  assert((first >> BPP_2LOG) == (last >> BPP_2LOG));
  assert(bmp < m_resource_limit[0].m_resource_id);
  
  first -= bmp;
  last -= bmp;
  ptr += bmp;
#endif
  ret |= BitmaskImpl::get(BITMAP_WORDS, ptr->m_data, first) << 0;
  ret |= BitmaskImpl::get(BITMAP_WORDS, ptr->m_data, last) << 1;
  return ret;
}



#undef JAM_FILE_ID

#endif 
