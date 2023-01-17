/*
   Copyright (c) 2006, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <algorithm>
#ifdef VM_TRACE
#ifndef NDBD_RANDOM_START_PAGE
#define NDBD_RANDOM_START_PAGE
#endif
#endif

#include <cstdint>
#include <kernel_types.h>
#include <Bitmask.hpp>
#include <assert.h>
#include "NdbSeqLock.hpp"
#include "Pool.hpp"
#include <Vector.hpp>
#include <EventLogger.hpp>

#define JAM_FILE_ID 291

#ifdef NDBD_RANDOM_START_PAGE
extern Uint32 g_random_start_page_id;
#endif

/**
 * Ndbd_mem_manager handles memory in pages of size 32KiB.
 *
 * Pages are arranged in 8GiB regions, there first page is a bitmap
 * indicating what pages in region have free page data, the last page is
 * not used.
 *
 * There is one base address and pages are numbered with an 32 bit index
 * from that address.  Index should be less than RNIL (0xFFFFFF00).  RNIL
 * is not a valid page number.
 *
 * Regions are numbered with a 14 bit number, there 0x3FFF may not be
 * used.  This limit possible page numbers to 0xFFFC0000.
 *
 * Furthermore there are zones defined that contains pages which have a
 * page number representable with some specific number of bits.
 *
 * There are currently four zones:
 *
 * ZONE_19: regions      0 - 1       , pages      0 - (2^19-1)
 * ZONE_27: regions      2 - (2^9-1) , pages (2^19) - (2^27-1)
 * ZONE_30: regions  (2^9) - (2^12-1), pages (2^27) - (2^30-1)
 * ZONE_32: regions (2^12) - 0x3FFE  , pages (2^30) - 0xFFFBFFFF
 */

/**
 * 13 -> 8192 words -> 32768 bytes
 * 18 -> 262144 words -> 1M
 */
#define BMW_2LOG 13
#define BITMAP_WORDS (1 << BMW_2LOG)

#define BPP_2LOG (BMW_2LOG + 5)
#define PAGE_REGION_MASK ((1 << BPP_2LOG) - 1)
#define SPACE_PER_BMP_2LOG ((2 + BMW_2LOG) + BPP_2LOG)

#ifdef VM_TRACE
#ifndef NDBD_RANDOM_START_PAGE
#define NDBD_RANDOM_START_PAGE
#endif
#endif


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

/**
  Information of restriction and current usage of shared global page memory.
*/
class Resource_limits
{
  /**
    Number of pages dedicated for different resource group but currently not
    in use.
  */
  Uint32 m_free_reserved;

  /**
    Number of pages currently in use.
  */
  Uint32 m_in_use;

  /**
    Total number of pages allocated.
  */
  Uint32 m_allocated;

  /**
    Total number of spare pages currently allocated by resource groups.
  */
  Uint32 m_spare;

  /**
    Number of pages that some resource have given up but have not been taken
    by some other resource yet.

    This should zero, but while transferring pages from one resource to another
    pages using give_up_pages() and take_pages() this can be non zero for a
    short moment.

    Note, untaken pages still are accounted as in_use in global.
  */
  Uint32 m_untaken;

  /**
    The number of pages otherwise dedicated to some resource that are available
    for other resources.

    See give_up_pages() and take_pages().
  */
  Uint32 m_lent;

  /**
    The number of pages used by some resource that are lent from dedicated
    pages for some other resource.

    See give_up_pages() and take_pages().
  */
  Uint32 m_borrowed;

  /**
    One more than highest page number allocated.

    Used internally by Ndbd_mem_manager for consistency checks.
  */
  Uint32 m_max_page;

  /**
    Number of pages reserved for high priority resource groups.

    Page allocations for low priority resource groups will be denied if number
    of free pages are less than this number.

    Note that not have any dedicated pages is the criteria for a resource group
    to have low priority, and there will never be any dedicted free pages for
    that resource.
  */
  Uint32 m_prio_free_limit;

  /**
    Per resource group statistics.
    See documentation for Resource_limit.
  */
  Resource_limit m_limit[MM_RG_COUNT];

  /**
    Keep 10% of unreserved shared memory only for high priority resource
    groups.

    High priority of a resource group is indicated by setting the minimal
    reserved to zero (Resource_limit::m_min == 0).
  */
  static const Uint32 HIGH_PRIO_FREE_PCT = 10;

