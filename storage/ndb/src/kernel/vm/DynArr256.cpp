/*
   Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "DynArr256.hpp"
#include "pc.hpp"
#include <stdio.h>
#include <assert.h>
#include <NdbOut.hpp>
#include <my_systime.h>  // my_micro_time

/**
 * Trick to be able to use ERROR_INSERTED macro inside DynArr256 and
 * DynArr256Pool by directing to member function implemented inline in
 * DynArr256.hpp where cerrorInsert is not hidden by below macro definition.
 */
#ifdef ERROR_INSERT
#define cerrorInsert get_ERROR_INSERT_VALUE()
#endif

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
  Uint32 m_prev_free;
};

struct DA256Node
{
  struct DA256CL m_lines[17];
};

struct DA256Page
{
  struct DA256CL m_header[2];
  struct DA256Node m_nodes[30];

  bool get(Uint32 node, Uint32 idx, Uint32 type_id, Uint32*& val_ptr) const;
  bool is_empty() const;
  bool is_full() const;
  Uint32 first_free() const;
  Uint32 last_free() const;
};

inline
bool DA256Page::is_empty() const
{
  return !(0x7fff & (m_header[0].m_magic | m_header[1].m_magic));
}

inline
bool DA256Page::is_full() const
{
  return !(0x7fff & ~(m_header[0].m_magic & m_header[1].m_magic));
}

inline
Uint32 DA256Page::first_free() const
{
  Uint32 node = BitmaskImpl::ctz(~m_header[0].m_magic | 0x8000);
  if (node == 15)
    node = 15 + BitmaskImpl::ctz(~m_header[1].m_magic | 0x8000);
  return node;
}

inline
Uint32 DA256Page::last_free() const
{
  Uint32 node = 29 - BitmaskImpl::clz((~m_header[1].m_magic << 17) | 0x10000);
  if (node == 14)
    node = 14 - BitmaskImpl::clz((~m_header[0].m_magic << 17) | 0x10000);
  return node;
}


#undef require
#define require(x) require_exit_or_core_with_printer((x), 0, ndbout_printer)
//#define DA256_USE_PX
//#define DA256_USE_PREFETCH
#define DA256_EXTRA_SAFE

#ifdef TEST_DYNARR256
#define UNIT_TEST
#include "NdbTap.hpp"
#endif

#ifdef UNIT_TEST
#include "my_sys.h"
#ifdef USE_CALLGRIND
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_TOGGLE_COLLECT()
#endif
Uint32 verbose = 0;
Uint32 allocatedpages = 0;
Uint32 releasedpages = 0;
Uint32 maxallocatedpages = 0;
Uint32 allocatednodes = 0;
Uint32 releasednodes = 0;
Uint32 maxallocatednodes = 0;
#endif

static
inline
Uint32 div15(Uint32 x)
{
  return ((x << 8) + (x << 4) + x + 255) >> 12;
}

