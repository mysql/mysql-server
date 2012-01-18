#include "Pool.hpp"
#include "PackPool.hpp"
#include "PagePool.hpp"

bool
PackAllocator::release(Uint32 i, Uint32 sz)
{
  Ptr<Page> page;
  page.i = page_id(i);
  page.p = m_page_pool->getPtr(page.i);

  if (i == RNIL || page.isNull())
    return false;

  if (!page.p->release(page_index(i), sz))
    return false;

  if (page.p->isEmpty())
    m_page_pool->release(page);

  return true;
}

bool
PackAllocator::seize(Uint32& i, Uint32 sz)
{
  Ptr<Page> page;
  Uint32 page_index = RNIL;
  unsigned page_lookups = 0;
  while (page_lookups < MAX_PAGE_LOOKUPS)
  {
    page_lookups++;
    m_page_pool->current(page);
    if (page.isNull()) break;
    if (page.p->seize(page_index, sz)) break;
    if (!m_page_pool->rotate()) break;
  }
  if (page_index == RNIL)
  {
    if (!m_page_pool->seize(page)) return false;
    if (!page.p->seize(page_index, sz)) return false;
  }
  if (page_index == RNIL) return false;
  i = make_ptri(page.i, page_index);
  return true;
}

#ifdef TAP_TEST

#include "CountingPool.hpp"
#include "NdbTap.hpp"
#include "blocks/record_types.hpp"
#include "test_context.hpp"

struct record
{
  unsigned short size;
  unsigned short sum;
  unsigned short data[1];
  record(unsigned short sz): size(sz), sum(0)
  {
    fill();
  }
  ~record()
  {
    size = 0xdead;
  }
  void fill()
  {
    sum = 0;
    for (int j = 0; j < size; j ++)
    {
      unsigned short v = rand();
      sum ^= v;
      data[j] = v;
    }
  }
  bool validate()
  {
    if (size == 0xdead) return false;
    unsigned short par = 0;
    for (int j = 0; j < size; j++)
    {
      par ^= data[j];
    }
    return sum == par;
  }
};

int
PackAllocator_test(int argc, char *argv[])
{
  Pool_context pc = test_context(10000);
  CountingPool<PackablePage, PackablePagePool> pagePool;
  pagePool.init(pc, RG_SCHEMA_MEMORY);
  PackAllocator allocator(pagePool);

  unsigned reorgstep = 20;
  unsigned nloops = 100000;
  unsigned nrecords = 40000;
  unsigned blocksize = 10000;

  int optc;
  while (EOF != (optc = getopt(argc, argv, "b:r:l:n:")))
    switch (optc)
    {
    case 'b':
      blocksize = atoi(optarg);
      break;
    case 'r':
      reorgstep = atoi(optarg);
      break;
    case 'l':
      nloops = atoi(optarg);
      break;
    case 'n':
      nrecords = atoi(optarg);
      break;
    }

  diag("reorgstep(-r)=%u nloops(-l)=%u nrecords(-n)=%u", reorgstep, nloops, nrecords);

  unsigned seized = 0;
  unsigned seize_failed = 0;
  unsigned released = 0;
  unsigned reorgs = 0;
  unsigned max_seized = 0;

  Uint32* records = new Uint32[nrecords];
  for (unsigned i = 0; i < nrecords; i ++)
    records[i] = RNIL;

  for (unsigned i = 0; i < nloops; i ++)
  {
    Uint32 reci = rand() % nrecords;
    if (records[reci] == RNIL)
    {
      /* seize */
      Ptr<record> p;
      unsigned short size = rand() % blocksize ;
      unsigned short sizeb = sizeof(struct record) - sizeof(unsigned short) +
                             sizeof(unsigned short) * size;
      bool ok = allocator.seize(p.i, sizeb);
      if (!ok)
      {
        seize_failed ++;
        goto try_reorg;
      }
      p.p = static_cast<record*>(allocator.getPtr(p.i));
      new(p.p) record(size);
      records[reci] = p.i;
      seized ++;
      if (seized - released > max_seized)
        max_seized = seized - released;
    }
    else
    {
      /* getPtr */
      Ptr<record> p;
      p.i = records[reci];
      p.p = static_cast<record*>(allocator.getPtr(p.i));
      if (p.p == NULL)
      {
        diag("getPtr %u failed", p.i);
        return 0;
      }
      if (!p.p->validate())
      {
        diag("validate %u@%p failed", p.i, p.p);
        return 0;
      }
      /* release */
      bool ok = allocator.release(p.i, 0);
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
        diag("reorg failed");
        return 0;
      }
      reorgs ++;
    }
  }

  diag("cleaning up: seized: %u (failed: %u), released: %u, max: %u, left: %u, reorgs: %u",
       seized, seize_failed, released, max_seized, seized - released, reorgs);

  diag("pagePool: size: %u, entrySize: %u, noOfFree: %u, used: %u, usedHi: %u",
       pagePool.getSize(), pagePool.getEntrySize(), pagePool.getNoOfFree(),
       pagePool.getUsed(), pagePool.getUsedHi());

  for (unsigned reci = 0; reci < nrecords; reci ++)
  {
    if (records[reci] != RNIL)
    {
      /* getPtr */
      Ptr<record> p;
      p.i = records[reci];
      p.p = static_cast<record*>(allocator.getPtr(p.i));
      if (p.p == NULL)
      {
        diag("getPtr %u failed", p.i);
        return 0;
      }
      if (!p.p->validate())
      {
        diag("validate %u@%p failed", p.i, p.p);
        return 0;
      }
      /* release */
      bool ok = allocator.release(p.i, 0);
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

  diag("pagePool: size: %u, entrySize: %u, noOfFree: %u, used: %u, usedHi: %u",
       pagePool.getSize(), pagePool.getEntrySize(), pagePool.getNoOfFree(),
       pagePool.getUsed(), pagePool.getUsedHi());

  return (seized == released) && (pagePool.getUsed() == 0);
}

int
main(int argc, char *argv[])
{
  plan(1);
  ok(PackAllocator_test(argc, argv), "PackAllocator");
  return exit_status();
}

#endif
