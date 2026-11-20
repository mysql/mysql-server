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

#include "sql/dd/impl/system_views/udt_methods.h"

namespace dd::system_views {

const UDT_Methods &UDT_Methods::instance() {
  static auto *s_instance = new UDT_Methods();
  return *s_instance;
}

/**
 * SQL-2023:
 *
 * CREATE VIEW METHOD_SPECIFICATIONS AS
 *  SELECT M.SPECIFIC_CATALOG, M.SPECIFIC_SCHEMA, M.SPECIFIC_NAME,
 *  M.USER_DEFINED_TYPE_CATALOG AS UDT_CATALOG,
 *  M.USER_DEFINED_TYPE_SCHEMA AS UDT_SCHEMA,
 *  M.USER_DEFINED_TYPE_NAME AS UDT_NAME,
 *  M.METHOD_NAME, IS_STATIC, IS_OVERRIDING, IS_CONSTRUCTOR,
 *  D.DATA_TYPE, D.CHARACTER_MAXIMUM_LENGTH, D.CHARACTER_OCTET_LENGTH,
 *  D.CHARACTER_SET_CATALOG, D.CHARACTER_SET_SCHEMA, D.CHARACTER_SET_NAME,
 *  D.COLLATION_CATALOG, D.COLLATION_SCHEMA, D.COLLATION_NAME,
 *  D.NUMERIC_PRECISION, D.NUMERIC_PRECISION_RADIX, D.NUMERIC_SCALE,
 *  D.DATETIME_PRECISION, D.INTERVAL_TYPE, D.INTERVAL_PRECISION,
 *  D.USER_DEFINED_TYPE_CATALOG AS RETURN_UDT_CATALOG,
 *  D.USER_DEFINED_TYPE_SCHEMA AS RETURN_UDT_SCHEMA,
 *  D.USER_DEFINED_TYPE_NAME AS RETURN_UDT_NAME,
 *  D.SCOPE_CATALOG, D.SCOPE_SCHEMA, D.SCOPE_NAME,
 *  D.MAXIMUM_CARDINALITY, D.DTD_IDENTIFIER, M.METHOD_LANGUAGE,
 *  M.PARAMETER_STYLE, M.IS_DETERMINISTIC, M.SQL_DATA_ACCESS,
 *  M.IS_NULL_CALL,
 *  M.CREATED,
 *  DT.DATA_TYPE AS RESULT_CAST_FROM_DATA_TYPE,
 *  RESULT_CAST_AS_LOCATOR,
 *  DT.CHARACTER_MAXIMUM_LENGTH AS RESULT_CAST_CHAR_MAX_LENGTH,
 *  DT.CHARACTER_OCTET_LENGTH AS RESULT_CAST_CHAR_OCTET_LENGTH,
 *  DT.CHARACTER_SET_CATALOG AS RESULT_CAST_CHAR_SET_CATALOG,
 *  DT.CHARACTER_SET_SCHEMA AS RESULT_CAST_CHAR_SET_SCHEMA,
 *  DT.CHARACTER_SET_NAME AS RESULT_CAST_CHAR_SET_NAME,
 *  DT.COLLATION_CATALOG AS RESULT_CAST_COLLATION_CATALOG,
 *  DT.COLLATION_SCHEMA AS RESULT_CAST_COLLATION_SCHEMA,
 *  DT.COLLATION_NAME AS RESULT_CAST_COLLATION_NAME,
 *  DT.NUMERIC_PRECISION AS RESULT_CAST_NUMERIC_PRECISION,
 *  DT.NUMERIC_PRECISION_RADIX AS RESULT_CAST_NUMERIC_RADIX,
 *  DT.NUMERIC_SCALE AS RESULT_CAST_NUMERIC_SCALE,
 *  DT.DATETIME_PRECISION AS RESULT_CAST_DATETIME_PRECISION,
 *  DT.INTERVAL_TYPE AS RESULT_CAST_INTERVAL_TYPE,
 *  DT.INTERVAL_PRECISION AS RESULT_CAST_INTERVAL_PRECISION,
 *  DT.USER_DEFINED_TYPE_CATALOG AS RESULT_CAST_TYPE_UDT_CATALOG,
 *  DT.USER_DEFINED_TYPE_SCHEMA AS RESULT_CAST_TYPE_UDT_SCHEMA,
 *  DT.USER_DEFINED_TYPE_NAME AS RESULT_CAST_TYPE_UDT_NAME,
 *  DT.SCOPE_CATALOG AS RESULT_CAST_SCOPE_CATALOG,
 *  DT.SCOPE_SCHEMA AS RESULT_CAST_SCOPE_SCHEMA,
 *  DT.SCOPE_NAME AS RESULT_CAST_SCOPE_NAME,
 *  DT.MAXIMUM_CARDINALITY AS RESULT_CAST_MAX_CARDINALITY,
 *  DT.DTD_IDENTIFIER AS RESULT_CAST_DTD_IDENTIFIER,
 *  D.DECLARED_DATA_TYPE, D.DECLARED_NUMERIC_PRECISION,
 *  D.DECLARED_NUMERIC_SCALE
 * FROM ...
 */
UDT_Methods::UDT_Methods() {
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_UDT_CATALOG, "UDT_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_UDT_SCHEMA, "UDT_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_UDT_NAME, "UDT_NAME",
                         "typ.name" + m_target_def.fs_name_collation());

