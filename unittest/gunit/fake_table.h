/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

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

/*
  A fake class to make setting up a TABLE object a little easier.
*/
class Fake_TABLE_SHARE : public TABLE_SHARE
{
public:
  Fake_TABLE_SHARE(uint number_of_columns)
  {
    fields= number_of_columns;
    // fix if you plan to test with >32 columns.
    column_bitmap_size= sizeof(int);
    tmp_table= NO_TMP_TABLE;
  }
};

/*
  A fake class to make setting up a TABLE object a little easier. The
  table has a local fake table share.
*/
class Fake_TABLE: public TABLE
{
  Fake_TABLE_SHARE table_share;
  MY_BITMAP write_set_struct;

  void inititalize()
  {
    s= &table_share;
    in_use= current_thd;
    null_row= '\0';
    write_set= &write_set_struct;
    read_set= NULL;

    EXPECT_EQ(0, bitmap_init(write_set, NULL, s->fields, false))
      << "Out of memory";

    static const char *table_name= "Fake";
    for (uint i= 0; i < s->fields; ++i)
    {
      field[i]->table_name= &table_name;
      field[i]->table= this;
      field[i]->field_index= i;
    }
    const_table= true;
  }

public:
  Fake_TABLE(Field *column1) : table_share(1)
  {
    field= new Field*[1];
    EXPECT_FALSE(field == NULL) << "Out of memory";
    field[0]= column1;
    inititalize();
  }

  Fake_TABLE(Field *column1, Field *column2) : table_share(2)
  {
    field= new Field*[2];
    EXPECT_FALSE(field == NULL) << "Out of memory";
    field[0]= column1;
    field[1]= column2;
    inititalize();
  }

  ~Fake_TABLE()
  {
    bitmap_free(write_set);
    delete[] field;
  }
};

#endif // FAKE_TABLE_H
