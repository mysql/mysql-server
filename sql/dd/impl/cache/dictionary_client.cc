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

#include "dd/cache/dictionary_client.h"

#include <stdio.h>
#include <memory>

#include "dd/cache/multi_map_base.h"
#include "dd/dd_schema.h"                    // dd::Schema_MDL_locker
#include "dd/impl/bootstrapper.h"            // bootstrap_stage
#include "dd/impl/dictionary_impl.h"
#include "dd/impl/object_key.h"
#include "dd/impl/raw/object_keys.h"         // Primary_id_key, ...
#include "dd/impl/raw/raw_record.h"
#include "dd/impl/raw/raw_record_set.h"      // Raw_record_set
#include "dd/impl/raw/raw_table.h"           // Raw_table
#include "dd/impl/sdi.h"                     // dd::sdi::drop_after_update
#include "dd/impl/tables/character_sets.h"   // create_name_key()
#include "dd/impl/tables/collations.h"       // create_name_key()
#include "dd/impl/tables/events.h"           // create_name_key()
#include "dd/impl/tables/index_stats.h"      // dd::Index_stats
#include "dd/impl/tables/routines.h"         // create_name_key()
#include "dd/impl/tables/schemata.h"         // create_name_key()
#include "dd/impl/tables/spatial_reference_systems.h" // create_name_key()
#include "dd/impl/tables/table_partitions.h" // get_partition_table_id()
#include "dd/impl/tables/table_stats.h"      // dd::Table_stats
#include "dd/impl/tables/tables.h"           // create_name_key()
#include "dd/impl/tables/tablespaces.h"      // create_name_key()
#include "dd/impl/tables/triggers.h"         // dd::tables::Triggers
#include "dd/impl/tables/view_routine_usage.h" // create_name_key
#include "dd/impl/tables/view_table_usage.h" // create_name_key
#include "dd/impl/transaction_impl.h"        // Transaction_ro
#include "dd/impl/types/entity_object_impl.h"// Entity_object_impl
#include "dd/impl/types/object_table_definition_impl.h" // fs_name_case()
#include "dd/properties.h"                   // Properties
#include "dd/types/abstract_table.h"         // Abstract_table
#include "dd/types/charset.h"                // Charset
#include "dd/types/collation.h"              // Collation
#include "dd/types/dictionary_object_table.h"
#include "dd/types/event.h"                  // Event
#include "dd/types/function.h"               // Function
#include "dd/types/index_stat.h"             // Index_stat
#include "dd/types/procedure.h"              // Procedure
#include "dd/types/routine.h"
#include "dd/types/schema.h"                 // Schema
#include "dd/types/spatial_reference_system.h" // Spatial_reference_system
#include "dd/types/table.h"                  // Table
#include "dd/types/table_stat.h"             // Table_stat
#include "dd/types/tablespace.h"             // Tablespace
#include "dd/types/view.h"                   // View
#include "dd/types/view_routine.h"           // View_routine
#include "dd/types/view_table.h"             // View_table
#include "debug_sync.h"                      // DEBUG_SYNC()
#include "handler.h"
#include "lex_string.h"
#include "log.h"                             // sql_print_warning()
#include "m_ctype.h"
#include "m_string.h"
#include "mdl.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql_com.h"
#include "mysqld.h"
#include "mysqld_error.h"
#include "shared_dictionary_cache.h"         // get(), release(), ...
#include "sql_class.h"                       // THD
#include "sql_plugin_ref.h"
#include "storage_adapter.h"                 // store(), drop(), ...
#include "table.h"

namespace dd {
class Dictionary_object;
namespace cache {
class Object_registry;
template <typename T> class Cache_element;
}  // namespace cache
}  // namespace dd

namespace {


/**
  Helper class providing overloaded functions asserting that we have proper
  MDL locks in place. Please note that the functions cannot be called
  until after we have the name of the object, so if we acquire an object
  by id, the asserts must be delayed until the object is retrieved.

  @note Checking for MDL locks is disabled for the DD initialization
        thread because the server is not multi threaded at this stage.
*/

class MDL_checker
{
private:

  /**
    Private helper function for asserting MDL for tables.

    @note For temporary tables, we have no locks.

    @param   thd            Thread context.
    @param   schema_name    Schema name to use in the MDL key.
    @param   object_name    Object name to use in the MDL key.
    @param   mdl_namespace  MDL key namespace to use.
    @param   lock_type      Weakest lock type accepted.

    @return true if we have the required lock, otherwise false.
  */

  static bool is_locked(THD *thd,
                        const char *schema_name,
                        const char *object_name,
                        MDL_key::enum_mdl_namespace mdl_namespace,
                        enum_mdl_type lock_type)
  {

    // For the schema name part, the behavior is dependent on whether
    // the schema name is supplied explicitly in the sql statement
    // or not. If it is, the case sensitive name is locked. If only
    // the table name is supplied in the SQL statement, then the
    // current schema is used as the schema part of the key, and in
    // that case, the lowercase name is locked. This applies only
    // when l_c_t_n == 2. To verify, we therefor use both variants
    // of the schema name.
    char schema_name_buf[NAME_LEN + 1];
    return thd->mdl_context.owns_equal_or_stronger_lock(
                              mdl_namespace,
                              schema_name,
                              object_name,
                              lock_type) ||
           thd->mdl_context.owns_equal_or_stronger_lock(
                              mdl_namespace,
                              dd::Object_table_definition_impl::
                                fs_name_case(schema_name,
                                             schema_name_buf),
                              object_name,
                              lock_type);
  }


  /**
    Private helper function for asserting MDL for tables.

    @note We need to retrieve the schema name, since this is required
          for the MDL key.

    @param   thd          Thread context.
    @param   table        Table object.
    @param   lock_type    Weakest lock type accepted.

    @return true if we have the required lock, otherwise false.
  */

  static bool is_locked(THD *thd, const dd::Abstract_table *table,
                        enum_mdl_type lock_type)
  {
    // The schema must be auto released to avoid disturbing the context
    // at the origin of the function call.
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Schema *schema= NULL;

    // If the schema acquisition fails, we cannot assure that we have a lock,
    // and therefore return false.
    if (thd->dd_client()->acquire(table->schema_id(), &schema))
      return false;

    // Skip check for temporary tables.
    if (!table || is_prefix(table->name().c_str(), tmp_file_prefix))
      return true;

    // Likewise, if there is no schema, we cannot have a proper lock.
    // This may in theory happen during bootstrapping since the meta data for
    // the system schema is not stored yet; however, this is prevented by
    // surrounding code calling this function only if '!thd->is_dd_system_thread'
    // i.e., this is not a bootstrapping thread.
    DBUG_ASSERT(!thd->is_dd_system_thread());

    // We must take l_c_t_n into account when reconstructing the
    // MDL key from the table name.
    char table_name_buf[NAME_LEN + 1];

    if (schema)
    {
      if (!my_strcasecmp(system_charset_info,
                         schema->name().c_str(),
                         "information_schema"))
        return is_locked(thd, schema->name().c_str(), table->name().c_str(),
                         MDL_key::TABLE, lock_type);
      else
        return is_locked(thd, schema->name().c_str(),
                         dd::Object_table_definition_impl::fs_name_case(
                                                             table->name(),
                                                             table_name_buf),
                         MDL_key::TABLE, lock_type);
    }

    return false;
  }


  /**
    Private helper function for asserting MDL for events.

    @note We need to retrieve the schema name, since this is required
          for the MDL key.

    @param   thd          Thread context.
    @param   event        Event object.
    @param   lock_type    Weakest lock type accepted.

    @return true if we have the required lock, otherwise false.
  */

  static bool is_locked(THD *thd, const dd::Event *event,
                        enum_mdl_type lock_type)
  {
    // The schema must be auto released to avoid disturbing the context
    // at the origin of the function call.
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Schema *schema= NULL;

    // If the schema acquisition fails, we cannot assure that we have a lock,
    // and therefore return false.
    if (thd->dd_client()->acquire(event->schema_id(), &schema))
      return false;

    char lc_event_name[NAME_LEN + 1];
    my_stpcpy(lc_event_name, event->name().c_str());
    my_casedn_str(&my_charset_utf8_tolower_ci, lc_event_name);

    // Likewise, if there is no schema, we cannot have a proper lock.
    // @todo This may happen during bootstrapping since the meta data for the
    // system schema is not stored yet. To be fixed in wl#6394. TODO_WL6394.
    if (schema)
      return is_locked(thd, schema->name().c_str(), lc_event_name,
                       MDL_key::EVENT, lock_type);
    else if (event->schema_id() == 1)
      return is_locked(thd, MYSQL_SCHEMA_NAME.str, lc_event_name,
                       MDL_key::EVENT, lock_type);

    return false;
  }


  /**
    Private helper function for asserting MDL for routines.

    @note We need to retrieve the schema name, since this is required
          for the MDL key.

    @param   thd          Thread context.
    @param   routine      Routine object.
    @param   lock_type    Weakest lock type accepted.

    @return true if we have the required lock, otherwise false.
  */

  static bool is_locked(THD *thd, const dd::Routine *routine,
                        enum_mdl_type lock_type)
  {
    // The schema must be auto released to avoid disturbing the context
    // at the origin of the function call.
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Schema *schema= NULL;

    // If the schema acquisition fails, we cannot assure that we have a lock,
    // and therefore return false.
    if (thd->dd_client()->acquire(routine->schema_id(), &schema))
      return false;

    MDL_key::enum_mdl_namespace mdl_namespace= MDL_key::FUNCTION;
    if (routine->type() == dd::Routine::RT_PROCEDURE)
      mdl_namespace= MDL_key::PROCEDURE;

    // Routine names are case in-sensitive to MDL's are taken
    // on lower case names.
    char lc_routine_name[NAME_LEN + 1];
    my_stpcpy(lc_routine_name, routine->name().c_str());
    my_casedn_str(system_charset_info, lc_routine_name);

    // Likewise, if there is no schema, we cannot have a proper lock.
    // @todo This may happen during bootstrapping since the meta data for the
    // system schema is not stored yet. To be fixed in wl#6394. TODO_WL6394.
    if (schema)
      return is_locked(thd, schema->name().c_str(), lc_routine_name,
                       mdl_namespace, lock_type);
    else if (routine->schema_id() == 1)
      return is_locked(thd, MYSQL_SCHEMA_NAME.str, lc_routine_name,
                       mdl_namespace, lock_type);

    return false;
  }


