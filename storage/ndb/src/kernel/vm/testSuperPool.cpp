#if 0
make -f Makefile -f - testSuperPool <<'_eof_'
testSuperPool: testSuperPool.cpp libkernel.a LinearPool.hpp
	$(CXXCOMPILE) -o $@ $@.cpp libkernel.a -L../../common/util/.libs -lgeneral
_eof_
exit $?
#endif

/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "SuperPool.hpp"
#include "LinearPool.hpp"
#include <NdbOut.hpp>

#define JAM_FILE_ID 270


template <Uint32 sz>
struct A {
  Uint32 a[sz];
  void fill() {
    Uint32 c = 0;
    for (Uint32 i = 0; i + 1 < sz; i++) {
      a[i] = random();
      c = (c << 1) ^ a[i];
    }
    a[sz - 1] = c;
  }
  void check() {
    Uint32 c = 0;
    for (Uint32 i = 0; i + 1 < sz; i++) {
      c = (c << 1) ^ a[i];
    }
    assert(a[sz - 1] == c);
  }
};

static Uint32
urandom(Uint32 n)
{
  return (Uint32)random() % n;
}

static Uint32
random_coprime(Uint32 n)
{
  Uint32 prime[] = { 101, 211, 307, 401, 503, 601, 701, 809, 907 };
  Uint32 count = sizeof(prime) / sizeof(prime[0]);
  assert(n != 0);
  while (1) {
    Uint32 i = urandom(count);
    if (n % prime[i] != 0)
      return prime[i];
  }
}

static int
cmpPtrI(const void* a, const void* b)
{
  Ptr<const void> u = *(Ptr<const void>*)a;
  Ptr<const void> v = *(Ptr<const void>*)b;
  return u.i < v.i ? -1 : u.i > v.i ? +1 : 0;
}

static int
cmpPtrP(const void* a, const void* b)
{
  Ptr<const void> u = *(Ptr<const void>*)a;
  Ptr<const void> v = *(Ptr<const void>*)b;
  return u.p < v.p ? -1 : u.p > v.p ? +1 : 0;
}

static Uint32 loopcount = 3;

template <class T>
static void
sp_test(GroupPool& gp)
{
  SuperPool& sp = gp.m_superPool;
  RecordPool<T> rp(gp);
  assert(gp.m_totPages == gp.m_freeList.m_pageCount);
  SuperPool::RecInfo& ri = rp.m_recInfo;
  Uint32 pageCount = sp.m_totPages;
  Uint32 perPage = rp.m_recInfo.m_maxPerPage;
  Uint32 perPool = perPage * pageCount;
  ndbout << "pages=" << pageCount << " perpage=" << perPage << " perpool=" << perPool << endl;
  Ptr<T>* ptrList = new Ptr<T> [perPool];
  memset(ptrList, 0x1f, perPool * sizeof(Ptr<T>));
  Uint32 verify = 1000;
  Uint32 useCount;
  Uint32 loop;
  for (loop = 0; loop < loopcount; loop++) {
    ndbout << "loop " << loop << endl;
    Uint32 i, j;
    // seize all
    ndbout << "seize all" << endl;
    for (i = 0; i < perPool + 1; i++) {
      if (verify == 0 || urandom(perPool) < verify)
        sp.verify(ri);
      j = i;
      Ptr<T> ptr1 = { 0, RNIL };
      if (! rp.seize(ptr1))
        break;
      ptr1.p->fill();
      ptr1.p->check();
      Ptr<T> ptr2 = { 0, ptr1.i };
      rp.getPtr(ptr2);
      assert(ptr1.i == ptr2.i && ptr1.p == ptr2.p);
      ptrList[j] = ptr1;
    }
    sp.verify(ri);
    ndbout << "seized " << i << endl;
    assert(i == perPool);
    useCount = sp.getRecUseCount(ri);
    assert(useCount == perPool);
    // check duplicates
    ndbout << "check dups" << endl;
    {
      Ptr<T>* ptrList2 = new Ptr<T> [perPool];
      memcpy(ptrList2, ptrList, perPool * sizeof(Ptr<T>));
      qsort(ptrList2, perPool, sizeof(Ptr<T>), cmpPtrI);
      for (i = 1; i < perPool; i++)
        assert(ptrList2[i - 1].i != ptrList2[i].i);
      qsort(ptrList2, perPool, sizeof(Ptr<T>), cmpPtrP);
      for (i = 1; i < perPool; i++)
        assert(ptrList2[i - 1].p != ptrList2[i].p);
      delete [] ptrList2;
    }
    // release all in various orders
    ndbout << "release all" << endl;
    Uint32 coprime = random_coprime(perPool);
    for (i = 0; i < perPool; i++) {
      if (verify == 0 || urandom(perPool) < verify)
        sp.verify(ri);
      switch (loop % 3) {
      case 0:   // ascending
        j = i;
        break;
      case 1:   // descending
        j = perPool - 1 - i;
        break;
      case 2:   // pseudo-random
        j = (coprime * i) % perPool;
        break;
      }
      Ptr<T>& ptr = ptrList[j];
      assert(ptr.i != RNIL && ptr.p != 0);
      ptr.p->check();
      rp.release(ptr);
      assert(ptr.i == RNIL && ptr.p == 0);
    }
    sp.verify(ri);
    useCount = sp.getRecUseCount(ri);
    assert(useCount == 0);
    // seize/release at random
    ndbout << "seize/release at random" << endl;
    for (i = 0; i < loopcount * perPool; i++) {
      if (verify == 0 || urandom(perPool) < verify)
        sp.verify(ri);
      j = urandom(perPool);
      Ptr<T>& ptr = ptrList[j];
      if (ptr.i == RNIL) {
        if (rp.seize(ptr))
          ptr.p->fill();
      } else {
        ptr.p->check();
        rp.release(ptr);
      }
    }
    ndbout << "used " << ri.m_useCount << endl;
    sp.verify(ri);
    // release all
    ndbout << "release all" << endl;
    for (i = 0; i < perPool; i++) {
      if (verify == 0 || urandom(perPool) < verify)
        sp.verify(ri);
      j = i;
      Ptr<T>& ptr = ptrList[j];
      if (ptr.i != RNIL) {
        ptr.p->check();
        rp.release(ptr);
      }
    }
    sp.verify(ri);
    useCount = sp.getRecUseCount(ri);
    assert(useCount == 0);
  }
  // done
  delete [] ptrList;
}

