/* Copyright (c) 2017, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD_SYSTEM_VIEWS__TYPES_INCLUDED
#define DD_SYSTEM_VIEWS__TYPES_INCLUDED

#include "sql/dd/impl/system_views/system_view_definition_impl.h"
#include "sql/dd/impl/system_views/system_view_impl.h"
#include "sql/dd/string_type.h"

namespace dd {
namespace system_views {

/*
  The class representing INFORMATION_SCHEMA.TYPES
  system view definition.
*/
class UDT_Types : public System_view_impl<System_view_select_definition_impl> {
 public:
  enum enum_fields {
    FIELD_USER_DEFINED_TYPE_CATALOG,
    FIELD_USER_DEFINED_TYPE_SCHEMA,
    FIELD_USER_DEFINED_TYPE_NAME,
    FIELD_USER_DEFINED_TYPE_CATEGORY,
    FIELD_IS_INSTANTIABLE,
    FIELD_IS_FINAL,
    FIELD_ORDERING_FORM,
    FIELD_ORDERING_CATEGORY,
    // FIXME: FIELD_ORDERING_ROUTINE_CATALOG,
    // FIXME: FIELD_ORDERING_ROUTINE_SCHEMA,
    // FIXME: FIELD_ORDERING_ROUTINE_NAME,
    FIELD_ORDERING_METHOD_SPECIFIC_NAME,
    FIELD_DATA_TYPE,
    FIELD_CHARACTER_MAXIMUM_LENGTH,
    FIELD_CHARACTER_OCTET_LENGTH,
    FIELD_CHARACTER_SET_NAME,
    FIELD_COLLATION_NAME,
    FIELD_NUMERIC_PRECISION,
    FIELD_NUMERIC_SCALE,
    FIELD_DATETIME_PRECISION,
    // FIXME: FIELD_DECLARED_DATA_TYPE,
    // FIXME: FIELD_DECLARED_NUMERIC_PRECISION,
    // FIXME: FIELD_DECLARED_NUMERIC_SCALE,
    FIELD_CREATE_TIME,
    FIELD_UPDATE_TIME
  };

  UDT_Types();

  static const UDT_Types &instance();

  static const String_type &view_name() {
    static String_type s_view_name("USER_DEFINED_TYPES");
    return s_view_name;
  }

  const String_type &name() const override { return UDT_Types::view_name(); }
};

}  // namespace system_views
}  // namespace dd

#endif  // DD_SYSTEM_VIEWS__TYPES_INCLUDED
