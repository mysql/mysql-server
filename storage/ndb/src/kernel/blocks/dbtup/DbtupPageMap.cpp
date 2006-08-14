/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#define DBTUP_C
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>

#define ljam() { jamLine(14000 + __LINE__); }
#define ljamEntry() { jamEntryLine(14000 + __LINE__); }

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
    ljam();
    allocMoreFragPages(regFragPtr);
    pageId = regFragPtr->emptyPrimPage.firstItem;
    if (pageId == RNIL) {
      ljam();
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
  PageRangePtr grpPageRangePtr;
  Uint32 loopLimit;
  Uint32 loopCount = 0;
  Uint32 pageRangeLimit = cnoOfPageRangeRec;
  ndbassert(logicalPageId < getNoOfPages(regFragPtr));
  grpPageRangePtr.i = regFragPtr->rootPageRange;
  while (true) {
    ndbassert(loopCount++ < 100);
    ndbrequire(grpPageRangePtr.i < pageRangeLimit);
    ptrAss(grpPageRangePtr, pageRange);
    loopLimit = grpPageRangePtr.p->currentIndexPos;
    ndbrequire(loopLimit <= 3);
    for (Uint32 i = 0; i <= loopLimit; i++) {
      ljam();
      if (grpPageRangePtr.p->startRange[i] <= logicalPageId) {
        if (grpPageRangePtr.p->endRange[i] >= logicalPageId) {
          if (grpPageRangePtr.p->type[i] == ZLEAF) {
            ljam();
            Uint32 realPageId = (logicalPageId - grpPageRangePtr.p->startRange[i]) +
                                 grpPageRangePtr.p->basePageId[i];
            return realPageId;
          } else {
            ndbrequire(grpPageRangePtr.p->type[i] == ZNON_LEAF);
            grpPageRangePtr.i = grpPageRangePtr.p->basePageId[i];
          }//if
        }//if
      }//if
    }//for
  }//while
  return 0;
}//Dbtup::getRealpid()

Uint32 Dbtup::getNoOfPages(Fragrecord* const regFragPtr)
{
  return regFragPtr->noOfPages;
}//Dbtup::getNoOfPages()

void Dbtup::initPageRangeSize(Uint32 size)
{
  cnoOfPageRangeRec = size;
}//Dbtup::initPageRangeSize()

