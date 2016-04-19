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

#include "dd/cache/dictionary_client.h"

#include "debug_sync.h"                      // DEBUG_SYNC()
#include "log.h"                             // sql_print_warning()
#include "sql_class.h"                       // THD

#include "cache_element.h"                   // Cache_element
#include "shared_dictionary_cache.h"         // get(), release(), ...
#include "storage_adapter.h"                 // store(), drop(), ...
#include "dd/dd_schema.h"                    // dd::Schema_MDL_locker
#include "dd/properties.h"                   // Properties
#include "dd/types/abstract_table.h"         // Abstract_table
#include "dd/types/charset.h"                // Charset
#include "dd/types/collation.h"              // Collation
#include "dd/types/event.h"                  // Event
#include "dd/types/function.h"               // Function
#include "dd/types/fwd.h"                    // Schema_const_iterator
#include "dd/types/procedure.h"              // Procedure
#include "dd/types/schema.h"                 // Schema
#include "dd/types/table.h"                  // Table
#include "dd/types/tablespace.h"             // Tablespace
#include "dd/types/view.h"                   // View
#include "dd/impl/bootstrapper.h"            // bootstrap_stage
#include "dd/impl/transaction_impl.h"        // Transaction_ro
#include "dd/impl/raw/object_keys.h"         // Primary_id_key, ...
#include "dd/impl/raw/raw_record_set.h"      // Raw_record_set
#include "dd/impl/raw/raw_table.h"           // Raw_table
#include "dd/impl/tables/character_sets.h"   // create_name_key()
#include "dd/impl/tables/collations.h"       // create_name_key()
#include "dd/impl/tables/events.h"           // create_name_key()
#include "dd/impl/tables/routines.h"         // create_name_key()
#include "dd/impl/tables/schemata.h"         // create_name_key()
#include "dd/impl/tables/tables.h"           // create_name_key()
#include "dd/impl/tables/tablespaces.h"      // create_name_key()
#include "dd/impl/tables/table_partitions.h" // get_partition_table_id()
#include "dd/impl/types/object_table_definition_impl.h" // fs_name_case()
#include "dd/impl/types/entity_object_impl.h"// Entity_object_impl

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
    if (thd->dd_client()->acquire<dd::Schema>(table->schema_id(), &schema))
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
      return is_locked(thd, schema->name().c_str(),
                       dd::Object_table_definition_impl::fs_name_case(table->name(),
                                                                      table_name_buf),
                       MDL_key::TABLE, lock_type);

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
    if (thd->dd_client()->acquire<dd::Schema>(event->schema_id(), &schema))
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
    if (thd->dd_client()->acquire<dd::Schema>(routine->schema_id(), &schema))
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
  static bool is_release_locked(THD *thd, const dd::Dictionary_object *object)
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
            is_locked(thd, event, MDL_INTENTION_EXCLUSIVE));
  }

  // Writing a Event object should be governed by MDL_EXCLUSIVE.
  static bool is_write_locked(THD *thd, const dd::Event *event)
  {
    return (thd->is_dd_system_thread() ||
            is_locked(thd, event, MDL_SHARED));
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

  // Restore the client's previous releaser.
  m_client->m_current_releaser= m_prev;
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
bool Dictionary_client::acquire(const K &key, const T **object, bool *local)
{
  DBUG_ASSERT(object);
  DBUG_ASSERT(local);
  *object= NULL;

  DBUG_EXECUTE_IF("fail_while_acquiring_dd_object",
  {
    my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
    return true;
  });

  // Lookup in the local registry.
  Cache_element<T> *element= NULL;
  m_registry.get(key, &element);
  if (element)
  {
    *local= true;
    *object= element->object();
    // Check proper MDL lock.
    DBUG_ASSERT(MDL_checker::is_read_locked(m_thd, *object));
    return false;
  }

  // The element is not present locally.
  *local= false;

  // Get the object from the shared cache.
  if (Shared_dictionary_cache::instance()->get(m_thd, key, &element))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  // Add the element to the local registry and assign the output object.
  if (element)
  {
    DBUG_ASSERT(element->object() && element->object()->id());
    // Sign up for auto release.
    m_registry.put(element);
    m_current_releaser->auto_release(element);
    *object= element->object();
    // Check proper MDL lock.
    DBUG_ASSERT(MDL_checker::is_read_locked(m_thd, *object));
  }
  return false;
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
    if (registry != &m_registry)
      m_registry.remove(element);
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
          release<Routine>(registry);
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

  bool present= false;
  bool error= acquire(key, &cached_object, &present);

  if (!error)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    *object= dynamic_cast<const T*>(cached_object);

    // Don't auto release the object here if it is returned.
    if (!present && *object)
      releaser.transfer_release(cached_object);
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve an object by its object id without caching it.
template <typename T>
bool Dictionary_client::acquire_uncached(Object_id id, const T** object)
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
    *object= dynamic_cast<const T*>(stored_object);

    // Delete the object if dynamic cast fails.
    if (stored_object && !*object)
      delete stored_object;
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve an object by its object id without caching it. Use isolation
// level ISO_READ_UNCOMMITTED for reading the object. Needed by WL#7743.
/* purecov: begin deadcode */
template <typename T>
bool Dictionary_client::acquire_uncached_uncommitted(Object_id id,
                                                     const T** object)
{
  const typename T::id_key_type key(id);
  const typename T::cache_partition_type *stored_object= NULL;

  // Read the uncached dictionary object using ISO_READ_UNCOMMITTED
  // isolation level.
  bool error= Shared_dictionary_cache::instance()->
                get_uncached(m_thd, key, ISO_READ_UNCOMMITTED, &stored_object);
  if (!error)
  {
    // We do not verify proper MDL locking here since the
    // returned object is owned by the caller.

    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    *object= dynamic_cast<const T*>(stored_object);

    // Delete the object if dynamic cast fails.
    if (stored_object && !*object)
      delete stored_object;
  }
  else
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);

  return error;
}
/* purecov: end */


// Retrieve an object by its name.
template <typename T>
bool Dictionary_client::acquire(const std::string &object_name,
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

  bool local= false;
  error= acquire(key, &cached_object, &local);

  if (!error)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    *object= dynamic_cast<const T*>(cached_object);

    // Don't auto release the object here if it is returned.
    if (!local && *object)
      releaser.transfer_release(cached_object);
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve an object by its name without caching it.
template <typename T>
bool Dictionary_client::acquire_uncached(const std::string &object_name,
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
  const typename T::cache_partition_type *stored_object= NULL;

  // Read the uncached dictionary object.
  error= Shared_dictionary_cache::instance()->
                get_uncached(m_thd, key, ISO_READ_COMMITTED, &stored_object);
  if (!error)
  {
    // We do not verify proper MDL locking here since the
    // returned object is owned by the caller.

    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    *object= dynamic_cast<const T*>(stored_object);

    // Delete the object if dynamic cast fails.
    if (stored_object && !*object)
      delete stored_object;
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve an object by its schema- and object name.
template <typename T>
bool Dictionary_client::acquire(const std::string &schema_name,
                                const std::string &object_name,
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

  bool local= false;
  error= acquire(key, &cached_object, &local);

  if (!error)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    *object= dynamic_cast<const T*>(cached_object);

    // Don't auto release the object here if it is returned.
    if (!local && *object)
      releaser.transfer_release(cached_object);
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve an object by its schema- and object name. Return as double
// pointer to base type.
template <typename T>
bool Dictionary_client::acquire(const std::string &schema_name,
                                const std::string &object_name,
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
  bool local= false;
  error= acquire(key, object, &local);

  if (!error)
  {
    // No downcasting is necessary here.
    // Don't auto release the object here if it is returned.
    if (!local && *object)
      releaser.transfer_release(*object);
  }
  else
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);

  return error;
}


// Retrieve an object by its schema- and object name without caching it.
template <typename T>
bool Dictionary_client::acquire_uncached(const std::string &schema_name,
                                         const std::string &object_name,
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

  // Create the name key for the object.
  typename T::name_key_type key;
  T::update_name_key(&key, schema->id(), object_name);

  // Read the uncached dictionary object.
  const typename T::cache_partition_type *stored_object= NULL;
  error= Shared_dictionary_cache::instance()->
           get_uncached(m_thd, key, ISO_READ_COMMITTED, &stored_object);

  if (!error)
  {
    // We do not verify proper MDL locking here since the
    // returned object is owned by the caller.

    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(object);
    *object= dynamic_cast<const T*>(stored_object);

    // Delete the object if dynamic cast fails.
    if (stored_object && !*object)
      delete stored_object;
  }
  else
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return error;
}


// Retrieve a table object by its se private id.
bool Dictionary_client::acquire_uncached_table_by_se_private_id(
                                      const std::string &engine,
                                      Object_id se_private_id,
                                      const Table **table)
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
  *table= dynamic_cast<const Table*>(stored_object);

  // Delete the object and report error if dynamic cast fails.
  if (!*table)
  {
    my_error(ER_INVALID_DD_OBJECT, MYF(0),
             Table::OBJECT_TABLE().name().c_str(),
             "Not a table object.");
    delete stored_object;
    return true;
  }

  return false;
}

// Retrieve a table object by its partition se private id.
/* purecov: begin deadcode */
bool Dictionary_client::acquire_uncached_table_by_partition_se_private_id(
                          const std::string &engine,
                          Object_id se_partition_id,
                          const Table **table)
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

  if (acquire_uncached<Table>(table_id, table))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    return true;
  }

  if (*table == NULL)
    return false;

  return false;
}
/* purecov: end */

// Local RAII-based class to make sure the acquired objects are
// deleted whenever the function returns and the instance goes out
// of scope and is deleted.
class Object_deleter
{
private:
  const Table **m_table;
  const Schema **m_schema;
public:
  Object_deleter(const Table **table, const Schema **schema):
    m_table(table), m_schema(schema)
  { }
  ~Object_deleter()
  {
    if (m_table && *m_table)
      delete *m_table;
    if (m_schema && *m_schema)
      delete *m_schema;
  }
};

// Retrieve a schema- and table name by the se private id of the table.
bool Dictionary_client::get_table_name_by_se_private_id(
                                    const std::string &engine,
                                    Object_id se_private_id,
                                    std::string *schema_name,
                                    std::string *table_name)
{
  // Objects to be acquired.
  const Table *tab_obj= NULL;
  const Schema *sch_obj= NULL;

  // Store empty in OUT params.
  DBUG_ASSERT(schema_name && table_name);
  schema_name->clear();
  table_name->clear();

  // Sign up for delete.
  Object_deleter object_deleter(&tab_obj, &sch_obj);

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
  if (acquire_uncached<Schema>(tab_obj->schema_id(), &sch_obj))
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
                                      const std::string &engine,
                                      Object_id se_partition_id,
                                      std::string *schema_name,
                                      std::string *table_name)
{
  const Table *tab_obj= NULL;
  const Schema *sch_obj= NULL;

  // Store empty in OUT params.
  DBUG_ASSERT(schema_name && table_name);
  schema_name->clear();
  table_name->clear();

  // Sign up for delete.
  Object_deleter object_deleter(&tab_obj, &sch_obj);

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
  if (acquire_uncached<Schema>(tab_obj->schema_id(), &sch_obj))
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


// Get the highest currently used se private id for the table objects.
/* purecov: begin deadcode */
bool Dictionary_client::get_tables_max_se_private_id(const std::string &engine,
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
    std::vector<std::string> *names) const
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
  while (r)
  {
    // Here, we need only the table name.
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
    const Object_type *o= NULL;
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


// Fetch all components of the given type in the default catalog.
/* purecov: begin deadcode */
template <typename T>
bool Dictionary_client::fetch_catalog_components(
    std::vector<const T*> *coll) const
{
  std::unique_ptr<Object_key> k(
    T::cache_partition_table_type::create_key_by_catalog_id(1));

  if (fetch(m_thd, coll, k.get()))
  {
    DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());
    DBUG_ASSERT(coll->empty());
    return true;
  }

  return false;
}
/* purecov: end */


// Fetch all global components of the given type.
/* purecov: begin deadcode */
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
/* purecov: end */


// Mark all objects acquired by this client as not being used anymore.
size_t Dictionary_client::release()
{ return release(&m_registry); }


// Remove and delete an object from the cache and the dd tables.
template <typename T>
bool Dictionary_client::drop(const T *object)
{
  // Lookup in the local registry using the partition type.
  Cache_element<typename T::cache_partition_type> *element= NULL;
  m_registry.get(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);
  DBUG_ASSERT(element);

  // Check proper MDL lock.
  DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, object));

  if (Storage_adapter::drop(m_thd, object) == false)
  {
    // Remove the element from the chain of auto releasers.
    (void) m_current_releaser->remove(element);

    // Remove the element from the local registry.
    m_registry.remove(element);

    // Remove the element from the cache, delete the wrapper and the object.
    Shared_dictionary_cache::instance()->drop(element);

    return false;
  }

  DBUG_ASSERT(m_thd->is_system_thread() || m_thd->killed || m_thd->is_error());

  return true;
}