DynArr256Pool::DynArr256Pool()
{
  m_type_id = RNIL;
  m_first_free = RNIL;
  m_last_free = RNIL;
  m_memroot = 0;
  m_inuse_nodes = 0;
  m_pg_count = 0;
  m_used = 0;
  m_usedHi = 0;
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

inline
bool
DA256Page::get(Uint32 node, Uint32 idx, Uint32 type_id, Uint32*& val_ptr) const
{
  Uint32 *magic_ptr, p;
  if (idx != 255)
  {
    Uint32 line = div15(idx);
    Uint32* ptr = (Uint32*)(m_nodes + node);

    p = 0;
    val_ptr = (ptr + 1 + idx + line);
    magic_ptr =(ptr + (idx & ~15));
  }
  else
  {
    Uint32 b = (node + 1) >> 4;
    Uint32 * ptr = (Uint32*)(m_header+b);

    p = node - (b << 4) + b;
    val_ptr = (ptr + 1 + p);
    magic_ptr = ptr;
  }

  Uint32 magic = *magic_ptr;

  return ((magic & (1 << p)) && (magic >> 16) == type_id);
}

Uint32 DynArr256::Head::getByteSize() const
{
  assert(m_no_of_nodes >= 0);
  return static_cast<Uint32>(m_no_of_nodes) * sizeof(DA256Node);
}

static const Uint32 g_max_sizes[5] = { 0, 256, 65536, 16777216, 4294967295U };

/**
 * sz = 0     =     1 - 0 level
 * sz = 1     = 256^1 - 1 level
 * sz = 2     = 256^2 - 2 level
 * sz = 3     = 256^3 - 3 level
 * sz = 4     = 256^4 - 4 level
 */
Uint32 *
DynArr256::get_dirty(Uint32 pos) const
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

    if (unlikely(! page->get(page_idx, p0, type_id, retVal)))
      goto err;

    ptrI = *retVal;
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
      if(ERROR_INSERTED(3005))
      {
        // Demonstrate Bug#25851801 7.6.2(DMR2):: COMPLETE CLUSTER CRASHED DURING UNIQUE KEY CREATION ...
        // Simulate m_pool.seize() failed.
        return 0;
      }
      if (unlikely((ptrI = m_pool.seize()) == RNIL))
      {
	return 0;
      }
      m_head.m_no_of_nodes++;
      * retVal = ptrI;
    }
    
    Uint32 page_no = ptrI >> DA256_BITS;
    Uint32 page_idx = ptrI & DA256_MASK;
    DA256Page * page = memroot + page_no;
    
    if (unlikely(! page->get(page_idx, p0, type_id, retVal)))
      goto err;

    ptrI = * retVal;
  } 
  
#if defined VM_TRACE || defined ERROR_INSERT
  if (pos > m_head.m_high_pos)
    m_head.m_high_pos = pos;
#endif

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
    free->m_next_free = RNIL;
    free->m_prev_free = RNIL;
#ifdef DA256_EXTRA_SAFE
    DA256Node* node = p->m_nodes+i;
    for (j = 0; j<17; j++)
      node->m_lines[j].m_magic = type_id;
#endif
  }
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
    m_head.m_no_of_nodes++;
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

  m_head.m_no_of_nodes -= idx;
  assert(m_head.m_no_of_nodes >= 0);
  return false;
}

