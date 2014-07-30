/*
   Copyright (c) 2005, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "ndbd_malloc.hpp"
#include <my_sys.h>
#include <ndb_global.h>
#include <NdbMem.h>
#include <NdbThread.h>
#include <NdbOut.hpp>

//#define TRACE_MALLOC
#ifdef TRACE_MALLOC
#include <stdio.h>
#endif

#define JAM_FILE_ID 287


extern void do_refresh_watch_dog(Uint32 place);

#define TOUCH_PARALLELISM 8
#define MIN_START_THREAD_SIZE (128 * 1024 * 1024)
#define TOUCH_PAGE_SIZE 4096
#define NUM_PAGES_BETWEEN_WATCHDOG_SETS 32768

struct AllocTouchMem
{
  volatile Uint32* watchCounter;
  size_t sz;
  void *p;
  Uint32 index;
};


// Enable/disable debug check for reads from uninitialized memory.
#if defined(VM_TRACE) || defined(ERROR_INSERT)
const bool debugUinitMemUse = true;
#else
const bool debugUinitMemUse = false;
#endif

extern "C"
void*
touch_mem(void* arg)
{
  struct AllocTouchMem* touch_mem_ptr = (struct AllocTouchMem*)arg;

  size_t sz = touch_mem_ptr->sz;
  Uint32 index = touch_mem_ptr->index;
  unsigned char *p = (unsigned char *)touch_mem_ptr->p;
  size_t num_pages_per_thread = 1;
  size_t first_page;
  size_t tot_pages = (sz + (TOUCH_PAGE_SIZE - 1)) / TOUCH_PAGE_SIZE;

  if (tot_pages > TOUCH_PARALLELISM)
  {
    num_pages_per_thread = ((tot_pages + (TOUCH_PARALLELISM - 1)) /
                           TOUCH_PARALLELISM);
  }

  first_page = index * num_pages_per_thread;

  if (first_page >= tot_pages)
  {
    return NULL; /* We're done, no page to handle */
  }
  else if ((tot_pages - first_page) < num_pages_per_thread)
  {
    num_pages_per_thread = tot_pages - first_page;
  }

  unsigned char * ptr = (unsigned char*)(p + (first_page * 4096));

  for (Uint32 i = 0;
       i < num_pages_per_thread;
       ptr += TOUCH_PAGE_SIZE, i++)
  {
    *ptr = 0;
    if (i % NUM_PAGES_BETWEEN_WATCHDOG_SETS == 0)
    {
      /* Roughly every 120 ms we come here in worst case */
      *(touch_mem_ptr->watchCounter) = 9;
    }
    if (debugUinitMemUse)
    {
      const unsigned char* end = p + sz;
      const size_t size = MIN(TOUCH_PAGE_SIZE, end - ptr);
      /*
        Initialize the memory to something likely to trigger access violations 
        if used as a pointer or array index, to make it easier to detect use of 
        uninitialized memory. See also TRASH macro.
       */
      MEM_CHECK_ADDRESSABLE(ptr, size);
      memset(ptr, 0xfb, size);
      /*
        Mark memory as being undefined for valgrind, so that valgrind may
        know that reads from this memory is an error.
       */
      MEM_UNDEFINED(ptr, size);
      *(touch_mem_ptr->watchCounter) = 9;
    }
  }
  return NULL;
}

void
ndbd_alloc_touch_mem(void *p, size_t sz, volatile Uint32 * watchCounter)
{
  struct NdbThread *thread_ptr[TOUCH_PARALLELISM];
  struct AllocTouchMem touch_mem_struct[TOUCH_PARALLELISM];

  Uint32 tmp = 0;
  if (watchCounter == 0)
  {
    watchCounter = &tmp;
  }

  for (Uint32 i = 0; i < TOUCH_PARALLELISM; i++)
  {
    touch_mem_struct[i].watchCounter = watchCounter;
    touch_mem_struct[i].sz = sz;
    touch_mem_struct[i].p = p;
    touch_mem_struct[i].index = i;

    thread_ptr[i] = NULL;
    if (sz > MIN_START_THREAD_SIZE)
    {
      thread_ptr[i] = NdbThread_Create(touch_mem,
                                       (NDB_THREAD_ARG*)&touch_mem_struct[i],
                                       0,
                                       "touch_thread",
                                       NDB_THREAD_PRIO_MEAN);
    }
    if (thread_ptr[i] == NULL)
    {
      touch_mem((void*)&touch_mem_struct[i]);
    }
  }
  for (Uint32 i = 0; i < TOUCH_PARALLELISM; i++)
  {
    void *dummy_status;
    if (thread_ptr[i])
    {
      NdbThread_WaitFor(thread_ptr[i], &dummy_status);
      NdbThread_Destroy(&thread_ptr[i]);
    }
  }
}


#ifdef TRACE_MALLOC
static void xxx(size_t size, size_t *s_m, size_t *s_k, size_t *s_b)
{
  *s_m = size/1024/1024;
  *s_k = (size - *s_m*1024*1024)/1024;
  *s_b = size - *s_m*1024*1024-*s_k*1024;
}
#endif

static Uint64 g_allocated_memory;
void *ndbd_malloc(size_t size)
{
  void *p = NdbMem_Allocate(size);
  if (p)
  {
    g_allocated_memory += size;

    ndbd_alloc_touch_mem(p, size, 0);

#ifdef TRACE_MALLOC
    {
      size_t s_m, s_k, s_b;
      xxx(size, &s_m, &s_k, &s_b);
      fprintf(stderr, "%p malloc(%um %uk %ub)", p, s_m, s_k, s_b);
      xxx(g_allocated_memory, &s_m, &s_k, &s_b);
      fprintf(stderr, "\t\ttotal(%um %uk %ub)\n", s_m, s_k, s_b);
    }
#endif
  }
  return p;
}

void ndbd_free(void *p, size_t size)
{
  NdbMem_Free(p);
  if (p)
  {
    g_allocated_memory -= size;
#ifdef TRACE_MALLOC
    fprintf(stderr, "%p free(%d)\n", p, size);
#endif
  }
}
