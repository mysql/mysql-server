/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__TABLE_PARTITIONS_INCLUDED
#define DD_TABLES__TABLE_PARTITIONS_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"                    // dd::Object_id
#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl

namespace dd {
  class Object_key;
  class Raw_record;
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Table_partitions : virtual public Object_table_impl
{
public:
  static const Table_partitions &instance()
  {
    static Table_partitions s_instance;
    return s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("table_partitions");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_ID,
    FIELD_TABLE_ID,
    FIELD_LEVEL,
    FIELD_NUMBER,
    FIELD_NAME,
    FIELD_ENGINE,
    FIELD_COMMENT,
    FIELD_OPTIONS,
    FIELD_SE_PRIVATE_DATA,
    FIELD_SE_PRIVATE_ID,
    FIELD_TABLESPACE_ID
  };

public:
  Table_partitions()
  {
    m_target_def.table_name(table_name());

    m_target_def.add_field(FIELD_ID,
                           "FIELD_ID",
                           "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
    m_target_def.add_field(FIELD_TABLE_ID,
                           "FIELD_TABLE_ID",
                           "table_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_LEVEL,
                           "FIELD_LEVEL",
                           "level TINYINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_NUMBER,
                           "FIELD_NUMBER",
                           "number SMALLINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_NAME,
                           "FIELD_NAME",
                           "name VARCHAR(64) NOT NULL COLLATE utf8_tolower_ci");
    m_target_def.add_field(FIELD_ENGINE,
                           "FIELD_ENGINE",
                           "engine VARCHAR(64)");
    m_target_def.add_field(FIELD_COMMENT,
                           "FIELD_COMMENT",
                           "comment VARCHAR(2048) NOT NULL");
    m_target_def.add_field(FIELD_OPTIONS,
                           "FIELD_OPTIONS",
                           "options MEDIUMTEXT");
    m_target_def.add_field(FIELD_SE_PRIVATE_DATA,
                           "FIELD_SE_PRIVATE_DATA",
                           "se_private_data MEDIUMTEXT");
    m_target_def.add_field(FIELD_SE_PRIVATE_ID,
                           "FIELD_SE_PRIVATE_ID",
                           "se_private_id BIGINT UNSIGNED");
    m_target_def.add_field(FIELD_TABLESPACE_ID,
                           "FIELD_TABLESPACE_ID",
                           "tablespace_id BIGINT UNSIGNED");

    m_target_def.add_index("PRIMARY KEY(id)");
    m_target_def.add_index("UNIQUE KEY(table_id, name)");
    m_target_def.add_index("UNIQUE KEY(table_id, level, number)");
    m_target_def.add_index("UNIQUE KEY(engine, se_private_id)");
    m_target_def.add_index("KEY(engine)");

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
  { return Table_partitions::table_name(); }

public:
  static Object_key *create_key_by_table_id(Object_id table_id);

  static ulonglong read_table_id(const Raw_record &r);

  static Object_key *create_se_private_key(
    const std::string &engine,
    ulonglong se_private_id);

  static bool get_partition_table_id(
    THD *thd,
    const std::string &engine,
    ulonglong se_private_id,
    Object_id *oid);

};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__TABLE_PARTITIONS_INCLUDED
