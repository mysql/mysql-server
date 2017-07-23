/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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


/**
  @file sql_data_change.cc

  Contains classes representing SQL-data change statements. Currently
  the only data change functionality implemented here is function
  defaults.
*/

#include "sql_data_change.h"

#include "sql_class.h"  // THD
#include "table.h"      // TABLE

/**
   Allocates and initializes a MY_BITMAP bitmap, containing one bit per column
   in the table. The table THD's MEM_ROOT is used to allocate memory.

   @param      table   The table whose columns should be used as a template
                       for the bitmap.
   @param[out] bitmap  A pointer to the allocated bitmap.

   @retval false Success.
   @retval true Memory allocation error.
*/
static bool allocate_column_bitmap(TABLE *table, MY_BITMAP **bitmap)
{
  DBUG_ENTER("allocate_column_bitmap");
  const uint number_bits= table->s->fields;
  MY_BITMAP *the_struct;
  my_bitmap_map *the_bits;

  DBUG_ASSERT(current_thd == table->in_use);
  if (multi_alloc_root(table->in_use->mem_root,
                       &the_struct, sizeof(MY_BITMAP),
                       &the_bits, bitmap_buffer_size(number_bits),
                       NULL) == NULL)
    DBUG_RETURN(true);

  if (bitmap_init(the_struct, the_bits, number_bits, FALSE) != 0)
    DBUG_RETURN(true);

  *bitmap= the_struct;

  DBUG_RETURN(false);
}


bool COPY_INFO::get_function_default_columns(TABLE *table)
{
  DBUG_ENTER("COPY_INFO::get_function_default_columns");

  if (m_function_default_columns != NULL)
    DBUG_RETURN(false);

  if (allocate_column_bitmap(table, &m_function_default_columns))
    DBUG_RETURN(true);

  if (!m_manage_defaults)
    DBUG_RETURN(false); // leave bitmap full of zeroes

  /*
    Find columns with function default on insert or update, mark them in
    bitmap.
  */
  for (uint i= 0; i < table->s->fields; ++i)
  {
    Field *f= table->field[i];
    if ((m_optype == INSERT_OPERATION && f->has_insert_default_function()) ||
        (m_optype == UPDATE_OPERATION && f->has_update_default_function()))
      bitmap_set_bit(m_function_default_columns, f->field_index);
  }

  if (bitmap_is_clear_all(m_function_default_columns))
    DBUG_RETURN(false); // no bit set, next step unneeded

  /*
    Remove explicitly assigned columns from the bitmap. The assignment
    target (lvalue) may not always be a column (Item_field), e.g. we could
    be inserting into a view, whose column is actually a base table's column
    converted with COLLATE: the lvalue would then be an
    Item_func_set_collation.
    If the lvalue is an expression tree, we clear all columns in it from the
    bitmap.
  */
  List<Item> *all_changed_columns[2]=
    { m_changed_columns, m_changed_columns2 };
  for (uint i= 0; i < 2; i++)
  {
    if (all_changed_columns[i] != NULL)
    {
      List_iterator<Item> lvalue_it(*all_changed_columns[i]);
      Item *lvalue_item;
      while ((lvalue_item= lvalue_it++) != NULL)
        lvalue_item->walk(&Item::remove_column_from_bitmap,
                      Item::enum_walk(Item::WALK_POSTFIX | Item::WALK_SUBQUERY),
                      reinterpret_cast<uchar*>(m_function_default_columns));
    }
  }

  DBUG_RETURN(false);
}


void COPY_INFO::set_function_defaults(TABLE *table)
{
  DBUG_ENTER("COPY_INFO::set_function_defaults");

  DBUG_ASSERT(m_function_default_columns != NULL);

  /* Quick reject test for checking the case when no defaults are invoked. */
  if (bitmap_is_clear_all(m_function_default_columns))
    DBUG_VOID_RETURN;

  for (uint i= 0; i < table->s->fields; ++i)
    if (bitmap_is_set(m_function_default_columns, i))
    {
      DBUG_ASSERT(bitmap_is_set(table->write_set, i));
      switch (m_optype)
      {
      case INSERT_OPERATION:
        table->field[i]->evaluate_insert_default_function();
        break;
      case UPDATE_OPERATION:
        table->field[i]->evaluate_update_default_function();
        break;
      }
    }

  /**
    @todo combine this call to update_generated_write_fields() with the
    one in fill_record() to avoid updating virtual generated fields twice.
    blobs_need_not_keep_old_value() is called to unset the m_keep_old_value
    flag. Allowing this flag to remain might interfere with the way the old
    BLOB value is handled. When update_generated_write_fields() is removed,
    blobs_need_not_keep_old_value() can also be removed.
  */
  if (table->has_gcol())
  {
    table->blobs_need_not_keep_old_value();
    update_generated_write_fields(table->write_set, table);
  }

  DBUG_VOID_RETURN;
}


bool COPY_INFO::ignore_last_columns(TABLE *table, uint count)
{
  if (get_function_default_columns(table))
    return true;
  for (uint i= 0; i < count; i++)
    bitmap_clear_bit(m_function_default_columns,
                     table->s->fields - 1 - i);
  return false;
}
