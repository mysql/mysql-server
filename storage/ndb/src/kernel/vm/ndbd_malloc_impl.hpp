/*
   Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDBD_MALLOC_IMPL_H
#define NDBD_MALLOC_IMPL_H

#include <cstdint>
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

#define MAX_ALLOC_PAGES ((1 << BPP_2LOG) - 2)

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

#define MM_RG_COUNT 9

class Resource_limits
{
  Uint32 m_free_reserved;
  Uint32 m_in_use;
  Uint32 m_allocated;
  Uint32 m_spare;
  Uint32 m_max_page;
  Resource_limit m_limit[MM_RG_COUNT];

  Uint32 alloc_resource_spare(Uint32 id, Uint32 cnt);
  void release_resource_spare(Uint32 id, Uint32 cnt);
  void dec_free_reserved(Uint32 cnt);
  void dec_in_use(Uint32 cnt);
  void dec_resource_in_use(Uint32 id, Uint32 cnt);
  void dec_resource_spare(Uint32 id, Uint32 cnt);
  void dec_spare(Uint32 cnt);
  Uint32 get_resource_in_use(Uint32 resource) const;
  Uint32 get_spare() const;
  void inc_free_reserved(Uint32 cnt);
  void inc_in_use(Uint32 cnt);
  void inc_resource_in_use(Uint32 id, Uint32 cnt);
  void inc_resource_spare(Uint32 id, Uint32 cnt);
  void inc_spare(Uint32 cnt);
public:
  Resource_limits();

  void get_resource_limit(Uint32 id, Resource_limit& rl) const;
  void init_resource_limit(Uint32 id, Uint32 min, Uint32 max);
  void init_resource_spare(Uint32 id, Uint32 pct);

  Uint32 get_allocated() const;
  Uint32 get_free_reserved() const;
  Uint32 get_free_shared() const;
  Uint32 get_in_use() const;
  Uint32 get_max_page() const;
  Uint32 get_resource_free(Uint32 id) const;
  Uint32 get_resource_free_reserved(Uint32 id) const;
  Uint32 get_resource_reserved(Uint32 id) const;
  Uint32 get_resource_spare(Uint32 resource) const;
  void set_max_page(Uint32 page);
  void set_allocated(Uint32 cnt);
  void set_free_reserved(Uint32 cnt);

  Uint32 post_alloc_resource_pages(Uint32 id, Uint32 cnt);
  void post_release_resource_pages(Uint32 id, Uint32 cnt);
  void post_alloc_resource_spare(Uint32 id, Uint32 cnt);

  void check() const;
  void dump() const;
};

class Ndbd_mem_manager 
{
public:
  Ndbd_mem_manager();
  
  void set_resource_limit(const Resource_limit& rl);
  bool get_resource_limit(Uint32 id, Resource_limit& rl) const;
  bool get_resource_limit_nolock(Uint32 id, Resource_limit& rl) const;

  bool init(Uint32 *watchCounter, Uint32 pages, bool allow_alloc_less_than_requested = true);
  void map(Uint32 * watchCounter, bool memlock = false, Uint32 resources[] = 0);
  void init_resource_spare(Uint32 id, Uint32 pct);
  void* get_memroot() const;
  
  void dump() const ;
  
  enum AllocZone
  {
    NDB_ZONE_LE_19 = 0, // Only allocate with page_id < (1 << 19)
    NDB_ZONE_LE_27 = 1,
    NDB_ZONE_LE_30 = 2,
    NDB_ZONE_LE_32 = 3,
  };

  void* alloc_page(Uint32 type, Uint32* i, enum AllocZone, bool locked = false);
  void* alloc_spare_page(Uint32 type, Uint32* i, enum AllocZone);
  void release_page(Uint32 type, Uint32 i, bool locked = false);
  
  void alloc_pages(Uint32 type,
                   Uint32* i,
                   Uint32 *cnt,
                   Uint32 min = 1,
                   AllocZone zone = NDB_ZONE_LE_32,
                   bool locked = false);
  void release_pages(Uint32 type, Uint32 i, Uint32 cnt, bool locked = false);

  void lock();
  void unlock();

  /**
   * Compute 2log of size 
   * @note size = 0     -> 0
   * @note size > 65536 -> 16
   */
  static Uint32 ndb_log2(Uint32 size);

