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

#ifndef SUPER_POOL_HPP
#define SUPER_POOL_HPP

#include <ndb_global.h>

#include <pc.hpp>
#include <ErrorReporter.hpp>

#define NDB_SP_VERIFY_LEVEL 1

/*
 * SuperPool - super pool for record pools (abstract class)
 *
 * Documents SuperPool and RecordPool<T>.
 *
 * GENERAL
 *
 * A "super pool" is a shared pool of pages of fixed size.  A "record
 * pool" is a pool of records of fixed size.  One super pool instance is
 * used by any number of record pools to allocate their memory.
 * A special case is a "page pool" where a record is a simple page,
 * possibly smaller than super pool page.
 *
 * A record pool allocates memory in pages.  Thus each used page is
 * associated with one record pool and one record type.  The records on
 * a page form an array starting at start of page.  Thus each record has
 * an index within the page.  Any last partial record which does not fit
 * on the page is disregarded.
 *
 * I-VALUE
 *
 * The old "i-p" principle is kept.  A reference to a super pool page or
 * record is stored as an "i-value" from which the record pointer "p" is
 * computed.  In super pool the i-value is a Uint32 with two parts:
 *
 * - "ip" index of page within super pool (high pageBits)
 * - "ir" index of record within page (low recBits)
 *
 * The translation between "ip" and page address is described in next
 * section.  Once page address is known, the record address is found
 * from "ir" in the obvious way.
 *
 * The main advantage with i-value is that it can be verified.  The
 * level of verification depends on compile type (release, debug).
 *
 * - "v0" minimal sanity check
 * - "v1" check record type matches page type, see below
 * - "v2" check record is in use (not yet implemented)
 *
 * Another advantage of a 32-bit i-value is that it extends the space of
 * 32-bit addressable records on a 64-bit platform.
 *
 * RNIL is 0xffffff00 and indicates NULL i-value.  To avoid hitting RNIL
 * it is required that pageBits <= 30 and that the maximum value of the
 * range (2^pageBits-1) is not used.
 *
 * MEMORY ROOT
 *
 * This super pool requires a "memory root" i.e. a memory address such
 * that the index of a page "ip" satisfies
 *
 *   page address = memory root + (signed)ip * page size
 *
 * This is possible on most platforms, provided that the memory root and
 * all pages are either on the heap or on the stack, in order to keep
 * the size of "ip" reasonably small.
 *
 * The cast (signed)ip is done as integer of pageBits bits.  "ip" has
 * same sign bit as i-value "i" so (signed)ip = (Int32)i >> recBits.
 * The RNIL restriction can be expressed as (signed)ip != -1.
 *
 * PAGE ENTRIES
 *
 * Each super pool page has a "page entry".  It contains:
 *
 * - page type
 * - i-value of first free record on page
 * - page use count, to see if page can be freed
 * - pointers (as i-values) to next and previous page in list
 *
 * Page entry cannot be stored on the page itself since this prevents
 * aligning pages to OS block size and the use of BATs (don't ask) for
 * page pools in NDB.  For now the implementation provides an array of
 * page entries with place for all (2^pageBits) entries.
 *
 * PAGE TYPE
 *
 * Page type is (in principle) unique to the record pool using the super
 * pool.  It is assigned in record pool constructor.  Page type zero
 * means that the page is free i.e. not allocated to a record pool.
 *
 * Each "i-p" conversion checks ("v1") that the record belongs to same
 * pool as the page.  This check is much more common than page or record
 * allocation.  To make it cache effective, there is a separate array of
 * reduced "type bits" (computed from real type).
 *
 * FREE LISTS
 *
 * A record is either used or on the free list of the record pool.
 * A page has a use count i.e. number of used records.  When use count
 * drops to zero the page can be returned to the super pool.  This is
 * not necessarily done at once, or ever.
 *
 * To make freeing pages feasible, the record pool free list has two
 * levels.  There are available pages (some free) and a singly linked
 * free list within the page.  A page allocated to record pool is on one
 * of 4 lists:
 *
 * - free page list (all free, available)
 * - active page list (some free, some used, available)
 * - full page list (none free)
 * - current page (list of 1), see below
 *
 * Some usage types (temporary pools) may never free records.  They pay
 * a small penalty for the extra overhead.
 *
 * RECORD POOL
 *
 * A pool of records which allocates its memory from a super pool
 * instance specified in the constructor.  There are 3 basic operations:
 *
 * - getPtr - translate i-value to pointer-to-record p
 * - seize - allocate record
 * - release - free record
 *
 * CURRENT PAGE
 *
 * getPtr is a fast computation which does not touch the page.  For
 * seize and release there is an optimization:
 *
 * Define "current page" as page of latest seize or release.  Its page
 * entry is cached under record pool instance.  The page is removed from
 * its normal list.  Seize and release on current page are fast and
 * avoid touching the page.  The current page is used until
 *
 * - seize and current page is full
 * - release and the page is not current page
 *
 * Then the real page entry is updated and the page is added to the
 * appropriate list, and a new page is made current.
 *
 * PAGE POLICY
 *
 * Allocating new page to record pool is expensive.  Therefore record
 * pool should not always return empty pages to super pool.  There are
 * two trivial policies, each with problems:
 *
 * - "pp1" never return empty page to super pool
 * - "pp2" always return empty page to super pool
 *
 * This implementation uses "pp2" for now.  A real policy is implemented
 * in next version.
 *
 * OPEN ISSUES AND LIMITATIONS
 *
 * - smarter (virtual) placement of check bits & page entries
 * - should getPtr etc be inlined?  (too much code)
 * - real page policy
 * - other implementations (only HeapPool is done)
 * - super pool list of all record pools, for statistics etc
 * - access by multiple threads is not supported
 */

