/*
   Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.

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


#include "ndbd_malloc_impl.hpp"
#include <ndb_global.h>
#include <EventLogger.hpp>
#include <portlib/NdbMem.h>

#define PAGES_PER_REGION_LOG BPP_2LOG

#ifdef _WIN32
void *sbrk(int increment)
{
  return (void*)-1;
}
#endif

extern EventLogger * g_eventLogger;

static int f_method_idx = 0;
#ifdef NDBD_MALLOC_METHOD_SBRK
static const char * f_method = "SMsm";
#else
static const char * f_method = "MSms";
#endif
#define MAX_CHUNKS 10

/*
 * For muti-threaded ndbd, these calls are used for locking around
 * memory allocation operations.
 *
 * For single-threaded ndbd, they are no-ops (but still called, to avoid
 * having to compile this file twice).
 */
extern void mt_mem_manager_init();
extern void mt_mem_manager_lock();
extern void mt_mem_manager_unlock();

#include <NdbOut.hpp>

extern void ndbd_alloc_touch_mem(void * p, size_t sz, volatile Uint32 * watchCounter);

const Uint32 Ndbd_mem_manager::zone_bound[ZONE_COUNT] =
{ /* bound in regions */
  ZONE_19_BOUND >> PAGES_PER_REGION_LOG,
  ZONE_27_BOUND >> PAGES_PER_REGION_LOG,
  ZONE_30_BOUND >> PAGES_PER_REGION_LOG,
  ZONE_32_BOUND >> PAGES_PER_REGION_LOG
};

/**
 * do_virtual_alloc uses debug functions NdbMem_ReserveSpace and
 * NdbMem_PopulateSpace to be able to use as high page numbers as possible for
 * each memory region.  Using high page numbers will likely lure bugs due to
 * storing not all required bits of page numbers.
 */

#ifdef VM_TRACE
#if defined(_WIN32) || defined(MADV_DONTDUMP)
/**
 * For Windows and Linux measures are taken not to dump the whole virtual
 * memory reserved but only those pages that are populated.
 * The linux support depends on having MADV_DONTDUMP defined.
 * For other OS do_virtual_alloc should not be used since it will produce huge
 * core dumps if crashing.
 */
#define USE_DO_VIRTUAL_ALLOC
#elif defined(USE_DO_VIRTUAL_ALLOC)
#error do_virtual_alloc is not supported, please undefine USE_DO_VIRTUAL_ALLOC.
#endif
#endif

#ifdef USE_DO_VIRTUAL_ALLOC

/*
 * To verify that the maximum of 16383 regions can be reserved without failure
 * in do_virtual_alloc define NDB_TEST_128TB_VIRTUAL_MEMORY, data node should
 * exit early with exit status 0, anything else is an error.
 * Look for NdbMem printouts in data node log.
 *
 * Also see Bug#28961597.
 */
//#define NDB_TEST_128TB_VIRTUAL_MEMORY
#ifdef NDB_TEST_128TB_VIRTUAL_MEMORY

static inline int
log_and_fake_success(const char func[], int line,
                     const char msg[], void* p, size_t s)
{
  ndbout_c("DEBUG: %s: %u: %s: p %p: len %zu",
           func, line, msg, p, s);
  return 0;
}

#define NdbMem_ReserveSpace(x,y) \
  log_and_fake_success(__func__, __LINE__, "NdbMem_ReserveSpace", (x), (y))

#define NdbMem_PopulateSpace(x,y) \
  log_and_fake_success(__func__, __LINE__, "NdbMem_PopulateSpace", (x), (y))

#endif

bool
Ndbd_mem_manager::do_virtual_alloc(Uint32 pages,
                                   InitChunk chunks[ZONE_COUNT],
                                   Uint32* watchCounter,
                                   Alloc_page** base_address)
{
  if (watchCounter)
    *watchCounter = 9;
  const Uint32 max_regions = zone_bound[ZONE_COUNT - 1];
  const Uint32 max_pages = max_regions << PAGES_PER_REGION_LOG;
  require(max_regions == (max_pages >> PAGES_PER_REGION_LOG));
  require(max_regions > 0); // TODO static_assert
  if (pages > max_pages)
  {
    return false;
  }
  const bool half_space = (pages <= (max_pages >> 1));

  /* Find out page count per zone */
  Uint32 page_count[ZONE_COUNT];
  Uint32 region_count[ZONE_COUNT];
  Uint32 prev_bound = 0;
  for (int i = 0; i < ZONE_COUNT; i++)
  {
    Uint32 n = pages / (ZONE_COUNT - i);
    if (half_space && n > (zone_bound[i] << (PAGES_PER_REGION_LOG - 1)))
    {
      n = zone_bound[i] << (PAGES_PER_REGION_LOG - 1);
    }
    else if (n > ((zone_bound[i] - prev_bound) << PAGES_PER_REGION_LOG))
    {
      n = (zone_bound[i] - prev_bound) << PAGES_PER_REGION_LOG;
    }
    page_count[i] = n;
    region_count[i] = (n + 256 * 1024 - 1) / (256 * 1024);
    prev_bound = zone_bound[i];
    pages -= n;
  }
  require(pages == 0);

  /* Reserve big enough continuous address space */
  require(ZONE_COUNT >= 2); // TODO static assert
  const Uint32 highest_low = zone_bound[0] - region_count[0];
  const Uint32 lowest_high = zone_bound[ZONE_COUNT - 2] +
                             region_count[ZONE_COUNT - 1];
  const Uint32 least_region_count = lowest_high - highest_low;
  Uint32 space_regions = max_regions;
  Alloc_page *space;
  int rc = -1;
  while (space_regions >= least_region_count)
  {
    if (watchCounter)
      *watchCounter = 9;
    rc = NdbMem_ReserveSpace(
           (void**)&space,
           (space_regions << PAGES_PER_REGION_LOG) * Uint64(32768));
    if (watchCounter)
      *watchCounter = 9;
    if (rc == 0)
    {
      ndbout_c("%s: Reserved address space for %u 8GiB regions at %p.",
               __func__,
              space_regions,
              space);
      break;
    }
    space_regions = (space_regions - 1 + least_region_count) / 2;
  }
  if (rc == -1)
  {
    ndbout_c("%s: Failed reserved address space for at least %u 8GiB regions.",
             __func__,
             least_region_count);
    return false;
  }

#ifdef NDBD_RANDOM_START_PAGE
  Uint32 range = highest_low;
  for (int i = 0; i < ZONE_COUNT; i++)
  {
    Uint32 rmax = (zone_bound[i] << PAGES_PER_REGION_LOG) - page_count[i];
    if (i > 0)
    {
      rmax -= zone_bound[i - 1] << PAGES_PER_REGION_LOG;
    }
    if (half_space)
    {
      rmax -= 1 << 17; /* lower half of region */
    }
    if (range > rmax)
    {
      rmax = range;
    }
  }
  m_random_start_page_id = rand() % range;
#endif

  Uint32 first_region[ZONE_COUNT];
  for (int i = 0; i < ZONE_COUNT; i++)
  {
    first_region[i] = (i < ZONE_COUNT - 1)
                      ? zone_bound[i]
                      : MIN(first_region[0] + space_regions, max_regions);
    first_region[i] -= ((page_count[i] +
#ifdef NDBD_RANDOM_START_PAGE
                         m_random_start_page_id +
#endif
                         ((1 << PAGES_PER_REGION_LOG) - 1))
                        >> PAGES_PER_REGION_LOG);

    if (watchCounter)
      *watchCounter = 9;
    rc = NdbMem_PopulateSpace(
           space + (first_region[i] - first_region[0]) * 8 * Uint64(32768),
           page_count[i] * Uint64(32768));
    if (watchCounter)
      *watchCounter = 9;
    if (rc != 0)
    {
      if (watchCounter)
        *watchCounter = 9;
      NdbMem_FreeSpace(
        space,
        (space_regions << PAGES_PER_REGION_LOG) * Uint64(32768));
      if (watchCounter)
        *watchCounter = 9;
      return false;
    }
    chunks[i].m_cnt = page_count[i];
    chunks[i].m_ptr = space + ((first_region[i] - first_region[0])
                                << PAGES_PER_REGION_LOG);
#ifndef NDBD_RANDOM_START_PAGE
    const Uint32 first_page = first_region[i] << PAGES_PER_REGION_LOG;
#else
    const Uint32 first_page = (first_region[i] << PAGES_PER_REGION_LOG) +
                              m_random_start_page_id;
#endif
    const Uint32 last_page = first_page + chunks[i].m_cnt - 1;
    ndbout_c("%s: Populated space with pages %u to %u at %p.",
             __func__,
             first_page,
             last_page,
             chunks[i].m_ptr);
    require(last_page < (zone_bound[i] << PAGES_PER_REGION_LOG));
  }
  *base_address = space - first_region[0] * 8 * Uint64(32768);
  if (watchCounter)
    *watchCounter = 9;
#ifdef NDB_TEST_128TB_VIRTUAL_MEMORY
  exit(0); // No memory mapped only faking no meaning to continue.
#endif
  return true;
}
#endif

