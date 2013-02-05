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

#include "test_utils.h"
#include "mock_create_field.h"


namespace create_field_unittest {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

class CreateFieldTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  Server_initializer initializer;
};


TEST_F(CreateFieldTest, init)
{
  // To do: Add all possible precisions.
  Item_func_now_local *now= new Item_func_now_local(0);

  Mock_create_field field_definition_none(MYSQL_TYPE_TIMESTAMP, NULL, NULL);
  EXPECT_EQ(Field::NONE, field_definition_none.unireg_check);

  Mock_create_field field_definition_dn(MYSQL_TYPE_TIMESTAMP, now, NULL);
  EXPECT_EQ(Field::TIMESTAMP_DN_FIELD, field_definition_dn.unireg_check);

  Mock_create_field field_definition_dnun(MYSQL_TYPE_TIMESTAMP, now, now);
  EXPECT_EQ(Field::TIMESTAMP_DNUN_FIELD, field_definition_dnun.unireg_check);

  Mock_create_field field_definition_un(MYSQL_TYPE_TIMESTAMP, NULL, now);
  EXPECT_EQ(Field::TIMESTAMP_UN_FIELD, field_definition_un.unireg_check);
}

}
