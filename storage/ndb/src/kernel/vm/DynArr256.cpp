/*
   Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "DynArr256.hpp"
#include <stdio.h>
#include <assert.h>
#include <NdbOut.hpp>

#define DA256_BITS  5
#define DA256_MASK 31

struct DA256CL
{
  Uint32 m_magic;
  Uint32 m_data[15];
};

struct DA256Free
{
  Uint32 m_magic;
  Uint32 m_next_free;
};

struct DA256Node
{
  struct DA256CL m_lines[17];
};

struct DA256Page
{
  struct DA256CL m_header[2];
  struct DA256Node m_nodes[30];
};

#undef require
#define require(x) require_exit_or_core_with_printer((x), 0, ndbout_printer)
//#define DA256_USE_PX
//#define DA256_USE_PREFETCH
#define DA256_EXTRA_SAFE


#ifdef UNIT_TEST
#ifdef USE_CALLGRIND
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_TOGGLE_COLLECT()
#endif
Uint32 allocatedpages = 0;
Uint32 allocatednodes = 0;
Uint32 releasednodes = 0;
#endif

inline
void
require_impl(bool x, int line)
{
  if (!x)
  {
    ndbout_c("LINE: %d", line);
    abort();
  }
}

DynArr256Pool::DynArr256Pool()
{
  m_type_id = RNIL;
  m_first_free = RNIL;
  m_memroot = 0;
}

void
DynArr256Pool::init(Uint32 type_id, const Pool_context & pc)
{
  init(0, type_id, pc);
}

void
DynArr256Pool::init(NdbMutex* m, Uint32 type_id, const Pool_context & pc)
{
  m_ctx = pc;
  m_type_id = type_id;
  m_memroot = (DA256Page*)m_ctx.get_memroot();
  m_mutex = m;
}

static const Uint32 g_max_sizes[5] = { 0, 256, 65536, 16777216, ~0 };

/**
 * sz = 0     =     1 - 0 level
 * sz = 1     = 256^1 - 1 level
 * sz = 2     = 256^2 - 2 level
 * sz = 3     = 256^3 - 3 level
 * sz = 4     = 256^4 - 4 level
 */
Uint32 *
DynArr256::get(Uint32 pos) const
{
  Uint32 sz = m_head.m_sz;
  Uint32 ptrI = m_head.m_ptr_i;
  DA256Page * memroot = m_pool.m_memroot;
  Uint32 type_id = (~m_pool.m_type_id) & 0xFFFF;
  
  if (unlikely(pos >= g_max_sizes[sz]))
  {
    return 0;
  }
  
#ifdef DA256_USE_PX
  Uint32 px[4] = { (pos >> 24) & 255, 
		   (pos >> 16) & 255, 
		   (pos >> 8)  & 255,
		   (pos >> 0)  & 255 };
#endif

  Uint32* retVal = &m_head.m_ptr_i;
  for(; sz --;)
  {
    if (unlikely(ptrI == RNIL))
    {
      return 0;
    }
#ifdef DA256_USE_PX
    Uint32 p0 = px[sz];
#else
    Uint32 shr = sz << 3;
    Uint32 p0 = (pos >> shr) & 255;
#endif
    Uint32 page_no = ptrI >> DA256_BITS;
    Uint32 page_idx = ptrI & DA256_MASK;
    DA256Page * page = memroot + page_no;
    
    Uint32 *magic_ptr, p;
    if (p0 != 255)
    {
      Uint32 line = ((p0 << 8) + (p0 << 4) + p0 + 255) >> 12;
      Uint32 * ptr = (Uint32*)(page->m_nodes + page_idx);
      
      p = 0;
      retVal = (ptr + 1 + p0 + line);
      magic_ptr =(ptr + (p0 & ~15));
    }
    else
    {
      Uint32 b = (page_idx + 1) >> 4;
      Uint32 * ptr = (Uint32*)(page->m_header+b);
      
      p = page_idx - (b << 4) + b;
      retVal = (ptr + 1 + p);
      magic_ptr = ptr;
    }
    
    ptrI = *retVal;
    Uint32 magic = *magic_ptr;
    
    if (unlikely(! ((magic & (1 << p)) && (magic >> 16) == type_id)))
      goto err;
  }
  
  return retVal;
err:
  require(false);
  return 0;
}

