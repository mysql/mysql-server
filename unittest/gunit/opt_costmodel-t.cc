/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "opt_costmodel.h"

namespace costmodel_unittest {


/*
  Tests for temporary tables that are not dependent on hard coded cost
  constants.
*/
void test_tmptable_cost(const Cost_model_server *cm,
                        Cost_model_server::enum_tmptable_type tmp_table_type)
{
  const uint rows= 3;

  // Cost of inserting and reading data in a temporary table
  EXPECT_EQ(cm->tmptable_readwrite_cost(tmp_table_type, rows, rows), 
            rows * cm->tmptable_readwrite_cost(tmp_table_type, 1.0, 1.0));
}


/*
  Test the Cost_model_server interface.
*/
TEST(CostModelTest, CostModelServer)
{
  const uint rows= 3; 

  // Create and initialize the server cost model
  Cost_model_server cm;
  cm.init();

  // Test row evaluate cost
  EXPECT_EQ(cm.row_evaluate_cost(1.0), ROW_EVALUATE_COST);
  EXPECT_EQ(cm.row_evaluate_cost(rows), 
            rows * cm.row_evaluate_cost(1.0));

  // Test key compare cost 
  EXPECT_EQ(cm.key_compare_cost(1.0), ROWID_COMPARE_COST);
  EXPECT_EQ(cm.key_compare_cost(rows), rows * cm.key_compare_cost(1.0));

  // Cost of creating a tempoary table without inserting data into it
  EXPECT_EQ(cm.tmptable_create_cost(Cost_model_server::MEMORY_TMPTABLE), 
            HEAP_TEMPTABLE_CREATE_COST);
  EXPECT_EQ(cm.tmptable_create_cost(Cost_model_server::DISK_TMPTABLE), 
            DISK_TEMPTABLE_CREATE_COST);

  // Cost of inserting one row in a temporary table
  EXPECT_EQ(cm.tmptable_readwrite_cost(Cost_model_server::MEMORY_TMPTABLE,
                                       1.0, 0.0), 
            HEAP_TEMPTABLE_ROW_COST);
  EXPECT_EQ(cm.tmptable_readwrite_cost(Cost_model_server::DISK_TMPTABLE,
                                       1.0, 0.0), 
            DISK_TEMPTABLE_ROW_COST);

  // Cost of reading one row in a temporary table
  EXPECT_EQ(cm.tmptable_readwrite_cost(Cost_model_server::MEMORY_TMPTABLE,
                                       0.0, 1.0), 
            HEAP_TEMPTABLE_ROW_COST);
  EXPECT_EQ(cm.tmptable_readwrite_cost(Cost_model_server::DISK_TMPTABLE,
                                       0.0, 1.0), 
            DISK_TEMPTABLE_ROW_COST);

  // Tests for temporary tables that are independent of cost constants
  test_tmptable_cost(&cm, Cost_model_server::MEMORY_TMPTABLE);
  test_tmptable_cost(&cm, Cost_model_server::DISK_TMPTABLE);
}


/*
  Test the Cost_model_table interface.
*/
TEST(CostModelTest, CostModelTable)
{
  const uint rows= 3; 
  const uint blocks= 4;

  // Create and initialize a cost model table object
  Cost_model_server cost_model_server;
  cost_model_server.init();
  Cost_model_table cm;
  cm.init(&cost_model_server);

  // Test row evaluate cost
  EXPECT_EQ(cm.row_evaluate_cost(1.0), ROW_EVALUATE_COST);
  EXPECT_EQ(cm.row_evaluate_cost(rows), 
            rows * cm.row_evaluate_cost(1.0));

  // Test key compare cost 
  EXPECT_EQ(cm.key_compare_cost(1.0), ROWID_COMPARE_COST);
  EXPECT_EQ(cm.key_compare_cost(rows),
            rows * cm.key_compare_cost(1.0));

  // Test io block read cost
  EXPECT_EQ(cm.io_block_read_cost(), 1.0);
  EXPECT_EQ(cm.io_block_read_cost(blocks),
            blocks * cm.io_block_read_cost());

  // Test disk seek base cost
  EXPECT_EQ(cm.disk_seek_base_cost(),
            DISK_SEEK_BASE_COST * cm.io_block_read_cost());

  // Test disk seek cost
  EXPECT_GT(cm.disk_seek_cost(2), cm.disk_seek_cost(1));
}

}
