/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

// First include (the generated) my_config.h, to get correct platform defines,
// then gtest.h (before any other MySQL headers), to avoid min() macros etc ...
#include "my_config.h"
#include <gtest/gtest.h>

#if defined(TARGET_OS_LINUX)

#include <malloc.h>

namespace {

void *nop_malloc_hook(size_t size, const void *caller)
{
  return NULL;
}

TEST(StdCxxNoThrow, NoThrow)
{
  typeof(__malloc_hook) old_malloc_hook= __malloc_hook;

  __malloc_hook= nop_malloc_hook;

  const int *pnull= NULL;
  int *ptr= new (std::nothrow) int;
  __malloc_hook= old_malloc_hook;

  EXPECT_EQ(pnull, ptr);
}

TEST(StdCxxExceptionInNew, NewThrowsException)
{
  typeof(__malloc_hook) old_malloc_hook= __malloc_hook;

  __malloc_hook= nop_malloc_hook;

  bool thrown= false;
  try
  {
    int *ptr= new int;
    *ptr= 0;
  }
  catch (std::bad_alloc &e)
  {
    thrown= true;
  }
  __malloc_hook= old_malloc_hook;

  EXPECT_TRUE(thrown);
}

}

#endif // TARGET_OS_LINUX
