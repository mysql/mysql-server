/*
   Copyright (C) 2005, 2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#include <ndb_global.h>
#include "SuperPool.hpp"

#define SP_ALIGN(sz, al) (((sz) + (al) - 1) & ~((al) - 1))

// This is used for m_freeRecI when there is no record pool page.
#define NNIL    0xffffffff

// SuperPool

SuperPool::SuperPool(Uint32 pageSize, Uint32 pageBits) :
  m_pageSize(pageSize),
  m_pageBits(pageBits),
  m_recBits(32 - m_pageBits),
  m_recMask((1 << m_recBits) - 1),
  m_memRoot(0),
  m_pageEnt(0),
  m_pageType(0),
  m_freeList(),
  m_initPages(0),
  m_incrPages(0),
  m_maxPages(0),
  m_totPages(0),
  m_typeCount(0),
  m_groupMinPct(0),
  m_groupMinPages(0),
  m_groupTotPages(0)
{
  assert(m_pageSize != 0 && (m_pageSize & (m_pageSize - 1)) == 0);
  assert(m_pageBits <= 30);
}

SuperPool::~SuperPool()
{
}

SuperPool::PageEnt::PageEnt() :
  m_pageType(0),
  m_useCount(0),
  m_freeRecI(NNIL),
  m_nextPageI(RNIL),
  m_prevPageI(RNIL)
{
}

// page list routines

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
  assert(pageI != RNIL);
}

void
SuperPool::movePages(PageList& pl1, PageList& pl2)
{
  PtrI pageI1 = pl1.m_tailPageI;
  PtrI pageI2 = pl2.m_headPageI;
  if (pl1.m_pageCount != 0) {
    assert(pageI1 != RNIL);
    if (pl2.m_pageCount != 0) {
      assert(pageI2 != RNIL);
      PageEnt& pe1 = getPageEnt(pageI1);
      PageEnt& pe2 = getPageEnt(pageI2);
      pe1.m_nextPageI = pageI2;
      pe2.m_prevPageI = pageI1;
      pl1.m_tailPageI = pl2.m_tailPageI;
      pl1.m_pageCount += pl2.m_pageCount;
    } else {
      assert(pageI2 == RNIL);
    }
  } else {
    assert(pageI1 == RNIL);
    pl1 = pl2;
  }
  pl2.m_headPageI = pl2.m_tailPageI = RNIL;
  pl2.m_pageCount = 0;
}

void
SuperPool::addHeadPage(PageList& pl, PtrI pageI)
{
  assert(pageI != RNIL);
  PageEnt& pe = getPageEnt(pageI);
  assert(pe.m_nextPageI == RNIL & pe.m_prevPageI == RNIL);
  PageList pl2(pageI);
  movePages(pl2, pl);
  pl = pl2;
}

void
SuperPool::addTailPage(PageList& pl, PtrI pageI)
{
  assert(pageI != RNIL);
  PageEnt& pe = getPageEnt(pageI);
  assert(pe.m_nextPageI == RNIL & pe.m_prevPageI == RNIL);
  PageList pl2(pageI);
  movePages(pl, pl2);
}

void
SuperPool::removePage(PageList& pl, PtrI pageI)
{
  assert(pageI != RNIL);
  PageEnt& pe = getPageEnt(pageI);
  if (pe.m_nextPageI != RNIL) {
    assert(pl.m_tailPageI != pageI);
    PageEnt& nextPe = getPageEnt(pe.m_nextPageI);
    nextPe.m_prevPageI = pe.m_prevPageI;
  } else {
    assert(pl.m_tailPageI == pageI);
    pl.m_tailPageI = pe.m_prevPageI;
  }
  if (pe.m_prevPageI != RNIL) {
    assert(pl.m_headPageI != pageI);
    PageEnt& prevPe = getPageEnt(pe.m_prevPageI);
    prevPe.m_nextPageI = pe.m_nextPageI;
  } else {
    assert(pl.m_headPageI == pageI);
    pl.m_headPageI = pe.m_nextPageI;
  }
  pe.m_nextPageI = RNIL;
  pe.m_prevPageI = RNIL;
  assert(pl.m_pageCount != 0);
  pl.m_pageCount--;
}

// reverse mapping

SuperPool::PtrI
SuperPool::getPageI(void* pageP)
{
  Uint32 pageSize = m_pageSize;
  Uint32 pageBits = m_pageBits;
  Uint32 recBits = m_recBits;
  void* memRoot = m_memRoot;
  my_ptrdiff_t ipL = (Uint8*)pageP - (Uint8*)memRoot;
  assert(ipL % pageSize == 0);
  ipL /= (Int32)pageSize;
  Int32 ip = (Int32)ipL;
  Int32 lim = 1 << (pageBits - 1);
  if (! (ip == ipL && -lim <= ip && ip < lim && ip != -1)) {
    // page was too distant from memory root
    return RNIL;
  }
  PtrI pageI = ip << recBits;
  assert(pageP == getPageP(pageI));
  return pageI;
}

// record pool

SuperPool::RecInfo::RecInfo(GroupPool& gp, Uint32 recSize) :
  m_groupPool(gp),
  m_recSize(recSize),
  m_recType(0),
  m_maxPerPage(0),
  m_freeRecI(NNIL),
  m_useCount(0),
  m_pageList(),
  m_hyX(1),
  m_hyY(2)
{
  SuperPool& sp = gp.m_superPool;
  m_recType = (sp.m_typeCount++ << 1) | 1;
  assert(m_recSize == SP_ALIGN(m_recSize, sizeof(Uint32)));
  { // compute max records per page
    Uint32 n1 = sp.m_pageSize / m_recSize;
    Uint32 b2 = (sp.m_recBits < 16 ? sp.m_recBits : 16);
    Uint32 n2 = (1 << b2) - 1;  // last is reserved
    m_maxPerPage = (n1 < n2 ? n1 : n2);
    assert(m_maxPerPage != 0);
  }
}

Uint32
SuperPool::getFreeCount(RecInfo& ri, PtrI recI)
{
  Uint32 n = 0;
  Uint32 recMask = m_recMask;
  Uint32 loopRecI = recI;
  while ((loopRecI & recMask) != recMask) {
    n++;
    void* loopRecP = getRecP(loopRecI, ri);
    loopRecI = *(Uint32*)loopRecP;
  }
  assert(n == (Uint16)n);
  return n;
}

Uint32
SuperPool::getRecPageCount(RecInfo& ri)
{
  Uint32 n = 0;
  for (Uint32 k = 0; k <= 2; k++)
    n += ri.m_pageList[k].m_pageCount;
  if (ri.m_freeRecI != NNIL)
    n += 1;
  return n;
}

Uint32
SuperPool::getRecTotCount(RecInfo& ri)
{
  return ri.m_maxPerPage * getRecPageCount(ri);
}

Uint32
SuperPool::getRecUseCount(RecInfo& ri)
{
  Uint32 n = ri.m_useCount;
  // current page does not keep count
  if (ri.m_freeRecI != NNIL) {
    Uint32 maxPerPage = ri.m_maxPerPage;
    Uint32 freeCount = getFreeCount(ri, ri.m_freeRecI);
    assert(maxPerPage >= freeCount);
    n += maxPerPage - freeCount;
  }
  return n;
}

// current page

Uint32
SuperPool::getRecPageList(RecInfo& ri, PageEnt& pe)
{
  if (pe.m_useCount == 0)
    return 0;
  if (pe.m_useCount < ri.m_maxPerPage)
    return 1;
  if (pe.m_useCount == ri.m_maxPerPage)
    return 2;
  assert(false);
  return ~(Uint32)0;
}

void
SuperPool::addCurrPage(RecInfo& ri, PtrI pageI)
{
  PageEnt& pe = getPageEnt(pageI);
  ri.m_freeRecI = pe.m_freeRecI;
  // remove from right list
  Uint32 k = getRecPageList(ri, pe);
  assert(k != 2);
  removePage(ri.m_pageList[k], pageI);
  assert(ri.m_useCount >= pe.m_useCount);
  ri.m_useCount -= pe.m_useCount;
}

void
SuperPool::removeCurrPage(RecInfo& ri)
{
  Uint32 recMask = m_recMask;
  PtrI pageI = ri.m_freeRecI & ~ m_recMask;
  // update page entry
  PageEnt& pe = getPageEnt(pageI);
  pe.m_freeRecI = ri.m_freeRecI;
  Uint32 maxPerPage = ri.m_maxPerPage;
  Uint32 freeCount = getFreeCount(ri, pe.m_freeRecI);
  assert(maxPerPage >= freeCount);
  pe.m_useCount = maxPerPage - freeCount;
  // add to right list
  Uint32 k = getRecPageList(ri, pe);
  addHeadPage(ri.m_pageList[k], pageI);
  ri.m_useCount += pe.m_useCount;
  ri.m_freeRecI = NNIL;
  if (k == 0) {
    freeRecPages(ri);
  }
}

// page allocation

bool
SuperPool::getAvailPage(RecInfo& ri)
{
  PtrI pageI;
  if ((pageI = ri.m_pageList[1].m_headPageI) != RNIL ||
      (pageI = ri.m_pageList[0].m_headPageI) != RNIL ||
      (pageI = getFreePage(ri)) != RNIL) {
    // the page is in record pool now
    if (ri.m_freeRecI != NNIL)
      removeCurrPage(ri);
    addCurrPage(ri, pageI);
    return true;
  }
  return false;
}

SuperPool::PtrI
SuperPool::getFreePage(RecInfo& ri)
{
  GroupPool& gp = ri.m_groupPool;
  PtrI pageI;
  if ((pageI = getFreePage(gp)) != RNIL) {
    initFreePage(ri, pageI);
    addHeadPage(ri.m_pageList[0], pageI);
    return pageI;
  }
  return RNIL;
}

SuperPool::PtrI
SuperPool::getFreePage(GroupPool& gp)
{
  PtrI pageI;
  if ((pageI = gp.m_freeList.m_headPageI) != RNIL) {
    removePage(gp.m_freeList, pageI);
    return pageI;
  }
  if (gp.m_totPages < getMaxPages(gp) &&
      (pageI = getFreePage()) != RNIL) {
    gp.m_totPages++;
    return pageI;
  }
  return RNIL;
}

SuperPool::PtrI
SuperPool::getFreePage()
{
  PtrI pageI;
  if ((pageI = m_freeList.m_headPageI) != RNIL) {
    removePage(m_freeList, pageI);
    return pageI;
  }
  if ((pageI = getNewPage()) != RNIL) {
    return pageI;
  }
  return RNIL;
}

void
SuperPool::initFreePage(RecInfo& ri, PtrI pageI)
{
  void* pageP = getPageP(pageI);
  // set up free record list
  Uint32 num = ri.m_maxPerPage;
  Uint32 recSize = ri.m_recSize;
  void* recP = (Uint8*)pageP;
  Uint32 irNext = 1;
  while (irNext < num) {
    *(Uint32*)recP = pageI | irNext;
    recP = (Uint8*)recP + recSize;
    irNext++;
  }
  // terminator has all recBits set
  *(Uint32*)recP = pageI | m_recMask;
  // set up new page entry
  PageEnt& pe = getPageEnt(pageI);
  new (&pe) PageEnt();
  pe.m_pageType = ri.m_recType;
  pe.m_freeRecI = pageI | 0;
  pe.m_useCount = 0;
  // set type check byte
  Uint32 ip = pageI >> m_recBits;
  m_pageType[ip] = (ri.m_recType & 0xFF);
}

// release

void
SuperPool::releaseNotCurrent(RecInfo& ri, PtrI recI)
{
  PageEnt& pe = getPageEnt(recI);
  void* recP = getRecP(recI, ri);
  *(Uint32*)recP = pe.m_freeRecI;
  pe.m_freeRecI = recI;
  PtrI pageI = recI & ~ m_recMask;
  Uint32 maxPerPage = ri.m_maxPerPage;
  // move to right list
  Uint32 k1 = getRecPageList(ri, pe);
  assert(pe.m_useCount != 0);
  pe.m_useCount--;
  Uint32 k2 = getRecPageList(ri, pe);
  if (k1 != k2) {
    removePage(ri.m_pageList[k1], pageI);
    addHeadPage(ri.m_pageList[k2], pageI);
    if (k2 == 0) {
      freeRecPages(ri);
    }
  }
  assert(ri.m_useCount != 0);
  ri.m_useCount--;
}

void
SuperPool::freeRecPages(RecInfo& ri)
{
  // ignore current page
  Uint32 useCount = ri.m_useCount;
  Uint32 totCount = 0;
  for (uint32 k = 0; k <= 2; k++)
    totCount += ri.m_pageList[k].m_pageCount;
  totCount *= ri.m_maxPerPage;
  assert(totCount >= useCount);
  if ((totCount - useCount) * ri.m_hyY < useCount * ri.m_hyX)
    return;
  // free all free pages
  GroupPool& gp = ri.m_groupPool;
  Uint32 minPages = getMinPages(gp);
  PageList& pl = ri.m_pageList[0];
  while (pl.m_pageCount != 0) {
    PtrI pageI = pl.m_headPageI;
    removePage(pl, pageI);
    PageEnt& pe = getPageEnt(pageI);
    pe.m_pageType = 0;
    pe.m_freeRecI = NNIL;
    Uint32 ip = pageI >> m_recBits;
    m_pageType[ip] = 0;
    if (gp.m_totPages <= minPages) {
      addHeadPage(gp.m_freeList, pageI);
    } else {
      // return excess to super pool
     addHeadPage(m_freeList, pageI);
     assert(gp.m_totPages != 0);
     gp.m_totPages--;
    }
  }
}

void
SuperPool::freeAllRecPages(RecInfo& ri, bool force)
{
  GroupPool& gp = ri.m_groupPool;
  if (ri.m_freeRecI != NNIL)
    removeCurrPage(ri);
  assert(force || ri.m_useCount == 0);
  for (Uint32 k = 0; k <= 2; k++)
    movePages(gp.m_freeList, ri.m_pageList[k]);
}

// size parameters

void
SuperPool::setInitPages(Uint32 initPages)
{
  m_initPages = initPages;
}

void
SuperPool::setIncrPages(Uint32 incrPages)
{
  m_incrPages = incrPages;
}

void
SuperPool::setMaxPages(Uint32 maxPages)
{
  m_maxPages = maxPages;
}

Uint32
SuperPool::getGpMinPages()
{
  Uint32 minPages = (m_groupMinPct * m_totPages) / 100;
  if (minPages < m_groupMinPages)
    minPages = m_groupMinPages;
  return minPages;
}

Uint32
SuperPool::getMinPages(GroupPool& gp)
{
  Uint32 minPages = (gp.m_minPct * m_totPages) / 100;
  if (minPages < gp.m_minPages)
    minPages = gp.m_minPages;
  return minPages;
}

Uint32
SuperPool::getMaxPages(GroupPool& gp)
{
  Uint32 n1 = getGpMinPages();
  Uint32 n2 = getMinPages(gp);
  assert(n1 >= n2);
  // pages reserved by other groups
  Uint32 n3 = n1 - n2;
  // rest can be claimed
  Uint32 n4 = (m_totPages >= n3 ? m_totPages - n3 : 0);
  return n4;
}

// debug

void
SuperPool::verify(RecInfo& ri)
{
  GroupPool& gp = ri.m_groupPool;
  verifyPageList(m_freeList);
  verifyPageList(gp.m_freeList);
  for (Uint32 k = 0; k <= 2; k++) {
    PageList& pl = ri.m_pageList[k];
    verifyPageList(pl);
    PtrI pageI = pl.m_headPageI;
    while (pageI != RNIL) {
      PageEnt& pe = getPageEnt(pageI);
      assert(pe.m_pageType == ri.m_recType);
      Uint32 maxPerPage = ri.m_maxPerPage;
      Uint32 freeCount = getFreeCount(ri, pe.m_freeRecI);
      assert(maxPerPage >= freeCount);
      Uint32 useCount = maxPerPage - freeCount;
      assert(pe.m_useCount == useCount);
      assert(k != 0 || useCount == 0);
      assert(k != 1 || (useCount != 0 && freeCount != 0));
      assert(k != 2 || freeCount == 0);
      pageI = pe.m_nextPageI;
    }
  }
}

void
SuperPool::verifyPageList(PageList& pl)
{
  Uint32 count = 0;
  PtrI pageI = pl.m_headPageI;
  while (pageI != RNIL) {
    PageEnt& pe = getPageEnt(pageI);
    if (pe.m_prevPageI == RNIL) {
      assert(count == 0);
    } else {
      PageEnt& prevPe = getPageEnt(pe.m_prevPageI);
      assert(prevPe.m_nextPageI == pageI);
    }
    if (pe.m_nextPageI == RNIL) {
      assert(pl.m_tailPageI == pageI);
    } else {
      PageEnt& nextPe = getPageEnt(pe.m_nextPageI);
      assert(nextPe.m_prevPageI == pageI);
    }
    if (pe.m_pageType != 0) {
      assert(pe.m_freeRecI != NNIL);
      PageEnt& pe2 = getPageEnt(pe.m_freeRecI);
      assert(&pe == &pe2);
    } else {
      assert(pe.m_freeRecI == NNIL);
    }
    pageI = pe.m_nextPageI;
    count++;
  }
  assert(pl.m_pageCount == count);
}

// GroupPool

GroupPool::GroupPool(SuperPool& sp) :
  m_superPool(sp),
  m_minPct(0),
  m_minPages(0),
  m_totPages(0),
  m_freeList()
{
}

GroupPool::~GroupPool()
{
}

void
GroupPool::setMinPct(Uint32 minPct)
{
  SuperPool& sp = m_superPool;
  // subtract any previous value
  assert(sp.m_groupMinPct >= m_minPct);
  sp.m_groupMinPct -= m_minPct;
  // add new value
  sp.m_groupMinPct += minPct;
  m_minPct = minPct;
}

void
GroupPool::setMinPages(Uint32 minPages)
{
  SuperPool& sp = m_superPool;
  // subtract any previous value
  assert(sp.m_groupMinPages >= m_minPages);
  sp.m_groupMinPages -= m_minPages;
  // add new value
  sp.m_groupMinPages += minPages;
  m_minPages = minPages;
}

// HeapPool

HeapPool::HeapPool(Uint32 pageSize, Uint32 pageBits) :
  SuperPool(pageSize, pageBits),
  m_areaHead(),
  m_currArea(&m_areaHead),
  m_lastArea(&m_areaHead)
{
}

HeapPool::~HeapPool()
{
  free(m_pageEnt);
  free(m_pageType);
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
  m_memory(0),
  m_pages(0),
  m_numPages(0)
{
}

SuperPool::PtrI
HeapPool::getNewPage()
{
  Area* ap = m_currArea;
  if (ap->m_currPage == ap->m_numPages) {
    // area is used up
    if (ap->m_nextArea == 0) {
      if (! allocMemoryImpl())
        return RNIL;
    }
    ap = m_currArea = ap->m_nextArea;
    assert(ap != 0);
  }
  assert(ap->m_currPage < ap->m_numPages);
  PtrI pageI = ap->m_firstPageI;
  Uint32 recBits = m_recBits;
  Int32 ip = (Int32)pageI >> recBits;
  ip += ap->m_currPage;
  pageI = ip << recBits;
  ap->m_currPage++;
  return pageI;
}

bool
HeapPool::allocInit()
{
  Uint32 pageCount = (1 << m_pageBits);
  if (m_pageEnt == 0) {
    // allocate page entry array
    Uint32 bytes = pageCount * sizeof(PageEnt);
    m_pageEnt = static_cast<PageEnt*>(malloc(bytes));
    if (m_pageEnt == 0)
      return false;
    for (Uint32 i = 0; i < pageCount; i++)
      new (&m_pageEnt[i]) PageEnt();
  }
  if (m_pageType == 0) {
    // allocate type check array
    Uint32 bytes = pageCount;
    m_pageType = static_cast<Uint8*>(malloc(bytes));
    if (m_pageType == 0)
      return false;
    memset(m_pageType, 0, bytes);
  }
  return true;
}

bool
HeapPool::allocArea(Area* ap, Uint32 tryPages)
{
  Uint32 pageSize = m_pageSize;
  // one page is usually lost due to alignment
  Uint8* p1 = (Uint8*)malloc(pageSize * (tryPages + 1));
  if (p1 == 0)
    return false;
  // align
  UintPtr n1 = (UintPtr)p1;
  UintPtr n2 = SP_ALIGN(n1, (UintPtr)pageSize);
  Uint8* p2 = p1 + (n2 - n1);
  assert(p2 >= p1 && p2 - p1 < pageSize && (UintPtr)p2 % pageSize == 0);
  // set memory root to first allocated page
  if (m_memRoot == 0)
    m_memRoot = p2;
  // convert to i-value
  Uint32 pageI = getPageI(p2);
  ap->m_firstPageI = pageI;
  ap->m_currPage = 0;
  ap->m_memory = p1;
  ap->m_pages = p2;
  ap->m_numPages = tryPages + (p1 == p2);
  return true;
}

bool
HeapPool::allocMemoryImpl()
{
  if (! allocInit())
    return false;
  // compute number of additional pages needed
  if (m_maxPages <= m_totPages)
    return false;
  Uint32 needPages = (m_totPages == 0 ? m_initPages : m_incrPages);
  if (needPages > m_maxPages - m_totPages)
    needPages = m_maxPages - m_totPages;
  while (needPages != 0) {
    // add new area
    Area* ap = static_cast<Area*>(malloc(sizeof(Area)));
    if (ap == 0)
      return false;
    new (ap) Area();
    m_lastArea->m_nextArea = ap;
    m_lastArea = ap;
    // initial malloc is done in m_incrPages pieces
    Uint32 wantPages = needPages;
    if (m_incrPages != 0 && wantPages > m_incrPages)
      wantPages = m_incrPages;
    Uint32 tryPages = wantPages;
    while (tryPages != 0) {
      if (allocArea(ap, tryPages))
        break;
      tryPages /= 2;
    }
    if (tryPages == 0)
      return false;
    // update counts
    Uint32 numPages = ap->m_numPages;
    m_totPages += numPages;
    needPages = (needPages > numPages ? needPages - numPages : 0);
  }
  return true;
}