  /**
    Private helper function for asserting MDL for schemata.

    @param   thd          Thread context.
    @param   schema       Schema object.
    @param   lock_type    Weakest lock type accepted.

    @return true if we have the required lock, otherwise false.
  */

  static bool is_locked(THD *thd, const dd::Schema *schema,
                        enum_mdl_type lock_type)
  {
    if (!schema)
      return true;

    // We must take l_c_t_n into account when reconstructing the
    // MDL key from the schema name.
    char name_buf[NAME_LEN + 1];
    return thd->mdl_context.owns_equal_or_stronger_lock(
                              MDL_key::SCHEMA,
                              dd::Object_table_definition_impl::
                                fs_name_case(schema->name(),
                                             name_buf),
                              "",
                              lock_type);
  }


  /**
    Private helper function for asserting MDL for spatial reference systems.

    @param   thd          Thread context.
    @param   srs          Spatial reference system object.
    @param   lock_type    Weakest lock type accepted.

    @return true if we have the required lock, otherwise false.
  */

  static bool is_locked(THD *thd, const dd::Spatial_reference_system *srs,
                        enum_mdl_type lock_type)
  {
    if (!srs)
      return true;

    // Check that the SRID is within the legal range to make sure we
    // don't overflow id_str below. The ID is unsigned, so we only
    // need to check the upper bound.
    DBUG_ASSERT(srs->id() <= UINT_MAX32);

    char id_str[11]; // uint32 => max 10 digits + \0
    int10_to_str(static_cast<long>(srs->id()), id_str, 10);

    return thd->mdl_context.owns_equal_or_stronger_lock(
                              MDL_key::SRID,
                              "",
                              id_str,
                              lock_type);
  }


  /**
    Private helper function for asserting MDL for tablespaces.

    @note We need to retrieve the schema name, since this is required
          for the MDL key.

    @param   thd          Thread context.
    @param   tablespace   Tablespace object.
    @param   lock_type    Weakest lock type accepted.

    @return true if we have the required lock, otherwise false.
  */

  static bool is_locked(THD *thd, const dd::Tablespace *tablespace,
                        enum_mdl_type lock_type)
  {
    if (!tablespace)
      return true;

    return thd->mdl_context.owns_equal_or_stronger_lock(
                              MDL_key::TABLESPACE,
                              "", tablespace->name().c_str(),
                              lock_type);
  }

public:
  // Releasing arbitrary dictionary objects is not checked.
  static bool is_release_locked(THD*, const dd::Dictionary_object*)
  { return true; }

  // Reading a table object should be governed by MDL_SHARED.
  static bool is_read_locked(THD *thd, const dd::Abstract_table *table)
  { return thd->is_dd_system_thread() || is_locked(thd, table, MDL_SHARED); }

  // Writing a table object should be governed by MDL_EXCLUSIVE.
  static bool is_write_locked(THD *thd, const dd::Abstract_table *table)
  {
    return thd->is_dd_system_thread() ||
           is_locked(thd, table, MDL_EXCLUSIVE);
  }

  // Reading a spatial reference system object should be governed by MDL_SHARED.
  static bool is_read_locked(THD *thd, const dd::Spatial_reference_system *srs)
  { return thd->is_dd_system_thread() || is_locked(thd, srs, MDL_SHARED); }

  // Writing a spatial reference system  object should be governed by
  // MDL_EXCLUSIVE.
  static bool is_write_locked(THD *thd, const dd::Spatial_reference_system *srs)
  { return !mysqld_server_started || is_locked(thd, srs, MDL_EXCLUSIVE); }

  // No MDL namespace for character sets.
  static bool is_read_locked(THD*, const dd::Charset*)
  { return true; }

  // No MDL namespace for character sets.
  static bool is_write_locked(THD*, const dd::Charset*)
  { return true; }

  // No MDL namespace for collations.
  static bool is_read_locked(THD*, const dd::Collation*)
  { return true; }

  // No MDL namespace for collations.
  static bool is_write_locked(THD*, const dd::Collation*)
  { return true; }

  /*
    Reading a schema object should be governed by at least
    MDL_INTENTION_EXCLUSIVE. IX is acquired when a schema is
    being accessed when creating/altering table; while opening
    a table before we know whether the table exists, and when
    explicitly acquiring a schema object for reading.
  */
  static bool is_read_locked(THD *thd, const dd::Schema *schema)
  {
    return thd->is_dd_system_thread() ||
           is_locked(thd, schema, MDL_INTENTION_EXCLUSIVE);
  }

  // Writing a schema object should be governed by MDL_EXCLUSIVE.
  static bool is_write_locked(THD *thd, const dd::Schema *schema)
  {
    return thd->is_dd_system_thread() ||
           is_locked(thd, schema, MDL_EXCLUSIVE);
  }

  // Releasing a schema object should be covered in the same way as for reading.
  static bool is_release_locked(THD *thd, const dd::Schema *schema)
  { return is_read_locked(thd, schema); }

  /*
    Reading a tablespace object should be governed by at least
    MDL_INTENTION_EXCLUSIVE. IX is acquired when a tablespace is
    being accessed when creating/altering table.
  */
  static bool is_read_locked(THD *thd, const dd::Tablespace *tablespace)
  {
    return thd->is_dd_system_thread() ||
           is_locked(thd, tablespace, MDL_INTENTION_EXCLUSIVE);
  }

  // Writing a tablespace object should be governed by MDL_EXCLUSIVE.
  static bool is_write_locked(THD *thd, const dd::Tablespace *tablespace)
  {
    return thd->is_dd_system_thread() ||
           is_locked(thd, tablespace, MDL_EXCLUSIVE);
  }

  // Reading a Event object should be governed at least MDL_SHARED.
  static bool is_read_locked(THD *thd, const dd::Event *event)
  {
    return (thd->is_dd_system_thread() ||
            is_locked(thd, event, MDL_SHARED));
  }

  // Writing a Event object should be governed by MDL_EXCLUSIVE.
  static bool is_write_locked(THD *thd, const dd::Event *event)
  {
    return (thd->is_dd_system_thread() ||
            is_locked(thd, event, MDL_EXCLUSIVE));
  }

  // Reading a Routine object should be governed at least MDL_SHARED.
  static bool is_read_locked(THD *thd, const dd::Routine *routine)
  {
    return (thd->is_dd_system_thread() ||
            is_locked(thd, routine, MDL_SHARED));
  }

  // Writing a Routine object should be governed by MDL_EXCLUSIVE.
  static bool is_write_locked(THD *thd, const dd::Routine *routine)
  {
    return (thd->is_dd_system_thread() ||
            is_locked(thd, routine, MDL_EXCLUSIVE));
  }
};

// Check if the component is hidden.
template <typename T>
bool is_component_hidden(dd::Raw_record*)
{ return false; }

template <>
bool is_component_hidden<dd::Abstract_table>(dd::Raw_record *r)
{ return r->read_bool(dd::tables::Tables::FIELD_HIDDEN); }

}


