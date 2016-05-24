/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_CACHE__SHARED_DICTIONARY_CACHE_INCLUDED
#define DD_CACHE__SHARED_DICTIONARY_CACHE_INCLUDED

#include "my_global.h"                      // DBUG_ASSERT() etc.
#include "handler.h"                        // enum_tx_isolation
#include "shared_multi_map.h"               // Shared_multi_map

#include "dd/types/charset.h"               // Charset
#include "dd/types/collation.h"             // Collation
#include "dd/types/event.h"                 // Event
#include "dd/types/routine.h"               // Routine
#include "dd/types/schema.h"                // Schema
#include "dd/types/table.h"                 // Table
#include "dd/types/tablespace.h"            // Tablespace

namespace dd {
namespace cache {


/**
  Shared dictionary cache containing several maps.

  The dictionary cache is mainly a collection of shared maps for the
  object types supported. The functions dispatch to the appropriate
  map based on the key and object type parameter. Cache misses are handled
  by retrieving the object from the storage adapter singleton.

  The shared dictionary cache itself does not handle concurrency at this
  outer layer. Concurrency is handled by the various instances of the
  shared multi map.
*/

class Shared_dictionary_cache
{
private:
  // We have 223 collations and 41 character sets after initializing
  // the server, as of MySQL 5.8.0.
  static const size_t collation_capacity= 256;
  static const size_t charset_capacity= 64;
  static const size_t event_capacity= 256;

  Shared_multi_map<Abstract_table> m_abstract_table_map;
  Shared_multi_map<Charset>        m_charset_map;
  Shared_multi_map<Collation>      m_collation_map;
  Shared_multi_map<Event>          m_event_map;
  Shared_multi_map<Routine>        m_routine_map;
  Shared_multi_map<Schema>         m_schema_map;
  Shared_multi_map<Tablespace>     m_tablespace_map;

  template <typename T> struct Type_selector { }; // Dummy type to use for
                                                  // selecting map instance.

  /**
    Overloaded functions to use for selecting map instance based
    on a key type. Const and non-const variants.
  */

  Shared_multi_map<Abstract_table> *m_map(Type_selector<Abstract_table>)
  { return &m_abstract_table_map; }
  Shared_multi_map<Charset>        *m_map(Type_selector<Charset>)
  { return &m_charset_map; }
  Shared_multi_map<Collation>      *m_map(Type_selector<Collation>)
  { return &m_collation_map; }
  Shared_multi_map<Event>        *m_map(Type_selector<Event>)
  { return &m_event_map; }
  Shared_multi_map<Routine>        *m_map(Type_selector<Routine>)
  { return &m_routine_map; }
  Shared_multi_map<Schema>         *m_map(Type_selector<Schema>)
  { return &m_schema_map; }
  Shared_multi_map<Tablespace>     *m_map(Type_selector<Tablespace>)
  { return &m_tablespace_map; }


  const Shared_multi_map<Abstract_table> *m_map(Type_selector<Abstract_table>) const
  { return &m_abstract_table_map; }
  const Shared_multi_map<Charset>        *m_map(Type_selector<Charset>) const
  { return &m_charset_map; }
  const Shared_multi_map<Collation>      *m_map(Type_selector<Collation>) const
  { return &m_collation_map; }
  const Shared_multi_map<Schema>         *m_map(Type_selector<Schema>) const
  { return &m_schema_map; }
  const Shared_multi_map<Tablespace>     *m_map(Type_selector<Tablespace>) const
  { return &m_tablespace_map; }


  /**
    Template function to get a map instance.

    To support generic code, the map instances are available through
    template function instances. This allows looking up the
    appropriate instance based on the key type. We must use
    overloading to accomplish this (see above). Const and non-const
    variants.

    @tparam  T  Dictionary object type.

    @return  The shared map handling objects of type T.
  */

  template <typename T>
  Shared_multi_map<T> *m_map()
  { return m_map(Type_selector<T>()); }


  template <typename T>
  const Shared_multi_map<T> *m_map() const
  { return m_map(Type_selector<T>()); }


  Shared_dictionary_cache()
  { }

public:
  static Shared_dictionary_cache *instance()
  {
    static Shared_dictionary_cache s_cache;
    return &s_cache;
  }


  // Set capacity of the shared maps.
  static void init()
  {
    instance()->m_map<Collation>()->set_capacity(collation_capacity);
    instance()->m_map<Charset>()->set_capacity(charset_capacity);

    // Set capacity to have room for all connections to leave an element
    // unused in the cache to avoid frequent cache misses while e.g.
    // opening a table.
    instance()->m_map<Abstract_table>()->set_capacity(max_connections);
    instance()->m_map<Event>()->set_capacity(event_capacity);
    instance()->m_map<Routine>()->set_capacity(stored_program_def_size);
    instance()->m_map<Schema>()->set_capacity(schema_def_size);
    instance()->m_map<Tablespace>()->set_capacity(tablespace_def_size);
  }


