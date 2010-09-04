/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/perfschema/pfs_global.cc
  Miscellaneous global dependencies (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_global.h"

#include <stdlib.h>
#include <string.h>

bool pfs_initialized= false;
ulonglong pfs_allocated_memory= 0;

/**
  Memory allocation for the performance schema.
  The memory used internally in the performance schema implementation
  is allocated once during startup, and considered static thereafter.
*/
void *pfs_malloc(size_t size, myf flags)
{
  DBUG_ASSERT(! pfs_initialized);
  DBUG_ASSERT(size > 0);

  void *ptr= malloc(size);
  if (likely(ptr != NULL))
    pfs_allocated_memory+= size;
  if (likely((ptr != NULL) && (flags & MY_ZEROFILL)))
    memset(ptr, 0, size);
  return ptr;
}

void pfs_free(void *ptr)
{
  if (ptr != NULL)
    free(ptr);
}

void pfs_print_error(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  /*
    Printing to anything else, like the error log, would generate even more
    recursive calls to the performance schema implementation
    (file io is instrumented), so that could lead to catastrophic results.
    Printing to something safe, and low level: stderr only.
  */
  vfprintf(stderr, format, args);
  va_end(args);
  fflush(stderr);
}

