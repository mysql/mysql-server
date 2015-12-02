/* Copyright (c) 2015 Oracle and/or its affiliates. All rights reserved.

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
#include "mysqld.h"                          // mysqld_server_started
#include "sql_class.h"                       // THD

#include "cache_element.h"                   // Cache_element
#include "shared_dictionary_cache.h"         // get(), release(), ...
#include "storage_adapter.h"                 // store(), drop(), ...
#include "dd/dd_schema.h"                    // dd::Schema_MDL_locker
#include "dd/properties.h"                   // Properties
#include "dd/types/abstract_table.h"         // Abstract_table
#include "dd/types/charset.h"                // Charset
#include "dd/types/collation.h"              // Collation
#include "dd/types/fwd.h"                    // Schema_const_iterator
#include "dd/types/schema.h"                 // Schema
#include "dd/types/table.h"                  // Table
#include "dd/types/tablespace.h"             // Tablespace
#include "dd/types/view.h"                   // View
#include "dd/impl/dictionary_object_collection.h" // Dictionary_object_coll...
#include "dd/impl/transaction_impl.h"        // Transaction_ro
#include "dd/impl/raw/object_keys.h"         // Primary_id_key, ...
#include "dd/impl/raw/raw_record_set.h"      // Raw_record_set
#include "dd/impl/raw/raw_table.h"           // Raw_table
#include "dd/impl/tables/character_sets.h"   // create_name_key()
#include "dd/impl/tables/collations.h"       // create_name_key()
#include "dd/impl/tables/schemata.h"         // create_name_key()
#include "dd/impl/tables/tables.h"           // create_name_key()
#include "dd/impl/tables/tablespaces.h"      // create_name_key()
#include "dd/impl/tables/table_partitions.h" // get_partition_table_id()
#include "dd/impl/types/object_table_definition_impl.h" // fs_name_case()

namespace {


/**
  Helper class providing overloaded functions asserting that we have proper
  MDL locks in place. Please note that the functions cannot be called
  until after we have the name of the object, so if we acquire an object
  by id, the asserts must be delayed until the object is retrieved.

  @note Checking for MDL locks is disabled until the server is started,
        as indicated by the 'mysqld_server_started' flag. This is because
        in this phase, MDL locks are not acquired since the server is not
        available for user connections yet.
*/

class MDL_checker
{
private:

  /**
    Private helper function for asserting MDL for tables.

    @note For temporary tables, we have no locks.

    @param   thd          Thread context.
    @param   schema_name  Schema name to use in the MDL key.
    @param   table        Table object.
    @param   lock_type    Weakest lock type accepted.

    @return true if we have the required lock, otherwise false.
  */

  static bool is_locked(THD *thd, const std::string &schema_name,
                        const dd::Abstract_table *table,
                        enum_mdl_type lock_type)
  {
    // Skip check for temporary tables.
    if (!table || is_prefix(table->name().c_str(), tmp_file_prefix))
      return true;

    // We must take l_c_t_n into account when reconstructing the
    // MDL key from the table name.
    char table_name_buf[NAME_LEN + 1];

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
                              MDL_key::TABLE,
                              schema_name.c_str(),
                              dd::Object_table_definition_impl::
                                fs_name_case(table->name(),
                                             table_name_buf),
                              lock_type) ||
           thd->mdl_context.owns_equal_or_stronger_lock(
                              MDL_key::TABLE,
                              dd::Object_table_definition_impl::
                                fs_name_case(schema_name,
                                             schema_name_buf),
                              dd::Object_table_definition_impl::
                                fs_name_case(table->name(),
                                             table_name_buf),
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

    // Likewise, if there is no schema, we cannot have a proper lock.
    // This may in theory happen during bootstrapping since the meta data for
    // the system schema is not stored yet; however, this is prevented by
    // surrounding code calling this function only if 'mysql_server_started',
    // i.e., bootstrapping is finished.
    DBUG_ASSERT(mysqld_server_started);
    if (schema)
      return is_locked(thd, schema->name(), table, lock_type);

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
  { return !mysqld_server_started || is_locked(thd, table, MDL_SHARED); }

  // Writing a table object should be governed by MDL_EXCLUSIVE.
  static bool is_write_locked(THD *thd, const dd::Abstract_table *table)
  { return !mysqld_server_started || is_locked(thd, table, MDL_EXCLUSIVE); }

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
    // We must take l_c_t_n into account when comparing the schema name.
    char name_buf[NAME_LEN + 1];
    return !dd::Schema_MDL_locker::is_lock_required(
                dd::Object_table_definition_impl::
                fs_name_case(schema->name(), name_buf)) ||
      is_locked(thd, schema, MDL_INTENTION_EXCLUSIVE);
  }