  // Shutdown the shared maps.
  static void shutdown()
  {
    instance()->m_map<Abstract_table>()->shutdown();
    instance()->m_map<Collation>()->shutdown();
    instance()->m_map<Charset>()->shutdown();
    instance()->m_map<Event>()->shutdown();
    instance()->m_map<Routine>()->shutdown();
    instance()->m_map<Schema>()->shutdown();
    instance()->m_map<Tablespace>()->shutdown();
  }


  /**
    Get an element from the cache, given the key.

    The operation retrieves an element by one of its keys from the cache
    (possibly involving a cache miss, which will need the thd to handle the
    miss) and returns it through the parameter. If there is no element for
    the given key, NULL is returned. The cache owns the returned element,
    i.e., the caller must not delete it. After using the element, release()
    must be called for every element received via get(). The reference
    counter for the element is incremented if the element is retrieved from
    the shared cache.

    @tparam      K       Key type.
    @tparam      T       Dictionary object type.
    @param       thd     Thread context.
    @param       key     Key to use for looking up the object.
    @param [out] element Element pointer, if present. NULL if not present.

    @retval      false   No error.
    @retval      true    Error (from handling a cache miss).
  */

  template <typename K, typename T>
  bool get(THD *thd, const K &key, Cache_element<T> **element);


  /**
    Read an object directly from disk, given the key.

    The operation retrieves an object by one of its keys from the persistent
    dd tables. The object is returned without being added to the shared
    cache. The object returned is owned by the caller, who thus becomes
    responsible of deleting it.

    @tparam      K         Key type.
    @tparam      T         Dictionary object type.
    @param       thd       Thread context.
    @param       key       Key to use for looking up the object.
    @param       isolation Isolation level to use.
    @param [out] object    Object pointer, if present. NULL if not present.

    @retval      false   No error.
    @retval      true    Error (from reading from the DD tables).
  */

  template <typename K, typename T>
  bool get_uncached(THD *thd, const K &key,
                    enum_tx_isolation isolation, const T **object) const;


  /**
    Add an object to the shared cache.

    The object may not be present already. The object is added to the cache,
    the use counter of its element wrapper in incremented, and the element
    pointer is returned. The user must release the object afterwards. The
    cache is the owner of the returned element and object.

    @tparam  T        Dictionary object type.
    @param   object   Object pointer to be added. May not be NULL.
    @param   element  Element pointer, if present. NULL if not present.
  */

  template <typename T>
  void put(const T* object, Cache_element<T> **element);


  /**
    Release an element used by a client.

    The element must be present and in use. If the element becomes unused,
    it is added to the free list, which is then rectified to enforce
    its capacity constraints.

    @tparam  T         Dictionary object type.
    @param   e         Element pointer.
  */

  template <typename T>
  void release(Cache_element<T> *e)
  { m_map<T>()->release(e); }


  /**
    Delete an element from the cache.

    This function will remove the element from the cache and delete the
    object pointed to. This means that all keys associated with the element
    will be removed from the maps, and the cache element wrapper will be
    deleted. The object may not be accessed after calling this function.

    @tparam  T         Dictionary object type.
    @param   element   Element pointer.
  */

  template <typename T>
  void drop(Cache_element<T> *element)
  { m_map<T>()->drop(element); }


  /**
    Replace the object and re-create the keys for an element.

    The operation removes the current keys from the internal maps in the
    cache, assigns the new object to the element, generates new keys based
    on the new object, and inserts the new keys into the internal maps in the
    cache. The old object is deleted.

    @tparam  T         Dictionary object type.
    @param   element   Element pointer.
    @param   object    New object to replace the old one.
  */

  template <typename T>
  void replace(Cache_element<T> *element, const T *object)
  { m_map<T>()->replace(element, object); }


  /**
    Alter stickiness of an element.

    @tparam  T         Dictionary object type.
    @param   element   Element pointer.
    @param   sticky    New stickiness to assign.
  */

  template <typename T>
  void set_sticky(Cache_element<T> *element, bool sticky)
  { m_map<T>()->set_sticky(element, sticky); }


  /**
    Debug dump of a shared cache partition to stderr.

    @tparam  T         Dictionary object type.
  */

  template <typename T>
  void dump() const
  {
#ifndef DBUG_OFF
    fprintf(stderr, "================================\n");
    fprintf(stderr, "Shared dictionary cache\n");
    m_map<T>()->dump();
    fprintf(stderr, "================================\n");
#endif
  }
};

} // namespace cache
} // namespace dd

#endif // DD_CACHE__SHARED_DICTIONARY_CACHE_INCLUDED