Uint32 *
DynArr256::set(Uint32 pos)
{
  Uint32 sz = m_head.m_sz;
  Uint32 type_id = (~m_pool.m_type_id) & 0xFFFF;  
  DA256Page * memroot = m_pool.m_memroot;
  
  if (unlikely(pos >= g_max_sizes[sz]))
  {
    if (unlikely(!expand(pos)))
    {
      return 0;
    }
    sz = m_head.m_sz;
  }
  
#ifdef DA256_USE_PX  
  Uint32 px[4] = { (pos >> 24) & 255, 
		   (pos >> 16) & 255, 
		   (pos >> 8)  & 255,
		   (pos >> 0)  & 255 };
#endif

  Uint32 ptrI = m_head.m_ptr_i;
  Uint32 *retVal = &m_head.m_ptr_i;
  for(; sz --;)
  {
#ifdef DA256_USE_PX
    Uint32 p0 = px[sz];
#else
    Uint32 shr = sz << 3;
    Uint32 p0 = (pos >> shr) & 255;
#endif
    if (ptrI == RNIL)
    {
      if (unlikely((ptrI = m_pool.seize()) == RNIL))
      {
	return 0;
      }
      * retVal = ptrI;
    }
    
    Uint32 page_no = ptrI >> DA256_BITS;
    Uint32 page_idx = ptrI & DA256_MASK;
    DA256Page * page = memroot + page_no;
    
    Uint32 *magic_ptr, p;
    if (p0 != 255)
    {
      Uint32 line = ((p0 << 8) + (p0 << 4) + p0 + 255) >> 12;
      Uint32 * ptr = (Uint32*)(page->m_nodes + page_idx);

      p = 0;
      magic_ptr = (ptr + (p0 & ~15));
      retVal = (ptr + 1 + p0 + line);
    }
    else
    {
      Uint32 b = (page_idx + 1) >> 4;
      Uint32 * ptr = (Uint32*)(page->m_header+b);
      
      p = page_idx - (b << 4) + b;
      magic_ptr = ptr;
      retVal = (ptr + 1 + p);
    }
     
    ptrI = * retVal;
    Uint32 magic = *magic_ptr;

    if (unlikely(! ((magic & (1 << p)) && (magic >> 16) == type_id)))
      goto err;
  } 
  
  return retVal;
  
err:
  require(false);
  return 0;
}

static
inline
void
initpage(DA256Page* p, Uint32 page_no, Uint32 type_id)
{
  Uint32 i, j;
#ifdef DA256_USE_PREFETCH
#if defined(__GNUC__) && !(__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#ifdef DA256_EXTRA_SAFE
  for (i = 0; i<(30 * 17 + 2); i++)
  {
    __builtin_prefetch (p->m_header + i, 1);    
  }
#else
  {
    __builtin_prefetch (p->m_header + 0, 1);    
    __builtin_prefetch (p->m_header + 1, 1);    
    for (i = 0; i<30; i++)
    {
      __builtin_prefetch (p->m_nodes + i, 1);
    }
  }
#endif
#endif
#endif
  DA256CL* cl;
  for  (i = 0; i<2; i++)
  {
    cl = p->m_header + i;
    cl->m_magic = (~type_id << 16);
  }
  
  DA256Free* free;
  
  for (i = 0; i<30; i++)
  {
    free = (DA256Free*)(p->m_nodes+i);
    free->m_magic = type_id;
    free->m_next_free = (page_no << DA256_BITS) + (i + 1);
#ifdef DA256_EXTRA_SAFE
    DA256Node* node = p->m_nodes+i;
    for (j = 0; j<17; j++)
      node->m_lines[j].m_magic = type_id;
#endif
  }
  
  free = (DA256Free*)(p->m_nodes+29);
  free->m_next_free = RNIL;
}