// Store a new dictionary object.
template <typename T>
bool Dictionary_client::store(T* object)
{
  // Make sure the object is not being used by this client.
  Cache_element<typename T::cache_partition_type> *element= NULL;
  m_registry.get(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);
  DBUG_ASSERT(!element);

  // Check proper MDL lock.
  DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, object));
  return Storage_adapter::store(m_thd, object);
}


// Replace a dictionary object by another and store it.
template <typename T>
bool Dictionary_client::update(const T** old_object, T* new_object,
                               bool persist)
{
  DBUG_ASSERT(*old_object);
  DBUG_ASSERT(new_object);

  // Make sure the old object is present and the new object is absent.
  Cache_element<typename T::cache_partition_type> *element= NULL;

#ifndef DBUG_OFF
  m_registry.get(
    static_cast<const typename T::cache_partition_type*>(new_object),
    &element);
  DBUG_ASSERT(!element);
#endif

  m_registry.get(
    static_cast<const typename T::cache_partition_type*>(*old_object),
    &element);
  DBUG_ASSERT(element);

  // Check proper MDL locks.
  DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, *old_object));
  DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, new_object));

  // Only the bootstrap thread is allowed to do this without persisting changes.
  DBUG_ASSERT(persist || m_thd->is_dd_system_thread());

  if (persist)
  {
    /*
      The object must maintain its id, otherwise, the update will not become
      an update, but instead, the new object will be added alongside the
      old one. The exception to this is that during bootstrap, we allow the
      dictionary tables to be updated with an object having id == INVALID
      in order to store it the first time.
    */
    DBUG_ASSERT((*old_object)->id() == new_object->id() ||
                (new_object->id() == INVALID_OBJECT_ID &&
                 m_thd->is_dd_system_thread()));

    /*
      We first store the new object. If store() fails, there is not a
      lot to do except returning true. In this case, the shared cache will
      stay unchanged.
    */
    if (store(new_object))
      return true;
  }

  // If we succeed in storing the new object, we must update the shared
  // cache accordingly. First, we remove the element from the chain of auto
  // releasers and from the local registry.
  Auto_releaser *actual_releaser= m_current_releaser->remove(element);
  m_registry.remove(element);

  // Then, we must replace the object in the shared cache and re-create the
  // keys. Note that we will take a clone of the new_object and add the clone
  // to the cache. This is to ensure that the original new_object pointer
  // remains owned by the caller of this function, while the clone is being
  // owned by the cache.
  T *new_object_clone= new_object->clone();
  Shared_dictionary_cache::instance()->replace(element,
    static_cast<const typename T::cache_partition_type*>(new_object_clone));

  // Put back the element, with its new keys, into the local registry.
  m_registry.put(element);

  // Put back the element into the correct auto releaser.
  if (actual_releaser)
    actual_releaser->auto_release(element);

  // And finally, we set *old_object to point to the new cached clone of
  // new_object. The dynamic cast should never fail in this case.
  *old_object= dynamic_cast<const T*>(element->object());
  DBUG_ASSERT(*old_object == new_object_clone);

  return false;
}


