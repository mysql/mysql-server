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

#ifndef SUPER_POOL_HPP
#define SUPER_POOL_HPP

#include <ndb_global.h>

#include <pc.hpp>
#include <ErrorReporter.hpp>

/*
 * SuperPool - super pool for record pools (abstract class)
 *
 * Documents: SuperPool GroupPool RecordPool<T>
 *
 * SUPER POOL
 *
 * A "super pool" is a shared pool of pages of fixed size.  A "record
 * pool" is a pool of records of fixed size.  One super pool instance is
 * used by a number of record pools to allocate their memory.  A special
 * case is a "page pool" where a record is a simple page whose size
 * divides super pool page size.
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
 * - "ip" index of page within super pool (high "pageBits")
 * - "ir" index of record within page (low "recBits")
 *
 * At most 16 recBits are used, the rest are zero.
 *
 * The translation between "ip" and page address is described in next
 * section.  Once page address is known, the record address is found
 * from "ir" in the obvious way.
 *
 * One advantage of i-value is that it can be verified.  The level of
 * verification can depend on compile options.
 *
 * - "v1" check i-value specifies valid page
 * - "v2" check record type matches page type, see below
 * - "v3" check record is in use
 * - "v4" check unused record is unmodified
 *
 * Another advantage of a 32-bit i-value is that it extends the space of
 * 32-bit addressable records on a 64-bit platform.
 *
 * MEMORY ROOT
 *
 * This super pool requires a "memory root" i.e. a memory address such
 * that the index of a page "ip" satisfies
 *
 *   page address = memory root + (signed)ip * page size
 *
 * This is possible on all platforms, provided that the memory root and
 * all pages are either on the heap or on the stack, in order to keep
 * the size of "ip" reasonably small.
 *
 * The cast (signed)ip is done as integer of pageBits bits.  "ip" has
 * same sign bit as i-value "i" so (signed)ip = (Int32)i >> recBits.
 *
 * RESERVED I-VALUES
 *
 * RNIL is 0xffffff00 (signed -256).  It is used everywhere in NDB as
 * "null pointer" i.e. as an i-value which does not point to a record.
 * In addition the signed values -255 to -1 are reserved for use by the
 * application.
 *
 * An i-value with all "ir" bits set is used as terminator in free
 * record list.  Unlike RNIL, it still has valid page bits "ip".
 *
 * Following restrictions avoid hitting the reserved values:
 *
 * - pageBits is <= 30
 * - the maximum "ip" value 2^pageBits-1 (signed -1) is not used
 * - the maximum "ir" value 2^recBits-1 is not used
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
 * aligning pages to OS block size and the use of BATs for page pools in
 * NDB.  For now the implementation provides an array of page entries
 * with place for all potential (2^pageBits) entries.
 *
 * PAGE TYPE
 *
 * Page type is unique to the record pool using the super pool.  It is
 * assigned in record pool constructor.  Page type zero means that the
 * page is free i.e. not allocated to a record pool.
 *
 * Each "i-p" conversion checks ("v2") that the record belongs to same
 * pool as the page.  This check is much more common than page or record
 * allocation.  To make it cache effective, there is a separate page
 * type array.  It truncates type to one non-zero byte.
 *
 * GROUP POOL
 *
 * Each record pool belongs to a group.  The group specifies minimum
 * size or memory percentage the group must be able to allocate.  The
 * sum of the minimum sizes of group pools is normally smaller than
 * super pool size.  This provides unclaimed memory which a group can
 * use temporarily to allocate more than its minimum.
 *
 * The record pools within a group compete freely for the available
 * memory within the group.
 *
 * Typical exmaple is group of all metadata pools.  The group allows
 * specifying the memory to reserve for metadata, without having to
 * specify number of tables, attributes, indexes, triggers, etc.
 *
 * PAGE LISTS
 *
 * Super pool has free page list.  Each group pool uses it to allocate
 * its own free page list.  And each record pool within the group uses
 * the group's free list to allocate its pages.
 *
 * A page allocated to a record pool has a use count i.e. number of used
 * records.  When use count drops to zero the page can be returned to
 * the group.  This is not necessarily done at once.
 *
 * The list of free records in a record pool has two levels.  There are
 * available pages (some free) and a singly linked free list within the
 * page.  A page allocated to record pool is on one of 4 lists:
 *
 * - free page (all free, available, could be returned to group)
 * - busy page (some free, some used, available)
 * - full page (none free)
 * - current page (list of one), see below
 *
 * Some usage types (temporary pools) may never free records.  They pay
 * a small penalty for the extra overhead.
 *
 * RECORD POOL
 *
 * A pool of records which allocates its memory from a super pool
 * instance via a group pool.  There are 3 basic operations:
 *
 * - getPtr - translate i-value to pointer-to-record p
 * - seize - allocate record
 * - release - free record
 *
 * CURRENT PAGE
 *
 * getPtr is a fast computation which does not touch the page entry.
 * For seize (and release) there is a small optimization.
 *
 * The "current page" is the page of latest seize.  It is unlinked from
 * its normal list and the free record pointer is stored under record
 * pool instance.
 *
 * The page remains current until there is a seize and the page is full.
 * Then the real page entry and its list membership are updated, and
 * a new page is made current.
 *
 * This implies that each (active) record pool allocates at least one
 * page which is never returned to the group.
 *
 * PAGE POLICY
 *
 * A group pool returns its "excess" (above minimum) free pages to the
 * super pool immediately.
 *
 * Allocating a new page to a record pool is expensive due to free list
 * setup.  Therefore a record pool should not always return empty pages
 * to the group.  Policies:
 *
 * - "pp1" never return empty page to the group
 * - "pp2" always return empty (non-current) page to the group
 * - "pp3" simple hysteresis
 *
 * Last one "pp3" is used.  It works as follows:
 *
 * When a page becomes free, check if number of free records exceeds
 * some fixed fraction of all records.  If it does, move all free pages
 * to the group.  Current page is ignored in the check.
 *
 * TODO
 *
 * Define abstract class SuperAlloc.  Make SuperPool a concrete class
 * with SuperAlloc instance in ctor.  Replace HeapPool by HeapAlloc.
 */