bool
DynArr256::expand(Uint32 pos)
{
  Uint32 i;
  Uint32 idx = 0;
  Uint32 alloc[5];
  Uint32 sz = m_head.m_sz;

  for (; pos >= g_max_sizes[sz]; sz++);

  if (m_head.m_sz == 0)
  {
    m_head.m_sz = sz;
    return true;
  }

  sz =  m_head.m_sz;
  for (; pos >= g_max_sizes[sz]; sz++)
  {
    Uint32 ptrI = m_pool.seize();
    if (unlikely(ptrI == RNIL))
      goto err;
    alloc[idx++] = ptrI;
  }
  
  alloc[idx] = m_head.m_ptr_i;
  m_head.m_sz = 1;
  for (i = 0; i<idx; i++)
  {
    m_head.m_ptr_i = alloc[i];
    Uint32 * ptr = get(0);
    * ptr = alloc[i + 1];
  }

  m_head.m_sz = sz;
  m_head.m_ptr_i = alloc[0];
  
  return true;
  
err:
  for (i = 0; i<idx; i++)
    m_pool.release(alloc[i]);
  return false;
}

void
DynArr256::init(ReleaseIterator &iter)
{
  iter.m_sz = 1;
  iter.m_pos = 0;
  iter.m_ptr_i[0] = RNIL;
  iter.m_ptr_i[1] = m_head.m_ptr_i;
  iter.m_ptr_i[2] = RNIL;
  iter.m_ptr_i[3] = RNIL;
  iter.m_ptr_i[4] = RNIL;
}

/**
 * Iter is in next pos
 *
 * 0 - done
 * 1 - data
 * 2 - no data
 */
Uint32
DynArr256::release(ReleaseIterator &iter, Uint32 * retptr)
{
  Uint32 sz = iter.m_sz;
  Uint32 ptrI = iter.m_ptr_i[sz];
  Uint32 page_no = ptrI >> DA256_BITS;
  Uint32 page_idx = ptrI & DA256_MASK;
  Uint32 type_id = (~m_pool.m_type_id) & 0xFFFF;
  DA256Page * memroot = m_pool.m_memroot;
  DA256Page * page = memroot + page_no;

  if (ptrI != RNIL)
  {
    Uint32 p0 = iter.m_pos & 255;
    for (; p0<256; p0++)
    {
      Uint32 *retVal, *magic_ptr, p;
      if (p0 != 255)
      {
	Uint32 line = ((p0 << 8) + (p0 << 4) + p0 + 255) >> 12;
	Uint32 * ptr = (Uint32*)(page->m_nodes + page_idx);
	
	p = 0;
	retVal = (ptr + 1 + p0 + line);
	magic_ptr =(ptr + (p0 & ~15));
      }
      else
      {
	Uint32 b = (page_idx + 1) >> 4;
	Uint32 * ptr = (Uint32*)(page->m_header+b);
	
	p = page_idx - (b << 4) + b;
	retVal = (ptr + 1 + p);
	magic_ptr = ptr;
      }
      
      Uint32 magic = *magic_ptr;
      Uint32 val = *retVal;
      if (unlikely(! ((magic & (1 << p)) && (magic >> 16) == type_id)))
	goto err;
      
      if (sz == m_head.m_sz)
      {
	* retptr = val;
	p0++;
	if (p0 != 256)
	{
	  /**
	   * Move next
	   */
	  iter.m_pos &= ~(Uint32)255;
	  iter.m_pos |= p0;
	}
	else
	{
	  /**
	   * Move up
	   */
	  m_pool.release(ptrI);
	  iter.m_sz --;
	  iter.m_pos >>= 8;
	}
	return 1;
      }
      else if (val != RNIL)
      {
	iter.m_sz++;
	iter.m_ptr_i[iter.m_sz] = val;
	iter.m_pos = (p0 << 8);
	* retVal = RNIL;
	return 2;
      }
    }
    
    assert(p0 == 256);
    m_pool.release(ptrI);
    iter.m_sz --;
    iter.m_pos >>= 8;
    return 2;
  }
  
  new (&m_head) Head();
  return 0;
  
err:
  require(false);
  return false;
}

