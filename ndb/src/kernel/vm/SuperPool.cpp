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

#include <ndb_global.h>
#include "SuperPool.hpp"

// SuperPool

SuperPool::SuperPool(Uint32 pageSize, Uint32 pageBits) :
  m_pageSize(SP_ALIGN_SIZE(pageSize, SP_ALIGN)),
  m_pageBits(pageBits),
  m_memRoot(0),
  m_pageEnt(0),
  m_typeCheck(0),
  m_typeSeq(0),
  m_pageList(),
  m_totalSize(0),
  m_initSize(0),
  m_incrSize(0),
  m_maxSize(0)
{
  assert(5 <= pageBits <= 30);
}

bool
SuperPool::init()
{
  return true;
}

SuperPool::~SuperPool()
{
}

SuperPool::PageEnt::PageEnt() :
  m_pageType(0),
  m_freeRecI(RNIL),
  m_useCount(0),
  m_nextPageI(RNIL),
  m_prevPageI(RNIL)
{
}

SuperPool::PageList::PageList() :
  m_headPageI(RNIL),
  m_tailPageI(RNIL),
  m_pageCount(0)
{
}

SuperPool::PageList::PageList(PtrI pageI) :
  m_headPageI(pageI),
  m_tailPageI(pageI),
  m_pageCount(1)
{
}

SuperPool::RecInfo::RecInfo(Uint32 recType, Uint32 recSize) :
  m_recType(recType),
  m_recSize(recSize),
  m_maxUseCount(0),
  m_currPageI(RNIL),
  m_currFreeRecI(RNIL),
  m_currUseCount(0),
  m_totalUseCount(0),
  m_totalRecCount(0),
  m_freeList(),
  m_activeList(),
  m_fullList()
{
}

SuperPool::PtrI
SuperPool::getPageI(void* pageP)
{
  const Uint32 pageSize = m_pageSize;
  const Uint32 pageBits = m_pageBits;
  const Uint32 recBits = 32 - pageBits;
  void* const memRoot = m_memRoot;
  assert(pageP == SP_ALIGN_PTR(pageP, memRoot, pageSize));
  my_ptrdiff_t ipL = ((Uint8*)pageP - (Uint8*)memRoot) / pageSize;
  Int32 ip = (Int32)ipL;
  Int32 lim = 1 << (pageBits - 1);
  assert(ip == ipL && -lim <= ip && ip < lim && ip != -1);
  PtrI pageI = ip << recBits;
  assert(pageP == getPageP(pageI));
  return pageI;
}

void
SuperPool::movePages(PageList& pl1, PageList& pl2)
{
  const Uint32 recBits = 32 - m_pageBits;
  if (pl1.m_pageCount != 0) {
    if (pl2.m_pageCount != 0) {
      PtrI pageI1 = pl1.m_tailPageI;
      PtrI pageI2 = pl2.m_headPageI;
      PageEnt& pe1 = getPageEnt(pageI1);
      PageEnt& pe2 = getPageEnt(pageI2);
      pe1.m_nextPageI = pageI2;
      pe2.m_prevPageI = pageI1;
      pl1.m_pageCount += pl2.m_pageCount;
    }
  } else {
    pl1 = pl2;
  }
  pl2.m_headPageI = pl2.m_tailPageI = RNIL;
  pl2.m_pageCount = 0;
}

void
SuperPool::addHeadPage(PageList& pl, PtrI pageI)
{
  PageList pl2(pageI);
  movePages(pl2, pl);
  pl = pl2;
}

void
SuperPool::addTailPage(PageList& pl, PtrI pageI)
{
  PageList pl2(pageI);
  movePages(pl, pl2);
}

void
SuperPool::removePage(PageList& pl, PtrI pageI)
{
  PageEnt& pe = getPageEnt(pageI);
  PtrI pageI1 = pe.m_prevPageI;
  PtrI pageI2 = pe.m_nextPageI;
  if (pageI1 != RNIL) {
    PageEnt& pe1 = getPageEnt(pageI1);
    pe1.m_nextPageI = pageI2;
    if (pageI2 != RNIL) {
      PageEnt& pe2 = getPageEnt(pageI2);
      pe2.m_prevPageI = pageI1;
    } else {
      pl.m_tailPageI = pageI1;
    }
  } else {
    if (pageI2 != RNIL) {
      PageEnt& pe2 = getPageEnt(pageI2);
      pe2.m_prevPageI = pageI1;
      pl.m_headPageI = pageI2;
    } else {
      pl.m_headPageI = pl.m_tailPageI = RNIL;
    }
  }
  pe.m_prevPageI = pe.m_nextPageI = RNIL;
  assert(pl.m_pageCount != 0);
  pl.m_pageCount--;
}