void
DynArr256::init(ReleaseIterator &iter)
{
  iter.m_sz = 1;
  iter.m_pos = ~(~0U << (8 * m_head.m_sz));
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
 *
 * if ptrVal is NULL, truncate work in trim mode and will stop
 * (return 0) as soon value is not RNIL
 */
Uint32
DynArr256::truncate(Uint32 trunc_pos, ReleaseIterator& iter, Uint32* ptrVal)
{
  Uint32 type_id = (~m_pool.m_type_id) & 0xFFFF;
  DA256Page * memroot = m_pool.m_memroot;

  for (;;)
  {
    if (iter.m_sz == 0 ||
        iter.m_pos < trunc_pos ||
        m_head.m_sz == 0 ||
        m_head.m_no_of_nodes == 0)
    {
      if (m_head.m_sz == 1 && m_head.m_ptr_i == RNIL)
      {
        assert(m_head.m_no_of_nodes == 0);
        m_head.m_sz = 0;
      }
      return 0;
    }

    Uint32* refPtr;
    Uint32 ptrI = iter.m_ptr_i[iter.m_sz];
    assert(ptrI != RNIL);
    Uint32 page_no = ptrI >> DA256_BITS;
    Uint32 page_idx = (ptrI & DA256_MASK) ;
    DA256Page* page = memroot + page_no;
    Uint32 node_addr = (iter.m_pos >> (8 * (m_head.m_sz - iter.m_sz)));
    Uint32 node_index = node_addr & 255;
    bool is_value = (iter.m_sz == m_head.m_sz);

    if (unlikely(! page->get(page_idx, node_index, type_id, refPtr)))
    {
      require(false);
    }
    assert(refPtr != NULL);
    if (ptrVal != NULL)
    {
      *ptrVal = *refPtr;
    }
    else if (is_value && *refPtr != RNIL)
    {
      return 0;
    }

    if (iter.m_sz == 1 &&
        node_addr == 0)
    {
      assert(iter.m_ptr_i[iter.m_sz] == m_head.m_ptr_i);
      assert(iter.m_ptr_i[iter.m_sz + 1] == RNIL);
      iter.m_ptr_i[iter.m_sz] = is_value ? RNIL : *refPtr;
      m_pool.release(m_head.m_ptr_i);
      m_head.m_sz --;
      m_head.m_no_of_nodes--;
      assert(m_head.m_no_of_nodes >= 0);
      m_head.m_ptr_i = iter.m_ptr_i[iter.m_sz];
      if (is_value)
        return 1;
    }
    else if (is_value || iter.m_ptr_i[iter.m_sz + 1] == *refPtr)
    { // sz--
      Uint32 ptrI = *refPtr;
      if (!is_value)
      {
        if (ptrI != RNIL)
        {
          m_pool.release(ptrI);
          m_head.m_no_of_nodes--;
          assert(m_head.m_no_of_nodes >= 0);
          *refPtr = iter.m_ptr_i[iter.m_sz+1] = RNIL;
        }
      }
      if (node_index == 0)
      {
        iter.m_sz --;
      }
      else if (!is_value && ptrI == RNIL)
      {
        assert((~iter.m_pos & ~(0xffffffff << (8 * (m_head.m_sz - iter.m_sz)))) == 0);
        iter.m_pos -= 1U << (8 * (m_head.m_sz - iter.m_sz));
      }
      else
      {
        assert((iter.m_pos & ~(0xffffffff << (8 * (m_head.m_sz - iter.m_sz)))) == 0);
        iter.m_pos --;
      }
#if defined VM_TRACE || defined ERROR_INSERT
      if (iter.m_pos < m_head.m_high_pos)
        m_head.m_high_pos = iter.m_pos;
#endif
      if (is_value && ptrVal != NULL)
        return 1;
    }
    else
    { // sz++
      assert(iter.m_ptr_i[iter.m_sz + 1] == RNIL);
      iter.m_sz ++;
      iter.m_ptr_i[iter.m_sz] = *refPtr;
      return 2;
    }
  }
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
  if (allocatednodes - releasednodes > maxallocatednodes)
    maxallocatednodes = allocatednodes - releasednodes;
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
    if (likely((page = (DA256Page*)m_ctx.alloc_page27(type_id, &page_no)) != 0))
    {
      initpage(page, page_no, type_id);
      m_pg_count++;
#ifdef UNIT_TEST
      allocatedpages++;
      if (allocatedpages - releasedpages > maxallocatedpages)
        maxallocatedpages = allocatedpages - releasedpages;
#endif
    }
    else
    {
      return RNIL;
    }
    m_last_free = m_first_free = ff = page_no;
  }
  else
  {
    page = memroot + ff;
  }
  
  Uint32 idx = page->first_free();
  DA256Free * ptr = (DA256Free*)(page->m_nodes + idx);
  if (likely(ptr->m_magic == type_id))
  {
    Uint32 last_free = page->last_free();
    Uint32 next_page = ((DA256Free*)(page->m_nodes + last_free))->m_next_free;
    if (likely(seizenode(page, idx, type_id)))
    {
      m_inuse_nodes++;
      if (page->is_full())
      {
        assert(m_first_free == ff);
        m_first_free = next_page;
        if (m_first_free == RNIL)
        {
          assert(m_last_free == ff);
          m_last_free = RNIL;
        }
        else
        {
          page = memroot + next_page;
          ((DA256Free*)(page->m_nodes + page->last_free()))->m_prev_free = RNIL;
        }
      }

      m_used++;
      if (m_used < m_usedHi)
        m_usedHi = m_used;

      return (ff << DA256_BITS) | idx;
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

  Guard2 g(m_mutex);
  Uint32 last_free = page->last_free();
  if (likely(releasenode(page, page_idx, type_id)))
  {
    m_inuse_nodes--;
    ptr->m_magic = type_id;
    if (last_free > 29)
    { // Add last to page free list
      Uint32 lf = m_last_free;
      ptr->m_prev_free = lf;
      ptr->m_next_free = RNIL;
      m_last_free = page_no;
      if (m_first_free == RNIL)
        m_first_free = page_no;

      if (lf != RNIL)
      {
        page = memroot + lf;
        DA256Free* pptr = (DA256Free*)(page->m_nodes + page->last_free());
        pptr->m_next_free = page_no;
      }
    }
    else if (page->is_empty())
    {
      // TODO msundell: release_page
      // unlink from free page list
      Uint32 nextpage = ((DA256Free*)(page->m_nodes + last_free))->m_next_free;
      Uint32 prevpage = ((DA256Free*)(page->m_nodes + last_free))->m_prev_free;
      m_ctx.release_page(type_id, page_no);
      m_pg_count--;
#ifdef UNIT_TEST
      releasedpages ++;
#endif
      if (nextpage != RNIL)
      {
        page = memroot + nextpage;
        ((DA256Free*)(page->m_nodes + page->last_free()))->m_prev_free = prevpage;
      }
      if (prevpage != RNIL)
      {
        page = memroot + prevpage;
        ((DA256Free*)(page->m_nodes + page->last_free()))->m_next_free = nextpage;
      }
      if (m_first_free == page_no)
      {
        m_first_free = nextpage;
      }
      if (m_last_free == page_no)
        m_last_free = prevpage;
    }
    else if (page_idx > last_free)
    { // last free node in page tracks free page list links
      ptr->m_next_free = ((DA256Free*)(page->m_nodes + last_free))->m_next_free;
      ptr->m_prev_free = ((DA256Free*)(page->m_nodes + last_free))->m_prev_free;
    }
    assert(m_used);
    m_used--;
    return;
  }
  require(false);
}

const DynArr256Pool::Info
DynArr256Pool::getInfo() const
{
  Info info;
  info.pg_count = m_pg_count;
  info.pg_byte_sz = static_cast<Uint32>(sizeof(DA256Page));
  info.inuse_nodes = m_inuse_nodes;
  info.node_byte_sz = static_cast<Uint32>(sizeof(DA256Node));
  info.nodes_per_page = 30;
  
  return info;
}

#ifdef UNIT_TEST

static
bool
release(DynArr256& arr)
{
  DynArr256::ReleaseIterator iter;
  arr.init(iter);
  Uint32 val;
  Uint32 cnt=0;
  Uint64 start;
  if (verbose > 2)
    ndbout_c("allocatedpages: %d (max %d) releasedpages: %d allocatednodes: %d (max %d) releasednodes: %d",
           allocatedpages, maxallocatedpages,
           releasedpages,
           allocatednodes, maxallocatednodes,
           releasednodes);
  start = my_micro_time();
  while (arr.release(iter, &val))
    cnt++;
  start = my_micro_time() - start;
  if (verbose > 1)
    ndbout_c("allocatedpages: %d (max %d) releasedpages: %d allocatednodes: %d (max %d) releasednodes: %d (%llu us)"
             " releasecnt: %d",
             allocatedpages, maxallocatedpages,
             releasedpages,
             allocatednodes, maxallocatednodes,
             releasednodes,
             start, cnt);
  return true;
}

static
bool
simple(DynArr256 & arr, int argc, char* argv[])
{
  if (verbose) ndbout_c("argc: %d", argc);
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
    if (verbose) ndbout_c("p: %p %p %d", s, g, v);
  }
  return true;
}

static
bool
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
      require(p);
      require(* p == val);
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
      require(p);
      if (item == (len << 1))
      {
	*p = val;
	len++;
      }
      else
      {
	require(* p == save[item+1]);
	* p = val;
      }
      save[item] = idx;
      save[item+1] = val;
    }
    }
  }
  return true;
}

