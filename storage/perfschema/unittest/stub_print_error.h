/* Copyright (c) 2008 MySQL AB, 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

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

bool pfs_initialized= false;

void *pfs_malloc(size_t size, myf flags)
{
  void *ptr= malloc(size);
  if (ptr && (flags & MY_ZEROFILL))
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
  /* Do not pollute the unit test output with annoying messages. */
}

