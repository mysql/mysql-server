/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA  */

#ifndef FAKE_TABLE_H
#define FAKE_TABLE_H

#include "sql_class.h"
#include "handler-t.h"
#include <gmock/gmock.h>
#include "mock_field_long.h" // todo: put this #include first

using ::testing::NiceMock;
using std::vector;
using std::string;

static const uint MAX_TABLE_COLUMNS= sizeof(int) * 8;

/*
  A fake class for setting up TABLE_LIST object, required for table id mgmt.
*/
class Fake_TABLE_LIST : public TABLE_LIST
{
public:
  Fake_TABLE_LIST()
  {
  }
  ~Fake_TABLE_LIST()
  {
  }
};

/*
  A fake class to make setting up a TABLE object a little easier.
*/
class Fake_TABLE_SHARE : public TABLE_SHARE
{
  uint32 all_set_buf;

public:
  /**
    Creates a TABLE_SHARE with the requested number of columns

    @param  number_of_columns  The number of columns in the table
  */
  Fake_TABLE_SHARE(uint number_of_columns)
  {
    static const char *fakepath= "fakepath";
    fields= number_of_columns;
    db_create_options= 0;
    primary_key= 0;
    column_bitmap_size= sizeof(int);
    tmp_table= NO_TMP_TABLE;
    db_low_byte_first= true;
    path.str= const_cast<char*>(fakepath);
    path.length= strlen(path.str);

    EXPECT_EQ(0, bitmap_init(&all_set, &all_set_buf, fields, false));
    bitmap_set_above(&all_set, 0, 1);
  }
  ~Fake_TABLE_SHARE() {}
};

/*
  A fake class to make setting up a TABLE object a little easier. The
  table has a local fake table share.
*/
class Fake_TABLE: public TABLE
{
  // make room for 8 indexes (mysql permits 64)
  static const int max_keys= 8;
  KEY m_keys[max_keys];
  // make room for up to 8 keyparts per index
  KEY_PART_INFO m_key_part_infos[max_keys][8];

  Fake_TABLE_LIST  table_list;
  Fake_TABLE_SHARE table_share;
  // Storage space for the handler's handlerton
  Fake_handlerton fake_handlerton;
  MY_BITMAP write_set_struct;
  uint32 write_set_buf;
  MY_BITMAP read_set_struct;
  uint32 read_set_buf;
  Field *m_field_array[MAX_TABLE_COLUMNS];
  Mock_field_long *m_mock_field_array[MAX_TABLE_COLUMNS];

  // Counter for creating unique index id's. See create_index().
  int highest_index_id;

  // Counter for creating unique table id's. See initialize().
  static int highest_table_id;

  void initialize()
  {
    s= &table_share;
    in_use= current_thd;
    null_row= '\0';
    read_set= &read_set_struct;
    write_set= &write_set_struct;
    next_number_field= NULL; // No autoinc column
    pos_in_table_list= &table_list;
    table_list.table= this;
    EXPECT_EQ(0, bitmap_init(write_set, &write_set_buf, s->fields, false));
    EXPECT_EQ(0, bitmap_init(read_set, &read_set_buf, s->fields, false));

    const_table= false;
    table_list.set_tableno(highest_table_id);
    highest_table_id= (highest_table_id + 1) % MAX_TABLES;
    key_info= &m_keys[0];
    for (int i= 0; i < max_keys; i++)
      key_info[i].key_part= m_key_part_infos[i];
    // We choose non-zero to avoid it working by coincidence.
    highest_index_id= 3;

    set_handler(&mock_handler);
    mock_handler.change_table_ptr(this, &table_share);
    field= m_field_array;
  }

public:

  /**
    Unless you hand it anything else, this class will create
    Mock_field_long columns, and this is their pack_length.
  */
  static const int DEFAULT_PACK_LENGTH= Mock_field_long::PACK_LENGTH;
  NiceMock<Mock_HANDLER> mock_handler;

