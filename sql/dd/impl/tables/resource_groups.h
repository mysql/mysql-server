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

#ifndef DD_TABLES__RESOURCE_GROUPS_INCLUDED
#define DD_TABLES__RESOURCE_GROUPS_INCLUDED

#include <new>
#include <string>

#include "dd/impl/raw/object_keys.h"
#include "dd/impl/types/resource_group_impl.h"
#include "dd/impl/types/entity_object_table_impl.h"   // dd::Object_table_i...
#include "dd/types/resource_group.h"

namespace dd
{

class Raw_record;

namespace tables
{
class Resource_groups : virtual public Entity_object_table_impl
{
public:
  Resource_groups();

  static const Resource_groups &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("resource_groups");
    return s_table_name;
  }

  enum enum_fields
  {
    FIELD_ID,
    FIELD_RESOURCE_GROUP_NAME,
    FIELD_RESOURCE_GROUP_TYPE,
    FIELD_RESOURCE_GROUP_ENABLED,
    FIELD_CPU_ID_MASK,
    FIELD_THREAD_PRIORITY
  };

public:

  const String_type &name() const override
  { return Resource_groups::table_name(); }

  Resource_group *create_entity_object(const Raw_record &) const override
  { return new (std::nothrow) Resource_group_impl(); }

  static bool update_object_key(Global_name_key *key,
                                const String_type &resource_group_name);
};
} // namespace tables
} // namespace dd


#endif // DD_TABLES__RESOURCE_GROUPS_INCLUDED
