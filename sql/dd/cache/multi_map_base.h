/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_CACHE__MULTI_MAP_BASE_INCLUDED
#define DD_CACHE__MULTI_MAP_BASE_INCLUDED

#include <stdio.h>

#include "dd/types/abstract_table.h"
#include "dd/types/charset.h"
#include "dd/types/collation.h"
#include "dd/types/event.h"
#include "dd/types/routine.h"
#include "dd/types/schema.h"
#include "dd/types/spatial_reference_system.h"
#include "dd/types/tablespace.h"
#include "element_map.h"                      // Element_map
#include "my_dbug.h"

namespace dd {
namespace cache {

template <typename T>
class Cache_element;


/**
  Implementation of a set of maps for a given object type.

  The class declares a set of maps, each of which maps from a key type
  to an element type. The element type wraps the template object type
  parameter into a wrapper instance.

  The implementation is intended to be used as a base to be extended for
  usage in a specific context. There is support for adding and removing
  elements in all maps with one operation (but not necessarily atomically),
  and for retrieving a single map. There is no support for tracking object
  usage, free list management, thread synchronization, etc.

  @tparam  T  Dictionary object type.
*/

template <typename T>
class Multi_map_base
{
private:
  Element_map<const T*, Cache_element<T> > m_rev_map;   // Reverse element map.

  Element_map<typename T::id_key_type, Cache_element<T> >
                                          m_id_map;     // Id map instance.
  Element_map<typename T::name_key_type, Cache_element<T> >
                                           m_name_map;  // Name map instance.
  Element_map<typename T::aux_key_type, Cache_element<T> >
                                          m_aux_map;    // Aux map instance.

  template <typename K> struct Type_selector { }; // Dummy type to use for
                                                  // selecting map instance.


  /**
    Overloaded functions to use for selecting an element list instance
    based on a key type. Const and non-const variants.
  */

  Element_map<const T*, Cache_element<T> >
    *m_map(Type_selector<const T*>)
  { return &m_rev_map; }

  const Element_map<const T*, Cache_element<T> >
    *m_map(Type_selector<const T*>) const
  { return &m_rev_map; }

  Element_map<typename T::id_key_type, Cache_element<T> >
    *m_map(Type_selector<typename T::id_key_type>)
  { return &m_id_map; }

  const Element_map<typename T::id_key_type, Cache_element<T> >
    *m_map(Type_selector<typename T::id_key_type>) const
  { return &m_id_map; }

  Element_map<typename T::name_key_type, Cache_element<T> >
    *m_map(Type_selector<typename T::name_key_type>)
  { return &m_name_map; }

  const Element_map<typename T::name_key_type, Cache_element<T> >
    *m_map(Type_selector<typename T::name_key_type>) const
  { return &m_name_map; }

  Element_map<typename T::aux_key_type, Cache_element<T> >
    *m_map(Type_selector<typename T::aux_key_type>)
  { return &m_aux_map; }

  const Element_map<typename T::aux_key_type, Cache_element<T> >
    *m_map(Type_selector<typename T::aux_key_type>) const
  { return &m_aux_map; }

  public:

  // Iterate based on the reverse map where all elements must be present.
  typedef typename Element_map<const T*,
                     Cache_element<T> >::Const_iterator Const_iterator;

  typedef typename Element_map<const T*,
                     Cache_element<T> >::Iterator Iterator;

protected:


  /**
    Template function to get an element map.

    To support generic code, the element map instances are available
    through template function instances. This allows looking up the
    appropriate instance based on the key type. We must use overloading
    to accomplish this (see above). Const and non-const variants.

    @tparam K Key type.

    @return The element map handling keys of type K.
   */

  template <typename K>
  Element_map<K, Cache_element<T> > *m_map()
  { return m_map(Type_selector<K>()); }

  template <typename K>
  const Element_map<K, Cache_element<T> > *m_map() const
  { return m_map(Type_selector<K>()); }


  /**
    Helper function to remove the mapping of a single element, without
    deleting the element itself. This function assumes that checking for
    key and element presence has already been done.

    @param element  Element to be removed and deleted.
  */

  void remove_single_element(Cache_element<T> *element);


  /**
    Helper function to add a single element.

    This function assumes that checking for key and element presence
    has already been done, that the object has been assigned, and that the
    keys have been generated.

    @param element  Element to be added.
  */

  void add_single_element(Cache_element<T> *element);


  /**
    Debug dump of the multi map base to stderr.
  */

  /* purecov: begin inspected */
  void dump() const
  {
#ifndef DBUG_OFF
    fprintf(stderr, "    Reverse element map:\n");
    m_map<const T*>()->dump();
    fprintf(stderr, "    Id map:\n");
    m_map<typename T::id_key_type>()->dump();
    fprintf(stderr, "    Name map:\n");
    m_map<typename T::name_key_type>()->dump();
    fprintf(stderr, "    Aux map:\n");
    m_map<typename T::aux_key_type>()->dump();
#endif
  }
  /* purecov: end */
};

} // namespace cache
} // namespace dd

#endif // DD_CACHE__MULTI_MAP_BASE_INCLUDED