static
bool
do_malloc(Uint32 pages,
          InitChunk* chunk,
          Uint32 *watchCounter,
          void * baseaddress)
{
  pages += 1;
  void * ptr = 0;
  Uint32 sz = pages;

retry:
  if (watchCounter)
    *watchCounter = 9;

  char method = f_method[f_method_idx];
  switch(method){
  case 0:
    return false;
  case 'S':
  case 's':
  {
    ptr = 0;
    while (ptr == 0)
    {
      if (watchCounter)
        *watchCounter = 9;

      ptr = sbrk(sizeof(Alloc_page) * sz);
      
      if (ptr == (void*)-1)
      {
	if (method == 'S')
	{
	  f_method_idx++;
	  goto retry;
	}
	
	ptr = 0;
	sz = 1 + (9 * sz) / 10;
	if (pages >= 32 && sz < 32)
	{
	  sz = pages;
	  f_method_idx++;
	  goto retry;
	}
      }
      else if (UintPtr(ptr) < UintPtr(baseaddress))
      {
        /**
         * Unusable memory :(
         */
        ndbout_c("sbrk(%lluMb) => %p which is less than baseaddress!!",
                 Uint64((sizeof(Alloc_page) * sz) >> 20), ptr);
        f_method_idx++;
        goto retry;
      }
    }
    break;
  }
  case 'M':
  case 'm':
  {
    ptr = 0;
    while (ptr == 0)
    {
      if (watchCounter)
        *watchCounter = 9;

      ptr = malloc(sizeof(Alloc_page) * sz);
      if (UintPtr(ptr) < UintPtr(baseaddress))
      {
        ndbout_c("malloc(%lluMb) => %p which is less than baseaddress!!",
                 Uint64((sizeof(Alloc_page) * sz) >> 20), ptr);
        free(ptr);
        ptr = 0;
      }

      if (ptr == 0)
      {
	if (method == 'M')
	{
	  f_method_idx++;
	  goto retry;
	}

	sz = 1 + (9 * sz) / 10;
	if (pages >= 32 && sz < 32)
	{
	  f_method_idx++;
	  goto retry;
	}
      }
    }
    break;
  }
  default:
    return false;
  }
  
  chunk->m_cnt = sz;
  chunk->m_ptr = (Alloc_page*)ptr;
  const UintPtr align = sizeof(Alloc_page) - 1;
  if (UintPtr(ptr) & align)
  {
    chunk->m_cnt--;
    chunk->m_ptr = (Alloc_page*)((UintPtr(ptr) + align) & ~align);
  }

#ifdef UNIT_TEST
  ndbout_c("do_malloc(%d) -> %p %d", pages, ptr, chunk->m_cnt);
  if (1)
  {
    Uint32 sum = 0;
    Alloc_page* page = chunk->m_ptr;
    for (Uint32 i = 0; i<chunk->m_cnt; i++, page++)
    {
      page->m_data[0*1024] = 0;
      page->m_data[1*1024] = 0;
      page->m_data[2*1024] = 0;
      page->m_data[3*1024] = 0;
      page->m_data[4*1024] = 0;
      page->m_data[5*1024] = 0;
      page->m_data[6*1024] = 0;
      page->m_data[7*1024] = 0;
    }
  }
#endif
  
  return true;
}

/**
 * Resource_limits
 */

Resource_limits::Resource_limits()
{
  m_allocated = 0;
  m_free_reserved = 0;
  m_in_use = 0;
  m_spare = 0;
  m_max_page = 0;
  m_prio_free_limit = 0;
  memset(m_limit, 0, sizeof(m_limit));
}

