#if 0
make -f Makefile -f - testSuperPool <<'_eof_'
testSuperPool: testSuperPool.cpp libkernel.a
	$(CXXCOMPILE) -o $@ $@.cpp libkernel.a -L../../common/util/.libs -lgeneral
_eof_
exit $?
#endif

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

#include "SuperPool.hpp"
#include <NdbOut.hpp>

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

template <Uint32 sz>
void
sp_test(SuperPool& sp)
{
  typedef A<sz> T;
  RecordPool<T> rp(sp);
  SuperPool::RecInfo& ri = rp.m_recInfo;
  Uint32 pageCount = sp.m_totalSize / sp.m_pageSize;
  Uint32 perPage = rp.m_recInfo.m_maxUseCount;
  Uint32 perPool = perPage * pageCount;
  ndbout << "pages=" << pageCount << " perpage=" << perPage << " perpool=" << perPool << endl;
  Ptr<T>* ptrList = new Ptr<T> [perPool];
  memset(ptrList, 0x1f, perPool * sizeof(Ptr<T>));
  Uint32 loop;
  for (loop = 0; loop < loopcount; loop++) {
    ndbout << "loop " << loop << endl;
    Uint32 i, j;
    // seize all
    ndbout << "seize all" << endl;
    for (i = 0; i < perPool + 1; i++) {
      j = i;
      sp.verify(ri);
      Ptr<T> ptr1 = { 0, RNIL };
      if (! rp.seize(ptr1))
        break;
      // write value
      ptr1.p->fill();
      ptr1.p->check();
      // verify getPtr
      Ptr<T> ptr2 = { 0, ptr1.i };
      rp.getPtr(ptr2);
      assert(ptr1.i == ptr2.i && ptr1.p == ptr2.p);
      // save
      ptrList[j] = ptr1;
    }
    assert(i == perPool);
    assert(ri.m_totalUseCount == perPool && ri.m_totalRecCount == perPool);
    sp.verify(ri);
    // check duplicates
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
    sp.setCurrPage(ri, RNIL);
    assert(ri.m_totalUseCount == 0 && ri.m_totalRecCount == 0);
    sp.verify(ri);
    // seize/release at random
    ndbout << "seize/release at random" << endl;
    for (i = 0; i < loopcount * perPool; i++) {
      j = urandom(perPool);
      Ptr<T>& ptr = ptrList[j];
      if (ptr.i == RNIL) {
        rp.seize(ptr);
        ptr.p->fill();
      } else {
        ptr.p->check();
        rp.release(ptr);
      }
    }
    ndbout << "used " << ri.m_totalUseCount << endl;
    sp.verify(ri);
    // release all
    ndbout << "release all" << endl;
    for (i = 0; i < perPool; i++) {
      j = i;
      Ptr<T>& ptr = ptrList[j];
      if (ptr.i != RNIL) {
        ptr.p->check();
        rp.release(ptr);
      }
    }
    sp.setCurrPage(ri, RNIL);
    assert(ri.m_totalUseCount == 0 && ri.m_totalRecCount == 0);
    sp.verify(ri);
  }
  // done
  delete [] ptrList;
}

static Uint32 pageCount = 99;
static Uint32 pageSize = 32768;
static Uint32 pageBits = 15;

const Uint32 sz1 = 3, sz2 = 4, sz3 = 53, sz4 = 424, sz5 = 5353;

template void sp_test<sz1>(SuperPool& sp);
template void sp_test<sz2>(SuperPool& sp);
template void sp_test<sz3>(SuperPool& sp);
template void sp_test<sz4>(SuperPool& sp);
template void sp_test<sz5>(SuperPool& sp);

int
main()
{
  HeapPool sp(pageSize, pageBits);
  sp.setSizes(pageCount * pageSize);
  if (! sp.init())
    assert(false);
  Uint16 s = (Uint16)getpid();
  srandom(s);
  ndbout << "rand " << s << endl;
  sp_test<sz1>(sp);
  sp_test<sz2>(sp);
  sp_test<sz3>(sp);
  sp_test<sz4>(sp);
  sp_test<sz5>(sp);
  return 0;
}