/* ---------------------------------------------------------------- */
/* ----------------------- INSERT_PAGE_RANGE_TAB ------------------ */
/* ---------------------------------------------------------------- */
/*       INSERT A PAGE RANGE INTO THE FRAGMENT                      */
/*                                                                  */
/*       NOTE:   THE METHOD IS ATOMIC. EITHER THE ACTION IS         */
/*               PERFORMED FULLY OR NO ACTION IS PERFORMED AT ALL.  */
/*               TO SUPPORT THIS THE CODE HAS A CLEANUP PART AFTER  */
/*               ERRORS.                                            */
/* ---------------------------------------------------------------- */
bool Dbtup::insertPageRangeTab(Fragrecord*  const regFragPtr,
                               Uint32 startPageId,
                               Uint32 noPages) 
{
  PageRangePtr currPageRangePtr;
  if (cfirstfreerange == RNIL) {
    ljam();
    return false;
  }//if
  currPageRangePtr.i = regFragPtr->currentPageRange;
  if (currPageRangePtr.i == RNIL) {
    ljam();
/* ---------------------------------------------------------------- */
/*       THE FIRST PAGE RANGE IS HANDLED WITH SPECIAL CODE          */
/* ---------------------------------------------------------------- */
    seizePagerange(currPageRangePtr);
    regFragPtr->rootPageRange = currPageRangePtr.i;
    currPageRangePtr.p->currentIndexPos = 0;
    currPageRangePtr.p->parentPtr = RNIL;
  } else {
    ljam();
    ptrCheckGuard(currPageRangePtr, cnoOfPageRangeRec, pageRange);
    if (currPageRangePtr.p->currentIndexPos < 3) {
      ljam();
/* ---------------------------------------------------------------- */
/*       THE SIMPLE CASE WHEN IT IS ONLY NECESSARY TO FILL IN THE   */
/*       NEXT EMPTY POSITION IN THE PAGE RANGE RECORD IS TREATED    */
/*       BY COMMON CODE AT THE END OF THE SUBROUTINE.               */
/* ---------------------------------------------------------------- */
      currPageRangePtr.p->currentIndexPos++;
    } else {
      ljam();
      ndbrequire(currPageRangePtr.p->currentIndexPos == 3);
      currPageRangePtr.i = leafPageRangeFull(regFragPtr, currPageRangePtr);
      if (currPageRangePtr.i == RNIL) {
        return false;
      }//if
      ptrCheckGuard(currPageRangePtr, cnoOfPageRangeRec, pageRange);
    }//if
  }//if
  currPageRangePtr.p->startRange[currPageRangePtr.p->currentIndexPos] = regFragPtr->nextStartRange;
/* ---------------------------------------------------------------- */
/*       NOW SET THE LEAF LEVEL PAGE RANGE RECORD PROPERLY          */
/*       PAGE_RANGE_PTR REFERS TO LEAF RECORD WHEN ARRIVING HERE    */
/* ---------------------------------------------------------------- */
  currPageRangePtr.p->endRange[currPageRangePtr.p->currentIndexPos] = 
                                                   (regFragPtr->nextStartRange + noPages) - 1;
  currPageRangePtr.p->basePageId[currPageRangePtr.p->currentIndexPos] = startPageId;
  currPageRangePtr.p->type[currPageRangePtr.p->currentIndexPos] = ZLEAF;
/* ---------------------------------------------------------------- */
/*       WE NEED TO UPDATE THE CURRENT PAGE RANGE IN CASE IT HAS    */
/*       CHANGED. WE ALSO NEED TO UPDATE THE NEXT START RANGE       */
/* ---------------------------------------------------------------- */
  regFragPtr->currentPageRange = currPageRangePtr.i;
  regFragPtr->nextStartRange += noPages;
/* ---------------------------------------------------------------- */
/*       WE NEED TO UPDATE THE END RANGE IN ALL PAGE RANGE RECORDS  */
/*       UP TO THE ROOT.                                            */
/* ---------------------------------------------------------------- */
  PageRangePtr loopPageRangePtr;
  loopPageRangePtr = currPageRangePtr;
  while (true) {
    ljam();
    loopPageRangePtr.i = loopPageRangePtr.p->parentPtr;
    if (loopPageRangePtr.i != RNIL) {
      ljam();
      ptrCheckGuard(loopPageRangePtr, cnoOfPageRangeRec, pageRange);
      ndbrequire(loopPageRangePtr.p->currentIndexPos < 4);
      loopPageRangePtr.p->endRange[loopPageRangePtr.p->currentIndexPos] += noPages;
    } else {
      ljam();
      break;
    }//if
  }//while
  regFragPtr->noOfPages += noPages;
  return true;
}//Dbtup::insertPageRangeTab()


