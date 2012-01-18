#ifndef PAGEALLOCATOR_HPP
#define PAGEALLOCATOR_HPP

#include "ndb_global.h"
#include "blocks/diskpage.hpp"
#include "blocks/dbtup/tuppage.hpp"
#include "ndbd_malloc_impl.hpp"
#include "DLFifoList.hpp"
#include "Pool.hpp"

#ifndef VM_TRACE
#define NDEBUG 1
#endif

// PackablePage

class PackablePage: public Tup_varsize_page
{
  STATIC_CONST(ZTH_MM_FREE = 3);
  STATIC_CONST(ZTH_MM_FULL = 4);
public:
  PackablePage()
  {
    Tup_varsize_page::init();
    next_page = prev_page = RNIL;
    page_state = ZTH_MM_FREE;
  }
  void* getPtr(Uint32 i) const
  {
    if (i >= high_index)
      return NULL;
    Uint32 w = get_index_word(i);
    Uint32 pos = (w >> Tup_varsize_page::POS_SHIFT) & Tup_varsize_page::POS_MASK;
    return (void*)&m_data[pos];
  }
  bool release(Uint32 i, Uint32 sz)
  {
    if (i >= high_index)
      return false;
    (void) free_record(i, Tup_varsize_page::CHAIN);
    return true;
  }
  bool seize(Uint32& i, Uint32 sz)
  {
    Uint32 words = (sz + 3) / 4;
    if ((words + 1) > largest_frag_size())
    {
      page_state = ZTH_MM_FULL;
      return false;
    }
    page_state = ZTH_MM_FREE;
    i = alloc_record(words, NULL, Tup_varsize_page::CHAIN);
    return i != RNIL;
  }
  bool reorg()
  {
    if (free_space > largest_frag_size())
    {
      Tup_varsize_page temp;
      Tup_varsize_page::reorg(&temp);
    }
    return true;
  }
  bool isEmpty() const
  {
    return high_index == 1 ;
  }
  // TODO: find better name for betterThan()
  // lhs.betterThan(rhs) returns true if it is preferable to
  // allocate from lhs page rather than rhs page.
  // If a page have had a seize-failure it is set to FULL, and
  // for these pages its better to have much allocatable memory,
  // since it will be more probable to have enough memory for a
  // seize in future than where are no more FREE pages. If this
  // still is not enough a new page are allocated.
  // For pages not that not yet have had seize-failure the state
  // is set to FREE, and for these low allocatable memory is
  // better since this raise the probability that some other
  // (FREE) page will be empty in future and released.
  bool betterThan(const PackablePage& rhs) const
  {
    if (page_state == ZTH_MM_FULL)
      return largest_frag_size() > rhs.largest_frag_size(); // more memory goes first
    /* else ZTH_MM_FREE */
    return largest_frag_size() < rhs.largest_frag_size(); // less memory goes first
  }
};

// PackablePagePool (typed wrapper around Pool_context)

class PackablePagePool
{
public:
  typedef PackablePage Page;

  PackablePagePool(): m_reorg_page(RNIL) { }
  bool init(Pool_context& ctx, Uint32 rgid);
  Page* getPtr(Uint32 i) const;
  bool release(Ptr<Page> page);
  bool release(Uint32 i);
  bool seize(Ptr<Page>& page);
  bool seize(Uint32& i);

  bool reorg();
  bool current(Ptr<Page>& page);
  bool rotate();
private:
  typedef DLFifoList<Page, Page, PackablePagePool> List;
  typedef LocalDLFifoList<Page, Page, PackablePagePool> LocalList;

  void handle_invalid_release(Ptr<Page>) const ATTRIBUTE_NORETURN;
  void handle_invalid_get_ptr(Uint32 i) const ATTRIBUTE_NORETURN;

  Pool_context m_ctx;
  Uint32 m_rgid;

  List::Head m_list;
  Uint32 m_reorg_page;
};

inline bool
PackablePagePool::init(Pool_context& ctx, Uint32 rgid)
{
  m_ctx = ctx;
  m_rgid = rgid;
  return true;
}

inline PackablePagePool::Page*
PackablePagePool::getPtr(Uint32 i) const
{
  if (i == RNIL)
    return NULL;

  void* p = i + static_cast<Alloc_page*>(m_ctx.get_memroot());
  if (p == NULL)
    handle_invalid_get_ptr(i);

  return static_cast<Page*>(p);
}

inline bool
PackablePagePool::release(Uint32 i)
{
  Ptr<Page> page;
  page.i = i;
  page.p = getPtr(page.i);
  return release(page);
}

inline bool
PackablePagePool::seize(Uint32& i)
{
  Ptr<Page> page;
  bool ok = seize(page);
  i = page.i;
  return ok;
}

inline bool
PackablePagePool::current(Ptr<Page>& page)
{
  LocalList list(*this, m_list);
  list.first(page);
  return !page.isNull();
}

#endif