// Update a modified dictionary object and remove it from the cache.
// Needed by WL#7743.
/* purecov: begin deadcode */
template <typename T>
bool Dictionary_client::update_and_invalidate(T* object)
{
  // Make sure the object is present.
  Cache_element<typename T::cache_partition_type> *element= NULL;
  m_registry.get(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);
  DBUG_ASSERT(element);

  // Check proper MDL lock.
  DBUG_ASSERT(MDL_checker::is_write_locked(m_thd, object));

  // Remove the element from the chain of auto releasers.
  (void) m_current_releaser->remove(element);

  // Remove the element from the local registry.
  m_registry.remove(element);

  // We must store the object first, because dropping it from the
  // shared cache may evict the element and delete the object.
  bool error= Storage_adapter::store(m_thd, object);

  // Drop the element from the shared cache. We will do this even if
  // store() fails, because otherwise, an updated object will be present
  // in the cache, and manual steps would be needed to undo the changes.
  // Dropping it from the cache always will lead to a cache miss, forcing
  // a read of the unmodified object.
  Shared_dictionary_cache::instance()->drop(element);

  // Return the outcome of store().
  return error;
}
/* purecov: end */


// Add a new dictionary object.
template <typename T>
void Dictionary_client::add_and_reset_id(T* object)
{
  // This may be called only during the initial stages of bootstrapping.
  DBUG_ASSERT(m_thd->is_dd_system_thread() &&
              bootstrap::stage() < bootstrap::BOOTSTRAP_CREATED);

  // Make sure the object is not being used by this client.
  Cache_element<typename T::cache_partition_type> *element= NULL;
  m_registry.get(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);
  DBUG_ASSERT(!element);

  // We only add objects that have not received an id yet.
  DBUG_ASSERT(object->id() == INVALID_OBJECT_ID);

  // Assign a temporary unique id. This is needed to have unique cache keys.
  static dd::Object_id next_id= 1;
  dynamic_cast<dd::Entity_object_impl*>(object->impl())->set_id(next_id++);

  // Add it to the shared cache.
  Shared_dictionary_cache::instance()->put(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);

  // Make sure we get the same object in return.
  DBUG_ASSERT(element && element->object() == object);

  // Add the element to the local registry.
  m_registry.put(element);

  // Sign up for auto release.
  m_current_releaser->auto_release(element);
}


