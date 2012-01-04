/*
   Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

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



#include "ndbd_malloc_impl.hpp"
#include <ndb_global.h>
#include <EventLogger.hpp>
#include <portlib/NdbMem.h>

#ifdef NDB_WIN
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

#ifdef VM_TRACE
#ifndef NDBD_RANDOM_START_PAGE
#define NDBD_RANDOM_START_PAGE
#endif
#endif

#ifdef NDBD_RANDOM_START_PAGE
static Uint32 g_random_start_page_id = 0;
#endif

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

#define ZONE_LO 0
#define ZONE_HI 1

/**
 * POOL_RECORD_BITS == 13 => 32 - 13 = 19 bits for page
 */
#define ZONE_LO_BOUND (1u << 19)

#include <NdbOut.hpp>

extern void ndbd_alloc_touch_mem(void * p, size_t sz, volatile Uint32 * watchCounter);

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
{
  m_base_page = 0;
  memset(m_buddy_lists, 0, sizeof(m_buddy_lists));
  memset(m_resource_limit, 0, sizeof(m_resource_limit));
  
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
  return (void*)(m_base_page - g_random_start_page_id);
#else
  return (void*)m_base_page;
#endif
}

/**
 *
 * resource 0 has following semantics:
 *
 * m_min  - remaining reserved for other resources
 * m_curr - sum(m_curr) for other resources (i.e total in use)
 * m_max  - totally allocated from OS
 *
 * resource N has following semantics:
 *
 * m_min = reserved
 * m_curr = currently used
 * m_max = max alloc, 0 = no limit
 *
 */
void
Ndbd_mem_manager::set_resource_limit(const Resource_limit& rl)
{
  Uint32 id = rl.m_resource_id;
  assert(id < XX_RL_COUNT);
  
  Uint32 reserve = id ? rl.m_min : 0;
  mt_mem_manager_lock();
  Uint32 current_reserved = m_resource_limit[0].m_min;
  
  m_resource_limit[id] = rl;
  m_resource_limit[id].m_curr = 0;
  m_resource_limit[0].m_min = current_reserved + reserve;
  mt_mem_manager_unlock();
}

bool
Ndbd_mem_manager::get_resource_limit(Uint32 id, Resource_limit& rl) const
{
  if (id < XX_RL_COUNT)
  {
    mt_mem_manager_lock();
    rl = m_resource_limit[id];
    mt_mem_manager_unlock();
    return true;
  }
  return false;
}

bool
Ndbd_mem_manager::get_resource_limit_nolock(Uint32 id, Resource_limit& rl) const
{
  if (id < XX_RL_COUNT)
  {
    rl = m_resource_limit[id];
    return true;
  }
  return false;
}

