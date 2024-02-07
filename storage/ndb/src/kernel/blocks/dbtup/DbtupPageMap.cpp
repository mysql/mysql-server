/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#define DBTUP_C
#define DBTUP_PAGE_MAP_CPP
#include <ndb_limits.h>
#include <RefConvert.hpp>
#include <pc.hpp>
#include <signaldata/RestoreImpl.hpp>
#include "../backup/Backup.hpp"
#include "../dblqh/Dblqh.hpp"
#include "Dbtup.hpp"

#define JAM_FILE_ID 415

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
//#define DEBUG_LCP 1
//#define DEBUG_LCP_REL 1
//#define DEBUG_LCP_ALLOC 1
//#define DEBUG_LCP_FREE 1
//#define DEBUG_LCP_SKIP 1
//#define DEBUG_LCP_SCANNED_BIT 1
#endif

#ifdef DEBUG_LCP
#define DEB_LCP(arglist)         \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_LCP(arglist) \
  do {                   \
  } while (0)
#endif

#ifdef DEBUG_LCP_REL
#define DEB_LCP_REL(arglist)     \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_LCP_REL(arglist) \
  do {                       \
  } while (0)
#endif

#ifdef DEBUG_LCP_ALLOC
#define DEB_LCP_ALLOC(arglist)   \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_LCP_ALLOC(arglist) \
  do {                         \
  } while (0)
#endif

#ifdef DEBUG_LCP_FREE
#define DEB_LCP_FREE(arglist)    \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_LCP_FREE(arglist) \
  do {                        \
  } while (0)
#endif

#ifdef DEBUG_LCP_SKIP
#define DEB_LCP_SKIP(arglist)    \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_LCP_SKIP(arglist) \
  do {                        \
  } while (0)
#endif

#ifdef DEBUG_LCP_SCANNED_BIT
#define DEB_LCP_SCANNED_BIT(arglist) \
  do {                               \
    g_eventLogger->info arglist;     \
  } while (0)
#else
#define DEB_LCP_SCANNED_BIT(arglist) \
  do {                               \
  } while (0)
#endif

#define DBUG_PAGE_MAP 0

//
// PageMap is a service used by Dbtup to map logical page id's to physical
// page id's. The mapping is needs the fragment and the logical page id to
// provide the physical id.
//
// This is a part of Dbtup which is the exclusive user of a certain set of
// variables on the fragment record and it is the exclusive user of the
// struct for page ranges.
//
// The use of the fragment page map is described in some detail in Backup.cpp
// as part of the LCP description. We use 2 bits for important state info on
// this and the previous LCP state for a page.
//
// The following methods operate on the data handled by the page map class.
//
// Public methods
// insertPageRange(Uint32 startPageId,     # In
//                 Uint32 noPages)         # In
// Inserts a range of pages into the mapping structure.
//
// void releaseFragPage()
// Releases a page belonging to a fragment.
//
// Uint32 allocFragPages(Uint32 tafpNoAllocRequested)
// Allocate a set of pages to the fragment from the page manager
//
// Uint32 getEmptyPage()
// Get an empty page from the pool of empty pages on the fragment.
// It returns the physical page id of the empty page.
// Returns RNIL if no empty page is available.
//
// Uint32 getRealpid(Uint32 logicalPageId)
// Return the physical page id provided the logical page id
//
// void initializePageRange()
// Initialise free list of page ranges and initialise the page raneg records.
//
// void initFragRange()
// Initialise the fragment variables when allocating a fragment to a table.
//
// void initPageRangeSize(Uint32 size)
// Initialise the number of page ranges.
//
// Uint32 getNoOfPages()
// Get the number of pages on the fragment currently.
//
//
// Private methods
// Uint32 leafPageRangeFull(PageRangePtr currPageRangePtr)
//
// void errorHandler()
// Method to crash NDB kernel in case of weird data set-up
//
// void allocMoreFragPages()
// When no more empty pages are attached to the fragment and we need more
// we allocate more pages from the page manager using this method.
//
// Private data
// On the fragment record
// currentPageRange    # The current page range where to insert the next range
// rootPageRange       # The root of the page ranges owned
// nextStartRange      # The next page id to assign when expanding the
//                     # page map
// noOfPages           # The number of pages in the fragment
// emptyPrimPage       # The first page of the empty pages in the fragment
//
// The full page range struct

/* Can be used from multi-threaded index build, cannot use jam's */
Uint32 *Dbtup::init_page_map_entry(Fragrecord *regFragPtr,
                                   Uint32 logicalPageId) {
  DEB_LCP(("(%u)init_page_map_entry tab(%u,%u):%u", instance(),
           regFragPtr->fragTableId, regFragPtr->fragmentId, logicalPageId));
  DynArr256 map(c_page_map_pool_ptr, regFragPtr->m_page_map);
  Uint32 *prev_ptr = map.set(2 * logicalPageId + 1);
  if (prev_ptr == 0) {
    return 0;
  }
  Uint32 *ptr = map.set(2 * logicalPageId);
  if (ptr == 0) {
    (*prev_ptr) = FREE_PAGE_BIT | LAST_LCP_FREE_BIT;
    return 0;
  }
  if (logicalPageId >= regFragPtr->m_max_page_cnt) {
    regFragPtr->m_max_page_cnt = logicalPageId + 1;
    if (DBUG_PAGE_MAP) {
      g_eventLogger->info("(%u)allocIP: tab(%u,%u), new max: %u", instance(),
                          regFragPtr->fragTableId, regFragPtr->fragmentId,
                          regFragPtr->m_max_page_cnt);
    }
  }
  (void)insert_free_page_id_list(regFragPtr, logicalPageId, ptr, prev_ptr,
                                 Uint32(0), Uint32(0));
  return map.get_dirty(2 * logicalPageId);
}

Uint32 Dbtup::getRealpid(Fragrecord *regFragPtr, Uint32 logicalPageId) {
  DynArr256 map(c_page_map_pool_ptr, regFragPtr->m_page_map);
  Uint32 *ptr = map.get(2 * logicalPageId);
  if (likely(ptr != 0)) {
    ndbrequire((*ptr) != RNIL) return ((*ptr) & PAGE_BIT_MASK);
  }
  ndbabort();
  return RNIL;
}

