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

#include "mock_field_timestamp.h"
#include "fake_table.h"
#include "test_utils.h"
#include "sql_data_change.h"

namespace {

using my_testing::Server_initializer;
using my_testing::Mock_error_handler;

/*
  Tests for the functionality of the COPY_INFO class. We test all public
  interfaces, and some of the protected parts:

  - COPY_INFO::get_function_default_columns, and
  - COPY_INFO::get_cached_bitmap
*/
class CopyInfoTest : public ::testing::Test
{
protected:
  virtual void SetUp() { initializer.SetUp(); }
  virtual void TearDown() { initializer.TearDown(); }

  Server_initializer initializer;
};


/**
  This is a simple mock Field class, which verifies that store_timestamp is
  called, depending on default and on update clauses, and whether the field is
  explicitly assigned a value. We inherit Field_long, but the data type does
  not matter.

  TODO: Introduce Google Mock to simplify writing of mock classes.
*/
class Mock_field : public Field_long
{
  uchar null_byte;
  bool store_timestamp_called;
  bool is_on_the_assigned_list;
public:

  Mock_field(utype unireg) :
    Field_long(NULL, 0, &null_byte, 0, unireg, "", false,  false),
    store_timestamp_called(false),
    is_on_the_assigned_list(false)
  {}

  void store_timestamp(const timeval*) { store_timestamp_called= true; }

  /*
    Informs the Mock_field that it appears in the list after INSERT INTO
    <table>.
  */
  void notify_added_to_assign_list() { is_on_the_assigned_list= true; }

  ~Mock_field() {
    if (!is_on_the_assigned_list && has_update_default_function())
      EXPECT_TRUE(store_timestamp_called);
    if ( is_on_the_assigned_list && has_update_default_function())
      EXPECT_FALSE(store_timestamp_called);
  }

};


/**
  This is a simple mock Item_field class, whose only raison d'etre is to pass
  on the call notify_added_to_assign_list() to its Mock_item_field.

  TODO: Introduce Google Mock to simplify writing of mock classes.
*/
class Mock_item_field : public Item_field
{
  Mock_field *mf;
public:
  Mock_item_field(Mock_field *field) : Item_field(field), mf(field) {}

  void notify_added_to_assign_list() { mf->notify_added_to_assign_list(); }
};


/*
  Compares two COPY_INFO::Statistics and makes sure they are equal.
*/
void check_equality(const COPY_INFO::Statistics a,
                    const COPY_INFO::Statistics b)
{
  EXPECT_EQ(a.records, b.records);
  EXPECT_EQ(a.deleted, b.deleted);
  EXPECT_EQ(a.updated, b.updated);
  EXPECT_EQ(a.copied,  b.copied);
  EXPECT_EQ(a.error_count, b.error_count);
  EXPECT_EQ(a.touched, b.touched);
}


class Mock_COPY_INFO: public COPY_INFO
{
public:

  /*
    Pass-through constructor.
  */
  Mock_COPY_INFO(operation_type optype,
                 List<Item> *inserted_columns,
                 enum_duplicates duplicate_handling,
                 bool ignore_errors)
    : COPY_INFO(optype,
                inserted_columns,
                true, // manage_defaults
                duplicate_handling,
                ignore_errors)
  {}

  /*
    Intelligent constructor that knows about the Mock_item_field
    class. Notifies the Mock_item_field's that they are on the list of
    inserted columns.
  */
  Mock_COPY_INFO(operation_type optype,
                 List<Mock_item_field> *inserted_columns,
                 enum_duplicates duplicate_handling,
                 bool ignore_errors)
    : COPY_INFO(optype,
                mock_item_field_list_to_item_list(inserted_columns),
                true, // manage_defaults
                duplicate_handling,
                ignore_errors)
  {}

  Mock_COPY_INFO(operation_type optype, List<Item> *fields, List<Item> *values)
    : COPY_INFO(optype, fields, values)
  {}

  /*
    We import these protected functions into the public namespace as we need
    to test them.
  */
  using COPY_INFO::get_function_default_columns;
  using COPY_INFO::get_cached_bitmap;

