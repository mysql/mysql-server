#include "ndb_global.h"
#include "ndbd_malloc_impl.hpp"
#include "DLFifoList.hpp"
#include "PagePool.hpp"

bool
PackablePagePool::release(Ptr<PackablePagePool::Page> page)
{
  if (page.i == RNIL || page.p == NULL)
    handle_invalid_release(page);

  LocalList list(*this, m_list);

  if (page.i == m_reorg_page)
  {
    Ptr<Page> reorg = page;
    list.next(reorg);
    m_reorg_page = reorg.i;
  }
  list.remove(page);

  m_ctx.release_page(m_rgid, page.i);
  return true;
}

bool
PackablePagePool::seize(Ptr<Page>& page)
{
  page.p = static_cast<Page*>(m_ctx.alloc_page(m_rgid, &page.i));
  if (page.p == NULL) return false; // Note: page.i need not to be RNIL
  new (page.p) Page;

  LocalList list(*this, m_list);
  list.addFirst(page);

  return true;
}

bool
PackablePagePool::reorg()
{
  LocalList list(*this, m_list);

  if (list.isEmpty())
    return true;

  Ptr<Page> page;
  Ptr<Page> prev;
  prev.i = m_reorg_page;
  if (prev.i != RNIL)
  {
    prev.p = getPtr(prev.i);
    page = prev;
    list.next(page);
  }
  else page.setNull();
  if (page.isNull())
  {
    list.first(page);
    prev.setNull();
  }

  page.p->reorg();
  m_reorg_page = page.i;

  if (!prev.isNull())
  {
    bool moveForward = page.p->betterThan(*prev.p);
    if (moveForward)
    {
      // swap prev and page
      m_reorg_page = prev.i;
      list.remove(page);
      list.insert(page, prev);
    }
  }
  return true;
}

bool
PackablePagePool::rotate()
{
  LocalList list(*this, m_list);

  if (list.isEmpty()) return false;

  Ptr<Page> page;
  list.first(page);
  if (!list.hasNext(page)) return false;

  list.remove(page);
  list.addLast(page);

  return true;
}

void
PackablePagePool::handle_invalid_get_ptr(Uint32 ptrI) const
{
  char buf[255];

  Ptr<void> ptr;
  ptr.i = ptrI;
  if (ptrI == RNIL)
    ptr.p = NULL;
  else
    ptr.p = ptrI + static_cast<Alloc_page*>(m_ctx.get_memroot());

  BaseString::snprintf(buf, sizeof(buf),
                       "Invalid memory access: page ptr (%x %p) memroot: %p",
                       ptr.i, ptr.p, m_ctx.get_memroot());

  m_ctx.handleAbort(NDBD_EXIT_PRGERR, buf);
}

void
PackablePagePool::handle_invalid_release(Ptr<Page> ptr) const
{
  char buf[255];

  BaseString::snprintf(buf, sizeof(buf),
                       "Invalid memory release: page ptr (%x %p) memroot: %p",
                       ptr.i, ptr.p, m_ctx.get_memroot());

  m_ctx.handleAbort(NDBD_EXIT_PRGERR, buf);
}

#ifdef TAP_TEST

#include <NdbTap.hpp>
#include "test_context.hpp"

#include "Pool.hpp"
#include "blocks/record_types.hpp"

TAPTEST(PackablePagePool)
{
  Pool_context pc = test_context(10000);

  PackablePagePool pagePool;
  pagePool.init(pc, RG_SCHEMA_MEMORY);

  unsigned reorgstep = 20;
  unsigned nloops = 100000;
  unsigned nrecords = 40000;
  Uint32* records;

  unsigned seized = 0;
  unsigned seize_failed = 0;
  unsigned released = 0;
  unsigned reorgs = 0;
  unsigned max_seized = 0;

  records = new Uint32[nrecords];
  for (unsigned i = 0; i < nrecords; i ++)
    records[i] = RNIL;

  for (unsigned i = 0; i < nloops; i ++)
  {
    Uint32 reci = rand() % nrecords;

    if (records[reci] == RNIL)
    {
      /* seize */
      Ptr<PackablePage> p;
      bool ok = pagePool.seize(p);
      if (!ok)
      {
        seize_failed++;
        goto try_reorg;
      }
      p.p = pagePool.getPtr(p.i);
      records[reci] = p.i;
      seized ++;
      if (seized-released > max_seized)
        max_seized = seized - released;
    }
    else
    {
      /* getPtr */
      Ptr<PackablePage> p;
      p.i = records[reci];
      p.p = pagePool.getPtr(p.i);
      if (p.p == NULL)
      {
        diag("getPtr %u failed", p.i);
        return 0;
      }
      /* release */
      bool ok = pagePool.release(p);
      if (!ok)
      {
        diag("release %u@%p failed", p.i, p.p);
        return 0;
      }
      records[reci] = p.i = RNIL;
      released ++;
    }
  try_reorg:
    if (reorgstep && rand() % reorgstep == 0)
    {
      bool ok = pagePool.reorg();
      if (!ok)
      {
        diag("reorg failed\n");
        return 0;
      }
      reorgs ++;
    }
  }
  diag("cleaning up: seized: %u (failed: %u), released: %u, max: %u, left: %u, reorgs: %u",
       seized, seize_failed, released, max_seized, seized - released, reorgs);

  for (unsigned reci = 0; reci < nrecords; reci ++)
  {
    if (records[reci] != RNIL)
    {
      /* getPtr */
      Ptr<PackablePage> p;
      p.i = records[reci];
      p.p = pagePool.getPtr(p.i);
      if (p.p == NULL)
      {
        diag("getPtr %u failed", p.i);
        return 0;
      }
      /* release */
      bool ok = pagePool.release(p);
      if (!ok)
      {
        diag("release %u@%p failed", p.i, p.p);
        return 0;
      }
      records[reci] = p.i = RNIL;
      released ++;
    }
  }
  diag("cleaned up: seized: %u (failed: %u), released: %u, max: %u, left: %u, reorgs: %u",
       seized, seize_failed, released, max_seized, seized - released, reorgs);

  return (seized == released);
}

#endif
