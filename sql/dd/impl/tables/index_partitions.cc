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

#include "sql/dd/impl/tables/index_partitions.h"

#include <new>

#include "sql/dd/impl/raw/object_keys.h" // dd::Parent_id_range_key
#include "sql/dd/impl/types/object_table_definition_impl.h"

namespace dd {
namespace tables {

const Index_partitions &Index_partitions::instance()
{
  static Index_partitions *s_instance= new Index_partitions();
  return *s_instance;
}

///////////////////////////////////////////////////////////////////////////

Index_partitions::Index_partitions()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

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
}

///////////////////////////////////////////////////////////////////////////

Object_key *Index_partitions::create_key_by_partition_id(
                                    Object_id partition_id)
{
  return new (std::nothrow) Parent_id_range_key(0, FIELD_PARTITION_ID,
                                               partition_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *Index_partitions::create_primary_key(
  Object_id partition_id, Object_id index_id)
{
  const int INDEX_NO= 0;

  return new (std::nothrow) Composite_pk(INDEX_NO,
                          FIELD_PARTITION_ID, partition_id,
                          FIELD_INDEX_ID, index_id);
}

///////////////////////////////////////////////////////////////////////////

}
}
