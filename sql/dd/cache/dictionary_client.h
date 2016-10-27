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

#ifndef DD_CACHE__DICTIONARY_CLIENT_INCLUDED
#define DD_CACHE__DICTIONARY_CLIENT_INCLUDED

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "dd/object_id.h"
#include "my_dbug.h"
#include "my_global.h"                        // DBUG_ASSERT() etc.
#include "object_registry.h"                  // Object_registry

class THD;
namespace dd {
class Schema;
class Table;
}  // namespace dd

namespace dd {
namespace cache {


/**
  Implementation of a dictionary client.

  The dictionary client provides a unified interface to accessing dictionary
  objects. The client is a member of the THD, and is typically used in
  server code to access the dictionary. When we refer to "the user" below,
  we mean the server code using the dictionary client.

  The main task of the client is to access a shared cache to retrieve
  dictionary objects. The shared cache, in its turn, will access the
  dictionary tables if there is a cache miss.

  To support cache eviction, the shared cache must keep track of which
  clients that have acquired an object. When a client acquires an object
  from the shared cache for the first time, it is added to a client local
  object registry. Further acquisition of the same object from the client
  will get the object from the client's registry. Thus, the usage tracking
  in the shared cache only keep track of the number of clients currently
  using the object, and hence, there must be an operation that complements
  acquisition, to inform the shared cache that the object is not used
  anymore. This complementing operation is called releasing the object.

  To manage releasing objects, the Auto_releaser class provides some
  support. When an auto releaser is instantiated, it will keep track of
  the objects that are acquired from the shared cache in its lifetime.
  Auto releasers may be nested or stacked, and the current releaser is
  the one at the top of the stack. The auto releaser stack is associated
  with a dictionary client instance. When the auto releaser goes out
  of scope, it will release all objects that have been acquired from the
  shared cache in its lifetime. Objects retrieved earlier than that will
  be automatically released by a releaser further down the auto releaser
  stack. For more coarse grained control, there is a release method that
  will release all objects acquired by the client.

  In addition to the auto releasers, the client has an object registry.
  The registry holds pointers to all currently acquired objects. Thus,
  the object registry is the union of the registers in the stack of
  auto releasers. The client's object registry is used for looking up
  objects, while the registers in the auto releasers are used for
  releasing objects.

  @note We must handle situations where an object is actually acquired from
        the shared cache, while the dynamic cast to a subtype fails. We use
        the auto release mechanism to achieve that.

  @note When a dictionary client method returns true, indicating that an
        error has occurred, the error has been reported, either by the
        client itself, or by the dictionary subsystem.
*/

template <typename T> class Cache_element;

class Dictionary_client
{
public:


  /**
    Class to help releasing and deleting objects.

    This class keeps a register of shared objects that are automatically
    released when the instance goes out of scope. When a new instance
    is created, the encompassing dictionary client's current auto releaser
    is replaced by this one, keeping a link to the old one. When the
    auto releaser is deleted, it links the old releaser back in as the
    client's current releaser.

    Shared objects that are added to the auto releaser will be released when
    the releaser is deleted. Only the dictionary client is allowed to add
    objects to the auto releaser.

    The usage pattern is that objects that are retrieved from the shared
    dictionary cache are added to the current auto releaser. Objects that
    are retrieved from the client's local object register are not added to
    the auto releaser. Thus, when the releaser is deleted, it releases all
    objects that have been retrieved from the shared cache during the
    lifetime of the releaser.

    Similarly the auto releaser maintains a list of objects created
    by acquire_uncached(). These objects are owned by the Auto_releaser
    and are deleted when the auto releaser goes out of scope.
  */

  class Auto_releaser
  {
    friend class Dictionary_client;

  private:
    Dictionary_client *m_client;
    Object_registry m_release_registry;
    Auto_releaser *m_prev;
    std::vector<Dictionary_object*> m_uncached_objects;

    /**
      Register an object to be auto released.

      @tparam T        Dictionary object type.
      @param  element  Cache element to auto release.
    */

    template <typename T>
    void auto_release(Cache_element<T> *element)
    {
      // Catch situations where we do not use a non-default releaser.
      DBUG_ASSERT(m_prev != NULL);
      m_release_registry.put(element);
    }


