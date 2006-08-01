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


#include "ArrayPool.hpp"
#include "WOPool.hpp"
#include "RWPool.hpp"
#include <NdbTick.h>
#include "ndbd_malloc_impl.hpp"
#include "SimulatedBlock.hpp"

#ifdef USE_CALLGRIND
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_TOGGLE_COLLECT()
#endif

#define T_TEST_AP   (1 << 0)
#define T_TEST_WO   (1 << 1)
#define T_TEST_RW   (1 << 2)

#define T_SEIZE       (1 << 0)
#define T_RELEASE     (1 << 1)
#define T_G_RELEASE   (1 << 2)
#define T_R_RELEASE   (1 << 3)
#define T_R_G_RELEASE (1 << 4)
#define T_MIX         (1 << 5)
#define T_GETPTR      (1 << 6)
#define T_FIFO        (1 << 7)

const char *test_names[] = {
  "seize",
  "release",
  "get+rel",
  "r-rel",
  "r-get+rel",
  "mix",
  "getptr",
  "fifo",
  0
};

Uint32 pools = ~0;
Uint32 tests = ~0;
Uint32 records = ~0;
Uint32 sizes = 7;
unsigned int seed;
Ndbd_mem_manager mm;
Configuration cfg;
Block_context ctx(cfg, mm);
struct BB : public SimulatedBlock
{
  BB(int no, Block_context& ctx) : SimulatedBlock(no, ctx) {}
};

BB block(DBACC, ctx);

template <typename T>
void
init(ArrayPool<T> & pool, Uint32 cnt)
{
  pool.setSize(cnt + 1, true);
}

template <typename T>
void
init(RecordPool<T, WOPool> & pool, Uint32 cnt)
{
  Pool_context pc;
  pc.m_block = &block;
  pool.wo_pool_init(0x2001, pc);
}

template <typename T>
void
init(RecordPool<T, RWPool> & pool, Uint32 cnt)
{
  Pool_context pc;
  pc.m_block = &block;
  pool.init(0x2001, pc);
}

