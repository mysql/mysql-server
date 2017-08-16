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

#include "sql/dd/impl/tables/view_routine_usage.h"

#include <new>
#include <string>

#include "sql/dd/impl/raw/object_keys.h"  // dd::Parent_id_range_key
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/mysqld.h"
#include "sql/stateless_allocator.h"

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

const View_routine_usage &View_routine_usage::instance()
{
  static View_routine_usage *s_instance= new (std::nothrow)View_routine_usage();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

View_routine_usage::View_routine_usage()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_VIEW_ID,
                         "FIELD_VIEW_ID",
                         "view_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_ROUTINE_CATALOG,
                         "FIELD_ROUTINE_CATALOG",
                         "routine_catalog VARCHAR(64) NOT NULL COLLATE " +
                         String_type(Object_table_definition_impl::
                                     fs_name_collation()->name));
  m_target_def.add_field(FIELD_ROUTINE_SCHEMA,
                         "FIELD_ROUTINE_SCHEMA",
                         "routine_schema VARCHAR(64) NOT NULL COLLATE " +
                         String_type(Object_table_definition_impl::
                                     fs_name_collation()->name));
  m_target_def.add_field(FIELD_ROUTINE_NAME,
                         "FIELD_ROUTINE_NAME",
                         "routine_name VARCHAR(64) NOT NULL COLLATE "
                         " utf8_general_ci");

  m_target_def.add_index("PRIMARY KEY(view_id, routine_catalog, "
                         "routine_schema, routine_name)");
  m_target_def.add_index("KEY (routine_catalog, routine_schema, "
                         "routine_name)");

  m_target_def.add_foreign_key("FOREIGN KEY (view_id) REFERENCES "
                               "tables(id)");
}

///////////////////////////////////////////////////////////////////////////

Object_key *View_routine_usage::create_key_by_view_id(
  Object_id view_id)
{
  return new (std::nothrow) Parent_id_range_key(0, FIELD_VIEW_ID, view_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *View_routine_usage::create_primary_key(
  Object_id view_id,
  const String_type &routine_catalog,
  const String_type &routine_schema,
  const String_type &routine_name)
{
  const int index_no= 0;

  return new (std::nothrow) Composite_obj_id_3char_key(index_no,
                                                       FIELD_VIEW_ID,
                                                       view_id,
                                                       FIELD_ROUTINE_CATALOG,
                                                       routine_catalog,
                                                       FIELD_ROUTINE_SCHEMA,
                                                       routine_schema,
                                                       FIELD_ROUTINE_NAME,
                                                       routine_name);
}

///////////////////////////////////////////////////////////////////////////

Object_key *View_routine_usage::create_key_by_name(
  const String_type &routine_catalog,
  const String_type &routine_schema,
  const String_type &routine_name)
{
  const int index_no= 1;

  return new (std::nothrow) Table_reference_range_key(index_no,
                                                 FIELD_ROUTINE_CATALOG,
                                                 routine_catalog,
                                                 FIELD_ROUTINE_SCHEMA,
                                                 routine_schema,
                                                 FIELD_ROUTINE_NAME,
                                                 routine_name);
}

///////////////////////////////////////////////////////////////////////////

}
}
