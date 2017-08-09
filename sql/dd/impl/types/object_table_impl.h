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

#ifndef DD__OBJECT_TABLE_IMPL_INCLUDED
#define DD__OBJECT_TABLE_IMPL_INCLUDED


#include <mysqld_error.h>

#include "sql/dd/impl/dictionary_impl.h"                // get_target_dd_...
#include "sql/dd/impl/types/object_table_definition_impl.h" // Object_table_defin...
#include "sql/dd/types/object_table.h"                  // Object_table
#include "sql/dd/types/object_table.h"                  // Object_table
#include "sql/log.h"                                    // log_*()
#include "sql/mysqld.h"                                 // opt_initialize

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table_impl : virtual public Object_table
{
protected:
  Object_table_definition_impl m_target_def;

public:
  Object_table_impl()
  {
    // Add common options
    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("ROW_FORMAT=DYNAMIC");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

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
  { return true; }

  virtual ~Object_table_impl()
  { }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__OBJECT_TABLE_IMPL_INCLUDED