    /**
      Register an uncached object to be auto deleted.

      @param  object  Dictionary object to auto delete.
    */
    template <typename T>
    void auto_delete(Dictionary_object *object)
    {
      // Catch situations where we do not use a non-default releaser.
      DBUG_ASSERT(m_prev != NULL);

#ifndef DBUG_OFF
      // Make sure we do not sign up a shared object for auto delete.
      Cache_element<typename T::cache_partition_type> *element= nullptr;
      m_client->m_registry.get(
        static_cast<const typename T::cache_partition_type*>(object),
        &element);
      DBUG_ASSERT(element == nullptr);
#endif

      m_uncached_objects.push_back(object);
    }


    /**
      Transfer an object from the current to the previous auto releaser.

      @tparam T        Dictionary object type.
      @param  object   Dictionary object to transfer.
    */

    template <typename T>
    void transfer_release(const T* object);


    /**
      Remove an element from some auto releaser down the chain.

      Return a pointer to the releaser where the element was found.
      Thus, the element may be re-inserted into the appropriate
      auto releaser after e.g. changing the keys.

      @tparam T        Dictionary object type.
      @param  element  Cache element to auto remove.

      @return Pointer to the auto releaser where the object was signed up.
     */

    template <typename T>
    Auto_releaser *remove(Cache_element<T> *element);

    // Create a new empty auto releaser. Used only by the Dictionary_client.
    Auto_releaser();


  public:


    /**
      Create a new auto releaser and link it into the dictionary client
      as the current releaser.

      @param  client  Dictionary client for which to install this auto
                      releaser.
    */

    explicit Auto_releaser(Dictionary_client *client);


    // Release all objects registered and restore previous releaser.
    ~Auto_releaser();


    // Debug dump to stderr.
    template <typename T>
    void dump() const;
  };


private:
  Object_registry m_registry;        // Local object registry.
  THD *m_thd;                        // Thread context, needed for cache misses.
  Auto_releaser m_default_releaser;  // Default auto releaser.
  Auto_releaser *m_current_releaser; // Current auto releaser.


  /**
    Get a dictionary object.

    The operation retrieves a dictionary object by one of its keys from the
    cache and returns it through the object parameter. If the object is
    already present in the client's local object registry, it is fetched
    from there. Otherwise, it is fetched from the shared cache (possibly
    involving a cache miss), and eventually added to the local object
    registry.

    If no object is found for the given key, NULL is returned. The shared
    cache owns the returned object, i.e., the caller must not delete it.
    After using the object(s), the user must release it using one of the
    release mechanisms described earlier.

    The reference counter for the object is incremented if the object is
    retrieved from the shared cache. If the object was present in the local
    registry, the reference counter stays the same. A cache miss is handled
    transparently by the shared cache.

    @note This function must be called with type T being the same as
          T::cache_partition_type. Dynamic casting to the actual subtype
          must be done at an outer level.

    @tparam      K       Key type.
    @tparam      T       Dictionary object type.
    @param       key     Key to use for looking up the object.
    @param [out] object  Object pointer, if an object exists, otherwise NULL.
    @param [out] local   Whether the object was read from the local object
                         registry.

    @retval      false   No error.
    @retval      true    Error (from handling a cache miss).
  */

  template <typename K, typename T>
  bool acquire(const K &key, const T **object, bool *local);


  /**
    Mark all objects of a certain type as not being used by this client.

    This function is called with the client's own object registry, or with
    the registry of an auto releaser (which will contain a subset of the
    objects in the client's object registry).

    The function will release all objects of a given type in the registry
    submitted.The objects must be present and in use. If the objects become
    unused, they are added to the free list in the shared cache, which is
    then rectified to enforce its capacity constraints. The objects are also
    removed from the client's object registry.

    @tparam      T        Dictionary object type.
    @param       registry Object registry tp release from.

    @return Number of objects released.
  */

  template <typename T>
  size_t release(Object_registry *registry);


  /**
    Release all objects in the submitted object registry.

    This function will release all objects from the client's registry, or
    from the registry of an auto releaser.

    @param       registry Object registry tp release from.

    @return Number of objects released.
  */

  size_t release(Object_registry *registry);

public:


  // Initialize an instance with a default auto releaser.
  explicit Dictionary_client(THD *thd);


