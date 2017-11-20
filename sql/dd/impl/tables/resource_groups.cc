/* Copyright (c) 2015, 2017  Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/tables/resource_groups.h"

#include "dd/impl/raw/object_keys.h" // dd::Global_name_key
#include "dd/impl/types/object_table_definition_impl.h" // dd::Raw_record
#include "dd/impl/types/resource_group_impl.h" // dd::Resource_group_impl


namespace dd {
namespace tables {

Resource_groups::Resource_groups()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(1);

  m_target_def.add_field(FIELD_ID, "FIELD_ID",
                         "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
  m_target_def.add_field(FIELD_RESOURCE_GROUP_NAME, "FIELD_RESOURCE_GROUP_NAME",
                         "resource_group_name VARCHAR(64) NOT NULL COLLATE "
                         "utf8_general_ci");
  m_target_def.add_field(FIELD_RESOURCE_GROUP_TYPE, "FIELD_RESOURCE_GROUP_TYPE",
                         "resource_group_type enum('SYSTEM', 'USER') NOT NULL");
  m_target_def.add_field(FIELD_RESOURCE_GROUP_ENABLED,
                         "FIELD_RESOURCE_GROUP_ENABLED",
                         "resource_group_enabled  boolean NOT NULL");
  m_target_def.add_field(FIELD_CPU_ID_MASK, "FIELD_CPU_ID_MASK",
                         "cpu_id_mask VARCHAR(1024) NOT NULL");
  m_target_def.add_field(FIELD_THREAD_PRIORITY, "FIELD_THREAD_PRIORITY",
                         "thread_priority int NOT NULL");

  m_target_def.add_index("PRIMARY KEY(id)");
  m_target_def.add_index("UNIQUE KEY (resource_group_name)");
}

const Resource_groups &Resource_groups::instance()
{
  static Resource_groups *s_instance= new Resource_groups();
  return *s_instance;
}

bool Resource_groups::update_object_key(Global_name_key *key,
                                        const String_type &name)
{
  key->update(FIELD_RESOURCE_GROUP_NAME, name);
  return false;
}

} // tables
} // dd
