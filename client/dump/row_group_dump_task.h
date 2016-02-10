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

#ifndef ROW_GROUP_DUMP_TASK_INCLUDED
#define ROW_GROUP_DUMP_TASK_INCLUDED

#include "abstract_simple_dump_task.h"
#include "row.h"
#include "mysql_field.h"
#include "table.h"
#include <vector>

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Represents single data row.
 */
class Row_group_dump_task : public Abstract_simple_dump_task
{
public:
  Row_group_dump_task(
      Table* source_table, const std::vector<Mysql_field>& fields,
      const bool has_generated_column);

  virtual I_data_object* get_related_db_object() const;

  bool can_be_executed() const;

  void set_completed();

  /**
    Returns a table the rows are contained in.
   */
  const Table* m_source_table;
  /**
    Contains all fields information.
   */
  const std::vector<Mysql_field>& m_fields;
  /**
    Returns all rows.
   */
  std::vector<Row*> m_rows;
  /**
    Contains generated/virtual fields.
  */
  const bool m_has_generated_columns;
};

}
}
}

#endif
