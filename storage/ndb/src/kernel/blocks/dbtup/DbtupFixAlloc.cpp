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

#define ljam() { jamLine(6000 + __LINE__); }
#define ljamEntry() { jamEntryLine(6000 + __LINE__); }

//
// Fixed Allocator
// This module is used to allocate and free fixed size tuples from the
// set of pages attached to a fragment. The fixed size is preset per
// fragment and their can only be one such value per fragment in the
// current implementation.
// 
// Public methods
// bool allocTh(Fragrecord* const regFragPtr, # In
//              Tablerec* const regTabPtr,    # In
//              Uint32 pageType,              # In
//              Signal* signal,               # In
//              Uint32& pageOffset,           # Out
//              PagePtr& pagePtr)             # In/Out
// This method allocates a fixed size and the pagePtr is a reference
// to the page and pageOffset is the offset in the page of the tuple.
//
// freeTh()
// This method is used to free a tuple header in normal transaction
// handling.
//
// freeThSr()
// This method is used to free a tuple as part of executing the undo
// log records.
//
// getThAtPageSr()
// This method is used to allocate a tuple on a set page as part of
// undo log execution.
//
//
// Private methods
// getThAtPage()
// This method gets a tuple from a page with free tuples.
//
// convertThPage()
// Convert an empty page into a page of free tuples in a linked list.
//
// getEmptyPageThCopy()
// A page recently taken from set of empty pages on fragment is made
// part of the copy pages.
//
// getEmptyPageTh()
// A page recently taken from the set of empty pages on the fragment is
// is made part of the set of free pages with fixed size tuples in the
// fragment.
// 
bool Dbtup::allocTh(Fragrecord* const regFragPtr,
                    Tablerec* const regTabPtr,
                    Uint32 pageType,
                    Signal* signal,
                    Uint32& pageOffset,
                    PagePtr& pagePtr) 
{
  if (pageType == SAME_PAGE) {
    ljam();
    ptrCheckGuard(pagePtr, cnoOfPage, page);
    if (pagePtr.p->pageWord[ZPAGE_STATE_POS] == ZTH_MM_FREE) {
      ljam();
      getThAtPage(regFragPtr, pagePtr.p, signal, pageOffset);
      return true;
    }//if
    pageType = NORMAL_PAGE;
  }//if
  if (pageType == COPY_PAGE) {
/* ---------------------------------------------------------------- */
// Allocate a tuple header for the copy of the tuple header
/* ---------------------------------------------------------------- */
    if (regFragPtr->thFreeCopyFirst == RNIL) {
/* ---------------------------------------------------------------- */
// No page in list with free tuple header exists
/* ---------------------------------------------------------------- */
      if (regFragPtr->noCopyPagesAlloc < ZMAX_NO_COPY_PAGES) {
        ljam();
/* ---------------------------------------------------------------- */
// We have not yet allocated the maximum number of copy pages for
// this fragment.
/* ---------------------------------------------------------------- */
        pagePtr.i = getEmptyPage(regFragPtr);
        if (pagePtr.i != RNIL) {
          ljam();
/* ---------------------------------------------------------------- */
// We have empty pages already allocated to this fragment. Allocate
// one of those as copy page.
/* ---------------------------------------------------------------- */
          ptrCheckGuard(pagePtr, cnoOfPage, page);
          getEmptyPageThCopy(regFragPtr, signal, pagePtr.p);
/* ---------------------------------------------------------------- */
// Convert page into a tuple header page.
/* ---------------------------------------------------------------- */
          convertThPage(regTabPtr->tupheadsize, pagePtr.p);
          getThAtPage(regFragPtr, pagePtr.p, signal, pageOffset);
          return true;
        }//if
      }//if
    } else {
      ljam();
/* ---------------------------------------------------------------- */
/*       NORMAL PATH WHEN COPY PAGE REQUESTED, GET PAGE POINTER     */
/*       AND THEN GOTO COMMON HANDLING OF GET TUPLE HEADER AT PAGE. */
/* ---------------------------------------------------------------- */
      pagePtr.i = getRealpid(regFragPtr, regFragPtr->thFreeCopyFirst);
      ptrCheckGuard(pagePtr, cnoOfPage, page);
      getThAtPage(regFragPtr, pagePtr.p, signal, pageOffset);
      return true;
    }//if
  }//if
/* ---------------------------------------------------------------- */
/*       EITHER NORMAL PAGE REQUESTED OR ALLOCATION FROM COPY PAGE  */
/*       FAILED. TRY ALLOCATING FROM NORMAL PAGE.                   */
/* ---------------------------------------------------------------- */
  Uint32 fragPageId = regFragPtr->thFreeFirst;
  if (fragPageId == RNIL) {
/* ---------------------------------------------------------------- */
// No prepared tuple header page with free entries exists.
/* ---------------------------------------------------------------- */
    pagePtr.i = getEmptyPage(regFragPtr);
    if (pagePtr.i != RNIL) {
      ljam();
/* ---------------------------------------------------------------- */
// We found empty pages on the fragment. Allocate an empty page and
// convert it into a tuple header page and put it in thFreeFirst-list.
/* ---------------------------------------------------------------- */
      ptrCheckGuard(pagePtr, cnoOfPage, page);
      getEmptyPageTh(regFragPtr, signal, pagePtr.p);
      convertThPage(regTabPtr->tupheadsize, pagePtr.p);
      getThAtPage(regFragPtr, pagePtr.p, signal, pageOffset);
      return true;
    } else {
      ljam();
/* ---------------------------------------------------------------- */
/*       THERE ARE NO EMPTY PAGES. MEMORY CAN NOT BE ALLOCATED.     */
/* ---------------------------------------------------------------- */
      terrorCode = ZMEM_NOMEM_ERROR;
      return false;
    }//if
  } else {
    ljam();
/* ---------------------------------------------------------------- */
/*       THIS SHOULD BE THE COMMON PATH THROUGH THE CODE, FREE      */
/*       COPY PAGE EXISTED.                                         */
/* ---------------------------------------------------------------- */
    pagePtr.i = getRealpid(regFragPtr, fragPageId);
    ptrCheckGuard(pagePtr, cnoOfPage, page);
    getThAtPage(regFragPtr, pagePtr.p, signal, pageOffset);
    return true;
  }//if
  ndbrequire(false); // Dead code
  return false;
}//Dbtup::allocTh()

