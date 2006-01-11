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



#include "ndbd_malloc_impl.hpp"
#include <ndb_global.h>

Uint32
Ndbd_mem_manager::log2(Uint32 input)
{
  input = input | (input >> 8);
  input = input | (input >> 4);
  input = input | (input >> 2);
  input = input | (input >> 1);
  Uint32 output = (input & 0x5555) + ((input >> 1) & 0x5555);
  output = (output & 0x3333) + ((output >> 2) & 0x3333);
  output = output + (output >> 4);
  output = (output & 0xf) + ((output >> 8) & 0xf);
  return output;
}

Ndbd_mem_manager::Ndbd_mem_manager(Uint32 default_grow)
{
  m_grow_size = default_grow;
  bzero(m_buddy_lists, sizeof(m_buddy_lists));
  m_base = 0;
  m_base_page = 0;
  
  m_pages_alloc = 0;
  m_pages_used = 0;
  
  if (sizeof(Free_page_data) != (4 * (1 << FPD_2LOG)))
  {
    abort();
  }
}

bool
Ndbd_mem_manager::init(Uint32 pages)
{
  assert(m_base == 0);
  assert(m_base_page == 0);
  assert(m_pages_alloc == 0);
  pages = pages ? pages : m_grow_size;
  
  m_base = malloc((2 + pages) * sizeof(Alloc_page));
  UintPtr ptr = (UintPtr)m_base;
  UintPtr rem = ptr % sizeof(Alloc_page);
  if (rem)
  {
    ptr += sizeof(Alloc_page) - rem;
  }
  else
  {
    pages++;
  }
  m_base_page = (Alloc_page*)ptr;
  m_pages_alloc += pages;
  m_pages_used += pages;

  Uint32 bmp = (pages + (1 << BPP_2LOG) - 1) >> BPP_2LOG;
  for(Uint32 i = 0; i < bmp; i++)
  {
    Uint32 start = i * (1 << BPP_2LOG);
    Uint32 end = start + (1 << BPP_2LOG);
    end = end > m_pages_alloc ? m_pages_alloc : end - 1;
    Alloc_page *ptr = m_base_page + start;
    BitmaskImpl::clear(BITMAP_WORDS, ptr->m_data);
    
    release(start+1, end - 1 - start);    
  }
}

void
Ndbd_mem_manager::release(Uint32 start, Uint32 cnt)
{
  assert(m_pages_used >= cnt);
  assert(start);
  m_pages_used -= cnt;

  set(start, start+cnt-1);
  
  release_impl(start, cnt);
}

void
Ndbd_mem_manager::release_impl(Uint32 start, Uint32 cnt)
{
  assert(start);  

  Uint32 test = check(start-1, start+cnt);
  if (test & 1)
  {
    Free_page_data *fd = get_free_page_data(m_base_page + start - 1, 
					    start - 1);
    Uint32 sz = fd->m_size;
    Uint32 left = start - sz;
    remove_free_list(left, fd->m_list);
    cnt += sz;
    start = left;
  }
 
  Uint32 right = start + cnt;
  if (test & 2)
  {
    Free_page_data *fd = get_free_page_data(m_base_page+right, right);
    Uint32 sz = fd->m_size;
    remove_free_list(right, fd->m_list);
    cnt += sz;
  }
  
  insert_free_list(start, cnt);
}

void
Ndbd_mem_manager::alloc(Uint32* ret, Uint32 *pages, Uint32 min)
{
  Uint32 start, i;
  Uint32 cnt = * pages;
  Uint32 list = log2(cnt - 1);
  
  assert(cnt);
  assert(list <= 16);

  for (i = list; i < 16; i++) 
  {
    if ((start = m_buddy_lists[i]))
    {
/* ---------------------------------------------------------------- */
/*       PROPER AMOUNT OF PAGES WERE FOUND. NOW SPLIT THE FOUND     */
/*       AREA AND RETURN THE PART NOT NEEDED.                       */
/* ---------------------------------------------------------------- */
      
      Uint32 sz = remove_free_list(start, i);
      Uint32 extra = sz - cnt;
      assert(sz >= cnt);
      if (extra)
      {
	insert_free_list(start + cnt, extra);
	clear_and_set(start, start+cnt-1);
      }
      else
      {
	clear(start, start+cnt-1);
      }
      * ret = start;
      m_pages_used += cnt;
      assert(m_pages_used <= m_pages_alloc);
      return;
    }
  }

  /**
   * Could not find in quaranteed list...
   *   search in other lists...
   */

  Uint32 min_list = log2(min - 1);
  assert(list >= min_list);
  for (i = list - 1; i >= min_list; i--) 
  {
    if ((start = m_buddy_lists[i]))
    {
      Uint32 sz = remove_free_list(start, i);
      Uint32 extra = sz - cnt;
      if (sz > cnt)
      {
	insert_free_list(start + cnt, extra);	
	sz -= extra;
	clear_and_set(start, start+sz-1);
      }
      else
      {
	clear(start, start+sz-1);
      }

      * ret = start;
      * pages = sz;
      m_pages_used += sz;
      assert(m_pages_used <= m_pages_alloc);
      return;
    }
  }
  * pages = 0;
}

