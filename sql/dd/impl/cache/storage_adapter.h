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

#ifndef DD_CACHE__STORAGE_ADAPTER_INCLUDED
#define DD_CACHE__STORAGE_ADAPTER_INCLUDED

#include "my_global.h"                       // DBUG_ASSERT() etc.
#include "handler.h"                         // enum_tx_isolation
#include "dd/impl/types/entity_object_impl.h" // set_id()

#ifndef DBUG_OFF
// Only needed for unit testing
#include "sql_class.h"                       // THD
#include "mysqld_error.h"                    // my_error
#include "mysql/psi/mysql_thread.h"          // mysql_mutex_t, mysql_cond_t
#include "dd/cache/object_registry.h"        // Object_registry
#include "dd/impl/cache/cache_element.h"     // Cache_element
#endif /* DBUG_OFF */

class THD;

namespace dd_cache_unittest {
  class CacheStorageTest;
}

namespace dd {
namespace cache {


/**
  Handling of access to persistent storage.

  This class provides static template functions that manipulates an object on
  persistent storage based on the submitted key and object type.
*/

#ifndef DBUG_OFF
// Simulate auto increment column in unit tests
static Object_id next_oid= 10000;
#endif /* !DBUG_OFF */

class Storage_adapter
{
#ifndef DBUG_OFF
friend class dd_cache_unittest::CacheStorageTest;
private:
  Object_registry m_storage;               // Fake storage.
  mysql_mutex_t m_lock;                    // Single mutex to sync threads.
  static bool s_use_fake_storage;          // Whether to use the fake adapter.

  Storage_adapter()
  { mysql_mutex_init(PSI_NOT_INSTRUMENTED, &m_lock, MY_MUTEX_INIT_FAST); }

  ~Storage_adapter()
  { mysql_mutex_destroy(&m_lock); }

  static Storage_adapter *fake_instance()
  {
    static Storage_adapter s_fake_storage;
    return &s_fake_storage;
  }

  // Fake get for unit testing.
  template <typename K, typename T>
  bool fake_get(THD *thd, const K &key, const T **object)
  {
    Cache_element<typename T::cache_partition_type> *element= NULL;
    bool ret= true;
    mysql_mutex_lock(&m_lock);
    m_storage.get(key, &element);
    if (element)
    {
      // We absolutely need to clone the object here,
      // otherwise there will be only one copy present, and
      // e.g. evicting the object from the cache will also
      // make it vanish from the fake storage.
      *object= dynamic_cast<const T*>(element->object())->clone();
      ret= false;
    }
    mysql_mutex_unlock(&m_lock);
    if (ret)
    {
      // Can't use my_error() in unit tests as da will not be set and
      // asserts fill fire
      thd->get_stmt_da()->
        set_error_status(ER_INVALID_DD_OBJECT,
                         ("No mapping for key '"+key.str()+"' in fake store").c_str(),
                         mysql_errno_to_sqlstate(ER_INVALID_DD_OBJECT));
    }
    return ret;
  }

  // Fake drop for unit testing.
  template <typename T>
  bool fake_drop(THD *thd, T *object)
  {
    Cache_element<typename T::cache_partition_type> *element= NULL;
    mysql_mutex_lock(&m_lock);
    m_storage.get(typename T::id_key_type(object->id()), &element);
    m_storage.remove(element);
    delete element->object();
    delete element;
    mysql_mutex_unlock(&m_lock);
    return false;
  }


  // Fake store for unit testing.
  template <typename T>
  bool fake_store(THD *thd, T *object)
  {
    Cache_element<typename T::cache_partition_type> *element=
      new Cache_element<typename T::cache_partition_type>();
    mysql_mutex_lock(&m_lock);
    // Fake auto inc col
    dynamic_cast<dd::Entity_object_impl*>(object)->set_id(next_oid++);
    // Need to clone as registry takes ownership
    element->set_object(object->clone());
    element->recreate_keys();
    m_storage.put(element);
    mysql_mutex_unlock(&m_lock);
    return false;
  }
#endif

public:


  /**
    Get a dictionary object from persistent storage.

    Create an access key based on the submitted key, and find the record
    from the appropriate table. Restore the record into a new dictionary
    object.

    @tparam      K         Key type.
    @tparam      T         Dictionary object type.
    @param       thd       Thread context.
    @param       key       Key for which to get the object.
    @param       isolation Isolation level.
    @param [out] object    Object retrieved, possibly NULL if not present.

    @retval      false   No error.
    @retval      true    Error.
  */

  template <typename K, typename T>
  static bool get(THD *thd,
                  const K &key,
                  enum_tx_isolation isolation,
                  const T **object);


  /**
    Drop a dictionary object from persistent storage.

    @tparam  T       Dictionary object type.
    @param   thd     Thread context.
    @param   object  Object to be dropped.

    @retval  false   No error.
    @retval  true    Error.
  */

  template <typename T>
  static bool drop(THD *thd, const T *object);


  /**
    Store a dictionary object to persistent storage.

    @tparam  T       Dictionary object type.
    @param   thd     Thread context.
    @param   object  Object to be stored.

    @retval  false   No error.
    @retval  true    Error.
  */

  template <typename T>
  static bool store(THD *thd, T *object);
};

} // namespace cache
} // namespace dd

#endif // DD_CACHE__STORAGE_ADAPTER_INCLUDED