void Dbtup::convertThPage(Uint32 Tupheadsize,
                          Page*  const regPagePtr) 
{
  Uint32 ctpConstant = Tupheadsize << 16;
  Uint32 nextTuple = ZPAGE_HEADER_SIZE + Tupheadsize;
  Uint32 endOfList;
  /*
  ASSUMES AT LEAST ONE TUPLE HEADER FITS AND THEREFORE NO HANDLING
  OF ZERO AS EXTREME CASE
  */
  do {
    ljam();
    endOfList = nextTuple - Tupheadsize;
    regPagePtr->pageWord[endOfList] = ctpConstant + nextTuple;
    nextTuple += Tupheadsize;
  } while (nextTuple <= ZWORDS_ON_PAGE);
  regPagePtr->pageWord[endOfList] = ctpConstant;
  Uint32 startOfList = ZPAGE_HEADER_SIZE;
  regPagePtr->pageWord[ZFREELIST_HEADER_POS] = (startOfList << 16) + endOfList;
}//Dbtup::convertThPage()

void Dbtup::getEmptyPageTh(Fragrecord* const regFragPtr,
                           Signal* signal,
                           Page* const regPagePtr) 
{
  if (isUndoLoggingNeeded(regFragPtr, regPagePtr->pageWord[ZPAGE_FRAG_PAGE_ID_POS])) {
    cprAddUndoLogPageHeader(signal,
                            regPagePtr,
                            regFragPtr);
  }//if
  regPagePtr->pageWord[ZPAGE_NEXT_POS] = regFragPtr->thFreeFirst;
  regPagePtr->pageWord[ZPAGE_STATE_POS] = ZTH_MM_FREE;
  regFragPtr->thFreeFirst = regPagePtr->pageWord[ZPAGE_FRAG_PAGE_ID_POS];

ndbrequire(regFragPtr->thFreeFirst != (RNIL -1));
}//Dbtup::getEmptyPageTh()

void Dbtup::getEmptyPageThCopy(Fragrecord* const regFragPtr,
                               Signal* signal,
                               Page* const regPagePtr) 
{
  if (isUndoLoggingNeeded(regFragPtr, regPagePtr->pageWord[ZPAGE_FRAG_PAGE_ID_POS])) {
    cprAddUndoLogPageHeader(signal,
                            regPagePtr,
                            regFragPtr);
  }//if
  regPagePtr->pageWord[ZPAGE_NEXT_POS] = regFragPtr->thFreeCopyFirst;
  regPagePtr->pageWord[ZPAGE_STATE_POS] = ZTH_MM_FREE_COPY;
  regFragPtr->thFreeCopyFirst = regPagePtr->pageWord[ZPAGE_FRAG_PAGE_ID_POS];
  regFragPtr->noCopyPagesAlloc++;
}//Dbtup::getEmptyPageThCopy()

