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

#ifndef DD_TABLES__FOREIGN_KEY_COLUMN_USAGE_INCLUDED
#define DD_TABLES__FOREIGN_KEY_COLUMN_USAGE_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"                    // dd::Object_id
#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl

namespace dd {
  class Object_key;
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Foreign_key_column_usage : public Object_table_impl
{
public:
  static const Foreign_key_column_usage &instance()
  {
    static Foreign_key_column_usage *s_instance= new Foreign_key_column_usage();
    return *s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("foreign_key_column_usage");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_FOREIGN_KEY_ID,
    FIELD_ORDINAL_POSITION,
    FIELD_COLUMN_ID,
    FIELD_REFERENCED_COLUMN_NAME
  };

public:
  Foreign_key_column_usage()
  {
    m_target_def.table_name(table_name());
    m_target_def.dd_version(1);

    m_target_def.add_field(FIELD_FOREIGN_KEY_ID,
                           "FIELD_FOREIGN_KEY_ID",
                           "foreign_key_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_ORDINAL_POSITION,
                           "FIELD_ORDINAL_POSITION",
                           "ordinal_position INT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_COLUMN_ID,
                           "FIELD_COLUMN_ID",
                           "column_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_REFERENCED_COLUMN_NAME,
                           "FIELD_REFERENCED_COLUMN_NAME",
                           "referenced_column_name VARCHAR(64) NOT NULL "
                           "COLLATE utf8_tolower_ci");

    m_target_def.add_index("UNIQUE KEY(foreign_key_id, column_id, "
                           "referenced_column_name)");
    m_target_def.add_index("UNIQUE KEY(foreign_key_id, ordinal_position)");

    m_target_def.add_foreign_key("FOREIGN KEY (foreign_key_id) REFERENCES "
                                 "foreign_keys(id)");
    m_target_def.add_foreign_key("FOREIGN KEY (column_id) REFERENCES "
                                 "columns(id)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("ROW_FORMAT=DYNAMIC");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return Foreign_key_column_usage::table_name(); }

public:
  static Object_key *create_key_by_foreign_key_id(Object_id fk_id);

  static Object_key *create_primary_key(
    Object_id fk_id, int ordinal_position);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__FOREIGN_KEY_COLUMN_USAGE_INCLUDED