static
inline
void
check_resource_limits(Resource_limit* rl)
{
#ifdef VM_TRACE
  Uint32 curr = 0;
  Uint32 res_alloc = 0;
  Uint32 shared_alloc = 0;
  Uint32 sumres = 0;
  for (Uint32 i = 1; i<XX_RL_COUNT; i++)
  {
    curr += rl[i].m_curr;
    sumres += rl[i].m_min;
    assert(rl[i].m_max == 0 || rl[i].m_curr <= rl[i].m_max);
    if (rl[i].m_curr > rl[i].m_min)
    {
      shared_alloc += rl[i].m_curr - rl[i].m_min;
      res_alloc += rl[i].m_min;
    }
    else
    {
      res_alloc += rl[i].m_curr;
    }
  }
  assert(curr == rl[0].m_curr);
  assert(res_alloc + shared_alloc == curr);
  assert(res_alloc <= sumres);
  assert(sumres == res_alloc + rl[0].m_min);
  assert(rl[0].m_curr <= rl[0].m_max);
#endif
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
Ndbd_mem_manager::init(Uint32 *watchCounter, bool alloc_less_memory)
{
  assert(m_base_page == 0);

  if (watchCounter)
    *watchCounter = 9;

  Uint32 pages = 0;
  Uint32 max_page = 0;
  Uint32 reserved = m_resource_limit[0].m_min;
  if (m_resource_limit[0].m_max)
  {
    pages = m_resource_limit[0].m_max;
  } 
  else
  {
    pages = m_resource_limit[0].m_min; // reserved
  }
  
  if (m_resource_limit[0].m_min == 0)
  {
    m_resource_limit[0].m_min = pages;
  }
  
  const Uint64 pg = Uint64(sizeof(Alloc_page));
  g_eventLogger->info("Ndbd_mem_manager::init(%d) min: %lluMb initial: %lluMb",
                      alloc_less_memory,
                      (pg*m_resource_limit[0].m_min)>>20,
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

#ifdef NDBD_RANDOM_START_PAGE
  /**
   * In order to find bad-users of page-id's
   *   we add a random offset to the page-id's returned
   *   however, due to ZONE_LO that offset can't be that big
   *   (since we at get_page don't know if it's a HI/LO page)
   */
  Uint32 max_rand_start = ZONE_LO_BOUND - 1;
  if (max_rand_start > pages)
  {
    max_rand_start -= pages;
    if (max_rand_start > 0x10000)
      g_random_start_page_id = 0x10000 + (rand() % (max_rand_start - 0x10000));
    else if (max_rand_start)
      g_random_start_page_id = rand() % max_rand_start;

    assert(Uint64(pages) + Uint64(g_random_start_page_id) <= 0xFFFFFFFF);

    ndbout_c("using g_random_start_page_id: %u (%.8x)",
             g_random_start_page_id, g_random_start_page_id);
  }
#endif

  /**
   * Do malloc
   */
  Uint32 allocated = 0;
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
  
  if (allocated < m_resource_limit[0].m_min)
  {
    g_eventLogger->
      error("Unable to alloc min memory from OS: min: %lldMb "
            " allocated: %lldMb",
            (Uint64)(sizeof(Alloc_page)*m_resource_limit[0].m_min) >> 20,
            (Uint64)(sizeof(Alloc_page)*allocated) >> 20);
    return false;
  }
  else if (allocated < pages)
  {
    g_eventLogger->
      warning("Unable to alloc requested memory from OS: min: %lldMb"
              " requested: %lldMb allocated: %lldMb",
              (Uint64)(sizeof(Alloc_page)*m_resource_limit[0].m_min)>>20,
              (Uint64)(sizeof(Alloc_page)*m_resource_limit[0].m_max)>>20,
              (Uint64)(sizeof(Alloc_page)*allocated)>>20);
    if (!alloc_less_memory)
      return false;
  }

  /**
   * Sort chunks...
   */
  qsort(m_unmapped_chunks.getBase(), m_unmapped_chunks.size(),
        sizeof(InitChunk), cmp_chunk);

  m_base_page = m_unmapped_chunks[0].m_ptr;
  
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

  m_resource_limit[0].m_resource_id = max_page;
  m_resource_limit[0].m_min = reserved;
  m_resource_limit[0].m_max = 0;

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
      limit += m_resource_limit[resources[i]].m_min;
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

    ndbd_alloc_touch_mem(chunk->m_ptr,
                         chunk->m_cnt * sizeof(Alloc_page),
                         watchCounter);

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
    }

    grow(chunk->m_start, chunk->m_cnt);
    sofar += chunk->m_cnt;

    m_unmapped_chunks.erase(idx);
  }
  
  mt_mem_manager_lock();
  check_resource_limits(m_resource_limit);
  mt_mem_manager_unlock();

  if (resources == 0 && memlock)
  {
    NdbMem_MemLockAll(1);
  }
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
  
  if ((start + cnt) == ((start_bmp + 1) << BPP_2LOG))
  {
    cnt--; // last page is always marked as empty
  }
  
  for (Uint32 i = 0; i<m_used_bitmap_pages.size(); i++)
    if (m_used_bitmap_pages[i] == start_bmp)
      goto found;

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
  
  {
    Alloc_page* bmp = m_base_page + start;
    memset(bmp, 0, sizeof(Alloc_page));
    cnt--;
    start++;
  }
  m_used_bitmap_pages.push_back(start_bmp);
  
found:
  if (cnt)
  {
    mt_mem_manager_lock();
    m_resource_limit[0].m_curr += cnt;
    m_resource_limit[0].m_max += cnt;
    if (start >= ZONE_LO_BOUND)
    {
      Uint64 mbytes = ((Uint64(cnt) * 32) + 1023) / 1024;
      ndbout_c("Adding %uMb to ZONE_HI (%u,%u)", (Uint32)mbytes, start, cnt);
      release(start, cnt);
    }
    else if (start + cnt <= ZONE_LO_BOUND)
    {
      Uint64 mbytes = ((Uint64(cnt)*32) + 1023) / 1024;
      ndbout_c("Adding %uMb to ZONE_LO (%u,%u)", (Uint32)mbytes, start, cnt);
      release(start, cnt);      
    }
    else
    {
      Uint32 cnt0 = ZONE_LO_BOUND - start;
      Uint32 cnt1 = start + cnt - ZONE_LO_BOUND;
      Uint64 mbytes0 = ((Uint64(cnt0)*32) + 1023) / 1024;
      Uint64 mbytes1 = ((Uint64(cnt1)*32) + 1023) / 1024;
      ndbout_c("Adding %uMb to ZONE_LO (split %u,%u)", (Uint32)mbytes0,
               start, cnt0);
      ndbout_c("Adding %uMb to ZONE_HI (split %u,%u)", (Uint32)mbytes1,
               ZONE_LO_BOUND, cnt1);
      release(start, cnt0);
      release(ZONE_LO_BOUND, cnt1);
    }
    mt_mem_manager_unlock();
  }
}