void
SuperPool::setCurrPage(RecInfo& ri, PtrI newPageI)
{
  PtrI oldPageI = ri.m_currPageI;
  if (oldPageI != RNIL) {
    // copy from cache
    PageEnt& pe = getPageEnt(oldPageI);
    pe.m_freeRecI = ri.m_currFreeRecI;
    pe.m_useCount = ri.m_currUseCount;
    // add to right list according to "pp2" policy
    if (pe.m_useCount == 0) {
      pe.m_pageType = 0;
      addHeadPage(m_pageList, oldPageI);
      ri.m_totalRecCount -= ri.m_maxUseCount;
    } else if (pe.m_useCount < ri.m_maxUseCount) {
      addHeadPage(ri.m_activeList, oldPageI);
    } else {
      addHeadPage(ri.m_fullList, oldPageI);
    }
  }
  if (newPageI != RNIL) {
    PageEnt& pe = getPageEnt(newPageI);
    // copy to cache
    ri.m_currPageI = newPageI;
    ri.m_currFreeRecI = pe.m_freeRecI;
    ri.m_currUseCount = pe.m_useCount;
    // remove from right list
    if (pe.m_useCount == 0) {
      removePage(ri.m_freeList, newPageI);
    } else if (pe.m_useCount < ri.m_maxUseCount) {
      removePage(ri.m_activeList, newPageI);
    } else {
      removePage(ri.m_fullList, newPageI);
    }
  } else {
    ri.m_currPageI = RNIL;
    ri.m_currFreeRecI = RNIL;
    ri.m_currUseCount = 0;
  }
}

bool
SuperPool::getAvailPage(RecInfo& ri)
{
  PtrI pageI;
  if ((pageI = ri.m_activeList.m_headPageI) != RNIL ||
      (pageI = ri.m_freeList.m_headPageI) != RNIL ||
      (pageI = getFreePage(ri)) != RNIL) {
    setCurrPage(ri, pageI);
    return true;
  }
  return false;
}

SuperPool::PtrI
SuperPool::getFreePage(RecInfo& ri)
{
  PtrI pageI;
  if (m_pageList.m_pageCount != 0) {
    pageI = m_pageList.m_headPageI;
    removePage(m_pageList, pageI);
  } else {
    pageI = getNewPage();
    if (pageI == RNIL)
      return RNIL;
  }
  void* pageP = getPageP(pageI);
  // set up free record list
  Uint32 maxUseCount = ri.m_maxUseCount;
  Uint32 recSize = ri.m_recSize;
  void* recP = (Uint8*)pageP;
  Uint32 irNext = 1;
  while (irNext < maxUseCount) {
    *(Uint32*)recP = pageI | irNext;
    recP = (Uint8*)recP + recSize;
    irNext++;
  }
  *(Uint32*)recP = RNIL;
  // add to total record count
  ri.m_totalRecCount += maxUseCount;
  // set up new page entry
  PageEnt& pe = getPageEnt(pageI);
  new (&pe) PageEnt();
  pe.m_pageType = ri.m_recType;
  pe.m_freeRecI = pageI | 0;
  pe.m_useCount = 0;
  // set type check bits
  setCheckBits(pageI, ri.m_recType);
  // add to record pool free list
  addHeadPage(ri.m_freeList, pageI);
  return pageI;
}

void
SuperPool::setSizes(size_t initSize, size_t incrSize, size_t maxSize)
{
  const Uint32 pageSize = m_pageSize;
  m_initSize = SP_ALIGN_SIZE(initSize, pageSize);
  m_incrSize = SP_ALIGN_SIZE(incrSize, pageSize);
  m_maxSize = SP_ALIGN_SIZE(maxSize, pageSize);
}

void
SuperPool::verify(RecInfo& ri)
{
  PageList* plList[3] = { &ri.m_freeList, &ri.m_activeList, &ri.m_fullList };
  for (int i = 0; i < 3; i++) {
    PageList& pl = *plList[i];
    unsigned count = 0;
    PtrI pageI = pl.m_headPageI;
    while (pageI != RNIL) {
      PageEnt& pe = getPageEnt(pageI);
      PtrI pageI1 = pe.m_prevPageI;
      PtrI pageI2 = pe.m_nextPageI;
      if (count == 0) {
        assert(pageI1 == RNIL);
      } else {
        assert(pageI1 != RNIL);
        PageEnt& pe1 = getPageEnt(pageI1);
        assert(pe1.m_nextPageI == pageI);
        if (pageI2 != RNIL) {
          PageEnt& pe2 = getPageEnt(pageI2);
          assert(pe2.m_prevPageI == pageI);
        }
      }
      pageI = pageI2;
      count++;
    }
    assert(pl.m_pageCount == count);
  }
}

// HeapPool