// Make a dictionary object sticky or not in the cache.
template <typename T>
void Dictionary_client::set_sticky(const T* object, bool sticky)
{
  // Check that the object is present.
  Cache_element<typename T::cache_partition_type> *element= NULL;
  m_registry.get(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);
  DBUG_ASSERT(element);

  // Alter the element's stickiness in the shared cache.
  Shared_dictionary_cache::instance()->set_sticky(element, sticky);
}


// Return the stickiness of an object.
template <typename T>
bool Dictionary_client::is_sticky(const T* object) const
{
  // Check that the object is present.
  Cache_element<typename T::cache_partition_type> *element= NULL;
  m_registry.get(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);
  DBUG_ASSERT(element);

  return element->sticky();
}


// Debug dump of the client and its registry to stderr.
/* purecov: begin inspected */
template <typename T>
void Dictionary_client::dump() const
{
#ifndef DBUG_OFF
  fprintf(stderr, "================================\n");
  fprintf(stderr, "Dictionary client\n");
  m_registry.dump<T>();
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

template bool Dictionary_client::fetch_catalog_components(
    std::vector<const Schema*>*) const;

template bool Dictionary_client::fetch_global_components(
    std::vector<const Charset*>*) const;

template bool Dictionary_client::fetch_global_components(
    std::vector<const Collation*>*) const;

template bool Dictionary_client::fetch_global_components(
    std::vector<const Tablespace*>*) const;

template bool Dictionary_client::fetch_global_components(
    std::vector<const Event*>*) const;

template bool Dictionary_client::fetch_global_components(
    std::vector<const Schema*>*) const;

template bool Dictionary_client::fetch_schema_component_names<Abstract_table>(
    const Schema*,
    std::vector<std::string>*) const;

template bool Dictionary_client::fetch_schema_component_names<Event>(
    const Schema*,
    std::vector<std::string>*) const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Abstract_table**);