namespace dd {
namespace cache {

// Transfer an object from the current to the previous auto releaser.
template <typename T>
void Dictionary_client::Auto_releaser::transfer_release(const T* object)
{
  DBUG_ASSERT(object);
  // Remove the object, which must be present.
  Cache_element<T> *element= NULL;
  m_release_registry.get(object, &element);
  DBUG_ASSERT(element);
  m_release_registry.remove(element);
  m_prev->auto_release(element);
}


// Remove an element from some auto releaser down the chain.
template <typename T>
Dictionary_client::Auto_releaser *Dictionary_client::Auto_releaser::remove(
    Cache_element<T> *element)
{
  DBUG_ASSERT(element);
  // Scan the auto releaser linked list and remove the element.
  for (Auto_releaser *releaser= this; releaser != NULL;
       releaser= releaser->m_prev)
  {
     Cache_element<T> *e= NULL;
     releaser->m_release_registry.get(element->object(), &e);
     if (e == element)
     {
       releaser->m_release_registry.remove(element);
       return releaser;
     }
  }
  // The element must be present in some auto releaser.
  DBUG_ASSERT(false); /* purecov: deadcode */
  return NULL;
}


// Create a new empty auto releaser.
Dictionary_client::Auto_releaser::Auto_releaser(): m_client(NULL), m_prev(NULL)
{ }


// Create a new auto releaser and link it into the dictionary client
// as the current releaser.
Dictionary_client::Auto_releaser::Auto_releaser(Dictionary_client *client):
  m_client(client), m_prev(client->m_current_releaser)
{ m_client->m_current_releaser= this; }


// Release all objects registered and restore previous releaser.
Dictionary_client::Auto_releaser::~Auto_releaser()
{
  // Release all objects registered.
  m_client->release<Abstract_table>(&m_release_registry);
  m_client->release<Schema>(&m_release_registry);
  m_client->release<Tablespace>(&m_release_registry);
  m_client->release<Charset>(&m_release_registry);
  m_client->release<Collation>(&m_release_registry);
  m_client->release<Event>(&m_release_registry);
  m_client->release<Routine>(&m_release_registry);
  m_client->release<Spatial_reference_system>(&m_release_registry);

  // Restore the client's previous releaser.
  m_client->m_current_releaser= m_prev;

  // Delete any remaining uncommitted or uncached objects if we only have
  // the default releaser left. If any objects remain, we probably aborted
  // the transaction.
  if (m_client->m_current_releaser == &m_client->m_default_releaser)
  {
    // We should either have reported an error or have removed all
    // uncommitted objects (typically committed them to the shared cache).
    DBUG_ASSERT(m_client->m_thd->is_error() ||
                m_client->m_thd->killed ||
                (m_client->m_registry_uncommitted.size_all() == 0 &&
                 m_client->m_registry_dropped.size_all() == 0));

    m_client->m_registry_uncommitted.erase_all();
    m_client->m_registry_dropped.erase_all();

    // Delete any objects retrieved by acquire_uncached() or
    // acquire_for_modification().
    delete_container_pointers(m_client->m_uncached_objects);
  }
}


// Debug dump to stderr.
template <typename T>
void Dictionary_client::Auto_releaser::dump() const
{
#ifndef DBUG_OFF
  fprintf(stderr, "================================\n");
  fprintf(stderr, "Auto releaser\n");
  m_release_registry.dump<T>();
  fprintf(stderr, "================================\n");
  fflush(stderr);
#endif
}


// Get a dictionary object.
template <typename K, typename T>
bool Dictionary_client::acquire(const K &key, const T **object,
                                bool *local_committed, bool *local_uncommitted)
{
  DBUG_ASSERT(object);
  DBUG_ASSERT(local_committed);
  DBUG_ASSERT(local_uncommitted);
  *object= NULL;

  DBUG_EXECUTE_IF("fail_while_acquiring_dd_object",
  {
    my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
    return true;
  });

  // Lookup in registry of uncommitted objects
  T *uncommitted_object= nullptr;
  bool dropped= false;
  acquire_uncommitted(key, &uncommitted_object, &dropped);
  if (uncommitted_object || dropped)
  {
    *local_committed= false;
    *local_uncommitted= true;
    *object= uncommitted_object;
    return false;
  }
  *local_uncommitted= false;

  // Lookup in the registry of committed objects.
  Cache_element<T> *element= NULL;
  m_registry_committed.get(key, &element);
  if (element)
  {
    // Check if an uncommitted object with the same id exists.
    // If so, the object has been renamed or dropped, and we should
    // return nothing.
    const typename T::id_key_type id_key(element->object()->id());
    acquire_uncommitted(id_key, &uncommitted_object, &dropped);
    DBUG_ASSERT(!dropped);
    if (uncommitted_object || dropped)
      return false;

    // Object has not been renamed
    *local_committed= true;
    *object= element->object();
    // Check proper MDL lock.
    DBUG_ASSERT(MDL_checker::is_read_locked(m_thd, *object));
    return false;
  }

  // The element is not present locally.
  *local_committed= false;

  // Get the object from the shared cache.
  if (Shared_dictionary_cache::instance()->get(m_thd, key, &element))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  // Add the element to the local registry and assign the output object.
  if (element)
  {
    // Recheck that we haven't renamed or dropped this object.
    const typename T::id_key_type id_key(element->object()->id());
    acquire_uncommitted(id_key, &uncommitted_object, &dropped);
    if (uncommitted_object || dropped)
    {
      // Here, we drop the object from the shared cache. If the
      // object has been dropped, we would otherwise contaminate
      // the shared cache. For simplicity, we drop it also in the
      // case of a modified (i.e., renamed) object. This would also
      // be handled in remove_uncommitted_objects() when the
      // shared cache is updated with the modified objects.
      DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, element->object()));
      Shared_dictionary_cache::instance()->drop(element);
      return false;
    }

    DBUG_ASSERT(element->object() && element->object()->id());
    // Sign up for auto release.
    m_registry_committed.put(element);
    m_current_releaser->auto_release(element);
    *object= element->object();
    // Check proper MDL lock.
    DBUG_ASSERT(MDL_checker::is_read_locked(m_thd, *object));
  }
  return false;
}


template <typename K, typename T>
void Dictionary_client::acquire_uncommitted(const K &key,
                                            T **object,
                                            bool *dropped)
{
  DBUG_ASSERT(object);
  DBUG_ASSERT(dropped);
  *object= nullptr;
  *dropped= false;

  Object_id uncommitted_id= INVALID_OBJECT_ID;
  Object_id dropped_id= INVALID_OBJECT_ID;

  Cache_element<T> *element= NULL;
  m_registry_uncommitted.get(key, &element);
  if (element)
  {
    *object= const_cast<T*>(element->object()); // TODO: Const cast
    // Check proper MDL lock.
    DBUG_ASSERT(MDL_checker::is_read_locked(m_thd, *object));
    uncommitted_id= (*object)->id();
  }

  m_registry_dropped.get(key, &element);
  if (element)
  {
    const T *dropped_object= element->object();
    // Check proper MDL lock.
    DBUG_ASSERT(MDL_checker::is_read_locked(m_thd, dropped_object));
    dropped_id= dropped_object->id();
  }

  // The object should never be present in both registries with the same id.
  DBUG_ASSERT(uncommitted_id != dropped_id ||
              dropped_id == INVALID_OBJECT_ID);

  *dropped= (dropped_id != INVALID_OBJECT_ID &&
             uncommitted_id == INVALID_OBJECT_ID);

  // If dropped, we return nullptr.
  if (*dropped)
    *object= nullptr;
}


// Mark all objects of a certain type as not being used by this client.
template <typename T>
size_t Dictionary_client::release(Object_registry *registry)
{
  DBUG_ASSERT(registry);
  size_t num_released= 0;

  // Iterate over all elements in the registry partition.
  typename Multi_map_base<T>::Const_iterator it;
  for (it= registry->begin<T>();
       it != registry->end<T>();
       ++num_released)
  {
    DBUG_ASSERT(it->second);
    DBUG_ASSERT(it->second->object());

    // Make sure we handle iterator invalidation: Increment
    // before erasing.
    Cache_element<T> *element= it->second;
    ++it;

    // Remove the element from the actual registry.
    registry->remove(element);

    // Remove the element from the client's object registry.
    if (registry != &m_registry_committed)
      m_registry_committed.remove(element);
    else
      (void) m_current_releaser->remove(element);

    // Clone the object before releasing it. The object is needed for checking
    // the meta data lock afterwards.
#ifndef DBUG_OFF
    std::unique_ptr<const T> object_clone(element->object()->clone());
#endif

    // Release the element from the shared cache.
    Shared_dictionary_cache::instance()->release(element);

    // Make sure we still have some meta data lock. This is checked to
    // catch situations where we have released the lock before releasing
    // the cached element. This will happen if we, e.g., declare a
    // Schema_MDL_locker after the Auto_releaser which keeps track of when
    // the elements are to be released. In that case, the instances will
    // be deleted in the opposite order, hence there will be a short period
    // where the schema locker is deleted (and hence, its MDL ticket is
    // released) while the actual schema object is still not released. This
    // means that there may be situations where we have a different thread
    // getting an X meta data lock on the schema name, while the reference
    // counter of the corresponding cache element is already > 0, which may
    // again trigger asserts in the shared cache and allow for improper object
    // usage.
    DBUG_ASSERT(MDL_checker::is_release_locked(m_thd, object_clone.get()));
  }
  return num_released;
}


// Release all objects in the submitted object registry.
size_t Dictionary_client::release(Object_registry *registry)
{
  return release<Abstract_table>(registry) +
          release<Schema>(registry) +
          release<Tablespace>(registry) +
          release<Charset>(registry) +
          release<Collation>(registry) +
          release<Event>(registry) +
          release<Routine>(registry) +
          release<Spatial_reference_system>(registry);
}


// Initialize an instance with a default auto releaser.
Dictionary_client::Dictionary_client(THD *thd): m_thd(thd),
        m_current_releaser(&m_default_releaser)
{
  DBUG_ASSERT(m_thd);
  // We cannot fully initialize the m_default_releaser in the member
  // initialization list since 'this' isn't fully initialized at that point.
  // Thus, we do it here.
  m_default_releaser.m_client= this;
}


// Make sure all objects are released.
Dictionary_client::~Dictionary_client()
{
  // Release the objects left in the object registry (should be empty).
  size_t num_released= release();
  DBUG_ASSERT(num_released == 0);
  if (num_released > 0)
  {
    sql_print_warning("Dictionary objects used but not released.");
  }

  // Delete the additional releasers (should be none).
  while (m_current_releaser &&
         m_current_releaser != &m_default_releaser)
  {
    /* purecov: begin deadcode */
    sql_print_warning("Dictionary object auto releaser not deleted");
    DBUG_ASSERT(false);
    delete m_current_releaser;
    /* purecov: end */
  }

  // Finally, release the objects left in the default releaser
  // (should be empty).
  num_released= release(&m_default_releaser.m_release_registry);
  DBUG_ASSERT(num_released == 0);
  if (num_released > 0)
  {
    sql_print_warning("Dictionary objects left in default releaser.");
  }
}


