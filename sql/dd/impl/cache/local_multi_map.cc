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

#include "dd/cache/local_multi_map.h"

#include <stddef.h>

#include "dd/cache/multi_map_base.h"
#include "my_dbug.h"

#include "cache_element.h"                    // Cache_element
#include "dd/types/abstract_table.h"          // Abstract_table
#include "dd/types/charset.h"                 // Charset
#include "dd/types/collation.h"               // Collation
#include "dd/types/event.h"                   // Event
#include "dd/types/routine.h"                 // Routine
#include "dd/types/schema.h"                  // Schema
#include "dd/types/spatial_reference_system.h"// Spatial_reference_system
#include "dd/types/tablespace.h"              // Tablespace

namespace dd {
namespace cache {


// Put a new element into the map.
template <typename T>
void Local_multi_map<T>::put(Cache_element<T> *element)
{
#ifndef DBUG_OFF
  // The new object instance may not be present in the map.
  Cache_element<T> *e= NULL;
  m_map<const T*>()->get(element->object(), &e);
  DBUG_ASSERT(!e);

  // Get all keys that were created within the element.
  const typename T::id_key_type *id_key= element->id_key();
  const typename T::name_key_type *name_key= element->name_key();
  const typename T::aux_key_type *aux_key= element->aux_key();

  // There must be at least one key.
  DBUG_ASSERT(id_key || name_key || aux_key);

  // None of the keys may exist.
  DBUG_ASSERT(
    (!id_key || !m_map<typename T::id_key_type>()->is_present(*id_key)) &&
    (!name_key || !m_map<typename T::name_key_type>()->is_present(*name_key)) &&
    (!aux_key || !m_map<typename T::aux_key_type>()->is_present(*aux_key)));
#endif

  // Add the keys and the element to the maps.
  Multi_map_base<T>::add_single_element(element);
}


// Remove an element from the map.
template <typename T>
void Local_multi_map<T>::remove(Cache_element<T> *element)
{
#ifndef DBUG_OFF
  // The object must be present.
  Cache_element<T> *e= NULL;
  m_map<const T*>()->get(element->object(), &e);
  DBUG_ASSERT(e);

  // Get all keys that were created within the element.
  const typename T::id_key_type *id_key= element->id_key();
  const typename T::name_key_type *name_key= element->name_key();
  const typename T::aux_key_type *aux_key= element->aux_key();

  // All non-null keys must exist.
  DBUG_ASSERT(
    (!id_key || m_map<typename T::id_key_type>()->is_present(*id_key)) &&
    (!name_key || m_map<typename T::name_key_type>()->is_present(*name_key)) &&
    (!aux_key || m_map<typename T::aux_key_type>()->is_present(*aux_key)));
#endif

  // Remove the keys and the element from the maps.
  Multi_map_base<T>::remove_single_element(element);
}


// Remove and delete all elements and objects from the map.
template <typename T>
void Local_multi_map<T>::erase()
{
  typename Multi_map_base<T>::Const_iterator it;
  for (it= begin(); it != end();)
  {
    DBUG_ASSERT(it->second);
    DBUG_ASSERT(it->second->object());

    // Make sure we handle iterator invalidation: Increment
    // before erasing.
    Cache_element<T> *element= it->second;
    ++it;

    // Remove the element from the multi map, delete the wrapped object.
    remove(element);
    delete element->object();
    delete element;
  }
}


// Explicitly instantiate the types for the various usages.
template class Local_multi_map<Abstract_table>;
template class Local_multi_map<Charset>;
template class Local_multi_map<Collation>;
template class Local_multi_map<Event>;
template class Local_multi_map<Routine>;
template class Local_multi_map<Schema>;
template class Local_multi_map<Spatial_reference_system>;
template class Local_multi_map<Tablespace>;

} // namespace cache
} // namespace dd
