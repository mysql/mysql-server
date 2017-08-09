/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/impl/tables/view_table_usage.h"

#include <new>
#include <string>

#include "sql/dd/impl/raw/object_keys.h"  // dd::Parent_id_range_key
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/mysqld.h"
#include "sql/stateless_allocator.h"

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

const View_table_usage &View_table_usage::instance()
{
  static View_table_usage *s_instance= new (std::nothrow) View_table_usage();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

View_table_usage::View_table_usage()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_VIEW_ID,
                         "FIELD_VIEW_ID",
                         "view_id BIGINT UNSIGNED NOT NULL");
  m_target_def.add_field(FIELD_TABLE_CATALOG,
                         "FIELD_TABLE_CATALOG",
                         "table_catalog VARCHAR(64) NOT NULL COLLATE " +
                         String_type(Object_table_definition_impl::
                                     fs_name_collation()->name));
  m_target_def.add_field(FIELD_TABLE_SCHEMA,
                         "FIELD_TABLE_SCHEMA",
                         "table_schema VARCHAR(64) NOT NULL COLLATE " +
                         String_type(Object_table_definition_impl::
                                     fs_name_collation()->name));
  m_target_def.add_field(FIELD_TABLE_NAME,
                         "FIELD_TABLE_NAME",
                         "table_name VARCHAR(64) NOT NULL COLLATE " +
                         String_type(Object_table_definition_impl::
                                     fs_name_collation()->name));

  m_target_def.add_index("PRIMARY KEY(view_id, table_catalog, "
                         "table_schema, table_name)");
  m_target_def.add_index("KEY (table_catalog, table_schema, table_name)");

  m_target_def.add_foreign_key("FOREIGN KEY (view_id) REFERENCES "
                               "tables(id)");
}

///////////////////////////////////////////////////////////////////////////

Object_key *View_table_usage::create_key_by_view_id(
  Object_id view_id)
{
  return new (std::nothrow) Parent_id_range_key(0, FIELD_VIEW_ID, view_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *View_table_usage::create_primary_key(
  Object_id view_id,
  const String_type &table_catalog,
  const String_type &table_schema,
  const String_type &table_name)
{
  const int index_no= 0;

  return new (std::nothrow) Composite_obj_id_3char_key(index_no,
                                                       FIELD_VIEW_ID,
                                                       view_id,
                                                       FIELD_TABLE_CATALOG,
                                                       table_catalog,
                                                       FIELD_TABLE_SCHEMA,
                                                       table_schema,
                                                       FIELD_TABLE_NAME,
                                                       table_name);
}

///////////////////////////////////////////////////////////////////////////

Object_key *View_table_usage::create_key_by_name(
  const String_type &table_catalog,
  const String_type &table_schema,
  const String_type &table_name)
{
  const int index_no= 1;

  return new (std::nothrow) Table_reference_range_key(index_no,
                                                 FIELD_TABLE_CATALOG,
                                                 table_catalog,
                                                 FIELD_TABLE_SCHEMA,
                                                 table_schema,
                                                 FIELD_TABLE_NAME,
                                                 table_name);
}

}
}
