/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#pragma once

#include "sql/dd/impl/system_views/system_view_definition_impl.h"
#include "sql/dd/impl/system_views/system_view_impl.h"
#include "sql/dd/string_type.h"

namespace dd {
namespace system_views {

/*
  The class representing the INFORMATION_SCHEMA.JSON_DUALITY_VIEW_COLUMNS system
  view definition.
*/
class Json_duality_view_columns
    : public System_view_impl<System_view_select_definition_impl> {
 public:
  enum enum_fields {
    FIELD_TABLE_CATALOG,
    FIELD_TABLE_SCHEMA,
    FIELD_TABLE_NAME,
    FIELD_REFERENCED_TABLE_CATALOG,
    FIELD_REFERENCED_TABLE_SCHEMA,
    FIELD_REFERENCED_TABLE_NAME,
    FIELD_IS_ROOT_TABLE,
    FIELD_REFERENCED_TABLE_ID,
    FIELD_REFERENCED_COLUMN_NAME,
    FIELD_JSON_KEY_NAME,
    FIELD_ALLOW_INSERT,
    FIELD_ALLOW_UPDATE,
    FIELD_ALLOW_DELETE,
    FIELD_READ_ONLY
  };

  Json_duality_view_columns();

  static const Json_duality_view_columns &instance();

  static const String_type &view_name() {
    static String_type s_view_name("JSON_DUALITY_VIEW_COLUMNS");
    return s_view_name;
  }

  const String_type &name() const override {
    return Json_duality_view_columns::view_name();
  }
};

}  // namespace system_views
}  // namespace dd
