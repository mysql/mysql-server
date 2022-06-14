/*
   Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifdef TEST_INTRUSIVELIST

#include <ndb_global.h>
#include <NdbTap.hpp>
#include "IntrusiveList.hpp"
#include "ArrayPool.hpp"
#include "RWPool.hpp"
#include "test_context.hpp"

#define JAM_FILE_ID 249

class Type
{
};

class SL: public Type
{
public:
  Uint32 nextList;
};

class DL: public SL
{
public:
  Uint32 prevList;
};

class T : public DL
{
public:
  Uint32 key;
  Uint32 data;
  Uint32 nextPool;
#ifndef USE_ARRAYPOOL
  Uint32 m_magic;
#endif
};

static unsigned scale = 100;

#define LIST_COMMON_TEST(pool, list, head) \
  Ptr<T> p; \
  Ptr<T> q; \
 \
  ok(&pool == &list.getPool(), "list.getPool()"); \
 \
  ok(list.isEmpty(), "list.isEmpty()"); \
 \
  Uint32 c_seized = 0; \
  Uint32 c_walked = 0; \
  Uint32 c_released = 0; \
  Uint32 c_moved = 0; \
  Uint32 c_half; \
 \
  while (list.seizeFirst(p)) { c_seized ++; p.p->key = c_seized; } \
  ok(!list.isEmpty(), "seizeFirst %u items", c_seized); \
 \
  c_half = c_seized / 2;   \
 \
  if (list.first(p)) { c_walked ++; while (list.next(p)) c_walked ++; } \
  ok(c_seized == c_walked, "walk next %u of %u items", c_walked, c_seized); \
 \
  list.first(q); \
  while (q.p->key != c_half && list.hasNext(q)) list.next(q); \
  ok(q.p->key == c_half, "find half %u (%u)", q.p->key, c_half); \
 \
  /* list before: key= c_seized, c_seized - 1, ..., c_half, ..., 2, 1 */ \
  /* list after: key= c_half + 1, c_half + 2, ..., c_seized - 1, c_seized, c_half - 1, c_half - 2, ..., 2, 1 and c_half removed into p */ \
  while (list.removeFirst(p)) { if (p.i == q.i) break; list.insertAfter(p, q); c_moved ++;} \
  ok(p.p->key == c_half, "rearrange: removed item %u (%u), moved %u items", p.p->key, c_half, c_moved); \
  ok(c_moved == (c_seized - c_half), "rearrange: moved %u of %u items", c_moved, (c_seized - c_half)); \
 \
  pool.release(p); \
  c_released ++; \
 \
  list.first(p); \
  ok(p.p->key == c_half + 1, "rearrange: first item %u (%u)", p.p->key, c_half + 1); \
 \
  ok(p.p == list.getPtr(p.i), "list.getPtr(%u) = %p (%p)", p.i, p.p, list.getPtr(p.i)); \
  ok(p.p == pool.getPtr(p.i), "pool.getPtr(%u) = %p (%p)", p.i, p.p, list.getPtr(p.i)); \
 \
  q.i = p.i; \
  q.p = NULL; \
  list.getPtr(q); \
  ok(q.p == p.p, "list.getPtr(q)"); \
 \
  q.i = RNIL; \
  q.p = NULL; \
  list.getPtr(q, p.i); \
  ok(q.p == p.p, "list.getPtr(q, p.i)")

