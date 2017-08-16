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

#include "sql/dd/impl/system_views/views.h"

#include <string>

#include "sql/stateless_allocator.h"

namespace dd {
namespace system_views {

const Views &Views::instance()
{
  static Views *s_instance= new Views();
  return *s_instance;
}

Views::Views()
{
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_TABLE_CATALOG, "TABLE_CATALOG",
                         "cat.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_SCHEMA, "TABLE_SCHEMA",
                         "sch.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_TABLE_NAME, "TABLE_NAME",
                         "vw.name" + m_target_def.fs_name_collation());
  m_target_def.add_field(FIELD_VIEW_DEFINITION, "VIEW_DEFINITION",
    "IF(CAN_ACCESS_VIEW(sch.name, vw.name, vw.view_definer, vw.options)=TRUE,"
    "   vw.view_definition_utf8, '')");
  m_target_def.add_field(FIELD_CHECK_OPTION, "CHECK_OPTION",
                         "vw.view_check_option");
  m_target_def.add_field(FIELD_IS_UPDATABLE, "IS_UPDATABLE",
                         "vw.view_is_updatable");
  m_target_def.add_field(FIELD_DEFINER, "DEFINER", "vw.view_definer");
  m_target_def.add_field(FIELD_SECURITY_TYPE, "SECURITY_TYPE",
    "IF (vw.view_security_type='DEFAULT', 'DEFINER', vw.view_security_type)");
  m_target_def.add_field(FIELD_CHARACTER_SET_CLIENT, "CHARACTER_SET_CLIENT",
                         "cs.name");
  m_target_def.add_field(FIELD_COLLATION_CONNECTION , "COLLATION_CONNECTION",
                         "conn_coll.name");

  m_target_def.add_from("mysql.tables vw");
  m_target_def.add_from("JOIN mysql.schemata sch ON vw.schema_id=sch.id");
  m_target_def.add_from("JOIN mysql.catalogs cat ON cat.id=sch.catalog_id");
  m_target_def.add_from("JOIN mysql.collations conn_coll"
                        " ON conn_coll.id= vw.view_connection_collation_id");
  m_target_def.add_from("JOIN mysql.collations client_coll"
                        " ON client_coll.id= vw.view_client_collation_id");
  m_target_def.add_from("JOIN mysql.character_sets cs"
                        " ON cs.id= client_coll.character_set_id");

  m_target_def.add_where("CAN_ACCESS_TABLE(sch.name, vw.name)");
  m_target_def.add_where("AND vw.type = 'VIEW'");
}

}
}
