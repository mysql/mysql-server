/* Copyright (c) 2016, 2026, Oracle and/or its affiliates.

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

#include "sql/dd/impl/tables/udt_methods.h"

#include <cstring>
#include <new>

#include "mysql/strings/m_ctype.h"
#include "sql/dd/impl/raw/object_keys.h"  // Parent_id_range_key
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/tables/dd_properties.h"  // TARGET_DD_VERSION
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/dd/impl/types/udt_method_impl.h"  // dd::UDT_Method_impl

namespace dd::tables {

const UDT_Methods &UDT_Methods::instance() {
  static auto *s_instance = new UDT_Methods();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

const CHARSET_INFO *UDT_Methods::name_collation() {
  return &my_charset_utf8mb3_general_ci;
}

///////////////////////////////////////////////////////////////////////////

UDT_Methods::UDT_Methods() {
  m_target_def.set_table_name("type_methods");

  m_target_def.add_field(FIELD_ID, "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_TYPE_ID, "FIELD_TYPE_ID",
                         "type_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_NAME, "FIELD_NAME",
                         "name VARCHAR(64) NOT NULL COLLATE " +
                             String_type(name_collation()->m_coll_name));

  m_target_def.add_field(FIELD_CREATED, "FIELD_CREATED",
                         "created TIMESTAMP NOT NULL");
  m_target_def.add_field(FIELD_LAST_ALTERED, "FIELD_LAST_ALTERED",
                         "last_altered TIMESTAMP NOT NULL");

  m_target_def.add_index(INDEX_PK_ID, "INDEX_PK_ID", "PRIMARY KEY (id)");
  m_target_def.add_index(INDEX_UK_TYPE_ID_NAME, "INDEX_UK_TYPE_ID_NAME",
                         "UNIQUE KEY (type_id, name)");

  m_target_def.add_foreign_key(FK_TYPE_ID, "FK_TYPE_ID",
                               "FOREIGN KEY (type_id) "
                               "REFERENCES types(id)");
}

///////////////////////////////////////////////////////////////////////////

UDT_Method *UDT_Methods::create_entity_object(const Raw_record &) const {
  return new (std::nothrow) UDT_Method_impl();
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Methods::update_object_key(Item_name_key *key, Object_id type_id,
                                    const String_type &name) {
  key->update(FIELD_TYPE_ID, type_id, FIELD_NAME, name, name_collation());
  return false;
}

///////////////////////////////////////////////////////////////////////////

}  // namespace dd::tables