  // Make sure all objects are released.
  ~Dictionary_client();


  /**
    Retrieve an object by its object id.

    @tparam       T       Dictionary object type.
    @param        id      Object id to retrieve.
    @param [out]  object  Dictionary object, if present; otherwise NULL.

    @retval       false   No error.
    @retval       true    Error (from handling a cache miss).
  */

  template <typename T>
  bool acquire(Object_id id, const T** object);


  /**
    Retrieve an object by its object id without caching it.

    The object is not cached but owned by the current auto releaser who
    makes sure it is deleted. The object must not be released, and may not
    be used as a parameter to the other dictionary client methods since it is
    not known by the object registry.

    @tparam       T       Dictionary object type.
    @param        id      Object id to retrieve.
    @param [out]  object  Dictionary object, if present; otherwise NULL.

    @retval       false   No error.
    @retval       true    Error (from reading the dictionary tables).
   */

  template <typename T>
  bool acquire_uncached(Object_id id, T** object);


  /**
    Retrieve an object by its name.

    @tparam       T             Dictionary object type.
    @param        object_name   Name of the object.
    @param [out]  object        Dictionary object, if present; otherwise NULL.

    @retval       false   No error.
    @retval       true    Error (from handling a cache miss).
  */

  template <typename T>
  bool acquire(const String_type &object_name, const T** object);


  /**
    Retrieve an object by its name without caching it.

    The object is not cached but owned by the current auto releaser who
    makes sure it is deleted. The object must not be released, and may not
    be used as a parameter to the other dictionary client methods since it is
    not known by the object registry.

    @note This is needed when acquiring objects during bootstrap to make
          sure we get objects from the DD tables in order to replace the
          temporarily generated meta data.

    @tparam       T            Dictionary object type.
    @param        object_name  Name of the object to be retrieved.
    @param [out]  object       Dictionary object, if present; otherwise NULL.

    @retval       false        No error.
    @retval       true         Error (from reading the dictionary tables).
   */

  template <typename T>
  bool acquire_uncached(const String_type &object_name, T** object);


  /**
    Retrieve an object by its schema- and object name.

    @note We will acquire an IX-lock on the schema name unless we already
          have one. This is needed for proper synchronization with schema
          DDL in cases where the table does not exist, and where the
          indirect synchronization based on table names therefore will not
          apply.

    @todo TODO: We should change the MDL acquisition (see above) for a more
          long term solution.

    @tparam       T             Dictionary object type.
    @param        schema_name   Name of the schema containing the object.
    @param        object_name   Name of the object.
    @param [out]  object        Dictionary object, if present; otherwise NULL.

    @retval       false   No error.
    @retval       true    Error (from handling a cache miss, or from
                                 failing to get an MDL lock).
  */

  template <typename T>
  bool acquire(const String_type &schema_name, const String_type &object_name,
               const T** object);


  /**
    Retrieve an object by its schema- and object name.

    @note We will acquire an IX-lock on the schema name unless we already
          have one. This is needed for proper synchronization with schema
          DDL in cases where the table does not exist, and where the
          indirect synchronization based on table names therefore will not
          apply.

    @note This is a variant of the method above asking for an object of type
          T, and hence using T's functions for updating name keys etc.
          This function, however, returns the instance pointed to as type
          T::cache_partition_type to ease handling of various subtypes
          of the same base type.

    @todo TODO: We should change the MDL acquisition (see above) for a more
          long term solution.

    @tparam       T             Dictionary object type.
    @param        schema_name   Name of the schema containing the object.
    @param        object_name   Name of the object.
    @param [out]  object        Dictionary object, if present; otherwise NULL.

    @retval       false   No error.
    @retval       true    Error (from handling a cache miss, or from
                                 failing to get an MDL lock).
  */

  template <typename T>
  bool acquire(const String_type &schema_name, const String_type &object_name,
               const typename T::cache_partition_type** object);