void Dbtup::releaseFragPages(Fragrecord* regFragPtr) 
{
  if (regFragPtr->rootPageRange == RNIL) {
    ljam();
    return;
  }//if
  PageRangePtr regPRPtr;
  regPRPtr.i = regFragPtr->rootPageRange;
  ptrCheckGuard(regPRPtr, cnoOfPageRangeRec, pageRange);
  while (true) {
    ljam();
    const Uint32 indexPos = regPRPtr.p->currentIndexPos;
    ndbrequire(indexPos < 4);

    const Uint32 basePageId = regPRPtr.p->basePageId[indexPos];
    regPRPtr.p->basePageId[indexPos] = RNIL;
    if (basePageId == RNIL) {
      ljam();
      /**
       * Finished with indexPos continue with next
       */
      if (indexPos > 0) {
        ljam();
	regPRPtr.p->currentIndexPos--;
	continue;
      }//if
      
      /* ---------------------------------------------------------------- */
      /* THE PAGE RANGE REC IS EMPTY. RELEASE IT.                         */
      /*----------------------------------------------------------------- */
      Uint32 parentPtr = regPRPtr.p->parentPtr;
      releasePagerange(regPRPtr);
      
      if (parentPtr != RNIL) {
	ljam();
	regPRPtr.i = parentPtr;
	ptrCheckGuard(regPRPtr, cnoOfPageRangeRec, pageRange);
	continue;
      }//if

      ljam();
      ndbrequire(regPRPtr.i == regFragPtr->rootPageRange);
      initFragRange(regFragPtr);
      for (Uint32 i = 0; i<MAX_FREE_LIST; i++)
      {
	LocalDLList<Page> tmp(c_page_pool, regFragPtr->free_var_page_array[i]);
	tmp.remove();
      }

      {
	LocalDLList<Page> tmp(c_page_pool, regFragPtr->emptyPrimPage);
	tmp.remove();
      }

      {
	LocalDLList<Page> tmp(c_page_pool, regFragPtr->thFreeFirst);
	tmp.remove();
      }

      {
	LocalSLList<Page> tmp(c_page_pool, regFragPtr->m_empty_pages);
	tmp.remove();
      }
      
      return;
    } else {
      if (regPRPtr.p->type[indexPos] == ZNON_LEAF) {
        jam();
	/* ---------------------------------------------------------------- */
	// A non-leaf node, we must release everything below it before we 
	// release this node.
	/* ---------------------------------------------------------------- */
        regPRPtr.i = basePageId;
        ptrCheckGuard(regPRPtr, cnoOfPageRangeRec, pageRange);
      } else {
        jam();
        ndbrequire(regPRPtr.p->type[indexPos] == ZLEAF);
	/* ---------------------------------------------------------------- */
	/* PAGE_RANGE_PTR /= RNIL AND THE CURRENT POS IS NOT A CHLED.       */
	/*----------------------------------------------------------------- */
	const Uint32 start = regPRPtr.p->startRange[indexPos];
	const Uint32 stop = regPRPtr.p->endRange[indexPos];
	ndbrequire(stop >= start);
	const Uint32 retNo = (stop - start + 1);
	returnCommonArea(basePageId, retNo);
      }//if
    }//if
  }//while
}//Dbtup::releaseFragPages()

void Dbtup::initializePageRange() 
{
  PageRangePtr regPTRPtr;
  for (regPTRPtr.i = 0;
       regPTRPtr.i < cnoOfPageRangeRec; regPTRPtr.i++) {
    ptrAss(regPTRPtr, pageRange);
    regPTRPtr.p->nextFree = regPTRPtr.i + 1;
  }//for
  regPTRPtr.i = cnoOfPageRangeRec - 1;
  ptrAss(regPTRPtr, pageRange);
  regPTRPtr.p->nextFree = RNIL;
  cfirstfreerange = 0;
  c_noOfFreePageRanges = cnoOfPageRangeRec;
}//Dbtup::initializePageRange()

void Dbtup::initFragRange(Fragrecord* const regFragPtr)
{
  regFragPtr->rootPageRange = RNIL;
  regFragPtr->currentPageRange = RNIL;
  regFragPtr->noOfPages = 0;
  regFragPtr->noOfVarPages = 0;
  regFragPtr->noOfPagesToGrow = 2;
  regFragPtr->nextStartRange = 0;
}//initFragRange()

