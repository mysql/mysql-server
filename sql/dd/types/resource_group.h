/* Copyright (c) 2014, 2017 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__RESOURCE_GROUP_INCLUDED
#define DD__RESOURCE_GROUP_INCLUDED

#include <bitset>  // std::bitset
#include <memory>  // std::unique_ptr

#include "sql/dd/types/entity_object_table.h" // dd::Entity_object_table
#include "sql/dd/types/entity_object.h"       // dd::Entity_object
#include "sql/resourcegroups/resource_group_basic_types.h" // Range, Type

namespace dd {

class Object_type;
class Primary_id_key;
class Global_name_key;
class Void_key;

namespace tables {
  class Resource_groups;
}

static constexpr int CPU_MASK_SIZE= 1024;
class Resource_group : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Entity_object_table &OBJECT_TABLE();

  typedef Resource_group cache_partition_type;
  typedef tables::Resource_groups cache_partition_table_type;
  typedef Primary_id_key id_key_type;
  typedef Global_name_key name_key_type;
  typedef Void_key aux_key_type;

public:
  ~Resource_group() override {}

  virtual bool update_id_key(id_key_type *key) const
  { return update_id_key(key, id()); }
  static bool update_id_key(id_key_type *key, Object_id id);

  virtual bool update_name_key(name_key_type *key) const
  { return update_name_key(key, name()); }
  static bool update_name_key(name_key_type *key,
                              const String_type &name);

  virtual bool update_aux_key(aux_key_type *) const
  { return true; }

  virtual const resourcegroups::Type &resource_group_type() const = 0;
  virtual void set_resource_group_type(const resourcegroups::Type &type) = 0;

  virtual bool resource_group_enabled() const = 0;
  virtual void set_resource_group_enabled(bool enabled) = 0;

  virtual const std::bitset<CPU_MASK_SIZE> &cpu_id_mask() const = 0;
  virtual void set_cpu_id_mask(
    const std::vector<resourcegroups::Range>& vcpu_vec) = 0;

  virtual int thread_priority() const = 0;
  virtual void set_thread_priority(int priority) = 0;

  virtual Resource_group *clone() const = 0;
};
} // namespace dd

#endif // DD__RESOURCE_GROUP_INCLUDED