/* Can be used from multi-threaded index build, cannot use jam's */
Uint32 Dbtup::getRealpidCheck(Fragrecord *regFragPtr, Uint32 logicalPageId) {
  /**
   * Jam cannot be used here since this code is called for Index build
   * which doesn't use fully initialised block objects.
   */
  DynArr256 map(c_page_map_pool_ptr, regFragPtr->m_page_map);
  // logicalPageId might not be mapped yet,
  // get_dirty returns NULL also in debug in this case.
  Uint32 *ptr = map.get_dirty(2 * logicalPageId);
  if (ptr == 0) {
    /**
     * No need to initialise page map entries during checks.
     * Checks are used during index builds and during copy
     * fragment process. Only LCP scans require page maps to
     * to exist for all pages, also non-existing pages. This
     * will be handled in getRealpidScan.
     */
    return RNIL;
  }
  if (likely(ptr != 0)) {
    Uint32 val = *ptr;
    if ((val & FREE_PAGE_BIT) != 0)
      return RNIL;
    else
      return (val & PAGE_BIT_MASK);
  }
  return RNIL;
}

Uint32 Dbtup::getNoOfPages(Fragrecord *const regFragPtr) {
  return regFragPtr->noOfPages;
}  // Dbtup::getNoOfPages()

void Dbtup::init_page(Fragrecord *regFragPtr, PagePtr pagePtr, Uint32 pageId) {
  pagePtr.p->page_state = ~0;
  pagePtr.p->frag_page_id = pageId;
  pagePtr.p->physical_page_id = pagePtr.i;
  pagePtr.p->nextList = RNIL;
  pagePtr.p->prevList = RNIL;
  pagePtr.p->m_flags = 0;
  Tup_fixsize_page *fix_page = (Tup_fixsize_page *)pagePtr.p;
  /**
   * A new page is required to be fully scanned the first LCP after
   * allocation to ensure that we generate DELETE BY ROWID for all
   * positions that are not yet inserted into, this ensures that
   * we don't leave deleted rows in change pages after an LCP.
   */
  fix_page->set_all_change_map();
  fix_page->clear_max_gci();
  ndbassert(fix_page->verify_change_maps(jamBuffer()));
}

#ifdef VM_TRACE
#define do_check_page_map(x) check_page_map(x)
#if DBUG_PAGE_MAP
bool Dbtup::find_page_id_in_list(Fragrecord *fragPtrP, Uint32 pageId) {
  /* Don't use jam's here unless a jamBuf is sent in */
  DynArr256 map(c_page_map_pool_ptr, fragPtrP->m_page_map);

  Uint32 prev = FREE_PAGE_RNIL;
  Uint32 curr = fragPtrP->m_free_page_id_list;

  while (curr != FREE_PAGE_RNIL) {
    const Uint32 *prevPtr = map.get(2 * curr + 1);
    ndbrequire(prevPtr != 0);
    ndbrequire(prev == ((*prevPtr) & PAGE_BIT_MASK));
    ndbrequire(((*prevPtr) & FREE_PAGE_BIT) == FREE_PAGE_BIT);

    Uint32 *nextPtr = map.get(2 * curr);
    ndbrequire(nextPtr != 0);
    ndbrequire(((*nextPtr) & FREE_PAGE_BIT) == FREE_PAGE_BIT);

    if (curr == pageId) return true;

    prev = curr;
    curr = (*nextPtr);
    curr &= PAGE_BIT_MASK;
  }
  return false;
}

void Dbtup::check_page_map(Fragrecord *fragPtrP) {
  if (m_is_query_block) {
    return;
  }
  /* Don't use jam's here unless a jamBuf is sent in */
  Uint32 max = fragPtrP->m_max_page_cnt;
  DynArr256 map(c_page_map_pool_ptr, fragPtrP->m_page_map);

  for (Uint32 i = 0; i < max; i++) {
    const Uint32 *ptr = map.get(2 * i);
    if (ptr == 0) {
      ndbrequire(find_page_id_in_list(fragPtrP, i) == false);
    } else {
      if ((*ptr) == RNIL) {
        ndbrequire(find_page_id_in_list(fragPtrP, i) == false);
      } else {
        Uint32 realpid = ((*ptr) & (Uint32)~LCP_SCANNED_BIT);
        if (realpid & FREE_PAGE_BIT) {
          ndbrequire(find_page_id_in_list(fragPtrP, i) == true);
        } else {
          PagePtr pagePtr;
          c_page_pool.getPtr(pagePtr, realpid);
          ndbrequire(pagePtr.p->frag_page_id == i);
          ndbrequire(pagePtr.p->physical_page_id == realpid);
          ndbrequire(find_page_id_in_list(fragPtrP, i) == false);
        }
      }
    }
  }
}
#else
void Dbtup::check_page_map(Fragrecord *) {}
#endif
#else
#define do_check_page_map(x)
#endif

Uint32 Dbtup::getRealpidScan(Fragrecord *regFragPtr, Uint32 logicalPageId,
                             Uint32 **next_ptr, Uint32 **prev_ptr) {
  DynArr256 map(c_page_map_pool_ptr, regFragPtr->m_page_map);
  Uint32 *ptr = map.get_dirty(2 * logicalPageId);
  if (ptr == 0 || (*ptr) == RNIL) {
    jam();
    /**
     * init_page_map_entry will update the Page map of the fragment.
     * This requires exclusive access to the fragment. We will ensure
     * that we have this exclusive access before we proceed using the
     * mutex protecting the page map.
     */
    acquire_frag_page_map_mutex(regFragPtr);
    ptr = init_page_map_entry(regFragPtr, logicalPageId);
    release_frag_page_map_mutex(regFragPtr);
    if (ptr == 0) {
      /**
       * This logical page id doesn't have any reference at all in the page
       * map. This means that it cannot have been used since the data node
       * was started or since the fragment was created. So it can definitely
       * not have any LCP_SCANNED_BIT set since this only happens when a
       * page is being dropped, to be dropped a page has to be mapped and once
       * it is mapped the map isn't removed.
       */
      jam();
      *next_ptr = *prev_ptr = 0;
      return RNIL;
    }
  }
  ndbrequire(ptr != 0);
  *next_ptr = ptr;
  *prev_ptr = map.get_dirty(2 * logicalPageId + 1);
  Uint32 val = *ptr;
  ndbassert(val != RNIL);
  if ((val & FREE_PAGE_BIT) != 0) {
    jam();
    return RNIL;
  } else {
    jam();
    return (val & PAGE_BIT_MASK);
  }
}

