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

#include "dd/impl/types/resource_group_impl.h"

#include "dd/impl/raw/object_keys.h"        // Primary_id_keys
#include "dd/impl/raw/raw_record.h"         // Raw_record
#include "dd/impl/tables/resource_groups.h" // Resource_groups
#include "dd/impl/transaction_impl.h"       // Open_dictionary_tables_ctx

using dd::tables::Resource_groups;

namespace dd {

// Resource_group implementation

const Entity_object_table &Resource_group::OBJECT_TABLE()
{
  return Resource_groups::instance();
}

const Object_type &Resource_group::TYPE()
{
  static Resource_group_type s_instance;
  return s_instance;
}

// Resource_group_impl implementation

Resource_group_impl::Resource_group_impl()
  : m_resource_group_name(""),
    m_type(resourcegroups::Type::SYSTEM_RESOURCE_GROUP),
    m_enabled(false),
    m_thread_priority(0)
{}

Resource_group_impl::Resource_group_impl(const Resource_group_impl &src)
  : Weak_object(src), Entity_object_impl(src),
    m_resource_group_name(src.m_resource_group_name),
    m_type(src.m_type),
    m_enabled(src.m_enabled),
    m_cpu_id_mask(src.m_cpu_id_mask),
    m_thread_priority(src.m_thread_priority)
{
}

bool Resource_group_impl::validate() const
{
  return false;
}

/**
  Check if the string contain characters of 0 and 1.

  @returns true if characters of string are either 0 or 1 else false.
*/

static inline bool is_valid_cpu_mask_str(const String_type &str)
{
  for (uint i= 0; i < str.size(); i++)
    if (str[i] != '0' && str[i] != '1')
      return false;
  return true;
}


bool Resource_group_impl::restore_attributes(const Raw_record &r)
{
  restore_id(r, Resource_groups::FIELD_ID);
  restore_name(r, Resource_groups::FIELD_RESOURCE_GROUP_NAME);

  m_type= static_cast<resourcegroups::Type>
          (r.read_int(Resource_groups::FIELD_RESOURCE_GROUP_TYPE));

  m_enabled= r.read_bool(Resource_groups::FIELD_RESOURCE_GROUP_ENABLED);

  // convert bitmap values.
  String_type cpu_id_mask_str= r.read_str(Resource_groups::FIELD_CPU_ID_MASK);

  if (cpu_id_mask_str.size() > CPU_MASK_SIZE ||
      !is_valid_cpu_mask_str(cpu_id_mask_str))
    return true;

  m_cpu_id_mask= std::bitset<CPU_MASK_SIZE>(cpu_id_mask_str);

  m_thread_priority= r.read_int(Resource_groups::FIELD_THREAD_PRIORITY);

  return false;
}

bool Resource_group_impl::store_attributes(Raw_record *r)
{
  return store_id(r, Resource_groups::FIELD_ID) ||
         store_name(r, Resource_groups::FIELD_RESOURCE_GROUP_NAME) ||
         r->store(Resource_groups::FIELD_RESOURCE_GROUP_TYPE,
                  static_cast<int>(m_type)) ||
         r->store(Resource_groups::FIELD_RESOURCE_GROUP_ENABLED,
                  m_enabled) ||
         r->store(Resource_groups::FIELD_CPU_ID_MASK,
                  String_type(m_cpu_id_mask.to_string().c_str())) ||
         r->store(Resource_groups::FIELD_THREAD_PRIORITY, m_thread_priority);
}

void Resource_group_impl::debug_print(String_type &outb) const
{
  dd::Stringstream_type ss;
  ss
  << "id: {OID: " << id() << "}; "
  << "Resource group name: " << m_resource_group_name << ";"
  << "CPU ID Mask: " << m_cpu_id_mask.to_string() << ";"
  << "Resource group type: " << (int) m_type << " ; "
  << "Thread priority: " << m_thread_priority << "; ]";

  outb= ss.str();
}

// Resource group type implementation
void Resource_group_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Resource_groups>();
}

bool Resource_group::update_id_key(id_key_type *key, Object_id id)
{
  key->update(id);
  return false;
}

bool Resource_group::update_name_key(name_key_type *key, const String_type &name)
{
  // Resource group names are case insensitive
  char lc_name[NAME_LEN + 1];
  my_stpncpy(lc_name, name.c_str(), NAME_LEN);
  my_casedn_str(system_charset_info, lc_name);
  lc_name[NAME_LEN]= '\0';
  return Resource_groups::update_object_key(key, lc_name);
}

} // dd
