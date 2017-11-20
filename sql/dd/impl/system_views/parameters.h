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

#ifndef DD_SYSTEM_VIEWS__PARAMETERS_INCLUDED
#define DD_SYSTEM_VIEWS__PARAMETERS_INCLUDED

#include "sql/dd/impl/system_views/system_view_definition_impl.h"
#include "sql/dd/impl/system_views/system_view_impl.h"
#include "sql/dd/string_type.h"

namespace dd {
namespace system_views {

/*
  The class representing INFORMATION_SCHEMA.PARAMETERS system view definition.
*/
class Parameters : public System_view_impl<System_view_select_definition_impl>
{
public:
  enum enum_fields
  {
    FIELD_SPECIFIC_CATALOG,
    FIELD_SPECIFIC_SCHEMA,
    FIELD_SPECIFIC_NAME,
    FIELD_ORDINAL_POSITION,
    FIELD_PARAMETER_MODE,
    FIELD_PARAMETER_NAME,
    FIELD_DATA_TYPE,
    FIELD_CHARACTER_MAXIMUM_LENGTH,
    FIELD_CHARACTER_OCTET_LENGTH,
    FIELD_NUMERIC_PRECISION,
    FIELD_NUMERIC_SCALE,
    FIELD_DATETIME_PRECISION,
    FIELD_CHARACTER_SET_NAME,
    FIELD_COLLATION_NAME,
    FIELD_DTD_IDENTIFIER,
    FIELD_ROUTINE_TYPE
  };

  Parameters();

  static const Parameters &instance();

  static const String_type &view_name()
  {
    static String_type s_view_name("PARAMETERS");
    return s_view_name;
  }

  virtual const String_type &name() const
  { return Parameters::view_name(); }
};

}
}

#endif // DD_SYSTEM_VIEWS__PARAMETERS_INCLUDED
