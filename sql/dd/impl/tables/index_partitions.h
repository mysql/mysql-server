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

#ifndef DD_TABLES__INDEX_PARTITIONS_INCLUDED
#define DD_TABLES__INDEX_PARTITIONS_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"                    // dd::Object_id
#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl

namespace dd {
  class Object_key;
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Index_partitions : virtual public Object_table_impl
{
public:
  static const Index_partitions &instance()
  {
    static Index_partitions s_instance;
    return s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("index_partitions");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_PARTITION_ID,
    FIELD_INDEX_ID,
    FIELD_OPTIONS,
    FIELD_SE_PRIVATE_DATA,
    FIELD_TABLESPACE_ID
  };

public:
  Index_partitions()
  {
    m_target_def.table_name(table_name());

    m_target_def.add_field(FIELD_PARTITION_ID,
                           "FIELD_PARTITION_ID",
                           "partition_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_INDEX_ID,
                           "FIELD_INDEX_ID",
                           "index_id BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(FIELD_OPTIONS,
                           "FIELD_OPTIONS",
                           "options MEDIUMTEXT");
    m_target_def.add_field(FIELD_SE_PRIVATE_DATA,
                           "FIELD_SE_PRIVATE_DATA",
                           "se_private_data MEDIUMTEXT");
    m_target_def.add_field(FIELD_TABLESPACE_ID,
                           "FIELD_TABLESPACE_ID",
                           "tablespace_id BIGINT UNSIGNED");

    m_target_def.add_index("PRIMARY KEY(partition_id, index_id)");

    m_target_def.add_foreign_key("FOREIGN KEY (partition_id) REFERENCES "
                                 "table_partitions(id)");
    m_target_def.add_foreign_key("FOREIGN KEY (index_id) REFERENCES "
                                 "indexes(id)");
    m_target_def.add_foreign_key("FOREIGN KEY (tablespace_id) REFERENCES "
                                 "tablespaces(id)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return Index_partitions::table_name(); }

public:
  static Object_key *create_key_by_partition_id(Object_id partition_id);

  static Object_key *create_primary_key(
    Object_id partition_id, Object_id index_id);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__INDEX_PARTITIONS_INCLUDED
