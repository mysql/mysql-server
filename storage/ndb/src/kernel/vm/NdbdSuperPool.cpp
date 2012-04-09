/*
   Copyright (C) 2006, 2008 MySQL AB
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
#include "ndbd_malloc_impl.hpp"
#include "NdbdSuperPool.hpp"

#define PSI (1 << (BMW_2LOG + 2))

struct AllocArea
{
  AllocArea(AllocArea* next);

  Uint16 m_currPage;                        // 2
  Uint16 m_numPages;  // number of pages    // 2
  SuperPool::PtrI m_firstPageI;             // 4
  void* m_memory;     // page-aligned pages // 4/8
  struct AllocArea* m_nextArea;             // 4/8
  // tot 16/24
};

AllocArea::AllocArea(AllocArea* next)
{
  m_nextArea = next;
  m_firstPageI = RNIL;
  m_currPage = m_numPages = 0;
  m_memory = 0;
}

NdbdSuperPool::NdbdSuperPool(class Ndbd_mem_manager & mm,
			     Uint32 pageSize, Uint32 pageBits) :
  SuperPool(pageSize, pageBits),
  m_mm(mm),
  m_currArea(0), m_firstArea(0)
{
  m_memRoot = m_mm.get_memroot();
  
  m_shift = Ndbd_mem_manager::ndb_log2((1 << (BMW_2LOG + 2)) / pageSize) - 1;
  m_add = (1 << m_shift) - 1;
}

NdbdSuperPool::~NdbdSuperPool()
{
  Uint32 cnt = PSI / sizeof(AllocArea);
  AllocArea* ap = m_firstArea;
  while(ap != 0)
  {
    AllocArea * first = ap;
    for(Uint32 i = 0; i<cnt; i++)
    {
      if (ap->m_numPages)
      {
	m_mm.release(ap->m_memory, ap->m_numPages >> m_shift);
      }
      ap = ap->m_nextArea;
    }
    m_mm.release((void*)first, 1);
  }
}

bool
NdbdSuperPool::init_1()
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

static
void
initAllocAreaPage(AllocArea * p1)
{
  AllocArea * ap = p1;
  Uint32 cnt = PSI / sizeof(AllocArea);
  for(Uint32 i = 0; i<cnt; i++, ap++)
  {
    new (ap) AllocArea(ap + 1);
  }

  (p1 + cnt - 1)->m_nextArea = 0;
}

bool
NdbdSuperPool::init_2()
{
  m_memRoot = m_mm.get_memroot();

  Uint32 cnt = 1;
  AllocArea* p1 = (AllocArea*)m_mm.alloc(&cnt, 1);
  if (p1 == 0)
    return false;

  initAllocAreaPage(p1);
  m_currArea = p1;
  m_firstArea = p1;
  return true;
}

SuperPool::PtrI
NdbdSuperPool::getNewPage()
{
  AllocArea* ap = m_currArea;
  Uint32 curr = ap->m_currPage;
  Uint32 cnt = ap->m_numPages;
  if (curr == cnt)
  {
    // area is used up
    if (! (ap = allocMem()))
    {
      abort();
      return RNIL;
    }
    curr = ap->m_currPage;
    cnt = ap->m_numPages;
  }

  assert(curr < cnt);
  PtrI pageI = ap->m_firstPageI;
  Uint32 recBits = m_recBits;
  Int32 ip = ((Int32)pageI >> recBits) + curr;
  pageI = ip << recBits;
  ap->m_currPage = curr + 1;
  return pageI;
}

Uint32
NdbdSuperPool::allocAreaMemory(AllocArea* ap, Uint32 tryPages)
{
  Uint32 cnt = (tryPages + m_add) >> m_shift;
  void* p1 = m_mm.alloc(&cnt, 1);
  if (p1 == 0)
  {
    abort();
    return 0;
  }
  Uint32 pageI = getPageI(p1);
  ap->m_firstPageI = pageI;
  ap->m_currPage = 0;
  ap->m_memory = p1;
  ap->m_numPages = cnt << m_shift;
  return cnt;
}

AllocArea*
NdbdSuperPool::allocArea()
{
  AllocArea * curr = m_currArea;
  AllocArea * next = curr->m_nextArea;
  if (next == 0)
  {
    Uint32 cnt = 1;
    AllocArea* p1 = (AllocArea*)m_mm.alloc(&cnt, 1);
    if (p1 == 0)
      return 0;
    
    initAllocAreaPage(p1);

    m_currArea->m_nextArea = p1;
    return m_currArea = p1;
  }
  else
  {
    m_currArea = m_currArea->m_nextArea;
    return m_currArea;
  }
}

AllocArea*
NdbdSuperPool::allocMem()
{
  // compute number of additional pages needed
  if (m_totPages >= m_maxPages)
  {
    abort();
    return 0;
  }
  Uint32 needPages = (m_totPages == 0 ? m_initPages : m_incrPages);
  
  // add new area
  AllocArea* ap = allocArea();
  if (ap == 0)
  {
    abort();
    return 0;
  }
  
  Uint32 numPages;
  if (!(numPages = allocAreaMemory(ap, needPages)))
  {
    abort();
    return 0;
  }
  
  // update counts
  m_totPages += numPages;
  return ap;
}
