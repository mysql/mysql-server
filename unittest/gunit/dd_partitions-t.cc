/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "../sql/dd/types/partition.h"
#include "../sql/dd/impl/types/partition_impl.h"
#include "../sql/dd/impl/types/table_impl.h"


namespace dd_partitions_unittest {

class PartitionsTest: public ::testing::Test
{
protected:
  typedef dd::Collection<dd::Partition> Partition_collection;
  Partition_collection m_partitions;
  dd::Table_impl *m_table;

  void SetUp()
  {
    m_table= new dd::Table_impl();
  }

  void TearDown()
  {
    delete m_table;
  }

  dd::Partition *add_partition()
  {
    return m_table->add_partition();
  }

  std::vector<int> get_partitions_levels()
  {
    const dd::Table_impl *table= m_table;
    std::vector<int> partition_levels;
    std::unique_ptr<dd::Iterator<const dd::Partition> > it1(table->partitions());

    while(true) {
      const dd::Partition *p= it1->next();
      if (!p) break;
      partition_levels.push_back(p->level());
    }
    return partition_levels;
  }

  template <class Iterator>
  bool is_sorted(Iterator first, Iterator last)
  {
    if (first==last)
      return true;
    Iterator next= first;
    while(++next != last)
    {
      if (*next < *first)
        return false;
      ++first;
    }
    return true;
  }

  PartitionsTest() {}
private:
  GTEST_DISALLOW_COPY_AND_ASSIGN_(PartitionsTest);
};

TEST_F(PartitionsTest, PartitionsConstIterator)
{
  dd::Partition *p1= add_partition();
  p1->set_name("p1");
  p1->set_level(3);
  p1->set_number(5);
  p1->set_comment("P1");

  dd::Partition *p2= add_partition();
  p2->set_name("p2");
  p2->set_level(1);
  p2->set_number(7);
  p2->set_comment("P2");

  dd::Partition *p3= add_partition();
  p3->set_name("p3");
  p3->set_level(7);
  p3->set_number(10);
  p3->set_comment("P3");

  const std::vector<int>& sorted_partition_levels= get_partitions_levels();
  EXPECT_TRUE(is_sorted(sorted_partition_levels.begin(),
                        sorted_partition_levels.end()));
  dd::Partition *p4= add_partition();
  p4->set_name("p4");
  p4->set_level(9);
  p4->set_number(17);
  p4->set_comment("P4");

  dd::Partition *p5= add_partition();
  p5->set_name("p5");
  p5->set_level(2);
  p5->set_number(27);
  p5->set_comment("P5");

  const std::vector<int>& sorted_partition_levels1= get_partitions_levels();
  EXPECT_TRUE(is_sorted(sorted_partition_levels1.begin(),
                        sorted_partition_levels1.end()));

}
}
