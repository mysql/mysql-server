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

#ifndef DD_SYSTEM_VIEWS__COLLATIONS_INCLUDED
#define DD_SYSTEM_VIEWS__COLLATIONS_INCLUDED

#include "sql/dd/impl/system_views/system_view_definition_impl.h"
#include "sql/dd/impl/system_views/system_view_impl.h"
#include "sql/dd/string_type.h"

namespace dd {
namespace system_views {

/*
  The class representing INFORMATION_SCHEMA.COLLATIONS system view
  definition.
*/
class Collations : public System_view_impl<System_view_select_definition_impl>
{
public:
  enum enum_fields
  {
    FIELD_COLLATION_NAME,
    FIELD_CHARACTER_SET_NAME,
    FIELD_ID,
    FIELD_IS_DEFAULT,
    FIELD_IS_COMPILED,
    FIELD_SORTLEN,
    FIELD_PAD_ATTRIBUTE
  };

  Collations();

  static const Collations &instance();

  static const String_type &view_name()
  {
    static String_type s_view_name("COLLATIONS");
    return s_view_name;
  }

  virtual const String_type &name() const
  { return Collations::view_name(); }
};

}
}

#endif // DD_SYSTEM_VIEWS__COLLATIONS_INCLUDED
