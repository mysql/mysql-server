/* Copyright (c) 2000, 2017, Alibaba and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file

  Implementation of SEQUENCE NEXTVAL() AND CURRVAL() function.

  Usage Like:
    'SELECT NEXTVAL(s1)'
    'SELECT CURRVAL(s1)'
*/

#include "sql/item_sequence_func.h"
#include "sql/sql_sequence.h"

/**
  @addtogroup Sequence Engine

  Sequence Engine native function implementation.

  @{
*/

/**
  NEXTVAL() function implementation.
*/
longlong Item_func_nextval::val_int() {
  ulonglong value;
  int error;
  TABLE *table = table_list->table;
  DBUG_ENTER("Item_func_nextval::val_int");
  DBUG_ASSERT(table->file);

  bitmap_set_bit(table->read_set, Sequence_field::FIELD_NUM_NEXTVAL);

  if (table->file->ha_rnd_init(1))
    goto err;
  else {
    if ((error = table->file->ha_rnd_next(table->record[0]))) {
      table->file->print_error(error, MYF(0));
      table->file->ha_rnd_end();
      goto err;
    }
    table->file->ha_rnd_end();

    value = table->field[Sequence_field::FIELD_NUM_NEXTVAL]->val_int();
    null_value = 0;
    DBUG_RETURN(value);
  }
err:
  null_value = 1;
  DBUG_RETURN(0);
}

/**
  CURRVAL() function implementation.
*/
longlong Item_func_currval::val_int() {
  ulonglong value;
  int error;
  TABLE *table = table_list->table;
  DBUG_ENTER("Item_func_currval::val_int");
  DBUG_ASSERT(table->file);

  bitmap_set_bit(table->read_set, Sequence_field::FIELD_NUM_CURRVAL);

  if (table->file->ha_rnd_init(1))
    goto err;
  else {
    if ((error = table->file->ha_rnd_next(table->record[0]))) {
      table->file->print_error(error, MYF(0));
      table->file->ha_rnd_end();
      goto err;
    }
    table->file->ha_rnd_end();

    value = table->field[Sequence_field::FIELD_NUM_CURRVAL]->val_int();
    null_value = 0;
    DBUG_RETURN(value);
  }
err:
  null_value = 1;
  DBUG_RETURN(0);
}


/// @} (end of group Sequence Engine)