static
bool
read(DynArr256& arr, int argc, char ** argv)
{
  Uint32 cnt = 100000;
  Uint64 mbytes = 16*1024;
  Uint32 seed = (Uint32) time(0);
  Uint32 seq = 0, seqmask = 0;

  for (int i = 1; i < argc; i++)
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

  if (mbytes >= 134217720)
  {
    ndberr.println("--mbytes must be less than 134217720");
    return false;
  }
  Uint32 maxidx = (Uint32)((1024*mbytes+31) / 32);
  Uint32 nodes = (maxidx+255) / 256;
  Uint32 pages = (nodes + 29)/ 30;
  if (verbose)
    ndbout_c("%lldmb data -> %d entries (%dkb)",
	   mbytes, maxidx, 32*pages);
  
  for (Uint32 i = 0; i<maxidx; i++)
  {
    Uint32 *ptr = arr.set(i);
    require(ptr);
    * ptr = i;
  }

  srand(seed);

  if (seq)
  {
    seq = rand();
    seqmask = ~(Uint32)0;
  }

  if (verbose)
    ndbout_c("Timing %d %s reads (seed: %u)", cnt,
	   seq ? "sequential" : "random", seed);

  for (Uint32 i = 0; i<10; i++)
  {
    Uint32 sum0 = 0, sum1 = 0;
    Uint64 start = my_micro_time();
    for (Uint32 i = 0; i<cnt; i++)
    {
      Uint32 idx = ((rand() & (~seqmask)) + ((i + seq) & seqmask)) % maxidx;
      Uint32 *ptr = arr.get(idx);
      sum0 += idx;
      sum1 += *ptr;
    }
    start = my_micro_time() - start;
    float uspg = (float)start; uspg /= cnt;
    if (verbose)
      ndbout_c("Elapsed %lldus diff: %d -> %f us/get", start, sum0 - sum1, uspg);
  }
  return true;
}