void Dbtup::set_last_lcp_state(Fragrecord *regFragPtr, Uint32 logicalPageId,
                               bool is_new_state_D) {
  ndbassert(!m_is_query_block);
  DynArr256 map(c_page_map_pool_ptr, regFragPtr->m_page_map);
  Uint32 *ptr = map.set(2 * logicalPageId + 1);
  ndbrequire(ptr != (Uint32 *)0);
  ndbassert((*ptr) != RNIL);
  set_last_lcp_state(ptr, is_new_state_D);
  do_check_page_map(regFragPtr);
}

void Dbtup::set_last_lcp_state(Uint32 *ptr, bool is_new_state_D) {
  if (unlikely(ptr == 0)) {
    jam();
    return;
  }
  Uint32 val = *ptr;
  ndbassert((val & FREE_PAGE_BIT) == FREE_PAGE_BIT);
  Uint32 new_last_lcp_state = is_new_state_D ? LAST_LCP_FREE_BIT : 0;
  val &= (Uint32)~LAST_LCP_FREE_BIT;
  val |= new_last_lcp_state;
  *ptr = val;
}

bool Dbtup::get_lcp_scanned_bit(Uint32 *next_ptr) {
  if (next_ptr == 0) {
    jam();
    return true;
  }
  if (((*next_ptr) & LCP_SCANNED_BIT) != 0) {
    jam();
    return true;
  }
  jam();
  return false;
}

bool Dbtup::get_lcp_scanned_bit(Fragrecord *regFragPtr, Uint32 logicalPageId) {
  ndbassert(!m_is_query_block);
  DynArr256 map(c_page_map_pool_ptr, regFragPtr->m_page_map);
  Uint32 *ptr = map.set(2 * logicalPageId);
  return get_lcp_scanned_bit(ptr);
}

/**
 * Currently not used code, can be activated when we can decrease
 * m_max_page_cnt.
 *
 *void
 *Dbtup::reset_lcp_scanned_bit(Fragrecord *regFragPtr, Uint32 logicalPageId)
 *{
 *  DynArr256 map(c_page_map_pool_ptr, regFragPtr->m_page_map);
 *  Uint32 *ptr = map.set(2 * logicalPageId);
 *  ndbassert(ptr != 0);
 *  ndbassert((*ptr) != RNIL);
 *#ifdef DEBUG_LCP_SCANNED_BIT
 *  if ((*ptr) & LCP_SCANNED_BIT)
 *  {
 *    g_eventLogger->info("(%u)tab(%u,%u):%u reset_lcp_scanned_bit",
 *      instance(),
 *      regFragPtr->fragTableId,
 *      regFragPtr->fragmentId,
 *      logicalPageId);
 *  }
 *#endif
 *  *ptr = (*ptr) & (Uint32)~LCP_SCANNED_BIT;
 *  do_check_page_map(regFragPtr);
 *}
 */

void Dbtup::reset_lcp_scanned_bit(Uint32 *next_ptr) {
  if (next_ptr == 0) {
    jam();
    return;
  }
  *next_ptr = (*next_ptr) & (Uint32)~LCP_SCANNED_BIT;
}

bool Dbtup::get_last_lcp_state(Uint32 *prev_ptr) {
  if (prev_ptr == 0) {
    jam();
    /**
     * If getRealpidScan returned a NULL pointer then the page
     * definitely didn't exist at the last LCP.
     */
    return true;
  }
  if (((*prev_ptr) & LAST_LCP_FREE_BIT) != 0) {
    jam();
    return true;
  } else {
    jam();
    return false;
  }
}

Uint32 Dbtup::insert_new_page_into_page_map(EmulatedJamBuffer *jamBuf,
                                            Fragrecord *regFragPtr,
                                            PagePtr pagePtr,
                                            Uint32 noOfPagesAllocated) {
  DynArr256 map(c_page_map_pool_ptr, regFragPtr->m_page_map);
  Uint32 pageId = regFragPtr->m_max_page_cnt;
  Uint32 *ptr;
  Uint32 *prev_ptr = 0;
  if (pageId >= MAX_PAGES_IN_DYN_ARRAY ||
      ((prev_ptr = map.set(2 * pageId + 1)) == 0) ||
      ((ptr = map.set(2 * pageId)) == 0)) {
    thrjam(jamBuf);
    if (prev_ptr != 0) {
      jam();
      *prev_ptr = FREE_PAGE_BIT | LAST_LCP_FREE_BIT;
    }
    returnCommonArea(pagePtr.i, noOfPagesAllocated);
    return RNIL;
  }
  /**
   * This should always get a new entry and this always is set initialised
   * to RNIL.
   */
  ndbrequire(*ptr == RNIL);
  *ptr = pagePtr.i;
  /* Ensure LAST_LCP_FREE_BIT is initialised to 1 */
  *prev_ptr = FREE_PAGE_BIT | LAST_LCP_FREE_BIT;
  regFragPtr->m_max_page_cnt = pageId + 1;
  if (DBUG_PAGE_MAP) {
    g_eventLogger->info("(%u)tab(%u,%u), new maxII: %u", instance(),
                        regFragPtr->fragTableId, regFragPtr->fragmentId,
                        regFragPtr->m_max_page_cnt);
  }
  return pageId;
}

Uint32 Dbtup::remove_first_free_from_page_map(EmulatedJamBuffer *jamBuf,
                                              Fragrecord *regFragPtr,
                                              PagePtr pagePtr) {
  Uint32 pageId = regFragPtr->m_free_page_id_list;
  DynArr256 map(c_page_map_pool_ptr, regFragPtr->m_page_map);
  Uint32 *ptr = map.set(2 * pageId);
  ndbrequire(ptr != 0);
  ndbassert((*ptr) != RNIL);
  Uint32 ptr_val = *ptr;
  ndbrequire((ptr_val & FREE_PAGE_BIT) != 0);
  Uint32 lcp_scanned_bit = ptr_val & LCP_SCANNED_BIT;
  Uint32 next = ptr_val & PAGE_BIT_MASK;
  *ptr = (pagePtr.i | lcp_scanned_bit);

#ifdef DEBUG_LCP_SCANNED_BIT
  if (lcp_scanned_bit) {
    g_eventLogger->info("(%u)tab(%u,%u):%u remove_first_free_from_page_map",
                        instance(), regFragPtr->fragTableId,
                        regFragPtr->fragmentId, pageId);
  }
#endif

  if (next != FREE_PAGE_RNIL) {
    thrjam(jamBuf);
    Uint32 *nextPrevPtr = map.set(2 * next + 1);
    ndbrequire(nextPrevPtr != 0);
    ndbassert((*nextPrevPtr) != RNIL);
    ndbassert(((*nextPrevPtr) & FREE_PAGE_BIT) == FREE_PAGE_BIT);
    Uint32 last_lcp_free_bit = (*nextPrevPtr) & LAST_LCP_FREE_BIT;
    *nextPrevPtr = FREE_PAGE_RNIL | FREE_PAGE_BIT | last_lcp_free_bit;
  }
  regFragPtr->m_free_page_id_list = next;
  DEB_LCP_FREE(("(%u)m_free_page_id_list(1), tab(%u,%u):%u", instance(),
                regFragPtr->fragTableId, regFragPtr->fragmentId, next));
  return pageId;
}