// Types forward.
class GroupPool;

class SuperPool {
public:
  // Type of i-value, used to reference both pages and records.
  typedef Uint32 PtrI;

  // Page entry.
  struct PageEnt {
    PageEnt();
    Uint16 m_pageType;          // zero if not in record pool
    Uint16 m_useCount;          // used records on the page
    PtrI m_freeRecI;            // first free record on the page
    PtrI m_nextPageI;
    PtrI m_prevPageI;
  };

  // Doubly-linked list of page entries.
  struct PageList {
    PageList();
    PageList(PtrI pageI);
    PtrI m_headPageI;
    PtrI m_tailPageI;
    Uint32 m_pageCount;
  };

  // Constructor.  Gives page size in bytes (must be power of 2) and
  // number of bits to use for page index "ip" in i-value.
  SuperPool(Uint32 pageSize, Uint32 pageBits);

  // Destructor.
  virtual ~SuperPool() = 0;

  // Move all pages from second list to end of first list.
  void movePages(PageList& pl1, PageList& pl2);

  // Add page to beginning of page list.
  void addHeadPage(PageList& pl, PtrI pageI);

  // Add page to end of page list.
  void addTailPage(PageList& pl, PtrI pageI);

  // Remove any page from page list.
  void removePage(PageList& pl, PtrI pageI);

  // Translate i-value ("ri" ignored) to page entry.
  PageEnt& getPageEnt(PtrI pageI);

  // Translate i-value ("ri" ignored) to page address.
  void* getPageP(PtrI pageI);

  // Translate page address to i-value.  Address must be page-aligned to
  // memory root.  Returns RNIL if "ip" range exceeded.
  PtrI getPageI(void* pageP);

  // Record pool info.
  struct RecInfo {
    RecInfo(GroupPool& gp, Uint32 recSize);
    GroupPool& m_groupPool;
    Uint32 m_recSize;
    Uint16 m_recType;
    Uint16 m_maxPerPage;
    PtrI m_freeRecI;            // first free record on current page
    Uint32 m_useCount;          // used records excluding current page
    PageList m_pageList[3];     // 0-free 1-busy 2-full
    Uint16 m_hyX;               // hysteresis fraction x/y in "pp3"
    Uint16 m_hyY;
  };