// align size
#define SP_ALIGN_SIZE(sz, al) \
  (((sz) + (al) - 1) & ~((al) - 1))

// align pointer relative to base
#define SP_ALIGN_PTR(p, base, al) \
  (void*)((Uint8*)(base) + SP_ALIGN_SIZE((Uint8*)(p) - (Uint8*)(base), (al)))

class SuperPool {
public:
  // Type of i-value, used to reference both pages and records.  Page
  // index "ip" occupies the high bits.  The i-value of a page is same
  // as i-value of record 0 on the page.
  typedef Uint32 PtrI;

  // Size and address alignment given as number of bytes (power of 2).
  STATIC_CONST( SP_ALIGN = 8 );

  // Page entry.  Current|y allocated as array of (2^pageBits).
  struct PageEnt {
    PageEnt();
    Uint32 m_pageType;
    Uint32 m_freeRecI;
    Uint32 m_useCount;
    PtrI m_nextPageI;
    PtrI m_prevPageI;
  };

  // Number of bits for cache effective type check given as log of 2.
  // Example: 2 means 4 bits and uses 32k for 2g of 32k pages.
  STATIC_CONST( SP_CHECK_LOG2 = 2 );

  // Doubly-linked list of pages.  There is one free list in super pool
  // and free, active, full list in each record pool.
  struct PageList {
    PageList();
    PageList(PtrI pageI);
    PtrI m_headPageI;
    PtrI m_tailPageI;
    Uint32 m_pageCount;
  };

  // Record pool information.  Each record pool instance contains one.
  struct RecInfo {
    RecInfo(Uint32 recType, Uint32 recSize);
    const Uint32 m_recType;
    const Uint32 m_recSize;
    Uint32 m_maxUseCount;       // could be computed
    Uint32 m_currPageI;         // current page
    Uint32 m_currFreeRecI;
    Uint32 m_currUseCount;
    Uint32 m_totalUseCount;     // total per pool
    Uint32 m_totalRecCount;
    PageList m_freeList;
    PageList m_activeList;
    PageList m_fullList;
  };

  // Constructor.  Gives page size in bytes (excluding page header) and
  // number of bits to use for page index "ip" in i-value.
  SuperPool(Uint32 pageSize, Uint32 pageBits);

  // Initialize.  Must be called after setting sizes or other parameters
  // and before the pool is used.
  virtual bool init();

  // Destructor.
  virtual ~SuperPool() = 0;

  // Translate i-value to page entry.
  PageEnt& getPageEnt(PtrI pageI);

  // Translate i-value to page address.
  void* getPageP(PtrI pageI);

  // Translate page address to i-value (unused).
  PtrI getPageI(void* pageP);

  // Given type, return non-zero reduced type check bits.
  Uint32 makeCheckBits(Uint32 type);

  // Get type check bits from type check array.
  Uint32 getCheckBits(PtrI pageI);

  // Set type check bits in type check array.
  void setCheckBits(PtrI pageI, Uint32 type);

  // Translate i-value to record address.
  void* getRecP(PtrI recI, RecInfo& ri);

  // Move all pages from second list to end of first list.
  void movePages(PageList& pl1, PageList& pl2);

  // Add page to beginning of page list.
  void addHeadPage(PageList& pl, PtrI pageI);

  // Add page to end of page list.
  void addTailPage(PageList& pl, PtrI pageI);

  // Remove any page from page list.
  void removePage(PageList& pl, PtrI pageI);

