/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "row_group_dump_task.h"

using namespace Mysql::Tools::Dump;

void Row_group_dump_task::set_completed()
{
  for (std::vector<Row*>::iterator it= m_rows.begin(); it != m_rows.end();
    ++it)
  {
    delete *it;
    *it= NULL;
  }

  Abstract_simple_dump_task::set_completed();
}

bool Row_group_dump_task::can_be_executed() const
{
  return true;
}

I_data_object* Row_group_dump_task::get_related_db_object() const
{
  return NULL;
}

Row_group_dump_task::Row_group_dump_task(Table* source_table,
  const std::vector<Mysql_field>& fields,
  const bool has_generated_column)
  : m_source_table(source_table),
  m_fields(fields),
  m_has_generated_columns(has_generated_column)
{}