  Uint32 alloc_resource_spare(Uint32 id, Uint32 cnt);
  void release_resource_spare(Uint32 id, Uint32 cnt);
  void dec_in_use(Uint32 cnt);
  void dec_resource_borrowed(Uint32 id, Uint32 cnt);
  void dec_resource_lent(Uint32 id, Uint32 cnt);
  void dec_resource_in_use(Uint32 id, Uint32 cnt);
  void dec_resource_spare(Uint32 id, Uint32 cnt);
  void dec_spare(Uint32 cnt);
  Uint32 get_resource_in_use(Uint32 resource) const;
  Uint32 get_resource_lent(Uint32 id) const;
  Uint32 get_resource_borrowed(Uint32 id) const;
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
  Uint32 get_reserved() const;
  Uint32 get_shared() const;
  Uint32 get_spare() const;
  Uint32 get_free_reserved() const;
  Uint32 get_free_shared() const;
  Uint32 get_in_use() const;
  Uint32 get_reserved_in_use() const;
  Uint32 get_shared_in_use() const;
  Uint32 get_max_page() const;
  Uint32 get_resource_free(Uint32 id) const;
  Uint32 get_resource_free_reserved(Uint32 id) const;
  Uint32 get_resource_free_shared(Uint32 id) const;
  Uint32 get_resource_free_lent(Uint32 id) const;
  Uint32 get_resource_reserved(Uint32 id) const;
  Uint32 get_resource_spare(Uint32 resource) const;
  void dec_free_reserved(Uint32 cnt);
  void dec_untaken(Uint32 cnt);
  void dec_borrowed(Uint32 cnt);
  void dec_lent(Uint32 cnt);
  void inc_borrowed(Uint32 cnt);
  void inc_lent(Uint32 cnt);
  void inc_untaken(Uint32 cnt);
  void inc_resource_lent(Uint32 id, Uint32 cnt);
  void inc_resource_borrowed(Uint32 id, Uint32 cnt);
  void set_max_page(Uint32 page);
  void set_allocated(Uint32 cnt);
  void set_free_reserved(Uint32 cnt);
  void update_low_prio_shared_limit();

  Uint32 post_alloc_resource_pages(Uint32 id, Uint32 cnt);
  void post_release_resource_pages(Uint32 id, Uint32 cnt);
  void post_alloc_resource_spare(Uint32 id, Uint32 cnt);

  bool give_up_pages(Uint32 id, Uint32 cnt);
  bool take_pages(Uint32 id, Uint32 cnt);
  void reclaim_lent_pages(Uint32 id, Uint32 cnt);

  void check() const;
  void dump() const;
};

class Ndbd_mem_manager 
{
  friend class Test_mem_manager;
public:
  Ndbd_mem_manager();
  
  void set_resource_limit(const Resource_limit& rl);
  bool get_resource_limit(Uint32 id, Resource_limit& rl) const;
  bool get_resource_limit_nolock(Uint32 id, Resource_limit& rl) const;

  Uint32 get_allocated() const;
  Uint32 get_reserved() const;
  Uint32 get_shared() const;
  Uint32 get_free_shared() const;
  Uint32 get_free_shared_nolock() const;
  Uint32 get_spare() const;
  Uint32 get_in_use() const;
  Uint32 get_reserved_in_use() const;
  Uint32 get_shared_in_use() const;

  bool init(Uint32 *watchCounter, Uint32 pages, bool allow_alloc_less_than_requested = true);
  void map(Uint32 * watchCounter, bool memlock = false, Uint32 resources[] = 0);
  void init_resource_spare(Uint32 id, Uint32 pct);
  void* get_memroot() const;
  
  void dump(bool locked) const ;
  void dump_on_alloc_fail(bool on);

  enum AllocZone
  {
    NDB_ZONE_LE_19 = 0, // Only allocate with page_id < (1 << 19)
    NDB_ZONE_LE_27 = 1,
    NDB_ZONE_LE_30 = 2,
    NDB_ZONE_LE_32 = 3,
  };

  void* get_page(Uint32 i) const; /* Note, no checks, i must be valid. */
  void* get_valid_page(Uint32 i) const; /* DO NOT USE see why at definition */

  void* alloc_page(Uint32 type,
                   Uint32* i,
                   enum AllocZone,
                   bool locked = false,
                   bool use_max_part = true);
  void* alloc_spare_page(Uint32 type, Uint32* i, enum AllocZone);
  void release_page(Uint32 type, Uint32 i, bool locked = false);
  
