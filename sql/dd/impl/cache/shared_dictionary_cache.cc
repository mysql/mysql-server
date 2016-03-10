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

#include "shared_dictionary_cache.h"

#include "sql_class.h"                      // THD::is_error()

#include "storage_adapter.h"                // Storage_adapter

namespace dd {
namespace cache {


// Get an element from the cache, given the key.
template <typename K, typename T>
bool Shared_dictionary_cache::get(THD *thd, const K &key,
                                  Cache_element<T> **element)
{
  bool error= false;
  DBUG_ASSERT(element);
  if (m_map<T>()->get(key, element))
  {
    // Handle cache miss.
    const T *new_object= NULL;
    error= get_uncached(thd, key, ISO_READ_COMMITTED, &new_object);

    // Add the new object, and assign the output element, even in the case of
    // a miss error (needed to remove the missed key).
    m_map<T>()->put(&key, new_object, element);
  }
  return error;
}


// Read an object directly from disk, given the key.
template <typename K, typename T>
bool Shared_dictionary_cache::get_uncached(THD *thd,
                                           const K &key,
                                           enum_tx_isolation isolation,
                                           const T **object) const
{
  DBUG_ASSERT(object);
  bool error= Storage_adapter::get(thd, key, isolation, object);
  DBUG_ASSERT(!error || thd->is_system_thread() || thd->killed || thd->is_error());

  return error;
}


// Add an object to the shared cache.
template <typename T>
void Shared_dictionary_cache::put(const T* object, Cache_element<T> **element)
{
  DBUG_ASSERT(object);
  // Cast needed to help the compiler choose the correct template instance..
  m_map<T>()->put(static_cast<const typename T::id_key_type*>(NULL),
                  object, element);
}


// Explicitly instantiate the types for the various usages.
template bool Shared_dictionary_cache::
  get<Abstract_table::id_key_type, Abstract_table>(
    THD *thd, const Abstract_table::id_key_type&,
    Cache_element<Abstract_table>**);
template bool Shared_dictionary_cache::
  get<Abstract_table::name_key_type, Abstract_table>(
    THD *thd, const Abstract_table::name_key_type&,
    Cache_element<Abstract_table>**);
template bool Shared_dictionary_cache::
  get<Abstract_table::aux_key_type, Abstract_table>(
    THD *thd, const Abstract_table::aux_key_type&,
    Cache_element<Abstract_table>**);
template bool Shared_dictionary_cache::
  get_uncached<Abstract_table::id_key_type, Abstract_table>(
    THD *thd, const Abstract_table::id_key_type&,
    enum_tx_isolation, const Abstract_table**) const;
template bool Shared_dictionary_cache::
  get_uncached<Abstract_table::name_key_type, Abstract_table>(
    THD *thd, const Abstract_table::name_key_type&,
    enum_tx_isolation, const Abstract_table**) const;
template bool Shared_dictionary_cache::
  get_uncached<Abstract_table::aux_key_type, Abstract_table>(
    THD *thd, const Abstract_table::aux_key_type&,
    enum_tx_isolation, const Abstract_table**) const;
template void Shared_dictionary_cache::put<Abstract_table>(
    const Abstract_table*,
    Cache_element<Abstract_table>**);

template bool Shared_dictionary_cache::
  get<Charset::id_key_type, Charset>(
    THD *thd, const Charset::id_key_type&,
    Cache_element<Charset>**);
template bool Shared_dictionary_cache::
  get<Charset::name_key_type, Charset>(
    THD *thd, const Charset::name_key_type&,
    Cache_element<Charset>**);
template bool Shared_dictionary_cache::
  get<Charset::aux_key_type, Charset>(
    THD *thd, const Charset::aux_key_type&,
    Cache_element<Charset>**);
template bool Shared_dictionary_cache::
  get_uncached<Charset::id_key_type, Charset>(
    THD *thd, const Charset::id_key_type&,
    enum_tx_isolation, const Charset**) const;
template bool Shared_dictionary_cache::
  get_uncached<Charset::name_key_type, Charset>(
    THD *thd, const Charset::name_key_type&,
    enum_tx_isolation, const Charset**) const;
template bool Shared_dictionary_cache::
  get_uncached<Charset::aux_key_type, Charset>(
    THD *thd, const Charset::aux_key_type&,
    enum_tx_isolation, const Charset**) const;
template void Shared_dictionary_cache::put<Charset>(
    const Charset*,
    Cache_element<Charset>**);

template bool Shared_dictionary_cache::
  get<Collation::id_key_type, Collation>(
    THD *thd, const Collation::id_key_type&,
    Cache_element<Collation>**);
template bool Shared_dictionary_cache::
  get<Collation::name_key_type, Collation>(
    THD *thd, const Collation::name_key_type&,
    Cache_element<Collation>**);
template bool Shared_dictionary_cache::
  get<Collation::aux_key_type, Collation>(
    THD *thd, const Collation::aux_key_type&,
    Cache_element<Collation>**);
template bool Shared_dictionary_cache::
  get_uncached<Collation::id_key_type, Collation>(
    THD *thd, const Collation::id_key_type&,
    enum_tx_isolation, const Collation**) const;
template bool Shared_dictionary_cache::
  get_uncached<Collation::name_key_type, Collation>(
    THD *thd, const Collation::name_key_type&,
    enum_tx_isolation, const Collation**) const;
template bool Shared_dictionary_cache::
  get_uncached<Collation::aux_key_type, Collation>(
    THD *thd, const Collation::aux_key_type&,
    enum_tx_isolation, const Collation**) const;
template void Shared_dictionary_cache::put<Collation>(
    const Collation*,
    Cache_element<Collation>**);

template bool Shared_dictionary_cache::
   get<Event::id_key_type, Event>(
     THD *thd, const Event::id_key_type&,
     Cache_element<Event>**);
template bool Shared_dictionary_cache::
   get<Event::name_key_type, Event>(
     THD *thd, const Event::name_key_type&,
     Cache_element<Event>**);
 template bool Shared_dictionary_cache::
   get<Event::aux_key_type, Event>(
     THD *thd, const Event::aux_key_type&,
     Cache_element<Event>**);
template bool Shared_dictionary_cache::
   get_uncached<Event::id_key_type, Event>(
     THD *thd, const Event::id_key_type&,
     enum_tx_isolation, const Event**) const;
template bool Shared_dictionary_cache::
   get_uncached<Event::name_key_type, Event>(
     THD *thd, const Event::name_key_type&,
     enum_tx_isolation, const Event**) const;
template bool Shared_dictionary_cache::
   get_uncached<Event::aux_key_type, Event>(
     THD *thd, const Event::aux_key_type&,
     enum_tx_isolation, const Event**) const;
template void Shared_dictionary_cache::put<Event>(
     const Event*,
     Cache_element<Event>**);

template bool Shared_dictionary_cache::
  get<Routine::id_key_type, Routine>(
    THD *thd, const Routine::id_key_type&,
    Cache_element<Routine>**);
template bool Shared_dictionary_cache::
  get<Routine::name_key_type, Routine>(
    THD *thd, const Routine::name_key_type&,
    Cache_element<Routine>**);
template bool Shared_dictionary_cache::
  get<Routine::aux_key_type, Routine>(
    THD *thd, const Routine::aux_key_type&,
    Cache_element<Routine>**);
template bool Shared_dictionary_cache::
  get_uncached<Routine::id_key_type, Routine>(
    THD *thd, const Routine::id_key_type&,
    enum_tx_isolation, const Routine**) const;
template bool Shared_dictionary_cache::
  get_uncached<Routine::name_key_type, Routine>(
    THD *thd, const Routine::name_key_type&,
    enum_tx_isolation, const Routine**) const;
template bool Shared_dictionary_cache::
  get_uncached<Routine::aux_key_type, Routine>(
    THD *thd, const Routine::aux_key_type&,
    enum_tx_isolation, const Routine**) const;
template void Shared_dictionary_cache::put<Routine>(
    const Routine*,
    Cache_element<Routine>**);

template bool Shared_dictionary_cache::
  get<Schema::id_key_type, Schema>(
    THD *thd, const Schema::id_key_type&,
    Cache_element<Schema>**);
template bool Shared_dictionary_cache::
  get<Schema::name_key_type, Schema>(
    THD *thd, const Schema::name_key_type&,
    Cache_element<Schema>**);
template bool Shared_dictionary_cache::
  get<Schema::aux_key_type, Schema>(
    THD *thd, const Schema::aux_key_type&,
    Cache_element<Schema>**);
template bool Shared_dictionary_cache::
  get_uncached<Schema::id_key_type, Schema>(
    THD *thd, const Schema::id_key_type&,
    enum_tx_isolation, const Schema**) const;
template bool Shared_dictionary_cache::
  get_uncached<Schema::name_key_type, Schema>(
    THD *thd, const Schema::name_key_type&,
    enum_tx_isolation, const Schema**) const;
template bool Shared_dictionary_cache::
  get_uncached<Schema::aux_key_type, Schema>(
    THD *thd, const Schema::aux_key_type&,
    enum_tx_isolation, const Schema**) const;
template void Shared_dictionary_cache::put<Schema>(
    const Schema*,
    Cache_element<Schema>**);

template bool Shared_dictionary_cache::
  get<Tablespace::id_key_type, Tablespace>(
    THD *thd, const Tablespace::id_key_type&,
    Cache_element<Tablespace>**);
template bool Shared_dictionary_cache::
  get<Tablespace::name_key_type, Tablespace>(
    THD *thd, const Tablespace::name_key_type&,
    Cache_element<Tablespace>**);
template bool Shared_dictionary_cache::
  get<Tablespace::aux_key_type, Tablespace>(
    THD *thd, const Tablespace::aux_key_type&,
    Cache_element<Tablespace>**);
template bool Shared_dictionary_cache::
  get_uncached<Tablespace::id_key_type, Tablespace>(
    THD *thd, const Tablespace::id_key_type&,
    enum_tx_isolation, const Tablespace**) const;
template bool Shared_dictionary_cache::
  get_uncached<Tablespace::name_key_type, Tablespace>(
    THD *thd, const Tablespace::name_key_type&,
    enum_tx_isolation, const Tablespace**) const;
template bool Shared_dictionary_cache::
  get_uncached<Tablespace::aux_key_type, Tablespace>(
    THD *thd, const Tablespace::aux_key_type&,
    enum_tx_isolation, const Tablespace**) const;
template void Shared_dictionary_cache::put<Tablespace>(
    const Tablespace*,
    Cache_element<Tablespace>**);
}
} // namespace dd