void Dbtup::getThAtPage(Fragrecord* const regFragPtr,
                        Page* const regPagePtr,
                        Signal* signal,
                        Uint32& pageOffset) 
{
  Uint32 freeListHeader = regPagePtr->pageWord[ZFREELIST_HEADER_POS];
  Uint32 startTuple = freeListHeader >> 16;
  Uint32 endTuple = freeListHeader & 0xffff;
  pageOffset = startTuple;	/* START IS THE ONE ALLOCATED */
  if (startTuple > 0) {
   if (startTuple != endTuple) {
/* ---------------------------------------------------------------- */
/*       NOT THE LAST, SIMPLY RESHUFFLE POINTERS.                   */
/* ---------------------------------------------------------------- */
     ndbrequire(startTuple < ZWORDS_ON_PAGE);
     startTuple = regPagePtr->pageWord[startTuple] & 0xffff;
     regPagePtr->pageWord[ZFREELIST_HEADER_POS] = endTuple +
                                                  (startTuple << 16);
     return;
   } else {
/* ---------------------------------------------------------------- */
/*       THIS WAS THE LAST TUPLE HEADER IN THIS PAGE. REMOVE IT FROM*/
/*       THE TUPLE HEADER FREE LIST OR TH COPY FREE LIST. ALSO SET  */
/*       A PROPER PAGE STATE.                                       */
/*                                                                  */
/*       WE ALSO HAVE TO INSERT AN UNDO LOG ENTRY TO ENSURE PAGE    */
/*       ARE MAINTAINED EVEN AFTER A SYSTEM CRASH.                  */
/* ---------------------------------------------------------------- */
     if (isUndoLoggingNeeded(regFragPtr,
                             regPagePtr->pageWord[ZPAGE_FRAG_PAGE_ID_POS])) {
       cprAddUndoLogPageHeader(signal,
                               regPagePtr,
                               regFragPtr);
     }//if
     if (regPagePtr->pageWord[ZPAGE_STATE_POS] == ZTH_MM_FREE) {
       ljam();
       regFragPtr->thFreeFirst = regPagePtr->pageWord[ZPAGE_NEXT_POS];
       regPagePtr->pageWord[ZPAGE_STATE_POS] = ZTH_MM_FULL;
     } else if (regPagePtr->pageWord[ZPAGE_STATE_POS] == ZTH_MM_FREE_COPY) {
       ljam();
       regFragPtr->thFreeCopyFirst = regPagePtr->pageWord[ZPAGE_NEXT_POS];
       regPagePtr->pageWord[ZPAGE_STATE_POS] = ZTH_MM_FULL_COPY;
     } else {
       ndbrequire(false);
     }//if
     regPagePtr->pageWord[ZFREELIST_HEADER_POS] = 0;
     regPagePtr->pageWord[ZPAGE_NEXT_POS] = RNIL;
   }//if
  } else {
    ndbrequire(false);
  }//if
  return;
}//Dbtup::getThAtPage()

void Dbtup::getThAtPageSr(Page* const regPagePtr,
                          Uint32& pageOffset) 
{
  Uint32 freeListHeader = regPagePtr->pageWord[ZFREELIST_HEADER_POS];
  Uint32 startTuple = freeListHeader >> 16;
  Uint32 endTuple = freeListHeader & 0xffff;
  ndbrequire(startTuple > 0);
  pageOffset = startTuple;	/* START IS THE ONE ALLOCATED */
  if (startTuple == endTuple) {
    ljam();
/* ---------------------------------------------------------------- */
/*       THIS WAS THE LAST TUPLE HEADER IN THIS PAGE. SINCE WE ARE  */
/*       UNDOING PAGE UPDATES WE SHALL NOT DO ANYTHING ABOUT THE    */
/*       PAGE HEADER. THIS IS DONE BY SEPARATE LOG RECORDS.         */
/* ---------------------------------------------------------------- */
    regPagePtr->pageWord[ZFREELIST_HEADER_POS] = 0;
  } else {
    ljam();
/* ---------------------------------------------------------------- */
/*       NOT THE LAST, SIMPLY RESHUFFLE POINTERS.                   */
/* ---------------------------------------------------------------- */
    ndbrequire(startTuple < ZWORDS_ON_PAGE);
    startTuple = regPagePtr->pageWord[startTuple] & 0xffff;	/* GET NEXT POINTER */
    regPagePtr->pageWord[ZFREELIST_HEADER_POS] = endTuple + (startTuple << 16);
  }//if
}//Dbtup::getThAtPageSr()