static
inline
bool
seizenode(DA256Page* page, Uint32 idx, Uint32 type_id)
{
  Uint32 i;
  Uint32 b = (idx + 1) >> 4;
  Uint32 p = idx - (b << 4) + b;
  
  DA256Node * ptr = (DA256Node*)(page->m_nodes + idx);  

#ifdef DA256_USE_PREFETCH
#if defined(__GNUC__) && !(__GNUC__ == 2 && __GNUC_MINOR__ < 96)
  __builtin_prefetch (page->m_header + b, 1); 
  for (i = 0; i<17; i++)
  {
    __builtin_prefetch (ptr->m_lines+i, 1);    
  }
#endif
#endif

#ifdef DA256_EXTRA_SAFE
  Uint32 check = type_id;
#endif
  type_id = ((~type_id) << 16) | 0xFFFF;
  
#ifdef DA256_EXTRA_SAFE
  if (unlikely(((page->m_header + b)->m_magic & (1 << p)) != 0))
  {
    return false;
  }
#endif

  (page->m_header + b)->m_magic |= (1 << p);
  (page->m_header + b)->m_data[p] = RNIL;
  for (i = 0; i<17; i++)
  {
    DA256CL * line = ptr->m_lines + i;
#ifdef DA256_EXTRA_SAFE
    if (unlikely(line->m_magic != check))
    {
      return false;
    }
#endif
    line->m_magic = type_id;
    for (Uint32 j = 0; j<15; j++)
      line->m_data[j] = RNIL;
  }

#ifdef UNIT_TEST
  allocatednodes++;
#endif
  return true;
}

static
bool
releasenode(DA256Page* page, Uint32 idx, Uint32 type_id)
{
  Uint32 i;
  Uint32 b = (idx + 1) >> 4;
  Uint32 p = idx - (b << 4) + b;
  
  DA256Node * ptr = (DA256Node*)(page->m_nodes + idx);  

#ifdef DA256_USE_PREFETCH
#if defined(__GNUC__) && !(__GNUC__ == 2 && __GNUC_MINOR__ < 96)
  __builtin_prefetch (page->m_header + b, 1); 
  for (i = 0; i<17; i++)
  {
    __builtin_prefetch (ptr->m_lines+i, 1);    
  }
#endif
#endif

#ifdef DA256_EXTRA_SAFE
  Uint32 check = ((~type_id) << 16) | 0xFFFF;
#endif

#ifdef DA256_EXTRA_SAFE
  if (unlikely((((page->m_header + b)->m_magic & (1 << p)) == 0)))
  {
    return false;
  }
#endif

  (page->m_header + b)->m_magic ^= (1 << p);
  for (i = 0; i<17; i++)
  {
    DA256CL * line = ptr->m_lines + i;
#ifdef DA256_EXTRA_SAFE
    if (unlikely(line->m_magic != check))
    {
      return false;
    }
#endif
    line->m_magic = type_id;
  }

#ifdef UNIT_TEST
  releasednodes++;
#endif

  return true;
}

