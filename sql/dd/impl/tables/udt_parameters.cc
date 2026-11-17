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

#include "sql/dd/impl/tables/udt_parameters.h"

#include <cstring>
#include <new>

#include "mysql/strings/m_ctype.h"
#include "sql/dd/impl/raw/object_keys.h"  // Parent_id_range_key
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/tables/dd_properties.h"  // TARGET_DD_VERSION
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/dd/impl/types/udt_parameter_impl.h"  // dd::UDT_Parameter_impl

namespace dd::tables {

const UDT_Parameters &UDT_Parameters::instance() {
  static auto *s_instance = new UDT_Parameters();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

const CHARSET_INFO *UDT_Parameters::name_collation() {
  return &my_charset_utf8mb3_general_ci;
}

///////////////////////////////////////////////////////////////////////////

UDT_Parameters::UDT_Parameters() {
  m_target_def.set_table_name("method_specification_parameters");

  m_target_def.add_field(FIELD_ID, "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_METHOD_ID, "FIELD_METHOD_ID",
                         "method_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_NAME, "FIELD_NAME",
                         "name VARCHAR(64) NOT NULL COLLATE " +
                             String_type(name_collation()->m_coll_name));

  m_target_def.add_field(FIELD_IS_RESULT, "FIELD_IS_RESULT", "is_result BOOL");

  m_target_def.add_field(FIELD_IS_UNSIGNED, "FIELD_IS_UNSIGNED",
                         "is_unsigned BOOL");
  m_target_def.add_field(FIELD_CHAR_MAX_LENGTH, "FIELD_CHAR_MAX_LENGTH",
                         "char_max_length INT UNSIGNED");
  m_target_def.add_field(FIELD_NUMERIC_PRECISION, "FIELD_NUMERIC_PRECISION",
                         "numeric_precision INT UNSIGNED");
  m_target_def.add_field(FIELD_NUMERIC_SCALE, "FIELD_NUMERIC_SCALE",
                         "numeric_scale INT UNSIGNED");
  m_target_def.add_field(FIELD_DATETIME_PRECISION, "FIELD_DATETIME_PRECISION",
                         "datetime_precision INT UNSIGNED");
  m_target_def.add_field(FIELD_COLLATION_ID, "FIELD_COLLATION_ID",
                         "collation_id BIGINT UNSIGNED");
  m_target_def.add_field(FIELD_IS_EXPLICIT_COLLATION,
                         "FIELD_IS_EXPLICIT_COLLATION",
                         "is_explicit_collation BOOL");

  m_target_def.add_field(FIELD_CREATED, "FIELD_CREATED",
                         "created TIMESTAMP NOT NULL");
  m_target_def.add_field(FIELD_LAST_ALTERED, "FIELD_LAST_ALTERED",
                         "last_altered TIMESTAMP NOT NULL");

  m_target_def.add_index(INDEX_PK_ID, "INDEX_PK_ID", "PRIMARY KEY (id)");
  m_target_def.add_index(INDEX_UK_METHOD_ID_NAME, "INDEX_UK_METHOD_ID_NAME",
                         "UNIQUE KEY (method_id, name)");

  m_target_def.add_foreign_key(FK_METHOD_ID, "FK_METHOD_ID",
                               "FOREIGN KEY (method_id) "
                               "REFERENCES method_specifications(id)");
}

///////////////////////////////////////////////////////////////////////////

UDT_Parameter *UDT_Parameters::create_entity_object(const Raw_record &) const {
  return new (std::nothrow) UDT_Parameter_impl();
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Parameters::update_object_key(Item_name_key *key, Object_id method_id,
                                       const String_type &name) {
  key->update(FIELD_METHOD_ID, method_id, FIELD_NAME, name, name_collation());
  return false;
}

///////////////////////////////////////////////////////////////////////////

}  // namespace dd::tables