void Dbtup::freeTh(Fragrecord*  const regFragPtr,
                   Tablerec* const regTabPtr,
                   Signal* signal,
                   Page*  const regPagePtr,
                   Uint32 freePageOffset) 
{
  Uint32 startOfList = regPagePtr->pageWord[ZFREELIST_HEADER_POS] >> 16;
  Uint32 endOfList = regPagePtr->pageWord[ZFREELIST_HEADER_POS] & 0xffff;
/* LINK THE NOW FREE TUPLE SPACE INTO BEGINNING OF FREE LIST OF OF THE PAGE */
/* SET THE SIZE OF THE NEW FREE SPACE AND LINK TO THE OLD START OF FREELIST */
  ndbrequire(freePageOffset < ZWORDS_ON_PAGE);
  regPagePtr->pageWord[freePageOffset] = (regTabPtr->tupheadsize << 16) +
                                          startOfList;
  if (endOfList == 0) {
    ljam();
    ndbrequire(startOfList == 0);
/* ---------------------------------------------------------------- */
/*       THE PAGE WAS PREVIOUSLY FULL, NO EMPTY SPACE AT ALL.       */
/*       THIS ENTRY WILL THEN BE BOTH THE START AND THE END OF THE  */
/*       LIST. IT WILL ALSO BE PUT ON THE PROPER FREE LIST.         */
/*                                                                  */
/*       UPDATE OF NEXT POINTER AND PAGE STATE MUST BE LOGGED TO    */
/*       THE UNDO LOG TO ENSURE THAT FREE LISTS ARE OK AFTER A      */
/*       SYSTEM RESTART.                                            */
/* ---------------------------------------------------------------- */
    if (isUndoLoggingNeeded(regFragPtr, regPagePtr->pageWord[ZPAGE_FRAG_PAGE_ID_POS])) {
      cprAddUndoLogPageHeader(signal,
                              regPagePtr,
                              regFragPtr);
    }//if
    regPagePtr->pageWord[ZFREELIST_HEADER_POS] = (freePageOffset << 16) + freePageOffset;
    if (regPagePtr->pageWord[ZPAGE_STATE_POS] == ZTH_MM_FULL) {
      ljam();
      regPagePtr->pageWord[ZPAGE_NEXT_POS] = regFragPtr->thFreeFirst;
      regFragPtr->thFreeFirst = regPagePtr->pageWord[ZPAGE_FRAG_PAGE_ID_POS];	
      regPagePtr->pageWord[ZPAGE_STATE_POS] = ZTH_MM_FREE;
    } else {
      ndbrequire(regPagePtr->pageWord[ZPAGE_STATE_POS] == ZTH_MM_FULL_COPY);
      ljam();
      regPagePtr->pageWord[ZPAGE_NEXT_POS] = regFragPtr->thFreeCopyFirst;
      regFragPtr->thFreeCopyFirst = regPagePtr->pageWord[ZPAGE_FRAG_PAGE_ID_POS];
      regPagePtr->pageWord[ZPAGE_STATE_POS] = ZTH_MM_FREE_COPY;
    }//if
  } else {
    ljam();
    regPagePtr->pageWord[ZFREELIST_HEADER_POS] = (freePageOffset << 16) + endOfList;
  }//if
}//Dbtup::freeTh()

void Dbtup::freeThSr(Tablerec* const regTabPtr,
                     Page*  const regPagePtr,
                     Uint32 freePageOffset) 
{
/* ------------------------------------------------------------------------ */
/* LINK THE NOW FREE TUPLE SPACE INTO BEGINNING OF FREE LIST OF OF THE PAGE */
/* SET THE SIZE OF THE NEW FREE SPACE AND LINK TO THE OLD START OF FREELIST */
/* ------------------------------------------------------------------------ */
  Uint32 startOfList = regPagePtr->pageWord[ZFREELIST_HEADER_POS] >> 16;
  Uint32 endOfList = regPagePtr->pageWord[ZFREELIST_HEADER_POS] & 0xffff;
  ndbrequire(freePageOffset < ZWORDS_ON_PAGE);
  regPagePtr->pageWord[freePageOffset] = (regTabPtr->tupheadsize << 16) + startOfList;
  if (endOfList == 0) {
    ljam();
    ndbrequire(startOfList == 0);
/* ---------------------------------------------------------------- */
/*       THE PAGE WAS PREVIOUSLY FULL, NO EMPTY SPACE AT ALL.       */
/*       THIS ENTRY WILL THEN BE BOTH THE START AND THE END OF THE  */
/*       LIST. IT WILL ALSO BE PUT ON THE PROPER FREE LIST.         */
/* ---------------------------------------------------------------- */
    regPagePtr->pageWord[ZFREELIST_HEADER_POS] = (freePageOffset << 16) + freePageOffset;
  } else {
    ljam();
    regPagePtr->pageWord[ZFREELIST_HEADER_POS] = (freePageOffset << 16) + endOfList;
  }//if
}//Dbtup::freeThSr()