  // Writing a schema object should be governed by MDL_EXCLUSIVE.
  static bool is_write_locked(THD *thd, const dd::Schema *schema)
  { return !mysqld_server_started || is_locked(thd, schema, MDL_EXCLUSIVE); }

  // Releasing a schema object should be covered in the same way as for reading.
  static bool is_release_locked(THD *thd, const dd::Schema *schema)
  { return is_read_locked(thd, schema); }

  /*
    Reading a tablespace object should be governed by at least
    MDL_INTENTION_EXCLUSIVE. IX is acquired when a tablespace is
    being accessed when creating/altering table.
  */
  static bool is_read_locked(THD *thd, const dd::Tablespace *tablespace)
  { return(!mysqld_server_started ||
           is_locked(thd, tablespace, MDL_INTENTION_EXCLUSIVE));
  }

  // Writing a tablespace object should be governed by MDL_EXCLUSIVE.
  static bool is_write_locked(THD *thd, const dd::Tablespace *tablespace)
  { return !mysqld_server_started || is_locked(thd, tablespace, MDL_EXCLUSIVE); }
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
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
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
          release<Collation>(registry);
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
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);

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
                get_uncached(m_thd, key, &stored_object);
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
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);

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
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
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
           get_uncached(m_thd, key, &stored_object);

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


// Retrieve a table object by its se private id.
bool Dictionary_client::acquire_uncached_table_by_se_private_id(
                                      const std::string &engine,
                                      Object_id se_private_id,
                                      const Table **table)
{
  // Create se private key.
  Table::aux_key_type key;
  Table::update_aux_key(&key, engine, se_private_id);

  const Table::cache_partition_type *stored_object= NULL;

  // Read the uncached dictionary object.
  bool error= Shared_dictionary_cache::instance()->
                get_uncached(m_thd, key, &stored_object);
  if (!error)
  {
    // Dynamic cast may legitimately return NULL only if the stored object
    // was NULL, i.e., the object did not exist.
    DBUG_ASSERT(table);
    *table= dynamic_cast<const Table*>(stored_object);

    // Delete the object and report error if dynamic cast fails.
    if (stored_object && !*table)
    {
      my_error(ER_INVALID_DD_OBJECT, MYF(0),
               Table::OBJECT_TABLE().name().c_str(),
               "Not a table object.");
      delete stored_object;
      return true;
    }
  }
  else
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);

  return error;
}

// Retrieve a table object by its partition se private id.
/* purecov: begin deadcode */
bool Dictionary_client::acquire_table_by_partition_se_private_id(
                                          const std::string &engine,
                                          Object_id se_partition_id,
                                          const Table **table)
{
  // We must make sure the objects are released correctly.
  Auto_releaser releaser(this);

  DBUG_ASSERT(table);
  *table= NULL;

  // Read record directly from the tables.
  Object_id table_id;
  if (tables::Table_partitions::get_partition_table_id(m_thd, engine,
                                                       se_partition_id,
                                                       &table_id))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }

  bool local= false;
  const Table::id_key_type key(table_id);
  const Table::cache_partition_type *cached_object= NULL;

  bool error= acquire(key, &cached_object, &local);

  if (!error)
  {
    // Dynamic cast may legitimately return NULL if we e.g. asked
    // for a dd::Table and got a dd::View in return.
    DBUG_ASSERT(table);
    *table= dynamic_cast<const Table*>(cached_object);

    // Don't auto release the object here if it is returned.
    if (!local && *table)
      releaser.transfer_release(cached_object);
  }
  else
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);

  return error;
}
/* purecov: end */