  Fake_TABLE(List<Field> fields) :
    table_share(fields.elements),
    mock_handler(static_cast<handlerton*>(NULL), &table_share)
  {
    field= m_field_array;

    List_iterator<Field> it(fields);
    int nbr_fields= 0;
    for (Field *cur_field= it++; cur_field; cur_field= it++)
      add(cur_field, nbr_fields++);

    initialize();
  }

  Fake_TABLE(Field *column) :
    table_share(1),
    mock_handler(static_cast<handlerton*>(NULL), &table_share)
  {
    initialize();
    add(column, 0);
  }

  Fake_TABLE(Field *column1, Field *column2) :
    table_share(2),
    mock_handler(static_cast<handlerton*>(NULL), &table_share)
  {
    initialize();
    add(column1, 0);
    add(column2, 1);
  }

  Fake_TABLE(Field *column1, Field *column2, Field *column3) :
    table_share(3),
    mock_handler(static_cast<handlerton*>(NULL), &table_share)
  {
    initialize();
    add(column1, 0);
    add(column2, 1);
    add(column3, 2);
  }


  /**
    Creates a table with the requested number of columns without
    creating indexes.

    @param  column_count     The number of columns in the table
    @param  cols_nullable    Whether or not columns are allowed to be NULL
  */
  Fake_TABLE(int column_count, bool cols_nullable) :
    table_share(column_count),
    mock_handler(&fake_handlerton, &table_share)
  {
    DBUG_ASSERT(static_cast<size_t>(column_count) <= sizeof(int) * 8);
    initialize();
    for (int i= 0; i < column_count; ++i)
    {
      std::stringstream str;
      str << "field_" << (i + 1);
      add(new Mock_field_long(str.str().c_str(), cols_nullable), i);
    }
  }


  /**
     Creates a one-column fake table and stores the value in the one field.

     @param column_value Item holding the integer value to be stored.
  */
  Fake_TABLE(Item_int *column_value) :
    table_share(1),
      mock_handler(&fake_handlerton, &table_share)
  {
    initialize();
    add(new Mock_field_long("field_1"), 0);
    column_value->save_in_field_no_warnings(field[0], true);
  }


  /**
     Creates a two-column fake table and stores the values in their
     corresponding fields.

     @param column1_value Item holding integer value to be stored.
     @param column2_value Item holding integer value to be stored.
  */
  Fake_TABLE(Item_int *column1_value, Item_int *column2_value) :
    table_share(2),
    mock_handler(static_cast<handlerton*>(NULL), &table_share)
  {
    field= m_field_array;
    field[0]= new Mock_field_long("field_1");
    field[0]->table= this;
    field[1]= new Mock_field_long("field_2");
    field[1]->table= this;
    initialize();
    column1_value->save_in_field_no_warnings(field[0], true);
    column2_value->save_in_field_no_warnings(field[1], true);
  }

  ~Fake_TABLE()
  {
    /*
      This DTOR should be empty, since we inherit from TABLE,
      which cannot have virtual member functions.
    */
  }

  // Defines an index over (column1, column2) and generates a unique id.
  int create_index(Field *column1, Field *column2) {
    column1->flags|= PART_KEY_FLAG;
    column2->flags|= PART_KEY_FLAG;
    int index_id= highest_index_id++;
    column1->key_start.set_bit(index_id);
    keys_in_use_for_query.set_bit(index_id);
    return index_id;
  }

  void set_handler(handler *h) { file= h; }
  TABLE_SHARE *get_share() { return &table_share; }

private:
  void add(Field *new_field, int pos)
  {
    field[pos]= new_field;
    new_field->table= this;
    static const char *table_name= "Fake";
    new_field->table_name= &table_name;
    new_field->field_index= pos;
    bitmap_set_bit(read_set, pos);
  }
};

// We choose non-zero to avoid it working by coincidence.
int Fake_TABLE::highest_table_id= 5;

#endif // FAKE_TABLE_H
