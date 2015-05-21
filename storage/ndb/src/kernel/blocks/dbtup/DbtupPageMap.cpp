/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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


#define DBTUP_C
#define DBTUP_PAGE_MAP_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <signaldata/RestoreImpl.hpp>

#define JAM_FILE_ID 415


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
//
// The following methods operate on the data handled by the page map class.
//
// Public methods
// insertPageRange(Uint32 startPageId,     # In
//                 Uint32 noPages)         # In
// Inserts a range of pages into the mapping structure.
//
// void releaseFragPages()
// Releases all pages and their mappings belonging to a fragment.
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

Uint32 Dbtup::getRealpid(Fragrecord* regFragPtr, Uint32 logicalPageId) 
{
  DynArr256 map(c_page_map_pool, regFragPtr->m_page_map);
  Uint32 * ptr = map.get(2 * logicalPageId);
  if (likely(ptr != 0))
  {
    return * ptr;
  }
  ndbrequire(false);
  return RNIL;
}

Uint32 
Dbtup::getRealpidCheck(Fragrecord* regFragPtr, Uint32 logicalPageId) 
{
  DynArr256 map(c_page_map_pool, regFragPtr->m_page_map);
  // logicalPageId might not be mapped yet,
  // get_dirty returns NULL also in debug in this case.
  Uint32 * ptr = map.get_dirty(2 * logicalPageId);
  if (likely(ptr != 0))
  {
    Uint32 val = * ptr;
    if ((val & FREE_PAGE_BIT) != 0)
      return RNIL;
    else
      return val;
  }
  return RNIL;
}

Uint32 Dbtup::getNoOfPages(Fragrecord* const regFragPtr)
{
  return regFragPtr->noOfPages;
}//Dbtup::getNoOfPages()

void
Dbtup::init_page(Fragrecord* regFragPtr, PagePtr pagePtr, Uint32 pageId)
{
  pagePtr.p->page_state = ~0;
  pagePtr.p->frag_page_id = pageId;
  pagePtr.p->physical_page_id = pagePtr.i;
  pagePtr.p->nextList = RNIL;
  pagePtr.p->prevList = RNIL;
  pagePtr.p->m_flags = 0;
}

#ifdef VM_TRACE
#define do_check_page_map(x) check_page_map(x)
#if DBUG_PAGE_MAP
bool
Dbtup::find_page_id_in_list(Fragrecord* fragPtrP, Uint32 pageId)
{
  DynArr256 map(c_page_map_pool, fragPtrP->m_page_map);  

  Uint32 prev = FREE_PAGE_RNIL;
  Uint32 curr = fragPtrP->m_free_page_id_list | FREE_PAGE_BIT;
  
  while (curr != FREE_PAGE_RNIL)
  {
    ndbrequire((curr & FREE_PAGE_BIT) != 0);
    curr &= ~(Uint32)FREE_PAGE_BIT;
    const Uint32 * prevPtr = map.get(2 * curr + 1);
    ndbrequire(prevPtr != 0);
    ndbrequire(prev == *prevPtr);
    
    if (curr == pageId)
      return true;
    
    Uint32 * nextPtr = map.get(2 * curr);
    ndbrequire(nextPtr != 0);
    prev = curr | FREE_PAGE_BIT;
    curr = (* nextPtr);
  }
  
  return false;
}

void
Dbtup::check_page_map(Fragrecord* fragPtrP)
{
  Uint32 max = fragPtrP->m_max_page_cnt;
  DynArr256 map(c_page_map_pool, fragPtrP->m_page_map);

  for (Uint32 i = 0; i<max; i++)
  {
    const Uint32 * ptr = map.get(2*i);
    if (ptr == 0)
    {
      ndbrequire(find_page_id_in_list(fragPtrP, i) == false);
    }
    else
    {
      Uint32 realpid = *ptr;
      if (realpid == RNIL)
      {
        ndbrequire(find_page_id_in_list(fragPtrP, i) == false);      
      }
      else if (realpid & FREE_PAGE_BIT)
      {
        ndbrequire(find_page_id_in_list(fragPtrP, i) == true);
      }
      else
      {
        PagePtr pagePtr;
        c_page_pool.getPtr(pagePtr, realpid);
        ndbrequire(pagePtr.p->frag_page_id == i);
        ndbrequire(pagePtr.p->physical_page_id == realpid);
      }
    }
  }
}
#else
void Dbtup::check_page_map(Fragrecord*) {}
#endif
#else
#define do_check_page_map(x)
#endif