void
Ndbd_mem_manager::release(Uint32 start, Uint32 cnt)
{
  assert(m_resource_limit[0].m_curr >= cnt);
  assert(start);
  m_resource_limit[0].m_curr -= cnt;
  
  set(start, start+cnt-1);
  
  Uint32 zone = start < ZONE_LO_BOUND ? 0 : 1;
  release_impl(zone, start, cnt);
}

void
Ndbd_mem_manager::release_impl(Uint32 zone, Uint32 start, Uint32 cnt)
{
  assert(start);  

  Uint32 test = check(start-1, start+cnt);
  if (start != ZONE_LO_BOUND && test & 1)
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
  if (right != ZONE_LO_BOUND && test & 2)
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
                        Uint32* ret, Uint32 *pages, Uint32 min)
{
  if (zone == NDB_ZONE_ANY)
  {
    Uint32 save = * pages;
    alloc_impl(ZONE_HI, ret, pages, min);
    if (*pages)
      return;
    * pages = save;
  }

  alloc_impl(ZONE_LO, ret, pages, min);
}

void
Ndbd_mem_manager::alloc_impl(Uint32 zone, 
                             Uint32* ret, Uint32 *pages, Uint32 min)
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
      assert(m_resource_limit[0].m_curr + cnt <= m_resource_limit[0].m_max);
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
      assert(m_resource_limit[0].m_curr + sz <= m_resource_limit[0].m_max);
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
  for (Uint32 zone = 0; zone < 2; zone ++)
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
    
    for (Uint32 i = 0; i<XX_RL_COUNT; i++)
    {
      printf("ri: %d min: %d curr: %d max: %d\n",
             i, 
             m_resource_limit[i].m_min,
             m_resource_limit[i].m_curr,
             m_resource_limit[i].m_max);
    }
  }
  mt_mem_manager_unlock();
}

void*
Ndbd_mem_manager::alloc_page(Uint32 type, Uint32* i, AllocZone zone)
{
  Uint32 idx = type & RG_MASK;
  assert(idx && idx < XX_RL_COUNT);
  mt_mem_manager_lock();
  Resource_limit tot = m_resource_limit[0];
  Resource_limit rl = m_resource_limit[idx];

  Uint32 cnt = 1;
  Uint32 res0 = (rl.m_curr < rl.m_min) ? 1 : 0;
  Uint32 limit = (rl.m_max == 0 || rl.m_curr < rl.m_max) ? 0 : 1; // Over limit
  Uint32 free = (tot.m_min + tot.m_curr < tot.m_max) ? 1 : 0; // Has free
  
  assert(tot.m_min >= res0);

  if (likely(res0 == 1 || (limit == 0 && free == 1)))
  {
    alloc(zone, i, &cnt, 1);
    if (likely(cnt))
    {
      m_resource_limit[0].m_curr = tot.m_curr + cnt;
      m_resource_limit[0].m_min = tot.m_min - res0;
      m_resource_limit[idx].m_curr = rl.m_curr + cnt;

      check_resource_limits(m_resource_limit);
      mt_mem_manager_unlock();
#ifdef NDBD_RANDOM_START_PAGE
      *i += g_random_start_page_id;
      return m_base_page + *i - g_random_start_page_id;
#else
      return m_base_page + *i;
#endif
    }
  }
  mt_mem_manager_unlock();
  return 0;
}