Uint32 Dbtup::allocFragPages(Fragrecord* regFragPtr, Uint32 tafpNoAllocRequested) 
{
  Uint32 tafpPagesAllocated = 0;
  while (true) {
    Uint32 noOfPagesAllocated = 0;
    Uint32 noPagesToAllocate = tafpNoAllocRequested - tafpPagesAllocated;
    Uint32 retPageRef = RNIL;
    allocConsPages(noPagesToAllocate, noOfPagesAllocated, retPageRef);
    if (noOfPagesAllocated == 0) {
      ljam();
      return tafpPagesAllocated;
    }//if
/* ---------------------------------------------------------------- */
/*       IT IS NOW TIME TO PUT THE ALLOCATED AREA INTO THE PAGE     */
/*       RANGE TABLE.                                               */
/* ---------------------------------------------------------------- */
    Uint32 startRange = regFragPtr->nextStartRange;
    if (!insertPageRangeTab(regFragPtr, retPageRef, noOfPagesAllocated)) {
      ljam();
      returnCommonArea(retPageRef, noOfPagesAllocated);
      return tafpPagesAllocated;
    }//if
    tafpPagesAllocated += noOfPagesAllocated;
    Uint32 loopLimit = retPageRef + noOfPagesAllocated;
    PagePtr loopPagePtr;
/* ---------------------------------------------------------------- */
/*       SINCE A NUMBER OF PAGES WERE ALLOCATED FROM COMMON AREA    */
/*       WITH SUCCESS IT IS NOW TIME TO CHANGE THE STATE OF         */
/*       THOSE PAGES TO EMPTY_MM AND LINK THEM INTO THE EMPTY       */
/*       PAGE LIST OF THE FRAGMENT.                                 */
/* ---------------------------------------------------------------- */
    Uint32 prev = RNIL;
    for (loopPagePtr.i = retPageRef; loopPagePtr.i < loopLimit; loopPagePtr.i++) {
      ljam();
      c_page_pool.getPtr(loopPagePtr);
      loopPagePtr.p->page_state = ZEMPTY_MM;
      loopPagePtr.p->frag_page_id = startRange +
	(loopPagePtr.i - retPageRef);
      loopPagePtr.p->physical_page_id = loopPagePtr.i;
      loopPagePtr.p->nextList = loopPagePtr.i + 1;
      loopPagePtr.p->prevList = prev;
      prev = loopPagePtr.i;
    }//for
    loopPagePtr.i--;
    ndbassert(loopPagePtr.p == c_page_pool.getPtr(loopPagePtr.i));
    loopPagePtr.p->nextList = RNIL;
    
    LocalDLList<Page> alloc(c_page_pool, regFragPtr->emptyPrimPage);
    if (noOfPagesAllocated > 1)
    {
      alloc.add(retPageRef, loopPagePtr);
    }
    else
    {
      alloc.add(loopPagePtr);
    }

/* ---------------------------------------------------------------- */
/*       WAS ENOUGH PAGES ALLOCATED OR ARE MORE NEEDED.             */
/* ---------------------------------------------------------------- */
    if (tafpPagesAllocated < tafpNoAllocRequested) {
      ljam();
    } else {
      ndbrequire(tafpPagesAllocated == tafpNoAllocRequested);
      ljam();
      return tafpNoAllocRequested;
    }//if
  }//while
}//Dbtup::allocFragPages()

void Dbtup::allocMoreFragPages(Fragrecord* const regFragPtr) 
{
  Uint32 noAllocPages = regFragPtr->noOfPagesToGrow >> 3; // 12.5%
  noAllocPages += regFragPtr->noOfPagesToGrow >> 4; // 6.25%
  noAllocPages += 2;
/* -----------------------------------------------------------------*/
// We will grow by 18.75% plus two more additional pages to grow
// a little bit quicker in the beginning.
/* -----------------------------------------------------------------*/
  Uint32 allocated = allocFragPages(regFragPtr, noAllocPages);
  regFragPtr->noOfPagesToGrow += allocated;
}//Dbtup::allocMoreFragPages()

