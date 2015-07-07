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

#ifndef TABLE_INCLUDED
#define TABLE_INCLUDED

#include "abstract_plain_sql_object.h"
#include "field.h"
#include "my_global.h"
#include <vector>
#include <string>

namespace Mysql{
namespace Tools{
namespace Dump{

class Table : public Abstract_plain_sql_object
{
public:
  Table(uint64 id, const std::string& name, const std::string& schema,
    const std::string& sql_formatted_definition, std::vector<Field>& fields,
    std::string type, uint64 row_count, uint64 row_bound, uint64 data_lenght);

  /**
    Retrieves type name.
   */
  std::string get_type() const;

  /**
    Retrieves number of rows in table, this value can be approximate.
   */
  uint64 get_row_count() const;

  /**
    Retrieves maximum number of rows in table. This value can be approximate,
    but should be upper bound for actual number of rows.
   */
  uint64 get_row_count_bound() const;

  /**
    Retrieves total number of bytes of rows data. This value can be
    approximate.
   */
  uint64 get_row_data_lenght() const;

  const std::vector<Field>& get_fields() const;

  const std::vector<std::string>& get_indexes_sql_definition() const;

  const std::string& get_sql_definition_without_indexes() const;

private:
  std::vector<Field> m_fields;
  std::vector<std::string> m_indexes_sql_definition;
  std::string m_sql_definition_without_indexes;
  std::string m_type;
  uint64 m_row_count;
  uint64 m_row_bound;
  uint64 m_data_lenght;
};

}
}
}

#endif
