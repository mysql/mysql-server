/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_SYSTEM_VIEWS__COLUMNS_INCLUDED
#define DD_SYSTEM_VIEWS__COLUMNS_INCLUDED

#include "sql/dd/impl/system_views/system_view_definition_impl.h"
#include "sql/dd/impl/system_views/system_view_impl.h"
#include "sql/dd/string_type.h"

namespace dd {
namespace system_views {

/*
  The class representing INFORMATION_SCHEMA.COLUMNS system view definition.
*/
class Columns : public System_view_impl<System_view_select_definition_impl>
{
public:
  enum enum_fields
  {
    FIELD_TABLE_CATALOG,
    FIELD_TABLE_SCHEMA,
    FIELD_TABLE_NAME,
    FIELD_COLUMN_NAME,
    FIELD_ORDINAL_POSITION,
    FIELD_COLUMN_DEFAULT,
    FIELD_IS_NULLABLE,
    FIELD_DATA_TYPE,
    FIELD_CHARACTER_MAXIMUM_LENGTH,
    FIELD_CHARACTER_OCTET_LENGTH,
    FIELD_NUMERIC_PRECISION,
    FIELD_NUMERIC_SCALE,
    FIELD_DATETIME_PRECISION,
    FIELD_CHARACTER_SET_NAME,
    FIELD_COLLATION_NAME,
    FIELD_COLUMN_TYPE,
    FIELD_COLUMN_KEY,
    FIELD_EXTRA,
    FIELD_PRIVILEGES,
    FIELD_COLUMN_COMMENT,
    FIELD_GENERATION_EXPRESSION,
    FIELD_SRS_ID
  };

  Columns();

  static const Columns &instance();

  static const String_type &view_name()
  {
    static String_type s_view_name("COLUMNS");
    return s_view_name;
  }

  virtual const String_type &name() const
  { return Columns::view_name(); }
};

}
}

#endif // DD_SYSTEM_VIEWS__COLUMNS_INCLUDED
