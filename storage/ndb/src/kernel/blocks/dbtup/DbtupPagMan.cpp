/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#define DBTUP_C
#define DBTUP_PAG_MAN_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <dblqh/Dblqh.hpp>

#define JAM_FILE_ID 407


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
// (=2) pages in each chunk and so forth up to free list 15 which
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
// accommodate all the data they are requested to store. It is also
// invoked when fragments deallocate page space back to the free area.
//
// The following routines are part of the external interface:
// void
// allocConsPages(EmulatedJamBuffer *jamBuff   #In/out
//                Uint32  noOfPagesToAllocate, #In
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
}//Dbtup::initializePage()

void Dbtup::allocConsPages(EmulatedJamBuffer* jamBuf,
                           Uint32 noOfPagesToAllocate,
                           Uint32& noOfPagesAllocated,
                           Uint32& allocPageRef)
{
  if (noOfPagesToAllocate == 0){ 
    thrjam(jamBuf);
    noOfPagesAllocated = 0;
    return;
  }//if

  if (noOfPagesToAllocate == 1)
  {
    void* p = m_ctx.m_mm.alloc_page(RT_DBTUP_PAGE,
                                    &allocPageRef,
                                    Ndbd_mem_manager::NDB_ZONE_LE_30);
    if (p != NULL)
    {
      noOfPagesAllocated = 1;
    }
    else
    {
      noOfPagesAllocated = 0;
    }
  }
  else
  {
#ifndef VM_TRACE
    ndbrequire(noOfPagesToAllocate == 1);
#else
    /* For DUMP_STATE_ORD 1211, 1212, and, 1213 */
    noOfPagesAllocated = noOfPagesToAllocate;
    m_ctx.m_mm.alloc_pages(RT_DBTUP_PAGE,
                           &allocPageRef,
                           &noOfPagesAllocated,
                           1);
#endif
  }
  if(noOfPagesAllocated == 0 && c_allow_alloc_spare_page)
  {
    void* p = m_ctx.m_mm.alloc_spare_page(RT_DBTUP_PAGE,
                                          &allocPageRef,
                                          Ndbd_mem_manager::NDB_ZONE_LE_30);
    if (p != NULL)
    {
      noOfPagesAllocated = 1;
    }
  }

  // Count number of allocated pages
  update_pages_allocated(noOfPagesAllocated);

  return;
}//allocConsPages()

void Dbtup::returnCommonArea(Uint32 retPageRef, Uint32 retNo)
{
  m_ctx.m_mm.release_pages(RT_DBTUP_PAGE, retPageRef, retNo);

  // Count number of allocated pages
  update_pages_allocated(-retNo);
}//Dbtup::returnCommonArea()

bool Dbtup::returnCommonArea_for_reuse(Uint32 retPageRef, Uint32 retNo)
{
  if (!m_ctx.m_mm.give_up_pages(RT_DBTUP_PAGE, retNo))
  {
    return false;
  }

  // Count number of allocated pages
  update_pages_allocated(-retNo);
  return true;
}

void
Dbtup::update_pages_allocated(int retNo)
{
  /**
   * In normal operation mode we only update m_pages_allocated
   * and m_pages_allocated_max from the LDM thread and this
   * requires no protection.
   * However during restore operations we can update this from
   * both recover threads and LDM threads and thus we need to
   * protect those changes with a mutex.
   *
   * Query threads should not be used here, only recover threads.
   * When restore phase is done we no longer need to use a mutex.
   */
  bool lock_flag = false;
  Dblqh *lqh_block;
  Dbtup *tup_block;
  if (m_is_query_block)
  {
    Uint32 instanceNo = c_lqh->m_current_ldm_instance;
    ndbrequire(instanceNo != 0);
    tup_block = (Dbtup*) globalData.getBlock(DBTUP, instanceNo);
    lqh_block = (Dblqh*) globalData.getBlock(DBLQH, instanceNo);
    ndbrequire(!lqh_block->is_restore_phase_done());
    ndbrequire(c_lqh->m_is_recover_block);
    lock_flag = true;
  }
  else
  {
    lqh_block = c_lqh;
    tup_block = this;
    if (!c_lqh->is_restore_phase_done() &&
        (globalData.ndbMtRecoverThreads +
         globalData.ndbMtQueryThreads) > 0)
    {
      lock_flag = true;
    }
  }
  if (lock_flag)
  {
    NdbMutex_Lock(lqh_block->m_lock_tup_page_mutex);
  }

  tup_block->m_pages_allocated += retNo;
  if (retNo > 0 &&
      tup_block->m_pages_allocated >
      tup_block->m_pages_allocated_max)
  {
    tup_block->m_pages_allocated_max = tup_block->m_pages_allocated;
  }

  if (lock_flag)
  {
    NdbMutex_Unlock(lqh_block->m_lock_tup_page_mutex);
  }
}

Uint32 Dbtup::get_pages_allocated() const
{
  return m_pages_allocated;
}
