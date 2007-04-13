/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef RPL_UTILITY_H
#define RPL_UTILITY_H

#ifndef __cplusplus
#error "Don't include this C++ header file from a non-C++ file!"
#endif

#include "mysql_priv.h"

uint32
field_length_from_packed(enum_field_types const field_type, 
                         byte const *const data);

/*
  A table definition from the master.

  RESPONSIBILITIES

  - Extract and decode table definition data from the table map event
  - Check if table definition in table map is compatible with table
    definition on slave

  DESCRIPTION

    Currently, the only field type data available is an array of the
    type operators that are present in the table map event.

  TODO

    Add type operands to this structure to allow detection of
    difference between, e.g., BIT(5) and BIT(10).
 */

class table_def
{
public:
  /*
    Convenience declaration of the type of the field type data in a
    table map event.
  */
  typedef unsigned char field_type;

  /*
    Constructor.

    SYNOPSIS
      table_def()
      types Array of types
      size  Number of elements in array 'types'
   */
  table_def(field_type *types, my_size_t size)
    : m_size(size), m_type(types)
  {
  }

  /*
    Return the number of fields there is type data for.

    SYNOPSIS
      size()

    RETURN VALUE
      The number of fields that there is type data for.
   */
  my_size_t size() const { return m_size; }

  /*
    Return a representation of the type data for one field.

    SYNOPSIS
      type()
      i   Field index to return data for

    RETURN VALUE

      Will return a representation of the type data for field
      'i'. Currently, only the type identifier is returned.
   */
  field_type type(my_ptrdiff_t i) const { return m_type[i]; }

  /*
    Decide if the table definition is compatible with a table.

    SYNOPSIS
      compatible_with()
      rli   Pointer to relay log info
      table Pointer to table to compare with.

    DESCRIPTION

      Compare the definition with a table to see if it is compatible
      with it.  A table definition is compatible with a table if:

      - the columns types of the table definition is a (not
        necessarily proper) prefix of the column type of the table, or

      - the other way around

    RETURN VALUE
      1  if the table definition is not compatible with 'table'
      0  if the table definition is compatible with 'table'
  */
  int compatible_with(RELAY_LOG_INFO *rli, TABLE *table) const;

private:
  my_size_t m_size;           // Number of elements in the types array
  field_type *m_type;                     // Array of type descriptors
};

#endif /* RPL_UTILITY_H */
