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

#ifndef DD_TABLES__INDEXES_INCLUDED
#define DD_TABLES__INDEXES_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"                    // dd::Object_id
#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl

namespace dd {
  class Object_key;
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Indexes : virtual public Object_table_impl
{
public:
  static const Indexes &instance()
  {
    static Indexes s_instance;
    return s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("indexes");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_ID,
    FIELD_TABLE_ID,
    FIELD_NAME,
    FIELD_TYPE,
    FIELD_ALGORITHM,
    FIELD_IS_ALGORITHM_EXPLICIT,
    FIELD_IS_GENERATED,
    FIELD_HIDDEN,
    FIELD_ORDINAL_POSITION,
    FIELD_COMMENT,
    FIELD_OPTIONS,
    FIELD_SE_PRIVATE_DATA,
    FIELD_TABLESPACE_ID,
    FIELD_ENGINE
  };

public:
  Indexes()
  {
    m_target_def.table_name(table_name());

    m_target_def.add_field(FIELD_ID,
                           "FIELD_ID",
                           "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
    m_target_def.add_field(FIELD_TABLE_ID,
                           "FIELD_TABLE_ID",
                           "table_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_NAME,
                           "FIELD_NAME",
                           "name VARCHAR(64) NOT NULL COLLATE utf8_tolower_ci");
    m_target_def.add_field(FIELD_TYPE,
                           "FIELD_TYPE",
                           "type ENUM(\n"
                           "  'PRIMARY',\n"
                           "  'UNIQUE',\n"
                           "  'MULTIPLE',\n"
                           "  'FULLTEXT',\n"
                           "  'SPATIAL'\n"
                           ") NOT NULL");
    m_target_def.add_field(FIELD_ALGORITHM,
                           "FIELD_ALGORITHM",
                           "algorithm ENUM(\n"
                           "  'SE_SPECIFIC',\n"
                           "  'BTREE',\n"
                           "  'RTREE',\n"
                           "  'HASH',\n"
                           "  'FULLTEXT'\n"
                           ") NOT NULL");
    m_target_def.add_field(FIELD_IS_ALGORITHM_EXPLICIT,
                           "FIELD_IS_ALGORITHM_EXPLICIT",
                           "is_algorithm_explicit BOOL NOT NULL");
    m_target_def.add_field(FIELD_IS_GENERATED,
                           "FIELD_IS_GENERATED",
                           "is_generated BOOL NOT NULL");
    m_target_def.add_field(FIELD_HIDDEN,
                           "FIELD_HIDDEN",
                           "hidden BOOL NOT NULL");
    m_target_def.add_field(FIELD_ORDINAL_POSITION,
                           "FIELD_ORDINAL_POSITION",
                           "ordinal_position INT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_COMMENT,
                           "FIELD_COMMENT",
                           "comment VARCHAR(2048) NOT NULL");
    m_target_def.add_field(FIELD_OPTIONS,
                           "FIELD_OPTIONS",
                           "options MEDIUMTEXT");
    m_target_def.add_field(FIELD_SE_PRIVATE_DATA,
                           "FIELD_SE_PRIVATE_DATA",
                           "se_private_data MEDIUMTEXT");
    m_target_def.add_field(FIELD_TABLESPACE_ID,
                           "FIELD_TABLESPACE_ID",
                           "tablespace_id BIGINT UNSIGNED");
    m_target_def.add_field(FIELD_ENGINE,
                           "FIELD_ENGINE",
                           "engine VARCHAR(64)");

    m_target_def.add_index("PRIMARY KEY(id)");
    m_target_def.add_index("UNIQUE KEY(table_id, name)");

    m_target_def.add_foreign_key("FOREIGN KEY (table_id) REFERENCES "
                                 "tables(id)");
    m_target_def.add_foreign_key("FOREIGN KEY (tablespace_id) REFERENCES "
                                 "tablespaces(id)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return Indexes::table_name(); }

public:
  static Object_key *create_key_by_table_id(Object_id table_id);

};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__INDEXES_INCLUDED
