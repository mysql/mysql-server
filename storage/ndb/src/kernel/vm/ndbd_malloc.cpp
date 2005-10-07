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

#include <ndb_global.h>
#include "ndbd_malloc.hpp"
#include <NdbMem.h>

//#define TRACE_MALLOC
#ifdef TRACE_MALLOC
#include <stdio.h>
#endif

static void xxx(size_t size, size_t *s_m, size_t *s_k, size_t *s_b)
{
  *s_m = size/1024/1024;
  *s_k = (size - *s_m*1024*1024)/1024;
  *s_b = size - *s_m*1024*1024-*s_k*1024;
}

static Uint64 g_allocated_memory;
void *ndbd_malloc(size_t size)
{
  void *p = NdbMem_Allocate(size);
  if (p)
  {
    g_allocated_memory += size;
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