#ifndef VM_TRACE
inline
#endif
void
Resource_limits::check() const
{
#ifdef VM_TRACE
  const Resource_limit* rl = m_limit;
  Uint32 curr = 0;
  Uint32 spare = 0;
  Uint32 res_alloc = 0;
  Uint32 shared_alloc = 0;
  Uint32 sumres = 0;
  for (Uint32 i = 0; i < MM_RG_COUNT; i++)
  {
    curr += rl[i].m_curr;
    spare += rl[i].m_spare;
    sumres += rl[i].m_min;
    // assert(rl[i].m_max == 0 || rl[i].m_curr <= rl[i].m_max);
    if (rl[i].m_curr + rl[i].m_spare > rl[i].m_min)
    {
      shared_alloc += rl[i].m_curr + rl[i].m_spare - rl[i].m_min;
      res_alloc += rl[i].m_min;
    }
    else
    {
      res_alloc += rl[i].m_curr + rl[i].m_spare;
    }
  }

  if(!((curr == get_in_use()) &&
       (spare == get_spare()) &&
       (res_alloc + shared_alloc == curr + spare) &&
       (res_alloc <= sumres) &&
       (sumres == res_alloc + get_free_reserved()) &&
       (get_in_use() + get_spare() <= get_allocated())))
  {
    dump();
  }

  assert(curr == get_in_use());
  assert(spare == get_spare());
  assert(res_alloc + shared_alloc == curr + spare);
  assert(res_alloc <= sumres);
  assert(sumres == res_alloc + get_free_reserved());
  assert(get_in_use() + get_spare() <= get_allocated());
#endif
}

void
Resource_limits::dump() const
{
  printf("ri: global "
         "max_page: %u free_reserved: %u in_use: %u allocated: %u spare: %u\n",
         m_max_page,
         m_free_reserved,
         m_in_use,
         m_allocated,
         m_spare);
  for (Uint32 i = 0; i < MM_RG_COUNT; i++)
  {
    printf("ri: %u id: %u min: %u curr: %u max: %u spare: %u spare_pct: %u\n",
           i,
           m_limit[i].m_resource_id,
           m_limit[i].m_min,
           m_limit[i].m_curr,
           m_limit[i].m_max,
           m_limit[i].m_spare,
           m_limit[i].m_spare_pct);
  }
}

/**
 *
 * resource N has following semantics:
 *
 * m_min = reserved
 * m_curr = currently used
 * m_max = max alloc, 0 = no limit
 *
 */
void
Resource_limits::init_resource_limit(Uint32 id, Uint32 min, Uint32 max)
{
  assert(id > 0);
  assert(id <= MM_RG_COUNT);

  m_limit[id - 1].m_resource_id = id;
  m_limit[id - 1].m_curr = 0;
  m_limit[id - 1].m_max = max;

  m_limit[id - 1].m_min = min;

  Uint32 reserve = min;
  Uint32 current_reserved = get_free_reserved();
  set_free_reserved(current_reserved + reserve);
}

void
Resource_limits::init_resource_spare(Uint32 id, Uint32 pct)
{
  require(m_limit[id - 1].m_spare_pct == 0);
  m_limit[id - 1].m_spare_pct = pct;

  (void) alloc_resource_spare(id, 0);
}

/**
 * Ndbd_mem_manager
 */

int
Ndbd_mem_manager::PageInterval::compare(const void* px, const void* py)
{
  const PageInterval* x = static_cast<const PageInterval*>(px);
  const PageInterval* y = static_cast<const PageInterval*>(py);

  if (x->start < y->start)
  {
    return -1;
  }
  if (x->start > y->start)
  {
    return +1;
  }
  if (x->end < y->end)
  {
    return -1;
  }
  if (x->end > y->end)
  {
    return +1;
  }
  return 0;
}

Uint32
Ndbd_mem_manager::ndb_log2(Uint32 input)
{
  if (input > 65535)
    return 16;
  input = input | (input >> 8);
  input = input | (input >> 4);
  input = input | (input >> 2);
  input = input | (input >> 1);
  Uint32 output = (input & 0x5555) + ((input >> 1) & 0x5555);
  output = (output & 0x3333) + ((output >> 2) & 0x3333);
  output = output + (output >> 4);
  output = (output & 0xf) + ((output >> 8) & 0xf);
  return output;
}

Ndbd_mem_manager::Ndbd_mem_manager()
: m_base_page(NULL),
  m_dump_on_alloc_fail(false),
  m_mapped_pages_count(0),
  m_mapped_pages_new_count(0)
{
  memset(m_buddy_lists, 0, sizeof(m_buddy_lists));

  if (sizeof(Free_page_data) != (4 * (1 << FPD_2LOG)))
  {
    g_eventLogger->error("Invalid build, ndbd_malloc_impl.cpp:%d", __LINE__);
    abort();
  }
  mt_mem_manager_init();
}

void*
Ndbd_mem_manager::get_memroot() const
{
#ifdef NDBD_RANDOM_START_PAGE
  return (void*)(m_base_page - m_random_start_page_id);
#else
  return (void*)m_base_page;
#endif
}

/**
 *
 * resource N has following semantics:
 *
 * m_min = reserved
 * m_curr = currently used including spare pages
 * m_max = max alloc, 0 = no limit
 * m_spare = pages reserved for restart or special use
 *
 */
void
Ndbd_mem_manager::set_resource_limit(const Resource_limit& rl)
{
  require(rl.m_resource_id > 0);
  mt_mem_manager_lock();
  m_resource_limits.init_resource_limit(rl.m_resource_id, rl.m_min, rl.m_max);
  mt_mem_manager_unlock();
}

bool
Ndbd_mem_manager::get_resource_limit(Uint32 id, Resource_limit& rl) const
{
  /**
   * DUMP DumpPageMemory(1000) is agnostic about what resource groups exists.
   * Allowing use of any id.
   */
  if (1 <= id && id <= MM_RG_COUNT)
  {
    mt_mem_manager_lock();
    m_resource_limits.get_resource_limit(id, rl);
    mt_mem_manager_unlock();
    return true;
  }
  return false;
}

bool
Ndbd_mem_manager::get_resource_limit_nolock(Uint32 id, Resource_limit& rl) const
{
  assert(id > 0);
  if (id <= MM_RG_COUNT)
  {
    m_resource_limits.get_resource_limit(id, rl);
    return true;
  }
  return false;
}

Uint32
Ndbd_mem_manager::get_allocated() const
{
  return m_resource_limits.get_allocated();
}