  // Translate i-value to record address.
  void* getRecP(PtrI recI, RecInfo& ri);

  // Count records on page free list.
  Uint32 getFreeCount(RecInfo& ri, PtrI freeRecPtrI);

  // Compute total number of pages in pool.
  Uint32 getRecPageCount(RecInfo& ri);

  // Compute total number of records (used or not) in pool.
  Uint32 getRecTotCount(RecInfo& ri);

  // Compute total number of used records in pool.
  Uint32 getRecUseCount(RecInfo& ri);

  // Compute record pool page list index (0,1,2).
  Uint32 getRecPageList(RecInfo& ri, PageEnt& pe);

  // Add current page.
  void addCurrPage(RecInfo& ri, PtrI pageI);

  // Remove current page.
  void removeCurrPage(RecInfo& ri);

  // Get page with some free records and make it current.  Takes head of
  // used or free list, or else gets free page from group pool.
  bool getAvailPage(RecInfo& ri);

  // Get free page from group pool and add it to record pool free list.
  // This is an expensive subroutine of getAvailPage(RecInfo&):
  PtrI getFreePage(RecInfo& ri);

  // Get free detached (not on list) page from group pool.
  PtrI getFreePage(GroupPool& gp);

  // Get free detached page from super pool.
  PtrI getFreePage();

  // Get new free detached page from the implementation.
  virtual PtrI getNewPage() = 0;

  // Initialize free list etc.  Subroutine of getFreePage(RecInfo&).
  void initFreePage(RecInfo& ri, PtrI pageI);

  // Release record which is not on current page.
  void releaseNotCurrent(RecInfo& ri, PtrI recI);

  // Free pages from record pool according to page policy.
  void freeRecPages(RecInfo& ri);

  // Free all pages in record pool.
  void freeAllRecPages(RecInfo& ri, bool force);

  // Set pool size parameters in pages.  Call allocMemory() for changes
  // (such as extra mallocs) to take effect.
  void setInitPages(Uint32 initPages);
  void setIncrPages(Uint32 incrPages);
  void setMaxPages(Uint32 maxPages);

  // Get number of pages reserved by all groups.
  Uint32 getGpMinPages();

  // Get number of pages reserved to a group.
  Uint32 getMinPages(GroupPool& gp);

  // Get max number of pages a group can try to allocate.
  Uint32 getMaxPages(GroupPool& gp);

  // Allocate more memory according to current parameters.  Returns
  // false if no new memory was allocated.   Otherwise returns true,
  // even if the amount allocated was less than requested.
  virtual bool allocMemory() = 0;

  // Debugging.
  void verify(RecInfo& ri);
  void verifyPageList(PageList& pl);

  // Super pool parameters.
  const Uint32 m_pageSize;
  const Uint16 m_pageBits;
  const Uint16 m_recBits;
  const Uint32 m_recMask;
  // Implementation must set up these 3 pointers.
  void* m_memRoot;
  PageEnt* m_pageEnt;
  Uint8* m_pageType;
  // Free page list.
  PageList m_freeList;
  // Free pages and sizes.
  Uint32 m_initPages;
  Uint32 m_incrPages;
  Uint32 m_maxPages;
  Uint32 m_totPages;
  Uint32 m_typeCount;
  // Reserved and allocated by group pools.
  Uint32 m_groupMinPct;
  Uint32 m_groupMinPages;
  Uint32 m_groupTotPages;
};

inline SuperPool::PageEnt&
SuperPool::getPageEnt(PtrI pageI)
{
  Uint32 ip = pageI >> m_recBits;
  return m_pageEnt[ip];
}

inline void*
SuperPool::getPageP(PtrI ptrI)
{
  Int32 ip = (Int32)ptrI >> m_recBits;
  return (Uint8*)m_memRoot + ip * (my_ptrdiff_t)m_pageSize;
}

inline void*
SuperPool::getRecP(PtrI ptrI, RecInfo& ri)
{
  Uint32 ip = ptrI >> m_recBits;
  assert(m_pageType[ip] == (ri.m_recType & 0xFF));
  Uint32 ir = ptrI & m_recMask;
  return (Uint8*)getPageP(ptrI) + ir * ri.m_recSize;
}