template <typename T, typename R>
void 
test_pool(R& pool, Uint32 cnt, Uint32 loops)
{
  Ptr<T> ptr;
  Uint32 *arr = (Uint32*)alloca(cnt * sizeof(Uint32));
  bzero(arr, cnt * sizeof(Uint32));
  if (tests & T_SEIZE)
  {
    Uint64 sum = 0;
    for(Uint32 i = 0; i<loops; i++)
    { // seize
      Uint64 start = NdbTick_CurrentMillisecond();
      CALLGRIND_TOGGLE_COLLECT();
      for(Uint32 j = 0; j<cnt; j++)
      {
	bool b = pool.seize(ptr);
	arr[j] = ptr.i;
	ptr.p->do_stuff();
	assert(b);
      }
      CALLGRIND_TOGGLE_COLLECT();
      Uint64 stop = NdbTick_CurrentMillisecond();      

      for(Uint32 j = 0; j<cnt; j++)
      {
	ptr.i = arr[j];
	pool.getPtr(ptr);
	ptr.p->do_stuff();
	pool.release(ptr.i);
	arr[j] = RNIL;
      }
      sum += (stop - start);
    }
    printf(" ; %lld", sum); fflush(stdout);
  }
  
  if (tests & T_RELEASE)
  { // release
    Uint64 sum = 0;
    for(Uint32 i = 0; i<loops; i++)
    {
      for(Uint32 j = 0; j<cnt; j++)
      {
	bool b = pool.seize(ptr);
	arr[j] = ptr.i;
	ptr.p->do_stuff();
      }

      Uint64 start = NdbTick_CurrentMillisecond();
      CALLGRIND_TOGGLE_COLLECT();
      for(Uint32 j = 0; j<cnt; j++)
      {
	pool.release(arr[j]);
	arr[j] = RNIL;
      }
      CALLGRIND_TOGGLE_COLLECT();
      Uint64 stop = NdbTick_CurrentMillisecond();      
      
      sum += (stop - start);
    }
    printf(" ; %lld", sum); fflush(stdout);
  }
  
  if (tests & T_G_RELEASE)
  { // getptr + release
    Uint64 sum = 0;
    for(Uint32 i = 0; i<loops; i++)
    {
      for(Uint32 j = 0; j<cnt; j++)
      {
	bool b = pool.seize(ptr);
	arr[j] = ptr.i;
	ptr.p->do_stuff();
      }

      Uint64 start = NdbTick_CurrentMillisecond();
      CALLGRIND_TOGGLE_COLLECT();
      for(Uint32 j = 0; j<cnt; j++)
      {
	pool.getPtr(ptr, arr[j]);
	ptr.p->do_stuff();
	pool.release(ptr);
	arr[j] = RNIL;
      }
      CALLGRIND_TOGGLE_COLLECT();
      Uint64 stop = NdbTick_CurrentMillisecond();      
      
      sum += (stop - start);
    }
    printf(" ; %lld", sum); fflush(stdout);
  }

  if (tests & T_R_RELEASE)
  { // release reverse
    Uint64 sum = 0;
    for(Uint32 i = 0; i<loops; i++)
    {
      for(Uint32 j = 0; j<cnt; j++)
      {
	bool b = pool.seize(ptr);
	arr[j] = ptr.i;
	ptr.p->do_stuff();
      }

      Uint64 start = NdbTick_CurrentMillisecond();
      CALLGRIND_TOGGLE_COLLECT();
      for(Uint32 j = 0; j<cnt; j++)
      {
	pool.release(arr[cnt - j - 1]);
	arr[cnt - j - 1] = RNIL;
      }
      CALLGRIND_TOGGLE_COLLECT();
      Uint64 stop = NdbTick_CurrentMillisecond();      
      
      sum += (stop - start);
    }
    printf(" ; %lld", sum); fflush(stdout);
  }
  
  if (tests & T_R_G_RELEASE)
  { // getptr + release
    Uint64 sum = 0;
    for(Uint32 i = 0; i<loops; i++)
    {
      for(Uint32 j = 0; j<cnt; j++)
      {
	bool b = pool.seize(ptr);
	arr[j] = ptr.i;
	ptr.p->do_stuff();
      }

      Uint64 start = NdbTick_CurrentMillisecond();
      CALLGRIND_TOGGLE_COLLECT();
      for(Uint32 j = 0; j<cnt; j++)
      {
	pool.getPtr(ptr, arr[cnt - j - 1]);
	ptr.p->do_stuff();
	pool.release(ptr);
	arr[cnt - j - 1] = RNIL;
      }
      CALLGRIND_TOGGLE_COLLECT();
      Uint64 stop = NdbTick_CurrentMillisecond();      
      
      sum += (stop - start);
    }
    printf(" ; %lld", sum); fflush(stdout);
  }
  
  if (tests & T_MIX)
  {
    Uint64 sum = 0;
    Uint64 start = NdbTick_CurrentMillisecond();
    Uint32 lseed = seed;
    CALLGRIND_TOGGLE_COLLECT();
    for(Uint32 i = 0; i<loops * cnt; i++)
    {
      int pos = rand_r(&lseed) % cnt;
      ptr.i = arr[pos];
      if (ptr.i == RNIL)
      {
	pool.seize(ptr);
	arr[pos] = ptr.i;
	assert(ptr.i != RNIL);
	ptr.p->do_stuff();
      }
      else
      {
	pool.getPtr(ptr);
	ptr.p->do_stuff();
	pool.release(ptr);
	arr[pos] = RNIL;
      }
    }
    CALLGRIND_TOGGLE_COLLECT();
    Uint64 stop = NdbTick_CurrentMillisecond();    
    
    for(Uint32 j = 0; j<cnt; j++)
    {
      ptr.i = arr[j];
      if (ptr.i != RNIL)
      {
	pool.getPtr(ptr);
	pool.release(ptr.i);
      }
      arr[j] = RNIL;
    }
    
    sum += (stop - start);
    printf(" ; %lld", sum); fflush(stdout);
  }
  
  if (tests & T_GETPTR)
  {
    Uint32 lseed = seed;
    for(Uint32 j = 0; j<cnt; j++)
    {
      bool b = pool.seize(ptr);
      arr[j] = ptr.i;
      ptr.p->do_stuff();
      assert(b);
    }

    Uint64 sum = 0;
    Uint64 start = NdbTick_CurrentMillisecond();
    CALLGRIND_TOGGLE_COLLECT();
    for(Uint32 i = 0; i<loops * cnt; i++)
    {
      int pos = rand_r(&lseed) % cnt;
      ptr.i = arr[pos];
      pool.getPtr(ptr);
      ptr.p->do_stuff();
    }
    CALLGRIND_TOGGLE_COLLECT();
    Uint64 stop = NdbTick_CurrentMillisecond();    
    
    for(Uint32 j = 0; j<cnt; j++)
    {
      ptr.i = arr[j];
      pool.getPtr(ptr);
      ptr.p->do_stuff();
      pool.release(ptr.i);
      arr[j] = RNIL;
    }
    
    sum += (stop - start);
    printf(" ; %lld", sum); fflush(stdout);
  }

  if (tests & T_FIFO)
  { // fifo
    Uint64 sum = 0;
    Uint64 start = NdbTick_CurrentMillisecond();
    CALLGRIND_TOGGLE_COLLECT();
    for(Uint32 i = 0; i<loops; i++)
    {
      Uint32 head = RNIL;
      Uint32 last = RNIL;
      
      Uint64 sum = 0;
      for(Uint32 j = 0; j<cnt; j++)
      {
	pool.seize(ptr);
	ptr.p->do_stuff();
	ptr.p->m_nextList = RNIL;
	if (head == RNIL)
	{
	  head = ptr.i;
	}
	else
	{
	  T* t = pool.getPtr(last);
	  t->m_nextList = ptr.i;
	}
	last = ptr.i;
      }

      while (head != RNIL)
      {
	pool.getPtr(ptr, head);
	ptr.p->do_stuff();
	head = ptr.p->m_nextList;
	pool.release(ptr);
      }
    }      
    CALLGRIND_TOGGLE_COLLECT();
    Uint64 stop = NdbTick_CurrentMillisecond();    
    sum += (stop - start);
    printf(" ; %lld", sum); fflush(stdout);
  }
  
  ndbout_c("");
}

