/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved. 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// First include (the generated) my_config.h, to get correct platform defines,
// then gtest.h (before any other MySQL headers), to avoid min() macros etc ...
#include "my_config.h"
#include <gtest/gtest.h>

#include "my_global.h"
#include "my_dbug.h"

#if defined(DBUG_OFF)
TEST(DebugTest, NoSuicide)
{
  DBUG_SUICIDE();
}
#else 
TEST(DebugDeathTest, Suicide)
{
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(DBUG_SUICIDE(), "");
}
#endif