// Retrieve an object by its object id.
template <typename T>
bool Dictionary_client::acquire(Object_id id, const T** object)
{
  const typename T::id_key_type key(id);
  const typename T::cache_partition_type *cached_object= NULL;

  // We must be sure the object is released correctly if dynamic cast fails.
  Auto_releaser releaser(this);

  bool local_committed= false;
  bool local_uncommitted= false;
  bool error= acquire(key, &cached_object,
                      &local_committed, &local_uncommitted);

  if (!error)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    *object= dynamic_cast<const T*>(cached_object);

    // Don't auto release the object here if it is returned.
    if (!local_committed && !local_uncommitted && *object)
      releaser.transfer_release(cached_object);
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


template <typename T>
bool Dictionary_client::acquire_for_modification(Object_id id, T** object)
{
  const typename T::id_key_type key(id);
  const typename T::cache_partition_type *cached_object= NULL;

  // We must be sure the object is released correctly if dynamic cast fails.
  Auto_releaser releaser(this);

  bool local_committed= false;
  bool local_uncommitted= false;
  bool error= acquire(key, &cached_object,
                      &local_committed, &local_uncommitted);

  if (!error)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    const T* casted= dynamic_cast<const T*>(cached_object);

    if (!casted)
      *object= nullptr;
    else
    {
      *object= casted->clone();
      auto_delete<T>(*object);
    }
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve an object by its object id without caching it.
template <typename T>
bool Dictionary_client::acquire_uncached(Object_id id, T** object)
{
  const typename T::id_key_type key(id);
  const typename T::cache_partition_type *stored_object= NULL;

  // Read the uncached dictionary object.
  bool error= Shared_dictionary_cache::instance()->
                get_uncached(m_thd, key, ISO_READ_COMMITTED, &stored_object);
  if (!error)
  {
    // We do not verify proper MDL locking here since the
    // returned object is owned by the caller.

    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    // TODO: Replace const_cast by directly using Storage_adapter
    *object= const_cast<T*>(dynamic_cast<const T*>(stored_object));

    // Delete the object if dynamic cast fails.
    if (stored_object && !*object)
      delete stored_object;
    else
      auto_delete<T>(*object);
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve an object by its object id without caching it.
template <typename T>
bool Dictionary_client::acquire_uncached_uncommitted(Object_id id,
                                                     T** object)
{
  const typename T::id_key_type key(id);
  DBUG_ASSERT(object);

  // First get the object from acquire_uncommitted. This should be safe
  // even without MDL, since the object is only available to this thread.
  typename T::cache_partition_type *uncommitted_object= nullptr;
  bool dropped= false;
  acquire_uncommitted(key, &uncommitted_object, &dropped);

  // In this case, if the object has been dropped, we return nullptr since
  // this is in line with the isolation level for the disk access.
  if (dropped)
  {
    *object= nullptr;
    return false;
  }

  if (uncommitted_object != nullptr)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return, but in this
    // case, we cannot delete the stored_object since it is present
    // in the uncommitted registry. The returned object, however,
    // must be auto deleted.
    *object= const_cast<T*>(dynamic_cast<const T*>(uncommitted_object->clone()));
    if (*object != nullptr)
      auto_delete<T>(*object);
    return false;
  }

  // Read the uncached dictionary object using ISO_READ_UNCOMMITTED
  // isolation level.
  const typename T::cache_partition_type *stored_object= nullptr;
  bool error= Shared_dictionary_cache::instance()->
                get_uncached(m_thd, key, ISO_READ_UNCOMMITTED, &stored_object);
  if (!error)
  {
    // Here, stored_object is a newly created instance, so we do not need to
    // clone() it, but we must delete it if dynamic cast fails.
    *object= const_cast<T*>(dynamic_cast<const T*>(stored_object));
    if (stored_object && !*object)
      delete stored_object;
    else
      auto_delete<T>(*object);
  }
  else
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);

  return error;
}


// Retrieve an object by its name.
template <typename T>
bool Dictionary_client::acquire(const String_type &object_name,
                                const T** object)
{
  // Create the name key for the object.
  typename T::name_key_type key;
  bool error= T::update_name_key(&key, object_name);
  if (error)
  {
    my_error(ER_INVALID_DD_OBJECT_NAME, MYF(0), object_name.c_str());
    return true;
  }

  // We must be sure the object is released correctly if dynamic cast fails.
  Auto_releaser releaser(this);
  const typename T::cache_partition_type *cached_object= NULL;

  bool local_committed= false;
  bool local_uncommitted = false;
  error= acquire(key, &cached_object,
                 &local_committed, &local_uncommitted);

  if (!error)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    *object= dynamic_cast<const T*>(cached_object);

    // Don't auto release the object here if it is returned.
    if (!local_committed && !local_uncommitted && *object)
      releaser.transfer_release(cached_object);
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


template <typename T>
bool Dictionary_client::acquire_for_modification(const String_type &object_name,
                                                 T** object)
{
  // Create the name key for the object.
  typename T::name_key_type key;
  bool error= T::update_name_key(&key, object_name);
  if (error)
  {
    my_error(ER_INVALID_DD_OBJECT_NAME, MYF(0), object_name.c_str());
    return true;
  }

  // We must be sure the object is released correctly if dynamic cast fails.
  Auto_releaser releaser(this);
  const typename T::cache_partition_type *cached_object= NULL;

  bool local_committed= false;
  bool local_uncommitted= false;
  error= acquire(key, &cached_object,
                 &local_committed, &local_uncommitted);

  if (!error)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    const T* casted= dynamic_cast<const T*>(cached_object);

    if (!casted)
      *object= nullptr;
    else
    {
      *object= casted->clone();
      auto_delete<T>(*object);
    }
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve an object by its schema- and object name.
template <typename T>
bool Dictionary_client::acquire(const String_type &schema_name,
                                const String_type &object_name,
                                const T** object)
{
  // We must make sure the schema is released and unlocked in the right order.
  Schema_MDL_locker mdl_locker(m_thd);
  Auto_releaser releaser(this);

  DBUG_ASSERT(object);
  *object= NULL;

  // Get the schema object by name.
  const Schema *schema= NULL;
  bool error= mdl_locker.ensure_locked(schema_name.c_str()) ||
              acquire(schema_name, &schema);

  // If there was an error, or if we found no valid schema, return here.
  if (error)
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  // A non existing schema is not reported as an error.
  if (!schema)
    return false;

  DEBUG_SYNC(m_thd, "acquired_schema_while_acquiring_table");

  // Create the name key for the object.
  typename T::name_key_type key;
  T::update_name_key(&key, schema->id(), object_name);

  // Acquire the dictionary object.
  const typename T::cache_partition_type *cached_object= NULL;

  bool local_committed= false;
  bool local_uncommitted= false;
  error= acquire(key, &cached_object,
                 &local_committed, &local_uncommitted);

  if (!error)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    *object= dynamic_cast<const T*>(cached_object);

    // Don't auto release the object here if it is returned.
    if (!local_committed && !local_uncommitted && *object)
      releaser.transfer_release(cached_object);
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


template <typename T>
bool Dictionary_client::acquire_for_modification(const String_type &schema_name,
                                                 const String_type &object_name,
                                                 T** object)
{
  // We must make sure the schema is released and unlocked in the right order.
  Schema_MDL_locker mdl_locker(m_thd);
  Auto_releaser releaser(this);

  DBUG_ASSERT(object);
  *object= NULL;

  // Get the schema object by name.
  const Schema *schema= NULL;
  bool error= mdl_locker.ensure_locked(schema_name.c_str()) ||
              acquire(schema_name, &schema);

  // If there was an error, or if we found no valid schema, return here.
  if (error)
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  // A non existing schema is not reported as an error.
  if (!schema)
    return false;

  // Create the name key for the object.
  typename T::name_key_type key;
  T::update_name_key(&key, schema->id(), object_name);

  // Acquire the dictionary object.
  const typename T::cache_partition_type *cached_object= NULL;

  bool local_committed= false;
  bool local_uncommitted= false;
  error= acquire(key, &cached_object,
                 &local_committed, &local_uncommitted);

  if (!error)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    const T* casted= dynamic_cast<const T*>(cached_object);

    if (!casted)
      *object= nullptr;
    else
    {
      *object= casted->clone();
      auto_delete<T>(*object);
    }
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve an object by its schema- and object name. Return as double
// pointer to base type.
template <typename T>
bool Dictionary_client::acquire(const String_type &schema_name,
                                const String_type &object_name,
                                const typename T::cache_partition_type** object)
{
  // We must make sure the schema is released and unlocked in the right order.
  Schema_MDL_locker mdl_locker(m_thd);
  Auto_releaser releaser(this);

  DBUG_ASSERT(object);
  *object= NULL;

  // Get the schema object by name.
  const Schema *schema= NULL;
  bool error= mdl_locker.ensure_locked(schema_name.c_str()) ||
              acquire(schema_name, &schema);

  // If there was an error, or if we found no valid schema, return here.
  if (error)
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }

  // A non existing schema is not reported as an error.
  if (!schema)
    return false;

  DEBUG_SYNC(m_thd, "acquired_schema_while_acquiring_table");

  // Create the name key for the object.
  typename T::name_key_type key;
  T::update_name_key(&key, schema->id(), object_name);

  // Acquire the dictionary object.
  bool local_committed= false;
  bool local_uncommitted= false;
  error= acquire(key, object,
                 &local_committed, &local_uncommitted);

  if (!error)
  {
    // No downcasting is necessary here.
    // Don't auto release the object here if it is returned.
    if (!local_committed && !local_uncommitted && *object)
      releaser.transfer_release(*object);
  }
  else
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);

  return error;
}


template <typename T>
bool Dictionary_client::acquire_for_modification(const String_type &schema_name,
                                                 const String_type &object_name,
                                                 typename T::cache_partition_type** object)
{
  // We must make sure the schema is released and unlocked in the right order.
  Schema_MDL_locker mdl_locker(m_thd);
  Auto_releaser releaser(this);

  DBUG_ASSERT(object);
  *object= NULL;

  // Get the schema object by name.
  const Schema *schema= NULL;
  bool error= mdl_locker.ensure_locked(schema_name.c_str()) ||
              acquire(schema_name, &schema);

  // If there was an error, or if we found no valid schema, return here.
  if (error)
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  // A non existing schema is not reported as an error.
  if (!schema)
    return false;

  // Create the name key for the object.
  typename T::name_key_type key;
  T::update_name_key(&key, schema->id(), object_name);

  // Acquire the dictionary object.
  const typename T::cache_partition_type *cached_object= NULL;

  bool local_committed= false;
  bool local_uncommitted= false;
  error= acquire(key, &cached_object,
                 &local_committed, &local_uncommitted);

  if (!error)
  {
    // Cast not necessary here since we return the T::cache_partition_type.
    if (cached_object != nullptr)
    {
      *object= cached_object->clone();
      auto_delete<T>(*object);
    }
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve a table object by its se private id.
bool Dictionary_client::acquire_uncached_table_by_se_private_id(
                                      const String_type &engine,
                                      Object_id se_private_id,
                                      Table **table)
{
  DBUG_ASSERT(table);
  *table= NULL;

  // Create se private key.
  Table::aux_key_type key;
  Table::update_aux_key(&key, engine, se_private_id);

  const Table::cache_partition_type *stored_object= NULL;

  // Read the uncached dictionary object.
  if (Shared_dictionary_cache::instance()->
        get_uncached(m_thd, key, ISO_READ_COMMITTED, &stored_object))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  // If object was not found.
  if (stored_object == NULL)
    return false;

  // Dynamic cast may legitimately return NULL only if the stored object
  // was NULL, i.e., the object did not exist.
  // TODO: Replace const_cast by directly using Storage_adapter
  *table= const_cast<Table*>(dynamic_cast<const Table*>(stored_object));

  // Delete the object and report error if dynamic cast fails.
  if (!*table)
  {
    my_error(ER_INVALID_DD_OBJECT, MYF(0),
             Table::OBJECT_TABLE().name().c_str(),
             "Not a table object.");
    delete stored_object;
    return true;
  }
  else
    auto_delete<Table>(*table);

  return false;
}

// Retrieve a table object by its partition se private id.
/* purecov: begin deadcode */
bool Dictionary_client::acquire_uncached_table_by_partition_se_private_id(
                          const String_type &engine,
                          Object_id se_partition_id,
                          Table **table)
{
  DBUG_ASSERT(table);
  *table= NULL;

  // Read record directly from the tables.
  Object_id table_id;
  if (tables::Table_partitions::get_partition_table_id(m_thd, engine,
                                                       se_partition_id,
                                                       &table_id))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  if (table_id == INVALID_OBJECT_ID)
    return false;

  if (acquire_uncached(table_id, table))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  if (*table == NULL)
    return false;

  return false;
}
/* purecov: end */


// Get names of index and column names from index statistics entries.
static bool get_index_statistics_entries(THD *thd,
                                  const String_type &schema_name,
                                  const String_type &table_name,
                                  std::vector<String_type> &index_names,
                                  std::vector<String_type> &column_names)
{
  /*
    Use READ UNCOMMITTED isolation, so this method works correctly when
    called from the middle of atomic ALTER TABLE statement.
  */
  dd::Transaction_ro trx(thd, ISO_READ_UNCOMMITTED);

  // Open the DD tables holding dynamic table statistics.
  trx.otx.register_tables<dd::Table_stat>();
  trx.otx.register_tables<dd::Index_stat>();
  if (trx.otx.open_tables())
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);
    return true;
  }

  // Create the range key based on schema and table name.
  std::unique_ptr<Object_key> object_key(
    dd::tables::Index_stats::create_range_key_by_table_name(
                               schema_name, table_name));

  Raw_table *table= trx.otx.get_table<dd::Index_stat>();
  DBUG_ASSERT(table);

  // Start the scan.
  std::unique_ptr<Raw_record_set> rs;
  if (table->open_record_set(object_key.get(), rs))
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);
    return true;
  }

  // Read each index entry.
  Raw_record *r= rs->current_record();
  while (r)
  {
    // Read index and column names.
    index_names.push_back(r->read_str(tables::Index_stats::FIELD_INDEX_NAME));
    column_names.push_back(r->read_str(tables::Index_stats::FIELD_COLUMN_NAME));

    if (rs->next(r))
    {
      DBUG_ASSERT(thd->is_error() || thd->killed);
      return true;
    }
  }

  return false;
}


/*
  Remove the dynamic statistics stored in mysql.table_stats and
  mysql.index_stats.
*/
bool Dictionary_client::remove_table_dynamic_statistics(
                                          const String_type &schema_name,
                                          const String_type &table_name)
{
  //
  // Get list of index statistics entries.
  //

  std::vector<String_type> index_names, column_names;
  if (get_index_statistics_entries(m_thd,
                                   schema_name,
                                   table_name,
                                   index_names,
                                   column_names))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }


  //
  // Drop index statistics entries for the table.
  //

  // Iterate and drop each index statistic entry, if exists.
  if (!index_names.empty())
  {
    const Index_stat *idx_stat= nullptr;
    std::vector<String_type>::iterator it_idxs= index_names.begin();
    std::vector<String_type>::iterator it_cols= column_names.begin();
    while(it_idxs != index_names.end())
    {
      // Fetch the entry.
      std::unique_ptr<Index_stat::name_key_type> key(
        tables::Index_stats::create_object_key(schema_name,
                                               table_name,
                                               *it_idxs,
                                               *it_cols));

      /*
        Use READ UNCOMMITTED isolation, so this method works correctly when
        called from the middle of atomic ALTER TABLE statement.
      */
      if (Storage_adapter::get(m_thd, *key, ISO_READ_UNCOMMITTED, &idx_stat))
      {
        DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
        return true;
      }

      // Drop the entry.
      if (idx_stat &&
          Storage_adapter::drop(m_thd, const_cast<Index_stat*>(idx_stat)))
      {
        DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
        return true;
      }

      delete idx_stat;
      idx_stat= nullptr;

      it_idxs++;
      it_cols++;
    }
  }


  //
  // Drop the table statistics entry.
  //

  // Fetch the entry.
  std::unique_ptr<Table_stat::name_key_type> key(
    tables::Table_stats::create_object_key(schema_name, table_name));

  /*
    Use READ UNCOMMITTED isolation, so this method works correctly when
    called from the middle of atomic ALTER TABLE statement.
  */
  const Table_stat *tab_stat= nullptr;
  if (Storage_adapter::get(m_thd, *key, ISO_READ_UNCOMMITTED, &tab_stat))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }

  // Drop the entry.
  if (tab_stat &&
      Storage_adapter::drop(m_thd, const_cast<Table_stat*>(tab_stat)))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }

  delete tab_stat;

  return false;
}