Uint32 
Dbtup::allocFragPage(EmulatedJamBuffer* jamBuf,
                     Uint32 * err, 
                     Fragrecord* regFragPtr)
{
  PagePtr pagePtr;
  Uint32 noOfPagesAllocated = 0;
  Uint32 list = regFragPtr->m_free_page_id_list;
  Uint32 max = regFragPtr->m_max_page_cnt;
  Uint32 cnt = regFragPtr->noOfPages;

  allocConsPages(jamBuf, 1, noOfPagesAllocated, pagePtr.i);
  if (noOfPagesAllocated == 0) 
  {
    thrjam(jamBuf);
    * err = ZMEM_NOMEM_ERROR;
    return RNIL;
  }//if
  
  Uint32 pageId;
  DynArr256 map(c_page_map_pool, regFragPtr->m_page_map);
  if (list == FREE_PAGE_RNIL)
  {
    thrjam(jamBuf);
    pageId = max;
    if (!Local_key::isShort(pageId))
    {
      /**
       * TODO: remove when ACC supports 48 bit references
       */
      thrjam(jamBuf);
      * err = 889;
      return RNIL;
    }
    Uint32 * ptr = map.set(2 * pageId);
    if (unlikely(ptr == 0))
    {
      thrjam(jamBuf);
      returnCommonArea(pagePtr.i, noOfPagesAllocated);
      * err = ZMEM_NOMEM_ERROR;
      return RNIL;
    }
    ndbrequire(* ptr == RNIL);
    * ptr = pagePtr.i;
    regFragPtr->m_max_page_cnt = max + 1;
  }
  else
  {
    thrjam(jamBuf);
    pageId = list;
    Uint32 * ptr = map.set(2 * pageId);
    ndbrequire(ptr != 0);
    Uint32 next = * ptr;
    * ptr = pagePtr.i;
    
    if (next != FREE_PAGE_RNIL)
    {
      thrjam(jamBuf);
      ndbrequire((next & FREE_PAGE_BIT) != 0);
      next &= ~FREE_PAGE_BIT;
      Uint32 * nextPrevPtr = map.set(2 * next + 1);
      ndbrequire(nextPrevPtr != 0);
      * nextPrevPtr = FREE_PAGE_RNIL;
    }
    regFragPtr->m_free_page_id_list = next; 
  }
  
  regFragPtr->noOfPages = cnt + 1;
  c_page_pool.getPtr(pagePtr);
  init_page(regFragPtr, pagePtr, pageId);
  
  if (DBUG_PAGE_MAP)
    ndbout_c("alloc -> (%u %u max: %u)", pageId, pagePtr.i, 
             regFragPtr->m_max_page_cnt);
  
  do_check_page_map(regFragPtr);
  return pagePtr.i;
}//Dbtup::allocFragPage()

