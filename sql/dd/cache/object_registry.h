/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD_CACHE__OBJECT_REGISTRY_INCLUDED
#define DD_CACHE__OBJECT_REGISTRY_INCLUDED

#include "local_multi_map.h"                  // Local_multi_map
#include "my_dbug.h"
#include "sql/dd/types/abstract_table.h"      // Abstract_table
#include "sql/dd/types/charset.h"             // Charset
#include "sql/dd/types/collation.h"           // Collation
#include "sql/dd/types/column_statistics.h"   // Column_statistics
#include "sql/dd/types/event.h"               // Event
#include "sql/dd/types/resource_group.h"      // Resource_group
#include "sql/dd/types/routine.h"             // Routine
#include "sql/dd/types/schema.h"              // Schema
#include "sql/dd/types/spatial_reference_system.h"// Spatial_reference_system
#include "sql/dd/types/tablespace.h"          // Tablespace

namespace dd {
namespace cache {


/**
  Object registry containing several maps.

  The registry is mainly a collection of maps for each type supported. The
  functions dispatch to the appropriate map based on the key and object type
  parameter. There is no support for locking or thread synchronization. The
  object registry is kind of the single threaded version of the shared
  dictionary cache.

  The object registry is intended to be used as a thread local record of
  which objects have been used.
*/

class Object_registry
{
private:
  template <typename T> struct Type_selector { }; // Dummy type to use for
                                                  // selecting map instance.

  Local_multi_map<Abstract_table> m_abstract_table_map;
  Local_multi_map<Charset>        m_charset_map;
  Local_multi_map<Collation>      m_collation_map;
  Local_multi_map<Column_statistics> m_column_statistics_map;
  Local_multi_map<Event>          m_event_map;
  Local_multi_map<Resource_group> m_resource_group_map;
  Local_multi_map<Routine>        m_routine_map;
  Local_multi_map<Schema>         m_schema_map;
  Local_multi_map<Spatial_reference_system> m_spatial_reference_system_map;
  Local_multi_map<Tablespace>     m_tablespace_map;


  /**
    Overloaded functions to use for selecting map instance based
    on a key type. Const and non-const variants.
  */

  Local_multi_map<Abstract_table>
    *m_map(Type_selector<Abstract_table>)
  { return &m_abstract_table_map; }

  const Local_multi_map<Abstract_table>
    *m_map(Type_selector<Abstract_table>) const
  { return &m_abstract_table_map; }

  Local_multi_map<Charset>
    *m_map(Type_selector<Charset>)
  { return &m_charset_map; }

  const Local_multi_map<Charset>
    *m_map(Type_selector<Charset>) const
  { return &m_charset_map; }

  Local_multi_map<Collation>
    *m_map(Type_selector<Collation>)
  { return &m_collation_map; }

  const Local_multi_map<Collation>
    *m_map(Type_selector<Collation>) const
  { return &m_collation_map; }

  Local_multi_map<Column_statistics>
    *m_map(Type_selector<Column_statistics>)
  { return &m_column_statistics_map; }

  const Local_multi_map<Column_statistics>
    *m_map(Type_selector<Column_statistics>) const
  { return &m_column_statistics_map; }

  Local_multi_map<Event>
    *m_map(Type_selector<Event>)
  { return &m_event_map; }

  const Local_multi_map<Event>
    *m_map(Type_selector<Event>) const
  { return &m_event_map; }

  Local_multi_map<Resource_group>
    *m_map(Type_selector<Resource_group>)
  { return &m_resource_group_map; }

  const Local_multi_map<Resource_group>
    *m_map(Type_selector<Resource_group>) const
  { return &m_resource_group_map; }

  Local_multi_map<Routine>
    *m_map(Type_selector<Routine>)
  { return &m_routine_map; }

  const Local_multi_map<Routine>
    *m_map(Type_selector<Routine>) const
  { return &m_routine_map; }

  Local_multi_map<Schema>
    *m_map(Type_selector<Schema>)
  { return &m_schema_map; }

  const Local_multi_map<Schema>
    *m_map(Type_selector<Schema>) const
  { return &m_schema_map; }

  Local_multi_map<Spatial_reference_system>
    *m_map(Type_selector<Spatial_reference_system>)
  { return &m_spatial_reference_system_map; }

  const Local_multi_map<Spatial_reference_system>
    *m_map(Type_selector<Spatial_reference_system>) const
  { return &m_spatial_reference_system_map; }