  /**
    Retrieve an object by its schema- and object name without caching it.

    The object is not cached but owned by the current auto releaser who
    makes sure it is deleted. The object must not be released, and may not
    be used as a parameter to the other dictionary client methods since it is
    not known by the object registry.

    @note This is needed when acquiring objects during bootstrap to make
          sure we get objects from the DD tables in order to replace the
          temporarily generated meta data.

    @tparam       T             Dictionary object type.
    @param        schema_name   Name of the schema containing the table.
    @param        object_name   Name of the object.
    @param [out]  object        Dictionary object, if present; otherwise NULL.

    @retval       false   No error.
    @retval       true    Error (from handling a cache miss).
  */

  template <typename T>
  bool acquire_uncached(const String_type &schema_name,
                        const String_type &object_name,
                        T** object);


  /**
    Retrieve a possibly uncommitted object by its schema- and object name
    without caching it.

    The object is not cached, hence, it is owned by the caller, who must
    make sure it is deleted. The object must not be released, and may not
    be used as a parameter to the other dictionary client methods since it is
    not known by the object registry.

    When the object is read from the persistent tables, the transaction
    isolation level is READ UNCOMMITTED. This is necessary to be able to
    read uncommitted data from an earlier stage of the same session.

    @tparam       T             Dictionary object type.
    @param        schema_name   Name of the schema containing the table.
    @param        object_name   Name of the object.
    @param [out]  object        Dictionary object, if present; otherwise NULL.

    @retval       false   No error.
    @retval       true    Error (from handling a cache miss).
  */

  template <typename T>
  bool acquire_uncached_uncommitted(const String_type &schema_name,
                                    const String_type &object_name,
                                    const T** object);


  /**
    Retrieve a table object by its se private id.

    @param       engine        Name of the engine storing the table.
    @param       se_private_id SE private id of the table.
    @param [out] table         Table object, if present; otherwise NULL.

    @note The object must be acquired uncached since we cannot acquire a
          metadata lock in advance since we do not know the table name.
          Thus, the returned table object is owned by the caller, who must
          make sure it is deleted.

    @retval      false    No error or if object was not found.
    @retval      true     Error (e.g. from reading DD tables, or if an
                                 object of a wrong type was found).
  */

  bool acquire_uncached_table_by_se_private_id(const String_type &engine,
                                               Object_id se_private_id,
                                               Table **table);


  /**
    Retrieve a table object by its partition se private id.

    @param       engine           Name of the engine storing the table.
    @param       se_partition_id  SE private id of the partition.
    @param [out] table            Table object, if present; otherwise NULL.

    @retval      false    No error or if object was not found.
    @retval      true     Error (from handling a cache miss).
  */

  bool acquire_uncached_table_by_partition_se_private_id(
         const String_type &engine,
         Object_id se_partition_id,
         Table **table);


  /**
    Retrieve schema and table name by the se private id of the table.

    @param        engine          Name of the engine storing the table.
    @param        se_private_id   SE private id of the table.
    @param  [out] schema_name     Name of the schema containing the table.
    @param  [out] table_name      Name of the table.

    @retval      false    No error OR if object was not found.
                          The OUT params will be set to empty
                          string when object is not found.
    @retval      true     Error.
  */

  bool get_table_name_by_se_private_id(const String_type &engine,
                                       Object_id se_private_id,
                                       String_type *schema_name,
                                       String_type *table_name);


  /**
    Retrieve schema and table name by the se private id of the partition.

    @param        engine           Name of the engine storing the table.
    @param        se_partition_id  SE private id of the table partition.
    @param  [out] schema_name      Name of the schema containing the table.
    @param  [out] table_name       Name of the table.

    @retval      false    No error or if object was not found.
                          The OUT params will be set to empty
                          string when object is not found.
    @retval      true     Error.
  */

  bool get_table_name_by_partition_se_private_id(const String_type &engine,
                                                 Object_id se_partition_id,
                                                 String_type *schema_name,
                                                 String_type *table_name);


  /**
    Retrieve a table name of a given trigger name and schema id.

    @param        schema_id        schema id of the trigger.
    @param        trigger_name     Name of the trigger.
    @param  [out] table_name       Name of the table for which
                                   trigger belongs to. Empty string if
                                   there is no such trigger.

    @retval      false    No error.
    @retval      true     Error.
  */

  bool get_table_name_by_trigger_name(Object_id schema_id,
                                      const String_type &trigger_name,
                                      String_type *table_name);