template <Uint32 sz> 
struct Rec { 
  Uint32 m_data;
  Uint32 m_magic; 
  Uint32 nextPool; 
  Uint32 m_nextList;
  char m_cdata[sz-16];

  void do_stuff() { 
    Uint32 sum = 0; 
    Uint32 *ptr = (Uint32*)this; 
    for(Uint32 i = 0; i<(sz >> 2); i++)
      sum += * ptr ++;
    m_data = sum;
  }
};

typedef Rec<32> Rec32;
typedef Rec<36> Rec36;
typedef Rec<56> Rec56;
typedef Rec<224> Rec224;

template <typename T>
void test_ap(Uint32 cnt, Uint32 loop)
{
  printf("AP ; %d ; %d", sizeof(T), (cnt * sizeof(T))>>10); fflush(stdout);
  ArrayPool<T> pool;
  init(pool, cnt);
  test_pool<T, ArrayPool<T> >(pool, cnt, loop);
}

template <typename T>
void test_rw(Uint32 cnt, Uint32 loop)
{
  printf("RW ; %d ; %d", sizeof(T), (cnt * sizeof(T))>>10); fflush(stdout);
  RecordPool<T, RWPool> pool;
  init(pool, cnt);
  test_pool<T, RecordPool<T, RWPool> >(pool, cnt, loop);
}

template <typename T>
void test_wo(Uint32 cnt, Uint32 loop)
{
  printf("WO ; %d ; %d", sizeof(T), (cnt * sizeof(T))>>10); fflush(stdout);
  RecordPool<T, WOPool> pool;
  init(pool, cnt);
  test_pool<T, RecordPool<T, WOPool> >(pool, cnt, loop);
}

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

