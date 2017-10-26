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

#ifndef DD__PLUGIN_TABLE_IMPL_INCLUDED
#define DD__PLUGIN_TABLE_IMPL_INCLUDED

#include "mysql/components/services/log_builtins.h"
#include "sql/dd/impl/types/plugin_table_definition_impl.h"
#include "sql/dd/types/object_table.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Plugin_table_impl : public Object_table
{
protected:
  Plugin_table_definition_impl m_target_def;

public:
  virtual const Object_table_definition *table_definition(
                  uint version MY_ATTRIBUTE((unused))) const
  {
    // Upgrade/downgrade not supported yet.
    if (m_target_def.dd_version() != version)
    {
      LogErr(WARNING_LEVEL, ER_DD_VERSION_UNSUPPORTED, version);
      return nullptr;
    }
    return &m_target_def;
  }

  virtual const Object_table_definition *table_definition(
                  THD *thd MY_ATTRIBUTE((unused))) const
  {
    // Upgrade/downgrade not supported yet.
    if (m_target_def.dd_version() != default_dd_version(thd))
    {
      LogErr(WARNING_LEVEL, ER_DD_VERSION_UNSUPPORTED,
             default_dd_version(thd));
      return nullptr;
    }
    return &m_target_def;
  }

  virtual uint default_dd_version(THD *thd) const
  {
    if (opt_initialize)
      return Dictionary_impl::get_target_dd_version();
    return Dictionary_impl::instance()->get_actual_dd_version(thd);
  }

  virtual bool populate(THD*) const
  { return false; }

  virtual bool hidden() const
  { return false; }

  Plugin_table_impl(const String_type &name, const String_type &definition,
                    const String_type &options, uint version,
                    const char* tablespace_name)
  {
    m_target_def.set_table_name(name);
    m_target_def.set_table_definition(definition);
    m_target_def.set_table_options(options);
    m_target_def.dd_version(version);
    if (tablespace_name)
      m_target_def.set_tablespace_name(tablespace_name);
  }

  Plugin_table_impl(const String_type &schema_name,
                    const String_type &name,
                    const String_type &definition,
                    const String_type &options, uint version,
                    const char* tablespace_name)
  : Plugin_table_impl(name, definition, options, version, tablespace_name)
  {
    m_target_def.set_schema_name(schema_name);
  }

  virtual ~Plugin_table_impl()
  { }

  virtual const String_type &schema() const
  { return m_target_def.get_schema_name(); }

  virtual const String_type &name() const
  { return m_target_def.get_table_name(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PLUGIN_TABLE_IMPL_INCLUDED
