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

#include "sql/dd/impl/cache/storage_adapter.h"

#include <memory>
#include <string>

#include "mutex_lock.h"                       // Mutex_lock
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "sql/dd/cache/dictionary_client.h"   // Dictionary_client
#include "sql/dd/impl/bootstrapper.h"         // bootstrap::stage
#include "sql/dd/impl/cache/cache_element.h"
#include "sql/dd/impl/raw/object_keys.h"      // Primary_id_key
#include "sql/dd/impl/raw/raw_record.h"       // Raw_record
#include "sql/dd/impl/raw/raw_table.h"        // Raw_table
#include "sql/dd/impl/sdi.h"                  // sdi::store() sdi::drop()
#include "sql/dd/impl/transaction_impl.h"     // Transaction_ro
#include "sql/dd/impl/types/entity_object_impl.h"
#include "sql/dd/types/abstract_table.h"      // Abstract_table
#include "sql/dd/types/charset.h"             // Charset
#include "sql/dd/types/collation.h"           // Collation
#include "sql/dd/types/column_statistics.h"   // Column_statistics
#include "sql/dd/types/entity_object_table.h" // Entity_object_table
#include "sql/dd/types/event.h"               // Event
#include "sql/dd/types/function.h"            // Routine, Function
#include "sql/dd/types/index_stat.h"          // Index_stat
#include "sql/dd/types/procedure.h"           // Procedure
#include "sql/dd/types/schema.h"              // Schema
#include "sql/dd/types/spatial_reference_system.h"// Spatial_reference_system
#include "sql/dd/types/table.h"               // Table
#include "sql/dd/types/table_stat.h"          // Table_stat
#include "sql/dd/types/tablespace.h"          // Tablespace
#include "sql/dd/types/view.h"                // View
#include "sql/dd/upgrade/upgrade.h"           // allow_sdi_creation
#include "sql/debug_sync.h"                   // DEBUG_SYNC
#include "sql/log.h"
#include "sql/sql_class.h"                    // THD

