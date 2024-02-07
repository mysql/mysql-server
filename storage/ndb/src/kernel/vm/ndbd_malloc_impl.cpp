/*
   Copyright (c) 2006, 2024, Oracle and/or its affiliates.

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

#include "ndbd_malloc_impl.hpp"
#include "my_config.h"
#include "ndbd_malloc.hpp"
#include "util/require.h"

#include <time.h>

#include <ndb_global.h>
#include <portlib/NdbMem.h>

#define JAM_FILE_ID 296

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
#define DEBUG_MEM_ALLOC 1
#endif

#ifdef DEBUG_MEM_ALLOC
#define DEB_MEM_ALLOC(arglist)   \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_MEM_ALLOC(arglist) \
  do {                         \
  } while (0)
#endif

#define PAGES_PER_REGION_LOG BPP_2LOG
#define ALLOC_PAGES_PER_REGION ((1 << PAGES_PER_REGION_LOG) - 2)

#ifdef _WIN32
void *sbrk(int increment) { return (void *)-1; }
#endif

static int f_method_idx = 0;
#ifdef NDBD_MALLOC_METHOD_SBRK
static const char *f_method = "SMsm";
#else
static const char *f_method = "MSms";
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

constexpr Uint32 Ndbd_mem_manager::zone_bound[ZONE_COUNT] =
    {/* bound in regions */
     ZONE_19_BOUND >> PAGES_PER_REGION_LOG,
     ZONE_27_BOUND >> PAGES_PER_REGION_LOG,
     ZONE_30_BOUND >> PAGES_PER_REGION_LOG,
     ZONE_32_BOUND >> PAGES_PER_REGION_LOG};

/*
 * Linux on ARM64 uses 64K as default memory page size.
 * Most others still use 4K or 8K.
 */
static constexpr size_t MAX_SYSTEM_PAGE_SIZE = 65536;
static constexpr size_t ALLOC_PAGES_PER_SYSTEM_PAGE =
    MAX_SYSTEM_PAGE_SIZE / sizeof(Alloc_page);

/**
 * do_virtual_alloc uses debug functions NdbMem_ReserveSpace and
 * NdbMem_PopulateSpace to be able to use as high page numbers as possible for
 * each memory region.  Using high page numbers will likely lure bugs due to
 * storing not all required bits of page numbers.
 */

/**
   Disable on Solaris:
   Bug #32575486 NDBMTD CONSUMES ALL AVAILABLE MEMORY IN DEBUG ON SOLARIS
*/
#if defined(VM_TRACE) && !defined(__sun)
#if defined(_WIN32) || (defined(MADV_DONTDUMP) && defined(MAP_NORESERVE)) || \
    defined(MAP_GUARD)
/*
 * Only activate use of do_virtual_alloc() if build platform allows reserving
 * address space only without reserving space on swap nor include memory in
 * core files dumped, since we start by trying to reserve 128TB of address
 * space.
 *
 * For Windows one uses VirtualAlloc(MEM_RESERVE).
 *
 * On Linux and Solaris (since 11.4 SRU 12) one uses mmap(MAP_NORESERVE) and
 * madvise(MADV_DONTDUMP).
 *
 * On FreeBSD one uses mmap(MAP_GUARD).
 *
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

static inline int log_and_fake_success(const char func[], int line,
                                       const char msg[], void *p, size_t s) {
  g_eventLogger->info("DEBUG: %s: %u: %s: p %p: len %zu", func, line, msg, p,
                      s);
  return 0;
}

#define NdbMem_ReserveSpace(x, y) \
  log_and_fake_success(__func__, __LINE__, "NdbMem_ReserveSpace", (x), (y))

#define NdbMem_PopulateSpace(x, y) \
  log_and_fake_success(__func__, __LINE__, "NdbMem_PopulateSpace", (x), (y))

#endif

bool Ndbd_mem_manager::do_virtual_alloc(Uint32 pages,
                                        InitChunk chunks[ZONE_COUNT],
                                        Uint32 *watchCounter,
                                        Alloc_page **base_address) {
  require(pages % ALLOC_PAGES_PER_SYSTEM_PAGE == 0);
  require(pages > 0);
  if (watchCounter) *watchCounter = 9;
  constexpr Uint32 max_regions = zone_bound[ZONE_COUNT - 1];
  constexpr Uint32 max_pages = max_regions << PAGES_PER_REGION_LOG;
  static_assert(max_regions == (max_pages >> PAGES_PER_REGION_LOG));
  static_assert(max_regions > 0);
  if (pages > max_pages) {
    return false;
  }
  const bool half_space = (pages <= (max_pages >> 1));

  /* Find out page count per zone */
  Uint32 page_count[ZONE_COUNT];
  Uint32 region_count[ZONE_COUNT];
  Uint32 prev_bound = 0;
  for (int i = 0; i < ZONE_COUNT; i++) {
    Uint32 n = pages / (ZONE_COUNT - i);
    if (half_space && n > (zone_bound[i] << (PAGES_PER_REGION_LOG - 1))) {
      n = zone_bound[i] << (PAGES_PER_REGION_LOG - 1);
    } else if (n > ((zone_bound[i] - prev_bound) << PAGES_PER_REGION_LOG)) {
      n = (zone_bound[i] - prev_bound) << PAGES_PER_REGION_LOG;
    }
    if (n % ALLOC_PAGES_PER_SYSTEM_PAGE != 0) {
      // Always assign whole system pages
      n -= n % ALLOC_PAGES_PER_SYSTEM_PAGE;
    }
    // Always have some pages in lowest zone
    if (n == 0 && i == 0) n = ALLOC_PAGES_PER_SYSTEM_PAGE;
    page_count[i] = n;
    region_count[i] = (n + 256 * 1024 - 1) / (256 * 1024);
    prev_bound = zone_bound[i];
    pages -= n;
  }
  require(pages == 0);

  /* Reserve big enough continuous address space */
  static_assert(ZONE_COUNT >= 2);
  const Uint32 highest_low = zone_bound[0] - region_count[0];
  const Uint32 lowest_high =
      zone_bound[ZONE_COUNT - 2] + region_count[ZONE_COUNT - 1];
  const Uint32 least_region_count = lowest_high - highest_low;
  Uint32 space_regions = max_regions;
  Alloc_page *space = nullptr;
  int rc = -1;
  while (space_regions >= least_region_count) {
    if (watchCounter) *watchCounter = 9;
    rc = NdbMem_ReserveSpace(
        (void **)&space,
        (space_regions << PAGES_PER_REGION_LOG) * Uint64(32768));
    if (watchCounter) *watchCounter = 9;
    if (rc == 0) {
      g_eventLogger->info(
          "%s: Reserved address space for %u 8GiB regions at %p.", __func__,
          space_regions, space);
      break;
    }
    space_regions = (space_regions - 1 + least_region_count) / 2;
  }
  if (rc == -1) {
    g_eventLogger->info(
        "%s: Failed reserved address space for at least %u 8GiB regions.",
        __func__, least_region_count);
    return false;
  }

#ifdef NDBD_RANDOM_START_PAGE
  Uint32 range = highest_low;
  for (int i = 0; i < ZONE_COUNT; i++) {
    Uint32 rmax = (zone_bound[i] << PAGES_PER_REGION_LOG) - page_count[i];
    if (i > 0) {
      rmax -= zone_bound[i - 1] << PAGES_PER_REGION_LOG;
    }
    if (half_space) {
      rmax -= 1 << 17; /* lower half of region */
    }
    if (range > rmax) {
      rmax = range;
    }
  }
  m_random_start_page_id = rand() % range;
#endif

  Uint32 first_region[ZONE_COUNT];
  for (int i = 0; i < ZONE_COUNT; i++) {
    first_region[i] = (i < ZONE_COUNT - 1)
                          ? zone_bound[i]
                          : MIN(first_region[0] + space_regions, max_regions);
    first_region[i] -= ((page_count[i] +
#ifdef NDBD_RANDOM_START_PAGE
                         m_random_start_page_id +
#endif
                         ((1 << PAGES_PER_REGION_LOG) - 1)) >>
                        PAGES_PER_REGION_LOG);

    chunks[i].m_cnt = page_count[i];
    chunks[i].m_ptr =
        space + ((first_region[i] - first_region[0]) << PAGES_PER_REGION_LOG);
#ifndef NDBD_RANDOM_START_PAGE
    const Uint32 first_page = first_region[i] << PAGES_PER_REGION_LOG;
#else
    const Uint32 first_page =
        (first_region[i] << PAGES_PER_REGION_LOG) + m_random_start_page_id;
#endif
    const Uint32 last_page = first_page + chunks[i].m_cnt - 1;
    g_eventLogger->info("%s: Populated space with pages %u to %u at %p.",
                        __func__, first_page, last_page, chunks[i].m_ptr);
    require(last_page < (zone_bound[i] << PAGES_PER_REGION_LOG));
  }
  *base_address = space - first_region[0] * 8 * Uint64(32768);
  if (watchCounter) *watchCounter = 9;
