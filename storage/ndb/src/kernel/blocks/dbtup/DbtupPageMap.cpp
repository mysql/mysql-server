/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#define DBTUP_C
#define DBTUP_PAGE_MAP_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>

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

Uint32 Dbtup::getEmptyPage(Fragrecord* regFragPtr)
{
  Uint32 pageId = regFragPtr->emptyPrimPage.firstItem;
  if (pageId == RNIL) {
    jam();
    allocFragPage(regFragPtr);
    pageId = regFragPtr->emptyPrimPage.firstItem;
    if (pageId == RNIL) {
      jam();
      return RNIL;
    }//if
  }//if
  PagePtr pagePtr;
  LocalDLList<Page> alloc_pages(c_page_pool, regFragPtr->emptyPrimPage);    
  alloc_pages.getPtr(pagePtr, pageId);
  alloc_pages.remove(pagePtr);
  return pageId;
}//Dbtup::getEmptyPage()

Uint32 Dbtup::getRealpid(Fragrecord* regFragPtr, Uint32 logicalPageId) 
{
  DynArr256 map(c_page_map_pool, regFragPtr->m_page_map);
  Uint32 * ptr = map.get(logicalPageId);
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
  Uint32 * ptr = map.get(logicalPageId);
  if (likely(ptr != 0))
  {
    return * ptr;
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
  pagePtr.p->page_state = ZEMPTY_MM;
  pagePtr.p->frag_page_id = pageId;
  pagePtr.p->physical_page_id = pagePtr.i;
  pagePtr.p->nextList = RNIL;
  pagePtr.p->prevList = RNIL;
}

Uint32 
Dbtup::allocFragPage(Fragrecord* regFragPtr) 
{
  Uint32 noOfPagesAllocated = 0;
  Uint32 retPageRef = RNIL;
  allocConsPages(1, noOfPagesAllocated, retPageRef);
  if (noOfPagesAllocated == 0) 
  {
    jam();
    return 0;
  }//if
  
  Uint32 pageId = regFragPtr->noOfPages;
  DynArr256 map(c_page_map_pool, regFragPtr->m_page_map);
  
  Uint32 * ptr = map.set(pageId);
  if (likely(ptr != 0))
  {
    jam();
    * ptr = retPageRef;
    regFragPtr->noOfPages = pageId + 1;

    Ptr<Page> pagePtr;
    pagePtr.i = retPageRef;
    c_page_pool.getPtr(pagePtr);
    init_page(regFragPtr, pagePtr, pageId);
    LocalDLList<Page> alloc(c_page_pool, regFragPtr->emptyPrimPage);
    alloc.add(pagePtr);
    return 1;
  }
  
  jam();
  returnCommonArea(retPageRef, noOfPagesAllocated);
  return 0;
}//Dbtup::allocFragPage()

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
