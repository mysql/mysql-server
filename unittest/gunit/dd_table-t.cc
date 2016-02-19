/* Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include "my_config.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "../sql/dd/properties.h"
#include "../sql/dd/impl/collection_impl.h"
#include "../sql/dd/impl/types/table_impl.h"
#include "../sql/dd/types/column.h"
#include "../sql/dd/dd.h"

namespace dd_columns_unittest {

class ColumnsTest: public ::testing::Test
{
protected:
  typedef dd::Collection<dd::Column> Column_collection;
  Column_collection m_columns;
  dd::Table_impl *m_table;

  void SetUp()
  {
    m_table= dynamic_cast<dd::Table_impl*>(dd::create_object<dd::Table>());
  }

  void TearDown()
  {
    delete m_table;
  }

  dd::Column *add_column()
  {
    return m_table->add_column();
  }

  const dd::Column *get_column(std::string name)
  {
    return m_table->get_column(name);
  }

  ColumnsTest() {}
private:
  GTEST_DISALLOW_COPY_AND_ASSIGN_(ColumnsTest);
};

TEST_F(ColumnsTest, ColumnsConstIterator)
{
  dd::Column *c1= add_column();
  c1->set_name("col1");

  dd::Column *c2= add_column();
  c2->set_name("col2");

  dd::Column *c3= add_column();
  c3->set_name("Col3");

  dd::Column *c4= add_column();
  c4->set_name("col3");

  dd::Column *c5= add_column();
  c5->set_name("col4");

  const dd::Column *found_c3= get_column("Col3");
  const dd::Column *found_c4= get_column("col3");
  const dd::Column *found_c5= get_column("col4");

  EXPECT_TRUE(found_c3 == c3);
  EXPECT_TRUE(found_c4 == c4);
  EXPECT_TRUE(found_c5 == c5);

}

}