  // Set current page.  Previous current page is updated and added to
  // appropriate list.
  void setCurrPage(RecInfo& ri, PtrI pageI);

  // Get page with some free records and make it current.  Takes head of
  // active or free list, or else gets free page from super pool.
  bool getAvailPage(RecInfo& ri);

  // Get free page from super pool and add it to record pool free list.
  // This is an expensive subroutine of getAvailPage().
  PtrI getFreePage(RecInfo& ri);

  // Get new free page from the implementation.
  virtual PtrI getNewPage() = 0;

  // Set 3 size parameters, rounded to page size.  If called before
  // init() then init() allocates the initial size.
  void setSizes(size_t initSize = 0, size_t incrSize = 0, size_t maxSize = 0);

  const Uint32 m_pageSize;
  const Uint32 m_pageBits;
  // implementation must set up these pointers
  void* m_memRoot;
  PageEnt* m_pageEnt;
  Uint32* m_typeCheck;
  Uint32 m_typeSeq;
  PageList m_pageList;
  size_t m_totalSize;
  size_t m_initSize;
  size_t m_incrSize;
  size_t m_maxSize;

  // Debugging.
  void verify(RecInfo& ri);
};

inline SuperPool::PageEnt&
SuperPool::getPageEnt(PtrI pageI)
{
  Uint32 ip = pageI >> (32 - m_pageBits);
  return m_pageEnt[ip];
}

inline void*
SuperPool::getPageP(PtrI ptrI)
{
  Int32 ip = (Int32)ptrI >> (32 - m_pageBits);
  my_ptrdiff_t sz = m_pageSize;
  void* pageP = (Uint8*)m_memRoot + ip * sz;
  return pageP;
}

inline Uint32
SuperPool::makeCheckBits(Uint32 type)
{
  Uint32 shift = 1 << SP_CHECK_LOG2;
  Uint32 mask = (1 << shift) - 1;
  return 1 + type % mask;
}

inline Uint32
SuperPool::getCheckBits(PtrI pageI)
{
  Uint32 ip = pageI >> (32 - m_pageBits);
  Uint32 xp = ip >> (5 - SP_CHECK_LOG2);
  Uint32 yp = ip & (1 << (5 - SP_CHECK_LOG2)) - 1;
  Uint32& w = m_typeCheck[xp];
  Uint32 shift = 1 << SP_CHECK_LOG2;
  Uint32 mask = (1 << shift) - 1;
  // get
  Uint32 bits = (w >> yp * shift) & mask;
  return bits;
}

inline void
SuperPool::setCheckBits(PtrI pageI, Uint32 type)
{
  Uint32 ip = pageI >> (32 - m_pageBits);
  Uint32 xp = ip >> (5 - SP_CHECK_LOG2);
  Uint32 yp = ip & (1 << (5 - SP_CHECK_LOG2)) - 1;
  Uint32& w = m_typeCheck[xp];
  Uint32 shift = 1 << SP_CHECK_LOG2;
  Uint32 mask = (1 << shift) - 1;
  // set
  Uint32 bits = makeCheckBits(type);
  w &= ~(mask << yp * shift);
  w |= (bits << yp * shift);
}

inline void*
SuperPool::getRecP(PtrI ptrI, RecInfo& ri)
{
  const Uint32 recMask = (1 << (32 - m_pageBits)) - 1;
  PtrI pageI = ptrI & ~recMask;
#if NDB_SP_VERIFY_LEVEL >= 1
  Uint32 bits1 = getCheckBits(pageI);
  Uint32 bits2 = makeCheckBits(ri.m_recType);
  assert(bits1 == bits2);
#endif
  void* pageP = getPageP(pageI);
  Uint32 ir = ptrI & recMask;
  void* recP = (Uint8*)pageP + ir * ri.m_recSize;
  return recP;
}

/*
 * HeapPool - SuperPool on heap (concrete class)
 *
 * A super pool based on malloc with memory root on the heap.  This
 * pool type has 2 realistic uses:
 *
 * - a small pool with only initial malloc and pageBits set to match
 * - the big pool from which all heap allocations are done
 *
 * A "smart" malloc may break "ip" limit by using different VM areas for
 * different sized requests.  For this reason malloc is done in units of
 * increment size if possible.   Memory root is set to start of first
 * malloc.
 */

class HeapPool : public SuperPool {
public:
  // Describes malloc area.  The areas are kept in singly linked list.
  // There is a list head and pointers to current and last area.
  struct Area {
    Area();
    Area* m_nextArea;
    PtrI m_firstPageI;
    Uint32 m_currPage;
    Uint32 m_numPages;
    void* m_memory;
  };