Uint32
Ndbd_mem_manager::get_reserved() const
{
  return m_resource_limits.get_reserved();
}

Uint32
Ndbd_mem_manager::get_shared() const
{
  return m_resource_limits.get_shared();
}

Uint32
Ndbd_mem_manager::get_spare() const
{
  return m_resource_limits.get_spare();
}

Uint32
Ndbd_mem_manager::get_in_use() const
{
  return m_resource_limits.get_in_use();
}

Uint32
Ndbd_mem_manager::get_reserved_in_use() const
{
  return m_resource_limits.get_reserved_in_use();
}

Uint32
Ndbd_mem_manager::get_shared_in_use() const
{
  return m_resource_limits.get_shared_in_use();
}

int
cmp_chunk(const void * chunk_vptr_1, const void * chunk_vptr_2)
{
  InitChunk * ptr1 = (InitChunk*)chunk_vptr_1;
  InitChunk * ptr2 = (InitChunk*)chunk_vptr_2;
  if (ptr1->m_ptr < ptr2->m_ptr)
    return -1;
  if (ptr1->m_ptr > ptr2->m_ptr)
    return 1;
  assert(false);
  return 0;
}

bool
Ndbd_mem_manager::init(Uint32 *watchCounter, Uint32 max_pages , bool alloc_less_memory)
{
  assert(m_base_page == 0);
  assert(max_pages > 0);
  assert(m_resource_limits.get_allocated() == 0);

  if (watchCounter)
    *watchCounter = 9;

  Uint32 pages = max_pages;
  Uint32 max_page = 0;
  
  const Uint64 pg = Uint64(sizeof(Alloc_page));
  g_eventLogger->info("Ndbd_mem_manager::init(%d) min: %lluMb initial: %lluMb",
                      alloc_less_memory,
                      (pg*m_resource_limits.get_free_reserved())>>20,
                      (pg*pages) >> 20);

  if (pages == 0)
  {
    return false;
  }

#if SIZEOF_CHARP == 4
  Uint64 sum = (pg*pages); 
  if (sum >= (Uint64(1) << 32))
  {
    g_eventLogger->error("Trying to allocate more that 4Gb with 32-bit binary!!");
    return false;
  }
#endif

  Uint32 allocated = 0;
  m_base_page = NULL;

#ifdef USE_DO_VIRTUAL_ALLOC
  {
    InitChunk chunks[ZONE_COUNT];
    if (do_virtual_alloc(pages, chunks, watchCounter, &m_base_page))
    {
      for (int i = 0; i < ZONE_COUNT; i++)
      {
        m_unmapped_chunks.push_back(chunks[i]);
        allocated += chunks[i].m_cnt;
      }
      require(allocated == pages);
    }
  }
#endif

#ifdef NDBD_RANDOM_START_PAGE
  if (m_base_page == NULL)
  {
    /**
     * In order to find bad-users of page-id's
     *   we add a random offset to the page-id's returned
     *   however, due to ZONE_19 that offset can't be that big
     *   (since we at get_page don't know if it's a HI/LO page)
     */
    Uint32 max_rand_start = ZONE_19_BOUND - 1;
    if (max_rand_start > pages)
    {
      max_rand_start -= pages;
      if (max_rand_start > 0x10000)
        m_random_start_page_id =
          0x10000 + (rand() % (max_rand_start - 0x10000));
      else if (max_rand_start)
        m_random_start_page_id = rand() % max_rand_start;

      assert(Uint64(pages) + Uint64(m_random_start_page_id) <= 0xFFFFFFFF);

      ndbout_c("using m_random_start_page_id: %u (%.8x)",
               m_random_start_page_id, m_random_start_page_id);
    }
  }
#endif

  /**
   * Do malloc
   */
  while (m_unmapped_chunks.size() < MAX_CHUNKS && allocated < pages)
  {
    InitChunk chunk;
    memset(&chunk, 0, sizeof(chunk));
    
    if (do_malloc(pages - allocated, &chunk, watchCounter, m_base_page))
    {
      if (watchCounter)
        *watchCounter = 9;

      m_unmapped_chunks.push_back(chunk);
      allocated += chunk.m_cnt;
    }
    else
    {
      break;
    }
  }
  
  if (allocated < m_resource_limits.get_free_reserved())
  {
    g_eventLogger->
      error("Unable to alloc min memory from OS: min: %lldMb "
            " allocated: %lldMb",
            (Uint64)(sizeof(Alloc_page)*m_resource_limits.get_free_reserved()) >> 20,
            (Uint64)(sizeof(Alloc_page)*allocated) >> 20);
    return false;
  }
  else if (allocated < pages)
  {
    g_eventLogger->
      warning("Unable to alloc requested memory from OS: min: %lldMb"
              " requested: %lldMb allocated: %lldMb",
              (Uint64)(sizeof(Alloc_page)*m_resource_limits.get_free_reserved())>>20,
              (Uint64)(sizeof(Alloc_page)*max_pages)>>20,
              (Uint64)(sizeof(Alloc_page)*allocated)>>20);
    if (!alloc_less_memory)
      return false;
  }

  if (m_base_page == NULL)
  {
    /**
     * Sort chunks...
     */
    qsort(m_unmapped_chunks.getBase(), m_unmapped_chunks.size(),
          sizeof(InitChunk), cmp_chunk);

    m_base_page = m_unmapped_chunks[0].m_ptr;
  }

  for (Uint32 i = 0; i<m_unmapped_chunks.size(); i++)
  {
    UintPtr start = UintPtr(m_unmapped_chunks[i].m_ptr) - UintPtr(m_base_page);
    start >>= (2 + BMW_2LOG);
    assert((Uint64(start) >> 32) == 0);
    m_unmapped_chunks[i].m_start = Uint32(start);
    Uint64 last64 = start + m_unmapped_chunks[i].m_cnt;
    assert((last64 >> 32) == 0);
    Uint32 last = Uint32(last64);

    if (last > max_page)
      max_page = last;
  }

  m_resource_limits.set_max_page(max_page);
  m_resource_limits.set_allocated(0);

  return true;
}

