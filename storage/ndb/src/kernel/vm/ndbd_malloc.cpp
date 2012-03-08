/*
   Copyright (C) 2005-2007 MySQL AB, 2010 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#include <ndb_global.h>
#include "ndbd_malloc.hpp"
#include <NdbMem.h>

//#define TRACE_MALLOC
#ifdef TRACE_MALLOC
#include <stdio.h>
#endif

extern void do_refresh_watch_dog(Uint32 place);

void
ndbd_alloc_touch_mem(void *p, size_t sz, volatile Uint32 * watchCounter)
{
  Uint32 tmp = 0;
  if (watchCounter == 0)
    watchCounter = &tmp;

  unsigned char * ptr = (unsigned char*)p;
  while (sz >= 4096)
  {
    * ptr = 0;
    ptr += 4096;
    sz -= 4096;
    * watchCounter = 9;
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