  void alloc_pages(Uint32 type,
                   Uint32* i,
                   Uint32 *cnt,
                   Uint32 min = 1,
                   AllocZone zone = NDB_ZONE_LE_32,
                   bool locked = false);
  void release_pages(Uint32 type, Uint32 i, Uint32 cnt, bool locked = false);

  bool give_up_pages(Uint32 type, Uint32 cnt);
  bool take_pages(Uint32 type, Uint32 cnt);

  void lock();
  void unlock();

  /**
   * Compute 2log of size 
   * @note size = 0     -> 0
   * @note size > 65536 -> 16
   */
  static Uint32 ndb_log2(Uint32 size);

private:
  enum { ZONE_19 = 0, ZONE_27 = 1, ZONE_30 = 2, ZONE_32 = 3, ZONE_COUNT = 4 };
  enum : Uint32 {
    ZONE_19_BOUND = (1U << 19U),
    ZONE_27_BOUND = (1U << 27U),
    ZONE_30_BOUND = (1U << 30U),
    ZONE_32_BOUND = (RNIL)
  };

  struct PageInterval
  {
    PageInterval(Uint32 start = 0, Uint32 end = 0)
    : start(start), end(end) {}
    static int compare(const void* x, const void* y);

    Uint32 start; /* inclusive */
    Uint32 end; /* exclusive */
  };

  static const Uint32 zone_bound[ZONE_COUNT];
  void grow(Uint32 start, Uint32 cnt);
  bool do_virtual_alloc(Uint32 pages,
                        InitChunk chunks[ZONE_COUNT],
                        Uint32* watchCounter,
                        Alloc_page** base_address);

  /**
   * Return pointer to free page data on page
   */
  static Free_page_data* get_free_page_data(Alloc_page*, Uint32 idx);
  Vector<Uint32> m_used_bitmap_pages;
  
  Uint32 m_buddy_lists[ZONE_COUNT][16];
  Resource_limits m_resource_limits;
  Alloc_page * m_base_page;
#ifdef NDBD_RANDOM_START_PAGE
  Uint32 m_random_start_page_id;
#endif
  bool m_dump_on_alloc_fail;