private:
  void grow(Uint32 start, Uint32 cnt);

  /**
   * Return pointer to free page data on page
   */
  static Free_page_data* get_free_page_data(Alloc_page*, Uint32 idx);
  Vector<Uint32> m_used_bitmap_pages;
  
  enum { ZONE_19 = 0, ZONE_27 = 1, ZONE_30 = 2, ZONE_32 = 3, ZONE_COUNT = 4 };
  enum {
    ZONE_19_BOUND = (1 << 19),
    ZONE_27_BOUND = (1 << 27),
    ZONE_30_BOUND = (1 << 30)
  };
  Uint32 m_buddy_lists[ZONE_COUNT][16];
  Resource_limits m_resource_limits;
  Alloc_page * m_base_page;
  
  void release_impl(Uint32 zone, Uint32 start, Uint32 cnt);  
  void insert_free_list(Uint32 zone, Uint32 start, Uint32 cnt);
  Uint32 remove_free_list(Uint32 zone, Uint32 start, Uint32 list);

  void set(Uint32 first, Uint32 last);
  void clear(Uint32 first, Uint32 last);
  void clear_and_set(Uint32 first, Uint32 last);
  Uint32 check(Uint32 first, Uint32 last);

  static Uint32 get_page_zone(Uint32 page);
  void alloc(AllocZone, Uint32* ret, Uint32 *pages, Uint32 min_requested);
  void alloc_impl(Uint32 zone, Uint32* ret, Uint32 *pages, Uint32 min);
  void release(Uint32 start, Uint32 cnt);

  /**
   * This is memory that has been allocated
   *   but not yet mapped (i.e it is not possible to get it using alloc_page(s)
   */
  Vector<InitChunk> m_unmapped_chunks;
};

/**
 * Resource_limits
 */

inline
Uint32 Resource_limits::post_alloc_resource_pages(Uint32 id, Uint32 cnt)
{
  const Uint32 inuse = get_resource_in_use(id) + get_resource_spare(id);
  const Uint32 reserved = get_resource_reserved(id);
  if (inuse < reserved)
  {
    Uint32 res_cnt = reserved - inuse;
    if (res_cnt > cnt)
      res_cnt = cnt;
    dec_free_reserved(res_cnt);
  }
  inc_resource_in_use(id, cnt);
  inc_in_use(cnt);

  return alloc_resource_spare(id, cnt);
}

inline
Uint32 Resource_limits::alloc_resource_spare(Uint32 id, Uint32 cnt)
{
  const Resource_limit& rl = m_limit[id - 1];

  Uint32 pct = rl.m_spare_pct;
  if (pct == 0)
  {
    return 0;
  }

  Uint32 inuse = rl.m_curr + rl.m_spare;
  Int64 spare_level = Int64(rl.m_spare) * 100 - Int64(inuse) * pct;

  if (spare_level >= 0)
  {
    return 0;
  }

  Uint32 gain = 100 - pct;
  Uint32 spare_need = (-spare_level + gain - 1) / gain;

  Uint32 spare_res = 0;
  if (rl.m_min > rl.m_curr + rl.m_spare)
  {
    spare_res = rl.m_min - rl.m_curr - rl.m_spare;
    if (spare_res >= spare_need)
    {
      m_limit[id - 1].m_spare += spare_need;
      m_spare += spare_need;
      m_free_reserved -= spare_need;
      return 0;
    }
    spare_need -= spare_res;
  }

  Uint32 free_shr = m_allocated - m_in_use - m_spare;
  if (rl.m_max > 0)
  {
    Uint32 limit = rl.m_max - rl.m_curr - rl.m_spare;
    if (free_shr > limit)
    {
      free_shr = limit;
    }
  }
  Uint32 spare_shr = (free_shr > spare_need) ? spare_need : free_shr;
  spare_need -= spare_shr;

  Uint32 spare_take = (spare_need > cnt) ? cnt : spare_need;

  m_limit[id - 1].m_spare += spare_res + spare_shr + spare_take;
  m_limit[id - 1].m_curr -= spare_take;
  m_free_reserved -= spare_res;
  m_in_use -= spare_take;
  m_spare += spare_res + spare_shr + spare_take;

  // TODO if spare_need > 0, mark out of memory in some way

  return spare_take;
}