  // Constructor.
  HeapPool(Uint32 pageSize, Uint32 pageBits);

  // Initialize.
  virtual bool init();

  // Destructor.
  virtual ~HeapPool();

  // Use malloc to allocate more.
  bool allocMoreData(size_t size);

  // Get new page from current area.
  virtual PtrI getNewPage();

  // List of malloc areas.
  Area m_areaHead;
  Area* m_currArea;
  Area* m_lastArea;

  // Fraction of malloc size to try if cannot get all in one.
  Uint32 m_mallocPart;
};

/*
 * RecordPool -  record pool using one super pool instance (template)
 *
 * Documented under SuperPool.  Satisfies ArrayPool interface.
 */

template <class T>
class RecordPool {
public:
  // Constructor.
  RecordPool(SuperPool& superPool);

  // Destructor.
  ~RecordPool();

  // Update pointer ptr.p according to i-value ptr.i.
  void getPtr(Ptr<T>& ptr);

  // Allocate record from the pool.
  bool seize(Ptr<T>& ptr);

  // Return record to the pool.
  void release(Ptr<T>& ptr);

  // todo variants of basic methods

  // Return all pages to super pool.  The force flag is required if
  // there are any used records.
  void free(bool force);

  SuperPool& m_superPool;
  SuperPool::RecInfo m_recInfo;
};

template <class T>
inline
RecordPool<T>::RecordPool(SuperPool& superPool) :
  m_superPool(superPool),
  m_recInfo(1 + superPool.m_typeSeq++, sizeof(T))
{
  SuperPool::RecInfo& ri = m_recInfo;
  assert(sizeof(T) == SP_ALIGN_SIZE(sizeof(T), sizeof(Uint32)));
  Uint32 maxUseCount = superPool.m_pageSize / sizeof(T);
  Uint32 sizeLimit = 1 << (32 - superPool.m_pageBits);
  if (maxUseCount >= sizeLimit)
    maxUseCount = sizeLimit;
  ri.m_maxUseCount = maxUseCount;
}

template <class T>
inline
RecordPool<T>::~RecordPool()
{
  free(true);
}

template <class T>
inline void
RecordPool<T>::getPtr(Ptr<T>& ptr)
{
  void* recP = m_superPool.getRecP(ptr.i, m_recInfo);
  ptr.p = static_cast<T*>(recP);
}

template <class T>
inline bool
RecordPool<T>::seize(Ptr<T>& ptr)
{
  SuperPool& sp = m_superPool;
  SuperPool::RecInfo& ri = m_recInfo;
  if (ri.m_currFreeRecI != RNIL || sp.getAvailPage(ri)) {
    SuperPool::PtrI recI = ri.m_currFreeRecI;
    void* recP = sp.getRecP(recI, ri);
    ri.m_currFreeRecI = *(Uint32*)recP;
    Uint32 useCount = ri.m_currUseCount;
    assert(useCount < ri.m_maxUseCount);
    ri.m_currUseCount = useCount + 1;
    ri.m_totalUseCount++;
    ptr.i = recI;
    ptr.p = static_cast<T*>(recP);
    return true;
  }
  return false;
}

template <class T>
inline void
RecordPool<T>::release(Ptr<T>& ptr)
{
  SuperPool& sp = m_superPool;
  SuperPool::RecInfo& ri = m_recInfo;
  const Uint32 recMask = (1 << (32 - sp.m_pageBits)) - 1;
  SuperPool::PtrI recI = ptr.i;
  SuperPool::PtrI pageI = recI & ~recMask;
  if (pageI != ri.m_currPageI) {
    sp.setCurrPage(ri, pageI);
  }
  void* recP = sp.getRecP(recI, ri);
  *(Uint32*)recP = ri.m_currFreeRecI;
  ri.m_currFreeRecI = recI;
  Uint32 useCount = ri.m_currUseCount;
  assert(useCount != 0);
  ri.m_currUseCount = useCount - 1;
  ri.m_totalUseCount--;
  ptr.i = RNIL;
  ptr.p = 0;
}

template <class T>
inline void
RecordPool<T>::free(bool force)
{
  SuperPool& sp = m_superPool;
  SuperPool::RecInfo& ri = m_recInfo;
  sp.setCurrPage(ri, RNIL);
  assert(force || ri.m_totalUseCount == 0);
  sp.movePages(sp.m_pageList, ri.m_freeList);
  sp.movePages(sp.m_pageList, ri.m_activeList);
  sp.movePages(sp.m_pageList, ri.m_fullList);
  ri.m_totalRecCount = 0;
}

#endif