  /**
    Get the highest currently used se private id for the table objects.

    @param       engine        Name of the engine storing the table.
    @param [out] max_id        Max SE private id.

    @return      true   Failure (error is reported).
    @return      false  Success.
  */

  bool get_tables_max_se_private_id(const String_type &engine,
                                    Object_id *max_id);


  /**
    Fetch the names of all the components in the schema.

    @note          This is an intermediate solution which will be replaced
                   by the implementation in WL#6599.

    @tparam        T              Type of object to retrieve names for.
    @param         schema         Schema for which to get component names.
    @param   [out] names          An std::vector containing all object names.

    @return      true   Failure (error is reported).
    @return      false  Success.
  */

  template <typename T>
  bool fetch_schema_component_names(
    const Schema *schema,
    std::vector<String_type> *names) const;


  /**
    Fetch all components in the schema.

    @tparam        T              Type of components to get.
    @param         schema         Schema for which to get components.
    @param   [out] coll           An std::vector containing all components.

    @return      true   Failure (error is reported).
    @return      false  Success.
  */

  template <typename T>
  bool fetch_schema_components(
    const Schema *schema,
    std::vector<const T*> *coll) const;


  /**
    Fetch all global components of the given type.

    @tparam        T              Type of components to get.
    @param   [out] coll           An std::vector containing all components.

    @return      true   Failure (error is reported).
    @return      false  Success.
  */

  template <typename T>
  bool fetch_global_components(std::vector<const T*> *coll) const;


  /**
    Fetch Object ids of all the views referencing base table/ view/ stored
    function name specified in "schema"."name".

    @tparam       T               Type of the object (View_table/View_routine)
                                  to retrieve view names for.
    @param        schema          Schema name.
    @param        tbl_or_sf_name  Base table/ View/ Stored function name.
    @param[out]   view_ids        Vector to store Object ids of all the views
                                  referencing schema.name.

    @return      true   Failure (error is reported).
    @return      false  Success.
  */
  template <typename T>
  bool fetch_referencing_views_object_id(
    const char *schema,
    const char *tbl_or_sf_name,
    std::vector<Object_id> *view_ids) const;


  /**
    Mark all objects acquired by this client as not being used anymore.

    This function will release all objects from the client's registry.

    @return Number of objects released.
  */

  size_t release();


  /**
    Remove and delete an object from the cache and the dd tables.

    This function will remove the object from the local registry as well as
    the shared cache. This means that all keys associated with the object will
    be removed from the maps, and the cache element wrapper will be deleted.
    Afterwards, the object pointed to will also be deleted, and finally, the
    corresponding entry in the appropriate dd table is deleted. The object may
    not be accessed after calling this function.

    @note The object parameter is const since the contents of the object
          is not really changed, the object is just deleted. The method
          makes sure there is an exclusive meta data lock on the object
          name.

    @tparam T       Dictionary object type.
    @param  object  Object to be dropped.

    @retval false   The operation was successful.
    @retval true    There was an error.
  */

  template <typename T>
  bool drop(const T *object);

  /**
    Remove and delete entries corresponding to uncached object from
    the data-dictionary and its cache (if any).

    @note This function assumes that object is not in the local
          registry (i.e. was not acquired by current client).
          It is responsibility of caller to free memory occupied
          by the uncached object.

    @tparam T       Dictionary object type.
    @param  object  Object to be dropped.

    @retval false   The operation was successful.
    @retval true    There was an error.
  */
  template <typename T>
  bool drop_uncached(const T *object);


  /**
    Store a new dictionary object.

    This function will write the object to the dd tables. The object is
    added neither to the dictionary client's object registry nor the shared
    cache.

   @note A precondition is that the object has not been acquired from the
         shared cache. For storing an object which is already in the cache,
         please use update().

    @tparam T       Dictionary object type.
    @param  object  Object to be stored.

    @retval false   The operation was successful.
    @retval true    There was an error.
  */

  template <typename T>
  bool store(T* object);