// Retrieve a schema- and table name by the se private id of the table.
bool Dictionary_client::get_table_name_by_se_private_id(
                                    const String_type &engine,
                                    Object_id se_private_id,
                                    String_type *schema_name,
                                    String_type *table_name)
{
  // Objects to be acquired.
  Table *tab_obj= NULL;
  Schema *sch_obj= NULL;

  // Store empty in OUT params.
  DBUG_ASSERT(schema_name && table_name);
  schema_name->clear();
  table_name->clear();

  // Acquire the table uncached, because we cannot acquire a meta data
  // lock since we do not know the table name.
  if (acquire_uncached_table_by_se_private_id(engine, se_private_id, &tab_obj))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  // Object not found.
  if (!tab_obj)
    return false;

  // Acquire the schema uncached to get the schema name. Like above, we
  // cannot lock it in advance since we do not know its name.
  if (acquire_uncached(tab_obj->schema_id(), &sch_obj))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  if (!sch_obj)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_name->c_str());
    return true;
  }

  // Now, we have both objects, and can assign the names.
  *schema_name= sch_obj->name();
  *table_name= tab_obj->name();

  return false;
}


// Retrieve a schema- and table name by the se private id of the partition.
/* purecov: begin deadcode */
bool Dictionary_client::get_table_name_by_partition_se_private_id(
                                      const String_type &engine,
                                      Object_id se_partition_id,
                                      String_type *schema_name,
                                      String_type *table_name)
{
  Table *tab_obj= NULL;
  Schema *sch_obj= NULL;

  // Store empty in OUT params.
  DBUG_ASSERT(schema_name && table_name);
  schema_name->clear();
  table_name->clear();

  if (acquire_uncached_table_by_partition_se_private_id(
        engine, se_partition_id, &tab_obj))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  // Object not found.
  if (!tab_obj)
    return false;

  // Acquire the schema to get the schema name.
  if (acquire_uncached(tab_obj->schema_id(), &sch_obj))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  if (!sch_obj)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_name->c_str());
    return true;
  }

  // Now, we have both objects, and can assign the names.
  *schema_name= sch_obj->name();
  *table_name= tab_obj->name();

  return false;
}
/* purecov: end */

bool Dictionary_client::get_table_name_by_trigger_name(
                          Object_id schema_id,
                          const String_type &trigger_name,
                          String_type *table_name)
{
  DBUG_ASSERT(table_name != nullptr);
  *table_name= "";

  // Read record directly from the tables.
  Object_id table_id;
  if (tables::Triggers::get_trigger_table_id(m_thd,
                                             schema_id,
                                             trigger_name,
                                             &table_id))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }

  const Table::id_key_type key(table_id);
  const Table::cache_partition_type *stored_object= nullptr;

  bool error= Shared_dictionary_cache::instance()->
                get_uncached(m_thd, key, ISO_READ_COMMITTED, &stored_object);

  if (!error)
  {
    // Dynamic cast may legitimately return nullptr if the stored
    // object was nullptr, i.e., the object did not exist.
    const Table *table= dynamic_cast<const Table*>(stored_object);

    // Delete the object and report error if dynamic cast fails.
    if (stored_object != nullptr && table == nullptr)
    {
      my_error(ER_INVALID_DD_OBJECT, MYF(0),
                Table::OBJECT_TABLE().name().c_str(),
                "Not a table object.");
      delete stored_object;
      return true;
    }

    // Copy the table name to OUT param.
    if (table != nullptr)
    {
      *table_name= table->name();
      delete stored_object;
    }
  }
  else
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);

  return error;
}


// Get the highest currently used se private id for the table objects.
/* purecov: begin deadcode */
bool Dictionary_client::get_tables_max_se_private_id(const String_type &engine,
                                                     Object_id *max_id)
{
  dd::Transaction_ro trx(m_thd, ISO_READ_COMMITTED);

  trx.otx.register_tables<dd::Schema>();
  trx.otx.register_tables<dd::Table>();

  if (trx.otx.open_tables())
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  return dd::tables::Tables::max_se_private_id(&trx.otx, engine, max_id);
}
/* purecov: end */


// Fetch the names of all the components in the schema.
template <typename T>
bool Dictionary_client::fetch_schema_component_names(
    const Schema *schema,
    std::vector<String_type> *names) const
{
  DBUG_ASSERT(names);

  // Create the key based on the schema id.
  std::unique_ptr<Object_key> object_key(
    T::cache_partition_table_type::create_key_by_schema_id(schema->id()));

  // Retrieve a set of the schema components, and add the component names
  // to the vector output parameter.
  Transaction_ro trx(m_thd, ISO_READ_COMMITTED);

  trx.otx.register_tables<T>();
  Raw_table *table= trx.otx.get_table<T>();
  DBUG_ASSERT(table);

  if (trx.otx.open_tables())
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  std::unique_ptr<Raw_record_set> rs;
  if (table->open_record_set(object_key.get(), rs))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  Raw_record *r= rs->current_record();
  String_type s;
  while (r)
  {
    // Get the table name, but only unless the object is hidden.
    if (!is_component_hidden<T>(r))
      names->push_back(r->read_str(T::cache_partition_table_type::FIELD_NAME));

    if (rs->next(r))
    {
      DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
      return true;
    }
  }

  return false;
}