Uint32
Dbtup::allocFragPage(Uint32 * err,
                     Tablerec* tabPtrP, Fragrecord* fragPtrP, Uint32 page_no)
{
  PagePtr pagePtr;
  DynArr256 map(c_page_map_pool, fragPtrP->m_page_map);
  Uint32 * ptr = map.set(2 * page_no);
  if (unlikely(ptr == 0))
  {
    jam();
    * err = ZMEM_NOMEM_ERROR;
    return RNIL;
  }
  const Uint32 * prevPtr = map.set(2 * page_no + 1);
  
  pagePtr.i = * ptr;
  if (likely(pagePtr.i != RNIL && (pagePtr.i & FREE_PAGE_BIT) == 0))
  {
    jam();
    return pagePtr.i;
  }
  
  LocalDLFifoList<Page> free_pages(c_page_pool, fragPtrP->thFreeFirst);
  Uint32 cnt = fragPtrP->noOfPages;
  Uint32 max = fragPtrP->m_max_page_cnt;
  Uint32 list = fragPtrP->m_free_page_id_list;
  Uint32 noOfPagesAllocated = 0;
  Uint32 next = pagePtr.i;

  allocConsPages(jamBuffer(), 1, noOfPagesAllocated, pagePtr.i);
  if (unlikely(noOfPagesAllocated == 0))
  {
    jam();
    * err = ZMEM_NOMEM_ERROR;
    return RNIL;
  }

  if (DBUG_PAGE_MAP)
    ndbout_c("alloc(%u %u max: %u)", page_no, pagePtr.i, max);
  
  * ptr = pagePtr.i;
  if (next == RNIL)
  {
    jam();
  }
  else
  {
    jam();
    ndbrequire(prevPtr != 0);
    Uint32 prev = * prevPtr;

    if (next == FREE_PAGE_RNIL)
    {
      jam();
      // This should be end of list...
      if (prev == FREE_PAGE_RNIL)
      {
        jam();
        ndbrequire(list == page_no); // page_no is both head and tail...
        fragPtrP->m_free_page_id_list = FREE_PAGE_RNIL;
      }
      else
      {
        jam();
        Uint32 * prevNextPtr = map.set(2 * (prev & ~(Uint32)FREE_PAGE_BIT));
        ndbrequire(prevNextPtr != 0);
        Uint32 prevNext = * prevNextPtr;
        ndbrequire(prevNext == (page_no | FREE_PAGE_BIT));
        * prevNextPtr = FREE_PAGE_RNIL;
      }
    }
    else
    {
      jam();
      next &= ~(Uint32)FREE_PAGE_BIT;
      Uint32 * nextPrevPtr = map.set(2 * next + 1);
      ndbrequire(nextPrevPtr != 0);
      ndbrequire(* nextPrevPtr == (page_no | FREE_PAGE_BIT));
      * nextPrevPtr = prev;
      if (prev == FREE_PAGE_RNIL)
      {
        jam();
        ndbrequire(list == page_no); // page_no is head
        fragPtrP->m_free_page_id_list = next;
      }
      else
      {
        jam();
        Uint32 * prevNextPtr = map.get(2 * (prev & ~(Uint32)FREE_PAGE_BIT));
        ndbrequire(prevNextPtr != 0);
        ndbrequire(* prevNextPtr == (page_no | FREE_PAGE_BIT));
        * prevNextPtr = next | FREE_PAGE_BIT;
      }
    }
  }
  
  fragPtrP->noOfPages = cnt + 1;
  if (page_no + 1 > max)
  {
    jam();
    fragPtrP->m_max_page_cnt = page_no + 1;
    if (DBUG_PAGE_MAP)
      ndbout_c("new max: %u", fragPtrP->m_max_page_cnt);
  }
  
  Uint32 lcp_scan_ptr_i = fragPtrP->m_lcp_scan_op;
  c_page_pool.getPtr(pagePtr);
  init_page(fragPtrP, pagePtr, page_no);
  if (lcp_scan_ptr_i != RNIL)
  {
    jam();
    ScanOpPtr scanOp;
    c_scanOpPool.getPtr(scanOp, lcp_scan_ptr_i);
    if (page_no < scanOp.p->m_endPage)
    {
      Local_key lcp_key = scanOp.p->m_scanPos.m_key;
      if (page_no > lcp_key.m_page_no)
      {
        jam();
        /**
         * We allocated a page during an LCP, it was within the pages that
         * will be checked during the LCP scan. The page has also not yet
         * been scanned by the LCP. Given that we know that the page will
         * only contain rows that would set the LCP_SKIP bit we will
         * set the LCP skip on the page level instead to speed up LCP
         * processing.
         */
        pagePtr.p->set_page_to_skip_lcp();
      }
    }
  }
  convertThPage((Fix_page*)pagePtr.p, tabPtrP, MM);
  pagePtr.p->page_state = ZTH_MM_FREE;
  free_pages.addFirst(pagePtr);

  do_check_page_map(fragPtrP);
  
  return pagePtr.i;
}