inline
void Resource_limits::dec_free_reserved(Uint32 cnt)
{
  assert(m_free_reserved >= cnt);
  m_free_reserved -= cnt;
}

inline
void Resource_limits::dec_in_use(Uint32 cnt)
{
  assert(m_in_use >= cnt);
  m_in_use -= cnt;
}

inline
void Resource_limits::dec_resource_in_use(Uint32 id, Uint32 cnt)
{
  assert(m_limit[id - 1].m_curr >= cnt);
  m_limit[id - 1].m_curr -= cnt;
}

inline
void Resource_limits::dec_resource_spare(Uint32 id, Uint32 cnt)
{
  assert(m_limit[id - 1].m_spare >= cnt);
  m_limit[id - 1].m_spare -= cnt;
}

inline
void Resource_limits::dec_spare(Uint32 cnt)
{
  assert(m_spare >= cnt);
  m_spare -= cnt;
}

inline
Uint32 Resource_limits::get_allocated() const
{
  return m_allocated;
}

inline
Uint32 Resource_limits::get_free_reserved() const
{
  return m_free_reserved;
}

inline
Uint32 Resource_limits::get_free_shared() const
{
  assert(m_allocated >= m_free_reserved + m_in_use + m_spare);
  return m_allocated - (m_free_reserved + m_in_use + m_spare);
}

inline
Uint32 Resource_limits::get_in_use() const
{
  return m_in_use;
}

inline
Uint32 Resource_limits::get_max_page() const
{
  return m_max_page;
}

inline
Uint32 Resource_limits::get_resource_free(Uint32 id) const
{
  require(id <= MM_RG_COUNT);
  const Resource_limit& rl = m_limit[id - 1];
  if (rl.m_max != 0)
  {
    return rl.m_max - (rl.m_curr + rl.m_spare);
  }
  return UINT32_MAX;
}

inline
Uint32 Resource_limits::get_resource_free_reserved(Uint32 id) const
{
  require(id <= MM_RG_COUNT);
  const Resource_limit& rl = m_limit[id - 1];
  if (rl.m_min > (rl.m_curr + rl.m_spare))
  {
     return rl.m_min - (rl.m_curr + rl.m_spare);
  }
  return 0;
}

inline
Uint32 Resource_limits::get_resource_in_use(Uint32 id) const
{
  require(id <= MM_RG_COUNT);
  return m_limit[id - 1].m_curr;
}

inline
void Resource_limits::get_resource_limit(Uint32 id, Resource_limit& rl) const
{
  require(id <= MM_RG_COUNT);
  rl = m_limit[id - 1];
}

inline
Uint32 Resource_limits::get_resource_reserved(Uint32 id) const
{
  require(id > 0);
  require(id <= MM_RG_COUNT);
  return m_limit[id - 1].m_min;
}

inline
Uint32 Resource_limits::get_resource_spare(Uint32 id) const
{
  require(id > 0);
  require(id <= MM_RG_COUNT);
  return m_limit[id - 1].m_spare;
}

inline
Uint32 Resource_limits::get_spare() const
{
  return m_spare;
}

inline
void Resource_limits::inc_free_reserved(Uint32 cnt)
{
  m_free_reserved += cnt;
  assert(m_free_reserved >= cnt);
}

inline
void Resource_limits::inc_in_use(Uint32 cnt)
{
  m_in_use += cnt;
  assert(m_in_use >= cnt);
}

inline
void Resource_limits::inc_resource_in_use(Uint32 id, Uint32 cnt)
{
  m_limit[id - 1].m_curr += cnt;
  assert(m_limit[id - 1].m_curr >= cnt);
}