// Retrieve a schema- and table name by the se private id of the table.
bool Dictionary_client::get_table_name_by_se_private_id(
                                    const std::string &engine,
                                    Object_id se_private_id,
                                    std::string *schema_name,
                                    std::string *table_name)
{
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

  // Objects to be acquired.
  const Table *tab_obj= NULL;
  const Schema *sch_obj= NULL;

  // Sign up for delete.
  Object_deleter object_deleter(&tab_obj, &sch_obj);

  // Acquire the table uncached, because we cannot acquire a meta data
  // lock since we do not know the table name.
  if (acquire_uncached_table_by_se_private_id(engine, se_private_id, &tab_obj))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }

  if (!tab_obj)
  {
    my_error(ER_BAD_TABLE_ERROR, MYF(0), table_name->c_str());
    return true;
  }

  // Acquire the schema uncached to get the schema name. Like above, we
  // cannot lock it in advance since we do not know its name.
  if (acquire_uncached<Schema>(tab_obj->schema_id(), &sch_obj))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
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
  // We must make sure the objects are released correctly.
  Auto_releaser releaser(this);
  const Table *tab_obj= NULL;
  if (acquire_table_by_partition_se_private_id(engine,
                                               se_partition_id, &tab_obj))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }

  if (!tab_obj)
  {
    my_error(ER_BAD_TABLE_ERROR, MYF(0), schema_name->c_str());
    return true;
  }

  // Acquire the schema to get the schema name.
  const Schema *sch_obj= NULL;
  if (acquire<Schema>(tab_obj->schema_id(), &sch_obj))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
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
  dd::Transaction_ro trx(m_thd);

  trx.otx.register_tables<dd::Schema>();
  trx.otx.register_tables<dd::Table>();

  if (trx.otx.open_tables())
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }

  return dd::tables::Tables::max_se_private_id(&trx.otx, engine, max_id);
}
/* purecov: end */


// Fetch the names of all the components in the schema.
bool Dictionary_client::fetch_schema_component_names(
    const Schema *schema,
    std::vector<std::string> *names) const
{
  DBUG_ASSERT(names);

  // Create the key based on the schema id.
  std::unique_ptr<Object_key> object_key(
    dd::tables::Tables::create_key_by_schema_id(schema->id()));

  // Retrieve a set of the schema components, and add the component names
  // to the vector output parameter.
  Transaction_ro trx(m_thd);

  trx.otx.register_tables<dd::Abstract_table>();
  Raw_table *table= trx.otx.get_table<dd::Abstract_table>();
  DBUG_ASSERT(table);

  if (trx.otx.open_tables())
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }

  std::unique_ptr<Raw_record_set> rs;
  if (table->open_record_set(object_key.get(), rs))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    return true;
  }

  Raw_record *r= rs->current_record();
  while (r)
  {
    // Here, we need only the table name.
    names->push_back(r->read_str(tables::Tables::FIELD_NAME));

    if (rs->next(r))
    {
      DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
      return true;
    }
  }

  return false;
}


// Fetch all the components in the schema.
template <typename Iterator_type>
bool Dictionary_client::fetch_schema_components(
    const Schema *schema,
    std::unique_ptr<Iterator_type> *iter) const
{
  std::unique_ptr<Dictionary_object_collection<
    typename Iterator_type::Object_type> > c(
      new (std::nothrow) Dictionary_object_collection<
        typename Iterator_type::Object_type>(m_thd));
  {
    std::unique_ptr<Object_key> k(
      Iterator_type::Object_type::cache_partition_table_type::
        create_key_by_schema_id(schema->id()));

    if (c->fetch(k.get()))
    {
      DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
      iter->reset(NULL);
      return true;
    }
  }
  iter->reset(c.release());

  return false;
}