void Dbtup::remove_page_id_from_dll(Fragrecord *fragPtrP, Uint32 page_no,
                                    Uint32 pagePtrI, Uint32 *ptr) {
  DynArr256 map(c_page_map_pool_ptr, fragPtrP->m_page_map);
  const Uint32 *prevPtr = map.set(2 * page_no + 1);
  ndbrequire(prevPtr != 0);
  ndbassert((*prevPtr) != RNIL);
  ndbassert(((*prevPtr) & FREE_PAGE_BIT) == FREE_PAGE_BIT);
  Uint32 next = *ptr;
  Uint32 prev = *prevPtr;
  {
    /**
     * Set new entry in DynArray before list manipulations, ensure that
     * we don't forget the LCP_SCANNED_BIT.
     */
    Uint32 lcp_scanned_bit = next & LCP_SCANNED_BIT;
    *ptr = pagePtrI | lcp_scanned_bit;
#ifdef DEBUG_LCP_SCANNED_BIT
    if (lcp_scanned_bit) {
      g_eventLogger->info("(%u)tab(%u,%u):%u remove_page_id_from_dll",
                          instance(), fragPtrP->fragTableId,
                          fragPtrP->fragmentId, page_no);
    }
#endif
  }
  next &= PAGE_BIT_MASK;
  prev &= PAGE_BIT_MASK;
  if (next == FREE_PAGE_RNIL) {
    jam();
    // This should be end of list...
    if (prev == FREE_PAGE_RNIL) {
      jam();
      /* page_no is both head and tail */
      if (fragPtrP->m_free_page_id_list != page_no) {
        g_eventLogger->info(
            "(%u)m_free_page_id_list = %u,"
            " tab(%u,%u):%u",
            instance(), fragPtrP->m_free_page_id_list, fragPtrP->fragTableId,
            fragPtrP->fragmentId, page_no);
        ndbrequire(fragPtrP->m_free_page_id_list == page_no);
      }
      fragPtrP->m_free_page_id_list = FREE_PAGE_RNIL;
      DEB_LCP_FREE(("(%u)m_free_page_id_list(2), tab(%u,%u):FREE_PAGE_RNIL",
                    instance(), fragPtrP->fragTableId, fragPtrP->fragmentId));
    } else {
      jam();
      /* page_no is tail, but not head */
      Uint32 *prevNextPtr = map.set(2 * prev);
      ndbrequire(prevNextPtr != 0);
      ndbassert((*prevNextPtr) != RNIL);
      Uint32 prevNext = *prevNextPtr;
      ndbrequire(prevNext & FREE_PAGE_BIT);
      Uint32 lcp_scanned_bit = prevNext & LCP_SCANNED_BIT;
      ndbrequire((prevNext & PAGE_BIT_MASK) == page_no);
      *prevNextPtr = FREE_PAGE_RNIL | FREE_PAGE_BIT | lcp_scanned_bit;
    }
  } else {
    jam();
    Uint32 *nextPrevPtr = map.set(2 * next + 1);
    ndbrequire(nextPrevPtr != 0);
    ndbassert((*nextPrevPtr) != RNIL);
    ndbassert(((*nextPrevPtr) & FREE_PAGE_BIT) == FREE_PAGE_BIT);
    Uint32 nextPrev = (*nextPrevPtr) & PAGE_BIT_MASK;
    Uint32 last_lcp_free_bit = (*nextPrevPtr) & LAST_LCP_FREE_BIT;
    ndbrequire(nextPrev == page_no);
    *nextPrevPtr = prev | last_lcp_free_bit | FREE_PAGE_BIT;
    if (prev == FREE_PAGE_RNIL) {
      jam();
      /* page_no is head but not tail */
      ndbrequire(fragPtrP->m_free_page_id_list == page_no);
      fragPtrP->m_free_page_id_list = next;
      DEB_LCP_FREE(("(%u)m_free_page_id_list(3), tab(%u,%u):%u", instance(),
                    fragPtrP->fragTableId, fragPtrP->fragmentId, next));
    } else {
      jam();
      /* page_no is neither head nor tail */
      Uint32 *prevNextPtr = map.get(2 * prev);
      ndbrequire(prevNextPtr != 0);
      Uint32 prevNext = *prevNextPtr;
      Uint32 lcp_scanned_bit = prevNext & LCP_SCANNED_BIT;
      ndbrequire(prevNext & FREE_PAGE_BIT);
      prevNext &= PAGE_BIT_MASK;
      ndbrequire(prevNext == page_no);
      *prevNextPtr = next | FREE_PAGE_BIT | lcp_scanned_bit;
    }
  }
}