  /**
    Replace a dictionary object by another and store the new one.

    This function will replace one dictionary object by another. The new object
    is also stored to the DD tables. The old object is deleted and may not be
    accessed after calling this function. The element wrapper is still present
    in the local object registry (and the shared cache), now with the new object
    being wrapped, and must be released eventually as usual.

    @note The new_object will be cloned, and the clone will be owned by the
          shared cache. The new_object pointer submitted to this function must
          be deleted explicitly by the caller.

    @note The old_object pointer will be reset to point to the new_object clone
          being owned by the cache.

    @tparam          T          Dictionary object type.
    @param  [in,out] old_object Old object, present in the cache, to be
                                replaced. Will be set to point to the new
                                cached object (upon success) or stay pointing
                                to the old object (upon failure).
    @param           new_object New object, not present in the cache, to replace
                                the old one.
    @param           persist    Store update persistently, default is true.
                                Only the bootstrap thread is allowed to
                                override this.

    @retval          false      The operation was successful.
    @retval          true       There was an error.
  */

  template <typename T>
  bool update(const T** old_object, T* new_object, bool persist= true);


  /**
    Update a modified dictionary object and remove it from the cache.

    This function will store the modified object into the DD tables,
    and remove the corresponding object from the shared cache if it
    is there. Then, further attempts to acquire the object will result
    in a cache miss, thus reading the object from the DD tables.
    This behavior is needed to maintain cache consistency regardless
    of the transaction outcome (commit or rollback).

    @note There must be an exclusive meta data lock on the object
          prior to calling this function.

    @note This operation is not allowed on a sticky object, since a sticky
          object should always be present in the cache.

    @note It is assumed that object is not in local registry (i.e. was
          not acquired by this dictionary client).

    @note It is responsibility of the caller to delete both old and new
          versions of updated object.

    @tparam T       Dictionary object type.
    @param  old_object  Old version of object to be updated (uncached!).
                        It is required only when object changes its name
                        or schema, can be nullptr otherwise.
    @param  new_object  New version of object to be updated.

    @retval false   The operation was successful.
    @retval true    There was an error.
  */

  template <typename T>
  bool update_uncached_and_invalidate(const T* old_object, T* new_object);


  /**
    This function is a wrapper to the function below.

    It calls add_and_reset_id() with value 'false' for the
    second parameter.

    @tparam T        Dictionary object type.
    @param  object   Object to be added to the shared cache
                     and the object registry.
  */

  template <typename T>
  void add_and_reset_id(T* object);


  /**
    Add a new dictionary object and assign an id.

    This function will add the object to the dictionary client's object
    registry and the shared cache. The object is not stored into the persistent
    dd tables. The newly added object's element is returned to the dictionary
    client and added to the local registry. The object must be released
    afterwards,

    The id should reset to 1 if we have cleared DD cache.

    @note This function is only to be used during server start.

    @note The new object will be owned by the shared cache. Thus, the
          dictionary user may not delete the object. Instead, the
          object must be released in the same way as other dictionary
          objects.

    @tparam T        Dictionary object type.
    @param  reset_id Reset id to 1
    @param  object   Object to be added to the shared cache
                     and the object registry.
  */

  template <typename T>
  void add_and_reset_id(T* object, bool reset_id);


  /**
    Remove table statistics entries from mysql.table_stats and
    mysql.index_stats.

    @param schema_name  Schema name of the table
    @param table_name   Table name of which stats should be cleaned.

    @return true  - on failure
    @return false - on success
  */
  bool remove_table_dynamic_statistics(const String_type &schema_name,
                                       const String_type &table_name);

  /**
    Make a dictionary object sticky or not in the cache.

    The object must be present in the local object registry.

    @tparam T       Dictionary object type.
    @param  object  Object to have its stickiness altered.
    @param  sticky  Whether the object should be sticky or not.
  */

  template <typename T>
  void set_sticky(const T* object, bool sticky);


  /**
    Return the stickiness of an object.

    The object must be present in the local object registry.

    @note The function reads the stickiness directly from the cache element
          in the client's object registry without locking or atomic read.

    @tparam T       Dictionary object type.
    @param  object  Object for which to check stickiness.

    @return  Whether the object is sticky or not.
  */

  template <typename T>
  bool is_sticky(const T* object) const;


  /**
    Debug dump of a partition of the client and its registry to stderr.

    @tparam T       Dictionary object type.
  */

  template <typename T>
  void dump() const;
};

} // namespace cache
} // namespace dd

#endif // DD_CACHE__DICTIONARY_CLIENT_INCLUDED
