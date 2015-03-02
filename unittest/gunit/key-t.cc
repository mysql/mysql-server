/*
   Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "key.h"                                // st_key

namespace key_unittest {

/**
  Test the API for setting and getting in-memory estimates.
*/

TEST(KeyInMemoryEstimate, InMemoryEstimateAPI)
{ 
  st_key key;

  /*
    Test setting to not initialized.
  */
  key.set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);
  EXPECT_EQ(key.in_memory_estimate(), IN_MEMORY_ESTIMATE_UNKNOWN);

  /*
    Test setting to lowest valid value.
  */
  key.set_in_memory_estimate(0.0);
  EXPECT_EQ(key.in_memory_estimate(), 0.0);

  /*
    Test setting to highest valid value.
  */
  key.set_in_memory_estimate(1.0);
  EXPECT_EQ(key.in_memory_estimate(), 1.0);

  /*
    Test setting to 50%.
  */
  key.set_in_memory_estimate(0.5);
  EXPECT_EQ(key.in_memory_estimate(), 0.5);
}

}