template bool Dictionary_client::acquire<Abstract_table>(const std::string&,
                                         const std::string&,
                                         const Abstract_table**);
template bool Dictionary_client::acquire_uncached(const std::string&,
                                                  const std::string&,
                                                  const Abstract_table**);
template bool Dictionary_client::drop(const Abstract_table*);
template bool Dictionary_client::store(Abstract_table*);
template void Dictionary_client::add_and_reset_id(Abstract_table*);
template bool Dictionary_client::update(const Abstract_table**,
                                        Abstract_table*, bool);
template void Dictionary_client::set_sticky(const Abstract_table*, bool);
template bool Dictionary_client::is_sticky(const Abstract_table*) const;
template void Dictionary_client::dump<Abstract_table>() const;

template bool Dictionary_client::acquire(Object_id, dd::Charset const**);
template bool Dictionary_client::acquire<dd::Charset>(std::string const&,
                                                      dd::Charset const**);

template bool Dictionary_client::drop(const Charset*);
template bool Dictionary_client::store(Charset*);
template void Dictionary_client::add_and_reset_id(Charset*);
template bool Dictionary_client::update(const Charset**, Charset*, bool);
template void Dictionary_client::set_sticky(const Charset*, bool);
template bool Dictionary_client::is_sticky(const Charset*) const;
template void Dictionary_client::dump<Charset>() const;


