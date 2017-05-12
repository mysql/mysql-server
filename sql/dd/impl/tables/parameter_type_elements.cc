/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/tables/parameter_type_elements.h"

#include <new>

#include "dd/impl/raw/object_keys.h"  // Parent_id_range_key
#include "dd/impl/types/object_table_definition_impl.h"

namespace dd {
namespace tables {


const Parameter_type_elements &Parameter_type_elements::instance()
{
  static Parameter_type_elements *s_instance= new Parameter_type_elements();
  return *s_instance;
}

Parameter_type_elements::Parameter_type_elements()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_PARAMETER_ID,
                         "FIELD_PARAMETER_ID",
                         "parameter_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_INDEX,
                         "FIELD_INDEX",
                         "element_index INT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_NAME,
                         "FIELD_NAME",
                         "name VARBINARY(255) NOT NULL");

  m_target_def.add_index("PRIMARY KEY(parameter_id, element_index)");
  // We may have multiple similar element names. Do we plan to deprecate it?
  // m_target_def.add_index("UNIQUE KEY(column_id, name)");

  m_target_def.add_foreign_key("FOREIGN KEY (parameter_id) REFERENCES "
                               "parameters(id)");
}

///////////////////////////////////////////////////////////////////////////

Object_key *Parameter_type_elements::create_key_by_parameter_id(
  Object_id parameter_id)
{
  return new (std::nothrow) Parent_id_range_key(
                              0, FIELD_PARAMETER_ID, parameter_id);
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Object_key *Parameter_type_elements::create_primary_key(
  Object_id parameter_id, int index)
{
  const int INDEX_NO= 0;

  return new (std::nothrow) Composite_pk(INDEX_NO,
                                         FIELD_PARAMETER_ID, parameter_id,
                                         FIELD_INDEX, index);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

}
}