#define LIST_PREV_TEST(pool, list, head) \
  list.first(q); \
  while (q.p->key != c_half - 1 && list.hasNext(q)) list.next(q); \
  ok(q.p->key == c_half - 1, "find %u (%u)", q.p->key, c_half - 1); \
  /* list before: key= c_half + 1, c_half + 2, ..., c_seized - 1, c_seized, c_half - 1, c_half - 2, ..., 2, 1 */ \
  /* list after: key= c_seized - 1, ..., c_half + 1, c_half -1, ..., 2, 1 and c_seized removed into p */ \
  while (list.removeFirst(p)) { if (p.i == q.i) break; list.insertBefore(p, q); q=p; c_moved ++;} \
  ok(p.p->key == c_seized, "rearrange: removed item %u (%u), moved %u items", p.p->key, c_seized, c_moved); \
  ok(c_moved == c_seized, "rearrange: moved %u of %u items", c_moved, c_seized); \
 \
  pool.release(p); \
  c_released ++; \
 \
  list.first(p); \
  ok(p.p->key == c_seized - 1, "rearrange: first item %u (%u)", p.p->key, c_seized - 1); \
 \
  while (p.p->key != c_half -1 && list.next(p)); \
  ok(p.p->key == c_half -1, "found %u (%u)", p.p->key, c_half -1); \
 \
  q = p; \
  (void) list.next(q); \
  list.remove(q.p); \
  pool.release(q); \
  c_released ++; \
  q = p; \
  (void) list.next(q); \
  ok(q.p->key == c_half -3, "found %u (%u)", q.p->key, c_half - 3); \
  list.release(p); \
  c_released ++

#define LIST_LAST_TEST(pool, list, head) \
  c_seized = 0; \
  while (list.seizeLast(p)) c_seized ++; \
  ok(c_seized == c_released, "seizeLast %u (%u)", c_seized, c_released); \
  c_released = 0; \
  while (list.last(p)) { list.releaseFirst(); c_released ++; } \
  ok(c_seized == c_released, "released %u (%u)", c_released, c_seized)

#define LIST_COUNT_TEST(pool, list, head, value) \
  { \
    Uint32 c = list.getCount(); \
    ok(c == value, "count %u (%u)", c, value); \
  }

#define LIST_RELEASE_FIRST(list) \
  while (list.releaseFirst()) c_released ++; \
  OK(c_seized == c_released); \
 \
  OK(list.isEmpty())

#define LIST_RELEASE_LAST(list) \
  while (list.releaseLast()) c_released ++; \
  ok(c_seized == c_released, "released %u (%u)", c_released, c_seized); \
 \
  ok(list.isEmpty(), "list.isEmpty()")

template<typename Pool>
void testSLList(Pool& pool)
{
  diag("testSLList");
  SLList<SL>::Head head;
  LocalSLList<Pool> list(pool, head);

  LIST_COMMON_TEST(pool, list, head);

  LIST_RELEASE_FIRST(list);
}

template<typename Pool>
void testDLList(Pool& pool)
{
  diag("testDLList");
  DLList<DL>::Head head;
  LocalDLList<Pool> list(pool, head);

  LIST_COMMON_TEST(pool, list, head);

  LIST_PREV_TEST(pool, list, head);

  LIST_RELEASE_FIRST(list);
}

template<typename Pool>
void testSLCList(Pool& pool)
{
  diag("testSLCList");
  SLCList<SL>::Head head;
  LocalSLCList<Pool> list(pool, head);

  LIST_COMMON_TEST(pool, list, head);

  LIST_COUNT_TEST(pool, list, head, c_seized - 1);

  LIST_RELEASE_FIRST(list);
}

template<typename Pool>
void testDLCList(Pool& pool)
{
  diag("testDLCList");
  DLCList<DL>::Head head;
  LocalDLCList<Pool> list(pool, head);

  LIST_COMMON_TEST(pool, list, head);

  LIST_COUNT_TEST(pool, list, head, c_seized - 1);

  LIST_PREV_TEST(pool, list, head);

  LIST_COUNT_TEST(pool, list, head, c_seized - 4);

  LIST_RELEASE_FIRST(list);
}

template<typename Pool>
void testSLFifoList(Pool& pool)
{
  diag("testSLFifoList");
  SLFifoList<SL>::Head head;
  LocalSLFifoList<Pool> list(pool, head);

  LIST_COMMON_TEST(pool, list, head);

  LIST_RELEASE_FIRST(list);

  LIST_LAST_TEST(pool, list, head);
}