static
bool
write(DynArr256& arr, int argc, char ** argv)
{
  Uint32 seq = 0, seqmask = 0;
  Uint32 cnt = 100000;
  Uint64 mbytes = 16*1024;
  Uint32 seed = (Uint32) time(0);

  for (int i = 1; i<argc; i++)
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

  if (mbytes >= 134217720)
  {
    ndberr.println("--mbytes must be less than 134217720");
    return false;
  }
  Uint32 maxidx = (Uint32)((1024*mbytes+31) / 32);
  Uint32 nodes = (maxidx+255) / 256;
  Uint32 pages = (nodes + 29)/ 30;
  if (verbose)
    ndbout_c("%lldmb data -> %d entries (%dkb)",
	   mbytes, maxidx, 32*pages);

  srand(seed);

  if (seq)
  {
    seq = rand();
    seqmask = ~(Uint32)0;
  }

  if (verbose)
    ndbout_c("Timing %d %s writes (seed: %u)", cnt,
	   seq ? "sequential" : "random", seed);
  for (Uint32 i = 0; i<10; i++)
  {
    Uint64 start = my_micro_time();
    for (Uint32 i = 0; i<cnt; i++)
    {
      Uint32 idx = ((rand() & (~seqmask)) + ((i + seq) & seqmask)) % maxidx;
      Uint32 *ptr = arr.set(idx);
      if (ptr == NULL) break; /* out of memory */
      *ptr = i;
    }
    start = my_micro_time() - start;
    float uspg = (float)start; uspg /= cnt;
    if (verbose)
      ndbout_c("Elapsed %lldus -> %f us/set", start, uspg);
    if (!release(arr))
      return false;
  }
  return true;
}

