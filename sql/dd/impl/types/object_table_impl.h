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

#ifndef DD__OBJECT_TABLE_IMPL_INCLUDED
#define DD__OBJECT_TABLE_IMPL_INCLUDED

#include "my_global.h"

#include "log.h"                                        // sql_print_warning
#include "mysqld.h"                                     // opt_initialize

#include "dd/impl/dictionary_impl.h"                    // get_target_dd_...
#include "dd/impl/types/object_table_definition_impl.h" // Object_table_defin...
#include "dd/types/object_table.h"                      // Object_table
#include "dd/types/object_table.h"                      // Object_table

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table_impl : virtual public Object_table
{
protected:
  Object_table_definition_impl m_target_def;

public:
  virtual const Object_table_definition *table_definition(
                  uint version MY_ATTRIBUTE((unused))) const
  {
    // Upgrade/downgrade not supported yet.
    if (m_target_def.dd_version() != version)
    {
      sql_print_warning("Data Dictionary version %d not supported", version);
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
      sql_print_warning("Data Dictionary version %d not supported",
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

  virtual bool populate(THD *thd) const
  { return false; }

  virtual bool hidden() const
  { return true; }

  virtual ~Object_table_impl()
  { }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__OBJECT_TABLE_IMPL_INCLUDED
