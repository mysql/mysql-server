/*
   Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "opt_costconstantcache.h"
#include "fake_table.h"
#include "test_utils.h"


namespace costmodel_unittest {

using my_testing::Server_initializer;

class CostModelTest : public ::testing::Test
{
protected:
  virtual void SetUp()
  {
    // Add a storage engine to the hton2plugin array.
    // This is needed for the cost model to add cost constants
    // for the storage engine
    LEX_STRING engine_name= {C_STRING_WITH_LEN("InnoDB")};

    hton2plugin[0]= new st_plugin_int();
    hton2plugin[0]->name= engine_name;

    initializer.SetUp();
  }
  virtual void TearDown()
  {
    initializer.TearDown();
    delete hton2plugin[0];
    hton2plugin[0]= NULL;
  }

  THD *thd() { return initializer.thd(); }

  Server_initializer initializer;
};

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
TEST_F(CostModelTest, CostModelServer)
{
  const uint rows= 3;

  // Create and initialize the server cost model
  Cost_model_server cm;
  cm.init();

  // Create and initialize a cost constant object that will be used
  // for verifying default values for cost constants
  const Server_cost_constants default_server_cost;

  // Test row evaluate cost
  EXPECT_EQ(cm.row_evaluate_cost(1.0), default_server_cost.row_evaluate_cost());
  EXPECT_EQ(cm.row_evaluate_cost(rows),
            rows * cm.row_evaluate_cost(1.0));

  // Test key compare cost
  EXPECT_EQ(cm.key_compare_cost(1.0), default_server_cost.key_compare_cost());
  EXPECT_EQ(cm.key_compare_cost(rows), rows * cm.key_compare_cost(1.0));

  // Cost of creating a tempoary table without inserting data into it
  EXPECT_EQ(cm.tmptable_create_cost(Cost_model_server::MEMORY_TMPTABLE),
            default_server_cost.memory_temptable_create_cost());
  EXPECT_EQ(cm.tmptable_create_cost(Cost_model_server::DISK_TMPTABLE),
            default_server_cost.disk_temptable_create_cost());

  // Cost of inserting one row in a temporary table
  EXPECT_EQ(cm.tmptable_readwrite_cost(Cost_model_server::MEMORY_TMPTABLE,
                                       1.0, 0.0),
            default_server_cost.memory_temptable_row_cost());
  EXPECT_EQ(cm.tmptable_readwrite_cost(Cost_model_server::DISK_TMPTABLE,
                                       1.0, 0.0),
            default_server_cost.disk_temptable_row_cost());

  // Cost of reading one row in a temporary table
  EXPECT_EQ(cm.tmptable_readwrite_cost(Cost_model_server::MEMORY_TMPTABLE,
                                       0.0, 1.0),
            default_server_cost.memory_temptable_row_cost());
  EXPECT_EQ(cm.tmptable_readwrite_cost(Cost_model_server::DISK_TMPTABLE,
                                       0.0, 1.0),
            default_server_cost.disk_temptable_row_cost());

  // Tests for temporary tables that are independent of cost constants
  test_tmptable_cost(&cm, Cost_model_server::MEMORY_TMPTABLE);
  test_tmptable_cost(&cm, Cost_model_server::DISK_TMPTABLE);
}


/*
  Test the Cost_model_table interface.
*/
TEST_F(CostModelTest, CostModelTable)
{
  const uint rows= 3;
  const double blocks= 4.0;
  const uint key= 0;

  // A table is needed in order to initialize the table cost model
  Fake_TABLE table(1, false);

  // Create and initialize a cost model table object
  Cost_model_server cost_model_server;
  cost_model_server.init();
  Cost_model_table cm;
  cm.init(&cost_model_server, &table);

  // Create and initialize a cost constant object that will be used
  // for verifying default values for cost constants
  const Server_cost_constants default_server_cost;
  const SE_cost_constants default_engine_cost;

  // Test row evaluate cost
  EXPECT_EQ(cm.row_evaluate_cost(1.0), default_server_cost.row_evaluate_cost());
  EXPECT_EQ(cm.row_evaluate_cost(rows),
            rows * cm.row_evaluate_cost(1.0));

  // Test key compare cost
  EXPECT_EQ(cm.key_compare_cost(1.0), default_server_cost.key_compare_cost());
  EXPECT_EQ(cm.key_compare_cost(rows),
            rows * cm.key_compare_cost(1.0));

  // Test io block read cost
  EXPECT_EQ(cm.io_block_read_cost(1.0),
            default_engine_cost.io_block_read_cost());
  EXPECT_EQ(cm.io_block_read_cost(blocks),
            blocks * cm.io_block_read_cost(1.0));

  // Test page_read_cost() with table in memory buffer
  EXPECT_EQ(cm.page_read_cost(1.0),
            default_engine_cost.memory_block_read_cost());
  EXPECT_EQ(cm.page_read_cost(blocks),
            blocks * cm.page_read_cost(1.0));

  // Test page_read_cost() with table data on disk
  table.file->stats.table_in_mem_estimate= 0.0; // Table is on disk
  EXPECT_EQ(cm.page_read_cost(1.0),
            default_engine_cost.io_block_read_cost());
  EXPECT_EQ(cm.page_read_cost(blocks),
            blocks * cm.page_read_cost(1.0));

  // Test page_read_cost_index() with index data in memory
  table.key_info[key].set_in_memory_estimate(1.0); // Index is in memory
  EXPECT_EQ(cm.page_read_cost_index(key, 1.0),
            default_engine_cost.memory_block_read_cost());
  EXPECT_EQ(cm.page_read_cost_index(key, blocks),
            blocks * cm.page_read_cost_index(key, 1.0));

  // Test page_read_oost_index() with index data on disk
  table.key_info[key].set_in_memory_estimate(0.0); // Index is on disk
  EXPECT_EQ(cm.page_read_cost_index(key, 1.0),
            default_engine_cost.io_block_read_cost());
  EXPECT_EQ(cm.page_read_cost_index(key, blocks),
            blocks * cm.page_read_cost_index(key, 1.0));

  // Test disk seek base cost
  EXPECT_EQ(cm.disk_seek_base_cost(),
            DISK_SEEK_BASE_COST * cm.io_block_read_cost(1.0));

  // Test disk seek cost
  EXPECT_GT(cm.disk_seek_cost(2.0), cm.disk_seek_cost(1.0));
}

}
