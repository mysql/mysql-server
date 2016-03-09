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

#ifndef DD__PLUGIN_TABLE_DEFINITION_IMPL_INCLUDED
#define DD__PLUGIN_TABLE_DEFINITION_IMPL_INCLUDED

#include "table.h"                            // MYSQL_TABLESPACE_NAME
#include "dd/impl/system_registry.h"          // System_tablespaces
#include "dd/types/object_table_definition.h"
#include "dd/types/table.h"

#include <string>
#include <vector>
#include <map>
#include <sstream>

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Plugin_table_definition_impl: public Object_table_definition
{
private:
  std::string m_table_name;
  std::string m_table_definition;
  std::string m_table_options;
  std::vector<std::string> m_populate_statements;

  uint m_dd_version;

public:
  Plugin_table_definition_impl(): m_dd_version(0)
  { }

  virtual ~Plugin_table_definition_impl()
  { }

  void set_table_name(const std::string &name)
  { m_table_name= name; }

  const std::string &get_table_name() const
  { return m_table_name; }

  void set_table_definition(const std::string &definition)
  { m_table_definition= definition; }

  void set_table_options(const std::string &options)
  { m_table_options= options; }

  virtual uint dd_version() const
  { return m_dd_version; }

  virtual void dd_version(uint version)
  { m_dd_version= version; }

  virtual std::string build_ddl_create_table() const
  {
    std::stringstream ss;
    ss << "CREATE TABLE " + m_table_name + "(\n";
    ss << m_table_definition << ")";
    ss << m_table_options;

    // Optionally output tablespace clause
    if (System_tablespaces::instance()->find(MYSQL_TABLESPACE_NAME.str))
      ss << " " << "TABLESPACE=" << MYSQL_TABLESPACE_NAME.str;

    return ss.str();
  }

  virtual std::string build_ddl_add_cyclic_foreign_keys() const
  { return ""; }

  virtual const std::vector<std::string> &dml_populate_statements() const
  { return m_populate_statements; }
};

///////////////////////////////////////////////////////////////////////////

}

#endif	// DD__PLUGIN_TABLE_DEFINITION_IMPL_INCLUDED

