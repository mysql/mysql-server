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

struct st_relay_log_info;
typedef st_relay_log_info RELAY_LOG_INFO;

uint32
field_length_from_packed(enum_field_types field_type, byte const *data);

/**
  A table definition from the master.

  The responsibilities of this class is:
  - Extract and decode table definition data from the table map event
  - Check if table definition in table map is compatible with table
    definition on slave

  Currently, the only field type data available is an array of the
  type operators that are present in the table map event.

  @todo Add type operands to this structure to allow detection of
     difference between, e.g., BIT(5) and BIT(10).
 */

class table_def
{
public:
  /**
    Convenience declaration of the type of the field type data in a
    table map event.
  */
  typedef unsigned char field_type;

  /**
    Constructor.

    @param types Array of types
    @param size  Number of elements in array 'types'
   */
  table_def(field_type *types, my_size_t size)
    : m_type(new unsigned char [size]), m_size(size)
  {
    if (m_type)
      memcpy(m_type, types, size);
    else
      m_size= 0;
  }

  ~table_def() {
    if (m_type)
      delete [] m_type;
#ifndef DBUG_OFF
    m_type= 0;
    m_size= 0;
#endif
  }

  /**
    Return the number of fields there is type data for.

    @return The number of fields that there is type data for.
   */
  my_size_t size() const { return m_size; }


  /*
    Return a representation of the type data for one field.

    @param index Field index to return data for

    @return Will return a representation of the type data for field
    <code>index</code>. Currently, only the type identifier is
    returned.
   */
  field_type type(my_ptrdiff_t index) const
  {
    DBUG_ASSERT(0 <= index);
    DBUG_ASSERT(static_cast<my_size_t>(index) < m_size);
    return m_type[index];
  }

  /**
    Decide if the table definition is compatible with a table.

    Compare the definition with a table to see if it is compatible
    with it.

    A table definition is compatible with a table if:
      - the columns types of the table definition is a (not
        necessarily proper) prefix of the column type of the table, or
      - the other way around

    @param rli   Pointer to relay log info
    @param table Pointer to table to compare with.

    @retval 1  if the table definition is not compatible with @c table
    @retval 0  if the table definition is compatible with @c table
  */
  int compatible_with(RELAY_LOG_INFO const *rli, TABLE *table) const;

private:
  my_size_t m_size;           // Number of elements in the types array
  field_type *m_type;                     // Array of type descriptors
};

/**
   Extend the normal table list with a few new fields needed by the
   slave thread, but nowhere else.
 */
struct RPL_TABLE_LIST
  : public st_table_list
{
  bool m_tabledef_valid;
  table_def m_tabledef;
};

#endif /* RPL_UTILITY_H */
