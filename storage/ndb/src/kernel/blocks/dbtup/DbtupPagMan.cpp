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
#define DBTUP_PAG_MAN_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>

/* ---------------------------------------------------------------- */
// 4) Page Memory Manager (buddy algorithm)
//
// The following data structures in Dbtup is used by the Page Memory
// Manager.
//
// cfreepageList[16]
// Pages with a header
//
// The cfreepageList is 16 free lists. Free list 0 contains chunks of
// pages with 2^0 (=1) pages in each chunk. Free list 1 chunks of 2^1
// (=2) pages in each chunk and so forth upto free list 15 which
// contains chunks of 2^15 (=32768) pages in each chunk.
// The cfreepageList array contains the pointer to the first chunk
// in each of those lists. The lists are doubly linked where the
// first page in each chunk contains the next and previous references
// in position ZPAGE_NEXT_CLUST_POS and ZPAGE_PREV_CLUST_POS in the
// page header.
// In addition the leading page and the last page in each chunk is marked
// with a state (=ZFREE_COMMON) in position ZPAGE_STATE_POS in page
// header. This state indicates that the page is the leading or last page
// in a chunk of free pages. Furthermore the leading and last page is
// also marked with a reference to the leading (=ZPAGE_FIRST_CLUST_POS)
// and the last page (=ZPAGE_LAST_CLUST_POS) in the chunk.
//
// The aim of these data structures is to enable a free area handling of
// free pages based on a buddy algorithm. When allocating pages it is
// performed in chunks of pages and the algorithm tries to make the
// chunks as large as possible.
// This manager is invoked when fragments lack internal page space to
// accomodate all the data they are requested to store. It is also
// invoked when fragments deallocate page space back to the free area.
//
// The following routines are part of the external interface:
// void
// allocConsPages(Uint32  noOfPagesToAllocate, #In
//                Uint32& noOfPagesAllocated,  #Out
//                Uint32& retPageRef)          #Out
// void
// returnCommonArea(Uint32 retPageRef,         #In
//                  Uint32 retNoPages)         #In
//
// allocConsPages tries to allocate noOfPagesToAllocate pages in one chunk.
// If this fails it delivers a chunk as large as possible. It returns the
// i-value of the first page in the chunk delivered, if zero pages returned
// this i-value is undefined. It also returns the size of the chunk actually
// delivered.
// 
// returnCommonArea is used when somebody is returning pages to the free area.
// It is used both from internal routines and external routines.
//
// The following routines are private routines used to support the
// above external interface:
// removeCommonArea()
// insertCommonArea()
// findFreeLeftNeighbours()
// findFreeRightNeighbours()
// Uint32
// nextHigherTwoLog(Uint32 input)
//
// nextHigherTwoLog is a support routine which is a mathematical function with
// an integer as input and an integer as output. It calculates the 2-log of
// (input + 1). If the 2-log of (input + 1) is larger than 15 then the routine
// will return 15. It is part of the external interface since it is also used
// by other similar memory management algorithms.
//
// External dependencies:
// None.
//
// Side Effects:
// Apart from the above mentioned data structures there are no more
// side effects other than through the subroutine parameters in the
// external interface.
//
/* ---------------------------------------------------------------- */

/* ---------------------------------------------------------------- */
/* CALCULATE THE 2-LOG + 1 OF TMP AND PUT RESULT INTO TBITS         */
/* ---------------------------------------------------------------- */
Uint32 Dbtup::nextHigherTwoLog(Uint32 input) 
{
  input = input | (input >> 8);
  input = input | (input >> 4);
  input = input | (input >> 2);
  input = input | (input >> 1);
  Uint32 output = (input & 0x5555) + ((input >> 1) & 0x5555);
  output = (output & 0x3333) + ((output >> 2) & 0x3333);
  output = output + (output >> 4);
  output = (output & 0xf) + ((output >> 8) & 0xf);
  return output;
}//nextHigherTwoLog()

void Dbtup::initializePage() 
{
  cnoOfAllocatedPages = 0;
}//Dbtup::initializePage()

void Dbtup::allocConsPages(Uint32 noOfPagesToAllocate,
                           Uint32& noOfPagesAllocated,
                           Uint32& allocPageRef)
{
  if (noOfPagesToAllocate == 0){ 
    jam();
    noOfPagesAllocated = 0;
    return;
  }//if

  m_ctx.m_mm.alloc_pages(RT_DBTUP_PAGE, &allocPageRef, 
			 &noOfPagesToAllocate, 1);
  cnoOfAllocatedPages += noOfPagesToAllocate;
  noOfPagesAllocated = noOfPagesToAllocate;
  return;
}//allocConsPages()

void Dbtup::returnCommonArea(Uint32 retPageRef, Uint32 retNo) 
{
  m_ctx.m_mm.release_pages(RT_DBTUP_PAGE, retPageRef, retNo);
  cnoOfAllocatedPages -= retNo;
}//Dbtup::returnCommonArea()

