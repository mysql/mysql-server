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

#ifndef DD__PLUGIN_TABLE_DEFINITION_IMPL_INCLUDED
#define DD__PLUGIN_TABLE_DEFINITION_IMPL_INCLUDED

#include <map>
#include <vector>

#include "sql/dd/impl/system_registry.h"      // System_tablespaces
#include "sql/dd/string_type.h"               // dd::String_type
#include "sql/dd/types/object_table_definition.h"
#include "sql/dd/types/table.h"
#include "sql/table.h"                        // MYSQL_TABLESPACE_NAME

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Plugin_table_definition_impl: public Object_table_definition
{
private:
  String_type m_schema_name;
  String_type m_table_name;
  String_type m_table_definition;
  String_type m_table_options;
  std::vector<String_type> m_populate_statements;

  uint m_dd_version;
  String_type m_tablespace_name;

public:
  Plugin_table_definition_impl(): m_dd_version(0)
  { }

  virtual ~Plugin_table_definition_impl()
  { }

  void set_schema_name(const String_type &name)
  { m_schema_name= name; }

  const String_type &get_schema_name() const
  { return m_schema_name; }

  void set_table_name(const String_type &name)
  { m_table_name= name; }

  const String_type &get_table_name() const
  { return m_table_name; }

  void set_table_definition(const String_type &definition)
  { m_table_definition= definition; }

  void set_table_options(const String_type &options)
  { m_table_options= options; }

  virtual uint dd_version() const
  { return m_dd_version; }

  virtual void dd_version(uint version)
  { m_dd_version= version; }

  void set_tablespace_name(const String_type &tablespace_name)
  { m_tablespace_name= tablespace_name; }

  virtual String_type build_ddl_create_table() const
  {
    Stringstream_type ss;
    ss << "CREATE TABLE ";

    if (!m_schema_name.empty())
      ss << m_schema_name << ".";

    ss << m_table_name + "(\n";
    ss << m_table_definition << ")";
    ss << m_table_options;

    DBUG_ASSERT(
      System_tablespaces::instance()->find(
        MYSQL_TABLESPACE_NAME.str) != nullptr);

    // Optionally output tablespace clause
    if (!m_tablespace_name.empty())
      ss << " " << "TABLESPACE=" << m_tablespace_name;

    return ss.str();
  }

  virtual String_type build_ddl_add_cyclic_foreign_keys() const
  { return ""; }

  virtual const std::vector<String_type> &dml_populate_statements() const
  { return m_populate_statements; }
};

///////////////////////////////////////////////////////////////////////////

}

#endif	// DD__PLUGIN_TABLE_DEFINITION_IMPL_INCLUDED