int
main(int argc, char **argv)
{
  Uint32 loops = 300000;
  for (Uint32 i = 1 ; i<argc ; i++)
  {
    if (argc > i+1 && strcmp(argv[i], "-pools") == 0)
    {
      pools = 0;
      for (Uint32 j = 0; j<strlen(argv[i+1]); j++)
      {
	char c = argv[i+1][j];
	if (c >= '0' && c <= '9')
	  pools |= 1 << (c - '0');
	else 
	  pools |= 1 << (10 + (c - 'a'));
      }
    }
    else if (argc > i+1 && strcmp(argv[i], "-tests") == 0)
    {
      tests = 0;
      for (Uint32 j = 0; j<strlen(argv[i+1]); j++)
      {
	char c = argv[i+1][j];
	if (c >= '0' && c <= '9')
	  tests |= 1 << (c - '0');
	else 
	  tests |= 1 << (10 + (c - 'a'));
      }
    }
    else if (argc > i+1 && strcmp(argv[i], "-sizes") == 0)
    {
      sizes = 0;
      for (Uint32 j = 0; j<strlen(argv[i+1]); j++)
      {
	char c = argv[i+1][j];
	if (c >= '0' && c <= '9')
	  sizes |= 1 << (c - '0');
	else
	  sizes |= 1 << (10 + (c - 'a'));
      }
    }
    else if (argc > i+1 && strcmp(argv[i], "-records") == 0)
    {
      records = 0;
      for (Uint32 j = 0; j<strlen(argv[i+1]); j++)
      {
	char c = argv[i+1][j];
	if (c >= '0' && c <= '9')
	  records |= 1 << (c - '0');
	else
	  records |= 1 << (10 + (c - 'a'));
      }
    }
    else if (argc > i+1 && strcmp(argv[i], "-loop") == 0)
    {
      loops = atoi(argv[i+1]);
    }
  }
  
  Resource_limit rl;
  rl.m_min = 0;
  rl.m_max = 10000;
  rl.m_resource_id = 0;
  mm.set_resource_limit(rl);
  if(!mm.init())
  {
    abort();
  }

  seed = time(0);
  Uint32 sz = 0;
  Uint32 cnt = 256;

  printf("pool ; rs ; ws");
  for (Uint32 i = 0; test_names[i] && i<31; i++)
    if (tests & (1 << i))
      printf(" ; %s", test_names[i]);
  ndbout_c("");

  while(cnt <= 1000000)
  {
    Uint32 loop = 768 * loops / cnt;
    if (sizes & (1 << sz))
    {
      if (records & 1)
      {
	if (pools & T_TEST_AP)
	  test_ap<Rec32>(cnt, loop);
	if (pools & T_TEST_WO)
	  test_wo<Rec32>(cnt, loop);
	if (pools & T_TEST_RW)
	  test_rw<Rec32>(cnt, loop);
      }
      if (records & 2)
      {
	if (pools & T_TEST_AP)
	  test_ap<Rec36>(cnt, loop);
	if (pools & T_TEST_WO)
	  test_wo<Rec36>(cnt, loop);
	if (pools & T_TEST_RW)
	  test_rw<Rec36>(cnt, loop);
      }
      if (records & 4)
      {
	if (pools & T_TEST_AP)
	  test_ap<Rec56>(cnt, loop);
	if (pools & T_TEST_WO)
	  test_wo<Rec56>(cnt, loop);
	if (pools & T_TEST_RW)
	  test_rw<Rec56>(cnt, loop);
      }
      if (records & 8)
      {
	if (pools & T_TEST_AP)
	  test_ap<Rec224>(cnt, loop);
	if (pools & T_TEST_WO)
	  test_wo<Rec224>(cnt, loop);
	if (pools & T_TEST_RW)
	  test_rw<Rec224>(cnt, loop);
      }
    }

    cnt += (512 << sz);
    sz++;
  }
}

Uint32 g_currentStartPhase;
Uint32 g_start_type;
NdbNodeBitmask g_nowait_nodes;

void childExit(int code, Uint32 currentStartPhase)
{
  abort();
}

void childAbort(int code, Uint32 currentStartPhase)
{
  abort();
}

void childReportError(int error)
{
  abort();
}

void
UpgradeStartup::sendCmAppChg(Ndbcntr& cntr, Signal* signal, Uint32 startLevel){
}

void
UpgradeStartup::execCM_APPCHG(SimulatedBlock & block, Signal* signal){
}

void
UpgradeStartup::sendCntrMasterReq(Ndbcntr& cntr, Signal* signal, Uint32 n){
}

void
UpgradeStartup::execCNTR_MASTER_REPLY(SimulatedBlock & block, Signal* signal){
}

#include <SimBlockList.hpp>

void
SimBlockList::unload()
{

}

#define INSTANCE(X) \
template void test_ap<X>(unsigned, unsigned);\
template void test_wo<X>(unsigned, unsigned);\
template void test_rw<X>(unsigned, unsigned);\
template void test_pool<X, ArrayPool<X> >(ArrayPool<X>&, unsigned, unsigned);\
template void test_pool<X, RecordPool<X, RWPool> >(RecordPool<X, RWPool>&, unsigned, unsigned); \
template void test_pool<X, RecordPool<X, WOPool> >(RecordPool<X, WOPool>&, unsigned, unsigned);\
template void init<X>(ArrayPool<X>&, unsigned);\
template void init<X>(RecordPool<X, RWPool>&, unsigned);\
template void init<X>(RecordPool<X, WOPool>&, unsigned)

INSTANCE(Rec32);
INSTANCE(Rec36);
INSTANCE(Rec56);
INSTANCE(Rec224);