Uint32
DynArr256Pool::seize()
{
  Uint32 type_id = m_type_id;
  DA256Page* page;
  DA256Page * memroot = m_memroot;

  Guard2 g(m_mutex);
  Uint32 ff = m_first_free;
  if (ff == RNIL)
  { 
    Uint32 page_no;
    if (likely((page = (DA256Page*)m_ctx.alloc_page(type_id, &page_no)) != 0))
    {
      initpage(page, page_no, type_id);
#ifdef UNIT_TEST
      allocatedpages++;
#endif
    }
    else
    {
      return RNIL;
    }
    ff = (page_no << DA256_BITS);
  }
  else
  {
    page = memroot + (ff >> DA256_BITS);
  }
  
  Uint32 idx = ff & DA256_MASK;
  DA256Free * ptr = (DA256Free*)(page->m_nodes + idx);
  if (likely(ptr->m_magic == type_id))
  {
    Uint32 next = ptr->m_next_free;
    if (likely(seizenode(page, idx, type_id)))
    {
      m_first_free = next;    
      return ff;
    }
  }
  
//error:
  require(false);
  return 0;
}

void
DynArr256Pool::release(Uint32 ptrI)
{
  Uint32 type_id = m_type_id;

  Uint32 page_no = ptrI >> DA256_BITS;
  Uint32 page_idx = ptrI & DA256_MASK;
  DA256Page * memroot = m_memroot;
  DA256Page * page = memroot + page_no;
  
  DA256Free * ptr = (DA256Free*)(page->m_nodes + page_idx);
  if (likely(releasenode(page, page_idx, type_id)))
  {
    ptr->m_magic = type_id;
    Guard2 g(m_mutex);
    Uint32 ff = m_first_free;
    ptr->m_next_free = ff;
    m_first_free = ptrI;
    return;
  }
  require(false);
}

#ifdef UNIT_TEST

#include <NdbTick.h>
#include "ndbd_malloc_impl.hpp"
#include "SimulatedBlock.hpp"

Ndbd_mem_manager mm;
Configuration cfg;
Block_context ctx(cfg, mm);
struct BB : public SimulatedBlock
{
  BB(int no, Block_context& ctx) : SimulatedBlock(no, ctx) {}
};

BB block(DBACC, ctx);

static
void
simple(DynArr256 & arr, int argc, char* argv[])
{
  ndbout_c("argc: %d", argc);
  for (Uint32 i = 1; i<(Uint32)argc; i++)
  {
    Uint32 * s = arr.set(atoi(argv[i]));
    {
      bool found = false;
      for (Uint32 j = 1; j<i; j++)
      {
	if (atoi(argv[i]) == atoi(argv[j]))
	{
	  found = true;
	  break;
	}
      }  
      if (!found)
	* s = i;
    }
    
    Uint32 * g = arr.get(atoi(argv[i]));
    Uint32 v = g ? *g : ~0;
    ndbout_c("p: %p %p %d", s, g, v);
  }
}

static
void
basic(DynArr256& arr, int argc, char* argv[])
{
#define MAXLEN 65536
  
  Uint32 len = 0;
  Uint32 save[2*MAXLEN];
  for (Uint32 i = 0; i<MAXLEN; i++)
  {
    int op = (rand() % 100) > 50;
    if (len == 0)
      op = 1;
    if (len == MAXLEN)
      op = 0;
    switch(op){
    case 0:{ // get
      Uint32 item = (rand() % len) << 1;
      Uint32 idx = save[item];
      Uint32 val = save[item+1];
      //ndbout_c("get(%d)", idx);
      Uint32 *p = arr.get(idx);
      assert(p);
      assert(* p == val);
      break;
    }
    case 1:{ // set
      Uint32 item = len << 1;
      Uint32 idx = i; //rand() & 0xFFFFF; // & 0xFFFFF; //rand(); //(65536*i) / 10000;
      Uint32 val = rand();
#if 0
      for(Uint32 j = 0; j < item; j += 2)
      {
	if (save[j] == idx)
	{
	  item = j;
	  break;
	}
      }
#endif
      //ndbout_c("set(%d, %x)", idx, val);
      Uint32 *p = arr.set(idx);
      assert(* p);
      if (item == (len << 1))
      {
	*p = val;
	len++;
      }
      else
      {
	assert(* p == save[item+1]);
	* p = val;
      }
      save[item] = idx;
      save[item+1] = val;
    }
    }
  }
}