  m_target_def.add_field(FIELD_METHOD_NAME, "METHOD_NAME", "met.name");

  m_target_def.add_field(FIELD_SPECIFIC_NAME, "SPECIFIC_NAME",
                         "met.specific_name");

  m_target_def.add_field(FIELD_IS_STATIC, "IS_STATIC", "met.is_static");
  m_target_def.add_field(FIELD_IS_OVERRIDING, "IS_OVERRIDING",
                         "met.is_overriding");
  m_target_def.add_field(FIELD_IS_CONSTRUCTOR, "IS_CONSTRUCTOR",
                         "met.is_constructor");

#ifdef LATER
  m_target_def.add_field(
      FIELD_DATA_TYPE, "DATA_TYPE",
      "SUBSTRING_INDEX(SUBSTRING_INDEX(met.column_type_utf8, '(', 1),' ', 1)");
#endif

  m_target_def.add_field(FIELD_CHARACTER_MAXIMUM_LENGTH,
                         "CHARACTER_MAXIMUM_LENGTH", "met.char_max_length");
  m_target_def.add_field(FIELD_CHARACTER_OCTET_LENGTH, "CHARACTER_OCTET_LENGTH",
                         "met.char_octet_length");

  m_target_def.add_field(FIELD_CHARACTER_SET_NAME, "CHARACTER_SET_NAME",
                         "cs.name");
  m_target_def.add_field(FIELD_COLLATION_NAME, "COLLATION_NAME", "coll.name");

  m_target_def.add_field(
      FIELD_NUMERIC_PRECISION, "NUMERIC_PRECISION",
      "IF (met.numeric_precision = 0, NULL, met.numeric_precision)");
  m_target_def.add_field(
      FIELD_NUMERIC_SCALE, "NUMERIC_SCALE",
      "IF (met.numeric_scale = 0 && met.numeric_precision = 0,"
      "    NULL, met.numeric_scale)");
  m_target_def.add_field(FIELD_DATETIME_PRECISION, "DATETIME_PRECISION",
                         "met.datetime_precision");

#ifdef LATER
  m_target_def.add_field(FIELD_RESULT_UDT_CATALOG, "RESULT_UDT_CATALOG",
                         "result_cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_RESULT_UDT_SCHEMA, "RESULT_UDT_SCHEMA",
                         "result_sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_RESULT_UDT_NAME, "RESULT_UDT_NAME",
                         "result_typ.name" + m_target_def.fs_name_collation());
#endif

  m_target_def.add_from("mysql.method_specifications met");

  m_target_def.add_from(
      "JOIN mysql.user_defined_types typ ON met.type_id=typ.id");
  m_target_def.add_from("JOIN mysql.schemata sch ON typ.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id=sch.catalog_id");

#ifdef LATER
  m_target_def.add_from(
      "JOIN mysql.user_defined_types result_typ ON "
      "met.result_type_id=result_typ.id");
  m_target_def.add_from(
      "JOIN mysql.schemata result_sch ON result_typ.schema_id=result_sch.id");
  m_target_def.add_from(
      "JOIN mysql.catalogs result_cat ON result_cat.id=result_sch.catalog_id");
#endif

  m_target_def.add_from(
      "LEFT JOIN mysql.collations coll "
      "ON met.collation_id=coll.id");
  m_target_def.add_from(
      "LEFT JOIN mysql.character_sets cs "
      "ON coll.character_set_id= cs.id");

  m_target_def.add_where("CAN_ACCESS_DATABASE(sch.name)");
}

}  // namespace dd::system_views