template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Charset**);
template bool Dictionary_client::acquire(Object_id, dd::Collation const**);
template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Collation**);
template bool Dictionary_client::acquire(const std::string &,
                                         const Collation**);
template bool Dictionary_client::drop(const Collation*);
template bool Dictionary_client::store(Collation*);
template void Dictionary_client::add_and_reset_id(Collation*);
template bool Dictionary_client::update(const Collation**, Collation*, bool);
template void Dictionary_client::set_sticky(const Collation*, bool);
template bool Dictionary_client::is_sticky(const Collation*) const;
template void Dictionary_client::dump<Collation>() const;

/* purecov: begin deadcode */
template bool Dictionary_client::acquire(Object_id, dd::Schema const**);
/* purecov: end */
template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Schema**);
template bool Dictionary_client::acquire_uncached(const std::string&,
                                                  const Schema**);
template bool Dictionary_client::drop(const Schema*);
template bool Dictionary_client::store(Schema*);
template void Dictionary_client::add_and_reset_id(Schema*);
template bool Dictionary_client::update(const Schema**, Schema*, bool);
template void Dictionary_client::set_sticky(const Schema*, bool);
template bool Dictionary_client::is_sticky(const Schema*) const;
template void Dictionary_client::dump<Schema>() const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Table**);
template bool Dictionary_client::acquire_uncached_uncommitted(Object_id,
                                                  const Table**);
template bool Dictionary_client::acquire(Object_id,
                                         const Table**);
template bool Dictionary_client::acquire(const std::string&,
                                         const std::string&,
                                         const Table**);
template bool Dictionary_client::acquire_uncached(const std::string&,
                                                  const std::string&,
                                                  const Table**);
template bool Dictionary_client::drop(const Table*);
template bool Dictionary_client::store(Table*);
template bool Dictionary_client::update_and_invalidate(Table*);
template void Dictionary_client::add_and_reset_id(Table*);
template bool Dictionary_client::update(const Table**, Table*, bool);
template void Dictionary_client::set_sticky(const Table*, bool);
template bool Dictionary_client::is_sticky(const Table*) const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Tablespace**);
template bool Dictionary_client::acquire_uncached_uncommitted(Object_id,
                                                            const Tablespace**);