void Dbtup::handle_lcp_skip_bit(EmulatedJamBuffer *jamBuf, Fragrecord *fragPtrP,
                                PagePtr pagePtr, Uint32 page_no) {
  Uint32 lcp_scan_ptr_i = fragPtrP->m_lcp_scan_op;
  if (lcp_scan_ptr_i != RNIL) {
    thrjam(jamBuf);
    DynArr256 map(c_page_map_pool_ptr, fragPtrP->m_page_map);
    const Uint32 *ptr = map.set(2 * page_no);
    ndbrequire(ptr != 0);
    ndbassert((*ptr) != RNIL);
    Uint32 lcp_scanned_bit = (*ptr) & LCP_SCANNED_BIT;
    ScanOpPtr scanOp;
    scanOp.i = lcp_scan_ptr_i;
    ndbrequire(c_scanOpPool.getValidPtr(scanOp));
    Local_key key;
    key.m_page_no = page_no;
    key.m_page_idx = ZNIL;
    if (is_rowid_in_remaining_lcp_set(pagePtr.p, fragPtrP, key, *scanOp.p,
                                      2 /* Debug for LCP skip bit */)) {
      thrjam(jamBuf);
      if (lcp_scanned_bit == 0) {
        thrjam(jamBuf);
        /**
         * We allocated a page during an LCP, it was within the pages that
         * will be checked during the LCP scan. The page has also not yet
         * been scanned by the LCP. Given that we know that the page will
         * only contain rows that would set the LCP_SKIP bit we will
         * set the LCP skip on the page level instead to speed up LCP
         * processing.
         *
         * We use this bit both for ALL ROWS pages and CHANGED ROWS pages.
         * When we come to the scanning of this page we will decide what
         * to do with the page whether to skip or record it as DELETE by
         * PAGEID.
         */
        /* Coverage tested */
        DEB_LCP_SKIP(("(%u)LCP_SKIP in tab(%u,%u):%u", instance(),
                      fragPtrP->fragTableId, fragPtrP->fragmentId, page_no));
        pagePtr.p->set_page_to_skip_lcp();
        c_backup->alloc_page_after_lcp_start(page_no);
      } else {
        jam();
        /* Coverage tested */
        /**
         * The page had already been handled since it had been dropped
         * after LCP start and is now allocated again still before the
         * LCP scan reached it. No need to do anything since its LCP
         * scanning was handled at drop time.
         */
      }
    } else {
      if (lcp_scanned_bit) {
        g_eventLogger->info("(%u):lcp_scanned_bit crash on tab(%u,%u):%u",
                            instance(), fragPtrP->fragTableId,
                            fragPtrP->fragmentId, page_no);
      }
      ndbrequire(lcp_scanned_bit == 0);
    }
  }
}

void Dbtup::handle_new_page(EmulatedJamBuffer *jamBuf, Fragrecord *fragPtrP,
                            Tablerec *tabPtrP, PagePtr pagePtr,
                            Uint32 page_no) {
  DEB_LCP_ALLOC(("(%u)allocFragPage: tab(%u,%u) page(%u)", instance(),
                 fragPtrP->fragTableId, fragPtrP->fragmentId, page_no));
  c_page_pool.getPtr(pagePtr);
  init_page(fragPtrP, pagePtr, page_no);
  handle_lcp_skip_bit(jamBuf, fragPtrP, pagePtr, page_no);
  convertThPage((Fix_page *)pagePtr.p, tabPtrP, MM);
  {
    LocalDLFifoList<Page_pool> free_pages(c_page_pool, fragPtrP->thFreeFirst);
    pagePtr.p->page_state = ZTH_MM_FREE;
    free_pages.addFirst(pagePtr);
  }
  if (DBUG_PAGE_MAP) {
    g_eventLogger->info("(%u)tab(%u,%u):%u alloc -> (%u max: %u)", instance(),
                        fragPtrP->fragTableId, fragPtrP->fragmentId, page_no,
                        pagePtr.i, fragPtrP->m_max_page_cnt);
  }

  do_check_page_map(fragPtrP);
}

Uint32 Dbtup::allocFragPage(EmulatedJamBuffer *jamBuf, Uint32 *err,
                            Fragrecord *regFragPtr, Tablerec *regTabPtr) {
  PagePtr pagePtr;
  Uint32 noOfPagesAllocated = 0;
  Uint32 list = regFragPtr->m_free_page_id_list;

  allocConsPages(jamBuf, 1, noOfPagesAllocated, pagePtr.i);
  if (noOfPagesAllocated == 0) {
    thrjam(jamBuf);
    *err = ZMEM_NOMEM_ERROR;
    return RNIL;
  }  // if

  Uint32 pageId;
  if (list == FREE_PAGE_RNIL) {
    thrjam(jamBuf);
    Uint32 max_page_cnt = regFragPtr->m_max_page_cnt;
    acquire_frag_page_map_mutex(regFragPtr);
    acquire_frag_mutex(regFragPtr, max_page_cnt);
    pageId = insert_new_page_into_page_map(jamBuf, regFragPtr, pagePtr,
                                           noOfPagesAllocated);
    DEB_LCP(("(%u)allocFragPage(1): tab(%u,%u):%u", instance(),
             regFragPtr->fragTableId, regFragPtr->fragmentId, pageId));
    if (pageId == RNIL) {
      release_frag_page_map_mutex(regFragPtr);
      release_frag_mutex(regFragPtr, max_page_cnt);
      thrjam(jamBuf);
      *err = ZMEM_NOMEM_ERROR;
      return RNIL;
    }
  } else {
    thrjam(jamBuf);
    acquire_frag_page_map_mutex(regFragPtr);
    acquire_frag_mutex(regFragPtr, regFragPtr->m_free_page_id_list);
    pageId = remove_first_free_from_page_map(jamBuf, regFragPtr, pagePtr);
    DEB_LCP(("(%u)allocFragPage(2): tab(%u,%u):%u", instance(),
             regFragPtr->fragTableId, regFragPtr->fragmentId, pageId));
  }
  if (DBUG_PAGE_MAP) {
    DynArr256 map(c_page_map_pool_ptr, regFragPtr->m_page_map);
    Uint32 *ptr = map.set(2 * pageId);
    ndbrequire(ptr != 0);
    ndbassert((*ptr) != RNIL);
    g_eventLogger->info("(%u)tab(%u,%u) allocRI(%u %u max: %u next: %x)",
                        instance(), regFragPtr->fragTableId,
                        regFragPtr->fragmentId, pageId, pagePtr.i,
                        regFragPtr->m_max_page_cnt, *ptr);
  }
  regFragPtr->noOfPages++;
  handle_new_page(jamBuf, regFragPtr, regTabPtr, pagePtr, pageId);
  release_frag_page_map_mutex(regFragPtr);
  release_frag_mutex(regFragPtr, pageId);
  return pagePtr.i;
}  // Dbtup::allocFragPage()

