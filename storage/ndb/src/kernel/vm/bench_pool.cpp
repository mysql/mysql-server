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

#include <valgrind/callgrind.h>

#define T_TEST_AP   (1 << 0)
#define T_TEST_WO   (1 << 1)
#define T_TEST_RW   (1 << 2)

#define T_SEIZE     (1 << 1)
#define T_RELEASE   (1 << 2)
#define T_MIX       (1 << 3)

#define T_L_SEIZE   (1 << 4)
#define T_L_RELEASE (1 << 5)
#define T_L_MIX     (1 << 6)

Uint32 tests = ~0;
Uint32 sizes = 7;
unsigned int seed;
Ndbd_mem_manager mm;
Configuration cfg;
Block_context ctx = { cfg, mm };
struct BB : public SimulatedBlock
{
  BB(int no, Block_context& ctx) : SimulatedBlock(no, ctx) {}
};

BB block(DBACC, ctx);

template <typename T>
void
init(ArrayPool<T> & pool, Uint32 cnt)
{
  pool.setSize(cnt + 1);
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
  {
    printf(" ; seize "); fflush(stdout);
    Uint64 sum = 0;
    for(Uint32 i = 0; i<loops; i++)
    {
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
	pool.release(ptr.i);
	arr[j] = RNIL;
      }

      sum += (stop - start);
      if (i == 0)
      {
	printf("; first ; %lld", (stop - start));
	fflush(stdout);
      }
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
    printf("; avg ; %lld ; tot ; %lld", sum/loops, sum); fflush(stdout);
  }

  {
    printf(" ; mix"); fflush(stdout);
    
    Uint64 sum = 0;
    Uint64 start = NdbTick_CurrentMillisecond();
    CALLGRIND_TOGGLE_COLLECT();
    for(Uint32 i = 0; i<loops * cnt; i++)
    {
      int pos = my_rand(&seed) % cnt;
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
	pool.release(ptr.i);
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
    CALLGRIND_TOGGLE_COLLECT();
    for(Uint32 i = 0; i<loops * cnt; i++)
    {
      int pos = my_rand(&seed) % cnt;
      ptr.i = arr[pos];
      pool.getPtr(ptr);
      ptr.p->do_stuff();
    }
    CALLGRIND_TOGGLE_COLLECT();
    Uint64 stop = NdbTick_CurrentMillisecond();    
    
    for(Uint32 j = 0; j<cnt; j++)
    {
      ptr.i = arr[j];
      pool.release(ptr.i);
      arr[j] = RNIL;
    }
    
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
  char m_cdata[sz-12];
  void do_stuff() { m_data += m_cdata[0] + m_cdata[sz-13]; }
};
typedef Rec<32> Rec32;

void test_ap(Uint32 cnt, Uint32 loop)
{
  printf("AP ; %d ; ws ; %d ; page ; n/a", sizeof(Rec32), (cnt * sizeof(Rec32))>>10);
  ArrayPool<Rec32> pool;
  init(pool, cnt);
  test_pool<Rec32, ArrayPool<Rec32> >(pool, cnt, loop);
}

void test_rw(Uint32 cnt, Uint32 loop)
{
  printf("RW ; %d ; ws ; %d ; page ; n/a", sizeof(Rec32), (cnt * sizeof(Rec32))>>10);
  RecordPool<Rec32, RWPool> pool;
  init(pool, cnt);
  test_pool<Rec32, RecordPool<Rec32, RWPool> >(pool, cnt, loop);
}

void test_wo(Uint32 cnt, Uint32 loop)
{
  printf("WO ; %d ; ws ; %d ; page ; n/a", sizeof(Rec32), (cnt * sizeof(Rec32))>>10);
  RecordPool<Rec32, WOPool> pool;
  init(pool, cnt);
  test_pool<Rec32, RecordPool<Rec32, WOPool> >(pool, cnt, loop);
}

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

unsigned 
my_rand(unsigned* seed)
{
  unsigned val = *seed;
  val += 117711;
  val *= 133131;
  return * seed = val;
}

int
main(int argc, char **argv)
{
  g_eventLogger.createConsoleHandler();
  g_eventLogger.setCategory("keso");
  g_eventLogger.enable(Logger::LL_ON, Logger::LL_INFO);
  g_eventLogger.enable(Logger::LL_ON, Logger::LL_CRITICAL);
  g_eventLogger.enable(Logger::LL_ON, Logger::LL_ERROR);
  g_eventLogger.enable(Logger::LL_ON, Logger::LL_WARNING);

  Uint32 loops = 300000;
  for (Uint32 i = 1 ; i<argc ; i++)
  {
    if (argc > i+1 && strcmp(argv[i], "-tests") == 0)
    {
      tests = 0;
      for (Uint32 j = 0; j<strlen(argv[i+1]); j++)
      {
	char c = argv[i+1][j];
	if (c >= '0' && c <= '9')
	  tests |= 1 << (c - '0');
	else 
	  tests |= 1 << (c - 'a');
      }
      ndbout_c("tests: %x", tests);
    }
    else if (argc > i+1 && strcmp(argv[i], "-sizes") == 0)
    {
      sizes = 0;
      for (Uint32 j = 0; j<strlen(argv[i+1]); j++)
      {
	char c = argv[i+1][j];
	sizes |= 1 << (c - '0');
      }
      ndbout_c("sizes: %x", sizes);
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
  mm.dump();

  seed = time(0);
  Uint32 sz = 0;
  Uint32 cnt = 768;

  while(cnt <= 1000000)
  {
    Uint32 loop = 768 * loops / cnt;
    if (sizes & (1 << sz))
    {
      if (tests & T_TEST_AP)
	test_ap(cnt, loop);
      if (tests & T_TEST_WO)
	test_wo(cnt, loop);
      if (tests & T_TEST_RW)
	test_rw(cnt, loop);
    }

    cnt += (1024 << sz);
    sz++;
  }
}

Uint32 g_currentStartPhase;

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

template void test_pool<Rec<(unsigned)32>, ArrayPool<Rec<(unsigned)32> > >(ArrayPool<Rec<(unsigned)32> >&, unsigned, unsigned);
template void test_pool<Rec<(unsigned)32>, RecordPool<Rec<(unsigned)32>, RWPool> >(RecordPool<Rec<(unsigned)32>, RWPool>&, unsigned, unsigned);
template void test_pool<Rec<(unsigned)32>, RecordPool<Rec<(unsigned)32>, WOPool> >(RecordPool<Rec<(unsigned)32>, WOPool>&, unsigned, unsigned);

template void init<Rec<(unsigned)32> >(ArrayPool<Rec<(unsigned)32> >&, unsigned);
template void init<Rec<(unsigned)32> >(RecordPool<Rec<(unsigned)32>, RWPool>&, unsigned);
template void init<Rec<(unsigned)32> >(RecordPool<Rec<(unsigned)32>, WOPool>&, unsigned);
