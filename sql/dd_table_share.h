#ifndef DD_TABLE_SHARE_INCLUDED
#define DD_TABLE_SHARE_INCLUDED
/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <sys/types.h>

#include "binary_log_types.h"        // enum_field_types
#include "dd/object_id.h"            // dd::Object_id
#include "m_ctype.h"
#include "my_inttypes.h"
#include "my_sys.h"                  // get_charset

class THD;
struct TABLE_SHARE;

typedef struct charset_info_st CHARSET_INFO;
namespace dd {
  class Table;
  enum class enum_column_types;
}


/**
  Read the table definition from the data-dictionary.

  @param thd        Thread handler
  @param share      Fill this with table definition
  @param open_view  Allow open of view
  @param table_def  If not NULL: a data-dictionary Table-object describing
                    table to be used for opening, instead of reading
                    information from DD. If NULL, a new dd::Table-object
                    will be constructed and read from the Data Dictionary.

  @note
    This function is called when the table definition is not cached in
    table_def_cache.
    The data is returned in 'share', which is alloced by
    alloc_table_share().. The code assumes that share is initialized.

  @returns
   false   OK
   true    Error
*/
bool open_table_def(THD *thd, TABLE_SHARE *share, bool open_view,
                    const dd::Table *table_def);


/* Map from new to old field type. */
enum_field_types dd_get_old_field_type(dd::enum_column_types type);

static inline CHARSET_INFO *dd_get_mysql_charset(dd::Object_id dd_cs_id)
{
  return get_charset(static_cast<uint> (dd_cs_id), MYF(0));
}

class Field;
class KEY_PART_INFO;

/*
  Check if the given key_part is suitable to be promoted as part of
  primary key.

  @param key_part    - pointer to KEY_PART_INTO which we are checking.
  @param table_field - Pointer to Field of column used by key_part.

  @returns
   true  - Is suitable for primary key.
   false - if not.
*/
bool is_suitable_for_primary_key(KEY_PART_INFO *key_part,
                                 Field *table_field);

#endif // DD_TABLE_SHARE_INCLUDED