Uint32 Dbtup::allocFragPage(Uint32 *err, Tablerec *tabPtrP,
                            Fragrecord *fragPtrP, Uint32 page_no) {
  PagePtr pagePtr;
  ndbrequire(page_no < MAX_PAGES_IN_DYN_ARRAY);
  DynArr256 map(c_page_map_pool_ptr, fragPtrP->m_page_map);
  DEB_LCP(("(%u)allocFragPage(3): tab(%u,%u):%u", instance(),
           fragPtrP->fragTableId, fragPtrP->fragmentId, page_no));
  acquire_frag_page_map_mutex(fragPtrP);
  acquire_frag_mutex(fragPtrP, page_no);
  Uint32 *prev_ptr = map.set(2 * page_no + 1);
  if (unlikely(prev_ptr == 0)) {
    release_frag_page_map_mutex(fragPtrP);
    release_frag_mutex(fragPtrP, page_no);
    jam();
    *err = ZMEM_NOMEM_ERROR;
    return RNIL;
  }
  Uint32 *ptr = map.set(2 * page_no);
  if (unlikely(ptr == 0)) {
    release_frag_page_map_mutex(fragPtrP);
    release_frag_mutex(fragPtrP, page_no);
    jam();
    *prev_ptr = FREE_PAGE_RNIL | LAST_LCP_FREE_BIT;
    *err = ZMEM_NOMEM_ERROR;
    return RNIL;
  }
  pagePtr.i = *ptr;
  if (likely(pagePtr.i != RNIL && (pagePtr.i & FREE_PAGE_BIT) == 0)) {
    release_frag_page_map_mutex(fragPtrP);
    release_frag_mutex(fragPtrP, page_no);
    jam();
    return (pagePtr.i & PAGE_BIT_MASK);
  }

  Uint32 noOfPagesAllocated = 0;
  allocConsPages(jamBuffer(), 1, noOfPagesAllocated, pagePtr.i);
  if (unlikely(noOfPagesAllocated == 0)) {
    release_frag_page_map_mutex(fragPtrP);
    release_frag_mutex(fragPtrP, page_no);
    jam();
    *err = ZMEM_NOMEM_ERROR;
    return RNIL;
  }

  if ((*ptr) == RNIL) {
    /**
     * DynArr256 delivered a fresh new entry, so no flags are initialised
     * and we will treat it as if it returned FREE_PAGE_BIT set,
     * LCP_SCANNED_BIT not set, but also not in any free list, so no need
     * to remove it from the doubly linked list. We will make use of entry
     * for a new page, so we don't set the FREE_PAGE_BIT either, we simply
     * insert the page pointer.
     *
     * Since it is the first occurrence of the page we initialise that the
     * page was free at the last LCP. We always need to set the FREE_PAGE_BIT
     * also to ensure that drop fragment doesn't drop things from the
     * prev_ptr position.
     */
    jam();
    *ptr = pagePtr.i;
    *prev_ptr = FREE_PAGE_BIT | LAST_LCP_FREE_BIT;
  } else {
    jam();
    /**
     * This page id was in the doubly linked list free list, we need to remove
     * it from this list.
     */
    remove_page_id_from_dll(fragPtrP, page_no, pagePtr.i, ptr);
  }
  if (DBUG_PAGE_MAP) {
    g_eventLogger->info("(%u)tab(%u,%u):%u alloc(%u max: %u next: %x)",
                        instance(), fragPtrP->fragTableId, fragPtrP->fragmentId,
                        page_no, pagePtr.i, fragPtrP->m_max_page_cnt, *ptr);
  }
  Uint32 max = fragPtrP->m_max_page_cnt;
  fragPtrP->noOfPages++;

  if (page_no + 1 > max) {
    jam();
    fragPtrP->m_max_page_cnt = page_no + 1;
    if (DBUG_PAGE_MAP) {
      g_eventLogger->info("(%u)tab(%u,%u) new max: %u", instance(),
                          fragPtrP->fragTableId, fragPtrP->fragmentId,
                          fragPtrP->m_max_page_cnt);
    }
  }
  handle_new_page(jamBuffer(), fragPtrP, tabPtrP, pagePtr, page_no);
  release_frag_page_map_mutex(fragPtrP);
  release_frag_mutex(fragPtrP, page_no);
  return pagePtr.i;
}

