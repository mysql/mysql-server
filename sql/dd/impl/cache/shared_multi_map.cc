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

#include "shared_multi_map.h"

#include <stddef.h>
#include <new>

#include "dd/impl/cache/cache_element.h"
#include "log.h"                             // sql_print_warning()
#include "my_dbug.h"

namespace dd {
namespace cache {


// Template function to find an element and mark it as used.
template <typename T>
template <typename K>
Cache_element<T> *Shared_multi_map<T>::use_if_present(const K &key)
{
  mysql_mutex_assert_owner(&m_lock);
  Cache_element<T> *e= NULL;
  // Look up in the appropriate map and get the element.
  m_map<K>()->get(key, &e);

  // Use the element if present.
  if (e)
  {
    // Remove the element from the free list.
    if (e->usage() == 0)
      m_free_list.remove(e);

    // Mark the element as being used, and return it.
    e->use();
    return e;
  }
  return NULL;
}


// Remove an element from the map.
template <typename T>
void Shared_multi_map<T>::remove(Cache_element<T> *element,
                                 Autolocker *lock)
{
  mysql_mutex_assert_owner(&m_lock);

#ifndef DBUG_OFF
  Cache_element<T> *e= NULL;
  m_map<const T*>()->get(element->object(), &e);

  // The element must be present, and its usage must be 1 (this thread).
  DBUG_ASSERT(e == element);
  DBUG_ASSERT(e->usage() == 1);

  // Get all keys that were created within the element.
  const typename T::id_key_type *id_key= element->id_key();
  const typename T::name_key_type *name_key= element->name_key();
  const typename T::aux_key_type *aux_key= element->aux_key();

  // None of the non-null keys may be missed.
  DBUG_ASSERT(
    (!id_key || !m_map<typename T::id_key_type>()->is_missed(*id_key)) &&
    (!name_key || !m_map<typename T::name_key_type>()->is_missed(*name_key)) &&
    (!aux_key || !m_map<typename T::aux_key_type>()->is_missed(*aux_key)));

  // All non-null keys must exist.
  DBUG_ASSERT(
    (!id_key || m_map<typename T::id_key_type>()->is_present(*id_key)) &&
    (!name_key || m_map<typename T::name_key_type>()->is_present(*name_key)) &&
    (!aux_key || m_map<typename T::aux_key_type>()->is_present(*aux_key)));
#endif

  // Remove the keys and the element from the shared map and the registry.
  Multi_map_base<T>::remove_single_element(element);

  // Sign up the object for being deleted.
  lock->auto_delete(element->object());

  // Reuse the element if there is room for it.
  if (!pool_capacity_exceeded())
    m_element_pool.push_back(element);
  else
    lock->auto_delete(element);
}


// Helper function to evict unused elements from the free list.
template <typename T>
void Shared_multi_map<T>::rectify_free_list(Autolocker *lock)
{
  mysql_mutex_assert_owner(&m_lock);
  while (map_capacity_exceeded() && m_free_list.length() > 0)
  {
    Cache_element<T> *e= m_free_list.get_lru();
    DBUG_ASSERT(e && e->object());
    m_free_list.remove(e);
    // Mark the object as being used to allow it to be removed.
    e->use();
    remove(e, lock);
  }
}


// Helper function to evict all unused elements.
template <typename T>
void Shared_multi_map<T>::evict_all_unused(Autolocker *lock)
{
  mysql_mutex_assert_owner(&m_lock);
  while (m_free_list.length())
  {
    Cache_element<T> *e= m_free_list.get_lru();
    DBUG_ASSERT(e && e->object());
    m_free_list.remove(e);
    // Mark the object as being used to allow it to be removed.
    e->use();
    remove(e, lock);
  }
}


// Shutdown the shared map. Delete all objects present.
template <typename T>
void Shared_multi_map<T>::shutdown()
{
  {
    Autolocker lock(this);
    m_capacity= 0;
    evict_all_unused(&lock);
    if (m_map<const T*>()->size() > 0)
    {
      /* purecov: begin deadcode */
      sql_print_warning("Dictionary cache not empty at shutdown.");
      dump();
      DBUG_ASSERT(false);
      /* purecov: end */
    }
  }
  // Delete all elements from the pool.
  for (typename std::vector<Cache_element<T>*>::iterator it=
         m_element_pool.begin();
       it != m_element_pool.end(); ++it)
    delete(*it);
   m_element_pool.clear();
}


// Get a wrapper element from the map handling the given key type.
template <typename T>
template <typename K>
bool Shared_multi_map<T>::get(const K &key, Cache_element<T> **element)
{
  Autolocker lock(this);
  *element= use_if_present(key);
  if (*element)
    return false;

  // Is the element already missed?
  if (m_map<K>()->is_missed(key))
  {
    while (m_map<K>()->is_missed(key))
      mysql_cond_wait(&m_miss_handled, &m_lock);

    *element= use_if_present(key);

    // Here, we return only if element is non-null. An absent element
    // does not mean that the object does not exist, it might have been
    // evicted after the thread handling the first cache miss added
    // it to the cache, before this waiting thread was alerted. Thus,
    // we need to handle this situation as a cache miss if the element
    // is absent.
    if (*element)
      return false;
  }

  // Mark the key as being missed.
  m_map<K>()->set_missed(key);
  return true;
}


// Put a new object and element wrapper into the map.
template <typename T>
template <typename K>
void Shared_multi_map<T>::put(const K *key, const T *object,
                              Cache_element<T> **element)
{
  DBUG_ASSERT(element);
  Autolocker lock(this);
  if (!object)
  {
    DBUG_ASSERT(key);
    // For a NULL object, we only need to signal that the miss is handled.
    if (m_map<K>()->is_missed(*key))
    {
      m_map<K>()->set_miss_handled(*key);
      mysql_cond_broadcast(&m_miss_handled);
    }
    DBUG_ASSERT(*element == NULL);
    return;
  }

#ifndef DBUG_OFF
  // The new object instance may not be present in the map.
  m_map<const T*>()->get(object, element);
  DBUG_ASSERT(*element == NULL);
#endif

  // Get a new element, either from the pool, or by allocating a new one.
  if (!m_element_pool.empty())
  {
    *element= m_element_pool.back();
    m_element_pool.pop_back();
    (*element)->init();
  }
  else
    *element= new (std::nothrow) Cache_element<T>();

  // Prepare the element. Assign the object and create keys.
  (*element)->set_object(object);
  (*element)->recreate_keys();

  // Get all keys that were created within the element.
  const typename T::id_key_type *id_key= (*element)->id_key();
  const typename T::name_key_type *name_key= (*element)->name_key();
  const typename T::aux_key_type *aux_key= (*element)->aux_key();

  // There must be at least one key.
  DBUG_ASSERT(id_key || name_key || aux_key);

  // For the non-null keys being missed, set that the miss is handled.
  bool key_missed= false;
  if (id_key && m_map<typename T::id_key_type>()->is_missed(*id_key))
  {
    key_missed= true;
    m_map<typename T::id_key_type>()->set_miss_handled(*id_key);
  }
  if (name_key && m_map<typename T::name_key_type>()->is_missed(*name_key))
  {
    key_missed= true;
    m_map<typename T::name_key_type>()->set_miss_handled(*name_key);
  }
  if (aux_key && m_map<typename T::aux_key_type>()->is_missed(*aux_key))
  {
    key_missed= true;
    m_map<typename T::aux_key_type>()->set_miss_handled(*aux_key);
  }

  // All non-null keys must exist, or none.
  bool all_keys_present=
    (!id_key || m_map<typename T::id_key_type>()->is_present(*id_key)) &&
    (!name_key || m_map<typename T::name_key_type>()->is_present(*name_key)) &&
    (!aux_key || m_map<typename T::aux_key_type>()->is_present(*aux_key));
  bool no_keys_present=
    (!id_key || !m_map<typename T::id_key_type>()->is_present(*id_key)) &&
    (!name_key || !m_map<typename T::name_key_type>()->is_present(*name_key)) &&
    (!aux_key || !m_map<typename T::aux_key_type>()->is_present(*aux_key));

  // If no keys are present, we must add the element.
  if (no_keys_present)
  {
    // Remove least recently used object(s).
    rectify_free_list(&lock);

    // Mark the element as in use, and register it in all key maps.
    (*element)->use();
    Multi_map_base<T>::add_single_element(*element);

    // In this case, one or more keys may be missed, so we must broadcast.
    if (key_missed)
      mysql_cond_broadcast(&m_miss_handled);

    // The element and the object is now owned by the cache.
    return;
  }

  // If all keys are already present, we discard the new element.
  if (all_keys_present)
  {
    DBUG_ASSERT(key);

    // If all keys are present, we sign up the object for being deleted.
    lock.auto_delete(object);

    // The element is added to the pool if room, or signed up for
    // auto delete.
    if (!pool_capacity_exceeded())
      m_element_pool.push_back(*element);
    else
      lock.auto_delete(*element);

    // Then we return the existing element.
    *element= use_if_present(*key);
    DBUG_ASSERT(*element);

    // In this case, no key may be missed, so there is no need to broadcast.
    DBUG_ASSERT(!key_missed);
    return;
  }

  // Must have all_keys_present ^ no_keys_present
  DBUG_ASSERT(false); /* purecov: inspected */
}


// Release one element.
template <typename T>
void Shared_multi_map<T>::release(Cache_element<T> *element)
{
  Autolocker lock(this);

#ifndef DBUG_OFF
  // The object must be present, and its usage must be > 0.
  Cache_element<T> *e= NULL;
  m_map<const T*>()->get(element->object(), &e);
  DBUG_ASSERT(e == element);
  DBUG_ASSERT(e->usage() > 0);

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

  // Release the element.
  element->release();

  // If the element is not used, add it to the free list.
  if (element->usage() == 0)
  {
    m_free_list.add_last(element);
    rectify_free_list(&lock);
  }
}


// Delete an element from the map.
template <typename T>
void Shared_multi_map<T>::drop(Cache_element<T> *element)
{
  Autolocker lock(this);
  remove(element, &lock);
}


// Delete an object corresponding to the key from the map if exists.
template <typename T>
template <typename K>
void Shared_multi_map<T>::drop_if_present(const K &key)
{
  Autolocker lock(this);

  Cache_element<T> *element= use_if_present(key);

  if (element)
  {
    remove(element, &lock);
  }
}


// Replace the object and re-generate the keys for an element.
template <typename T>
void Shared_multi_map<T>::replace(Cache_element<T> *element, const T* object)
{
  Autolocker lock(this);

#ifndef DBUG_OFF
  // The object must be present, and its usage must be 1 (this thread).
  Cache_element<T> *e= NULL;
  m_map<const T*>()->get(element->object(), &e);
  DBUG_ASSERT(e == element);
  DBUG_ASSERT(e->usage() == 1);
  DBUG_ASSERT(object);
#endif

  // Remove the single element from the maps, but do not delete the instance.
  Multi_map_base<T>::remove_single_element(element);

  // If different, delete the old object, and replace it by the new one.
  if (element->object() != object)
  {
    lock.auto_delete(element->object());
    element->set_object(object);
  }

  // Re-create the keys based on the current version of the object.
  element->recreate_keys();

  // Add the element again, now with the newly generated keys.
  Multi_map_base<T>::add_single_element(element);
}


// Explicitly instantiate the types for the various usages.
template class Shared_multi_map<Abstract_table>;
template bool Shared_multi_map<Abstract_table>::
  get<const Abstract_table*>
    (const Abstract_table* const&, Cache_element<Abstract_table> **);
template bool Shared_multi_map<Abstract_table>::
  get<Abstract_table::id_key_type>
    (const Abstract_table::id_key_type&, Cache_element<Abstract_table> **);
template bool Shared_multi_map<Abstract_table>::
  get<Abstract_table::name_key_type>
    (const Abstract_table::name_key_type&, Cache_element<Abstract_table> **);
template bool Shared_multi_map<Abstract_table>::
  get<Abstract_table::aux_key_type>
    (const Abstract_table::aux_key_type&, Cache_element<Abstract_table> **);
template void Shared_multi_map<Abstract_table>::
  put<Abstract_table::id_key_type>
    (const Abstract_table::id_key_type*, const Abstract_table*,
      Cache_element<Abstract_table> **);
template void Shared_multi_map<Abstract_table>::
  put<Abstract_table::name_key_type>
    (const Abstract_table::name_key_type*, const Abstract_table*,
      Cache_element<Abstract_table> **);
template void Shared_multi_map<Abstract_table>::
  put<Abstract_table::aux_key_type>
    (const Abstract_table::aux_key_type*, const Abstract_table*,
      Cache_element<Abstract_table> **);
template void Shared_multi_map<Abstract_table>::
  drop_if_present<Abstract_table::id_key_type>(const Abstract_table::id_key_type&);

template class Shared_multi_map<Charset>;
template bool Shared_multi_map<Charset>::
  get<const Charset*>
    (const Charset* const&, Cache_element<Charset> **);
template bool Shared_multi_map<Charset>::
  get<Charset::id_key_type>
    (const Charset::id_key_type&, Cache_element<Charset> **);
template bool Shared_multi_map<Charset>::
  get<Charset::name_key_type>
    (const Charset::name_key_type&, Cache_element<Charset> **);
template bool Shared_multi_map<Charset>::
  get<Charset::aux_key_type>
    (const Charset::aux_key_type&, Cache_element<Charset> **);
template void Shared_multi_map<Charset>::
  put<Charset::id_key_type>
    (const Charset::id_key_type*, const Charset*,
      Cache_element<Charset> **);
template void Shared_multi_map<Charset>::
  put<Charset::name_key_type>
    (const Charset::name_key_type*, const Charset*,
      Cache_element<Charset> **);
template void Shared_multi_map<Charset>::
  put<Charset::aux_key_type>
    (const Charset::aux_key_type*, const Charset*,
      Cache_element<Charset> **);
template void Shared_multi_map<Charset>::
  drop_if_present<Charset::id_key_type>(const Charset::id_key_type&);

template class Shared_multi_map<Collation>;
template bool Shared_multi_map<Collation>::
  get<const Collation*>
    (const Collation* const&, Cache_element<Collation> **);
template bool Shared_multi_map<Collation>::
  get<Collation::id_key_type>
    (const Collation::id_key_type&, Cache_element<Collation> **);
template bool Shared_multi_map<Collation>::
  get<Collation::name_key_type>
    (const Collation::name_key_type&, Cache_element<Collation> **);
template bool Shared_multi_map<Collation>::
  get<Collation::aux_key_type>
    (const Collation::aux_key_type&, Cache_element<Collation> **);
template void Shared_multi_map<Collation>::
  put<Collation::id_key_type>
    (const Collation::id_key_type*, const Collation*,
      Cache_element<Collation> **);
template void Shared_multi_map<Collation>::
  put<Collation::name_key_type>
    (const Collation::name_key_type*, const Collation*,
      Cache_element<Collation> **);
template void Shared_multi_map<Collation>::
  put<Collation::aux_key_type>
    (const Collation::aux_key_type*, const Collation*,
      Cache_element<Collation> **);
template void Shared_multi_map<Collation>::
  drop_if_present<Collation::id_key_type>(const Collation::id_key_type&);

template class Shared_multi_map<Event>;
template bool Shared_multi_map<Event>::
get<const Event*>
(const Event* const&, Cache_element<Event> **);
template bool Shared_multi_map<Event>::
get<Event::id_key_type>
(const Event::id_key_type&, Cache_element<Event> **);
template bool Shared_multi_map<Event>::
get<Event::name_key_type>
(const Event::name_key_type&, Cache_element<Event> **);
template bool Shared_multi_map<Event>::
get<Event::aux_key_type>
(const Event::aux_key_type&, Cache_element<Event> **);
template void Shared_multi_map<Event>::
put<Event::id_key_type>
(const Event::id_key_type*, const Event*,
 Cache_element<Event> **);
template void Shared_multi_map<Event>::
put<Event::name_key_type>
(const Event::name_key_type*, const Event*,
 Cache_element<Event> **);
template void Shared_multi_map<Event>::
put<Event::aux_key_type>
(const Event::aux_key_type*, const Event*,
 Cache_element<Event> **);
template void Shared_multi_map<Event>::
  drop_if_present<Event::id_key_type>(const Event::id_key_type&);

template class Shared_multi_map<Routine>;
template bool Shared_multi_map<Routine>::
  get<const Routine*>
    (const Routine* const&, Cache_element<Routine> **);
template bool Shared_multi_map<Routine>::
  get<Routine::id_key_type>
    (const Routine::id_key_type&, Cache_element<Routine> **);
template bool Shared_multi_map<Routine>::
  get<Routine::name_key_type>
    (const Routine::name_key_type&, Cache_element<Routine> **);
template bool Shared_multi_map<Routine>::
  get<Routine::aux_key_type>
    (const Routine::aux_key_type&, Cache_element<Routine> **);
template void Shared_multi_map<Routine>::
  put<Routine::id_key_type>
    (const Routine::id_key_type*, const Routine*,
      Cache_element<Routine> **);
template void Shared_multi_map<Routine>::
  put<Routine::name_key_type>
    (const Routine::name_key_type*, const Routine*,
      Cache_element<Routine> **);
template void Shared_multi_map<Routine>::
  put<Routine::aux_key_type>
    (const Routine::aux_key_type*, const Routine*,
      Cache_element<Routine> **);
template void Shared_multi_map<Routine>::
  drop_if_present<Routine::id_key_type>(const Routine::id_key_type&);

template class Shared_multi_map<Schema>;
template bool Shared_multi_map<Schema>::
  get<const Schema*>
    (const Schema* const&, Cache_element<Schema> **);
template bool Shared_multi_map<Schema>::
  get<Schema::id_key_type>
    (const Schema::id_key_type&, Cache_element<Schema> **);
template bool Shared_multi_map<Schema>::
  get<Schema::name_key_type>
    (const Schema::name_key_type&, Cache_element<Schema> **);
template bool Shared_multi_map<Schema>::
  get<Schema::aux_key_type>
    (const Schema::aux_key_type&, Cache_element<Schema> **);
template void Shared_multi_map<Schema>::
  put<Schema::id_key_type>
    (const Schema::id_key_type*, const Schema*,
      Cache_element<Schema> **);
template void Shared_multi_map<Schema>::
  put<Schema::name_key_type>
    (const Schema::name_key_type*, const Schema*,
      Cache_element<Schema> **);
template void Shared_multi_map<Schema>::
  put<Schema::aux_key_type>
    (const Schema::aux_key_type*, const Schema*,
      Cache_element<Schema> **);
template void Shared_multi_map<Schema>::
  drop_if_present<Schema::id_key_type>(const Schema::id_key_type&);

template class Shared_multi_map<Spatial_reference_system>;
template bool Shared_multi_map<Spatial_reference_system>::
  get<const Spatial_reference_system*>
    (const Spatial_reference_system* const&,
     Cache_element<Spatial_reference_system> **);
template bool Shared_multi_map<Spatial_reference_system>::
  get<Spatial_reference_system::id_key_type>
    (const Spatial_reference_system::id_key_type&,
     Cache_element<Spatial_reference_system> **);
template bool Shared_multi_map<Spatial_reference_system>::
  get<Spatial_reference_system::name_key_type>
    (const Spatial_reference_system::name_key_type&,
     Cache_element<Spatial_reference_system> **);
template bool Shared_multi_map<Spatial_reference_system>::
  get<Spatial_reference_system::aux_key_type>
    (const Spatial_reference_system::aux_key_type&,
     Cache_element<Spatial_reference_system> **);
template void Shared_multi_map<Spatial_reference_system>::
  put<Spatial_reference_system::id_key_type>
    (const Spatial_reference_system::id_key_type*,
     const Spatial_reference_system*,
     Cache_element<Spatial_reference_system> **);
template void Shared_multi_map<Spatial_reference_system>::
  put<Spatial_reference_system::name_key_type>
    (const Spatial_reference_system::name_key_type*,
     const Spatial_reference_system*,
     Cache_element<Spatial_reference_system> **);
template void Shared_multi_map<Spatial_reference_system>::
  put<Spatial_reference_system::aux_key_type>
    (const Spatial_reference_system::aux_key_type*,
     const Spatial_reference_system*,
     Cache_element<Spatial_reference_system> **);
template void Shared_multi_map<Spatial_reference_system>::
  drop_if_present<Spatial_reference_system::id_key_type>(
    const Spatial_reference_system::id_key_type&);

template class Shared_multi_map<Tablespace>;
template bool Shared_multi_map<Tablespace>::
  get<const Tablespace*>
    (const Tablespace* const&, Cache_element<Tablespace> **);
template bool Shared_multi_map<Tablespace>::
  get<Tablespace::id_key_type>
    (const Tablespace::id_key_type&, Cache_element<Tablespace> **);
template bool Shared_multi_map<Tablespace>::
  get<Tablespace::name_key_type>
    (const Tablespace::name_key_type&, Cache_element<Tablespace> **);
template bool Shared_multi_map<Tablespace>::
  get<Tablespace::aux_key_type>
    (const Tablespace::aux_key_type&, Cache_element<Tablespace> **);
template void Shared_multi_map<Tablespace>::
  put<Tablespace::id_key_type>
    (const Tablespace::id_key_type*, const Tablespace*,
      Cache_element<Tablespace> **);
template void Shared_multi_map<Tablespace>::
  put<Tablespace::name_key_type>
    (const Tablespace::name_key_type*, const Tablespace*,
      Cache_element<Tablespace> **);
template void Shared_multi_map<Tablespace>::
  put<Tablespace::aux_key_type>
    (const Tablespace::aux_key_type*, const Tablespace*,
      Cache_element<Tablespace> **);
template void Shared_multi_map<Tablespace>::
  drop_if_present<Tablespace::id_key_type>(const Tablespace::id_key_type&);

} // namespace cache
} // namespace dd