void
Ndbd_mem_manager::map(Uint32 * watchCounter, bool memlock, Uint32 resources[])
{
  Uint32 limit = ~(Uint32)0;
  Uint32 sofar = 0;

  if (resources != 0)
  {
    limit = 0;
    for (Uint32 i = 0; resources[i] ; i++)
    {
      limit += m_resource_limits.get_resource_reserved(resources[i]);
    }
  }

  while (m_unmapped_chunks.size() && sofar < limit)
  {
    Uint32 remain = limit - sofar;

    unsigned idx = m_unmapped_chunks.size() - 1;
    InitChunk * chunk = &m_unmapped_chunks[idx];
    if (watchCounter)
      *watchCounter = 9;

    if (chunk->m_cnt > remain)
    {
      /**
       * Split chunk
       */
      Uint32 extra = chunk->m_cnt - remain;
      chunk->m_cnt = remain;

      InitChunk newchunk;
      newchunk.m_start = chunk->m_start + remain;
      newchunk.m_ptr = m_base_page + newchunk.m_start;
      newchunk.m_cnt = extra;
      m_unmapped_chunks.push_back(newchunk);

      // pointer could have changed after m_unmapped_chunks.push_back
      chunk = &m_unmapped_chunks[idx];
    }

    g_eventLogger->info("Touch Memory Starting, %u pages, page size = %d",
                        chunk->m_cnt,
                        (int)sizeof(Alloc_page));

    ndbd_alloc_touch_mem(chunk->m_ptr,
                         chunk->m_cnt * sizeof(Alloc_page),
                         watchCounter);

    g_eventLogger->info("Touch Memory Completed");

    if (memlock)
    {
      /**
       * memlock pages that I added...
       */
      if (watchCounter)
        *watchCounter = 9;

      /**
       * Don't memlock everything in one go...
       *   cause then process won't be killable
       */
      const Alloc_page * start = chunk->m_ptr;
      Uint32 cnt = chunk->m_cnt;
      g_eventLogger->info("Lock Memory Starting, %u pages, page size = %d",
                          chunk->m_cnt,
                          (int)sizeof(Alloc_page));

      while (cnt > 32768) // 1G
      {
        if (watchCounter)
          *watchCounter = 9;

        NdbMem_MemLock(start, 32768 * sizeof(Alloc_page));
        start += 32768;
        cnt -= 32768;
      }
      if (watchCounter)
        *watchCounter = 9;

      NdbMem_MemLock(start, cnt * sizeof(Alloc_page));

      g_eventLogger->info("Lock memory Completed");
    }

    grow(chunk->m_start, chunk->m_cnt);
    sofar += chunk->m_cnt;

    m_unmapped_chunks.erase(idx);
  }
  
  mt_mem_manager_lock();
  m_resource_limits.check();
  mt_mem_manager_unlock();

  if (resources == 0 && memlock)
  {
    NdbMem_MemLockAll(1);
  }

  /* Note: calls to map() must be serialized by other means. */
  m_mapped_pages_lock.write_lock();
  if (m_mapped_pages_new_count != m_mapped_pages_count)
  {
    /* Do not support shrinking memory */
    require(m_mapped_pages_new_count > m_mapped_pages_count);

    qsort(m_mapped_pages,
          m_mapped_pages_new_count,
          sizeof(m_mapped_pages[0]),
          PageInterval::compare);

    /* Validate no overlapping intervals */
    for (Uint32 i = 1; i < m_mapped_pages_new_count; i++)
    {
      require(m_mapped_pages[i - 1].end <= m_mapped_pages[i].start);
    }

    m_mapped_pages_count = m_mapped_pages_new_count;
  }
  m_mapped_pages_lock.write_unlock();
}

void
Ndbd_mem_manager::init_resource_spare(Uint32 id, Uint32 pct)
{
  mt_mem_manager_lock();
  m_resource_limits.init_resource_spare(id, pct);
  mt_mem_manager_unlock();
}

#include <NdbOut.hpp>

void
Ndbd_mem_manager::grow(Uint32 start, Uint32 cnt)
{
  assert(cnt);
  Uint32 start_bmp = start >> BPP_2LOG;
  Uint32 last_bmp = (start + cnt - 1) >> BPP_2LOG;
  
#if SIZEOF_CHARP == 4
  assert(start_bmp == 0 && last_bmp == 0);
#endif
  
  if (start_bmp != last_bmp)
  {
    Uint32 tmp = ((start_bmp + 1) << BPP_2LOG) - start;
    grow(start, tmp);
    grow((start_bmp + 1) << BPP_2LOG, cnt - tmp);
    return;
  }

  for (Uint32 i = 0; i<m_used_bitmap_pages.size(); i++)
    if (m_used_bitmap_pages[i] == start_bmp)
    {
      /* m_mapped_pages should contain the ranges of allocated pages.
       * In release build there will typically be one big range.
       * In debug build there are typically four ranges, one per allocation
       * zone.
       * Not all ranges passed to grow() may be used, but for a big range it
       * is only the first partial range that can not be used.
       * This part of code will be called with the range passed to top call to
       * grow() broken up in 8GB regions by recursion above, and the ranges
       * will always be passed with increasing addresses, and the start will
       * match end of previous calls range.
       * To keep use as few entries as possible in m_mapped_pages these
       * adjacent ranges are combined.
       */
      if (m_mapped_pages_new_count > 0 &&
          m_mapped_pages[m_mapped_pages_new_count - 1].end == start)
      {
        m_mapped_pages[m_mapped_pages_new_count - 1].end = start + cnt;
      }
      else
      {
        require(m_mapped_pages_new_count < NDB_ARRAY_SIZE(m_mapped_pages));
        m_mapped_pages[m_mapped_pages_new_count].start = start;
        m_mapped_pages[m_mapped_pages_new_count].end = start + cnt;
        m_mapped_pages_new_count++;
      }
      goto found;
    }

  if (start != (start_bmp << BPP_2LOG))
  {
    
    ndbout_c("ndbd_malloc_impl.cpp:%d:grow(%d, %d) %d!=%d not using %uMb"
	     " - Unable to use due to bitmap pages missaligned!!",
	     __LINE__, start, cnt, start, (start_bmp << BPP_2LOG),
	     (cnt >> (20 - 15)));
    g_eventLogger->error("ndbd_malloc_impl.cpp:%d:grow(%d, %d) not using %uMb"
                         " - Unable to use due to bitmap pages missaligned!!",
                         __LINE__, start, cnt,
                         (cnt >> (20 - 15)));

    dump();
    return;
  }
  
#ifdef UNIT_TEST
  ndbout_c("creating bitmap page %d", start_bmp);
#endif

  if (m_mapped_pages_new_count > 0 &&
      m_mapped_pages[m_mapped_pages_new_count - 1].end == start)
  {
    m_mapped_pages[m_mapped_pages_new_count - 1].end = start + cnt;
  }
  else
  {
    require(m_mapped_pages_new_count < NDB_ARRAY_SIZE(m_mapped_pages));
    m_mapped_pages[m_mapped_pages_new_count].start = start;
    m_mapped_pages[m_mapped_pages_new_count].end = start + cnt;
    m_mapped_pages_new_count++;
  }

  {
    Alloc_page* bmp = m_base_page + start;
    memset(bmp, 0, sizeof(Alloc_page));
    cnt--;
    start++;
  }
  m_used_bitmap_pages.push_back(start_bmp);

found:
  if ((start + cnt) == ((start_bmp + 1) << BPP_2LOG))
  {
    cnt--; // last page is always marked as empty
  }

  if (cnt)
  {
    mt_mem_manager_lock();
    const Uint32 allocated = m_resource_limits.get_allocated();
    m_resource_limits.set_allocated(allocated + cnt);
    const Uint64 mbytes = ((Uint64(cnt)*32) + 1023) / 1024;
    /**
     * grow first split large page ranges to ranges completely within
     * a BPP regions.
     * Boundary between lo and high zone coincide with a BPP region
     * boundary.
     */
    NDB_STATIC_ASSERT((ZONE_19_BOUND & ((1 << BPP_2LOG) - 1)) == 0);
    if (start < ZONE_19_BOUND)
    {
      require(start + cnt < ZONE_19_BOUND);
      g_eventLogger->info("Adding %uMb to ZONE_19 (%u, %u)",
                          (Uint32)mbytes,
                          start,
                          cnt);
    }
    else if (start < ZONE_27_BOUND)
    {
      require(start + cnt < ZONE_27_BOUND);
      g_eventLogger->info("Adding %uMb to ZONE_27 (%u, %u)",
                          (Uint32)mbytes,
                          start,
                          cnt);
    }
    else if (start < ZONE_30_BOUND)
    {
      require(start + cnt < ZONE_30_BOUND);
      g_eventLogger->info("Adding %uMb to ZONE_30 (%u, %u)",
                          (Uint32)mbytes,
                          start,
                          cnt);
    }
    else
    {
      g_eventLogger->info("Adding %uMb to ZONE_32 (%u, %u)",
                          (Uint32)mbytes,
                          start,
                          cnt);
    }
    release(start, cnt);
    mt_mem_manager_unlock();
  }
}