HeapPool::HeapPool(Uint32 pageSize, Uint32 pageBits) :
  SuperPool(pageSize, pageBits),
  m_areaHead(),
  m_currArea(&m_areaHead),
  m_lastArea(&m_areaHead),
  m_mallocPart(4)
{
}

bool
HeapPool::init()
{
  const Uint32 pageBits = m_pageBits;
  if (! SuperPool::init())
    return false;;
  // allocate page entry array
  Uint32 peBytes = (1 << pageBits) * sizeof(PageEnt);
  m_pageEnt = static_cast<PageEnt*>(malloc(peBytes));
  if (m_pageEnt == 0)
    return false;
  memset(m_pageEnt, 0, peBytes);
  // allocate type check array
  Uint32 tcWords = 1 << (pageBits - (5 - SP_CHECK_LOG2));
  m_typeCheck = static_cast<Uint32*>(malloc(tcWords << 2));
  if (m_typeCheck == 0)
    return false;
  memset(m_typeCheck, 0, tcWords << 2);
  // allocate initial data
  assert(m_totalSize == 0);
  if (! allocMoreData(m_initSize))
    return false;
  return true;
}

HeapPool::~HeapPool()
{
  free(m_pageEnt);
  free(m_typeCheck);
  Area* ap;
  while ((ap = m_areaHead.m_nextArea) != 0) {
    m_areaHead.m_nextArea = ap->m_nextArea;
    free(ap->m_memory);
    free(ap);
  }
}

HeapPool::Area::Area() :
  m_nextArea(0),
  m_firstPageI(RNIL),
  m_currPage(0),
  m_numPages(0),
  m_memory(0)
{
}

SuperPool::PtrI
HeapPool::getNewPage()
{
  const Uint32 pageSize = m_pageSize;
  const Uint32 pageBits = m_pageBits;
  const Uint32 recBits= 32 - pageBits;
  Area* ap = m_currArea;
  if (ap->m_currPage == ap->m_numPages) {
    // area is used up
    if (ap->m_nextArea == 0) {
      // todo dynamic increase
      assert(m_incrSize == 0);
      return RNIL;
    }
    ap = m_currArea = ap->m_nextArea;
  }
  assert(ap->m_currPage < ap->m_numPages);
  PtrI pageI = ap->m_firstPageI;
  Int32 ip = (Int32)pageI >> recBits;
  ip += ap->m_currPage;
  pageI = ip << recBits;
  ap->m_currPage++;
  return pageI;
}

bool
HeapPool::allocMoreData(size_t size)
{
  const Uint32 pageSize = m_pageSize;
  const Uint32 pageBits = m_pageBits;
  const Uint32 recBits = 32 - pageBits;
  const Uint32 incrSize = m_incrSize;
  const Uint32 incrPages = incrSize / pageSize;
  const Uint32 mallocPart = m_mallocPart;
  size = SP_ALIGN_SIZE(size, pageSize);
  if (incrSize != 0)
    size = SP_ALIGN_SIZE(size, incrSize);
  Uint32 needPages = size / pageSize;
  while (needPages != 0) {
    Uint32 wantPages = needPages;
    if (incrPages != 0 && wantPages > incrPages)
      wantPages = incrPages;
    Uint32 tryPages = 0;
    void* p1 = 0;
    for (Uint32 i = mallocPart; i > 0 && p1 == 0; i--) {
      // one page is usually wasted due to alignment to memory root
      tryPages = ((wantPages + 1) * i) / mallocPart;
      if (tryPages < 2)
        break;
      p1 = malloc(pageSize * tryPages);
    }
    if (p1 == 0)
      return false;
    if (m_memRoot == 0) {
      // set memory root at first "big" alloc
      // assume malloc header makes later ip = -1 impossible
      m_memRoot = p1;
    }
    void* p2 = SP_ALIGN_PTR(p1, m_memRoot, pageSize);
    Uint32 numPages = tryPages - (p1 != p2);
    my_ptrdiff_t ipL = ((Uint8*)p2 - (Uint8*)m_memRoot) / pageSize;
    Int32 ip = (Int32)ipL;
    Int32 lim = 1 << (pageBits - 1);
    if (! (ip == ipL && -lim <= ip && ip + numPages < lim)) {
      free(p1);
      return false;
    }
    assert(ip != -1);
    PtrI pageI = ip << recBits;
    needPages = (needPages >= numPages ? needPages - numPages : 0);
    m_totalSize += numPages * pageSize;
    // allocate new area
    Area* ap = static_cast<Area*>(malloc(sizeof(Area)));
    if (ap == 0) {
      free(p1);
      return false;
    }
    new (ap) Area();
    ap->m_firstPageI = pageI;
    ap->m_numPages = numPages;
    ap->m_memory = p1;
    m_lastArea->m_nextArea = ap;
    m_lastArea = ap;
  }
  return true;
}