void
Ndbd_mem_manager::release_page(Uint32 type, Uint32 i)
{
  Uint32 idx = type & RG_MASK;
  assert(idx && idx < XX_RL_COUNT);
  mt_mem_manager_lock();
  Resource_limit tot = m_resource_limit[0];
  Resource_limit rl = m_resource_limit[idx];

#ifdef NDBD_RANDOM_START_PAGE
  i -= g_random_start_page_id;
#endif

  Uint32 sub = (rl.m_curr <= rl.m_min) ? 1 : 0; // Over min ?
  release(i, 1);
  m_resource_limit[0].m_curr = tot.m_curr - 1;
  m_resource_limit[0].m_min = tot.m_min + sub;
  m_resource_limit[idx].m_curr = rl.m_curr - 1;

  check_resource_limits(m_resource_limit);
  mt_mem_manager_unlock();
}

void
Ndbd_mem_manager::alloc_pages(Uint32 type, Uint32* i, Uint32 *cnt, Uint32 min)
{
  Uint32 idx = type & RG_MASK;
  assert(idx && idx < XX_RL_COUNT);
  mt_mem_manager_lock();
  Resource_limit tot = m_resource_limit[0];
  Resource_limit rl = m_resource_limit[idx];

  Uint32 req = *cnt;

  Uint32 max = rl.m_max - rl.m_curr;
  Uint32 res0 = rl.m_min - rl.m_curr;
  Uint32 free_shared = tot.m_max - (tot.m_min + tot.m_curr);

  Uint32 res1;
  if (rl.m_curr + req <= rl.m_min)
  {
    // all is reserved...
    res0 = req;
    res1 = 0;
  }
  else
  {
    req = rl.m_max ? max : req;
    res0 = (rl.m_curr > rl.m_min) ? 0 : res0;
    res1 = req - res0;
    
    if (unlikely(res1 > free_shared))
    {
      res1 = free_shared;
      req = res0 + res1;
    }
  }

  // req = pages to alloc
  // res0 = portion that is reserved
  // res1 = part that is over reserver
  assert (res0 + res1 == req);
  assert (tot.m_min >= res0);
  
  if (likely(req))
  {
    // Hi order allocations can always use any zone
    alloc(NDB_ZONE_ANY, i, &req, min); 
    * cnt = req;
    if (unlikely(req < res0)) // Got min than what was reserved :-(
    {
      res0 = req;
    }
    assert(tot.m_min >= res0);
    assert(tot.m_curr + req <= tot.m_max);
    
    m_resource_limit[0].m_curr = tot.m_curr + req;
    m_resource_limit[0].m_min = tot.m_min - res0;
    m_resource_limit[idx].m_curr = rl.m_curr + req;
    check_resource_limits(m_resource_limit);
    mt_mem_manager_unlock();
#ifdef NDBD_RANDOM_START_PAGE
    *i += g_random_start_page_id;
#endif
    return ;
  }
  mt_mem_manager_unlock();
  * cnt = req;
#ifdef NDBD_RANDOM_START_PAGE
  *i += g_random_start_page_id;
#endif
  return;
}

void
Ndbd_mem_manager::release_pages(Uint32 type, Uint32 i, Uint32 cnt)
{
  Uint32 idx = type & RG_MASK;
  assert(idx && idx < XX_RL_COUNT);
  mt_mem_manager_lock();
  Resource_limit tot = m_resource_limit[0];
  Resource_limit rl = m_resource_limit[idx];

#ifdef NDBD_RANDOM_START_PAGE
  i -= g_random_start_page_id;
#endif

  release(i, cnt);

  Uint32 currnew = rl.m_curr - cnt;
  if (rl.m_curr > rl.m_min)
  {
    if (currnew < rl.m_min)
    {
      m_resource_limit[0].m_min = tot.m_min + (rl.m_min - currnew);
    }
  }
  else
  {
    m_resource_limit[0].m_min = tot.m_min + cnt;
  }
  m_resource_limit[0].m_curr = tot.m_curr - cnt;
  m_resource_limit[idx].m_curr = currnew;
  check_resource_limits(m_resource_limit);
  mt_mem_manager_unlock();
}

#ifdef UNIT_TEST

#include <Vector.hpp>
#include <NdbTick.h>

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
  rl.m_resource_id = 0;
  mem.set_resource_limit(rl);
  rl.m_min = sz < 16384 ? sz : 16384;
  rl.m_max = 0;
  rl.m_resource_id = 1;
  mem.set_resource_limit(rl);
  
  mem.init(NULL);
  mem.dump();
  printf("pid: %d press enter to continue\n", getpid());
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
      Uint64 start = NdbTick_CurrentMillisecond();      
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

template class Vector<InitChunk>;