void
Ndbd_mem_manager::release(Uint32 start, Uint32 cnt)
{
  assert(start);
#if defined VM_TRACE || defined ERROR_INSERT
  memset(m_base_page + start, 0xF5, cnt * sizeof(m_base_page[0]));
#endif

  set(start, start+cnt-1);

  Uint32 zone = get_page_zone(start);
  release_impl(zone, start, cnt);
}

void
Ndbd_mem_manager::release_impl(Uint32 zone, Uint32 start, Uint32 cnt)
{
  assert(start);

  Uint32 test = check(start-1, start+cnt);
  if (test & 1)
  {
    Free_page_data *fd = get_free_page_data(m_base_page + start - 1,
					    start - 1);
    Uint32 sz = fd->m_size;
    Uint32 left = start - sz;
    remove_free_list(zone, left, fd->m_list);
    cnt += sz;
    start = left;
  }

  Uint32 right = start + cnt;
  if (test & 2)
  {
    Free_page_data *fd = get_free_page_data(m_base_page+right, right);
    Uint32 sz = fd->m_size;
    remove_free_list(zone, right, fd->m_list);
    cnt += sz;
  }

  insert_free_list(zone, start, cnt);
}

void
Ndbd_mem_manager::alloc(AllocZone zone,
                        Uint32* ret,
                        Uint32 *pages,
                        Uint32 min)
{
  const Uint32 save = * pages;
  for (Uint32 z = zone; ; z--)
  {
    alloc_impl(z, ret, pages, min);
    if (*pages)
    {
#if defined VM_TRACE || defined ERROR_INSERT
      memset(m_base_page + *ret, 0xF6, *pages * sizeof(m_base_page[0]));
#endif
      return;
    }
    if (z == 0)
    {
      if (unlikely(m_dump_on_alloc_fail))
      {
        printf("%s: Page allocation failed: zone=%u pages=%u (at least %u)\n",
               __func__,
               zone,
               save,
               min);
        dump();
      }
      return;
    }
    * pages = save;
  }
}

void
Ndbd_mem_manager::alloc_impl(Uint32 zone,
                             Uint32* ret,
                             Uint32 *pages,
                             Uint32 min)
{
  Int32 i;
  Uint32 start;
  Uint32 cnt = * pages;
  Uint32 list = ndb_log2(cnt - 1);

  assert(cnt);
  assert(list <= 16);

  for (i = list; i < 16; i++)
  {
    if ((start = m_buddy_lists[zone][i]))
    {
/* ---------------------------------------------------------------- */
/*       PROPER AMOUNT OF PAGES WERE FOUND. NOW SPLIT THE FOUND     */
/*       AREA AND RETURN THE PART NOT NEEDED.                       */
/* ---------------------------------------------------------------- */

      Uint32 sz = remove_free_list(zone, start, i);
      Uint32 extra = sz - cnt;
      assert(sz >= cnt);
      if (extra)
      {
	insert_free_list(zone, start + cnt, extra);
	clear_and_set(start, start+cnt-1);
      }
      else
      {
	clear(start, start+cnt-1);
      }
      * ret = start;
      assert(m_resource_limits.get_in_use() + cnt <= m_resource_limits.get_allocated());
      return;
    }
  }

  /**
   * Could not find in quaranteed list...
   *   search in other lists...
   */

  Int32 min_list = ndb_log2(min - 1);
  assert((Int32)list >= min_list);
  for (i = list - 1; i >= min_list; i--) 
  {
    if ((start = m_buddy_lists[zone][i]))
    {
      Uint32 sz = remove_free_list(zone, start, i);
      Uint32 extra = sz - cnt;
      if (sz > cnt)
      {
	insert_free_list(zone, start + cnt, extra);	
	sz -= extra;
	clear_and_set(start, start+sz-1);
      }
      else
      {
	clear(start, start+sz-1);
      }

      * ret = start;
      * pages = sz;
      assert(m_resource_limits.get_in_use() + sz <= m_resource_limits.get_allocated());
      return;
    }
  }
  * pages = 0;
}