Uint32 Dbtup::leafPageRangeFull(Fragrecord*  const regFragPtr, PageRangePtr currPageRangePtr)
{
/* ---------------------------------------------------------------- */
/*       THE COMPLEX CASE WHEN THE LEAF NODE IS FULL. GO UP THE TREE*/
/*       TO FIND THE FIRST RECORD WITH A FREE ENTRY. ALLOCATE NEW   */
/*       PAGE RANGE RECORDS THEN ALL THE WAY DOWN TO THE LEAF LEVEL */
/*       AGAIN. THE TREE SHOULD ALWAYS REMAIN BALANCED.             */
/* ---------------------------------------------------------------- */
  PageRangePtr parentPageRangePtr;
  PageRangePtr foundPageRangePtr;
  parentPageRangePtr = currPageRangePtr;
  Uint32 tiprNoLevels = 1;
  while (true) {
    ljam();
    parentPageRangePtr.i = parentPageRangePtr.p->parentPtr;
    if (parentPageRangePtr.i == RNIL) {
      ljam();
/* ---------------------------------------------------------------- */
/*       WE HAVE REACHED THE ROOT. A NEW ROOT MUST BE ALLOCATED.    */
/* ---------------------------------------------------------------- */
      if (c_noOfFreePageRanges < tiprNoLevels) {
        ljam();
        return RNIL;
      }//if
      PageRangePtr oldRootPRPtr;
      PageRangePtr newRootPRPtr;
      oldRootPRPtr.i = regFragPtr->rootPageRange;
      ptrCheckGuard(oldRootPRPtr, cnoOfPageRangeRec, pageRange);
      seizePagerange(newRootPRPtr);
      regFragPtr->rootPageRange = newRootPRPtr.i;
      oldRootPRPtr.p->parentPtr = newRootPRPtr.i;

      newRootPRPtr.p->basePageId[0] = oldRootPRPtr.i;
      newRootPRPtr.p->parentPtr = RNIL;
      newRootPRPtr.p->startRange[0] = 0;
      newRootPRPtr.p->endRange[0] = regFragPtr->nextStartRange - 1;
      newRootPRPtr.p->type[0] = ZNON_LEAF;
      newRootPRPtr.p->startRange[1] = regFragPtr->nextStartRange;
      newRootPRPtr.p->endRange[1] = regFragPtr->nextStartRange - 1;
      newRootPRPtr.p->type[1] = ZNON_LEAF;
      newRootPRPtr.p->currentIndexPos = 1;
      foundPageRangePtr = newRootPRPtr;
      break;
    } else {
      ljam();
      ptrCheckGuard(parentPageRangePtr, cnoOfPageRangeRec, pageRange);
      if (parentPageRangePtr.p->currentIndexPos < 3) {
        ljam();
/* ---------------------------------------------------------------- */
/*       WE HAVE FOUND AN EMPTY ENTRY IN A PAGE RANGE RECORD.       */
/*       ALLOCATE A NEW PAGE RANGE RECORD, FILL IN THE START RANGE, */
/*       ALLOCATE A NEW PAGE RANGE RECORD AND UPDATE THE POINTERS   */
/* ---------------------------------------------------------------- */
        parentPageRangePtr.p->currentIndexPos++;
        parentPageRangePtr.p->startRange[parentPageRangePtr.p->currentIndexPos] = regFragPtr->nextStartRange;
        parentPageRangePtr.p->endRange[parentPageRangePtr.p->currentIndexPos] = regFragPtr->nextStartRange - 1;
        parentPageRangePtr.p->type[parentPageRangePtr.p->currentIndexPos] = ZNON_LEAF;
        foundPageRangePtr = parentPageRangePtr;
        break;
      } else {
        ljam();
        ndbrequire(parentPageRangePtr.p->currentIndexPos == 3);
/* ---------------------------------------------------------------- */
/*       THE PAGE RANGE RECORD WAS FULL. FIND THE PARENT RECORD     */
/*       AND INCREASE THE NUMBER OF LEVELS WE HAVE TRAVERSED        */
/*       GOING UP THE TREE.                                         */
/* ---------------------------------------------------------------- */
        tiprNoLevels++;
      }//if
    }//if
  }//while
/* ---------------------------------------------------------------- */
/*       REMEMBER THE ERROR LEVEL IN CASE OF ALLOCATION ERRORS      */
/* ---------------------------------------------------------------- */
  PageRangePtr newPageRangePtr;
  PageRangePtr prevPageRangePtr;
  prevPageRangePtr = foundPageRangePtr;
  if (c_noOfFreePageRanges < tiprNoLevels) {
    ljam();
    return RNIL;
  }//if
/* ---------------------------------------------------------------- */
/*       NOW WE HAVE PERFORMED THE SEARCH UPWARDS AND FILLED IN THE */
/*       PROPER FIELDS IN THE PAGE RANGE RECORD WHERE SOME SPACE    */
/*       WAS FOUND. THE NEXT STEP IS TO ALLOCATE PAGE RANGES SO     */
/*       THAT WE KEEP THE B-TREE BALANCED. THE NEW PAGE RANGE       */
/*       ARE ALSO PROPERLY UPDATED ON THE PATH TO THE LEAF LEVEL.   */
/* ---------------------------------------------------------------- */
  while (true) {
    ljam();
    seizePagerange(newPageRangePtr);
    tiprNoLevels--;
    ndbrequire(prevPageRangePtr.p->currentIndexPos < 4);
    prevPageRangePtr.p->basePageId[prevPageRangePtr.p->currentIndexPos] = newPageRangePtr.i;
    newPageRangePtr.p->parentPtr = prevPageRangePtr.i;
    newPageRangePtr.p->currentIndexPos = 0;
    if (tiprNoLevels > 0) {
      ljam();
      newPageRangePtr.p->startRange[0] = regFragPtr->nextStartRange;
      newPageRangePtr.p->endRange[0] = regFragPtr->nextStartRange - 1;
      newPageRangePtr.p->type[0] = ZNON_LEAF;
      prevPageRangePtr = newPageRangePtr;
    } else {
      ljam();
      break;
    }//if
  }//while
  return newPageRangePtr.i;
}//Dbtup::leafPageRangeFull()

