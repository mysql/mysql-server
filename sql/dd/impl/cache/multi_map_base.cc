/* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/cache/multi_map_base.h"

#include "cache_element.h"                    // Cache_element
#include "dd/types/abstract_table.h"          // Abstract_table
#include "dd/types/charset.h"                 // Charset
#include "dd/types/collation.h"               // Collation
#include "dd/types/event.h"                   // Event
#include "dd/types/routine.h"                 // Routine
#include "dd/types/schema.h"                  // Schema
#include "dd/types/spatial_reference_system.h"// Spatial_reference_system
#include "dd/types/tablespace.h"              // Tablespace
#include "my_dbug.h"

namespace dd {
namespace cache {


// Helper function to remove the mapping of a single element.
template <typename T>
void Multi_map_base<T>::remove_single_element(Cache_element<T> *element)
{
  // Remove the element from all maps.
  DBUG_ASSERT(element->object());
  if (element->object())
    m_map<const T*>()->remove(element->object());
  if (element->id_key())
    m_map<typename T::id_key_type>()->remove(*element->id_key());
  if (element->name_key())
    m_map<typename T::name_key_type>()->remove(*element->name_key());
  if (element->aux_key())
    m_map<typename T::aux_key_type>()->remove(*element->aux_key());
}


// Helper function to add a single element.
template <typename T>
void Multi_map_base<T>::add_single_element(Cache_element<T> *element)
{
  // Add the element to all maps.
  DBUG_ASSERT(element->object());
  if (element->object())
    m_map<const T*>()->put(element->object(), element);
  if (element->id_key())
    m_map<typename T::id_key_type>()->put(*element->id_key(), element);
  if (element->name_key())
    m_map<typename T::name_key_type>()->put(*element->name_key(), element);
  if (element->aux_key())
    m_map<typename T::aux_key_type>()->put(*element->aux_key(), element);
}


// Explicitly instantiate the types for the various usages.
template class Multi_map_base<Abstract_table>;
template class Multi_map_base<Charset>;
template class Multi_map_base<Collation>;
template class Multi_map_base<Event>;
template class Multi_map_base<Routine>;
template class Multi_map_base<Schema>;
template class Multi_map_base<Spatial_reference_system>;
template class Multi_map_base<Tablespace>;

} // namespace cache
} // namespace dd