void
Ndbd_mem_manager::insert_free_list(Uint32 zone, Uint32 start, Uint32 size)
{
  Uint32 list = ndb_log2(size) - 1;
  Uint32 last = start + size - 1;

  Uint32 head = m_buddy_lists[zone][list];
  Free_page_data* fd_first = get_free_page_data(m_base_page+start, 
						start);
  fd_first->m_list = list;
  fd_first->m_next = head;
  fd_first->m_prev = 0;
  fd_first->m_size = size;

  Free_page_data* fd_last = get_free_page_data(m_base_page+last, last);
  fd_last->m_list = list;
  fd_last->m_next = head;
  fd_last->m_prev = 0;
  fd_last->m_size = size;
  
  if (head)
  {
    Free_page_data* fd = get_free_page_data(m_base_page+head, head);
    assert(fd->m_prev == 0);
    assert(fd->m_list == list);
    fd->m_prev = start;
  }
  
  m_buddy_lists[zone][list] = start;
}

Uint32 
Ndbd_mem_manager::remove_free_list(Uint32 zone, Uint32 start, Uint32 list)
{
  Free_page_data* fd = get_free_page_data(m_base_page+start, start);
  Uint32 size = fd->m_size;
  Uint32 next = fd->m_next;
  Uint32 prev = fd->m_prev;
  assert(fd->m_list == list);
  
  if (prev)
  {
    assert(m_buddy_lists[zone][list] != start);
    fd = get_free_page_data(m_base_page+prev, prev);
    assert(fd->m_next == start);
    assert(fd->m_list == list);
    fd->m_next = next;
  }
  else
  {
    assert(m_buddy_lists[zone][list] == start);
    m_buddy_lists[zone][list] = next;
  }
  
  if (next)
  {
    fd = get_free_page_data(m_base_page+next, next);
    assert(fd->m_list == list);
    assert(fd->m_prev == start);
    fd->m_prev = prev;
  }

  return size;
}

void
Ndbd_mem_manager::dump() const
{
  mt_mem_manager_lock();
  for (Uint32 zone = 0; zone < ZONE_COUNT; zone ++)
  {
    for (Uint32 i = 0; i<16; i++)
    {
      printf(" list: %d - ", i);
      Uint32 head = m_buddy_lists[zone][i];
      while(head)
      {
        Free_page_data* fd = get_free_page_data(m_base_page+head, head);
        printf("[ i: %d prev %d next %d list %d size %d ] ",
               head, fd->m_prev, fd->m_next, fd->m_list, fd->m_size);
        head = fd->m_next;
      }
      printf("EOL\n");
    }

    m_resource_limits.dump();
  }
  mt_mem_manager_unlock();
}

void
Ndbd_mem_manager::dump_on_alloc_fail(bool on)
{
  m_dump_on_alloc_fail = on;
}

void
Ndbd_mem_manager::lock()
{
  mt_mem_manager_lock();
}

void
Ndbd_mem_manager::unlock()
{
  mt_mem_manager_unlock();
}

void*
Ndbd_mem_manager::alloc_page(Uint32 type,
                             Uint32* i,
                             AllocZone zone,
                             bool locked)
{
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  if (!locked)
    mt_mem_manager_lock();

  Uint32 cnt = 1;
  const Uint32 min = 1;
  const Uint32 free_res = m_resource_limits.get_resource_free_reserved(idx);
  if (free_res < cnt)
  {
    const Uint32 free_shr = m_resource_limits.get_resource_free_shared(idx);
    const Uint32 free = m_resource_limits.get_resource_free(idx);
    if (free < min || (free_shr + free_res < min))
    {
      if (!locked)
        mt_mem_manager_unlock();
      return NULL;
    }
  }
  alloc(zone, i, &cnt, min);
  if (likely(cnt))
  {
    const Uint32 spare_taken = m_resource_limits.post_alloc_resource_pages(idx, cnt);
    if (spare_taken > 0)
    {
      require(spare_taken == cnt);
      release(*i, spare_taken);
      m_resource_limits.check();
      if (!locked)
        mt_mem_manager_unlock();
      *i = RNIL;
      return NULL;
    }
    m_resource_limits.check();
    if (!locked)
      mt_mem_manager_unlock();
#ifdef NDBD_RANDOM_START_PAGE
    *i += m_random_start_page_id;
    return m_base_page + *i - m_random_start_page_id;
#else
    return m_base_page + *i;
#endif
  }
  if (!locked)
    mt_mem_manager_unlock();
  return 0;
}

void*
Ndbd_mem_manager::alloc_spare_page(Uint32 type, Uint32* i, AllocZone zone)
{
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  mt_mem_manager_lock();

  Uint32 cnt = 1;
  const Uint32 min = 1;
  if (m_resource_limits.get_resource_spare(idx) >= min)
  {
    alloc(zone, i, &cnt, min);
    if (likely(cnt))
    {
      assert(cnt == min);
      m_resource_limits.post_alloc_resource_spare(idx, cnt);
      m_resource_limits.check();
      mt_mem_manager_unlock();
#ifdef NDBD_RANDOM_START_PAGE
      *i += m_random_start_page_id;
      return m_base_page + *i - m_random_start_page_id;
#else
      return m_base_page + *i;
#endif
    }
  }
  mt_mem_manager_unlock();
  return 0;
}

void
Ndbd_mem_manager::release_page(Uint32 type, Uint32 i, bool locked)
{
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  if (!locked)
    mt_mem_manager_lock();

#ifdef NDBD_RANDOM_START_PAGE
  i -= m_random_start_page_id;
#endif

  release(i, 1);
  m_resource_limits.post_release_resource_pages(idx, 1);

  m_resource_limits.check();
  if (!locked)
    mt_mem_manager_unlock();
}

void
Ndbd_mem_manager::alloc_pages(Uint32 type,
                              Uint32* i,
                              Uint32 *cnt,
                              Uint32 min,
                              AllocZone zone,
                              bool locked)
{
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  if (!locked)
    mt_mem_manager_lock();

  Uint32 req = *cnt;
  const Uint32 free_res = m_resource_limits.get_resource_free_reserved(idx);
  if (free_res < req)
  {
    const Uint32 free = m_resource_limits.get_resource_free(idx);
    if (free < req)
    {
      req = free;
    }
    const Uint32 free_shr = m_resource_limits.get_free_shared();
    if (free_shr + free_res < req)
    {
      req = free_shr + free_res;
    }
    if (req < min)
    {
      *cnt = 0;
      if (!locked)
        mt_mem_manager_unlock();
      return;
    }
  }

  // Hi order allocations can always use any zone
  alloc(zone, i, &req, min);
  const Uint32 spare_taken = m_resource_limits.post_alloc_resource_pages(idx, req);
  if (spare_taken > 0)
  {
    req -= spare_taken;
    release(*i + req, spare_taken);
  }
  if (0 < req && req < min)
  {
    release(*i, req);
    m_resource_limits.post_release_resource_pages(idx, req);
    req = 0;
  }
  * cnt = req;
  m_resource_limits.check();
  if (!locked)
    mt_mem_manager_unlock();
#ifdef NDBD_RANDOM_START_PAGE
  *i += m_random_start_page_id;
#endif
}