namespace dd {
namespace cache {

Storage_adapter* Storage_adapter::instance()
{
  static Storage_adapter s_instance;
  return &s_instance;
}


bool Storage_adapter::s_use_fake_storage= false;


// Generate a new object id for a registry partition.
template <typename T>
Object_id Storage_adapter::next_oid()
{
  static Object_id next_oid= FIRST_OID;
  return next_oid++;
}


// Get the number of core objects in a registry partition.
template <typename T>
size_t Storage_adapter::core_size()
{
  MUTEX_LOCK(lock, &m_lock);
  return m_core_registry.size<typename T::cache_partition_type>();
}


// Get a dictionary object id from core storage.
template <typename T>
Object_id Storage_adapter::core_get_id(const typename T::name_key_type &key)
{
  Cache_element<typename T::cache_partition_type> *element= nullptr;
  MUTEX_LOCK(lock, &m_lock);
  m_core_registry.get(key, &element);
  if (element)
  {
    DBUG_ASSERT(element->object());
    return element->object()->id();
  }
  return INVALID_OBJECT_ID;
}


// Get a dictionary object from core storage.
template <typename K, typename T>
void Storage_adapter::core_get(const K &key, const T **object)
{
  DBUG_ASSERT(object);
  *object= nullptr;
  Cache_element<typename T::cache_partition_type> *element= nullptr;
  MUTEX_LOCK(lock, &m_lock);
  m_core_registry.get(key, &element);
  if (element)
  {
    // Must clone the object here, otherwise evicting the object from
    // the shared cache will also make it vanish from the core storage.
    *object= dynamic_cast<const T*>(element->object())->clone();
  }
}


// Get a dictionary object from persistent storage.
template <typename K, typename T>
bool Storage_adapter::get(THD *thd,
                          const K &key,
                          enum_tx_isolation isolation,
                          const T **object)
{
  DBUG_ASSERT(object);
  *object= nullptr;

  instance()->core_get(key, object);
  if (*object || s_use_fake_storage)
    return false;

  // We may have a cache miss while checking for existing tables during
  // server start. At this stage, the object will be considered not existing.
  if (bootstrap::stage() < bootstrap::BOOTSTRAP_CREATED)
    return false;

  // Start a DD transaction to get the object.
  Transaction_ro trx(thd, isolation);
  trx.otx.register_tables<T>();

  if (trx.otx.open_tables())
  {
    DBUG_ASSERT(thd->is_system_thread() || thd->killed || thd->is_error());
    return true;
  }

  const Entity_object_table &table= T::OBJECT_TABLE();
  // Get main object table.
  Raw_table *t= trx.otx.get_table(table.name());

  // Find record by the object-id.
  std::unique_ptr<Raw_record> r;
  if (t->find_record(key, r))
  {
    DBUG_ASSERT(thd->is_system_thread() || thd->killed || thd->is_error());
    return true;
  }

  // Restore the object from the record.
  Entity_object *new_object= NULL;
  if (r.get() && table.restore_object_from_record(&trx.otx, *r.get(), &new_object))
  {
    DBUG_ASSERT(thd->is_system_thread() || thd->killed || thd->is_error());
    return true;
  }

  // Delete the new object if dynamic cast fails.
  if (new_object)
  {
    // Here, a failing dynamic cast is not a legitimate situation.
    // In production, we report an error.
    *object= dynamic_cast<T*>(new_object);
    if (!*object)
    {
      /* purecov: begin inspected */
      my_error(ER_INVALID_DD_OBJECT, MYF(0), new_object->name().c_str());
      delete new_object;
      DBUG_ASSERT(false);
      return true;
      /* purecov: end */
    }
  }

  return false;
}


// Drop a dictionary object from core storage.
template <typename T>
void Storage_adapter::core_drop(THD *thd MY_ATTRIBUTE((unused)),
                                const T *object)
{
  DBUG_ASSERT(s_use_fake_storage || thd->is_dd_system_thread());
  DBUG_ASSERT(bootstrap::stage() <= bootstrap::BOOTSTRAP_CREATED);
  Cache_element<typename T::cache_partition_type> *element= nullptr;
  MUTEX_LOCK(lock, &m_lock);

  // For unit tests, drop based on id to simulate behavior of persistent tables.
  // For storing core objects during bootstrap, drop based on names since id may
  // differ between scaffolding objects and persisted objects.
  if (s_use_fake_storage)
  {
    typename T::id_key_type key;
    object->update_id_key(&key);
    m_core_registry.get(key, &element);
  }
  else
  {
    typename T::name_key_type key;
    object->update_name_key(&key);
    m_core_registry.get(key, &element);
  }
  if (element)
  {
    m_core_registry.remove(element);
    delete element->object();
    delete element;
  }
}


// Drop a dictionary object from persistent storage.
template <typename T>
bool Storage_adapter::drop(THD *thd, const T *object)
{
  if (s_use_fake_storage || bootstrap::stage() < bootstrap::BOOTSTRAP_CREATED)
  {
    instance()->core_drop(thd, object);
    return false;
  }

  if (object->impl()->validate())
  {
    DBUG_ASSERT(thd->is_system_thread() || thd->killed || thd->is_error());
    return true;
  }

  if (sdi::drop(thd, object))
  {
    return true;
  }

  // Drop the object from the dd tables. We need to switch transaction ctx to do this.
  Update_dictionary_tables_ctx ctx(thd);
  ctx.otx.register_tables<T>();

  if (ctx.otx.open_tables() || object->impl()->drop(&ctx.otx))
  {
    DBUG_ASSERT(thd->is_system_thread() || thd->killed || thd->is_error());
    return true;
  }

  return false;
}


// Store a dictionary object to core storage.
template <typename T>
void Storage_adapter::core_store(THD *thd, T *object)
{
  DBUG_ASSERT(s_use_fake_storage || thd->is_dd_system_thread());
  DBUG_ASSERT(bootstrap::stage() <= bootstrap::BOOTSTRAP_CREATED);
  Cache_element<typename T::cache_partition_type> *element=
    new Cache_element<typename T::cache_partition_type>();

  if (object->id() != INVALID_OBJECT_ID)
  {
    // For unit tests, drop old object (based on id) to simulate update.
    if (s_use_fake_storage)
      core_drop(thd, object);
  }
  else
  {
    dynamic_cast<dd::Entity_object_impl*>(object)->set_id(next_oid<T>());
  }

  // Need to clone since core registry takes ownership
  element->set_object(object->clone());
  element->recreate_keys();
  MUTEX_LOCK(lock, &m_lock);
  m_core_registry.put(element);
}


// Store a dictionary object to persistent storage.
template <typename T>
bool Storage_adapter::store(THD *thd, T *object)
{
  if (s_use_fake_storage || bootstrap::stage() < bootstrap::BOOTSTRAP_CREATED)
  {
    instance()->core_store(thd, object);
    return false;
  }

  if (object->impl()->validate())
  {
    DBUG_ASSERT(thd->is_system_thread() || thd->killed || thd->is_error());
    return true;
  }

  // Store the object into the dd tables. We need to switch transaction
  // ctx to do this.
  Update_dictionary_tables_ctx ctx(thd);
  ctx.otx.register_tables<T>();
  DEBUG_SYNC(thd, "before_storing_dd_object");

  if (ctx.otx.open_tables() || object->impl()->store(&ctx.otx))
  {
    DBUG_ASSERT(thd->is_system_thread() || thd->killed || thd->is_error());
    return true;
  }

  // Do not create SDIs for tablespaces and tables while creating
  // dictionary entry during upgrade.
  if (bootstrap::stage() > bootstrap::BOOTSTRAP_CREATED &&
      dd::upgrade::allow_sdi_creation() &&
      sdi::store(thd, object))
    return true;

  return false;
}


// Sync a dictionary object from persistent to core storage.
template <typename T>
bool Storage_adapter::core_sync(THD *thd,
                                const typename T::name_key_type &key,
                                const T *object)
{
  DBUG_ASSERT(thd->is_dd_system_thread());
  DBUG_ASSERT(bootstrap::stage() <= bootstrap::BOOTSTRAP_CREATED);

  // Copy the name, needed for error output. The object has to be
  // dropped before get().
  String_type name(object->name());
  core_drop(thd, object);
  const typename T::cache_partition_type* new_obj= nullptr;

  /*
    Fetch the object from persistent tables. The object was dropped
    from the core registry above, so we know get() will fetch it
    from the tables.

    There is a theoretical possibility of get() failing or sending
    back a nullptr if there has been a corruption or wrong usage
    (e.g. dropping a DD table), leaving one or more DD tables
    inaccessible. Assume, e.g., that the 'mysql.tables' table has
    been dropped. Then, the following will happen during restart:

    1. After creating the scaffolding, the meta data representing
       the DD tables is kept in the shared cache, secured by a
       scoped auto releaser in 'sync_meta_data()' in the bootstrapper
       (this is to make sure the meta data is not evicted during
       synchronization).
    2. We sync the DD tables, starting with 'mysql.character_sets'
       (because it is the first entry in the System_table_registry).
    3. Here in core_sync(), the entry in the core registry is
       removed. Then, we call get(), which will read the meta data
       from the persistent DD tables.
    4. While trying to fetch the meta data for the first table to
       be synced (i.e., 'mysql.character_sets'), we first open
       the tables that are needed to read the meta data for a table
       (i.e., we open the core tables). One of these tables is the
       'mysql.tables' table.
    5. While opening these tables, the server will fetch the meta
       data for them. The meta data for 'mysql.tables' is indeed
       found (because it was created as part of the scaffolding
       with the meta data now being in the shared cache), however,
       when opening the table in the storage engine, we get a
       failure because the SE knows nothing about this table, and
       is unable to open it.
  */
  if (get(thd, key, ISO_READ_COMMITTED, &new_obj) || new_obj == nullptr)
  {
    LogErr(ERROR_LEVEL, ER_DD_METADATA_NOT_FOUND, name.c_str());
    return true;
  }

  Cache_element<typename T::cache_partition_type> *element=
    new Cache_element<typename T::cache_partition_type>();
  element->set_object(new_obj);
  element->recreate_keys();
  MUTEX_LOCK(lock, &m_lock);
  m_core_registry.put(element);
  return false;
}


// Remove and delete all elements and objects from core storage.
void Storage_adapter::erase_all()
{
  MUTEX_LOCK(lock, &m_lock);
  instance()->m_core_registry.erase_all();
}


// Dump the contents of the core storage.
void Storage_adapter::dump()
{
#ifndef DBUG_OFF
  MUTEX_LOCK(lock, &m_lock);
  fprintf(stderr, "================================\n");
  fprintf(stderr, "Storage adapter\n");
  m_core_registry.dump<dd::Tablespace>();
  m_core_registry.dump<dd::Schema>();
  m_core_registry.dump<dd::Abstract_table>();
  fprintf(stderr, "================================\n");
#endif
}


// Explicitly instantiate the type for the various usages.
template bool Storage_adapter::core_sync(THD *,
                                         const Table::name_key_type &,
                                         const Table*);
template bool Storage_adapter::core_sync(THD *,
                                         const Tablespace::name_key_type &,
                                         const Tablespace*);
template bool Storage_adapter::core_sync(THD *,
                                         const Schema::name_key_type &,
                                         const Schema*);

template Object_id Storage_adapter::core_get_id<Table>(
      const Table::name_key_type &);
template Object_id Storage_adapter::core_get_id<Schema>(
      const Schema::name_key_type &);
template Object_id Storage_adapter::core_get_id<Tablespace>(
      const Tablespace::name_key_type &);

template
void Storage_adapter::core_get(
       dd::Item_name_key const&, const dd::Schema**);
template
void Storage_adapter::core_get<dd::Item_name_key, dd::Abstract_table>(
       dd::Item_name_key const&, const dd::Abstract_table**);
template
void Storage_adapter::core_get<dd::Global_name_key, dd::Tablespace>(
       dd::Global_name_key const&, const dd::Tablespace**);

template Object_id Storage_adapter::next_oid<Abstract_table>();
template Object_id Storage_adapter::next_oid<Table>();
template Object_id Storage_adapter::next_oid<View>();
template Object_id Storage_adapter::next_oid<Charset>();
template Object_id Storage_adapter::next_oid<Collation>();
template Object_id Storage_adapter::next_oid<Column_statistics>();
template Object_id Storage_adapter::next_oid<Event>();
template Object_id Storage_adapter::next_oid<Routine>();
template Object_id Storage_adapter::next_oid<Function>();
template Object_id Storage_adapter::next_oid<Procedure>();
template Object_id Storage_adapter::next_oid<Schema>();
template Object_id Storage_adapter::next_oid<Spatial_reference_system>();
template Object_id Storage_adapter::next_oid<Tablespace>();

template size_t Storage_adapter::core_size<Abstract_table>();
template size_t Storage_adapter::core_size<Table>();
template size_t Storage_adapter::core_size<Schema>();
template size_t Storage_adapter::core_size<Tablespace>();

template bool Storage_adapter::get<Abstract_table::id_key_type,
                                Abstract_table>
       (THD *, const Abstract_table::id_key_type &,
        enum_tx_isolation, const Abstract_table **);
template bool Storage_adapter::get<Abstract_table::name_key_type,
                                Abstract_table>
       (THD *, const Abstract_table::name_key_type &,
        enum_tx_isolation, const Abstract_table **);
template bool Storage_adapter::get<Abstract_table::aux_key_type,
                                Abstract_table>
       (THD *, const Abstract_table::aux_key_type &,
        enum_tx_isolation, const Abstract_table **);
template bool Storage_adapter::drop(THD *, const Abstract_table *);
template bool Storage_adapter::store(THD *, Abstract_table *);
template bool Storage_adapter::drop(THD *, const Table*);
template bool Storage_adapter::store(THD *, Table*);
template bool Storage_adapter::drop(THD *, const View*);
template bool Storage_adapter::store(THD *, View*);

template bool Storage_adapter::get<Charset::id_key_type, Charset>
       (THD *, const Charset::id_key_type &,
        enum_tx_isolation, const Charset **);
template bool Storage_adapter::get<Charset::name_key_type, Charset>
       (THD *, const Charset::name_key_type &,
        enum_tx_isolation, const Charset **);
template bool Storage_adapter::get<Charset::aux_key_type, Charset>
       (THD *, const Charset::aux_key_type &,
        enum_tx_isolation, const Charset **);
template bool Storage_adapter::drop(THD *, const Charset*);
template bool Storage_adapter::store(THD *, Charset*);

template bool Storage_adapter::get<Collation::id_key_type, Collation>
       (THD *, const Collation::id_key_type &,
        enum_tx_isolation, const Collation **);
template bool Storage_adapter::get<Collation::name_key_type, Collation>
       (THD *, const Collation::name_key_type &,
        enum_tx_isolation, const Collation **);
template bool Storage_adapter::get<Collation::aux_key_type, Collation>
       (THD *, const Collation::aux_key_type &,
        enum_tx_isolation, const Collation **);
template bool Storage_adapter::drop(THD *, const Collation*);
template bool Storage_adapter::store(THD *, Collation*);

template bool
Storage_adapter::get<Column_statistics::id_key_type, Column_statistics>
  (THD *, const Column_statistics::id_key_type &, enum_tx_isolation,
   const Column_statistics **);
template bool
Storage_adapter::get<Column_statistics::name_key_type, Column_statistics>
  (THD *, const Column_statistics::name_key_type &, enum_tx_isolation,
   const Column_statistics **);
template bool
Storage_adapter::get<Column_statistics::aux_key_type, Column_statistics>
  (THD *, const Column_statistics::aux_key_type &, enum_tx_isolation,
   const Column_statistics **);
template bool Storage_adapter::drop(THD *, const Column_statistics*);
template bool Storage_adapter::store(THD *, Column_statistics*);

template bool Storage_adapter::get<Event::id_key_type, Event>
(THD *, const Event::id_key_type &, enum_tx_isolation, const Event **);
template bool Storage_adapter::get<Event::name_key_type, Event>
(THD *, const Event::name_key_type &, enum_tx_isolation, const Event **);
template bool Storage_adapter::get<Event::aux_key_type, Event>
(THD *, const Event::aux_key_type &, enum_tx_isolation, const Event **);
template bool Storage_adapter::drop(THD *, const Event*);
template bool Storage_adapter::store(THD *, Event*);

template bool Storage_adapter::get<Resource_group::id_key_type, Resource_group>
  (THD *, const Tablespace::id_key_type &,
   enum_tx_isolation, const Resource_group **);
template bool Storage_adapter::get<Resource_group::name_key_type, Resource_group>
  (THD *, const Tablespace::name_key_type &,
   enum_tx_isolation, const Resource_group **);
template bool Storage_adapter::get<Resource_group::aux_key_type, Resource_group>
  (THD *, const Tablespace::aux_key_type &,
   enum_tx_isolation, const Resource_group **);
template bool Storage_adapter::drop(THD *, const Resource_group *);
template bool Storage_adapter::store(THD *, Resource_group *);

template bool Storage_adapter::get<Routine::id_key_type, Routine>
       (THD *, const Routine::id_key_type &, enum_tx_isolation,
        const Routine **);
template bool Storage_adapter::get<Routine::name_key_type, Routine>
       (THD *, const Routine::name_key_type &, enum_tx_isolation,
        const Routine **);
template bool Storage_adapter::get<Routine::aux_key_type, Routine>
       (THD *, const Routine::aux_key_type &, enum_tx_isolation,
        const Routine **);
template bool Storage_adapter::drop(THD *, const Routine*);
template bool Storage_adapter::store(THD *, Routine*);
template bool Storage_adapter::drop(THD *, const Function*);
template bool Storage_adapter::store(THD *, Function*);
template bool Storage_adapter::drop(THD *, const Procedure*);
template bool Storage_adapter::store(THD *, Procedure*);

template bool Storage_adapter::get<Schema::id_key_type, Schema>
       (THD *, const Schema::id_key_type &,
        enum_tx_isolation, const Schema **);
template bool Storage_adapter::get<Schema::name_key_type, Schema>
       (THD *, const Schema::name_key_type &,
        enum_tx_isolation, const Schema **);
template bool Storage_adapter::get<Schema::aux_key_type, Schema>
       (THD *, const Schema::aux_key_type &,
        enum_tx_isolation, const Schema **);
template bool Storage_adapter::drop(THD *, const Schema*);
template bool Storage_adapter::store(THD *, Schema*);

template bool Storage_adapter::get<Spatial_reference_system::id_key_type,
                                   Spatial_reference_system>
       (THD *, const Spatial_reference_system::id_key_type &,
        enum_tx_isolation, const Spatial_reference_system **);
template bool Storage_adapter::get<Spatial_reference_system::name_key_type,
                                   Spatial_reference_system>
       (THD *, const Spatial_reference_system::name_key_type &,
        enum_tx_isolation, const Spatial_reference_system **);
template bool Storage_adapter::get<Spatial_reference_system::aux_key_type,
                                   Spatial_reference_system>
       (THD *, const Spatial_reference_system::aux_key_type &,
        enum_tx_isolation, const Spatial_reference_system **);
template bool Storage_adapter::drop(THD *, const Spatial_reference_system*);
template bool Storage_adapter::store(THD *, Spatial_reference_system*);

template bool Storage_adapter::get<Tablespace::id_key_type, Tablespace>
       (THD *, const Tablespace::id_key_type &,
        enum_tx_isolation, const Tablespace **);
template bool Storage_adapter::get<Tablespace::name_key_type, Tablespace>
       (THD *, const Tablespace::name_key_type &,
        enum_tx_isolation, const Tablespace **);
template bool Storage_adapter::get<Tablespace::aux_key_type, Tablespace>
       (THD *, const Tablespace::aux_key_type &,
        enum_tx_isolation, const Tablespace **);
template bool Storage_adapter::drop(THD *, const Tablespace*);
template bool Storage_adapter::store(THD *, Tablespace*);

/*
  DD objects dd::Table_stat and dd::Index_stat are not cached,
  because these objects are only updated and never read by DD
  API's. Information schema system views use these DD tables
  to project table/index statistics. As these objects are
  not in DD cache, it cannot make it to core storage.
*/

template <>
void Storage_adapter::core_get(const Table_stat::name_key_type&,
                               const Table_stat**)
{ }

template <>
void Storage_adapter::core_get(const Index_stat::name_key_type&,
                               const Index_stat**)
{ }

template <>
void Storage_adapter::core_drop(THD*, const Table_stat*)
{ }

template <>
void Storage_adapter::core_drop(THD*, const Index_stat*)
{ }

template <>
void Storage_adapter::core_store(THD*, Table_stat*)
{ }

template <>
void Storage_adapter::core_store(THD*, Index_stat*)
{ }

template bool Storage_adapter::get<Table_stat::name_key_type, Table_stat>
                (THD *, const Table_stat::name_key_type &,
                 enum_tx_isolation, const Table_stat **);
template bool Storage_adapter::store(THD *, Table_stat*);
template bool Storage_adapter::drop(THD *, const Table_stat*);
template bool Storage_adapter::get<Index_stat::name_key_type, Index_stat>
                (THD *, const Index_stat::name_key_type &,
                 enum_tx_isolation, const Index_stat **);
template bool Storage_adapter::store(THD *, Index_stat*);
template bool Storage_adapter::drop(THD *, const Index_stat*);

} // namespace cache
} // namespace dd
