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

#include <mysqld_error.h>

#include "sql/dd/impl/types/object_table_impl.h"        // Object_table_impl
#include "sql/dd/impl/bootstrap_ctx.h"                  // DD_bootstrap_ctx

namespace dd {

///////////////////////////////////////////////////////////////////////////

Object_table_impl::Object_table_impl(): m_last_dd_version(0), m_target_def(),
    m_actual_present(false), m_actual_def(), m_hidden(true)
{
  m_target_def.add_option(static_cast<int>(Common_option::ENGINE),
                          "ENGINE",
                          "ENGINE=INNODB");
  m_target_def.add_option(static_cast<int>(Common_option::CHARSET),
                          "CHARSET",
                          "DEFAULT CHARSET=utf8");
  m_target_def.add_option(static_cast<int>(Common_option::COLLATION),
                          "COLLATION",
                          "COLLATE=utf8_bin");
  m_target_def.add_option(static_cast<int>(Common_option::ROW_FORMAT),
                          "ROW_FORMAT",
                          "ROW_FORMAT=DYNAMIC");
  m_target_def.add_option(static_cast<int>(Common_option::STATS_PERSISTENT),
                          "STATS_PERSISTENT",
                          "STATS_PERSISTENT=0");
  m_target_def.add_option(static_cast<int>(Common_option::TABLESPACE),
                          "TABLESPACE",
                          String_type("TABLESPACE=") +
                            String_type(MYSQL_TABLESPACE_NAME.str));
}

bool Object_table_impl::set_actual_table_definition(
  const Properties &table_def_properties) const
{
  m_actual_present= true;
  return m_actual_def.restore_from_properties(table_def_properties);
}

int Object_table_impl::field_number(int target_field_number,
  const String_type &field_label) const
{
  /*
    During upgrade, we must re-interpret the field number using the
    field label. Otherwise, we use the target field number. Note that
    for minor downgrade, we use the target field number directly since
    only extensions are allowed.
  */
  if (bootstrap::DD_bootstrap_ctx::instance().is_upgrade())
    return m_actual_def.field_number(field_label);
  return target_field_number;
}

///////////////////////////////////////////////////////////////////////////

Object_table *Object_table::create_object_table()
{
  return new (std::nothrow) Object_table_impl();
}

///////////////////////////////////////////////////////////////////////////

}
