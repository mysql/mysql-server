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

#include "sql/dd/impl/system_views/udt_parameters.h"

namespace dd::system_views {

const UDT_Parameters &UDT_Parameters::instance() {
  static auto *s_instance = new UDT_Parameters();
  return *s_instance;
}

/**
 * SQL-2023:
 *
 * CREATE VIEW METHOD_SPECIFICATION_PARAMETERS AS
 * SELECT SPECIFIC_CATALOG, SPECIFIC_SCHEMA, SPECIFIC_NAME,
 * P.ORDINAL_POSITION, P.PARAMETER_MODE, P.IS_RESULT,
 * P.AS_LOCATOR, P.PARAMETER_NAME,
 * P.FROM_SQL_SPECIFIC_CATALOG, P.FROM_SQL_SPECIFIC_SCHEMA,
 * P.FROM_SQL_SPECIFIC_NAME, D.DATA_TYPE,
 * D.CHARACTER_MAXIMUM_LENGTH, D.CHARACTER_OCTET_LENGTH,
 * D.CHARACTER_SET_CATALOG, D.CHARACTER_SET_SCHEMA, D.CHARACTER_SET_NAME,
 * D.COLLATION_CATALOG, D.COLLATION_SCHEMA, D.COLLATION_NAME,
 * D.NUMERIC_PRECISION, D.NUMERIC_PRECISION_RADIX, D.NUMERIC_SCALE,
 * D.DATETIME_PRECISION, D.INTERVAL_TYPE, D.INTERVAL_PRECISION,
 * D.USER_DEFINED_TYPE_CATALOG AS PARAMETER_UDT_CATALOG,
 * D.USER_DEFINED_TYPE_SCHEMA AS PARAMETER_UDT_SCHEMA,
 * D.USER_DEFINED_TYPE_NAME AS PARAMETER_UDT_NAME,
 * D.SCOPE_CATALOG, D.SCOPE_SCHEMA, D.SCOPE_NAME,
 * D.MAXIMUM_CARDINALITY, D.DTD_IDENTIFIER,
 * D.DECLARED_DATA_TYPE, D.DECLARED_NUMERIC_PRECISION,
 * D.DECLARED_NUMERIC_SCALE
 * FROM ...
 */
UDT_Parameters::UDT_Parameters() {
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_UDT_CATALOG, "UDT_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_UDT_SCHEMA, "UDT_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_UDT_NAME, "UDT_NAME",
                         "typ.name" + m_target_def.fs_name_collation());

  m_target_def.add_field(FIELD_METHOD_SPECIFIC_NAME, "METHOD_SPECIFIC_NAME",
                         "met.specific_name");
  m_target_def.add_field(FIELD_ORDINAL_POSITION, "ORDINAL_POSITION",
                         "par.ordinal_position");

  m_target_def.add_field(FIELD_PARAMETER_MODE, "PARAMETER_MODE", "par.mode");
  m_target_def.add_field(FIELD_IS_RESULT, "IS_RESULT", "par.is_result");

  m_target_def.add_field(FIELD_PARAMETER_NAME, "PARAMETER_NAME", "par.name");

#ifdef LATER
  m_target_def.add_field(
      FIELD_DATA_TYPE, "DATA_TYPE",
      "SUBSTRING_INDEX(SUBSTRING_INDEX(par.column_type_utf8, '(', 1),' ', 1)");
#endif

  m_target_def.add_field(FIELD_CHARACTER_MAXIMUM_LENGTH,
                         "CHARACTER_MAXIMUM_LENGTH", "par.char_max_length");

  m_target_def.add_field(FIELD_CHARACTER_OCTET_LENGTH, "CHARACTER_OCTET_LENGTH",
                         "par.char_octet_length");

  m_target_def.add_field(FIELD_CHARACTER_SET_NAME, "CHARACTER_SET_NAME",
                         "cs.name");
  m_target_def.add_field(FIELD_COLLATION_NAME, "COLLATION_NAME", "coll.name");

  m_target_def.add_field(
      FIELD_NUMERIC_PRECISION, "NUMERIC_PRECISION",
      "IF (par.numeric_precision = 0, NULL, par.numeric_precision)");
  m_target_def.add_field(
      FIELD_NUMERIC_SCALE, "NUMERIC_SCALE",
      "IF (par.numeric_scale = 0 && par.numeric_precision = 0,"
      "    NULL, par.numeric_scale)");
  m_target_def.add_field(FIELD_DATETIME_PRECISION, "DATETIME_PRECISION",
                         "par.datetime_precision");

#ifdef LATER
  m_target_def.add_field(FIELD_PARAMETER_UDT_CATALOG, "PARAMETER_UDT_CATALOG",
                         "par_cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_PARAMETER_UDT_SCHEMA, "PARAMETER_UDT_SCHEMA",
                         "par_sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_PARAMETER_UDT_NAME, "PARAMETER_UDT_NAME",
                         "par_typ.name" + m_target_def.fs_name_collation());
#endif

  m_target_def.add_from("mysql.method_specification_parameters par");

  m_target_def.add_from(
      "JOIN mysql.method_specifications met ON par.method_id=met.id");
  m_target_def.add_from(
      "JOIN mysql.user_defined_types typ ON met.type_id=typ.id");
  m_target_def.add_from("JOIN mysql.schemata sch ON typ.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id=sch.catalog_id");

#ifdef LATER
  m_target_def.add_from(
      "JOIN mysql.user_defined_types par_typ ON "
      "par.parameter_type_id=par_typ.id");
  m_target_def.add_from(
      "JOIN mysql.schemata par_sch ON par_typ.schema_id=par_sch.id");
  m_target_def.add_from(
      "JOIN mysql.catalogs par_cat ON par_cat.id=par_sch.catalog_id");
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
