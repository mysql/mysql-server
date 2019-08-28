/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "my_config.h"
#include <gtest/gtest.h>
#include "delayable_insert_operation.h"

namespace
{

class Mock_delayable: public Delayable_insert_operation
{
public:
  Mock_delayable() : Delayable_insert_operation() {}

  using COPY_INFO::get_cached_bitmap;
};


TEST(DelayableInsertOperation, SetDupAndIgnore)
{
  enum_duplicates duplicate_handling= DUP_REPLACE;
  bool ignore_errors= true;

  Mock_delayable delayed_insert;

  delayed_insert.set_dup_and_ignore(duplicate_handling, ignore_errors);
  EXPECT_EQ(duplicate_handling, delayed_insert.get_duplicate_handling());
  EXPECT_EQ(ignore_errors, delayed_insert.get_ignore_errors());
}


/*
  Test that Delayable_insert_operation does not touch its bitmap during
  invocation of set_function_defaults(TABLE*).
 */
TEST(DelayableInsertOperation, SetFunctionDefaults)
{
  /* 
    The operation should not use the table, so we give the table some invalid
    address, which will crash if used. This value is used to defeat any test
    for NULL.
  */
  TABLE *table= reinterpret_cast<TABLE*>(0xffffffff);

  Mock_delayable delayed_insert;

  MY_BITMAP *initial_value= NULL;
  EXPECT_EQ(initial_value, delayed_insert.get_cached_bitmap());
  delayed_insert.set_function_defaults(table);
  EXPECT_EQ(initial_value, delayed_insert.get_cached_bitmap())
    << "Not supposed to allocate anything";
}

}
