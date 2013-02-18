/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved. 

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "handler.h"

namespace cost_estimate_unittest {

TEST(CostEstimateTest, Basics)
{
  Cost_estimate ce1;

  EXPECT_EQ(0, ce1.total_cost());
  EXPECT_TRUE(ce1.is_zero());

  const double initial_io_cost= 4.5;

  ce1.add_io(initial_io_cost);
  EXPECT_FALSE(ce1.is_zero());
  EXPECT_DOUBLE_EQ(initial_io_cost, ce1.total_cost());

  const double initial_cpu_cost= 3.3;
  ce1.add_cpu(initial_cpu_cost);

  EXPECT_DOUBLE_EQ(initial_cpu_cost, ce1.get_cpu_cost());
  EXPECT_DOUBLE_EQ(initial_io_cost, ce1.get_io_cost());
  EXPECT_DOUBLE_EQ(initial_io_cost + initial_cpu_cost, ce1.total_cost());
  
  EXPECT_EQ(0, ce1.get_mem_cost());
  EXPECT_EQ(0, ce1.get_import_cost());

  const double initial_mem_cost= 7;
  const double initial_import_cost= 11;
  ce1.add_mem(initial_mem_cost);
  ce1.add_import(initial_import_cost);

  const double total_initial_cost= 
    initial_io_cost + initial_cpu_cost + initial_import_cost;
  EXPECT_DOUBLE_EQ(total_initial_cost, ce1.total_cost());
  
  const double added_io_cost= 1.5;
  ce1.add_io(added_io_cost);
  EXPECT_DOUBLE_EQ(initial_io_cost + added_io_cost, ce1.get_io_cost());
  EXPECT_DOUBLE_EQ(total_initial_cost + added_io_cost, ce1.total_cost());

  EXPECT_FALSE(ce1.is_zero());

  ce1.reset();
  EXPECT_TRUE(ce1.is_zero());
  
}

TEST(CostEstimateTest, Operators)
{
  Cost_estimate ce_io;

  EXPECT_EQ(0, ce_io.total_cost());
  EXPECT_TRUE(ce_io.is_zero());

  const double initial_io_cost= 4.5;
  ce_io.add_io(initial_io_cost);
  EXPECT_DOUBLE_EQ(initial_io_cost, ce_io.total_cost());

  Cost_estimate ce_cpu;
  const double initial_cpu_cost= 3.3;
  ce_cpu.add_cpu(initial_cpu_cost);
  EXPECT_DOUBLE_EQ(initial_cpu_cost, ce_cpu.total_cost());
  EXPECT_EQ(0, ce_cpu.get_io_cost());
  
  // Copy CTOR
  Cost_estimate ce_copy(ce_io);
  const double added_io_cost= 1.5;
  ce_io.add_io(added_io_cost); // should not add to ce_copy
  EXPECT_DOUBLE_EQ(initial_io_cost + added_io_cost, ce_io.total_cost());
  EXPECT_DOUBLE_EQ(initial_io_cost, ce_copy.total_cost());

  // operator+=
  ce_copy+= ce_cpu;
  EXPECT_DOUBLE_EQ(initial_io_cost + initial_cpu_cost, ce_copy.total_cost());
  
  // operator+
  Cost_estimate ce_copy2= ce_io + ce_cpu;
  const double copy2_totcost= 
    initial_io_cost + added_io_cost + initial_cpu_cost;
  EXPECT_DOUBLE_EQ(copy2_totcost, ce_copy2.total_cost());

  Cost_estimate ce_mem_import1;
  const double import1_mem_cost= 3;
  const double import1_import_cost= 5;
  ce_mem_import1.add_mem(import1_mem_cost);
  ce_mem_import1.add_import(import1_import_cost);

  Cost_estimate ce_mem_import2;
  const double import2_mem_cost= 11;
  const double import2_import_cost= 13;
  ce_mem_import2.add_mem(import2_mem_cost);
  ce_mem_import2.add_import(import2_import_cost);

  // operator+
  Cost_estimate ce_mi_copy= ce_mem_import1 + ce_mem_import2;
  EXPECT_DOUBLE_EQ(import1_import_cost + import2_import_cost, 
                   ce_mi_copy.total_cost());
  EXPECT_DOUBLE_EQ(import1_mem_cost + import2_mem_cost, 
                   ce_mi_copy.get_mem_cost());
  EXPECT_DOUBLE_EQ(import1_import_cost + import2_import_cost, 
                   ce_mi_copy.get_import_cost());

  // operator+=
  ce_mi_copy+= ce_mem_import1;
  EXPECT_DOUBLE_EQ(2*import1_import_cost + import2_import_cost, 
                   ce_mi_copy.total_cost());
  EXPECT_DOUBLE_EQ(2*import1_mem_cost + import2_mem_cost, 
                   ce_mi_copy.get_mem_cost());
  EXPECT_DOUBLE_EQ(2*import1_import_cost + import2_import_cost, 
                   ce_mi_copy.get_import_cost());

  // copy assignment
  Cost_estimate ce_copy3;
  ce_copy3= ce_copy2;
  EXPECT_DOUBLE_EQ(copy2_totcost, ce_copy3.total_cost());
}


} //namespace
