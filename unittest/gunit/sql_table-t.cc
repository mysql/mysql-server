/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines,
// then gtest.h (before any other MySQL headers), to avoid min() macros etc ...
#include "my_config.h"
#include <gtest/gtest.h>

#include "mock_create_field.h"
#include "test_utils.h"
#include "mock_field_timestamp.h"
#include "item.h"
#include "sql_class.h"
#include "rpl_handler.h"                        // delegates_init()
#include "sql_table.h"

namespace sql_table_unittest {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

/*
  Test of functionality in the file sql_table.cc
 */
class SqlTableTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  THD *get_thd() { return initializer.thd(); }

  Server_initializer initializer;
};


/*
  Test of promote_first_timestamp_column(). We pass it a list of two TIMESTAMP
  NOT NULL columns, the first of which should be promoted to DEFAULT
  CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP. The second column should not
  be promoted.
 */
TEST_F(SqlTableTest, PromoteFirstTimestampColumn1)
{
  Mock_create_field column_1_definition(MYSQL_TYPE_TIMESTAMP, NULL, NULL);
  Mock_create_field column_2_definition(MYSQL_TYPE_TIMESTAMP, NULL, NULL);
  column_1_definition.flags|= NOT_NULL_FLAG;
  column_2_definition.flags|= NOT_NULL_FLAG;
  List<Create_field> definitions;
  definitions.push_front(&column_1_definition);
  definitions.push_back(&column_2_definition);
  promote_first_timestamp_column(&definitions);
  EXPECT_EQ(Field::TIMESTAMP_DNUN_FIELD, column_1_definition.unireg_check);
  EXPECT_EQ(Field::NONE, column_2_definition.unireg_check);
}



/*
  Test of promote_first_timestamp_column(). We pass it a list of two TIMESTAMP
  NOT NULL columns, the first of which should be promoted to DEFAULT
  CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP. The second column should not
  be promoted.
 */
TEST_F(SqlTableTest, PromoteFirstTimestampColumn2)
{
  Mock_create_field column_1_definition(MYSQL_TYPE_TIMESTAMP2, NULL, NULL);
  Mock_create_field column_2_definition(MYSQL_TYPE_TIMESTAMP2, NULL, NULL);
  column_1_definition.flags|= NOT_NULL_FLAG;
  column_2_definition.flags|= NOT_NULL_FLAG;
  List<Create_field> definitions;
  definitions.push_front(&column_1_definition);
  definitions.push_back(&column_2_definition);
  promote_first_timestamp_column(&definitions);
  EXPECT_EQ(Field::TIMESTAMP_DNUN_FIELD, column_1_definition.unireg_check);
  EXPECT_EQ(Field::NONE, column_2_definition.unireg_check);
}


/*
  Test of promote_first_timestamp_column(). We pass it a list of two columns,
  one TIMESTAMP NULL DEFAULT 1, and one TIMESTAMP NOT NULL. No promotion
  should take place.
 */
TEST_F(SqlTableTest, PromoteFirstTimestampColumn3)
{
  Item_string  *item_str= new Item_string("1", 1, &my_charset_latin1);
  Mock_create_field column_1_definition(MYSQL_TYPE_TIMESTAMP, item_str, NULL);
  Mock_create_field column_2_definition(MYSQL_TYPE_TIMESTAMP, NULL, NULL);
  column_2_definition.flags|= NOT_NULL_FLAG;
  List<Create_field> definitions;
  definitions.push_front(&column_1_definition);
  definitions.push_back(&column_2_definition);
  promote_first_timestamp_column(&definitions);
  EXPECT_EQ(Field::NONE, column_1_definition.unireg_check);
  EXPECT_EQ(Field::NONE, column_2_definition.unireg_check);
}


/** Prefix used by MySQL to indicate pre-5.1 table name encoding */
const char		srv_mysql50_table_name_prefix[10] = "#mysql50#";

/*
  This is a test case based on innobase_init()
  There was an out-of-bounds read when converting "-@" to a table name.
 */
TEST_F(SqlTableTest, FileNameToTableName)
{
  struct PackStuff
  {
    char foo1;
    char str[3];
    char foo2;
  };
  PackStuff foo;
  memcpy(foo.str, "-@", 3);
  MEM_NOACCESS(&foo.foo1, 1);
  MEM_NOACCESS(&foo.foo2, 1);

  const char test_filename[] = "-@";
  char       test_tablename[sizeof test_filename
                            + sizeof(srv_mysql50_table_name_prefix) - 1];

  // This one used to fail with AddressSanitizer
  uint name_length;
  name_length= filename_to_tablename(test_filename,
                                     test_tablename,
                                     sizeof(test_tablename)
#ifndef DBUG_OFF
                                     , true
#endif
                                     );
  EXPECT_EQ((sizeof(test_tablename)) - 1, name_length);

  // This one used to fail if compiled with -DHAVE_VALGRIND
  name_length= filename_to_tablename(foo.str,
                                     test_tablename,
                                     sizeof(test_tablename)
#ifndef DBUG_OFF
                                     , true
#endif
                                     );
  EXPECT_EQ((sizeof(test_tablename)) - 1, name_length);

}

}
