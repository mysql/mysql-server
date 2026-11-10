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

#include "sql/dd/impl/system_views/udt_methods.h"

namespace dd::system_views {

const UDT_Methods &UDT_Methods::instance() {
  static auto *s_instance = new UDT_Methods();
  return *s_instance;
}

UDT_Methods::UDT_Methods() {
  m_target_def.set_view_name(view_name());

#ifdef NEVER
  m_target_def.add_field(
      FIELD_SPECIFIC_CATALOG, "SPECIFIC_CATALOG",
      "specific_cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(
      FIELD_SPECIFIC_SCHEMA, "SPECIFIC_SCHEMA",
      "specific_sch.name" + m_target_def.fs_name_collation());
#endif

  m_target_def.add_field(FIELD_SPECIFIC_NAME, "SPECIFIC_NAME",
                         "met.specific_name");

  m_target_def.add_field(FIELD_UDT_CATALOG, "UDT_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_UDT_SCHEMA, "UDT_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_UDT_NAME, "UDT_NAME",
                         "typ.name" + m_target_def.fs_name_collation());

  m_target_def.add_field(FIELD_METHOD_NAME, "METHOD_NAME", "met.name");

  m_target_def.add_field(FIELD_IS_STATIC, "IS_STATIC", "met.is_static");
  m_target_def.add_field(FIELD_IS_OVERRIDING, "IS_OVERRIDING",
                         "met.is_overriding");
  m_target_def.add_field(FIELD_IS_CONSTRUCTOR, "IS_CONSTRUCTOR",
                         "met.is_constructor");

  m_target_def.add_field(FIELD_CHARACTER_MAXIMUM_LENGTH,
                         "CHARACTER_MAXIMUM_LENGTH", "met.char_max_length");
  m_target_def.add_field(FIELD_CHARACTER_OCTET_LENGTH, "CHARACTER_OCTET_LENGTH",
                         "met.char_octet_length");
  m_target_def.add_field(
      FIELD_NUMERIC_PRECISION, "NUMERIC_PRECISION",
      "IF (met.numeric_precision = 0, NULL, met.numeric_precision)");
  m_target_def.add_field(
      FIELD_NUMERIC_SCALE, "NUMERIC_SCALE",
      "IF (met.numeric_scale = 0 && met.numeric_precision = 0,"
      "    NULL, met.numeric_scale)");
  m_target_def.add_field(FIELD_DATETIME_PRECISION, "DATETIME_PRECISION",
                         "met.datetime_precision");

  m_target_def.add_field(FIELD_CHARACTER_SET_NAME, "CHARACTER_SET_NAME",
                         "cs.name");
  m_target_def.add_field(FIELD_COLLATION_NAME, "COLLATION_NAME", "coll.name");

  m_target_def.add_from("mysql.type_methods met");
  m_target_def.add_from("JOIN mysql.types typ ON met.type_id=typ.id");
  m_target_def.add_from("JOIN mysql.schemata sch ON typ.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id=sch.catalog_id");

  m_target_def.add_from(
      "LEFT JOIN mysql.collations coll "
      "ON met.collation_id=coll.id");
  m_target_def.add_from(
      "LEFT JOIN mysql.character_sets cs "
      "ON coll.character_set_id= cs.id");

  m_target_def.add_where("CAN_ACCESS_DATABASE(sch.name)");
}

}  // namespace dd::system_views
