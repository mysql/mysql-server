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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "fake_table.h"
#include "mock_field_timestamp.h"
#include "test_utils.h"

namespace field_timestamp_unittests {


using my_testing::Server_initializer;
using my_testing::Mock_error_handler;


/*
  Tests of the public interface of Field_timestamp.
*/
class FieldTimestampTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  THD *get_thd() { return initializer.thd(); }

  Server_initializer initializer;
};


TEST_F(FieldTimestampTest, hasInsertDefaultFunction)
{
  {
    Mock_field_timestamp field_dn(Field::TIMESTAMP_DN_FIELD);
    EXPECT_TRUE(field_dn.has_insert_default_function());
  }
  {
    Mock_field_timestamp field_un(Field::TIMESTAMP_UN_FIELD);
    EXPECT_FALSE(field_un.has_insert_default_function());
  }
  {
    Mock_field_timestamp field_dnun(Field::TIMESTAMP_DNUN_FIELD);
    EXPECT_TRUE(field_dnun.has_insert_default_function());
  }
}


TEST_F(FieldTimestampTest, hasUpdateDefaultFunction)
{
  {
    Mock_field_timestamp field_dn(Field::TIMESTAMP_DN_FIELD);
    EXPECT_FALSE(field_dn.has_update_default_function());
  }
  {
    Mock_field_timestamp field_un(Field::TIMESTAMP_UN_FIELD);
    EXPECT_TRUE(field_un.has_update_default_function());
  }
  {
    Mock_field_timestamp field_dnun(Field::TIMESTAMP_DNUN_FIELD);
    EXPECT_TRUE(field_dnun.has_update_default_function());
  }
}


/*
  Test of DEFAULT CURRENT_TIMESTAMP functionality. Note that CURRENT_TIMESTAMP
  should be truncated to whole seconds.
*/
TEST_F(FieldTimestampTest, EvaluateInsertDefaultFunction)
{
  const timeval now= { 1, 1 };
  get_thd()->set_time(&now);

  {
    Mock_field_timestamp field_dn(Field::TIMESTAMP_DN_FIELD);
    field_dn.evaluate_insert_default_function();
    EXPECT_EQ(now.tv_sec,  field_dn.to_timeval().tv_sec);
    EXPECT_EQ(0, field_dn.to_timeval().tv_usec);
  }
  {
    Mock_field_timestamp field_un(Field::TIMESTAMP_UN_FIELD);
    field_un.evaluate_insert_default_function();
    EXPECT_EQ(0, field_un.to_timeval().tv_sec);
    EXPECT_EQ(0, field_un.to_timeval().tv_usec);
  }
  {
    Mock_field_timestamp field_dnun(Field::TIMESTAMP_DNUN_FIELD);
    field_dnun.evaluate_insert_default_function();
    EXPECT_EQ(now.tv_sec,  field_dnun.to_timeval().tv_sec);
    EXPECT_EQ(0, field_dnun.to_timeval().tv_usec);
  }
}


/*
  Test of ON UPDATE CURRENT_TIMESTAMP functionality. Note that
  CURRENT_TIMESTAMP should be truncated to whole seconds.
*/
TEST_F(FieldTimestampTest, EvaluateUpdateDefaultFunction)
{
  const timeval now= { 1, 1 };
  get_thd()->set_time(&now);

  {
    Mock_field_timestamp field_dn(Field::TIMESTAMP_DN_FIELD);
    field_dn.evaluate_update_default_function();
    EXPECT_EQ(0, field_dn.to_timeval().tv_sec);
    EXPECT_EQ(0, field_dn.to_timeval().tv_usec);
  }
  {
    Mock_field_timestamp field_un(Field::TIMESTAMP_UN_FIELD);
    field_un.evaluate_update_default_function();
    EXPECT_EQ(now.tv_sec,  field_un.to_timeval().tv_sec);
    EXPECT_EQ(0, field_un.to_timeval().tv_usec);
  }
  {
    Mock_field_timestamp field_dnun(Field::TIMESTAMP_DNUN_FIELD);
    field_dnun.evaluate_update_default_function();
    EXPECT_EQ(now.tv_sec,  field_dnun.to_timeval().tv_sec);
    EXPECT_EQ(0, field_dnun.to_timeval().tv_usec);
  }
}

}
