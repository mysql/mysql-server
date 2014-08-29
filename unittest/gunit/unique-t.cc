/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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
#include <stddef.h>

#include "test_utils.h"
#include "fake_costmodel.h"
#include "sql_class.h"
#include "uniques.h"

namespace unique_unittest {

using my_testing::Server_initializer;

class UniqueCostTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;
};

// This is an excerpt of code from get_best_disjunct_quick()
TEST_F(UniqueCostTest, GetUseCost)
{
  const ulong num_keys= 328238;
  const ulong key_size= 96;

  // Set up the optimizer cost model
  Fake_Cost_model_table cost_model_table;

  size_t unique_calc_buff_size=
    Unique::get_cost_calc_buff_size(num_keys, key_size, MIN_SORT_MEMORY);
  void *rawmem= alloc_root(thd()->mem_root,
                           unique_calc_buff_size * sizeof(uint));
  Bounds_checked_array<uint> cost_buff=
    Bounds_checked_array<uint>(static_cast<uint*>(rawmem), unique_calc_buff_size);
  const double dup_removal_cost=
    Unique::get_use_cost(cost_buff, num_keys, key_size, MIN_SORT_MEMORY,
                         &cost_model_table);
  EXPECT_GT(dup_removal_cost, 0.0);
}

}
