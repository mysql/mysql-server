/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>
#include <stddef.h>
#include <string>

#include "sql/item_func.h"
#include "sql/sql_lex.h"
#include "template_utils.h"
#include "thr_lock.h"
#include "unittest/gunit/parsertest.h"
#include "unittest/gunit/test_utils.h"

namespace join_syntax_unittest {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

class JoinSyntaxTest : public ParserTest
{
};


void check_name_resolution_tables(std::initializer_list<const char*> aliases,
                                  SQL_I_List<TABLE_LIST> tables)
{
  TABLE_LIST *table_list= tables.first;
  for (auto alias : aliases)
  {
    ASSERT_FALSE(table_list == NULL);
    EXPECT_STREQ(alias, table_list->alias)
      << "Wrong table alias " << table_list->alias
      << ", expected " << alias << ".";
    table_list= table_list->next_name_resolution_table;
  }
}


TEST_F(JoinSyntaxTest, CrossJoin)
{
  SELECT_LEX *query_block= parse("SELECT * FROM t1 JOIN t2 JOIN t3");
  check_name_resolution_tables({"t1", "t2", "t3"}, query_block->table_list);
}


TEST_F(JoinSyntaxTest, CrossJoinOn)
{
  SELECT_LEX *query_block= parse("SELECT * FROM t1 JOIN t2 JOIN t3 ON 1");
  check_name_resolution_tables({"t1", "t2", "t3"}, query_block->table_list);
}

} // namespace join_syntax_unittest
