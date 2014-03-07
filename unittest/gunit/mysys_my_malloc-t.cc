/* Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include <my_global.h>
#include <my_sys.h>

namespace mysys_my_malloc_unittest {

TEST(Mysys, Malloc)
{
  void *p;

  p= my_malloc(PSI_NOT_INSTRUMENTED, 0, MYF(0));
  EXPECT_TRUE(p != NULL) << "Zero-sized block allocation.";

  p= my_realloc(PSI_NOT_INSTRUMENTED, p, 32, MYF(0));
  EXPECT_TRUE(p != NULL) << "Reallocated zero-sized block.";

  p= my_realloc(PSI_NOT_INSTRUMENTED, p, 16, MYF(0));
  EXPECT_TRUE(p != NULL) << "Trimmed block.";

  my_free(p);
  p= NULL;

  my_free(p);
}

}