void Dbtup::releasePagerange(PageRangePtr regPRPtr) 
{
  regPRPtr.p->nextFree = cfirstfreerange;
  cfirstfreerange = regPRPtr.i;
  c_noOfFreePageRanges++;
}//Dbtup::releasePagerange()

void Dbtup::seizePagerange(PageRangePtr& regPageRangePtr) 
{
  regPageRangePtr.i = cfirstfreerange;
  ptrCheckGuard(regPageRangePtr, cnoOfPageRangeRec, pageRange);
  cfirstfreerange = regPageRangePtr.p->nextFree;
  regPageRangePtr.p->nextFree = RNIL;
  regPageRangePtr.p->currentIndexPos = 0;
  regPageRangePtr.p->parentPtr = RNIL;
  for (Uint32 i = 0; i < 4; i++) {
    regPageRangePtr.p->startRange[i] = 1;
    regPageRangePtr.p->endRange[i] = 0;
    regPageRangePtr.p->type[i] = ZNON_LEAF;
    regPageRangePtr.p->basePageId[i] = (Uint32)-1;
  }//for
  c_noOfFreePageRanges--;
}//Dbtup::seizePagerange()

void Dbtup::errorHandler(Uint32 errorCode)
{
  switch (errorCode) {
  case 0:
    ljam();
    break;
  case 1:
    ljam();
    break;
  case 2:
    ljam();
    break;
  default:
    ljam();
  }
  ndbrequire(false);
}//Dbtup::errorHandler()