/*
 * GroupPool - subset of a super pool pages (concrete class)
 */

class GroupPool {
public:
  // Types.
  typedef SuperPool::PageList PageList;

  // Constructor.
  GroupPool(SuperPool& sp);

  // Destructor.
  ~GroupPool();

  // Set minimum pct reserved in super pool.
  void setMinPct(Uint32 resPct);

  // Set minimum pages reserved in super pool.
  void setMinPages(Uint32 resPages);

  SuperPool& m_superPool;
  Uint32 m_minPct;
  Uint32 m_minPages;
  Uint32 m_totPages;
  PageList m_freeList;
};

/*
 * RecordPool -  record pool using one super pool instance (template)
 */

template <class T>
class RecordPool {
public:
  // Constructor.
  RecordPool(GroupPool& gp);

  // Destructor.
  ~RecordPool();

  // Update pointer ptr.p according to i-value ptr.i.
  void getPtr(Ptr<T>& ptr);

  // Allocate record from the pool.
  bool seize(Ptr<T>& ptr);

  // Return record to the pool.
  void release(Ptr<T>& ptr);

  // todo variants of basic methods

  // Return all pages to group pool.  The force flag is required if
  // there are any used records.
  void freeAllRecPages(bool force);

  SuperPool& m_superPool;
  SuperPool::RecInfo m_recInfo;
};

template <class T>
inline
RecordPool<T>::RecordPool(GroupPool& gp) :
  m_superPool(gp.m_superPool),
  m_recInfo(gp, sizeof(T))
{
}

template <class T>
inline
RecordPool<T>::~RecordPool()
{
  freeAllRecPages(true);
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
  Uint32 recMask = sp.m_recMask;
  // get current page
  if ((ri.m_freeRecI & recMask) != recMask ||
      sp.getAvailPage(ri)) {
    SuperPool::PtrI recI = ri.m_freeRecI;
    void* recP = sp.getRecP(recI, ri);
    ri.m_freeRecI = *(Uint32*)recP;
    ptr.i = recI;
    ptr.p = static_cast<T*>(recP);
    return true;
  }
  ptr.i = RNIL;
  ptr.p = 0;
  return false;
}

template <class T>
inline void
RecordPool<T>::release(Ptr<T>& ptr)
{
  SuperPool& sp = m_superPool;
  SuperPool::RecInfo& ri = m_recInfo;
  SuperPool::PtrI recI = ptr.i;
  Uint32 recMask = sp.m_recMask;
  // check if current page
  if ((recI & ~recMask) == (ri.m_freeRecI & ~recMask)) {
    void* recP = sp.getRecP(recI, ri);
    *(Uint32*)recP = ri.m_freeRecI;
    ri.m_freeRecI = recI;
  } else {
    sp.releaseNotCurrent(ri, recI);
  }
  ptr.i = RNIL;
  ptr.p = 0;
}

template <class T>
inline void
RecordPool<T>::freeAllRecPages(bool force)
{
  SuperPool& sp = m_superPool;
  sp.freeAllRecPages(m_recInfo, force);
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
 * A smart malloc may break "ip" limit by using different VM areas for
 * different sized requests.  For this reason malloc is done in units of
 * increment size if possible.  Memory root is set to the page-aligned
 * address from first page malloc.
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
    void* m_memory;     // from malloc
    void* m_pages;      // page-aligned pages
    Uint32 m_numPages;  // number of pages
  };

  // Constructor.
  HeapPool(Uint32 pageSize, Uint32 pageBits);

  // Destructor.
  virtual ~HeapPool();

  // Get new page from current area.
  virtual PtrI getNewPage();

  // Allocate fixed arrays.
  bool allocInit();

  // Allocate array of aligned pages.
  bool allocArea(Area* ap, Uint32 tryPages);

  // Allocate memory.
  virtual bool allocMemory() { return allocMemoryImpl();}
  bool allocMemoryImpl();

  // List of malloc areas.
  Area m_areaHead;
  Area* m_currArea;
  Area* m_lastArea;
};

#endif