unsigned long long 
micro()
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  unsigned long long ret = tv.tv_sec;
  ret *= 1000000;
  ret += tv.tv_usec;
  return ret;
}

static
void
read(DynArr256& arr, int argc, char ** argv)
{
  Uint32 cnt = 100000;
  Uint64 mbytes = 16*1024;
  Uint32 seed = time(0);
  Uint32 seq = 0, seqmask = 0;

  for (Uint32 i = 2; i<argc; i++)
  {
    if (strncmp(argv[i], "--mbytes=", sizeof("--mbytes=")-1) == 0)
    {
      mbytes = atoi(argv[i]+sizeof("--mbytes=")-1);
      if (argv[i][strlen(argv[i])-1] == 'g' ||
	  argv[i][strlen(argv[i])-1] == 'G')
	mbytes *= 1024;
    }
    else if (strncmp(argv[i], "--cnt=", sizeof("--cnt=")-1) == 0)
    {
      cnt = atoi(argv[i]+sizeof("--cnt=")-1);
    }
    else if (strncmp(argv[i], "--seq", sizeof("--seq")-1) == 0)
    {
      seq = 1;
    }
  }
  
  /**
   * Populate with 5Mb
   */
  Uint32 maxidx = (1024*mbytes+31) / 32;
  Uint32 nodes = (maxidx+255) / 256;
  Uint32 pages = (nodes + 29)/ 30;
  ndbout_c("%lldmb data -> %d entries (%dkb)",
	   mbytes, maxidx, 32*pages);
  
  for (Uint32 i = 0; i<maxidx; i++)
  {
    Uint32 *ptr = arr.set(i);
    assert(ptr);
    * ptr = i;
  }

  srand(seed);

  if (seq)
  {
    seq = rand();
    seqmask = ~(Uint32)0;
  }

  ndbout_c("Timing %d %s reads (seed: %u)", cnt, 
	   seq ? "sequential" : "random", seed);

  for (Uint32 i = 0; i<10; i++)
  {
    Uint32 sum0 = 0, sum1 = 0;
    Uint64 start = micro();
    for (Uint32 i = 0; i<cnt; i++)
    {
      Uint32 idx = ((rand() & (~seqmask)) + ((i + seq) & seqmask)) % maxidx;
      Uint32 *ptr = arr.get(idx);
      sum0 += idx;
      sum1 += *ptr;
    }
    start = micro() - start;
    float uspg = start; uspg /= cnt;
    ndbout_c("Elapsed %lldus diff: %d -> %f us/get", start, sum0 - sum1, uspg);
  }
}

static
void
write(DynArr256& arr, int argc, char ** argv)
{
  Uint32 seq = 0, seqmask = 0;
  Uint32 cnt = 100000;
  Uint64 mbytes = 16*1024;
  Uint32 seed = time(0);

  for (Uint32 i = 2; i<argc; i++)
  {
    if (strncmp(argv[i], "--mbytes=", sizeof("--mbytes=")-1) == 0)
    {
      mbytes = atoi(argv[i]+sizeof("--mbytes=")-1);
      if (argv[i][strlen(argv[i])-1] == 'g' ||
	  argv[i][strlen(argv[i])-1] == 'G')
	mbytes *= 1024;
    }
    else if (strncmp(argv[i], "--cnt=", sizeof("--cnt=")-1) == 0)
    {
      cnt = atoi(argv[i]+sizeof("--cnt=")-1);
    }
    else if (strncmp(argv[i], "--seq", sizeof("--seq")-1) == 0)
    {
      seq = 1;
    }
  }
  
  /**
   * Populate with 5Mb
   */
  Uint32 maxidx = (1024*mbytes+31) / 32;
  Uint32 nodes = (maxidx+255) / 256;
  Uint32 pages = (nodes + 29)/ 30;
  ndbout_c("%lldmb data -> %d entries (%dkb)",
	   mbytes, maxidx, 32*pages);

  srand(seed);

  if (seq)
  {
    seq = rand();
    seqmask = ~(Uint32)0;
  }

  ndbout_c("Timing %d %s writes (seed: %u)", cnt, 
	   seq ? "sequential" : "random", seed);
  for (Uint32 i = 0; i<10; i++)
  {
    Uint64 start = micro();
    for (Uint32 i = 0; i<cnt; i++)
    {
      Uint32 idx = ((rand() & (~seqmask)) + ((i + seq) & seqmask)) % maxidx;
      Uint32 *ptr = arr.set(idx);
      *ptr = i;
    }
    start = micro() - start;
    float uspg = start; uspg /= cnt;
    ndbout_c("Elapsed %lldus -> %f us/set", start, uspg);
    DynArr256::ReleaseIterator iter;
    arr.init(iter);
    Uint32 val;
    while(arr.release(iter, &val));
  }
}