// Fetch all the objects of the given type in the default catalog.
/* purecov: begin deadcode */
template <typename Iterator_type>
bool Dictionary_client::fetch_catalog_components(
    std::unique_ptr<Iterator_type> *iter) const
{
  std::unique_ptr<Dictionary_object_collection
    <typename Iterator_type::Object_type> > c(
      new (std::nothrow) Dictionary_object_collection
        <typename Iterator_type::Object_type>(m_thd));
  {
    std::unique_ptr<Object_key> k(
      Iterator_type::Object_type::cache_partition_table_type::
        create_key_by_catalog_id(1));
    if (c->fetch(k.get()))
    {
      DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
      iter->reset(NULL);
      return true;
    }
  }

  iter->reset(c.release());
  return false;
}
/* purecov: end */


// Fetch all the global objects of the given type.
/* purecov: begin deadcode */
template <typename Iterator_type>
bool Dictionary_client::fetch_global_components(
    std::unique_ptr<Iterator_type> *iter) const
{
  std::unique_ptr<Dictionary_object_collection
    <typename Iterator_type::Object_type> > c(
      new (std::nothrow) Dictionary_object_collection
        <typename Iterator_type::Object_type>(m_thd));
  if (c->fetch(NULL))
  {
    DBUG_ASSERT(m_thd->is_error() || m_thd->killed);
    iter->reset(NULL);
    return true;
  }

  iter->reset(c.release());
  return false;
}
/* purecov: end */


// Mark all objects acquired by this client as not being used anymore.
size_t Dictionary_client::release()
{ return release(&m_registry); }


// Remove and delete an object from the cache and the dd tables.
template <typename T>
bool Dictionary_client::drop(T *object)
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

  DBUG_ASSERT(m_thd->is_error() || m_thd->killed);

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


// Update a modified dictionary object.
template <typename T>
bool Dictionary_client::update(T* object)
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
  Auto_releaser *actual_releaser= m_current_releaser->remove(element);

  // Remove the element from the local registry.
  m_registry.remove(element);

  // If we fail to store the new object, we must drop it from the shared
  // cache. This is easiest since we do not know here which changes to revert.
  // Dropping the object should be safe since this thread should be the only
  // user of the object. The element is already removed from the local
  // registry and the chain of auto releasers.
  if (store(object))
  {
    Shared_dictionary_cache::instance()->drop(element);
    return true;
  }

  // If the new object was successfully stored, we must replace the object
  // in the shared cache and re-create the keys.
  Shared_dictionary_cache::instance()->replace(element,
    static_cast<const typename T::cache_partition_type*>(object));

  // Put back the element, with its new keys, into the local registry.
  m_registry.put(element);

  // Put back the element into the correct auto releaser.
  if (actual_releaser)
    actual_releaser->auto_release(element);

  return false;
}


// Add a new dictionary object. (Needed by WL#6394)
/* purecov: begin deadcode */
template <typename T>
void Dictionary_client::add(const T* object)
{
  // Make sure the object is not being used by this client.
  Cache_element<typename T::cache_partition_type> *element= NULL;
  m_registry.get(
    static_cast<const typename T::cache_partition_type*>(object),
    &element);
  DBUG_ASSERT(!element);

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
/* purecov: end */


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
    std::unique_ptr<Abstract_table_const_iterator>*) const;

template bool Dictionary_client::fetch_schema_components(
    const Schema*,
    std::unique_ptr<Table_const_iterator>*) const;

template bool Dictionary_client::fetch_schema_components(
    const Schema*,
    std::unique_ptr<View_const_iterator>*) const;

template bool Dictionary_client::fetch_catalog_components(
    std::unique_ptr<Schema_const_iterator>*) const;