#ifdef NDB_TEST_128TB_VIRTUAL_MEMORY
  exit(0);  // No memory mapped only faking no meaning to continue.
#endif
  return true;
}
#endif

static bool do_malloc(Uint32 pages, InitChunk *chunk, Uint32 *watchCounter,
                      void *baseaddress) {
  void *ptr = 0;
  Uint32 sz = pages;

retry:
  if (watchCounter) *watchCounter = 9;

  char method = f_method[f_method_idx];
  switch (method) {
    case 0:
      return false;
    case 'S':
    case 's': {
      ptr = 0;
      while (ptr == 0) {
        if (watchCounter) *watchCounter = 9;

        ptr = sbrk(sizeof(Alloc_page) * sz);

        if (ptr == (void *)-1) {
          if (method == 'S') {
            f_method_idx++;
            goto retry;
          }

          ptr = 0;
          sz = 1 + (9 * sz) / 10;
          if (pages >= 32 && sz < 32) {
            sz = pages;
            f_method_idx++;
            goto retry;
          }
        } else if (UintPtr(ptr) < UintPtr(baseaddress)) {
          /**
           * Unusable memory :(
           */
          g_eventLogger->info(
              "sbrk(%lluMb) => %p which is less than baseaddress!!",
              Uint64((sizeof(Alloc_page) * sz) >> 20), ptr);
          f_method_idx++;
          goto retry;
        }
      }
      break;
    }
    case 'M':
    case 'm': {
      ptr = 0;
      while (ptr == 0) {
        if (watchCounter) *watchCounter = 9;

        ptr = NdbMem_AlignedAlloc(
            ALLOC_PAGES_PER_SYSTEM_PAGE * sizeof(Alloc_page),
            sizeof(Alloc_page) * sz);
        if (UintPtr(ptr) < UintPtr(baseaddress)) {
          g_eventLogger->info(
              "malloc(%lluMb) => %p which is less than baseaddress!!",
              Uint64((sizeof(Alloc_page) * sz) >> 20), ptr);
          free(ptr);
          ptr = 0;
        }

        if (ptr == 0) {
          if (method == 'M') {
            f_method_idx++;
            goto retry;
          }

          sz = 1 + (9 * sz) / 10;
          if (pages >= 32 && sz < 32) {
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
  chunk->m_ptr = (Alloc_page *)ptr;
  const UintPtr align = sizeof(Alloc_page) - 1;
  /*
   * Ensure aligned to 32KB boundary.
   * Unsure why that is needed.
   * NdbMem_PopulateSpace() in ndbd_alloc_touch_mem() need system page
   * alignment, typically 4KB or 8KB.
   */
  if (UintPtr(ptr) & align) {
    chunk->m_cnt--;
    chunk->m_ptr = (Alloc_page *)((UintPtr(ptr) + align) & ~align);
  }

#ifdef UNIT_TEST
  g_eventLogger->info("do_malloc(%d) -> %p %d", pages, ptr, chunk->m_cnt);
  if (1) {
    Uint32 sum = 0;
    Alloc_page *page = chunk->m_ptr;
    for (Uint32 i = 0; i < chunk->m_cnt; i++, page++) {
      page->m_data[0 * 1024] = 0;
      page->m_data[1 * 1024] = 0;
      page->m_data[2 * 1024] = 0;
      page->m_data[3 * 1024] = 0;
      page->m_data[4 * 1024] = 0;
      page->m_data[5 * 1024] = 0;
      page->m_data[6 * 1024] = 0;
      page->m_data[7 * 1024] = 0;
    }
  }
#endif

  return true;
}

/**
 * Resource_limits
 */

Resource_limits::Resource_limits() {
  m_allocated = 0;
  m_free_reserved = 0;
  m_in_use = 0;
  m_spare = 0;
  m_untaken = 0;
  m_max_page = 0;
  // By default allow no low prio usage of shared
  m_prio_free_limit = UINT32_MAX;
  m_lent = 0;
  m_borrowed = 0;
  memset(m_limit, 0, sizeof(m_limit));
}

#ifndef VM_TRACE
inline
#endif
    void
    Resource_limits::check() const {
#ifdef VM_TRACE
  const Resource_limit *rl = m_limit;
  Uint32 curr = 0;
  Uint32 spare = 0;
  Uint32 lent = 0;
  Uint32 borrowed = 0;
  Uint32 sumres_lent = 0;
  Uint32 sumres_alloc = 0;  // includes spare and lent pages
  Uint32 shared_alloc = 0;
  Uint32 sumres = 0;
  for (Uint32 i = 0; i < MM_RG_COUNT; i++) {
    curr += rl[i].m_curr;
    spare += rl[i].m_spare;
    lent += rl[i].m_lent;
    borrowed += rl[i].m_borrowed;
    sumres_lent += rl[i].m_lent;
    sumres += rl[i].m_min;
    const Uint32 res_alloc = rl[i].m_curr + rl[i].m_spare + rl[i].m_lent;
    require(res_alloc <= rl[i].m_max);
    if (res_alloc > rl[i].m_min) {
      shared_alloc += res_alloc - rl[i].m_min;
      sumres_alloc += rl[i].m_min;
    } else {
      sumres_alloc += res_alloc;
    }
  }

  if (!((curr + m_untaken == get_in_use()) && (spare == get_spare()) &&
        (sumres_alloc + shared_alloc == curr + spare + sumres_lent) &&
        (sumres == sumres_alloc + get_free_reserved()) &&
        (get_in_use() + get_spare() <= get_allocated()) && (lent == m_lent) &&
        (borrowed == m_borrowed))) {
    dump();
  }

  require(curr + m_untaken == get_in_use());
  require(spare == get_spare());
  require(sumres_alloc + shared_alloc == curr + spare + sumres_lent);
  require(sumres == sumres_alloc + get_free_reserved());
  require(get_in_use() + get_spare() <= get_allocated());
  require(lent == m_lent);
  require(borrowed == m_borrowed);
#endif
}

void Resource_limits::dump() const {
  g_eventLogger->info(
      "ri: global "
      "max_page: %u free_reserved: %u in_use: %u allocated: %u spare: %u: "
      "untaken: %u: lent: %u: borrowed: %u",
      m_max_page, m_free_reserved, m_in_use, m_allocated, m_spare, m_untaken,
      m_lent, m_borrowed);
  for (Uint32 i = 0; i < MM_RG_COUNT; i++) {
    if (m_limit[i].m_resource_id == 0 && m_limit[i].m_min == 0 &&
        m_limit[i].m_curr == 0 && m_limit[i].m_max == 0 &&
        m_limit[i].m_lent == 0 && m_limit[i].m_borrowed == 0 &&
        m_limit[i].m_spare == 0 && m_limit[i].m_spare_pct == 0) {
      continue;
    }
    g_eventLogger->info(
        "ri: %u id: %u min: %u curr: %u max: %u lent: %u"
        " borrowed: %u spare: %u spare_pct: %u",
        i, m_limit[i].m_resource_id, m_limit[i].m_min, m_limit[i].m_curr,
        m_limit[i].m_max, m_limit[i].m_lent, m_limit[i].m_borrowed,
        m_limit[i].m_spare, m_limit[i].m_spare_pct);
  }
}

/**
 *
 * resource N has following semantics:
 *
 * m_min = reserved
 * m_curr = currently used
 * m_max = max alloc
 *
 */
void Resource_limits::init_resource_limit(Uint32 id, Uint32 min, Uint32 max) {
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

void Resource_limits::init_resource_spare(Uint32 id, Uint32 pct) {
  require(m_limit[id - 1].m_spare_pct == 0);
  m_limit[id - 1].m_spare_pct = pct;

  (void)alloc_resource_spare(id, 0);
}

/**
 * Ndbd_mem_manager
 */

int Ndbd_mem_manager::PageInterval::compare(const void *px, const void *py) {
  const PageInterval *x = static_cast<const PageInterval *>(px);
  const PageInterval *y = static_cast<const PageInterval *>(py);

  if (x->start < y->start) {
    return -1;
  }
  if (x->start > y->start) {
    return +1;
  }
  if (x->end < y->end) {
    return -1;
  }
  if (x->end > y->end) {
    return +1;
  }
  return 0;
}

Uint32 Ndbd_mem_manager::ndb_log2(Uint32 input) {
  if (input > 65535) return 16;
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
      m_mapped_pages_new_count(0) {
  size_t system_page_size = NdbMem_GetSystemPageSize();
  if (system_page_size > MAX_SYSTEM_PAGE_SIZE) {
    g_eventLogger->error(
        "Default system page size, %zu, is bigger than supported %zu\n",
        system_page_size, MAX_SYSTEM_PAGE_SIZE);
    abort();
  }
  memset(m_buddy_lists, 0, sizeof(m_buddy_lists));

  if (sizeof(Free_page_data) != (4 * (1 << FPD_2LOG))) {
    g_eventLogger->error("Invalid build, ndbd_malloc_impl.cpp:%d", __LINE__);
    abort();
  }
  mt_mem_manager_init();
}

void *Ndbd_mem_manager::get_memroot() const {
#ifdef NDBD_RANDOM_START_PAGE
  return (void *)(m_base_page - m_random_start_page_id);
#else
  return (void *)m_base_page;
#endif
}

/**
 *
 * resource N has following semantics:
 *
 * m_min = reserved
 * m_curr = currently used including spare pages
 * m_max = max alloc
 * m_spare = pages reserved for restart or special use
 *
 */
void Ndbd_mem_manager::set_resource_limit(const Resource_limit &rl) {
  require(rl.m_resource_id > 0);
  mt_mem_manager_lock();
  m_resource_limits.init_resource_limit(rl.m_resource_id, rl.m_min, rl.m_max);
  mt_mem_manager_unlock();
}

bool Ndbd_mem_manager::get_resource_limit(Uint32 id, Resource_limit &rl) const {
  /**
   * DUMP DumpPageMemory(1000) is agnostic about what resource groups exists.
   * Allowing use of any id.
   */
  if (1 <= id && id <= MM_RG_COUNT) {
    mt_mem_manager_lock();
    m_resource_limits.get_resource_limit(id, rl);
    mt_mem_manager_unlock();
    return true;
  }
  return false;
}

bool Ndbd_mem_manager::get_resource_limit_nolock(Uint32 id,
                                                 Resource_limit &rl) const {
  assert(id > 0);
  if (id <= MM_RG_COUNT) {
    m_resource_limits.get_resource_limit(id, rl);
    return true;
  }
  return false;
}

Uint32 Ndbd_mem_manager::get_allocated() const {
  mt_mem_manager_lock();
  const Uint32 val = m_resource_limits.get_allocated();
  mt_mem_manager_unlock();
  return val;
}

Uint32 Ndbd_mem_manager::get_reserved() const {
  mt_mem_manager_lock();
  const Uint32 val = m_resource_limits.get_reserved();
  mt_mem_manager_unlock();
  return val;
}

Uint32 Ndbd_mem_manager::get_shared() const {
  mt_mem_manager_lock();
  const Uint32 val = m_resource_limits.get_shared();
  mt_mem_manager_unlock();
  return val;
}

Uint32 Ndbd_mem_manager::get_free_shared() const {
  mt_mem_manager_lock();
  const Uint32 val = m_resource_limits.get_free_shared();
  mt_mem_manager_unlock();
  return val;
}

Uint32 Ndbd_mem_manager::get_free_shared_nolock() const {
  /* Used by mt_getSendBufferLevel for quick read. */
  const Uint32 val = m_resource_limits.get_free_shared();  // racy
  return val;
}

Uint32 Ndbd_mem_manager::get_spare() const {
  mt_mem_manager_lock();
  const Uint32 val = m_resource_limits.get_spare();
  mt_mem_manager_unlock();
  return val;
}

Uint32 Ndbd_mem_manager::get_in_use() const {
  mt_mem_manager_lock();
  const Uint32 val = m_resource_limits.get_in_use();
  mt_mem_manager_unlock();
  return val;
}

Uint32 Ndbd_mem_manager::get_reserved_in_use() const {
  mt_mem_manager_lock();
  const Uint32 val = m_resource_limits.get_reserved_in_use();
  mt_mem_manager_unlock();
  return val;
}

Uint32 Ndbd_mem_manager::get_shared_in_use() const {
  mt_mem_manager_lock();
  const Uint32 val = m_resource_limits.get_shared_in_use();
  mt_mem_manager_unlock();
  return val;
}

int cmp_chunk(const void *chunk_vptr_1, const void *chunk_vptr_2) {
  InitChunk *ptr1 = (InitChunk *)chunk_vptr_1;
  InitChunk *ptr2 = (InitChunk *)chunk_vptr_2;
  if (ptr1->m_ptr < ptr2->m_ptr) return -1;
  if (ptr1->m_ptr > ptr2->m_ptr) return 1;
  assert(false);
  return 0;
}

bool Ndbd_mem_manager::init(Uint32 *watchCounter, Uint32 max_pages,
                            bool alloc_less_memory) {
  assert(m_base_page == 0);
  assert(max_pages > 0);
  assert(m_resource_limits.get_allocated() == 0);

  DEB_MEM_ALLOC(("Allocating %u pages", max_pages));

  if (watchCounter) *watchCounter = 9;

  Uint32 pages = max_pages;
  Uint32 max_page = 0;

  const Uint64 pg = Uint64(sizeof(Alloc_page));
  if (pages == 0) {
    return false;
  }

#if SIZEOF_CHARP == 4
  Uint64 sum = (pg * pages);
  if (sum >= (Uint64(1) << 32)) {
    g_eventLogger->error(
        "Trying to allocate more that 4Gb with 32-bit binary!!");
    return false;
  }
#endif

  Uint32 allocated = 0;
  m_base_page = NULL;

#ifdef USE_DO_VIRTUAL_ALLOC
  {
    /*
     * Add one page per extra ZONE used due to using all zones even if not
     * needed.
     */
    int zones_needed = 1;
    for (zones_needed = 1; zones_needed <= ZONE_COUNT; zones_needed++) {
      if (pages < (zone_bound[zones_needed - 1] << PAGES_PER_REGION_LOG)) break;
    }
    pages += ZONE_COUNT - zones_needed;
  }
#endif

  /*
   * Always allocate even number of pages to cope with 64K system page size
   * on ARM.
   */
  if (pages % ALLOC_PAGES_PER_SYSTEM_PAGE != 0) {
    // Round up page count
    pages =
        (pages / ALLOC_PAGES_PER_SYSTEM_PAGE + 1) * ALLOC_PAGES_PER_SYSTEM_PAGE;
  }

#ifdef USE_DO_VIRTUAL_ALLOC
  {
    InitChunk chunks[ZONE_COUNT];
    if (do_virtual_alloc(pages, chunks, watchCounter, &m_base_page)) {
      for (int i = 0; i < ZONE_COUNT; i++) {
        m_unmapped_chunks.push_back(chunks[i]);
        DEB_MEM_ALLOC(("Adding one more chunk with %u pages", chunks[i].m_cnt));
        allocated += chunks[i].m_cnt;
      }
      require(allocated == pages);
    }
  }
#endif

#ifdef NDBD_RANDOM_START_PAGE
  if (m_base_page == NULL) {
    /**
     * In order to find bad-users of page-id's
     *   we add a random offset to the page-id's returned
     *   however, due to ZONE_19 that offset can't be that big
     *   (since we at get_page don't know if it's a HI/LO page)
     */
    Uint32 max_rand_start = ZONE_19_BOUND - 1;
    if (max_rand_start > pages) {
      max_rand_start -= pages;
      if (max_rand_start > 0x10000)
        m_random_start_page_id =
            0x10000 + (rand() % (max_rand_start - 0x10000));
      else if (max_rand_start)
        m_random_start_page_id = rand() % max_rand_start;

      assert(Uint64(pages) + Uint64(m_random_start_page_id) <= 0xFFFFFFFF);

      g_eventLogger->info("using m_random_start_page_id: %u (%.8x)",
                          m_random_start_page_id, m_random_start_page_id);
    }
  }
#endif

  /**
   * Do malloc
   */
  while (m_unmapped_chunks.size() < MAX_CHUNKS && allocated < pages) {
    InitChunk chunk;
    memset(&chunk, 0, sizeof(chunk));

    if (do_malloc(pages - allocated, &chunk, watchCounter, m_base_page)) {
      if (watchCounter) *watchCounter = 9;

      m_unmapped_chunks.push_back(chunk);
      allocated += chunk.m_cnt;
      DEB_MEM_ALLOC(("malloc of a chunk of %u pages", chunk.m_cnt));
      if (allocated < pages) {
        /* Add one more page for another chunk */
        pages += ALLOC_PAGES_PER_SYSTEM_PAGE;
      }
    } else {
      break;
    }
  }

  if (allocated < m_resource_limits.get_free_reserved()) {
    g_eventLogger->error(
        "Unable to alloc min memory from OS: min: %lldMb "
        " allocated: %lldMb",
        (Uint64)(sizeof(Alloc_page) * m_resource_limits.get_free_reserved()) >>
            20,
        (Uint64)(sizeof(Alloc_page) * allocated) >> 20);
    return false;
  } else if (allocated < pages) {
    g_eventLogger->warning(
        "Unable to alloc requested memory from OS: min: %lldMb"
        " requested: %lldMb allocated: %lldMb",
        (Uint64)(sizeof(Alloc_page) * m_resource_limits.get_free_reserved()) >>
            20,
        (Uint64)(sizeof(Alloc_page) * max_pages) >> 20,
        (Uint64)(sizeof(Alloc_page) * allocated) >> 20);
    if (!alloc_less_memory) return false;
  }

  if (m_base_page == NULL) {
    /**
     * Sort chunks...
     */
    qsort(m_unmapped_chunks.getBase(), m_unmapped_chunks.size(),
          sizeof(InitChunk), cmp_chunk);

    m_base_page = m_unmapped_chunks[0].m_ptr;
  }

  for (Uint32 i = 0; i < m_unmapped_chunks.size(); i++) {
    UintPtr start = UintPtr(m_unmapped_chunks[i].m_ptr) - UintPtr(m_base_page);
    start >>= (2 + BMW_2LOG);
    assert((Uint64(start) >> 32) == 0);
    m_unmapped_chunks[i].m_start = Uint32(start);
    Uint64 last64 = start + m_unmapped_chunks[i].m_cnt;
    assert((last64 >> 32) == 0);
    Uint32 last = Uint32(last64);

    if (last > max_page) max_page = last;
  }

  g_eventLogger->info("Ndbd_mem_manager::init(%d) min: %lluMb initial: %lluMb",
                      alloc_less_memory,
                      (pg * m_resource_limits.get_free_reserved()) >> 20,
                      (pg * pages) >> 20);

  m_resource_limits.set_max_page(max_page);
  m_resource_limits.set_allocated(0);

  return true;
}

void Ndbd_mem_manager::map(Uint32 *watchCounter, bool memlock,
                           Uint32 resources[]) {
  require(watchCounter != nullptr);
  Uint32 limit = ~(Uint32)0;
  Uint32 sofar = 0;

  if (resources != 0) {
    /*
     * To reduce start up time, only touch memory needed for selected resources.
     * The rest of memory will be touched in a second call to map.
     */
    limit = 0;
    for (Uint32 i = 0; resources[i]; i++) {
      limit += m_resource_limits.get_resource_reserved(resources[i]);
    }
    if (limit % ALLOC_PAGES_PER_SYSTEM_PAGE != 0) {
      limit +=
          ALLOC_PAGES_PER_SYSTEM_PAGE - (limit % ALLOC_PAGES_PER_SYSTEM_PAGE);
    }
  }

  while (m_unmapped_chunks.size() && sofar < limit) {
    Uint32 remain = limit - sofar;

    unsigned idx = m_unmapped_chunks.size() - 1;
    InitChunk *chunk = &m_unmapped_chunks[idx];
    if (watchCounter) *watchCounter = 9;

    if (chunk->m_cnt > remain) {
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
                        chunk->m_cnt, (int)sizeof(Alloc_page));

    ndbd_alloc_touch_mem(chunk->m_ptr, chunk->m_cnt * sizeof(Alloc_page),
                         watchCounter, true /* make_readwritable */);

    g_eventLogger->info("Touch Memory Completed");

    if (memlock) {
      /**
       * memlock pages that I added...
       */
      if (watchCounter) *watchCounter = 9;

      /**
       * Don't memlock everything in one go...
       *   cause then process won't be killable
       */
      const Alloc_page *start = chunk->m_ptr;
      Uint32 cnt = chunk->m_cnt;
      g_eventLogger->info("Lock Memory Starting, %u pages, page size = %d",
                          chunk->m_cnt, (int)sizeof(Alloc_page));

      while (cnt > 32768)  // 1G
      {
        if (watchCounter) *watchCounter = 9;

        NdbMem_MemLock(start, 32768 * sizeof(Alloc_page));
        start += 32768;
        cnt -= 32768;
      }
      if (watchCounter) *watchCounter = 9;

      NdbMem_MemLock(start, cnt * sizeof(Alloc_page));

      g_eventLogger->info("Lock memory Completed");
    }

    DEB_MEM_ALLOC(("grow %u pages", chunk->m_cnt));
    grow(chunk->m_start, chunk->m_cnt);
    sofar += chunk->m_cnt;

    m_unmapped_chunks.erase(idx);
  }

  mt_mem_manager_lock();
  if (resources == nullptr) {
    // Allow low prio use of shared only when all memory is mapped.
    m_resource_limits.update_low_prio_shared_limit();
  }
  m_resource_limits.check();
  mt_mem_manager_unlock();

  if (resources == 0 && memlock) {
    NdbMem_MemLockAll(1);
  }

  /* Note: calls to map() must be serialized by other means. */
  m_mapped_pages_lock.write_lock();
  if (m_mapped_pages_new_count != m_mapped_pages_count) {
    /* Do not support shrinking memory */
    require(m_mapped_pages_new_count > m_mapped_pages_count);

    qsort(m_mapped_pages, m_mapped_pages_new_count, sizeof(m_mapped_pages[0]),
          PageInterval::compare);

    /* Validate no overlapping intervals */
    for (Uint32 i = 1; i < m_mapped_pages_new_count; i++) {
      require(m_mapped_pages[i - 1].end <= m_mapped_pages[i].start);
    }

    m_mapped_pages_count = m_mapped_pages_new_count;
  }
  m_mapped_pages_lock.write_unlock();
}

void Ndbd_mem_manager::init_resource_spare(Uint32 id, Uint32 pct) {
  mt_mem_manager_lock();
  m_resource_limits.init_resource_spare(id, pct);
  mt_mem_manager_unlock();
}

#include <NdbOut.hpp>

void Ndbd_mem_manager::grow(Uint32 start, Uint32 cnt) {
  assert(cnt);
  Uint32 start_bmp = start >> BPP_2LOG;
  Uint32 last_bmp = (start + cnt - 1) >> BPP_2LOG;

#if SIZEOF_CHARP == 4
  assert(start_bmp == 0 && last_bmp == 0);
#endif

  if (start_bmp != last_bmp) {
    Uint32 tmp = ((start_bmp + 1) << BPP_2LOG) - start;
    grow(start, tmp);
    grow((start_bmp + 1) << BPP_2LOG, cnt - tmp);
    return;
  }

  for (Uint32 i = 0; i < m_used_bitmap_pages.size(); i++)
    if (m_used_bitmap_pages[i] == start_bmp) {
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
          m_mapped_pages[m_mapped_pages_new_count - 1].end == start) {
        m_mapped_pages[m_mapped_pages_new_count - 1].end = start + cnt;
      } else {
        require(m_mapped_pages_new_count < NDB_ARRAY_SIZE(m_mapped_pages));
        m_mapped_pages[m_mapped_pages_new_count].start = start;
        m_mapped_pages[m_mapped_pages_new_count].end = start + cnt;
        m_mapped_pages_new_count++;
      }
      goto found;
    }

  if (start != (start_bmp << BPP_2LOG)) {
    g_eventLogger->info(
        "ndbd_malloc_impl.cpp:%d:grow(%d, %d) %d!=%d not using %uMb"
        " - Unable to use due to bitmap pages missaligned!!",
        __LINE__, start, cnt, start, (start_bmp << BPP_2LOG),
        (cnt >> (20 - 15)));
    g_eventLogger->error(
        "ndbd_malloc_impl.cpp:%d:grow(%d, %d) not using %uMb"
        " - Unable to use due to bitmap pages missaligned!!",
        __LINE__, start, cnt, (cnt >> (20 - 15)));

    dump(false);
    return;
  }

#ifdef UNIT_TEST
  g_eventLogger->info("creating bitmap page %d", start_bmp);
#endif

  if (m_mapped_pages_new_count > 0 &&
      m_mapped_pages[m_mapped_pages_new_count - 1].end == start) {
    m_mapped_pages[m_mapped_pages_new_count - 1].end = start + cnt;
  } else {
    require(m_mapped_pages_new_count < NDB_ARRAY_SIZE(m_mapped_pages));
    m_mapped_pages[m_mapped_pages_new_count].start = start;
    m_mapped_pages[m_mapped_pages_new_count].end = start + cnt;
    m_mapped_pages_new_count++;
  }

  {
    Alloc_page *bmp = m_base_page + start;
    memset(bmp, 0, sizeof(Alloc_page));
    cnt--;
    start++;
  }
  m_used_bitmap_pages.push_back(start_bmp);

found:
  if ((start + cnt) == ((start_bmp + 1) << BPP_2LOG)) {
    cnt--;  // last page is always marked as empty
  }

  if (cnt) {
    mt_mem_manager_lock();
    const Uint32 allocated = m_resource_limits.get_allocated();
    m_resource_limits.set_allocated(allocated + cnt);
    const Uint64 mbytes = ((Uint64(cnt) * 32) + 1023) / 1024;
    /**
     * grow first split large page ranges to ranges completely within
     * a BPP regions.
     * Boundary between lo and high zone coincide with a BPP region
     * boundary.
     */
    static_assert((ZONE_19_BOUND & ((1 << BPP_2LOG) - 1)) == 0);
    if (start < ZONE_19_BOUND) {
      require(start + cnt < ZONE_19_BOUND);
      g_eventLogger->info("Adding %uMb to ZONE_19 (%u, %u)", (Uint32)mbytes,
                          start, cnt);
    } else if (start < ZONE_27_BOUND) {
      require(start + cnt < ZONE_27_BOUND);
      g_eventLogger->info("Adding %uMb to ZONE_27 (%u, %u)", (Uint32)mbytes,
                          start, cnt);
    } else if (start < ZONE_30_BOUND) {
      require(start + cnt < ZONE_30_BOUND);
      g_eventLogger->info("Adding %uMb to ZONE_30 (%u, %u)", (Uint32)mbytes,
                          start, cnt);
    } else {
      g_eventLogger->info("Adding %uMb to ZONE_32 (%u, %u)", (Uint32)mbytes,
                          start, cnt);
    }
    release(start, cnt);
    mt_mem_manager_unlock();
  }
}

void Ndbd_mem_manager::release(Uint32 start, Uint32 cnt) {
  assert(start);
#if defined VM_TRACE || defined ERROR_INSERT
  memset(m_base_page + start, 0xF5, cnt * sizeof(m_base_page[0]));
#endif

  set(start, start + cnt - 1);

  Uint32 zone = get_page_zone(start);
  release_impl(zone, start, cnt);
}

void Ndbd_mem_manager::release_impl(Uint32 zone, Uint32 start, Uint32 cnt) {
  assert(start);

  Uint32 test = check(start - 1, start + cnt);
  if (test & 1) {
    Free_page_data *fd = get_free_page_data(m_base_page + start - 1, start - 1);
    Uint32 sz = fd->m_size;
    Uint32 left = start - sz;
    remove_free_list(zone, left, fd->m_list);
    cnt += sz;
    start = left;
  }

  Uint32 right = start + cnt;
  if (test & 2) {
    Free_page_data *fd = get_free_page_data(m_base_page + right, right);
    Uint32 sz = fd->m_size;
    remove_free_list(zone, right, fd->m_list);
    cnt += sz;
  }

  insert_free_list(zone, start, cnt);
}

void Ndbd_mem_manager::alloc(AllocZone zone, Uint32 *ret, Uint32 *pages,
                             Uint32 min) {
  const Uint32 save = *pages;
  for (Uint32 z = zone;; z--) {
    alloc_impl(z, ret, pages, min);
    if (*pages) {
#if defined VM_TRACE || defined ERROR_INSERT
      memset(m_base_page + *ret, 0xF6, *pages * sizeof(m_base_page[0]));
#endif
      return;
    }
    if (z == 0) {
      if (unlikely(m_dump_on_alloc_fail)) {
        g_eventLogger->info(
            "Page allocation failed in %s: zone=%u pages=%u (at least %u)",
            __func__, zone, save, min);
        dump(true);
      }
      return;
    }
    *pages = save;
  }
}

void Ndbd_mem_manager::alloc_impl(Uint32 zone, Uint32 *ret, Uint32 *pages,
                                  Uint32 min) {
  Int32 i;
  Uint32 start;
  Uint32 cnt = *pages;
  Uint32 list = ndb_log2(cnt - 1);

  assert(cnt);
  assert(list <= 16);

  for (i = list; i < 16; i++) {
    if ((start = m_buddy_lists[zone][i])) {
      /* ---------------------------------------------------------------- */
      /*       PROPER AMOUNT OF PAGES WERE FOUND. NOW SPLIT THE FOUND     */
      /*       AREA AND RETURN THE PART NOT NEEDED.                       */
      /* ---------------------------------------------------------------- */

      Uint32 sz = remove_free_list(zone, start, i);
      Uint32 extra = sz - cnt;
      assert(sz >= cnt);
      if (extra) {
        insert_free_list(zone, start + cnt, extra);
        clear_and_set(start, start + cnt - 1);
      } else {
        clear(start, start + cnt - 1);
      }
      *ret = start;
      assert(m_resource_limits.get_in_use() + cnt <=
             m_resource_limits.get_allocated());
      return;
    }
  }

  /**
   * Could not find in quaranteed list...
   *   search in other lists...
   */

  Int32 min_list = ndb_log2(min - 1);
  assert((Int32)list >= min_list);
  for (i = list - 1; i >= min_list; i--) {
    if ((start = m_buddy_lists[zone][i])) {
      Uint32 sz = remove_free_list(zone, start, i);
      Uint32 extra = sz - cnt;
      if (sz > cnt) {
        insert_free_list(zone, start + cnt, extra);
        sz -= extra;
        clear_and_set(start, start + sz - 1);
      } else {
        clear(start, start + sz - 1);
      }

      *ret = start;
      *pages = sz;
      assert(m_resource_limits.get_in_use() + sz <=
             m_resource_limits.get_allocated());
      return;
    }
  }
  *pages = 0;
}

void Ndbd_mem_manager::insert_free_list(Uint32 zone, Uint32 start,
                                        Uint32 size) {
  Uint32 list = ndb_log2(size) - 1;
  Uint32 last = start + size - 1;

  Uint32 head = m_buddy_lists[zone][list];
  Free_page_data *fd_first = get_free_page_data(m_base_page + start, start);
  fd_first->m_list = list;
  fd_first->m_next = head;
  fd_first->m_prev = 0;
  fd_first->m_size = size;

  Free_page_data *fd_last = get_free_page_data(m_base_page + last, last);
  fd_last->m_list = list;
  fd_last->m_next = head;
  fd_last->m_prev = 0;
  fd_last->m_size = size;

  if (head) {
    Free_page_data *fd = get_free_page_data(m_base_page + head, head);
    assert(fd->m_prev == 0);
    assert(fd->m_list == list);
    fd->m_prev = start;
  }

  m_buddy_lists[zone][list] = start;
}

Uint32 Ndbd_mem_manager::remove_free_list(Uint32 zone, Uint32 start,
                                          Uint32 list) {
  Free_page_data *fd = get_free_page_data(m_base_page + start, start);
  Uint32 size = fd->m_size;
  Uint32 next = fd->m_next;
  Uint32 prev = fd->m_prev;
  assert(fd->m_list == list);

  if (prev) {
    assert(m_buddy_lists[zone][list] != start);
    fd = get_free_page_data(m_base_page + prev, prev);
    assert(fd->m_next == start);
    assert(fd->m_list == list);
    fd->m_next = next;
  } else {
    assert(m_buddy_lists[zone][list] == start);
    m_buddy_lists[zone][list] = next;
  }

  if (next) {
    fd = get_free_page_data(m_base_page + next, next);
    assert(fd->m_list == list);
    assert(fd->m_prev == start);
    fd->m_prev = prev;
  }

  return size;
}

void Ndbd_mem_manager::dump(bool locked) const {
  if (!locked) mt_mem_manager_lock();
  g_eventLogger->info("Begin Ndbd_mem_manager::dump");
  for (Uint32 zone = 0; zone < ZONE_COUNT; zone++) {
    g_eventLogger->info("zone %u", zone);
    for (Uint32 i = 0; i < 16; i++) {
      Uint32 head = m_buddy_lists[zone][i];
      if (head == 0) continue;
      g_eventLogger->info(" list: %d - ", i);
      while (head) {
        Free_page_data *fd = get_free_page_data(m_base_page + head, head);
        g_eventLogger->info("[ i: %d prev %d next %d list %d size %d ] ", head,
                            fd->m_prev, fd->m_next, fd->m_list, fd->m_size);
        head = fd->m_next;
      }
      g_eventLogger->info("EOL");
    }
  }
  m_resource_limits.dump();
  g_eventLogger->info("End Ndbd_mem_manager::dump");
  if (!locked) mt_mem_manager_unlock();
}

void Ndbd_mem_manager::dump_on_alloc_fail(bool on) {
  m_dump_on_alloc_fail = on;
}

void Ndbd_mem_manager::lock() { mt_mem_manager_lock(); }

void Ndbd_mem_manager::unlock() { mt_mem_manager_unlock(); }

void *Ndbd_mem_manager::alloc_page(Uint32 type, Uint32 *i, AllocZone zone,
                                   bool locked, bool use_max_part) {
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  if (!locked) mt_mem_manager_lock();

  m_resource_limits.reclaim_lent_pages(idx, 1);

  Uint32 cnt = 1;
  const Uint32 min = 1;
  const Uint32 free_res = m_resource_limits.get_resource_free_reserved(idx);
  if (free_res < cnt) {
    if (use_max_part) {
      const Uint32 free_shr = m_resource_limits.get_resource_free_shared(idx);
      const Uint32 free = m_resource_limits.get_resource_free(idx);
      if (free < min || (free_shr + free_res < min)) {
        if (unlikely(m_dump_on_alloc_fail)) {
          g_eventLogger->info(
              "Page allocation failed in %s: no free resource page.", __func__);
          dump(true);
        }
        if (!locked) mt_mem_manager_unlock();
        return NULL;
      }
    } else {
      if (unlikely(m_dump_on_alloc_fail)) {
        g_eventLogger->info(
            "Page allocation failed in %s: no free reserved resource page.",
            __func__);
        dump(true);
      }
      if (!locked) mt_mem_manager_unlock();
      return NULL;
    }
  }
  alloc(zone, i, &cnt, min);
  if (likely(cnt)) {
    const Uint32 spare_taken =
        m_resource_limits.post_alloc_resource_pages(idx, cnt);
    if (spare_taken > 0) {
      require(spare_taken == cnt);
      release(*i, spare_taken);
      m_resource_limits.check();
      if (unlikely(m_dump_on_alloc_fail)) {
        g_eventLogger->info(
            "Page allocation failed in %s: no free non-spare resource page.",
            __func__);
        dump(true);
      }
      if (!locked) mt_mem_manager_unlock();
      *i = RNIL;
      return NULL;
    }
    m_resource_limits.check();
    if (!locked) mt_mem_manager_unlock();
#ifdef NDBD_RANDOM_START_PAGE
    *i += m_random_start_page_id;
    return m_base_page + *i - m_random_start_page_id;
#else
    return m_base_page + *i;
#endif
  }
  if (unlikely(m_dump_on_alloc_fail)) {
    g_eventLogger->info(
        "Page allocation failed in %s: no page available in zone %d.", __func__,
        zone);
    dump(true);
  }
  if (!locked) mt_mem_manager_unlock();
  return 0;
}

void *Ndbd_mem_manager::alloc_spare_page(Uint32 type, Uint32 *i,
                                         AllocZone zone) {
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  mt_mem_manager_lock();

  Uint32 cnt = 1;
  const Uint32 min = 1;
  if (m_resource_limits.get_resource_spare(idx) >= min) {
    alloc(zone, i, &cnt, min);
    if (likely(cnt)) {
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
  if (unlikely(m_dump_on_alloc_fail)) {
    g_eventLogger->info("Page allocation failed in %s: no spare page.",
                        __func__);
    dump(true);
  }
  mt_mem_manager_unlock();
  return 0;
}

void Ndbd_mem_manager::release_page(Uint32 type, Uint32 i, bool locked) {
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  if (!locked) mt_mem_manager_lock();

#ifdef NDBD_RANDOM_START_PAGE
  i -= m_random_start_page_id;
#endif

  release(i, 1);
  m_resource_limits.post_release_resource_pages(idx, 1);

  m_resource_limits.check();
  if (!locked) mt_mem_manager_unlock();
}

void Ndbd_mem_manager::alloc_pages(Uint32 type, Uint32 *i, Uint32 *cnt,
                                   Uint32 min, AllocZone zone, bool locked) {
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  if (!locked) mt_mem_manager_lock();

  Uint32 req = *cnt;
  m_resource_limits.reclaim_lent_pages(idx, req);

  const Uint32 free_res = m_resource_limits.get_resource_free_reserved(idx);
  if (free_res < req) {
    const Uint32 free = m_resource_limits.get_resource_free(idx);
    if (free < req) {
      req = free;
    }
    const Uint32 free_shr = m_resource_limits.get_free_shared();
    if (free_shr + free_res < req) {
      req = free_shr + free_res;
    }
    if (req < min) {
      *cnt = 0;
      if (unlikely(m_dump_on_alloc_fail)) {
        g_eventLogger->info(
            "Page allocation failed in %s: not enough free resource pages.",
            __func__);
        dump(true);
      }
      if (!locked) mt_mem_manager_unlock();
      return;
    }
  }

  // Hi order allocations can always use any zone
  alloc(zone, i, &req, min);
  const Uint32 spare_taken =
      m_resource_limits.post_alloc_resource_pages(idx, req);
  if (spare_taken > 0) {
    req -= spare_taken;
    release(*i + req, spare_taken);
  }
  if (0 < req && req < min) {
    release(*i, req);
    m_resource_limits.post_release_resource_pages(idx, req);
    req = 0;
  }
  *cnt = req;
  m_resource_limits.check();
  if (req == 0 && unlikely(m_dump_on_alloc_fail)) {
    g_eventLogger->info(
        "Page allocation failed in %s: no page available in zone %d.", __func__,
        zone);
    dump(true);
  }
  if (!locked) mt_mem_manager_unlock();
#ifdef NDBD_RANDOM_START_PAGE
  *i += m_random_start_page_id;
#endif
}

void Ndbd_mem_manager::release_pages(Uint32 type, Uint32 i, Uint32 cnt,
                                     bool locked) {
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  if (!locked) mt_mem_manager_lock();

#ifdef NDBD_RANDOM_START_PAGE
  i -= m_random_start_page_id;
#endif

  release(i, cnt);
  m_resource_limits.post_release_resource_pages(idx, cnt);
  m_resource_limits.check();
  if (!locked) mt_mem_manager_unlock();
}

/** Transfer pages between resource groups without risk that some other
 * resource gets them in between.
 *
 * In some cases allocating pages fail.  Preferable the application can handle
 * the allocation failure gracefully.
 * In other cases application really need to have those pages.
 * For that the memory manager support giving up and taking pages.
 *
 * The allocation may fail, either because there are no free pages at all, or
 * that all free pages are reserved by other resources, or that the current
 * resource have reached it upper limit of allowed allocations.
 *
 * One can use a combination of give_up_pages() and take_pages() instead of
 * release_pages() and alloc_pages() to avoid that the pages are put into the
 * global free list of pages but rather only the book keeping about how many
 * pages are used in what way.
 *
 * An examples transferring pages from DM to TM.
 *
 * 1) Try do an ordinary alloc_pages(TM) first. If that succeed there is no
 *    need for special page transfer.  Follow up with release_pages(DM).
 *
 * 2) When alloc_pages(TM) fail, do give_up_pages(DM) instead of
 *    release_pages(DM).  This function should never fail.
 *    All given up pages will be counted as lent.
 *    These pages may not be further used by DM until lent count is decreased.
 *    See point 5) how lent pages are reclaimed.
 *
 * 3) Call take_pages(TM).  This will increase the count of pages in use for
 *    TM, as a normal alloc_pages() would do.  And the borrowed pages count is
 *    increased.
 *
 * 4) When later calling release_pages(TM), it will decrease both the global
 *    and the TM resource borrow count.  This will eventually allow reclaim of
 *    lent DM pages, see next point.
 *
 * 5) When later calling alloc_pages(DM) it will first try to reclaim lent out
 *    pages.
 *    If the global counts for untaken and borrowed together is less than the
 *    global lent count, that means that some lent pages have been
 *    taken/borrowed and also released and those we may reclaim that many lent
 *    pages.
 *    If DM has lent pages, The minimum of globally reclaimable lent pages and
 *    request count of pages and the number of lent pages in resource are
 *    reclaimed.
 *
 * Code example:
 *
    ...
    Uint32 page_count = 3;
    Uint32 DM_page_no;
    Uint32 DM_page_count = page_count;
    mem.alloc_pages(RG_DM, &DM_page_no, &DM_page_count, page_count);
    ...
    assert(DM_page_count == page_count);
    Uint32 TM_page_no;
    Uint32 TM_page_count = page_count;
    mem.alloc_pages(RG_TM, &TM_page_no, &TM_page_count, page_count);
    if (TM_page_count != 0)
    {
      mem.release_pages(RG_DM, DM_page_no, page_count);
    }
    else
    {
      require(mem.give_up_pages(RG_DM, page_count));
      require(mem.take_pages(RG_TM, page_count));
      DM_page_no = TM_page_no;
      TM_page_count = page_count;
    }
    ...
    mem.release_pages(RG_TM, TM_page_no, TM_page_count);
    ...
    DM_page_count = 1;
    // Typically will reclaim one lent out DM page
    mem.alloc_pages(RG_DM, &DM_page_no, &DM_page_count, 1);
    ...
    mem.release_pages(RG_DM, DM_page_no, DM_page_count);
    ...
 */

bool Resource_limits::give_up_pages(Uint32 id, Uint32 cnt) {
  const Resource_limit &rl = m_limit[id - 1];

  /* Only support give up pages for resources with only reserved pages to
   * simplify logic.
   */

  require(rl.m_min == rl.m_max);

  if (get_resource_in_use(id) < cnt) {
    // Can not pass more pages than actually in use!
    return false;
  }

  post_release_resource_pages(id, cnt);
  inc_untaken(cnt);
  inc_resource_lent(id, cnt);
  inc_lent(cnt);
  dec_free_reserved(cnt);

  return true;
}

bool Ndbd_mem_manager::give_up_pages(Uint32 type, Uint32 cnt) {
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  mt_mem_manager_lock();

  if (!m_resource_limits.give_up_pages(idx, cnt)) {
    m_resource_limits.dump();
    mt_mem_manager_unlock();
    return false;
  }

  m_resource_limits.check();
  mt_mem_manager_unlock();
  return true;
}

bool Resource_limits::take_pages(Uint32 id, Uint32 cnt) {
  const Resource_limit &rl = m_limit[id - 1];

  /* Support take pages only for "unlimited" resources (m_max == HIGHEST_LIMIT)
   * and with no spare pages (m_spare_pct == 0) to simplify logic.
   */

  require(rl.m_max == Resource_limit::HIGHEST_LIMIT);
  require(rl.m_spare_pct == 0);

  if (m_untaken < cnt) {
    return false;
  }

  inc_resource_borrowed(id, cnt);
  inc_borrowed(cnt);
  dec_untaken(cnt);
  const Uint32 spare_taken = post_alloc_resource_pages(id, cnt);
  require(spare_taken == 0);

  return true;
}

bool Ndbd_mem_manager::take_pages(Uint32 type, Uint32 cnt) {
  Uint32 idx = type & RG_MASK;
  assert(idx && idx <= MM_RG_COUNT);
  mt_mem_manager_lock();

  if (!m_resource_limits.take_pages(idx, cnt)) {
    m_resource_limits.dump();
    mt_mem_manager_unlock();
    return false;
  }

  m_resource_limits.check();
  mt_mem_manager_unlock();
  return true;
}

template class Vector<InitChunk>;

#if defined(TEST_NDBD_MALLOC)

#include <NdbHost.h>
#include <Vector.hpp>
#include "portlib/NdbTick.h"
#include "portlib/ndb_stacktrace.h"

struct Chunk {
  Uint32 pageId;
  Uint32 pageCount;
};

struct Timer {
  Uint64 sum;
  Uint32 cnt;

  Timer() { sum = cnt = 0; }

  NDB_TICKS st;

  void start() { st = NdbTick_getCurrentTicks(); }

  Uint64 calc_diff() {
    const NDB_TICKS st2 = NdbTick_getCurrentTicks();
    const NdbDuration dur = NdbTick_Elapsed(st, st2);
    return dur.microSec();
  }

  void stop() { add(calc_diff()); }

  void add(Uint64 diff) {
    sum += diff;
    cnt++;
  }

  void print(const char *title) const {
    float ps = sum;
    ps /= cnt;
    printf("%s %fus/call %lld %d\n", title, ps, sum, cnt);
  }
};

void abort_handler(int signum) {
  ndb_print_stacktrace();
  signal(SIGABRT, SIG_DFL);
  abort();
}

class Test_mem_manager : public Ndbd_mem_manager {
 public:
  static constexpr Uint32 ZONE_COUNT = Ndbd_mem_manager::ZONE_COUNT;
  Test_mem_manager(Uint32 tot_mem, Uint32 data_mem, Uint32 trans_mem,
                   Uint32 data_mem2 = 0, Uint32 trans_mem2 = 0);
  ~Test_mem_manager();

 private:
  Uint32 m_leaked_mem;
};

enum Resource_groups {
  RG_DM = 1,
  RG_TM = 2,
  RG_QM = 3,
  RG_DM2 = 4,
  RG_TM2 = 5,
  RG_QM2 = 6,
};

Test_mem_manager::Test_mem_manager(Uint32 tot_mem, Uint32 data_mem,
                                   Uint32 trans_mem, Uint32 data_mem2,
                                   Uint32 trans_mem2) {
  const Uint32 reserved_mem = data_mem + trans_mem + data_mem2 + trans_mem2;
  assert(tot_mem >= reserved_mem);

  Resource_limit rl;
  // Data memory
  rl.m_min = data_mem;
  rl.m_max = rl.m_min;
  rl.m_resource_id = RG_DM;
  set_resource_limit(rl);

  // Transaction memory
  rl.m_min = trans_mem;
  rl.m_max = Resource_limit::HIGHEST_LIMIT;
  rl.m_resource_id = RG_TM;
  set_resource_limit(rl);

  // Query memory
  rl.m_min = 0;
  rl.m_max = Resource_limit::HIGHEST_LIMIT;
  rl.m_resource_id = RG_QM;
  set_resource_limit(rl);

  // Data memory
  rl.m_min = data_mem2;
  rl.m_max = rl.m_min;
  rl.m_resource_id = RG_DM2;
  set_resource_limit(rl);

  // Transaction memory
  rl.m_min = trans_mem2;
  rl.m_max = Resource_limit::HIGHEST_LIMIT;
  rl.m_resource_id = RG_TM2;
  set_resource_limit(rl);

  // Query memory
  rl.m_min = 0;
  rl.m_max = Resource_limit::HIGHEST_LIMIT;
  rl.m_resource_id = RG_QM2;
  set_resource_limit(rl);

  /*
   * Add one extra page for the initial bitmap page and the final empty page
   * for each complete region (8GiB).
   * And one extra page for initial page of last region which do not need an
   * empty page.
   */
  require(tot_mem > 0);
  const Uint32 extra_mem = 2 * ((tot_mem - 1) / ALLOC_PAGES_PER_REGION) + 1;
  init(NULL, tot_mem + extra_mem);
  Uint32 dummy_watchdog_counter_marking_page_mem = 0;
  map(&dummy_watchdog_counter_marking_page_mem);

  /*
   * Depending on system page size, or if build have
   * NDB_TEST_128TB_VIRTUAL_MEMORY on, the actual pages available can be more
   * than estimated. For test program to only see the expected number of pages
   * one need to allocate some pages to hide them.
   */

  const Ndbd_mem_manager::AllocZone zone = Ndbd_mem_manager::NDB_ZONE_LE_32;

  Uint32 shared_mem = tot_mem - reserved_mem;
  Uint32 page_count = 0;
  Uint32 *free_pages = new Uint32[trans_mem + shared_mem];
  while (page_count < trans_mem + shared_mem &&
         alloc_page(RG_TM, &free_pages[page_count], zone)) {
    page_count++;
  }

  /* hide and leak all other pages */
  Uint32 leak_page;
  Uint32 leak_count = 0;
  while (alloc_page(RG_TM, &leak_page, zone)) leak_count++;
  m_leaked_mem = leak_count;

  /* free pages again */
  while (page_count > 0) {
    page_count--;
    release_page(RG_TM, free_pages[page_count]);
  }
  delete[] free_pages;
}

Test_mem_manager::~Test_mem_manager() {
  require(m_resource_limits.get_in_use() == m_leaked_mem);
}

#define NDBD_MALLOC_PERF_TEST 0
static void perf_test(int sz, int run_time);
static void transfer_test();

int main(int argc, char **argv) {
  ndb_init();
  ndb_init_stacktrace();
  signal(SIGABRT, abort_handler);

  int sz = 1 * 32768;
  int run_time = 30;
  if (argc > 1) sz = 32 * atoi(argv[1]);

  if (argc > 2) run_time = atoi(argv[2]);

  g_eventLogger->createConsoleHandler();
  g_eventLogger->setCategory("ndbd_malloc-t");
  g_eventLogger->enable(Logger::LL_ON, Logger::LL_INFO);
  g_eventLogger->enable(Logger::LL_ON, Logger::LL_CRITICAL);
  g_eventLogger->enable(Logger::LL_ON, Logger::LL_ERROR);
  g_eventLogger->enable(Logger::LL_ON, Logger::LL_WARNING);

  transfer_test();

  if (NDBD_MALLOC_PERF_TEST) {
    perf_test(sz, run_time);
  }

  ndb_end(0);
}

#define DEBUG 0

void transfer_test() {
  const Uint32 data_pages = 18;
  Test_mem_manager mem(data_pages, 4, 4, 4, 4);
  Ndbd_mem_manager::AllocZone zone = Ndbd_mem_manager::NDB_ZONE_LE_32;

  Uint32 dm[4 + 1];
  Uint32 dm2[4];
  Uint32 tm[6];
  Uint32 tm2[6];

  if (DEBUG) mem.dump(false);

  // Allocate 4 pages each from DM and DM2 resources.
  for (int i = 0; i < 4; i++) {
    require(mem.alloc_page(RG_DM, &dm[i], zone));
    require(mem.alloc_page(RG_DM2, &dm2[i], zone));
  }

  // Allocate 5 pages each from TM and TM2 resources.
  for (int i = 0; i < 5; i++) {
    require(mem.alloc_page(RG_TM, &tm[i], zone));
    require(mem.alloc_page(RG_TM2, &tm2[i], zone));
  }

  // Allocating a 6th page for TM should fail since all 18 pages are allocated.
  require(mem.alloc_page(RG_TM, &tm[5], zone) == nullptr);

  // Start transfer of pages from RG_DM to RG_TM
  require(mem.give_up_pages(RG_DM, 1));

  /* Start and complete transfer between RG_DM2 to RG_TM2 before completing
   * transfer from RG_DM to RG_TM started above.
   */
  require(mem.alloc_page(RG_TM2, &tm2[5], zone) == nullptr);
  require(mem.give_up_pages(RG_DM2, 1));
  require(mem.take_pages(RG_TM2, 1));
  tm2[5] = dm2[3];
  dm2[3] = RNIL;
  mem.release_page(RG_TM2, tm2[5]);

  /* Verify that one can not allocate a page for RG_DM since it already have
   * reached its maximum of 4 (including the lent page)
   */
  require(mem.alloc_page(RG_DM, &dm[4], zone) != nullptr);

  // Proceed with taking over the page to RG_TM
  require(mem.take_pages(RG_TM, 1));
  tm[5] = dm[3];
  dm[3] = RNIL;

  require(mem.alloc_page(RG_DM, &dm[3], zone) == nullptr);

  mem.release_page(RG_DM, dm[4]);
  mem.release_page(RG_TM, tm[5]);

  require(mem.alloc_page(RG_DM, &dm[3], zone));
  require(mem.alloc_page(RG_DM2, &dm2[3], zone));

  // Cleanup, release all allocated pages.
  for (int i = 0; i < 4; i++) {
    mem.release_page(RG_DM, dm[i]);
    mem.release_page(RG_DM2, dm2[i]);
  }

  for (int i = 0; i < 5; i++) {
    mem.release_page(RG_TM, tm[i]);
    mem.release_page(RG_TM2, tm2[i]);
  }

  if (DEBUG) mem.dump(false);
}

void perf_test(int sz, int run_time) {
  char buf[255];
  Timer timer[4];
  printf("Startar modul test av Page Manager %dMb %ds\n", (sz >> 5), run_time);

  const Uint32 data_sz = sz / 3;
  const Uint32 trans_sz = sz / 3;
  Test_mem_manager mem(sz, data_sz, trans_sz);
  mem.dump(false);

  printf("pid: %d press enter to continue\n", NdbHost_GetProcessId());
  fgets(buf, sizeof(buf), stdin);

  Vector<Chunk> chunks;
  Ndbd_mem_manager::AllocZone zone = Ndbd_mem_manager::NDB_ZONE_LE_32;
  time_t stop = time(0) + run_time;
  for (Uint32 i = 0; time(0) < stop; i++) {
    mem.dump(false);
    printf("pid: %d press enter to continue\n", NdbHost_GetProcessId());
    fgets(buf, sizeof(buf), stdin);
    time_t stop = time(0) + run_time;
    for (Uint32 i = 0; time(0) < stop; i++) {
      // Case
      Uint32 c = (rand() % 100);
      if (c < 50) {
        c = 0;
      } else if (c < 93) {
        c = 1;
      } else {
        c = 2;
      }

      Uint32 alloc = 1 + rand() % 3200;

      if (chunks.size() == 0 && c == 0) {
        c = 1 + rand() % 2;
      }

      if (DEBUG) {
        printf("loop=%d ", i);
      }
      switch (c) {
        case 0: {  // Release
          const int ch = rand() % chunks.size();
          Chunk chunk = chunks[ch];
          chunks.erase(ch);
          timer[0].start();
          mem.release_pages(RG_DM, chunk.pageId, chunk.pageCount);
          timer[0].stop();
          if (DEBUG) {
            printf(" release %d %d\n", chunk.pageId, chunk.pageCount);
          }
        } break;
        case 2: {  // Seize(n) - fail
          alloc += sz;
        }
          [[fallthrough]];
        case 1: {  // Seize(n) (success)
          Chunk chunk;
          chunk.pageCount = alloc;
          if (DEBUG) {
            printf(" alloc %d -> ", alloc);
            fflush(stdout);
          }
          timer[0].start();
          mem.alloc_pages(RG_DM, &chunk.pageId, &chunk.pageCount, 1, zone);
          Uint64 diff = timer[0].calc_diff();

          if (DEBUG) {
            printf("%d %d", chunk.pageId, chunk.pageCount);
          }
          assert(chunk.pageCount <= alloc);
          if (chunk.pageCount != 0) {
            chunks.push_back(chunk);
            if (chunk.pageCount != alloc) {
              timer[2].add(diff);
              if (DEBUG) {
                printf(
                    " -  Tried to allocate %d - only allocated %d - free: %d",
                    alloc, chunk.pageCount, 0);
              }
            } else {
              timer[1].add(diff);
            }
          } else {
            timer[3].add(diff);
            if (DEBUG) {
              printf("  Failed to alloc %d pages with %d pages free", alloc, 0);
            }
          }
          if (DEBUG) {
            printf("\n");
          }
        } break;
      }
    }
  }
  if (!DEBUG) {
    while (chunks.size() > 0) {
      Chunk chunk = chunks.back();
      mem.release_pages(RG_DM, chunk.pageId, chunk.pageCount);
      chunks.erase(chunks.size() - 1);
    }
  }

  const char *title[] = {"release   ", "alloc full", "alloc part",
                         "alloc fail"};
  for (Uint32 i = 0; i < 4; i++) {
    timer[i].print(title[i]);
  }
  mem.dump(false);
}

template class Vector<Chunk>;

#endif