void
Dbtup::releaseFragPage(Fragrecord* fragPtrP, 
                       Uint32 logicalPageId, PagePtr pagePtr)
{
  Uint32 list = fragPtrP->m_free_page_id_list;
  Uint32 cnt = fragPtrP->noOfPages;
  DynArr256 map(c_page_map_pool, fragPtrP->m_page_map);
  Uint32 * next = map.set(2 * logicalPageId);
  Uint32 * prev = map.set(2 * logicalPageId + 1);
  ndbrequire(next != 0 && prev != 0);
  
  returnCommonArea(pagePtr.i, 1);

  /**
   * Add to head or tail of list...
   */
  const char * where = 0;
  if (list == FREE_PAGE_RNIL)
  {
    jam();
    * next = * prev = FREE_PAGE_RNIL;
    fragPtrP->m_free_page_id_list = logicalPageId;
    where = "empty";
  }
  else
  {
    jam();
    * next = list | FREE_PAGE_BIT;
    * prev = FREE_PAGE_RNIL;
    fragPtrP->m_free_page_id_list = logicalPageId;
    Uint32 * nextPrevPtr = map.set(2 * list + 1);
    ndbrequire(nextPrevPtr != 0);
    ndbrequire(*nextPrevPtr == FREE_PAGE_RNIL);
    * nextPrevPtr = logicalPageId | FREE_PAGE_BIT;
    where = "head";
  }

  fragPtrP->noOfPages = cnt - 1;
  if (DBUG_PAGE_MAP)
    ndbout_c("release(%u %u)@%s", logicalPageId, pagePtr.i, where);
  do_check_page_map(fragPtrP);
}

void Dbtup::errorHandler(Uint32 errorCode)
{
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
  ndbrequire(false);
}//Dbtup::errorHandler()

void
Dbtup::rebuild_page_free_list(Signal* signal)
{
  Ptr<Fragoperrec> fragOpPtr;
  fragOpPtr.i = signal->theData[1];
  Uint32 pageId = signal->theData[2];
  Uint32 tail = signal->theData[3];
  ptrCheckGuard(fragOpPtr, cnoOfFragoprec, fragoperrec);
  
  Ptr<Fragrecord> fragPtr;
  fragPtr.i= fragOpPtr.p->fragPointer;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  
  if (pageId == fragPtr.p->m_max_page_cnt)
  {
    RestoreLcpConf* conf = (RestoreLcpConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = fragOpPtr.p->m_senderData;
    sendSignal(fragOpPtr.p->m_senderRef,
	       GSN_RESTORE_LCP_CONF, signal, 
	       RestoreLcpConf::SignalLength, JBB);
    
    releaseFragoperrec(fragOpPtr);    
    return;
  }

  DynArr256 map(c_page_map_pool, fragPtr.p->m_page_map);
  Uint32* nextPtr = map.set(2 * pageId);
  Uint32* prevPtr = map.set(2 * pageId + 1);

  // Out of memory ?? Should nto be possible here/now
  ndbrequire(nextPtr != 0 && prevPtr != 0);
  
  if (* nextPtr == RNIL)
  {
    jam();
    /**
     * An unallocated page id...put in free list
     */
#if DBUG_PAGE_MAP
    char * where;
#endif
    if (tail == RNIL)
    {
      jam();
      ndbrequire(fragPtr.p->m_free_page_id_list == FREE_PAGE_RNIL);
      fragPtr.p->m_free_page_id_list = pageId;
      *nextPtr = FREE_PAGE_RNIL;
      *prevPtr = FREE_PAGE_RNIL;
#if DBUG_PAGE_MAP
      where = "head";
#endif
    }
    else
    {
      jam();
      ndbrequire(fragPtr.p->m_free_page_id_list != FREE_PAGE_RNIL);

      *nextPtr = FREE_PAGE_RNIL;
      *prevPtr = tail | FREE_PAGE_BIT;

      Uint32 * prevNextPtr = map.set(2 * tail);
      ndbrequire(prevNextPtr != 0);
      ndbrequire(* prevNextPtr == FREE_PAGE_RNIL);
      * prevNextPtr = pageId | FREE_PAGE_BIT;
#if DBUG_PAGE_MAP
      where = "tail";
#endif
    }
    tail = pageId;
#if DBUG_PAGE_MAP
    ndbout_c("adding page %u to free list @ %s", pageId, where);
#endif
  } 
  
  signal->theData[0] = ZREBUILD_FREE_PAGE_LIST;
  signal->theData[1] = fragOpPtr.i;
  signal->theData[2] = pageId + 1;
  signal->theData[3] = tail;
  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
}