inline
void Resource_limits::inc_resource_spare(Uint32 id, Uint32 cnt)
{
  m_limit[id - 1].m_spare += cnt;
  assert(m_limit[id - 1].m_spare >= cnt);
}

inline
void Resource_limits::inc_spare(Uint32 cnt)
{
  m_spare += cnt;
  assert(m_spare >= cnt);
}

inline
void Resource_limits::post_release_resource_pages(Uint32 id, Uint32 cnt)
{
  const Uint32 inuse = get_resource_in_use(id) + get_resource_spare(id);
  const Uint32 reserved = get_resource_reserved(id);
  if (inuse - cnt < reserved)
  {
    Uint32 res_cnt = reserved - inuse + cnt;
    if (res_cnt > cnt)
      res_cnt = cnt;
    inc_free_reserved(res_cnt);
  }
  dec_resource_in_use(id, cnt);
  dec_in_use(cnt);

  release_resource_spare(id, cnt);
}

inline
void Resource_limits::release_resource_spare(Uint32 id, Uint32 cnt)
{
  const Resource_limit& rl = m_limit[id - 1];

  Uint32 pct = rl.m_spare_pct;
  if (pct == 0)
  {
    return;
  }
  Uint32 gain = 100 - pct;
  Uint32 inuse = rl.m_curr + rl.m_spare;
  Int64 spare_level = Int64(rl.m_spare) * 100 - Int64(inuse) * pct;

  if (spare_level < gain)
  {
    return;
  }

  Uint32 spare_excess = spare_level / gain;

  if (inuse < rl.m_min + spare_excess)
  {
    Uint32 res_cnt = rl.m_min + spare_excess - inuse;
    if (res_cnt > spare_excess)
      res_cnt = spare_excess;
    m_free_reserved += res_cnt;
  }
  m_limit[id - 1].m_spare -= spare_excess;
  m_spare -= spare_excess;
}

inline
void Resource_limits::set_allocated(Uint32 cnt)
{
  m_allocated = cnt;
}

inline
void Resource_limits::set_free_reserved(Uint32 cnt)
{
  m_free_reserved = cnt;
}

inline
void Resource_limits::set_max_page(Uint32 page)
{
  m_max_page = page;
}

inline
void Resource_limits::post_alloc_resource_spare(Uint32 id, Uint32 cnt)
{
  assert(get_resource_spare(id) > 0);
  dec_resource_spare(id, cnt);
  inc_resource_in_use(id, cnt);
  dec_spare(cnt);
  inc_in_use(cnt);
}

/**
 * Ndbd_mem_manager
 */

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
Uint32 Ndbd_mem_manager::get_page_zone(Uint32 page)
{
  if (page < ZONE_19_BOUND)
  {
    return ZONE_19;
  }
  else if (page < ZONE_27_BOUND)
  {
    return ZONE_27;
  }
  else if (page < ZONE_30_BOUND)
  {
    return ZONE_30;
  }
  else
  {
    return ZONE_32;
  }
}

inline
void
Ndbd_mem_manager::set(Uint32 first, Uint32 last)
{
  /**
   * First and last page in a BPP region may not be available for external use.
   * First page is the bitmap page for the region.
   * Last page is always unused.
   */
  require(first & ((1 << BPP_2LOG) - 1));
  require((first+1) & ((1 << BPP_2LOG) - 1));
  Alloc_page * ptr = m_base_page;
#if ((SPACE_PER_BMP_2LOG < 32) && (SIZEOF_CHARP == 4)) || (SIZEOF_CHARP == 8)
  Uint32 bmp = first & ~((1 << BPP_2LOG) - 1);
  assert((first >> BPP_2LOG) == (last >> BPP_2LOG));
  assert(bmp < m_resource_limits.get_max_page());

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
  assert(bmp < m_resource_limits.get_max_page());
  
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
  assert(bmp < m_resource_limits.get_max_page());
  
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
  assert(bmp < m_resource_limits.get_max_page());
  
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