  Local_multi_map<Tablespace>
    *m_map(Type_selector<Tablespace>)
  { return &m_tablespace_map; }

  const Local_multi_map<Tablespace>
    *m_map(Type_selector<Tablespace>) const
  { return &m_tablespace_map; }

  /**
    Template function to get a map instance.

    To support generic code, the map instances are available through
    template function instances. This allows looking up the appropriate
    instance based on the key type. We must use overloading to accomplish
    this (see above). Const and non-const variants.

    @tparam T Dictionary object type.

    @return The map handling objects of type T.
   */

  template <typename T>
  Local_multi_map<T> *m_map()
  { return m_map(Type_selector<T>()); }

  template <typename T>
  const Local_multi_map<T> *m_map() const
  { return m_map(Type_selector<T>()); }

public:


  /**
    Get an iterator to the beginning of the local reverse map.

    The reverse map is guaranteed to contain all elements, that why we
    use it for iteration. The other maps may not contain all elements
    since keys may be NULL.

    Const and non-const variants.

    @tparam T Dictionary object type.

    @return Iterator to the beginning of the local reverse map.
  */

  template <typename T>
  typename Multi_map_base<T>::Const_iterator begin() const
  { return m_map<T>()->begin(); }

  template <typename T>
  typename Multi_map_base<T>::Iterator begin()
  { return m_map<T>()->begin(); }


  /**
    Get an iterator to one past the end of the local reverse map.

    The reverse map is guaranteed to contain all elements, that why we
    use it for iteration. The other maps may not contain all elements
    since keys may be NULL.

    Const and non-const variants.

    @tparam T Dictionary object type.

    @return Iterator to one past the end of the local reverse map.
  */

  template <typename T>
  typename Multi_map_base<T>::Const_iterator end() const
  { return m_map<T>()->end(); }

  template <typename T>
  typename Multi_map_base<T>::Iterator end()
  { return m_map<T>()->end(); }


  /**
    Get the element corresponding to the given key.

    @tparam      K        Key type.
    @tparam      T        Dictionary object type.
    @param       key      Key too lookup.
    @param [out] element  Element, if present, otherwise, NULL.
  */

  template <typename K, typename T>
  void get(const K &key, Cache_element<T> **element) const
  { m_map<T>()->get(key, element); }


  /**
    Add a new element to the registry.

    @tparam  T        Dictionary object type.
    @param   element  Element to be added.
  */

  template <typename T>
  void put(Cache_element<T> *element)
  { m_map<T>()->put(element); }


  /**
    Remove an element from the registry.

    @tparam  T        Dictionary object type.
    @param   element  Element to be removed.
  */

  template <typename T>
  void remove(Cache_element<T> *element)
  { m_map<T>()->remove(element); }


  /**
    Remove and delete all objects of a given type from the registry.

    @tparam  T        Dictionary object type.
  */

  template <typename T>
  void erase()
  {
    m_map<T>()->erase();
  }


  /**
    Remove and delete all objects from the registry.
  */

  void erase_all()
  {
    m_abstract_table_map.erase();
    m_charset_map.erase();
    m_collation_map.erase();
    m_event_map.erase();
    m_routine_map.erase();
    m_schema_map.erase();
    m_spatial_reference_system_map.erase();
    m_tablespace_map.erase();
  }


  /**
    Get the number of objects of a given type in the registry.

    @tparam  T        Dictionary object type.
    @return  Number of objects.
  */

  template <typename T>
  size_t size() const
  {
    return m_map<T>()->size();
  }


  /**
    Get the total number of objects in the registry.

    @return  Number of objects.
  */

  size_t size_all() const
  {
    return m_abstract_table_map.size() +
      m_charset_map.size() +
      m_collation_map.size() +
      m_event_map.size() +
      m_routine_map.size() +
      m_schema_map.size() +
      m_spatial_reference_system_map.size() +
      m_tablespace_map.size();
  }


  /**
    Debug dump of the object registry to stderr.

    @tparam      T        Dictionary object type.
  */

  /* purecov: begin inspected */
  template <typename T>
  void dump() const
  {
#ifndef DBUG_OFF
    m_map<T>()->dump();
#endif
  }
  /* purecov: end */
};

} // namespace cache
} // namespace dd

#endif // DD_CACHE__OBJECT_REGISTRY_INCLUDED
