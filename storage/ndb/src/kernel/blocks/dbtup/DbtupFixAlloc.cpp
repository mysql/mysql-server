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
// bool
// alloc_fix_rec(Fragrecord* const regFragPtr, # In
//               Tablerec* const regTabPtr,    # In
//               Uint32 pageType,              # In
//               Signal* signal,               # In
//               Uint32& pageOffset,           # Out
//               PagePtr& pagePtr)             # In/Out
// This method allocates a fixed size and the pagePtr is a reference
// to the page and pageOffset is the offset in the page of the tuple.
//
// freeTh()
// This method is used to free a tuple header in normal transaction
// handling.
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
// getEmptyPageTh()
// A page recently taken from the set of empty pages on the fragment is
// is made part of the set of free pages with fixed size tuples in the
// fragment.
// 
Uint32*
Dbtup::alloc_fix_rec(Fragrecord* const regFragPtr,
		     Tablerec* const regTabPtr,
		     Local_key* key,
		     Uint32 * out_frag_page_id) 
{
/* ---------------------------------------------------------------- */
/*       EITHER NORMAL PAGE REQUESTED OR ALLOCATION FROM COPY PAGE  */
/*       FAILED. TRY ALLOCATING FROM NORMAL PAGE.                   */
/* ---------------------------------------------------------------- */
  PagePtr pagePtr;
  pagePtr.i= regFragPtr->thFreeFirst;
  if (pagePtr.i == RNIL) {
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
      ptrCheckGuard(pagePtr, cnoOfPage, cpage);
      
      convertThPage(regTabPtr->m_offsets[MM].m_fix_header_size, 
		    (Fix_page*)pagePtr.p);
      
      pagePtr.p->next_page = regFragPtr->thFreeFirst;
      pagePtr.p->page_state = ZTH_MM_FREE;
      regFragPtr->thFreeFirst = pagePtr.i;
    } else {
      ljam();
/* ---------------------------------------------------------------- */
/*       THERE ARE NO EMPTY PAGES. MEMORY CAN NOT BE ALLOCATED.     */
/* ---------------------------------------------------------------- */
      return 0;
    }
  } else {
    ljam();
/* ---------------------------------------------------------------- */
/*       THIS SHOULD BE THE COMMON PATH THROUGH THE CODE, FREE      */
/*       COPY PAGE EXISTED.                                         */
/* ---------------------------------------------------------------- */
    ptrCheckGuard(pagePtr, cnoOfPage, cpage);
  }

  Uint32 page_offset= alloc_tuple_from_page(regFragPtr, (Fix_page*)pagePtr.p);
  
  *out_frag_page_id= pagePtr.p->frag_page_id;
  key->m_page_no = pagePtr.i;
  key->m_page_idx = page_offset;
  return pagePtr.p->m_data + page_offset;
}

void Dbtup::convertThPage(Uint32 Tupheadsize,
                          Fix_page*  const regPagePtr) 
{
  Uint32 nextTuple = Tupheadsize;
  Uint32 endOfList;
  /*
  ASSUMES AT LEAST ONE TUPLE HEADER FITS AND THEREFORE NO HANDLING
  OF ZERO AS EXTREME CASE
  */
  Uint32 cnt= 0;
  Uint32 pos= 0;
  Uint32 prev = 0xFFFF;
#ifdef VM_TRACE
  memset(regPagePtr->m_data, 0xF1, 4*Fix_page::DATA_WORDS);
#endif
  while (pos + nextTuple <= Fix_page::DATA_WORDS)
  {
    regPagePtr->m_data[pos] = (prev << 16) | (pos + nextTuple);
    regPagePtr->m_data[pos + 1] = Tuple_header::FREE;
    prev = pos;
    pos += nextTuple;
    cnt ++;
  } 
  
  regPagePtr->m_data[prev] |= 0xFFFF;
  regPagePtr->next_free_index= 0;
  regPagePtr->free_space= cnt;
  regPagePtr->m_page_header.m_page_type = File_formats::PT_Tup_fixsize_page;
}//Dbtup::convertThPage()

Uint32
Dbtup::alloc_tuple_from_page(Fragrecord* const regFragPtr,
			     Fix_page* const regPagePtr)
{
  Uint32 idx= regPagePtr->alloc_record();
  if(regPagePtr->free_space == 0)
  {
    jam();
/* ---------------------------------------------------------------- */
/*       THIS WAS THE LAST TUPLE HEADER IN THIS PAGE. REMOVE IT FROM*/
/*       THE TUPLE HEADER FREE LIST OR TH COPY FREE LIST. ALSO SET  */
/*       A PROPER PAGE STATE.                                       */
/*                                                                  */
/*       WE ALSO HAVE TO INSERT AN UNDO LOG ENTRY TO ENSURE PAGE    */
/*       ARE MAINTAINED EVEN AFTER A SYSTEM CRASH.                  */
/* ---------------------------------------------------------------- */
    ndbrequire(regPagePtr->page_state == ZTH_MM_FREE);
    regFragPtr->thFreeFirst = regPagePtr->next_page;
    regPagePtr->next_page = RNIL;
    regPagePtr->page_state = ZTH_MM_FULL;
  }
  
  return idx;
}//Dbtup::getThAtPage()


void Dbtup::free_fix_rec(Fragrecord* regFragPtr,
			 Tablerec* regTabPtr,
			 Local_key* key,
			 Fix_page* regPagePtr)
{
  Uint32 free= regPagePtr->free_record(key->m_page_idx);
  
  if(free == 1)
  {
    ljam();
    ndbrequire(regPagePtr->page_state == ZTH_MM_FULL);
    regPagePtr->page_state = ZTH_MM_FREE;
    regPagePtr->next_page= regFragPtr->thFreeFirst;
    regFragPtr->thFreeFirst = key->m_page_no;
  } 
}//Dbtup::freeTh()

