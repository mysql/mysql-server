/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "sql/dd/impl/system_views/udt_types.h"

namespace dd::system_views {

const UDT_Types &UDT_Types::instance() {
  static auto *s_instance = new UDT_Types();
  return *s_instance;
}

/**
 * SQL-2023:
 *
 * CREATE VIEW USER_DEFINED_TYPES AS
 *  SELECT UDT.USER_DEFINED_TYPE_CATALOG, UDT.USER_DEFINED_TYPE_SCHEMA,
 *  UDT.USER_DEFINED_TYPE_NAME, UDT.USER_DEFINED_TYPE_CATEGORY,
 *  UDT.IS_INSTANTIABLE, UDT.IS_FINAL, UDT.ORDERING_FORM,
 *  UDT.ORDERING_CATEGORY, UDT.ORDERING_ROUTINE_CATALOG,
 *  UDT.ORDERING_ROUTINE_SCHEMA, UDT.ORDERING_ROUTINE_NAME, UDT.REFERENCE_TYPE,
 *  DTD.DATA_TYPE, DTD.CHARACTER_MAXIMUM_LENGTH, DTD.CHARACTER_OCTET_LENGTH,
 *  DTD.CHARACTER_SET_CATALOG, DTD.CHARACTER_SET_SCHEMA, DTD.CHARACTER_SET_NAME,
 *  DTD.COLLATION_CATALOG, DTD.COLLATION_SCHEMA, DTD.COLLATION_NAME,
 *  DTD.NUMERIC_PRECISION, DTD.NUMERIC_PRECISION_RADIX, DTD.NUMERIC_SCALE,
 *  DTD.DATETIME_PRECISION, DTD.INTERVAL_TYPE, DTD.INTERVAL_PRECISION,
 *  UDT.SOURCE_DTD_IDENTIFIER, UDT.REF_DTD_IDENTIFIER,
 *  DTD.DECLARED_DATA_TYPE, DTD.DECLARED_NUMERIC_PRECISION,
 *  DTD.DECLARED_NUMERIC_SCALE, DTD.MAXIMUM_CARDINALITY
 *  FROM ...
 */
UDT_Types::UDT_Types() {
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_USER_DEFINED_TYPE_CATALOG,
                         "USER_DEFINED_TYPE_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_USER_DEFINED_TYPE_SCHEMA,
                         "USER_DEFINED_TYPE_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_USER_DEFINED_TYPE_NAME, "USER_DEFINED_TYPE_NAME",
                         "typ.name" + m_target_def.fs_name_collation());

  m_target_def.add_field(FIELD_USER_DEFINED_TYPE_CATEGORY,
                         "USER_DEFINED_TYPE_CATEGORY", "typ.type_category");

  m_target_def.add_field(FIELD_IS_INSTANTIABLE, "IS_INSTANTIABLE",
                         "typ.is_instantiable");

  m_target_def.add_field(FIELD_IS_FINAL, "IS_FINAL", "typ.is_final");

  m_target_def.add_field(FIELD_ORDERING_FORM, "ORDERING_FORM",
                         "typ.ordering_form");

  m_target_def.add_field(FIELD_ORDERING_CATEGORY, "ORDERING_CATEGORY",
                         "typ.ordering_category");

#ifdef LATER
  /*
   * TODO: CLARIFY
   * This points to a routine:
   * - UDT.ORDERING_ROUTINE_CATALOG,
   * - UDT.ORDERING_ROUTINE_SCHEMA,
   * - UDT.ORDERING_ROUTINE_NAME
   * in I_S.ROUTINE:
   * - ROUTINE_CATALOG,
   * - ROUTINE_SCHEMA,
   * - ROUTINE_NAME,
   * And then a I_S.ROUTINE can implement a specific method of a type:
   * - ROUTINE.TYPE_UDT_CATALOG
   * - ROUTINE.TYPE_UDT_SCHEMA
   * - ROUTINE.TYPE_UDT_NAME
   * - ROUTINE.SPECIFIC_NAME
   * So, pointing to the specific method name directly.
   */
  m_target_def.add_field(FIELD_ORDERING_METHOD_SPECIFIC_NAME,
                         "ORDERING_METHOD_SPECIFIC_NAME", "ordering_met.name");
#endif

  m_target_def.add_field(
      FIELD_DATA_TYPE, "DATA_TYPE",
      "SUBSTRING_INDEX(SUBSTRING_INDEX(typ.data_type_utf8, '(', 1),' ', 1)");

  m_target_def.add_field(FIELD_CHARACTER_MAXIMUM_LENGTH,
                         "CHARACTER_MAXIMUM_LENGTH", "typ.char_max_length");
  m_target_def.add_field(FIELD_CHARACTER_OCTET_LENGTH, "CHARACTER_OCTET_LENGTH",
                         "typ.char_octet_length");

  m_target_def.add_field(FIELD_CHARACTER_SET_NAME, "CHARACTER_SET_NAME",
                         "cs.name");
  m_target_def.add_field(FIELD_COLLATION_NAME, "COLLATION_NAME", "coll.name");

  m_target_def.add_field(
      FIELD_NUMERIC_PRECISION, "NUMERIC_PRECISION",
      "IF (typ.numeric_precision = 0, NULL, typ.numeric_precision)");
  m_target_def.add_field(
      FIELD_NUMERIC_SCALE, "NUMERIC_SCALE",
      "IF (typ.numeric_scale = 0 && typ.numeric_precision = 0,"
      "    NULL, typ.numeric_scale)");
  m_target_def.add_field(FIELD_DATETIME_PRECISION, "DATETIME_PRECISION",
                         "typ.datetime_precision");

  m_target_def.add_from("mysql.user_defined_types typ");
  m_target_def.add_from("JOIN mysql.schemata sch ON typ.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id=sch.catalog_id");

#ifdef LATER
  m_target_def.add_from(
      "LEFT JOIN mysql.method_specifications ordering_met "
      "ON typ.ordering_method_id=ordering_met.id");
#endif

  m_target_def.add_from(
      "LEFT JOIN mysql.collations coll "
      "ON typ.collation_id=coll.id");
  m_target_def.add_from(
      "LEFT JOIN mysql.character_sets cs "
      "ON coll.character_set_id= cs.id");

  m_target_def.add_where("CAN_ACCESS_DATABASE(sch.name)");
}

}  // namespace dd::system_views