template<typename Pool>
void testDLFifoList(Pool& pool)
{
  diag("testDLFifoList");
  DLFifoList<DL>::Head head;
  LocalDLFifoList<Pool> list(pool, head);

  LIST_COMMON_TEST(pool, list, head);

  LIST_PREV_TEST(pool, list, head);

  LIST_RELEASE_LAST(list);

  LIST_LAST_TEST(pool, list, head);
}

template<typename Pool>
void testSLCFifoList(Pool& pool)
{
  diag("testSLCFifoList");
  SLCFifoList<SL>::Head head;
  LocalSLCFifoList<Pool> list(pool, head);

  LIST_COMMON_TEST(pool, list, head);

  LIST_COUNT_TEST(pool, list, head, c_seized - 1);

  LIST_RELEASE_FIRST(list);

  LIST_COUNT_TEST(pool, list, head, 0);

  LIST_LAST_TEST(pool, list, head);

  LIST_COUNT_TEST(pool, list, head, 0);
}

template<typename Pool>
void testDLCFifoList(Pool& pool)
{
  diag("testDLCFifoList");
  DLCFifoList<DL>::Head head;
  LocalDLCFifoList<Pool> list(pool, head);

  LIST_COMMON_TEST(pool, list, head);

  LIST_COUNT_TEST(pool, list, head, c_seized - 1);

  LIST_PREV_TEST(pool, list, head);

  LIST_COUNT_TEST(pool, list, head, c_seized - 4);

  LIST_RELEASE_LAST(list);

  LIST_COUNT_TEST(pool, list, head, 0);

  LIST_LAST_TEST(pool, list, head);

  LIST_COUNT_TEST(pool, list, head, 0);
}

