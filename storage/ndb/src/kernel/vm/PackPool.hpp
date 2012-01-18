#ifndef PACKALLOCATOR_HPP
#define PACKALLOCATOR_HPP

#include "ndb_global.h"
#include "CountingPool.hpp"
#include "PagePool.hpp"
#include "Pool.hpp"

// PackAllocator

class PackAllocator
{
  typedef PackablePage Page;
  typedef CountingPool<Page, PackablePagePool> Pool;

  STATIC_CONST(MAX_PAGE_LOOKUPS = 2);
public:
  PackAllocator(): m_page_pool(NULL) {}
  PackAllocator(Pool& page_pool): m_page_pool(&page_pool) {}
  void* getPtr(Uint32 i);
  bool release(Uint32 i, Uint32 sz);
  bool seize(Uint32& i, Uint32 sz);
private:
  Uint32 page_id(Uint32 i) const
  {
    return i >> 13;
  }
  Uint32 page_index(Uint32 i) const
  {
    return i & ((1 << 13) - 1);
  }
  Uint32 make_ptri(Uint32 page_id, Uint32 page_index) const
  {
    return (page_id << 13) | page_index;
  }

  Pool* m_page_pool;
};

inline void*
PackAllocator::getPtr(Uint32 i)
{
  if (i == RNIL) return NULL;
  Page* page = m_page_pool->getPtr(page_id(i));
  if (page == NULL) return NULL;
  void * p = page->getPtr(page_index(i));
  return p;
}

// PackPool

template<typename T, class A=PackAllocator> class PackPool
{
  A m_allocator;
public:
  PackPool() { }
  bool init(A& allocator)
  {
    m_allocator = allocator;
    return true;
  }

  bool seize(Ptr<T>& p)
  {
    bool ok = m_allocator.seize(p.i, sizeof(T));
    if (!ok) return ok;
    p.p = getPtr(p.i);
    return true;
  }
  bool release(Ptr<T> p)
  {
    return m_allocator.release(p.i, sizeof(T));
  }
  T* getPtr(Uint32 i)
  {
    return static_cast<T*>(m_allocator.getPtr(i));
  }
};

#endif