/**
  Fetch objects from DD tables that match the supplied key.

  @tparam Object_type Type of object to fetch.
  @param thd          Thread handle
  @param coll         Vector to fill with objects.
  @param object_key   The search key. If key is not supplied, then
                      we do full index scan.

  @return false       Success.
  @return true        Failure (error is reported).
*/

template <typename Object_type>
bool fetch(THD *thd, std::vector<const Object_type*> *coll,
           const Object_key *object_key)
{
  // Since we clear the vector on failure, it should be empty
  // when we start.
  DBUG_ASSERT(coll->empty());

  std::vector<Object_id> ids;

  {
    Transaction_ro trx(thd, ISO_READ_COMMITTED);
    trx.otx.register_tables<Object_type>();
    Raw_table *table= trx.otx.get_table<Object_type>();
    DBUG_ASSERT(table);

    if (trx.otx.open_tables())
    {
      DBUG_ASSERT(thd->is_system_thread() || thd->killed || thd->is_error());
      return true;
    }

    // Retrieve list of object ids. Do this in a nested scope to make sure
    // the record set is deleted before the transaction is committed (a
    // dependency in the Raw_record_set destructor.
    {
      std::unique_ptr<Raw_record_set> rs;
      if (table->open_record_set(object_key, rs))
      {
        DBUG_ASSERT(thd->is_system_thread() || thd->killed || thd->is_error());
        return true;
      }

      Raw_record *r= rs->current_record();
      while (r)
      {
        ids.push_back(r->read_int(0)); // Read ID, which is always 1st field.

        if (rs->next(r))
        {
          DBUG_ASSERT(thd->is_system_thread() ||
                      thd->killed || thd->is_error());
          return true;
        }
      }
    }

    // Close the scope to end DD transaction. This allows to avoid
    // nested DD transaction when loading objects.
  }

  // Load objects by id. This must be done without caching the
  // objects since the dictionary object collection is used in
  // situations where we do not have an MDL lock (e.g. a SHOW statement).
  for (Object_id id : ids)
  {
    Object_type *o= NULL;
    if (thd->dd_client()->acquire_uncached(id, &o))
    {
      DBUG_ASSERT(thd->is_system_thread() || thd->killed || thd->is_error());
      // Delete objects already created.
      for (const Object_type *comp : *coll)
        delete comp;
      coll->clear();
      return true;
    }

    // Since we don't have metadata lock, the object could have been
    // deleted after we retrieved the IDs. So we need to check that
    // the object still exists and it is not an error if it doesn't.
    if (o)
      coll->push_back(o);
  }

  return false;
}


// Fetch all components in the schema.
template <typename T>
bool Dictionary_client::fetch_schema_components(
    const Schema *schema,
    std::vector<const T*> *coll) const
{
  std::unique_ptr<Object_key> k(
    T::cache_partition_table_type::create_key_by_schema_id(schema->id()));

  if (fetch(m_thd, coll, k.get()))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    DBUG_ASSERT(coll->empty());
    return true;
  }

  return false;
}


// Fetch all global components of the given type.
template <typename T>
bool Dictionary_client::fetch_global_components(
    std::vector<const T*> *coll) const
{
  if (fetch(m_thd, coll, NULL))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    DBUG_ASSERT(coll->empty());
    return true;
  }

  return false;
}


template <typename T>
bool Dictionary_client::fetch_referencing_views_object_id(
  const char *schema,
  const char *tbl_or_sf_name,
  std::vector<Object_id> *view_ids) const
{
  /*
    Use READ UNCOMMITTED isolation, so this method works correctly when
    called from the middle of atomic DROP TABLE/DATABASE or
    RENAME TABLE statements.
  */
  dd::Transaction_ro trx(m_thd, ISO_READ_UNCOMMITTED);

  // Register View_table_usage/View_routine_usage.
  trx.otx.register_tables<T>();
  Raw_table *view_usage_table= trx.otx.get_table<T>();
  DBUG_ASSERT(view_usage_table);

  // Open registered tables.
  if (trx.otx.open_tables())
  {
    DBUG_ASSERT(m_thd->is_system_thread() ||
                m_thd->killed ||
                m_thd->is_error());
    return true;
  }

  // Create the key based on the base table/ view/ stored function name.
  std::unique_ptr<Object_key> object_key(
    T::cache_partition_table_type::create_key_by_name(
      String_type(Dictionary_impl::default_catalog_name()),
      String_type(schema),
      String_type(tbl_or_sf_name)));
  std::unique_ptr<Raw_record_set> rs;
  if (view_usage_table->open_record_set(object_key.get(), rs))
  {
    DBUG_ASSERT(m_thd->is_system_thread() ||
                m_thd->killed ||
                m_thd->is_error());
    return true;
  }

  Raw_record *vtr= rs->current_record();
  while (vtr)
  {
    /* READ VIEW ID */
    Object_id id= vtr->read_int(T::cache_partition_table_type::FIELD_VIEW_ID);
    view_ids->push_back(id);

    if (rs->next(vtr))
    {
      DBUG_ASSERT(m_thd->is_system_thread() ||
                  m_thd->killed ||
                  m_thd->is_error());
      return true;
    }
  }

  return false;
}


// Mark all objects acquired by this client as not being used anymore.
size_t Dictionary_client::release()
{ return release(&m_registry_committed); }


// Remove and delete an object from the cache and the dd tables.
template <typename T>
bool Dictionary_client::drop(const T *object)
{
  // Check proper MDL lock.
  DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, object));

  if (Storage_adapter::drop(m_thd, object))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  // Prepare an instance to be added to the dropped registry. This must be done
  // prior to cleaning up the committed registry since the instance we drop might
  // be present there (since we are allowed to drop const object coming from
  // acquire()).
  T *dropped_object= object->clone();

  // The shared cache is updated right away. This is safe since we have
  // MDL on the name. Further acquisition from this thread will see
  // that the object is dropped (by inspecting the dropped registry),
  // so the shared cache will not be "polluted" by this thread either.

  // Uncommitted object which was acquired for modification might have
  // corrupted name.... So lookup by id. (see mysql_rename_table()
  // problem)
  Cache_element<typename T::cache_partition_type> *element= nullptr;
  const typename T::id_key_type id_key(object->id());
  m_registry_committed.get(id_key, &element);

  if (element)
  {
    // Remove the element from the chain of auto releasers.
    (void) m_current_releaser->remove(element);
    // Remove the element from the local registry.
    m_registry_committed.remove(element);
    // Remove the element from the cache, delete the wrapper and the object.
    Shared_dictionary_cache::instance()->drop(element);
  }
  else
    Shared_dictionary_cache::instance()->
      drop_if_present<typename T::id_key_type,
                      typename T::cache_partition_type>(id_key);

  // Finally, add a clone to the dropped registry. Note that we are allowed to
  // drop a const object, e.g. coming from acquire(). This means that the
  // object instance, or the same id in a different instance, may be present in
  // the uncommitted registry. This is handled inside register_dropped_object(),
  // where we ensure that the uncommitted and dropped registries are consistent.
  register_dropped_object(dropped_object);

  return false;
}


// Store a new dictionary object.
template <typename T>
bool Dictionary_client::store(T* object)
{
#ifndef DBUG_OFF
  // Make sure the object is not being used by this client.
  Cache_element<typename T::cache_partition_type> *element= NULL;
  m_registry_committed.get(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);
  DBUG_ASSERT(!element);
#endif

  // Make sure the object has an invalid object id.
  DBUG_ASSERT(object->id() == INVALID_OBJECT_ID);

  // Check proper MDL lock.
  DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, object));
  if (Storage_adapter::store(m_thd, object))
    return true;

  DBUG_ASSERT(object->id() != INVALID_OBJECT_ID);
  register_uncommitted_object(object->clone());
  return false;
}

// Store a new dictionary object.
template <>
bool Dictionary_client::store(Table_stat* object)
{ return Storage_adapter::store<Table_stat>(m_thd, object); }

template <>
bool Dictionary_client::store(Index_stat* object)
{ return Storage_adapter::store<Index_stat>(m_thd, object); }


// Update a persisted dictionary object, but keep the shared cache unchanged.
template <typename T>
bool Dictionary_client::update(T* new_object)
{
  DBUG_ASSERT(new_object);

  // Make sure the object has a valid object id.
  DBUG_ASSERT(new_object->id() != INVALID_OBJECT_ID);

  // The new_object instance should not be present in the committed registry.
  Cache_element<typename T::cache_partition_type> *element= NULL;

#ifndef DBUG_OFF
  m_registry_committed.get(
    static_cast<const typename T::cache_partition_type*>(new_object),
    &element);
  DBUG_ASSERT(!element);
#endif

  // new_object->id() may or may not be reflected in the uncommitted registry.
  const typename T::id_key_type id_key(new_object->id());
  const T* old_object= nullptr;
  m_registry_uncommitted.get(id_key, &element);

  if (element)
  {
    // If new_object->id() is present in the uncommitted registry, then
    // that object is the previously stored object for this id, since the
    // only way to enter the uncommitted registry is through store() or
    // update().
    old_object= dynamic_cast<const T*>(element->object());
  }
  else
  {
    // If not present, then the previously stored object can be acquire()'d
    // in the usual way (a cache miss handled by ISO_READ_COMMITTED is fine,
    // since the object hasn't been stored by this transaction yet anyway).
    if (acquire(new_object->id(), &old_object))
      return true;
  }

  // Either way, we now should have the previously stored object.
  DBUG_ASSERT(old_object);

  // Check proper MDL locks.
  DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, old_object));
  DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, new_object));

  /*
    We first store the new object. If store() fails, there is not a
    lot to do except returning true.
  */
  if (Storage_adapter::store(m_thd, new_object))
    return true;

  /*
    Remove the old sdi after store() has successfully written the
    new one. Note that this is a noop unless we are writing the SDI
    to file and the name of the new object is different. (If the
    names are identical the old file will be over-written by
    store(). If we are storing the SDI in a tablespace the key
    does not depend on the name and the store is a transactional
    update).
  */
  if (sdi::drop_after_update(m_thd, old_object, new_object))
  {
    return true;
  }

  if (element)
  {
    // Remove and delete the old uncommitted object.
    m_registry_uncommitted.remove(element);
    delete element->object();
    element->set_object(new_object);
    element->recreate_keys();
    m_registry_uncommitted.put(element);
  }
  else
  {
    register_uncommitted_object(new_object);
  }

  // Remove the new object from the auto deleter.
  no_auto_delete<T>(new_object);
  return false;
}