void Dbtup::releaseFragPage(Fragrecord *fragPtrP, Uint32 logicalPageId,
                            PagePtr pagePtr) {
  /**
   * This call is done under exclusive fragment access
   * This means no TUP fragment mutexes are acquired since no
   * query thread are allowed to execute on the fragment while
   * we are in exclusive mode.
   */
  DynArr256 map(c_page_map_pool_ptr, fragPtrP->m_page_map);
  DEB_LCP_REL(("(%u)releaseFragPage: tab(%u,%u) page(%u)", instance(),
               fragPtrP->fragTableId, fragPtrP->fragmentId, logicalPageId));
  Uint32 *next = map.set(2 * logicalPageId);
  Uint32 *prev = map.set(2 * logicalPageId + 1);
  ndbrequire(next != 0 && prev != 0);
  ndbassert(((*prev) & FREE_PAGE_BIT) == FREE_PAGE_BIT);

  bool page_freed = false;
  Uint32 lcp_scanned_bit = (*next) & LCP_SCANNED_BIT;
  Uint32 last_lcp_state = (*prev) & LAST_LCP_FREE_BIT;
  Uint32 lcp_scan_ptr_i = fragPtrP->m_lcp_scan_op;
  bool lcp_to_scan = false;
  bool rowid_in_remaining_lcp_set = false;
  if (lcp_scan_ptr_i != RNIL) {
    /**
     * We use the method is_rowid_in_remaining_lcp_set. We set the
     * key to the page and beyond the last row in the page. This means
     * that if the page is not fully scanned yet we will set the
     * LCP_SCANNED_BIT, in this case we might have already handled
     * some of the rows, so there is a small probability that we will
     * duplicate some DELETE BY ROWID, but it should only have a minor
     * performance impact. Otherwise we will ignore it.
     */
    jam();
    ScanOpPtr scanOp;
    Local_key key;
    scanOp.i = lcp_scan_ptr_i;
    ndbrequire(c_scanOpPool.getValidPtr(scanOp));
    key.m_page_no = logicalPageId;
    key.m_page_idx = ZNIL;
    rowid_in_remaining_lcp_set = is_rowid_in_remaining_lcp_set(
        pagePtr.p, fragPtrP, key, *scanOp.p, 1 /* Debug for LCP scanned bit */);
    if (rowid_in_remaining_lcp_set || pagePtr.p->is_page_to_skip_lcp()) {
      jam();
      lcp_to_scan = true;
      if (lcp_scanned_bit == 0) {
        jam();
        /**
         * Page is being dropped and it is part of LCP and has not
         * yet been scanned by LCP. This means we need to act right
         * now before we release the page and record the needed
         * information. Also we haven't already dropped the page
         * already before in this LCP scan.
         *
         * is_rowid_in_remaining_lcp_set is normally false when
         * is_page_to_skip_lcp is true. The problem however is that
         * this is a state on the page, and the page is being dropped,
         * so in this case we need to ensure that the page is not
         * containing any row at restore. We ensure this by using
         * a DELETE BY PAGEID in this case. We also flag that the
         * page have been scanned for LCP in the page map. It is
         * possible to arrive here after allocate, drop, allocate
         * and drop again. In this case the LCP scanned bit will
         * still be set and we can ignore the page.
         *
         * We will release the page during handle_lcp_drop_change
         * to ensure that we are certain to get the space we
         * need to store the space needed to store things into the
         * LCP keep list.
         *
         * If the SKIP_LCP flag was set on the page then this page
         * was added since the start of the LCP and in that case
         * we record the page as a DELETE by PAGEID and then sets
         * the LCP_SCANNED_BIT to ensure that any further allocation
         * and release of the fragment before LCP scan has passed it
         * is ignored.
         *
         * For ALL ROWS pages the LCP scan is always completed in
         * this state, all the rows existing at start of LCP has been
         * deleted and all of those were put into LCP through the LCP
         * keep list. So we ensure that the page is ignored for the
         * rest of the LCP scan by setting the LCP_SCANNED_BIT here
         * as well.
         *
         * No need to clear the page to skip lcp flag here since the
         * page is dropped immediately following this.
         *
         * If last page state was D the page will be empty at the
         * previous LCP, so this means that there is no need to
         * delete rows that won't be there. There is not really any
         * problem in performing deletes in this case, but it would
         * cause unnecessary work. The rows that was present in the
         * page at start of LCP have already been handled through
         * LCP keep list.
         *
         * If last state was A there was a set of rows installed by
         * the previous LCP, some of those rows will remain if we
         * don't ensure that they are removed. So in this case we
         * remove all rows on the page that hasn't got the LCP_SKIP
         * flag set. Those rows with this flag set have been handled
         * by the LCP keep list before arriving here.
         */
        bool is_change_part = c_backup->is_change_part_state(logicalPageId);
        DEB_LCP_SCANNED_BIT(("(%u)Set lcp_scanned_bit on tab(%u,%u):%u",
                             instance(), fragPtrP->fragTableId,
                             fragPtrP->fragmentId, logicalPageId));
        lcp_scanned_bit = LCP_SCANNED_BIT;
        bool page_to_skip_lcp = pagePtr.p->is_page_to_skip_lcp();
        Uint32 new_last_lcp_state;
        if (page_to_skip_lcp) {
          new_last_lcp_state = LAST_LCP_FREE_BIT;
          c_backup->alloc_dropped_page_after_lcp_start(is_change_part);
        } else {
          new_last_lcp_state = 0;
          c_backup->dropped_page_after_lcp_start(is_change_part,
                                                 (last_lcp_state == 0));
        }
        if (is_change_part && (last_lcp_state == 0)) {
          /**
           * Page is a change page and last LCP state was A.
           * We set page_freed to true, the reason is that we're
           * "stealing" the page to be deleted for use by the
           * LCP keep free list. This removes any possibility that
           * we will run out of memory for this operation.
           *
           * The page is removed later when the LCP keep list operation
           * is completed.
           */
          jam();
          /* Coverage tested */
          c_page_pool.getPtr(pagePtr);
          bool delete_by_pageid = page_to_skip_lcp;
          page_freed = true;
          ndbrequire(c_backup->is_partial_lcp_enabled());
          handle_lcp_drop_change_page(fragPtrP, logicalPageId, pagePtr,
                                      delete_by_pageid);
        } else {
          /* Coverage tested */
          DEB_LCP_REL(
              ("(%u) change_part: %u, last_lcp_state: %u "
               "in tab(%u,%u) page(%u)",
               instance(), is_change_part, last_lcp_state,
               fragPtrP->fragTableId, fragPtrP->fragmentId, logicalPageId));
        }
        last_lcp_state = new_last_lcp_state;
      } else {
        DEB_LCP_REL(
            ("(%u)lcp_scanned_bit already set when page released"
             "in tab(%u,%u) page(%u)",
             instance(), fragPtrP->fragTableId, fragPtrP->fragmentId,
             logicalPageId));
        /* Coverage tested */
      }
    } else {
      /* Coverage tested */
    }
  }

  if (!page_freed) {
    jam();
    returnCommonArea(pagePtr.i, 1);
  }

#ifdef DEBUG_LCP_SCANNED_BIT
  if (lcp_scanned_bit) {
    g_eventLogger->info("(%u)tab(%u,%u):%u set lcp_scanned_bit", instance(),
                        fragPtrP->fragTableId, fragPtrP->fragmentId,
                        logicalPageId);
  }
#endif

  const char *where = insert_free_page_id_list(
      fragPtrP, logicalPageId, next, prev, lcp_scanned_bit, last_lcp_state);
  fragPtrP->noOfPages--;
  if (DBUG_PAGE_MAP) {
    g_eventLogger->info("(%u)tab(%u,%u):%u release(%u)@%s", instance(),
                        fragPtrP->fragTableId, fragPtrP->fragmentId,
                        logicalPageId, pagePtr.i, where);
  }
  do_check_page_map(fragPtrP);
}

/* Can be used from multi-threaded index build, cannot use jam's */
const char *Dbtup::insert_free_page_id_list(Fragrecord *fragPtrP,
                                            Uint32 logicalPageId, Uint32 *next,
                                            Uint32 *prev,
                                            Uint32 lcp_scanned_bit,
                                            Uint32 last_lcp_state) {
  /**
   * Add to head or tail of list...
   */
  DynArr256 map(c_page_map_pool_ptr, fragPtrP->m_page_map);
  Uint32 list = fragPtrP->m_free_page_id_list;
  const char *where = 0;
  if (list == FREE_PAGE_RNIL) {
    *next = FREE_PAGE_RNIL | FREE_PAGE_BIT | lcp_scanned_bit;
    *prev = FREE_PAGE_RNIL | FREE_PAGE_BIT | last_lcp_state;
    fragPtrP->m_free_page_id_list = logicalPageId;
    DEB_LCP_FREE(("(%u)m_free_page_id_list(4), tab(%u,%u):%u", instance(),
                  fragPtrP->fragTableId, fragPtrP->fragmentId, logicalPageId));
    where = (const char *)"empty";
  } else {
    *next = list | FREE_PAGE_BIT | lcp_scanned_bit;
    *prev = FREE_PAGE_RNIL | FREE_PAGE_BIT | last_lcp_state;
    fragPtrP->m_free_page_id_list = logicalPageId;
    DEB_LCP_FREE(("(%u)m_free_page_id_list(5), tab(%u,%u):%u", instance(),
                  fragPtrP->fragTableId, fragPtrP->fragmentId, logicalPageId));
    Uint32 *nextPrevPtr = map.set(2 * list + 1);
    ndbrequire(nextPrevPtr != 0);
    ndbassert((*nextPrevPtr) != RNIL);
    Uint32 nextPrev = *nextPrevPtr;
    Uint32 this_last_lcp_state = nextPrev & LAST_LCP_FREE_BIT;
    ndbrequire(((*nextPrevPtr) & PAGE_BIT_MASK) == FREE_PAGE_RNIL);
    *nextPrevPtr = logicalPageId | FREE_PAGE_BIT | this_last_lcp_state;
    where = (const char *)"head";
  }
  return where;
}

