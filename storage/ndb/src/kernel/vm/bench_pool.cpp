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


#include "NdbdSuperPool.hpp"
#include "ArrayPool.hpp"
#include <NdbTick.h>
#include "ndbd_malloc_impl.hpp"

template <typename T>
inline
void
init(ArrayPool<T> & pool, Uint32 cnt)
{
  pool.setSize(cnt + 1);
}

template <typename T>
inline
void
init(RecordPool<T> & pool, Uint32 cnt)
{
}


template<typename T, typename R>
inline
void 
test_pool(R& pool, Uint32 cnt, Uint32 loops)
{
  init(pool, cnt);
  Ptr<T> ptr;
  Uint32 *arr = (Uint32*)alloca(cnt * sizeof(Uint32));
  {
    printf(" ; seize "); fflush(stdout);
    Uint64 sum = 0;
    for(Uint32 i = 0; i<loops; i++)
    {
      Uint64 start = NdbTick_CurrentMillisecond();
      for(Uint32 j = 0; j<cnt; j++)
      {
	bool b = pool.seize(ptr);
	arr[j] = ptr.i;
	assert(b);
      }
      Uint64 stop = NdbTick_CurrentMillisecond();      

      for(Uint32 j = 0; j<cnt; j++)
      {
	ptr.i = arr[j];
	pool.getPtr(ptr);
	pool.release(ptr);
	arr[j] = RNIL;
      }
      
      sum += (stop - start);
      if (i == 0)
	printf("; first ; %lld", (stop - start));
    }
    printf(" ; avg ; %lld ; tot ; %lld", sum/loops, sum);fflush(stdout);
  }

  {
    printf(" ; release "); fflush(stdout);
    Uint64 sum = 0;
    for(Uint32 i = 0; i<loops; i++)
    {
      for(Uint32 j = 0; j<cnt; j++)
      {
	bool b = pool.seize(ptr);
	arr[j] = ptr.i;
	assert(b);
      }

      Uint64 start = NdbTick_CurrentMillisecond();
      for(Uint32 j = 0; j<cnt; j++)
      {
	ptr.i = arr[j];
	pool.release(ptr);
	arr[j] = RNIL;
      }
      Uint64 stop = NdbTick_CurrentMillisecond();      
      
      sum += (stop - start);
    }
    printf("; avg ; %lld ; tot ; %lld", sum/loops, sum); fflush(stdout);
  }

  {
    printf(" ; mix"); fflush(stdout);
    
    Uint64 sum = 0;
    Uint64 start = NdbTick_CurrentMillisecond();
    for(Uint32 i = 0; i<loops * cnt; i++)
    {
      int pos = rand() % cnt;
      ptr.i = arr[pos];
      if (ptr.i == RNIL)
      {
	pool.seize(ptr);
	arr[pos] = ptr.i;
	assert(ptr.i != RNIL);
      }
      else
      {
	pool.release(ptr);
	arr[pos] = RNIL;
      }
    }
    Uint64 stop = NdbTick_CurrentMillisecond();    
    
    for(Uint32 j = 0; j<cnt; j++)
    {
      ptr.i = arr[j];
      if (ptr.i != RNIL)
      {
	pool.getPtr(ptr);
	pool.release(ptr);
      }
      arr[j] = RNIL;
    }
    
    sum += (stop - start);
    printf(" ; %lld", sum); fflush(stdout);
  }
  
  {
    printf(" ; getPtr"); fflush(stdout);

    for(Uint32 j = 0; j<cnt; j++)
    {
      bool b = pool.seize(ptr);
      arr[j] = ptr.i;
      assert(b);
    }

    Uint64 sum = 0;
    Uint64 start = NdbTick_CurrentMillisecond();
    for(Uint32 i = 0; i<loops * cnt; i++)
    {
      int pos = rand() % cnt;
      ptr.i = arr[pos];
      pool.getPtr(ptr);
    }
    Uint64 stop = NdbTick_CurrentMillisecond();    

    for(Uint32 j = 0; j<cnt; j++)
    {
      ptr.i = arr[j];
      pool.getPtr(ptr);
      pool.release(ptr);
      arr[j] = RNIL;
    }
    
    sum += (stop - start);
    printf(" ; %lld", sum); fflush(stdout);
  }
  ndbout_c("");
}

template <Uint32 sz> struct Rec { char data[sz-4]; Uint32 nextPool; };
typedef Rec<32> Rec32;
typedef Rec<36> Rec36;
typedef Rec<256> Rec256;
typedef Rec<260> Rec260;

Ndbd_mem_manager mem;

template <typename T>
inline
void test_rp(Uint32 cnt, Uint32 loop, Uint32 pgsz)
{
  printf("RP ; %d ; ws ; %d ; page ; %d", 
	 sizeof(T), (sizeof(T)*cnt) >> 10, pgsz >> 10);
  NdbdSuperPool sp(mem, pgsz, 19);
  GroupPool gp(sp);
  sp.init_1();
  sp.init_2();
  
  sp.setInitPages(4);
  sp.setIncrPages(4);
  sp.setMaxPages(~0);
  sp.allocMemory();
  
  RecordPool<T> pool(gp);
  test_pool<T, RecordPool<T> >(pool, cnt, loop);
}

template <typename T>
inline
void test_ap(Uint32 cnt, Uint32 loop)
{
  printf("AP ; %d ; ws ; %d ; page ; n/a", sizeof(T), (cnt * sizeof(T))>>10);
  ArrayPool<T> pool;
  test_pool<T, ArrayPool<T> >(pool, cnt, loop);
}

int
main(int argc, char **argv)
{
  mem.init(10000);

  Uint32 cnt = 100;
  Uint32 loop = 300000;

  while(cnt <= 1000000)
  {
    test_rp<Rec32>(cnt, loop, 8192);
    test_rp<Rec32>(cnt, loop, 32768);
    test_ap<Rec32>(cnt, loop);
    
    test_rp<Rec36>(cnt, loop, 8192);
    test_rp<Rec36>(cnt, loop, 32768);
    test_ap<Rec36>(cnt, loop);

    test_rp<Rec256>(cnt, loop, 8192);
    test_rp<Rec256>(cnt, loop, 32768);
    test_ap<Rec256>(cnt, loop);

    test_rp<Rec260>(cnt, loop, 8192);
    test_rp<Rec260>(cnt, loop, 32768);
    test_ap<Rec260>(cnt, loop);

    cnt *= 100;
    loop /= 100;
  }
}

void
ErrorReporter::handleAssert(const char * msg, const char * file, 
			    int line, int)
{
  ndbout << "ErrorReporter::handleAssert activated - " 
	 << " line= " << line << endl;
  abort();
}