template <typename T>
void Dictionary_client::register_uncommitted_object(T* object)
{
  Cache_element<typename T::cache_partition_type> *element= nullptr;
#ifndef DBUG_OFF
  // Make sure we do not sign up a shared object for auto delete.
  m_registry_committed.get(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);
  DBUG_ASSERT(element == nullptr);

  // We need a top level auto releaser to make sure the uncommitted objects
  // are removed. This is done in the auto releaser destructor. When
  // renove_uncommitted_objects() is called implicitly as part of commit/
  // rollback, this should not be necessary.
  DBUG_ASSERT(m_current_releaser != &m_default_releaser);

  // store() should have been called before if this is a
  // new object so that it has a proper ID already.
  DBUG_ASSERT(object->id() != INVALID_OBJECT_ID);

  // Make sure the same id is not present in the dropped registry.
  const typename T::id_key_type id_key(object->id());
  m_registry_dropped.get(id_key, &element);
  DBUG_ASSERT(element == nullptr);
#endif

  element= new Cache_element<typename T::cache_partition_type>();
  element->set_object(object);
  element->recreate_keys();
  m_registry_uncommitted.put(element);
}


template <typename T>
void Dictionary_client::register_dropped_object(T* object)
{
  Cache_element<typename T::cache_partition_type> *element= nullptr;
#ifndef DBUG_OFF
  // Make sure we do not sign up a shared object for auto delete.
  m_registry_committed.get(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);
  DBUG_ASSERT(element == nullptr);

  // We need a top level auto releaser to make sure the dropped objects
  // are removed. This is done in the auto releaser destructor. When
  // renove_uncommitted_objects() is called implicitly as part of commit/
  // rollback, this should not be necessary.
  DBUG_ASSERT(m_current_releaser != &m_default_releaser);

  // store() should have been called before if this is a
  // new object so that it has a proper ID already.
  DBUG_ASSERT(object->id() != INVALID_OBJECT_ID);
#endif

  // Could be in the uncommitted registry, remove and delete.
  const typename T::id_key_type id_key(object->id());
  typename T::cache_partition_type *modified= nullptr;
  bool dropped= false;
  acquire_uncommitted(id_key, &modified, &dropped);
  DBUG_ASSERT(!dropped);
  if (modified != nullptr)
  {
    m_registry_uncommitted.get(
      static_cast<const typename T::cache_partition_type*>(modified),
      &element);
    DBUG_ASSERT(element != nullptr);
    m_registry_uncommitted.remove(element);
    DBUG_ASSERT(element->object() != object);
    delete element->object();
    // The element is reused below, so we don't delete it.
  }

  if (element == nullptr)
    element= new Cache_element<typename T::cache_partition_type>();

  element->set_object(object);
  element->recreate_keys();

  // Check if the object is already registered. This could happen if
  // the object is dropped twice in the same statement. Currently
  // this is possible when updating view metadata since alter view
  // is implemented as drop+create. We have to look up on name since
  // the ID has changed.
  Cache_element<typename T::cache_partition_type> *dropped_ele= nullptr;
  m_registry_dropped.get(*element->name_key(), &dropped_ele);
  if (dropped_ele != nullptr)
  {
    // We have dropped an object with the same name earlier.
    // Remove the old object so that we can insert the new
    // object without getting key conflicts.
    // Note: This means that the previously dropped object can
    // now be retrieved again with the old ID!
    m_registry_dropped.remove(dropped_ele);
    delete dropped_ele->object();
    delete dropped_ele;
  }

  m_registry_dropped.put(element);
}


template <typename T>
void Dictionary_client::remove_uncommitted_objects(bool commit_to_shared_cache)
{
#ifndef DBUG_OFF
  // Note: The ifdef'ed block below is only for consistency checks in
  // debug builds.
  typename Multi_map_base<typename T::cache_partition_type>::Const_iterator it;
  for (it= m_registry_dropped.begin<typename T::cache_partition_type>();
       it != m_registry_dropped.end<typename T::cache_partition_type>();
       it++)
  {
    const typename T::cache_partition_type* dropped_object= it->second->object();
    DBUG_ASSERT(dropped_object != nullptr);

    // Checking proper MDL lock is skipped here because when dropping a
    // schema, the implementation of the MDL checking does not work properly.

    // Make sure that dropped object ids are not present persistently with
    // isolation level READ UNCOMMITTED.
    const typename T::id_key_type id_key(dropped_object->id());

    // Fetch the dictionary object by PK from the DD tables, and verify that
    // it's not available, but only if:
    // - This is not a DD system thread (due to SE being faked).
    // - The transaction is being committed, not rolled back.
    // - We're not allowing direct access to DD tables.
    if (!m_thd->is_dd_system_thread() && commit_to_shared_cache &&
        DBUG_EVALUATE_IF("skip_dd_table_access_check", false, true))
    {
      const typename T::cache_partition_type *stored_object= nullptr;
      if (!Shared_dictionary_cache::instance()->
            get_uncached(m_thd, id_key, ISO_READ_UNCOMMITTED, &stored_object))
        DBUG_ASSERT(stored_object == nullptr);
    }

    // Make sure that dropped object ids are not present in the shared cache.
    DBUG_ASSERT(!(Shared_dictionary_cache::instance()->
                   available<typename T::id_key_type,
                             typename T::cache_partition_type>(id_key)));

    // Make sure that dropped object ids are not present in the uncommitted
    // registry.
    Cache_element<typename T::cache_partition_type> *element= nullptr;
    m_registry_uncommitted.get(id_key, &element);
    DBUG_ASSERT(element == nullptr);

    // Make sure that dropped object ids are not present in the committed
    // registry.
    m_registry_committed.get(id_key, &element);
    DBUG_ASSERT(element == nullptr);
  }
#endif
  if (commit_to_shared_cache)
  {
    typename Multi_map_base<typename T::cache_partition_type>::Const_iterator it;
    for (it= m_registry_uncommitted.begin<typename T::cache_partition_type>();
         it != m_registry_uncommitted.end<typename T::cache_partition_type>();
         it++)
    {
      typename T::cache_partition_type* uncommitted_object=
        const_cast<typename T::cache_partition_type*>(it->second->object());
      DBUG_ASSERT(uncommitted_object != nullptr);

      // Check proper MDL lock.
      DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, uncommitted_object));

      // Get a committed version of the object and invalidate it.
      // Note that we have to access m_registry_committed directly
      // instead of using acquire() to avoid finding the uncommitted object.
      const typename T::id_key_type key(uncommitted_object->id());
      Cache_element<typename T::cache_partition_type> *element= NULL;
      m_registry_committed.get(key, &element);

      if (element)
      {
        DBUG_ASSERT(element->object() != nullptr);

        // Remove the element from the chain of auto releasers.
        (void) m_current_releaser->remove(element);
        m_registry_committed.remove(element);

        // Remove the element from the cache, delete the wrapper and the object.
        Shared_dictionary_cache::instance()->drop(element);
      }
      else
      {
        Shared_dictionary_cache::instance()->
          drop_if_present<typename T::id_key_type,
                          typename T::cache_partition_type>(uncommitted_object->id());
      }
#ifndef DBUG_OFF
      // Make sure the uncommitted id is not present in the dropped registry.
      m_registry_committed.get(key, &element);
      DBUG_ASSERT(element == nullptr);
#endif
    }

    // We must do this in two iterations to handle situations where two uncommitted
    // objects swap names.
    for (it= m_registry_uncommitted.begin<typename T::cache_partition_type>();
         it != m_registry_uncommitted.end<typename T::cache_partition_type>();
         it++)
    {
      typename T::cache_partition_type* uncommitted_object=
        const_cast<typename T::cache_partition_type*>(it->second->object());
      DBUG_ASSERT(uncommitted_object != nullptr);

      Cache_element<typename T::cache_partition_type> *element= NULL;

      // In put, the reference counter is stepped up, so this is safe.
      Shared_dictionary_cache::instance()->put(
          static_cast<const typename T::cache_partition_type*>(
            uncommitted_object->clone()), &element);

      m_registry_committed.put(element);
      // Sign up for auto release.
      m_current_releaser->auto_release(element);
    }
  } // commit_to_shared_cache
  m_registry_uncommitted.erase<typename T::cache_partition_type>();
  m_registry_dropped.erase<typename T::cache_partition_type>();
}


void Dictionary_client::rollback_modified_objects()
{
  remove_uncommitted_objects<Abstract_table>(false);
  remove_uncommitted_objects<Schema>(false);
  remove_uncommitted_objects<Tablespace>(false);
  remove_uncommitted_objects<Charset>(false);
  remove_uncommitted_objects<Collation>(false);
  remove_uncommitted_objects<Event>(false);
  remove_uncommitted_objects<Routine>(false);
  remove_uncommitted_objects<Spatial_reference_system>(false);
}


void Dictionary_client::commit_modified_objects()
{
  remove_uncommitted_objects<Abstract_table>(true);
  remove_uncommitted_objects<Schema>(true);
  remove_uncommitted_objects<Tablespace>(true);
  remove_uncommitted_objects<Charset>(true);
  remove_uncommitted_objects<Collation>(true);
  remove_uncommitted_objects<Event>(true);
  remove_uncommitted_objects<Routine>(true);
  remove_uncommitted_objects<Spatial_reference_system>(true);
}