template <class T>
static void
lp_test(GroupPool& gp)
{
  SuperPool& sp = gp.m_superPool;
  LinearPool<T, 5> lp(gp);
  ndbout << "linear pool test" << endl;
  Ptr<T> ptr;
  Uint32 loop;
  for (loop = 0; loop < loopcount; loop++) {
    int count = 0;
    while (1) {
      bool ret = lp.seize(ptr);
      lp.verify();
      if (! ret)
        break;
      assert(ptr.i == count);
      Ptr<T> ptr2;
      ptr2.i = ptr.i;
      ptr2.p = 0;
      lp.getPtr(ptr2);
      assert(ptr.p == ptr2.p);
      count++;
    }
    assert(count != 0);
    ndbout << "seized " << count << endl;
    switch (loop % 3) {
    case 0:
      {
        int n = 0;
        while (n < count) {
          ptr.i = n;
          lp.release(ptr);
          lp.verify();
          n++;
        }
        ndbout << "released in order" << endl;
      }
      break;
    case 1:
      {
        int n = count;
        while (n > 0) {
          n--;
          ptr.i = n;
          lp.release(ptr);
          lp.verify();
        }
        ndbout << "released in reverse" << endl;
      }
      break;
    default:
      {
        int coprime = random_coprime(count);
        int n = 0;
        while (n < count) {
          int m = (coprime * n) % count;
          ptr.i = m;
          lp.release(ptr);
          lp.verify();
          n++;
        }
        ndbout << "released at random" << endl;
      }
      break;
    }
    { Uint32 cnt = lp.count(); assert(cnt == 0); }
    // seize_index test
    char *used = new char [10 * count];
    memset(used, false, sizeof(used));
    Uint32 i, ns = 0, nr = 0;
    for (i = 0; i < count; i++) {
      Uint32 index = urandom(10 * count);
      if (used[index]) {
        ptr.i = index;
        lp.release(ptr);
        lp.verify();
        nr++;
      } else {
        int i = lp.seize_index(ptr, index);
        assert(i >= 0);
        lp.verify();
        if (i == 0) // no space
          continue;
        assert(ptr.i == index);
        Ptr<T> ptr2;
        ptr2.i = ptr.i;
        ptr2.p = 0;
        lp.getPtr(ptr2);
        assert(ptr.p == ptr2.p);
        ns++;
      }
      used[index] = ! used[index];
    }
    ndbout << "random sparse seize " << ns << " release " << nr << endl;
    nr = 0;
    for (i = 0; i < 10 * count; i++) {
      if (used[i]) {
        ptr.i = i;
        lp.release(ptr);
        lp.verify();
        used[i] = false;
        nr++;
      }
    }
    ndbout << "released " << nr << endl;
    { Uint32 cnt = lp.count(); assert(cnt == 0); }
  }
}

static Uint32 pageSize = 32768;
static Uint32 pageBits = 17;

const Uint32 sz1 = 3;
const Uint32 sz2 = 4;
const Uint32 sz3 = 53;
const Uint32 sz4 = 424;
const Uint32 sz5 = 5353;

typedef A<sz1> T1;
typedef A<sz2> T2;
typedef A<sz3> T3;
typedef A<sz4> T4;
typedef A<sz5> T5;

template static void sp_test<T1>(GroupPool& sp);
template static void sp_test<T2>(GroupPool& sp);
template static void sp_test<T3>(GroupPool& sp);
template static void sp_test<T4>(GroupPool& sp);
template static void sp_test<T5>(GroupPool& sp);
//
template static void lp_test<T3>(GroupPool& sp);

int
main(int argc, char** argv)
{
  if (argc > 1 && strncmp(argv[1], "-l", 2) == 0)
    loopcount = atoi(argv[1] + 2);
  HeapPool sp(pageSize, pageBits);
  sp.setInitPages(7);
  sp.setMaxPages(7);
  if (! sp.allocMemory())
    assert(false);
  GroupPool gp(sp);
  Uint16 s = (Uint16)getpid();
  srandom(s);
  ndbout << "rand " << s << endl;
  int count;
  count = 0;
  while (++count <= 0) { // change to 1 to find new bug
    sp_test<T1>(gp);
    sp_test<T2>(gp);
    sp_test<T3>(gp);
    sp_test<T4>(gp);
    sp_test<T5>(gp);
  }
  count = 0;
  while (++count <= 1) {
    lp_test<T3>(gp);
  }
  return 0;
}
