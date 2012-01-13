/* Copyright (c) 2005, 2006 MySQL AB
   Copyright (c) 2009-2011, Monty Program Ab
   Use is subject to license terms.
   Copyright (c) 2009-2011, Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "strings_def.h"

static void *my_str_malloc_default(size_t size)
{
  void *ret= malloc(size);
  if (!ret)
    exit(1);
  return ret;
}

static void my_str_free_default(void *ptr)
{
  free(ptr);
}

void *(*my_str_malloc)(size_t)= &my_str_malloc_default;
void (*my_str_free)(void *)= &my_str_free_default;
