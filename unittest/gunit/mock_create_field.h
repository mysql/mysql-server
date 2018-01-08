/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MOCK_CREATE_FIELD_H
#define MOCK_CREATE_FIELD_H

#include "sql/field.h"
#include "my_dbug.h"

class Mock_create_field : public Create_field
{
  LEX_STRING m_lex_string;
public:
  Mock_create_field(enum_field_types field_type,
                    Item* insert_default, Item* update_default)
  {
    /*
      Only TIMESTAMP is implemented for now.
      Other types would need different parameters (fld_length, etc).
    */
    DBUG_ASSERT(field_type == MYSQL_TYPE_TIMESTAMP ||
                field_type == MYSQL_TYPE_TIMESTAMP2);
    init(NULL, // THD *thd
         NULL, // char *fld_name
         field_type,
         NULL, // char *fld_length
         NULL, // char *fld_decimals,
         0, // uint fld_type_modifier
         insert_default, // Item *fld_default_value,
         update_default, // Item *fld_on_update_value,
         /*
            Pointer can't be NULL, or Create_field::init() will
            core dump. This is undocumented, of
            course. </sarcasm>
         */
         &m_lex_string, // LEX_STRING *fld_comment,
         NULL, // char *fld_change,
         NULL, // List<String> *fld_interval_list,
         NULL, // const CHARSET_INFO *fld_charset,
         0, // uint fld_geom_type
         nullptr, // gcol info
         {} // Nullable<gis::srid_t> srid
         );
  }
};

#endif // MOCK_CREATE_FIELD_H