int
main(int argc, char** argv)
{
  if (0)
  {
    for (Uint32 i = 0; i<30; i++)
    {
      Uint32 b = (i + 1) >> 4;
      Uint32 p = i - (b << 4) + b;
      printf("[ %d %d %d ]\n", i, b, p);
    }
    return 0;
  }

  Pool_context pc;
  pc.m_block = &block;
  
  Resource_limit rl;
  rl.m_min = 0;
  rl.m_max = 10000;
  rl.m_resource_id = 0;
  mm.set_resource_limit(rl);
  if(!mm.init())
  {
    abort();
  }

  DynArr256Pool pool;
  pool.init(0x2001, pc);

  DynArr256::Head head;
  DynArr256 arr(pool, head);

  if (strcmp(argv[1], "--simple") == 0)
    simple(arr, argc, argv);
  else if (strcmp(argv[1], "--basic") == 0)
    basic(arr, argc, argv);
  else if (strcmp(argv[1], "--read") == 0)
    read(arr, argc, argv);
  else if (strcmp(argv[1], "--write") == 0)
    write(arr, argc, argv);

  DynArr256::ReleaseIterator iter;
  arr.init(iter);
  Uint32 cnt = 0, val;
  while (arr.release(iter, &val)) cnt++;
  
  ndbout_c("allocatedpages: %d allocatednodes: %d releasednodes: %d"
	   " releasecnt: %d",
	   allocatedpages, 
	   allocatednodes,
	   releasednodes,
	   cnt);
  
  return 0;
#if 0
  printf("sizeof(DA256Page): %d\n", sizeof(DA256Page));

  DA256Page page;

  for (Uint32 i = 0; i<10000; i++)
  {
    Uint32 arg = rand() & 255;
    Uint32 base = 0;
    Uint32 idx = arg & 256;
    printf("%d\n", arg);

    assert(base <= 30);
    
    if (idx == 255)
    {
      Uint32 b = (base + 1) >> 4;
      Uint32 p = base - (b << 4) + b;
      Uint32 magic = page.m_header[b].m_magic;
      Uint32 retVal = page.m_header[b].m_data[p];
      
      require(magic & (1 << p));
      return retVal;
    }
    else
    {
      // 4 bit extra offset per idx
      Uint32 line = idx / 15;
      Uint32 off = idx % 15;
      
      {
	Uint32 pos = 1 + idx + line;
	Uint32 magic = pos & ~15;
	
	Uint32 * ptr = (Uint32*)&page.m_nodes[base];
	assert((ptr + pos) == &page.m_nodes[base].m_lines[line].m_data[off]);
	assert((ptr + magic) == &page.m_nodes[base].m_lines[line].m_magic);
      }
    }
  }
#endif
}

Uint32 g_currentStartPhase;
Uint32 g_start_type;
NdbNodeBitmask g_nowait_nodes;

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

#endif
