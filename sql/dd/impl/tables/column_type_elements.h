/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__COLUMN_TYPE_ELEMENTS_INCLUDED
#define DD_TABLES__COLUMN_TYPE_ELEMENTS_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"                    // dd::Object_id
#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl 

namespace dd {
  class Object_key;
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Column_type_elements : virtual public Object_table_impl
{
public:
  static const Column_type_elements &instance()
  {
    static Column_type_elements s_instance;
    return s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("column_type_elements");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_COLUMN_ID,
    FIELD_INDEX,
    FIELD_NAME
  };

public:
  Column_type_elements()
  {
    m_target_def.table_name(table_name());

    m_target_def.add_field(FIELD_COLUMN_ID,
                           "FIELD_COLUMN_ID",
                           "column_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_INDEX,
                           "FIELD_INDEX",
                           "element_index INT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_NAME,
                           "FIELD_NAME",
                           "name VARBINARY(255) NOT NULL");

    m_target_def.add_index("PRIMARY KEY(column_id, element_index)");
    // We may have multiple similar element names. Do we plan to deprecate it?
    // m_target_def.add_index("UNIQUE KEY(column_id, name)");

    m_target_def.add_foreign_key("FOREIGN KEY (column_id) REFERENCES "
                                 "columns(id)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return Column_type_elements::table_name(); }

public:
  static Object_key *create_key_by_column_id(Object_id column_id);

  static Object_key *create_primary_key(Object_id column_id, int index);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__COLUMN_TYPE_ELEMENTS_INCLUDED
