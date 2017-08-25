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

#ifndef DD_SYSTEM_VIEWS__REFERENTIAL_CONSTRAINTS_INCLUDED
#define DD_SYSTEM_VIEWS__REFERENTIAL_CONSTRAINTS_INCLUDED

#include "sql/dd/impl/system_views/system_view_definition_impl.h"
#include "sql/dd/impl/system_views/system_view_impl.h"

namespace dd {
namespace system_views {

/*
  The class representing INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS system
  view definition.
*/
class Referential_constraints :
  public System_view_impl<System_view_select_definition_impl>
{
public:
  enum enum_fields
  {
    FIELD_CONSTRAINT_CATALOG,
    FIELD_CONSTRAINT_SCHEMA,
    FIELD_CONSTRAINT_NAME,
    FIELD_UNIQUE_CONSTRAINT_CATALOG,
    FIELD_UNIQUE_CONSTRAINT_SCHEMA,
    FIELD_UNIQUE_CONSTRAINT_NAME,
    FIELD_MATCH_OPTION,
    FIELD_UPDATE_RULE,
    FIELD_DELETE_RULE,
    FIELD_TABLE_NAME,
    FIELD_REFERENCED_TABLE_NAME
  };

  Referential_constraints();

  static const Referential_constraints &instance();

  static const String_type &view_name()
  {
    static String_type s_view_name("REFERENTIAL_CONSTRAINTS");
    return s_view_name;
  }
  virtual const String_type &name() const
  { return Referential_constraints::view_name(); }
};

}
}

#endif // DD_SYSTEM_VIEWS__REFERENTIAL_CONSTRAINTS_INCLUDED
