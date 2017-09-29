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

#include "sql/dd/impl/system_views/column_statistics.h"

namespace dd {
namespace system_views {

const Column_statistics &Column_statistics::instance()
{
  static Column_statistics *s_instance= new Column_statistics();
  return *s_instance;
}

Column_statistics::Column_statistics()
{
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_SCHEMA_NAME, "SCHEMA_NAME", "SCHEMA_NAME");
  m_target_def.add_field(FIELD_TABLE_NAME, "TABLE_NAME", "TABLE_NAME");
  m_target_def.add_field(FIELD_COLUMN_NAME, "COLUMN_NAME", "COLUMN_NAME");
  m_target_def.add_field(FIELD_HISTOGRAM, "HISTOGRAM", "HISTOGRAM");
  m_target_def.add_from("mysql.column_statistics");
  m_target_def.add_where("CAN_ACCESS_TABLE(SCHEMA_NAME, TABLE_NAME)");
}

}
}
