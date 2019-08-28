/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA  */

#ifndef FAKE_TABLE_H
#define FAKE_TABLE_H

#include "sql_class.h"

/*
  A fake class to make setting up a TABLE object a little easier.
*/
class Fake_TABLE_SHARE : public TABLE_SHARE
{
public:
  Fake_TABLE_SHARE(uint number_of_columns)
  {
    static const char *fakepath= "fakepath";
    fields= number_of_columns;
    // fix if you plan to test with >32 columns.
    column_bitmap_size= sizeof(int);
    tmp_table= NO_TMP_TABLE;
    db_low_byte_first= true;
    path.str= const_cast<char*>(fakepath);
    path.length= strlen(path.str);
  }
  ~Fake_TABLE_SHARE() {}
};

/*
  A fake class to make setting up a TABLE object a little easier. The
  table has a local fake table share.
*/
class Fake_TABLE: public TABLE
{
  Fake_TABLE_SHARE table_share;
  MY_BITMAP write_set_struct;
  uint32 write_set_buf;
  Field *field_array[32];

  void inititalize()
  {
    s= &table_share;
    file= NULL;
    in_use= current_thd;
    null_row= '\0';
    write_set= &write_set_struct;
    read_set= NULL;
    next_number_field= NULL; // No autoinc column
    pos_in_table_list= NULL;
    EXPECT_EQ(0, bitmap_init(write_set, &write_set_buf, s->fields, false));

    static const char *table_name= "Fake";
    for (uint i= 0; i < s->fields; ++i)
    {
      field[i]->table_name= &table_name;
      field[i]->table= this;
      field[i]->field_index= i;
    }
    const_table= true;
    maybe_null= 0;
  }

public:
  Fake_TABLE(Field *column1) : table_share(1)
  {
    field= field_array;
    field[0]= column1;
    inititalize();
  }

  Fake_TABLE(Field *column1, Field *column2) : table_share(2)
  {
    field= field_array;
    field[0]= column1;
    field[1]= column2;
    inititalize();
  }

  Fake_TABLE(Field *column1, Field *column2, Field *column3)
  : table_share(3)
  {
    field= field_array;
    field[0]= column1;
    field[1]= column2;
    field[2]= column3;
    inititalize();
  }

  ~Fake_TABLE()
  {
    /*
      This DTOR should be empty, since we inherit from TABLE,
      which cannot have virtual member functions.
    */ 
  }

  void set_handler(handler *h) { file= h; }
  TABLE_SHARE *get_share() { return &table_share; }
};

#endif // FAKE_TABLE_H