  /**
   * m_mapped_page is used by get_valid_page() to determine what pages are
   * mapped into memory.
   *
   * This is normally not changed but still some thread safety is needed for
   * the rare cases when changes do happen whenever map() is called.
   *
   * A static array is used since pointers can not be protected by NdbSeqLock.
   *
   * Normally all page memory are allocated in one big chunk, but in debug mode
   * with do_virtual_alloc activated there will be at least one chunk per zone.
   * An arbitrary factor 3 is used to still handle with other unseen allocation
   * patterns.
   *
   * Intervals in m_mapped_pages consists of interval start (inclusive) and
   * end (exclusive).  No two intervals overlap.  Intervals are sorted on start
   * page number with lowest number first.
   */
  mutable NdbSeqLock m_mapped_pages_lock;
  Uint32 m_mapped_pages_count;
  /**
   * m_mapped_pages[0 to m_mapped_pages_count - 1] is protected by seqlock.
   * upper part is protected by same means as m_mapped_pages_new_count.
   */
  PageInterval m_mapped_pages[ZONE_COUNT * 3];
  /**
   * m_mapped_pages_new_count is not protected by seqlock but depends on calls
   * to grow() via map() is serialized by other means.
   */
  Uint32 m_mapped_pages_new_count;

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
void Resource_limits::reclaim_lent_pages(Uint32 id, Uint32 cnt)
{
  const Uint32 resource_lent = get_resource_lent(id);
  const Uint32 returned_lent = m_lent - (m_untaken + m_borrowed);
  const Uint32 reclaim_lent = std::min({cnt, returned_lent, resource_lent});
  if (reclaim_lent > 0)
  {
    dec_resource_lent(id, reclaim_lent);
    dec_lent(reclaim_lent);
    inc_free_reserved(reclaim_lent);
  }
}

inline
Uint32 Resource_limits::post_alloc_resource_pages(Uint32 id, Uint32 cnt)
{
  const Uint32 inuse = get_resource_in_use(id) +
                       get_resource_spare(id) +
                       get_resource_lent(id);
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
  if (rl.m_min > rl.m_curr + rl.m_spare + rl.m_lent)
  {
    spare_res = rl.m_min - (rl.m_curr + rl.m_spare + rl.m_lent);
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
  assert(rl.m_max >= rl.m_curr + rl.m_spare + spare_res + rl.m_lent);
  Uint32 limit = rl.m_max - (rl.m_curr + rl.m_spare + spare_res + rl.m_lent);
  if (free_shr > limit)
  {
    free_shr = limit;
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

  require(rl.m_max >= rl.m_curr + rl.m_spare);

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
Uint32 Resource_limits::get_reserved() const
{
  Uint32 reserved = 0;
  for (Uint32 id = 1; id <= MM_RG_COUNT; id++)
  {
    reserved += get_resource_reserved(id);
  }
  return reserved;
}

inline
Uint32 Resource_limits::get_shared() const
{
  const Uint32 reserved = get_reserved();
  const Uint32 allocated = get_allocated();
  if (allocated < reserved)
    return 0;
  return allocated - reserved;
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
  const Uint32 used = m_free_reserved + m_in_use + m_spare;
  const Uint32 total = m_allocated;
  /*
   * When called from get_free_shared_nolock ensure that total is not less
   * than used.
   */
  if (unlikely(total < used))
  {
    return 0;
  }
  return total - used;
}

inline
Uint32 Resource_limits::get_in_use() const
{
  return m_in_use + m_untaken;
}

inline
Uint32 Resource_limits::get_reserved_in_use() const
{
  return get_reserved() - get_free_reserved();
}

inline
Uint32 Resource_limits::get_shared_in_use() const
{
  return get_shared() - get_free_shared();
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
  assert(rl.m_curr + rl.m_spare + rl.m_lent <= rl.m_max);
  return rl.m_max - (rl.m_curr + rl.m_spare + rl.m_lent);
}

inline
Uint32 Resource_limits::get_resource_free_reserved(Uint32 id) const
{
  require(id <= MM_RG_COUNT);
  const Resource_limit& rl = m_limit[id - 1];
  if (rl.m_min > (rl.m_curr + rl.m_spare + rl.m_lent))
  {
     return rl.m_min - (rl.m_curr + rl.m_spare + rl.m_lent);
  }
  return 0;
}

inline
Uint32 Resource_limits::get_resource_free_shared(Uint32 id) const
{
  const Uint32 free_shared = get_free_shared();

  require(id <= MM_RG_COUNT);
  const Resource_limit& rl = m_limit[id - 1];

  if (rl.m_min > 0) /* high prio */
  {
    return free_shared;
  }
  if (free_shared >= m_prio_free_limit) /* low prio */
  {
    return free_shared - m_prio_free_limit;
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
  const Uint32 inuse = get_resource_in_use(id) +
                       get_resource_spare(id) +
                       get_resource_lent(id);
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

  /* If resource have pages borrowed from other resource, return them now.
   * Note that there is no way that this release is exactly for the pages
   * earlier borrowed by take_pages().
   */
  const Uint32 return_borrowed = std::min(cnt, get_resource_borrowed(id));
  if (return_borrowed > 0)
  {
    dec_resource_borrowed(id, return_borrowed);
    dec_borrowed(return_borrowed);
  }

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
void Resource_limits::update_low_prio_shared_limit()
{
  Uint32 shared = get_shared();
  m_prio_free_limit = shared * HIGH_PRIO_FREE_PCT / 100 + 1;
}

inline
void Resource_limits::set_free_reserved(Uint32 cnt)
{
  m_free_reserved = cnt;
}

inline
Uint32 Resource_limits::get_resource_lent(Uint32 id) const
{
  require(id > 0);
  require(id <= MM_RG_COUNT);
  return m_limit[id - 1].m_lent;
}

inline
Uint32 Resource_limits::get_resource_borrowed(Uint32 id) const
{
  require(id > 0);
  require(id <= MM_RG_COUNT);
  return m_limit[id - 1].m_borrowed;
}

inline
void Resource_limits::dec_untaken(Uint32 cnt)
{
  assert(m_untaken >= cnt);
  m_untaken -= cnt;
}

inline
void Resource_limits::dec_borrowed(Uint32 cnt)
{
  assert(m_borrowed >= cnt);
  m_borrowed -= cnt;
}

inline
void Resource_limits::dec_lent(Uint32 cnt)
{
  assert(m_lent >= cnt);
  m_lent -= cnt;
}

inline
void Resource_limits::inc_borrowed(Uint32 cnt)
{
  m_borrowed += cnt;
  assert(m_borrowed >= cnt);
}

inline
void Resource_limits::inc_lent(Uint32 cnt)
{
  m_lent += cnt;
  assert(m_lent >= cnt);
}

inline
void Resource_limits::inc_untaken(Uint32 cnt)
{
  m_untaken += cnt;
  assert(m_untaken >= cnt);
}

inline
void Resource_limits::dec_resource_lent(Uint32 id, Uint32 cnt)
{
  require(id > 0);
  require(id <= MM_RG_COUNT);
  m_limit[id - 1].m_lent -= cnt;
}

inline
void Resource_limits::dec_resource_borrowed(Uint32 id, Uint32 cnt)
{
  require(id > 0);
  require(id <= MM_RG_COUNT);
  m_limit[id - 1].m_borrowed -= cnt;
}

inline
void Resource_limits::inc_resource_lent(Uint32 id, Uint32 cnt)
{
  require(id > 0);
  require(id <= MM_RG_COUNT);
  m_limit[id - 1].m_lent += cnt;
}

inline
void Resource_limits::inc_resource_borrowed(Uint32 id, Uint32 cnt)
{
  require(id > 0);
  require(id <= MM_RG_COUNT);
  m_limit[id - 1].m_borrowed += cnt;
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
void*
Ndbd_mem_manager::get_page(Uint32 page_num) const
{
#ifdef NDBD_RANDOM_START_PAGE
  page_num -= m_random_start_page_id;
#endif
  return (void*)(m_base_page + page_num);
}
/**
 * get_valid_page returns page pointer if requested page is handled by
 * Ndbd_mem_manager, otherwise it returns NULL.
 *
 * Note: Use of function in release code paths should be regarded as bugs.
 * Accessing a page through a potentially invalid page reference is never a
 * good idea.
 *
 * This function is typically used for converting legacy code using static
 * arrays of records to dynamically allocated records.
 * For these static arrays there has been possible to inspect state of freed
 * records to determine that they are free.  Still this was a weak way to ensure
 * if the reference to record actually is to the right version of record.
 *
 * In some cases it is used to dump the all records of a kind for debugging
 * purposes, for these cases this function provides a mean to implement this
 * in a way to at least minimize risk for memory faults leading to program
 * exit.
 *
 * In any case one should strive to remove this function!
 */
inline
void*
Ndbd_mem_manager::get_valid_page(Uint32 page_num) const
{
#ifdef NDBD_RANDOM_START_PAGE
  page_num -= m_random_start_page_id;
#endif
  const Uint32 page_region_index = page_num & PAGE_REGION_MASK;
  if (unlikely(page_region_index == 0 ||
               page_region_index == PAGE_REGION_MASK))
  {
    /**
     * First page in region are used internally for bitmap.
     * Last page is region is reserved for no use.
     */
#ifdef NDBD_RANDOM_START_PAGE
    g_eventLogger->info(
        "Warning: Ndbd_mem_manager::get_valid_page: internal page %u %u",
        (page_num + m_random_start_page_id), page_num);
#else
    g_eventLogger->info(
        "Warning: Ndbd_mem_manager::get_valid_page: internal page %u",
        page_num);
#endif
#ifdef VM_TRACE
    abort();
#endif
    return NULL;
  }
  bool page_is_mapped;
  Uint32 lock_value;
  do {
    lock_value = m_mapped_pages_lock.read_lock();

    Uint32 a = 0; /* inclusive lower limit */
    Uint32 z = m_mapped_pages_count; /* exclusive upper limit */
    page_is_mapped = false;
    while (a < z)
    {
      Uint32 i = (a + z) / 2;
      if (page_num < m_mapped_pages[i].start)
      {
        z = i;
      }
      else if (page_num < m_mapped_pages[i].end)
      {
        page_is_mapped = true;
        break;
      }
      else
      {
        a = i + 1;
      }
    }
  } while (!m_mapped_pages_lock.read_unlock(lock_value));

  if (unlikely(!page_is_mapped))
  {
#ifdef NDBD_RANDOM_START_PAGE
    g_eventLogger->info(
        "Warning: Ndbd_mem_manager::get_valid_page: unmapped page %u %u",
        (page_num + m_random_start_page_id), page_num);
#else
    g_eventLogger->info(
        "Warning: Ndbd_mem_manager::get_valid_page: unmapped page %u",
        page_num);
#endif
#ifdef VM_TRACE
    abort();
#endif
    return NULL;
  }

  return (void*)(m_base_page + page_num);
}

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