template<typename Pool>
void testConcat(Pool& pool)
{
  diag("testConcat");
  SLFifoList<SL>::Head slhead;
  DLFifoList<DL>::Head dlhead;
  SLCFifoList<SL>::Head slchead;
  DLCFifoList<DL>::Head dlchead;

  Ptr<T> p;

  Uint32 c_seized = 0;

  {
    LocalSLFifoList<Pool> list(pool, slhead);
    for (; c_seized < 1 * scale ; c_seized ++)
    {
      if (!list.seizeFirst(p)) break;
      p.p->key = c_seized + 1;
    }
    ok(c_seized == 1 * scale, "sl seized up to %u", c_seized);
  } /* sl: 100-1 */
  {
    LocalDLFifoList<Pool> list(pool, dlhead);
    for (; c_seized < 2 * scale ; c_seized ++)
    {
      if (!list.seizeFirst(p)) break;
      p.p->key = c_seized + 1;
    }
    ok(c_seized == 2 * scale, "dl seized up to %u", c_seized);
  } /* dl: 200-101 */
  {
    LocalSLCFifoList<Pool> list(pool, slchead);
    for (; c_seized < 3 * scale; c_seized ++)
    {
      if (!list.seizeFirst(p)) break;
      p.p->key = c_seized + 1;
    }
    ok(c_seized == 3 * scale, "slc seized up to %u", c_seized);
    ok(list.getCount() == 1 * scale, "slc.count %u (%u)", list.getCount(), 1 * scale);
  } /* slc: 300-201 */
  {
    LocalDLCFifoList<Pool> list(pool, dlchead);
    for (; c_seized < 4 * scale; c_seized ++)
    {
      if (!list.seizeFirst(p)) break;
      p.p->key = c_seized + 1;
    }
    ok(c_seized == 4 * scale, "dlc seized up to %u", c_seized);
    ok(list.getCount() == 1 * scale, "dlc.count %u (%u)", list.getCount(), 1 * scale);
  } /* dlc: 400-301 */
  {
    LocalSLCFifoList<Pool> list(pool, slchead);
    list.appendList(dlchead);
    ok(list.getCount() == 2 * scale, "slc.append(dlc) %u (%u) items", list.getCount(), 2 * scale);
  } /* slc: 300-201, 400-301 */
  {
    LocalSLFifoList<Pool> list(pool, slhead);
    list.prependList(slchead);
    Uint32 c = 0;
    if (list.first(p))
    {
      c ++;
      while (list.next(p)) c ++;
    }
    ok(c == 3 * scale, "sl.prepend(slc) %u (%u) items", c, 3 * scale);
  } /* sl: 300-201, 400-301, 100-1 */
  {
    LocalDLCFifoList<Pool> list(pool, dlchead);
    for (; c_seized < 5 * scale; c_seized ++)
    {
      if (!list.seizeFirst(p)) break;
      p.p->key = c_seized + 1;
    }
    ok(c_seized == 5 * scale, "dlc seized up to %u", c_seized);
  } /* dlc: 500-401 */
  {
    LocalDLFifoList<Pool> list(pool, dlhead);
    list.appendList(dlchead);
    Uint32 c = 0;
    if (list.first(p))
    {
      c ++;
      while (list.next(p)) c ++;
    }
    ok(c == 2 * scale, "dl.append(dlc) %u (%u) items", c, 2 * scale);
  } /* dl: 200-101, 500-401 */
  {
    LocalSLFifoList<Pool> list(pool, slhead);
    list.prependList(dlhead);
    Uint32 c = 0;
    if (list.first(p))
    {
      c ++;
      while (list.next(p)) c ++;
    }
    ok(c == 5 * scale, "sl.prepend(dl) %u (%u) items", c, 5 * scale);
  } /* sl: 200-101, 500-401, 300-201, 400-301, 100-1 */
  ok(slchead.getCount() == 0, "slc.count %u (0)", slchead.getCount());
  ok(dlchead.getCount() == 0, "dlc.count %u (0)", dlchead.getCount());
  {
    LocalSLFifoList<Pool> list(pool, slhead);
    list.first(p);
    ok(p.p->key == 2 * scale, "sl#1: %u (%u)", p.p->key, 2 * scale);
    for (unsigned i = 0; i < 1 * scale; i++) list.next(p);
    ok(p.p->key == 5 * scale, "sl#1: %u (%u)", p.p->key, 5 * scale);
    for (unsigned i = 0; i < 1 * scale; i++) list.next(p);
    ok(p.p->key == 3 * scale, "sl#1: %u (%u)", p.p->key, 3 * scale);
    for (unsigned i = 0; i < 1 * scale; i++) list.next(p);
    ok(p.p->key == 4 * scale, "sl#1: %u (%u)", p.p->key, 4 * scale);
    for (unsigned i = 0; i < 1 * scale; i++) list.next(p);
    ok(p.p->key == 1 * scale, "sl#1: %u (%u)", p.p->key, 1 * scale);
    for (unsigned i = 0; i < 1 * scale; i++) list.next(p);
    ok(p.i == RNIL, "sl#%u %u (RNIL:%u)", 5 * scale + 1, p.i, RNIL);
  }
}

#include <stdlib.h>

int
main(int argc, char **argv)
{
  if (argc == 2)
    scale = atoi(argv[1]);

#ifdef USE_ARRAYPOOL
  (void)test_context(1 * scale);
  ArrayPool<T> pool;

  pool.setSize(10 * scale);
#else
  Pool_context pc = test_context(1 * scale);
  RecordPool<RWPool<T> > pool;
  pool.init(1, pc);
#endif

  plan(0);

  testSLList(pool);
  testDLList(pool);
  testSLCList(pool);
  testDLCList(pool);
  testSLFifoList(pool);
  testDLFifoList(pool);
  testSLCFifoList(pool);
  testDLCFifoList(pool);

  testConcat(pool);

  return exit_status();
}

#endif