template bool Dictionary_client::acquire(const std::string&,
                                         const Tablespace**);
template bool Dictionary_client::acquire_uncached(const std::string&,
                                                  const Tablespace**);
template bool Dictionary_client::acquire(Object_id,
                                         const Tablespace**);
template bool Dictionary_client::drop(const Tablespace*);
template bool Dictionary_client::store(Tablespace*);
template bool Dictionary_client::update_and_invalidate(Tablespace*);
template void Dictionary_client::add_and_reset_id(Tablespace*);
template bool Dictionary_client::update(const Tablespace**, Tablespace*, bool);
template void Dictionary_client::set_sticky(const Tablespace*, bool);
template bool Dictionary_client::is_sticky(const Tablespace*) const;
template void Dictionary_client::dump<Tablespace>() const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const View**);
template bool Dictionary_client::acquire(Object_id,
                                         const View**);
template bool Dictionary_client::acquire(const std::string&,
                                         const std::string&,
                                         const View**);
template bool Dictionary_client::acquire_uncached(const std::string&,
                                                  const std::string&,
                                                  const View**);
template bool Dictionary_client::drop(const View*);
template bool Dictionary_client::store(View*);
template void Dictionary_client::add_and_reset_id(View*);
template bool Dictionary_client::update(const View**, View*, bool);
template void Dictionary_client::set_sticky(const View*, bool);
template bool Dictionary_client::is_sticky(const View*) const;


template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Event**);
template bool Dictionary_client::acquire(Object_id,
                                         const Event**);
template bool Dictionary_client::acquire<Event>(const std::string&,
                                                const std::string&,
                                                const Event**);
template bool Dictionary_client::acquire_uncached(const std::string&,
                                                  const std::string&,
                                                  const Event**);
template bool Dictionary_client::drop(const Event*);
template bool Dictionary_client::store(Event*);
template bool Dictionary_client::update(const Event**, Event*, bool);
template void Dictionary_client::add_and_reset_id(Event*);
template void Dictionary_client::set_sticky(const Event*, bool);
template bool Dictionary_client::is_sticky(const Event*) const;


template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Function**);
template bool Dictionary_client::acquire(Object_id,
                                         const Function**);
template bool Dictionary_client::acquire(const std::string&,
                                         const std::string&,
                                         const Function**);
template bool Dictionary_client::acquire_uncached(const std::string&,
                                                  const std::string&,
                                                  const Function**);
template bool Dictionary_client::drop(const Function*);
template bool Dictionary_client::store(Function*);
template bool Dictionary_client::update(const Function**, Function*,  bool);
template void Dictionary_client::add_and_reset_id(Function*);
template void Dictionary_client::set_sticky(const Function*, bool);
template bool Dictionary_client::is_sticky(const Function*) const;


template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Procedure**);
template bool Dictionary_client::acquire(Object_id,
                                         const Procedure**);
template bool Dictionary_client::acquire(const std::string&,
                                         const std::string&,
                                         const Procedure**);
template bool Dictionary_client::acquire_uncached(const std::string&,
                                                  const std::string&,
                                                  const Procedure**);
template bool Dictionary_client::drop(const Procedure*);
template bool Dictionary_client::store(Procedure*);
template bool Dictionary_client::update(const Procedure**, Procedure*, bool);
template void Dictionary_client::add_and_reset_id(Procedure*);
template void Dictionary_client::set_sticky(const Procedure*, bool);
template bool Dictionary_client::is_sticky(const Procedure*) const;

template bool Dictionary_client::drop(const Routine*);
template bool Dictionary_client::update(const Routine**, Routine*, bool);

template bool Dictionary_client::acquire<Function>(
  const std::string&,
  const std::string&,
  const Function::cache_partition_type**);
template bool Dictionary_client::acquire<Procedure>(
  const std::string&,
  const std::string&,
  const Procedure::cache_partition_type**);

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Routine**);
/**
 @endcond
*/

} // namespace cache
} // namespace dd