void
Ndbd_mem_manager::release_pages(Uint32 type, Uint32 i, Uint32 cnt, bool locked)
{
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  if (!locked)
    mt_mem_manager_lock();

#ifdef NDBD_RANDOM_START_PAGE
  i -= m_random_start_page_id;
#endif

  release(i, cnt);
  m_resource_limits.post_release_resource_pages(idx, cnt);
  m_resource_limits.check();
  if (!locked)
    mt_mem_manager_unlock();
}

#ifdef UNIT_TEST

#include <Vector.hpp>
#include <NdbHost.h>

struct Chunk {
  Uint32 pageId;
  Uint32 pageCount;
};

struct Timer
{
  Uint64 sum;
  Uint32 cnt;

  Timer() { sum = cnt = 0;}

  struct timeval st;

  void start() {
    gettimeofday(&st, 0);
  }

  Uint64 calc_diff() {
    struct timeval st2;
    gettimeofday(&st2, 0);
    Uint64 diff = st2.tv_sec;
    diff -= st.tv_sec;
    diff *= 1000000;
    diff += st2.tv_usec;
    diff -= st.tv_usec;
    return diff;
  }
  
  void stop() {
    add(calc_diff());
  }
  
  void add(Uint64 diff) { sum += diff; cnt++;}

  void print(const char * title) const {
    float ps = sum;
    ps /= cnt;
    printf("%s %fus/call %lld %d\n", title, ps, sum, cnt);
  }
};

int 
main(int argc, char** argv)
{
  int sz = 1*32768;
  int run_time = 30;
  if (argc > 1)
    sz = 32*atoi(argv[1]);

  if (argc > 2)
    run_time = atoi(argv[2]);

  char buf[255];
  Timer timer[4];
  printf("Startar modul test av Page Manager %dMb %ds\n", 
	 (sz >> 5), run_time);
  g_eventLogger->createConsoleHandler();
  g_eventLogger->setCategory("keso");
  g_eventLogger->enable(Logger::LL_ON, Logger::LL_INFO);
  g_eventLogger->enable(Logger::LL_ON, Logger::LL_CRITICAL);
  g_eventLogger->enable(Logger::LL_ON, Logger::LL_ERROR);
  g_eventLogger->enable(Logger::LL_ON, Logger::LL_WARNING);
  
#define DEBUG 0

  Ndbd_mem_manager mem;
  Resource_limit rl;
  rl.m_min = 0;
  rl.m_max = sz;
  rl.m_curr = 0;
  rl.m_spare = 0;
  rl.m_resource_id = 0;
  mem.set_resource_limit(rl);
  rl.m_min = sz < 16384 ? sz : 16384;
  rl.m_max = 0;
  rl.m_resource_id = 1;
  mem.set_resource_limit(rl);
  
  mem.init(NULL);
  mem.dump();
  printf("pid: %d press enter to continue\n", NdbHost_GetProcessId());
  fgets(buf, sizeof(buf), stdin);
  Vector<Chunk> chunks;
  time_t stop = time(0) + run_time;
  for(Uint32 i = 0; time(0) < stop; i++){
    //mem.dump();
    
    // Case
    Uint32 c = (rand() % 100);
    if (c < 50)
    {
      c = 0;
    } 
    else if (c < 93)
    {
      c = 1;
    }
    else
    {
      c = 2;
    }
    
    Uint32 alloc = 1 + rand() % 3200;
    
    if(chunks.size() == 0 && c == 0)
    {
      c = 1 + rand() % 2;
    }
    
    if(DEBUG)
      printf("loop=%d ", i);
    switch(c){ 
    case 0:{ // Release
      const int ch = rand() % chunks.size();
      Chunk chunk = chunks[ch];
      chunks.erase(ch);
      timer[0].start();
      mem.release(chunk.pageId, chunk.pageCount);
      timer[0].stop();
      if(DEBUG)
	printf(" release %d %d\n", chunk.pageId, chunk.pageCount);
    }
      break;
    case 2: { // Seize(n) - fail
      alloc += sz;
      // Fall through
    }
    case 1: { // Seize(n) (success)
      Chunk chunk;
      chunk.pageCount = alloc;
      if (DEBUG)
      {
	printf(" alloc %d -> ", alloc); fflush(stdout);
      }
      timer[0].start();
      mem.alloc(&chunk.pageId, &chunk.pageCount, 1);
      Uint64 diff = timer[0].calc_diff();

      if (DEBUG)
	printf("%d %d", chunk.pageId, chunk.pageCount);
      assert(chunk.pageCount <= alloc);
      if(chunk.pageCount != 0){
	chunks.push_back(chunk);
	if(chunk.pageCount != alloc) {
	  timer[2].add(diff);
	  if (DEBUG)
	    printf(" -  Tried to allocate %d - only allocated %d - free: %d",
		   alloc, chunk.pageCount, 0);
	}
	else
	{
	  timer[1].add(diff);
	}
      } else {
	timer[3].add(diff);
	if (DEBUG)
	  printf("  Failed to alloc %d pages with %d pages free",
		 alloc, 0);
      }
      if (DEBUG)
	printf("\n");
    }
      break;
    }
  }
  if (!DEBUG)
    while(chunks.size() > 0){
      Chunk chunk = chunks.back();
      mem.release(chunk.pageId, chunk.pageCount);      
      chunks.erase(chunks.size() - 1);
    }

  const char *title[] = {
    "release   ",
    "alloc full",
    "alloc part",
    "alloc fail"
  };
  for(Uint32 i = 0; i<4; i++)
    timer[i].print(title[i]);

  mem.dump();
}

template class Vector<Chunk>;

#endif

#define JAM_FILE_ID 296


template class Vector<InitChunk>;
