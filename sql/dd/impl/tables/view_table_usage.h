/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__VIEW_TABLE_USAGE_INCLUDED
#define DD_TABLES__VIEW_TABLE_USAGE_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"                    // dd::Object_id
#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl

namespace dd {
  class Object_key;
namespace tables {

///////////////////////////////////////////////////////////////////////////

class View_table_usage : public Object_table_impl
{
public:
  static const View_table_usage &instance()
  {
    static View_table_usage *s_instance= new (std::nothrow) View_table_usage();
    return *s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("view_table_usage");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_VIEW_ID,
    FIELD_TABLE_CATALOG,
    FIELD_TABLE_SCHEMA,
    FIELD_TABLE_NAME
  };

public:
  View_table_usage()
  {
    m_target_def.table_name(table_name());
    m_target_def.dd_version(1);

    m_target_def.add_field(FIELD_VIEW_ID,
                           "FIELD_VIEW_ID",
                           "view_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_TABLE_CATALOG,
                           "FIELD_TABLE_CATALOG",
                           "table_catalog VARCHAR(64) NOT NULL COLLATE " +
                           std::string(Object_table_definition_impl::
                                         fs_name_collation()->name));
    m_target_def.add_field(FIELD_TABLE_SCHEMA,
                           "FIELD_TABLE_SCHEMA",
                           "table_schema VARCHAR(64) NOT NULL COLLATE " +
                           std::string(Object_table_definition_impl::
                                         fs_name_collation()->name));
    m_target_def.add_field(FIELD_TABLE_NAME,
                           "FIELD_TABLE_NAME",
                           "table_name VARCHAR(64) NOT NULL COLLATE " +
                           std::string(Object_table_definition_impl::
                                         fs_name_collation()->name));

    m_target_def.add_index("PRIMARY KEY(view_id, table_catalog, "
                           "table_schema, table_name)");
    m_target_def.add_index("KEY (table_catalog, table_schema, table_name)");

    m_target_def.add_foreign_key("FOREIGN KEY (view_id) REFERENCES "
                                 "tables(id)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("ROW_FORMAT=DYNAMIC");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return View_table_usage::table_name(); }

public:
  static Object_key *create_key_by_view_id(Object_id view_id);

  static Object_key *create_primary_key(Object_id view_id,
                                        const std::string &table_catalog,
                                        const std::string &table_schema,
                                        const std::string &table_name);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__VIEW_TABLE_USAGE_INCLUDED
