/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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
#include <algorithm>

bool Key_part_spec::operator==(const Key_part_spec& other) const
{
  return length == other.length &&
         !my_strcasecmp(system_charset_info, field_name.str,
                        other.field_name.str);
}

/**
  Construct an (almost) deep copy of this key. Only those
  elements that are known to never change are not copied.
  If out of memory, a partial copy is returned and an error is set
  in THD.
*/

Key_spec::Key_spec(const Key_spec &rhs, MEM_ROOT *mem_root)
  :type(rhs.type),
  key_create_info(rhs.key_create_info),
  columns(rhs.columns, mem_root),
  name(rhs.name),
  generated(rhs.generated)
{
  list_copy_and_replace_each_value(columns, mem_root);
}


/**
  Construct an (almost) deep copy of this foreign key. Only those
  elements that are known to never change are not copied.
  If out of memory, a partial copy is returned and an error is set
  in THD.
*/

Foreign_key_spec::Foreign_key_spec(const Foreign_key_spec &rhs,
                                   MEM_ROOT *mem_root)
  :Key_spec(rhs, mem_root),
  ref_db(rhs.ref_db),
  ref_table(rhs.ref_table),
  ref_columns(rhs.ref_columns, mem_root),
  delete_opt(rhs.delete_opt),
  update_opt(rhs.update_opt),
  match_opt(rhs.match_opt)
{
  list_copy_and_replace_each_value(ref_columns, mem_root);
}

/*
  Test if a foreign key (= generated key) is a prefix of the given key
  (ignoring key name, key type and order of columns)

  NOTES:
    This is only used to test if an index for a FOREIGN KEY exists

  IMPLEMENTATION
    We only compare field names

  RETURN
    0	Generated key is a prefix of other key
    1	Not equal
*/

bool foreign_key_prefix(Key_spec *a, Key_spec *b)
{
  /* Ensure that 'a' is the generated key */
  if (a->generated)
  {
    if (b->generated && a->columns.elements > b->columns.elements)
      std::swap(a, b);                       // Put shorter key in 'a'
  }
  else
  {
    if (!b->generated)
      return TRUE;                              // No foreign key
    std::swap(a, b);                       // Put generated key in 'a'
  }

  /* Test if 'a' is a prefix of 'b' */
  if (a->columns.elements > b->columns.elements)
    return TRUE;                                // Can't be prefix

  List_iterator<Key_part_spec> col_it1(a->columns);
  List_iterator<Key_part_spec> col_it2(b->columns);
  const Key_part_spec *col1, *col2;

#ifdef ENABLE_WHEN_INNODB_CAN_HANDLE_SWAPED_FOREIGN_KEY_COLUMNS
  while ((col1= col_it1++))
  {
    bool found= 0;
    col_it2.rewind();
    while ((col2= col_it2++))
    {
      if (*col1 == *col2)
      {
        found= TRUE;
	break;
      }
    }
    if (!found)
      return TRUE;                              // Error
  }
  return FALSE;                                 // Is prefix
#else
  while ((col1= col_it1++))
  {
    col2= col_it2++;
    if (!(*col1 == *col2))
      return TRUE;
  }
  return FALSE;                                 // Is prefix
#endif
}

/**
  @brief  validate
    Check if the foreign key options are compatible with columns
    on which the FK is created.

  @param table_fields         List of columns 

  @return
    false   Key valid
  @return
    true   Key invalid
 */
bool Foreign_key_spec::validate(List<Create_field> &table_fields)
{
  Create_field  *sql_field;
  Key_part_spec *column;
  List_iterator<Key_part_spec> cols(columns);
  List_iterator<Create_field> it(table_fields);
  DBUG_ENTER("Foreign_key_spec::validate");
  while ((column= cols++))
  {
    it.rewind();
    while ((sql_field= it++) &&
           my_strcasecmp(system_charset_info,
                         column->field_name.str,
                         sql_field->field_name)) {}
    if (!sql_field)
    {
      my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name.str);
      DBUG_RETURN(TRUE);
    }
    if (type == KEYTYPE_FOREIGN && sql_field->gcol_info)
    {
      if (delete_opt == FK_OPTION_SET_NULL)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN, MYF(0), 
                 "ON DELETE SET NULL");
        DBUG_RETURN(TRUE);
      }
      if (update_opt == FK_OPTION_SET_NULL)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN, MYF(0), 
                 "ON UPDATE SET NULL");
        DBUG_RETURN(TRUE);
      }
      if (update_opt == FK_OPTION_CASCADE)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN, MYF(0), 
                 "ON UPDATE CASCADE");
        DBUG_RETURN(TRUE);
      }
    }
  }
  DBUG_RETURN(FALSE);
}