// Debug dump of the client and its registry to stderr.
/* purecov: begin inspected */
template <typename T>
void Dictionary_client::dump() const
{
#ifndef DBUG_OFF
  fprintf(stderr, "================================\n");
  fprintf(stderr, "Dictionary client (committed)\n");
  m_registry_committed.dump<T>();
  fprintf(stderr, "Dictionary client (uncommitted)\n");
  m_registry_uncommitted.dump<T>();
  fprintf(stderr, "Dictionary client (dropped)\n");
  m_registry_dropped.dump<T>();
  fprintf(stderr, "Dictionary client (uncached)\n");
  for (std::vector<Dictionary_object*>::const_iterator it=
         m_uncached_objects.begin();
       it != m_uncached_objects.end(); it++)
  {
    if (*it != nullptr)
      fprintf(stderr, "id=%llu, name= %s\n", (*it)->id(),
              (*it)->name().c_str());
    else
      fprintf(stderr, "nullptr\n");
  }
  fprintf(stderr, "================================\n");
#endif
}
/* purecov: end */

// The explicit instantiation of the template members below
// is not handled well by doxygen, so we enclose this in a
// cond/endcon block. Documenting these does not add much
// value anyway, if the member definitions were in a header
// file, the compiler would do the instantiation for us.

/**
 @cond
*/

// Explicitly instantiate the types for the various usages.
template bool Dictionary_client::fetch_schema_components(
    const Schema*,
    std::vector<const Abstract_table*>*) const;

template bool Dictionary_client::fetch_schema_components(
    const Schema*,
    std::vector<const Table*>*) const;

template bool Dictionary_client::fetch_schema_components(
    const Schema*,
    std::vector<const View*>*) const;

template bool Dictionary_client::fetch_schema_components(
    const Schema*,
    std::vector<const Event*>*) const;

template bool Dictionary_client::fetch_schema_components(
    const Schema*,
    std::vector<const Routine*>*) const;

template bool Dictionary_client::fetch_global_components(
    std::vector<const Charset*>*) const;

template bool Dictionary_client::fetch_global_components(
    std::vector<const Collation*>*) const;

template bool Dictionary_client::fetch_global_components(
    std::vector<const Schema*>*) const;

template bool Dictionary_client::fetch_schema_component_names<Abstract_table>(
    const Schema*,
    std::vector<String_type>*) const;

template bool Dictionary_client::fetch_schema_component_names<Event>(
    const Schema*,
    std::vector<String_type>*) const;

template bool Dictionary_client::fetch_referencing_views_object_id<View_table>(
    const char *schema,
    const char *tbl_or_sf_name,
    std::vector<Object_id> *view_ids) const;

template bool Dictionary_client::fetch_referencing_views_object_id<View_routine>(
    const char *schema,
    const char *tbl_or_sf_name,
    std::vector<Object_id> *view_ids) const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  Abstract_table**);
template bool Dictionary_client::acquire(const String_type&,
                                         const String_type&,
                                         const Abstract_table**);
template void Dictionary_client::remove_uncommitted_objects<Abstract_table>(bool);
template bool Dictionary_client::drop(const Abstract_table*);
template bool Dictionary_client::store(Abstract_table*);
template bool Dictionary_client::update(Abstract_table*);
template void Dictionary_client::dump<Abstract_table>() const;

template bool Dictionary_client::acquire(Object_id, dd::Charset const**);
template bool Dictionary_client::acquire_for_modification(Object_id,
                                                          dd::Charset**);
template void Dictionary_client::remove_uncommitted_objects<Charset>(bool);
template bool Dictionary_client::acquire(String_type const&, Charset const**);
template bool Dictionary_client::acquire_for_modification(String_type const&,
                                                          dd::Charset**);

template bool Dictionary_client::drop(const Charset*);
template bool Dictionary_client::store(Charset*);
template bool Dictionary_client::update(Charset*);
template void Dictionary_client::dump<Charset>() const;


template bool Dictionary_client::acquire_uncached(Object_id,
                                                  Charset**);
template bool Dictionary_client::acquire(Object_id, dd::Collation const**);
template bool Dictionary_client::acquire_for_modification(Object_id,
                                                          dd::Collation **);
template void Dictionary_client::remove_uncommitted_objects<Collation>(bool);
template bool Dictionary_client::acquire_uncached(Object_id,
                                                  Collation**);
template bool Dictionary_client::acquire(const String_type &,
                                         const Collation**);
template bool Dictionary_client::acquire_for_modification(const String_type &,
                                                          Collation**);
template bool Dictionary_client::drop(const Collation*);
template bool Dictionary_client::store(Collation*);
template bool Dictionary_client::update(Collation*);
template void Dictionary_client::dump<Collation>() const;

template bool Dictionary_client::acquire(Object_id, Schema const**);
template bool Dictionary_client::acquire_for_modification(Object_id,
                                                          Schema**);
template bool Dictionary_client::acquire_uncached(Object_id,
                                                  Schema**);
template bool Dictionary_client::acquire_uncached_uncommitted(Object_id,
                                                              Schema**);
template bool Dictionary_client::acquire_for_modification(const String_type&,
                                                          Schema**);
template void Dictionary_client::remove_uncommitted_objects<Schema>(bool);

template bool Dictionary_client::drop(const Schema*);
template bool Dictionary_client::store(Schema*);
template bool Dictionary_client::update(Schema*);
template void Dictionary_client::dump<Schema>() const;

template bool Dictionary_client::acquire(Object_id,
                                         const Spatial_reference_system**);
template bool Dictionary_client::acquire_uncached(
    Object_id,
    Spatial_reference_system**);
template bool Dictionary_client::drop(const Spatial_reference_system*);
template bool Dictionary_client::store(Spatial_reference_system*);
template bool Dictionary_client::update(Spatial_reference_system*);
template void Dictionary_client::dump<Spatial_reference_system>() const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  Table**);
template bool Dictionary_client::acquire(Object_id,
                                         const Table**);
template bool Dictionary_client::acquire_for_modification(Object_id,
                                                          Table**);
template bool Dictionary_client::acquire(const String_type&,
                                         const String_type&,
                                         const Table**);
template bool Dictionary_client::acquire_for_modification(const String_type&,
                                                          const String_type&,
                                                          Table**);
template void Dictionary_client::remove_uncommitted_objects<Table>(bool);
template bool Dictionary_client::drop(const Table*);
template bool Dictionary_client::store(Table*);
template bool Dictionary_client::update(Table*);

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  Tablespace**);
template bool Dictionary_client::acquire(const String_type&,
                                         const Tablespace**);


template bool Dictionary_client::acquire_for_modification(const String_type&,
                                                          Tablespace**);
template bool Dictionary_client::acquire(Object_id,
                                         const Tablespace**);
template bool Dictionary_client::acquire_uncached_uncommitted(Object_id,
                                         Tablespace**);
template bool Dictionary_client::acquire_for_modification(Object_id,
                                                          Tablespace**);
template void Dictionary_client::remove_uncommitted_objects<Tablespace>(bool);
template bool Dictionary_client::drop(const Tablespace*);
template bool Dictionary_client::store(Tablespace*);
template bool Dictionary_client::update(Tablespace*);
template void Dictionary_client::dump<Tablespace>() const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  View**);
template bool Dictionary_client::acquire_uncached_uncommitted(Object_id,
                                                              View**);
template bool Dictionary_client::acquire(Object_id,
                                         const View**);
template bool Dictionary_client::acquire_for_modification(Object_id,
                                                          View**);
template bool Dictionary_client::acquire(const String_type&,
                                         const String_type&,
                                         const View**);
template bool Dictionary_client::acquire_for_modification(const String_type&,
                                                          const String_type&,
                                                          View**);
template void Dictionary_client::remove_uncommitted_objects<View>(bool);
template bool Dictionary_client::drop(const View*);
template bool Dictionary_client::store(View*);
template bool Dictionary_client::update(View*);

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  Event**);
template bool Dictionary_client::acquire(Object_id,
                                         const Event**);
template bool Dictionary_client::acquire_for_modification(Object_id,
                                                          Event**);
template void Dictionary_client::remove_uncommitted_objects<Event>(bool);
template bool Dictionary_client::acquire(const String_type&,
                                         const String_type&,
                                         const Event**);
template bool Dictionary_client::acquire_for_modification(const String_type&,
                                                          const String_type&,
                                                          Event**);
template bool Dictionary_client::drop(const Event*);
template bool Dictionary_client::store(Event*);
template bool Dictionary_client::update(Event*);

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  Function**);
template bool Dictionary_client::acquire(Object_id,
                                         const Function**);
template bool Dictionary_client::acquire(const String_type&,
                                         const String_type&,
                                         const Function**);
template bool Dictionary_client::drop(const Function*);
template bool Dictionary_client::store(Function*);
template bool Dictionary_client::update(Function*);

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  Procedure**);
template bool Dictionary_client::acquire(Object_id,
                                         const Procedure**);
template bool Dictionary_client::acquire_for_modification(Object_id,
                                                          Procedure**);
template void Dictionary_client::remove_uncommitted_objects<Procedure>(bool);
template bool Dictionary_client::acquire(const String_type&,
                                         const String_type&,
                                         const Procedure**);
template bool Dictionary_client::acquire_for_modification(const String_type&,
                                                          const String_type&,
                                                          Procedure**);
template bool Dictionary_client::drop(const Procedure*);
template bool Dictionary_client::store(Procedure*);
template bool Dictionary_client::update(Procedure*);

template bool Dictionary_client::drop(const Routine*);
template void Dictionary_client::remove_uncommitted_objects<Routine>(bool);
template bool Dictionary_client::update(Routine*);

template bool Dictionary_client::acquire<Function>(
  const String_type&,
  const String_type&,
  const Function::cache_partition_type**);
template bool Dictionary_client::acquire<Procedure>(
  const String_type&,
  const String_type&,
  const Procedure::cache_partition_type**);
template bool Dictionary_client::acquire_for_modification<Function>(
  const String_type&,
  const String_type&,
  Function::cache_partition_type**);
template bool Dictionary_client::acquire_for_modification<Procedure>(
  const String_type&,
  const String_type&,
  Procedure::cache_partition_type**);

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  Routine**);
template bool Dictionary_client::acquire_for_modification(Object_id,
                                                          Routine**);
/**
 @endcond
*/

} // namespace cache
} // namespace dd