void Dbtup::errorHandler(Uint32 errorCode) {
  switch (errorCode) {
    case 0:
      jam();
      break;
    case 1:
      jam();
      break;
    case 2:
      jam();
      break;
    default:
      jam();
  }
  ndbabort();
}  // Dbtup::errorHandler()

void Dbtup::rebuild_page_free_list(Signal *signal) {
  Ptr<Fragoperrec> fragOpPtr;
  fragOpPtr.i = signal->theData[1];
  Uint32 pageId = signal->theData[2];
  Uint32 tail = signal->theData[3];
  ptrCheckGuard(fragOpPtr, cnoOfFragoprec, fragoperrec);

  Ptr<Fragrecord> fragPtr;
  fragPtr.i = fragOpPtr.p->fragPointer;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  if (pageId == fragPtr.p->m_max_page_cnt) {
    do_check_page_map(fragPtr.p);
    RestoreLcpConf *conf = (RestoreLcpConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = fragOpPtr.p->m_senderData;
    conf->restoredLcpId = fragOpPtr.p->m_restoredLcpId;
    conf->restoredLocalLcpId = fragOpPtr.p->m_restoredLocalLcpId;
    conf->maxGciCompleted = fragOpPtr.p->m_maxGciCompleted;
    conf->afterRestore = 1;
    sendSignal(fragOpPtr.p->m_senderRef, GSN_RESTORE_LCP_CONF, signal,
               RestoreLcpConf::SignalLength, JBB);

    releaseFragoperrec(fragOpPtr);
    if (c_lqh->m_is_recover_block) {
      jam();
      /**
       * We will reset all pointers to the LDM instance to avoid
       * using those pointers when not supposed to.
       * This is a debugging tool to ensure that we quickly find
       * use of these variables that are outside of their definition.
       */
      c_lqh->reset_restore_thread_access();
    }
    return;
  }

  DynArr256 map(c_page_map_pool_ptr, fragPtr.p->m_page_map);
  Uint32 *nextPtr = map.set(2 * pageId);
  Uint32 *prevPtr = map.set(2 * pageId + 1);

  // Out of memory ?? Should not be possible here/now
  ndbrequire(nextPtr != 0 && prevPtr != 0);

  /**
   * This is called as part of restore, the pages that are defined here
   * should have the LAST_LCP_FREE_BIT initialised to 0, those that are
   * not yet allocated should have their state initialised to 1.
   *
   * LAST_LCP_FREE_BIT indicates state at last LCP, at restart the last
   * LCP is the restore and this has now been completed. So after this
   * we need to have a well defined state of LAST_LCP_FREE.
   * Later allocations of new page ids are always assumed to not be part
   * of the last LCP.
   *
   * Using Partial LCP the restore process can call releaseFragPage.
   * In this case *nextPtr isn't equal to RNIL, but FREE_PAGE_BIT of
   * *nextPagePtr is set instead. So need to check for either of those
   * two events.
   */
  if (*nextPtr == RNIL || ((((*nextPtr) & FREE_PAGE_BIT) != 0))) {
    jam();
    /**
     * An unallocated page id...put in free list
     */

#if DBUG_PAGE_MAP
    const char *where;
#endif
    if (tail == RNIL) {
      jam();
      ndbrequire(fragPtr.p->m_free_page_id_list == FREE_PAGE_RNIL);
      fragPtr.p->m_free_page_id_list = pageId;
      DEB_LCP_FREE(("(%u)m_free_page_id_list(6), tab(%u,%u):%u", instance(),
                    fragPtr.p->fragTableId, fragPtr.p->fragmentId, pageId));
      *nextPtr = FREE_PAGE_RNIL | FREE_PAGE_BIT;
      *prevPtr = FREE_PAGE_RNIL | FREE_PAGE_BIT | LAST_LCP_FREE_BIT;
#if DBUG_PAGE_MAP
      where = "head";
#endif
    } else {
      jam();
      ndbrequire(fragPtr.p->m_free_page_id_list != FREE_PAGE_RNIL);

      *nextPtr = FREE_PAGE_RNIL | FREE_PAGE_BIT;
      *prevPtr = tail | FREE_PAGE_BIT | LAST_LCP_FREE_BIT;

      Uint32 *prevNextPtr = map.set(2 * tail);
      ndbrequire(prevNextPtr != 0);
      ndbrequire((*prevNextPtr) == (FREE_PAGE_RNIL | FREE_PAGE_BIT));
      *prevNextPtr = pageId | FREE_PAGE_BIT;
#if DBUG_PAGE_MAP
      where = "tail";
#endif
      DEB_LCP_FREE(("(%u)tab(%u,%u):%u into free page id list", instance(),
                    fragPtr.p->fragTableId, fragPtr.p->fragmentId, pageId));
    }
    tail = pageId;
#if DBUG_PAGE_MAP
    g_eventLogger->info("(%u)tab(%u,%u):%u adding page to free list @ %s",
                        instance(), fragPtr.p->fragTableId,
                        fragPtr.p->fragmentId, pageId, where);
#endif
  } else {
    jam();
    /* Clear LAST_LCP_FREE_BIT and set FREE_PAGE_BIT */
    DEB_LCP_FREE(("(%u)tab(%u,%u):%u, next: %x, prev: %x", instance(),
                  fragPtr.p->fragTableId, fragPtr.p->fragmentId, pageId,
                  *nextPtr, *prevPtr));
    *prevPtr = (*prevPtr) & PAGE_BIT_MASK;
    *prevPtr = (*prevPtr) | FREE_PAGE_BIT;
  }

  signal->theData[0] = ZREBUILD_FREE_PAGE_LIST;
  signal->theData[1] = fragOpPtr.i;
  signal->theData[2] = pageId + 1;
  signal->theData[3] = tail;
  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
}
