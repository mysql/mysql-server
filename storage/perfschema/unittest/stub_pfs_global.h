/* Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <my_global.h>
#include <my_sys.h>
#include <pfs_global.h>
#include <string.h>

bool pfs_initialized= false;
size_t pfs_allocated_memory_size= 0;
size_t pfs_allocated_memory_count= 0;

bool stub_alloc_always_fails= true;
int stub_alloc_fails_after_count= 0;

void *pfs_malloc(PFS_builtin_memory_class *klass, size_t size, myf)
{
  /*
    Catch non initialized sizing parameter in the unit tests.
  */
  DBUG_ASSERT(size <= 100*1024*1024);

  if (stub_alloc_always_fails)
    return NULL;

  if (--stub_alloc_fails_after_count <= 0)
    return NULL;

  void *ptr= malloc(size);
  if (ptr != NULL)
    memset(ptr, 0, size);
  return ptr;
}

void pfs_free(PFS_builtin_memory_class *, size_t, void *ptr)
{
  if (ptr != NULL)
    free(ptr);
}

void *pfs_malloc_array(PFS_builtin_memory_class *klass, size_t n, size_t size, myf flags)
{
  size_t array_size= n * size;
  /* Check for overflow before allocating. */
  if (is_overflow(array_size, n, size))
    return NULL;
  return pfs_malloc(klass, array_size, flags);
}

void pfs_free_array(PFS_builtin_memory_class *klass, size_t n, size_t size, void *ptr)
{
  if (ptr == NULL)
    return;
  size_t array_size= n * size;
  return pfs_free(klass, array_size, ptr);
}

bool is_overflow(size_t product, size_t n1, size_t n2)
{
  if (n1 != 0 && (product / n1 != n2))
    return true;
  else
    return false;
}

void pfs_print_error(const char *format, ...)
{
}


