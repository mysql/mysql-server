/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/system_views/resource_groups.h"

namespace dd {
namespace system_views {

const Resource_groups &Resource_groups::instance()
{
  static Resource_groups *s_instance= new Resource_groups();
  return *s_instance;
}

Resource_groups::Resource_groups()
{
  m_target_def.set_view_name(view_name());

  m_target_def.add_field(FIELD_RESOURCE_GROUP_NAME, "RESOURCE_GROUP_NAME",
                         "res.resource_group_name");
  m_target_def.add_field(FIELD_RESOURCE_GROUP_TYPE, "RESOURCE_GROUP_TYPE",
                         "res.resource_group_type");
  m_target_def.add_field(FIELD_RESOURCE_GROUP_ENABLED, "RESOURCE_GROUP_ENABLED",
                         "res.resource_group_enabled");
  m_target_def.add_field(FIELD_VCPU_IDS, "VCPU_IDS",
                         "CONVERT_CPU_ID_MASK(res.CPU_ID_MASK)");
  m_target_def.add_field(FIELD_THREAD_PRIORITY, "THREAD_PRIORITY",
                         "res.THREAD_PRIORITY");
  m_target_def.add_from("mysql.resource_groups res");
  m_target_def.add_where("CAN_ACCESS_RESOURCE_GROUP(res.resource_group_name)");
}

}
}