template bool Dictionary_client::fetch_global_components(
    std::unique_ptr<Tablespace_const_iterator>*) const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Abstract_table**);
template bool Dictionary_client::acquire(const std::string&,
                                         const std::string&,
                                         const Abstract_table**);
template bool Dictionary_client::acquire_uncached(const std::string&,
                                                  const std::string&,
                                                  const Abstract_table**);
template bool Dictionary_client::drop(Abstract_table*);
template bool Dictionary_client::store(Abstract_table*);
template bool Dictionary_client::update(Abstract_table*);
template void Dictionary_client::add(const Abstract_table*);
template void Dictionary_client::set_sticky(const Abstract_table*, bool);
template bool Dictionary_client::is_sticky(const Abstract_table*) const;
template void Dictionary_client::dump<Abstract_table>() const;

#ifndef DBUG_OFF
// These instantiations are currently only needed for unit testing
template bool Dictionary_client::acquire(Object_id, dd::Charset const**);
template bool Dictionary_client::acquire<dd::Charset>(std::string const&,
                                                      dd::Charset const**);
#endif /* !DBUG_OFF */

template bool Dictionary_client::drop(Charset*);
template bool Dictionary_client::store(Charset*);
template bool Dictionary_client::update(Charset*);
template void Dictionary_client::add(const Charset*);
template void Dictionary_client::set_sticky(const Charset*, bool);
template bool Dictionary_client::is_sticky(const Charset*) const;
template void Dictionary_client::dump<Charset>() const;


#ifndef DBUG_OFF
// These instantiations are currently only needed for unit testing
template bool Dictionary_client::acquire(Object_id, dd::Collation const**);
#endif /* !DBUG_OFF */

template bool Dictionary_client::acquire(const std::string &,
                                         const Collation**);
template bool Dictionary_client::drop(Collation*);
template bool Dictionary_client::store(Collation*);
template bool Dictionary_client::update(Collation*);
template void Dictionary_client::add(const Collation*);
template void Dictionary_client::set_sticky(const Collation*, bool);
template bool Dictionary_client::is_sticky(const Collation*) const;
template void Dictionary_client::dump<Collation>() const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Schema**);
template bool Dictionary_client::drop(Schema*);
template bool Dictionary_client::store(Schema*);
template bool Dictionary_client::update(Schema*);
template void Dictionary_client::add(const Schema*);
template void Dictionary_client::set_sticky(const Schema*, bool);
template bool Dictionary_client::is_sticky(const Schema*) const;
template void Dictionary_client::dump<Schema>() const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Table**);
template bool Dictionary_client::acquire(Object_id,
                                         const Table**);
template bool Dictionary_client::acquire(const std::string&,
                                         const std::string&,
                                         const Table**);
template bool Dictionary_client::acquire_uncached(const std::string&,
                                                  const std::string&,
                                                  const Table**);
template bool Dictionary_client::drop(Table*);
template bool Dictionary_client::store(Table*);
template bool Dictionary_client::update(Table*);
template void Dictionary_client::add(const Table*);
template void Dictionary_client::set_sticky(const Table*, bool);
template bool Dictionary_client::is_sticky(const Table*) const;

template bool Dictionary_client::acquire_uncached(Object_id,
                                                  const Tablespace**);
template bool Dictionary_client::acquire(const std::string&,
                                         const Tablespace**);
template bool Dictionary_client::acquire(Object_id,
                                         const Tablespace**);
template bool Dictionary_client::drop(Tablespace*);
template bool Dictionary_client::store(Tablespace*);
template bool Dictionary_client::update(Tablespace*);
template void Dictionary_client::add(const Tablespace*);
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
template bool Dictionary_client::drop(View*);
template bool Dictionary_client::store(View*);
template bool Dictionary_client::update(View*);
template void Dictionary_client::add(const View*);
template void Dictionary_client::set_sticky(const View*, bool);
template bool Dictionary_client::is_sticky(const View*) const;

/**
 @endcond
*/

} // namespace cache
} // namespace dd