  virtual ~Mock_COPY_INFO() {}

private:
  List<Item> *mock_item_field_list_to_item_list(List<Mock_item_field> *columns)
  {
    List<Item> *items= new List<Item>;
    EXPECT_FALSE(items == NULL) << "Out of memory";

    List_iterator<Mock_item_field> iterator(*columns);
    Mock_item_field *assigned_column;
    while ((assigned_column= iterator++) != NULL)
    {
      assigned_column->notify_added_to_assign_list();
      items->push_back(assigned_column);
    }
    return items;
  }

};


/*
  Convenience class for creating a Mock_COPY_INFO to represent an insert
  operation.
*/
class Mock_COPY_INFO_insert : public Mock_COPY_INFO
{
public:
  Mock_COPY_INFO_insert() :
    Mock_COPY_INFO(COPY_INFO::INSERT_OPERATION, static_cast<List<Item>*>(NULL),
                   DUP_UPDATE, false)
  {}
  Mock_COPY_INFO_insert(List<Item> *fields) :
    Mock_COPY_INFO(COPY_INFO::INSERT_OPERATION, fields,
                   DUP_UPDATE, false)
  {}
};


/*
  Convenience class for creating a Mock_COPY_INFO to represent an update
  operation.
*/
class Mock_COPY_INFO_update : public Mock_COPY_INFO
{
public:
  Mock_COPY_INFO_update()
    :Mock_COPY_INFO(COPY_INFO::UPDATE_OPERATION, NULL, NULL)
  {}
};


/*
  Tests that constuctors initialize the stats object properly.
*/
TEST_F(CopyInfoTest, constructors)
{
  List<Item> inserted_columns;

  COPY_INFO insert(COPY_INFO::INSERT_OPERATION,
                   &inserted_columns,
                   true, // manage_defaults
                   DUP_UPDATE,
                   true);

  EXPECT_EQ(0U, insert.stats.records);
  EXPECT_EQ(0U, insert.stats.deleted);
  EXPECT_EQ(0U, insert.stats.updated);
  EXPECT_EQ(0U, insert.stats.copied);
  EXPECT_EQ(0U, insert.stats.error_count);
  EXPECT_EQ(0U, insert.stats.touched);

  List<Item> columns;
  List<Item> values;
  COPY_INFO update(COPY_INFO::UPDATE_OPERATION, &columns, &values);

  EXPECT_EQ(0U, update.stats.records);
  EXPECT_EQ(0U, update.stats.deleted);
  EXPECT_EQ(0U, update.stats.updated);
  EXPECT_EQ(0U, update.stats.copied);
  EXPECT_EQ(0U, update.stats.error_count);
  EXPECT_EQ(0U, update.stats.touched);

}


/*
  Tests the accessors when the COPY_INFO represents an insert operation.
*/
TEST_F(CopyInfoTest, insertAccessors)
{
  List<Item> inserted_columns;

  COPY_INFO insert(COPY_INFO::INSERT_OPERATION,
                   &inserted_columns,
                   true, // manage_defaults
                   DUP_REPLACE,
                   true);

  EXPECT_EQ(COPY_INFO::INSERT_OPERATION, insert.get_operation_type());
  EXPECT_EQ(&inserted_columns, insert.get_changed_columns());
  EXPECT_EQ(static_cast<List<Item>*>(NULL), insert.get_changed_columns2());
  EXPECT_TRUE(insert.get_manage_defaults());
  EXPECT_EQ(DUP_REPLACE, insert.get_duplicate_handling());
  EXPECT_TRUE(insert.get_ignore_errors());
}


/*
  Tests the accessors when the COPY_INFO represents a load data infile
  operation.
*/
TEST_F(CopyInfoTest, loadDataAccessors)
{
  List<Item> inserted_columns;
  List<Item> inserted_columns2;

  COPY_INFO load_data(COPY_INFO::INSERT_OPERATION,
                      &inserted_columns,
                      &inserted_columns2,
                      true, // manage_defaults
                      DUP_UPDATE,
                      true, // ignore_duplicates
                      123);

  EXPECT_EQ(COPY_INFO::INSERT_OPERATION, load_data.get_operation_type());
  EXPECT_EQ(&inserted_columns, load_data.get_changed_columns());
  EXPECT_EQ(&inserted_columns2, load_data.get_changed_columns2());
  EXPECT_TRUE(load_data.get_manage_defaults());
  EXPECT_EQ(DUP_UPDATE, load_data.get_duplicate_handling());
  EXPECT_TRUE(load_data.get_ignore_errors());
}


/*
  Tests the accessors when the COPY_INFO represents an update operation.
*/
TEST_F(CopyInfoTest, updateAccessors)
{
  List<Item> columns;
  List<Item> values;

  COPY_INFO update(COPY_INFO::UPDATE_OPERATION, &columns, &values);

  EXPECT_EQ(COPY_INFO::UPDATE_OPERATION, update.get_operation_type());
  EXPECT_EQ(&columns, update.get_changed_columns());
  EXPECT_EQ(static_cast<List<Item>*>(NULL), update.get_changed_columns2());
  EXPECT_TRUE(update.get_manage_defaults());
  EXPECT_EQ(DUP_ERROR, update.get_duplicate_handling());
  EXPECT_FALSE(update.get_ignore_errors());
}


Field_long make_field()
{
  static uchar unused_null_byte;

  Field_long a(NULL,
               0,
               &unused_null_byte,
               0,
               Field::TIMESTAMP_DN_FIELD,
               "a",
               false,
               false);
  return a;
}


/*
  Test of the lazy instantiation performed by get_function_default_columns().

  - The bitmap pointer is initially NULL.

  - That calling get_function_default_columns() indeed points the member to a
    lazily instantiated bitmap.

  - That on a second call to get_function_default_columns(), a new bitmap is
    not allocated.

    We repeat the test for insert and update operations.
*/
TEST_F(CopyInfoTest, getFunctionDefaultColumns)
{
  Mock_COPY_INFO_insert insert;
  Mock_COPY_INFO_update update;

  Field_long a= make_field();
  Fake_TABLE table(&a);

  MY_BITMAP *initial_value= NULL;

  EXPECT_EQ(initial_value, insert.get_cached_bitmap());

  insert.get_function_default_columns(&table);
  EXPECT_NE(initial_value, insert.get_cached_bitmap())
    << "The output parameter must be set!";

  const MY_BITMAP *function_default_columns= insert.get_cached_bitmap();
  insert.get_function_default_columns(&table);
  EXPECT_EQ(function_default_columns, insert.get_cached_bitmap())
    << "Not supposed to allocate a new bitmap on second call.";

  EXPECT_EQ(initial_value, update.get_cached_bitmap());
  update.get_function_default_columns(&table);
  EXPECT_NE(initial_value, update.get_cached_bitmap())
    << "The output parameter must be set!";

  function_default_columns= update.get_cached_bitmap();
  update.get_function_default_columns(&table);
  EXPECT_EQ(function_default_columns, update.get_cached_bitmap())
    << "Not supposed to allocate a new bitmap on second call.";
}


/*
  Here we test that calling COPY_INFO::set_function_defaults() indeed causes
  store_timestamp to be called on the columns that are not on the list of
  assigned_columns. We seize the opportunity to test
  COPY_INFO::function_defaults_apply() since we have to call it anyways in
  order for set_function_defaults() not to assert.
*/
TEST_F(CopyInfoTest, setFunctionDefaults)
{
  Mock_field a(Field::TIMESTAMP_UN_FIELD);
  Mock_field b(Field::TIMESTAMP_DNUN_FIELD);

  EXPECT_TRUE(a.has_update_default_function());
  EXPECT_TRUE(b.has_update_default_function());

  Fake_TABLE table(&a, &b);

  List<Mock_item_field> assigned_columns;
  assigned_columns.push_front(new Mock_item_field(&a));

  Mock_COPY_INFO insert(COPY_INFO::INSERT_OPERATION,
                        &assigned_columns,
                        DUP_ERROR,
                        true);

  ASSERT_FALSE(insert.get_function_default_columns(&table)) << "Out of memory";

  insert.add_function_default_columns(&table, table.write_set);
  EXPECT_FALSE(bitmap_is_set(table.write_set, 0));
  EXPECT_TRUE (bitmap_is_set(table.write_set, 1));

  EXPECT_TRUE(insert.function_defaults_apply(&table)) << "They do apply";

  insert.set_function_defaults(&table);
}

}