void
Ndbd_mem_manager::insert_free_list(Uint32 start, Uint32 size)
{
  Uint32 list = log2(size) - 1;
  Uint32 last = start + size - 1;

  Uint32 head = m_buddy_lists[list];
  Free_page_data* fd_first = get_free_page_data(m_base_page+start, 
						start);
  fd_first->m_list = list;
  fd_first->m_next = head;
  fd_first->m_prev = 0;
  fd_first->m_size = size;

  Free_page_data* fd_last = get_free_page_data(m_base_page+last, last);
  fd_last->m_list = list;
  fd_last->m_next = head;
  fd_last->m_prev = 0;
  fd_last->m_size = size;
  
  if (head)
  {
    Free_page_data* fd = get_free_page_data(m_base_page+head, head);
    assert(fd->m_prev == 0);
    assert(fd->m_list == list);
    fd->m_prev = start;
  }
  
  m_buddy_lists[list] = start;
}

Uint32 
Ndbd_mem_manager::remove_free_list(Uint32 start, Uint32 list)
{
  Free_page_data* fd = get_free_page_data(m_base_page+start, start);
  Uint32 size = fd->m_size;
  Uint32 next = fd->m_next;
  Uint32 prev = fd->m_prev;
  assert(fd->m_list == list);
  
  if (prev)
  {
    assert(m_buddy_lists[list] != start);
    fd = get_free_page_data(m_base_page+prev, prev);
    assert(fd->m_next == start);
    assert(fd->m_list == list);
    fd->m_next = next;
  }
  else
  {
    assert(m_buddy_lists[list] == start);
    m_buddy_lists[list] = next;
  }
  
  if (next)
  {
    fd = get_free_page_data(m_base_page+next, next);
    assert(fd->m_list == list);
    assert(fd->m_prev == start);
    fd->m_prev = prev;
  }

  return size;
}

Uint32
Ndbd_mem_manager::get_no_allocated_pages() const
{
  return m_pages_alloc;
}

Uint32
Ndbd_mem_manager::get_no_used_pages() const
{
  return m_pages_used;
}

Uint32
Ndbd_mem_manager::get_no_free_pages() const
{
  return m_pages_alloc - m_pages_used;
}


void
Ndbd_mem_manager::dump() const
{
  for(Uint32 i = 0; i<16; i++)
  {
    printf(" list: %d - ", i);
    Uint32 head = m_buddy_lists[i];
    while(head)
    {
      Free_page_data* fd = get_free_page_data(m_base_page+head, head); 
      printf("[ i: %d prev %d next %d list %d size %d ] ", 
	     head, fd->m_prev, fd->m_next, fd->m_list, fd->m_size);
      head = fd->m_next;
    }
    printf("EOL\n");
  }
}

#ifdef UNIT_TEST

#include <Vector.hpp>

struct Chunk {
  Uint32 pageId;
  Uint32 pageCount;
};

int 
main(void)
{
  printf("Startar modul test av Page Manager\n");
#define DEBUG 0

  Ndbd_mem_manager mem;
  mem.init(32000);
  Vector<Chunk> chunks;
  const Uint32 LOOPS = 100000;
  for(Uint32 i = 0; i<LOOPS; i++){
    //mem.dump();
    
    // Case
    Uint32 c = (rand() % 100);
    const Uint32 free = mem.get_no_allocated_pages() - mem.get_no_used_pages();
    if (c < 60)
    {
      c = 0;
    } 
    else if (c < 93)
    {
      c = 1;
    }
    else
    {
      c = 2;
    }
    
    Uint32 alloc = 0;
    if(free <= 1)
    {
      c = 0;
      alloc = 1;
    } 
    else 
    {
      alloc = 1 + (rand() % (free - 1));
    }  

    if(chunks.size() == 0 && c == 0)
    {
      c = 1 + rand() % 2;
    }
    
    if(DEBUG)
      printf("loop=%d ", i);
    switch(c){ 
    case 0:{ // Release
      const int ch = rand() % chunks.size();
      Chunk chunk = chunks[ch];
      chunks.erase(ch);
      mem.release(chunk.pageId, chunk.pageCount);
      if(DEBUG)
	printf(" release %d %d\n", chunk.pageId, chunk.pageCount);
    }
      break;
    case 2: { // Seize(n) - fail
      alloc += free;
      // Fall through
    }
    case 1: { // Seize(n) (success)
      Chunk chunk;
      chunk.pageCount = alloc;
      mem.alloc(&chunk.pageId, &chunk.pageCount, 1);
      if (DEBUG)
	printf(" alloc %d -> %d %d", alloc, chunk.pageId, chunk.pageCount);
      assert(chunk.pageCount <= alloc);
      if(chunk.pageCount != 0){
	chunks.push_back(chunk);
	if(chunk.pageCount != alloc) {
	  if (DEBUG)
	    printf(" -  Tried to allocate %d - only allocated %d - free: %d",
		   alloc, chunk.pageCount, free);
	}
      } else {
	if (DEBUG)
	  printf("  Failed to alloc %d pages with %d pages free",
		 alloc, free);
      }
      if (DEBUG)
	printf("\n");
      if(alloc == 1 && free > 0)
	assert(chunk.pageCount == alloc);
    }
      break;
    }
  }
  if (!DEBUG)
    while(chunks.size() > 0){
      Chunk chunk = chunks.back();
      mem.release(chunk.pageId, chunk.pageCount);      
      chunks.erase(chunks.size() - 1);
    }
}

template class Vector<Chunk>;

#endif
