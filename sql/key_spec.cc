/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "key_spec.h"

#include "derror.h"      // ER_THD
#include "field.h"       // Create_field
#include "mysqld.h"      // system_charset_info

#include <algorithm>

KEY_CREATE_INFO default_key_create_info=
  { HA_KEY_ALG_SE_SPECIFIC, false, 0, {NullS, 0}, {NullS, 0} };

bool Key_part_spec::operator==(const Key_part_spec& other) const
{
  return length == other.length &&
         !my_strcasecmp(system_charset_info, field_name.str,
                        other.field_name.str);
}


bool foreign_key_prefix(const Key_spec *a, const Key_spec *b)
{
  /* Ensure that 'a' is the generated key */
  if (a->generated)
  {
    if (b->generated && a->columns.size() > b->columns.size())
      std::swap(a, b);                       // Put shorter key in 'a'
  }
  else
  {
    if (!b->generated)
      return true;                              // No foreign key
    std::swap(a, b);                       // Put generated key in 'a'
  }

  /* Test if 'a' is a prefix of 'b' */
  if (a->columns.size() > b->columns.size())
    return true;                                // Can't be prefix

#ifdef ENABLE_WHEN_INNODB_CAN_HANDLE_SWAPED_FOREIGN_KEY_COLUMNS
  while ((col1= col_it1++))
  {
    bool found= false;
    col_it2.rewind();
    while ((col2= col_it2++))
    {
      if (*col1 == *col2)
      {
        found= true;
	break;
      }
    }
    if (!found)
      return true;                              // Error
  }
  return false;                                 // Is prefix
#else
  for (size_t i= 0; i < a->columns.size(); i++)
  {
    if (!(*(a->columns[i]) == (*b->columns[i])))
      return true;
  }
  return false;                                 // Is prefix
#endif
}


bool Foreign_key_spec::validate(THD *thd, List<Create_field> &table_fields) const
{
  Create_field  *sql_field;
  List_iterator<Create_field> it(table_fields);
  DBUG_ENTER("Foreign_key_spec::validate");
  if (ref_columns.size() > 0 && ref_columns.size() != columns.size())
  {
    my_error(ER_WRONG_FK_DEF, MYF(0),
             (name.str ? name.str : "foreign key without name"),
             ER_THD(thd, ER_KEY_REF_DO_NOT_MATCH_TABLE_REF));
    DBUG_RETURN(true);
  }
  for (const Key_part_spec *column : columns)
  {
    it.rewind();
    while ((sql_field= it++) &&
           my_strcasecmp(system_charset_info,
                         column->field_name.str,
                         sql_field->field_name)) {}
    if (!sql_field)
    {
      my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name.str);
      DBUG_RETURN(true);
    }
    if (sql_field->gcol_info)
    {
      if (delete_opt == FK_OPTION_SET_NULL)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN, MYF(0),
                 "ON DELETE SET NULL");
        DBUG_RETURN(true);
      }
      if (update_opt == FK_OPTION_SET_NULL)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN, MYF(0),
                 "ON UPDATE SET NULL");
        DBUG_RETURN(true);
      }
      if (update_opt == FK_OPTION_CASCADE)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN, MYF(0),
                 "ON UPDATE CASCADE");
        DBUG_RETURN(true);
      }
    }
  }
  DBUG_RETURN(false);
}