static
void
usage(FILE *f, int argc, char **argv)
{
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "\t%s --simple <index1> <index2> ... <indexN>\n", argv[0]);
  fprintf(stderr, "\t%s --basic\n", argv[0]);
  fprintf(stderr, "\t%s { --read | --write } [ --mbytes=<megabytes> | --mbytes=<gigabytes>[gG] ] [ --cnt=<count> ] [ --seq ]\n", argv[0]);
  fprintf(stderr, "defaults:\n");
  fprintf(stderr, "\t--mbytes=16g\n");
  fprintf(stderr, "\t--cnt=100000\n");
}

# include "test_context.hpp"

#ifdef TEST_DYNARR256
static
char* flatten(int argc, char** argv) /* NOT MT-SAFE */
{
  static char buf[10000];
  size_t off = 0;
  for (; argc > 0; argc--, argv++)
  {
    int i = 0;
    if (off > 0 && (off + 1 < sizeof(buf)))
      buf[off++] = ' ';
    for (i = 0; (off + 1 < sizeof(buf)) && argv[0][i] != 0; i++, off++)
      buf[off] = argv[0][i];
    buf[off] = 0;
  }
  return buf;
}
#endif

int
main(int argc, char** argv)
{
#ifndef TEST_DYNARR256
  verbose = 1;
  if (argc == 1) {
    usage(stderr, argc, argv);
    exit(2);
  }
#else
  verbose = 0;
#endif
  while (argc > 1 && strcmp(argv[1], "-v") == 0)
  {
    verbose++;
    argc--;
    argv++;
  }

  Pool_context pc = test_context(10000 /* pages */);

  DynArr256Pool pool;
  pool.init(0x2001, pc);

  DynArr256::Head head;
  DynArr256 arr(pool, head);

#ifdef TEST_DYNARR256
  if (argc == 1)
  {
    char *argv[2] = { (char*)"dummy", NULL };
    plan(5);
    ok(simple(arr, 1, argv), "simple");
    ok(basic(arr, 1, argv), "basic");
    ok(read(arr, 1, argv), "read");
    ok(write(arr, 1, argv), "write");
  }
  else if (strcmp(argv[1], "--simple") == 0)
  {
    plan(2);
    ok(simple(arr, argc - 1, argv + 1), "simple %s", flatten(argc - 1, argv + 1));
  }
  else if (strcmp(argv[1], "--basic") == 0)
  {
    plan(2);
    ok(basic(arr, argc - 1, argv + 1), "basic %s", flatten(argc - 1, argv + 1));
  }
  else if (strcmp(argv[1], "--read") == 0)
  {
    plan(2);
    ok(read(arr, argc - 1, argv + 1), "read %s", flatten(argc - 1, argv + 1));
  }
  else if (strcmp(argv[1], "--write") == 0)
  {
    plan(2);
    ok(write(arr, argc - 1, argv + 1), "write %s", flatten(argc - 1, argv + 1));
  }
  else
  {
    usage(stderr, argc, argv);
    BAIL_OUT("Bad usage: %s %s", argv[0], flatten(argc - 1, argv + 1));
  }
#else
  if (strcmp(argv[1], "--simple") == 0)
    simple(arr, argc - 1, argv + 1);
  else if (strcmp(argv[1], "--basic") == 0)
    basic(arr, argc - 1, argv + 1);
  else if (strcmp(argv[1], "--read") == 0)
    read(arr, argc - 1, argv + 1);
  else if (strcmp(argv[1], "--write") == 0)
    write(arr, argc - 1, argv + 1);
  else
  {
    usage(stderr, argc, argv);
    exit(2);
  }
#endif

  release(arr);
  if (verbose)
    ndbout_c("allocatedpages: %d (max %d) releasedpages: %d allocatednodes: %d (max %d) releasednodes: %d",
           allocatedpages, maxallocatedpages,
           releasedpages,
           allocatednodes, maxallocatednodes,
           releasednodes);

#ifdef TEST_DYNARR256
  ok(allocatednodes == releasednodes &&
     allocatedpages == releasedpages,
     "release");
  return exit_status();
#else
  return 0;
#endif
}

#endif

#define JAM_FILE_ID 233

