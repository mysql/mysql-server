/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_SERVER_DYNAMIC_LOADER_H
#define MYSQL_SERVER_DYNAMIC_LOADER_H

#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/mysql_runtime_error_service.h>

#include <mysql/components/services/dynamic_loader.h>
#include <forward_list>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "c_string_less.h"
#include "mysql_component_imp.h"
#include "rwlock_scoped_lock.h"

/**
  Making component object and the generation ID as a pair. Here generation ID
  represents the group ID maintained at the time of components insertion.
  The component deinitialization is going to be done as a groups based on the
  generation ID. This pair is assigned as a value to the my_component_registry
  map.
*/
typedef std::map<const char *, std::unique_ptr<mysql_component>, c_string_less>
    my_component_registry;

using components_vector = std::vector<mysql_component *>;
using generation_urns_list = std::forward_list<components_vector>;

/**
  A class with an implementation of the Dynamic Loader Service.
*/
class mysql_dynamic_loader_imp {
  typedef std::unordered_map<my_string,
                             my_service<SERVICE_TYPE(dynamic_loader_scheme)>>
      scheme_service_map;

  /* contain the actual fields definitions */
  static my_component_registry components_list;
  static mysql_rwlock_t LOCK_dynamic_loader;
  static generation_urns_list urns_with_gen_list;

 public:
  /**
    Initializes loader for usage. Initializes RW lock, all other structures
    should be empty. Shouldn't be called multiple times.
  */
  static void init();
  /**
    De-initializes loader. De-initializes RW lock, all other structures
    doesn't require any action.
  */
  static void deinit();

  /**
    De-initializes RW lock
  */
  static void rw_lock_deinit();

 public: /* Service Implementations */
  /**
    Loads specified group of Components by URN, initializes them and
    registers all Service Implementations present in these Components.
    Assures all dependencies will be met after loading specified Components.
    The dependencies may be circular, in such case it's necessary to specify
    all Components on cycle to load in one batch. From URNs specified the
    scheme part of URN (part before "://") is extracted and used to acquire
    Service Implementation of scheme Component loader Service for specified
    scheme.

    @param urns List of URNs of Components to load.
    @param component_count Number of Components on list to load.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */

  static DEFINE_BOOL_METHOD(load, (const char *urns[], int component_count));
  /**
    Unloads specified group of Components by URN, deinitializes them and
    unregisters all Service Implementations present in these Components.
    Assumes, thous does not check it, all dependencies of not unloaded
    Components will still be met after unloading specified Components.
    The dependencies may be circular, in such case it's necessary to specify
    all Components on cycle to unload in one batch. From URNs specified the
    scheme part of URN (part before "://") is extracted and used to acquire
    Service Implementation of scheme Component loader Service for specified
    scheme. URN specified should be identical to ones specified in load()
    method, i.e. all letters must have the same case.

    @param urns List of URNs of Components to unload.
    @param component_count Number of Components on list to unload.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */

  static DEFINE_BOOL_METHOD(unload, (const char *urns[], int component_count));

  typedef std::pair<my_component_registry::const_iterator,
                    minimal_chassis::rwlock_scoped_lock>
      component_iterator;

  /**
    Creates iterator that iterates through all loaded Components.
    If successful it leaves read lock on dynamic loader until iterator is
    released.

    @param [out] out_iterator Pointer to Component iterator handle.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static DEFINE_BOOL_METHOD(iterator_create,
                            (my_h_component_iterator * out_iterator));

  /**
    Releases Component iterator. Releases read lock on dynamic loader.

    @param iterator Component iterator handle.
  */
  static DEFINE_METHOD(void, iterator_release,
                       (my_h_component_iterator iterator));

  /**
    Gets name and URN of Service pointed to by iterator.

    @param iterator Component iterator handle.
    @param [out] out_name Pointer to string with Component name to set result
      pointer to.
    @param [out] out_urn Pointer to string with URN from which the Component was
      loaded from, to set result pointer to.
    @return Status of performed operation
    @retval false success
    @retval true Failure, may be caused when called on iterator that went
      through all values already.
  */
  static DEFINE_BOOL_METHOD(iterator_get,
                            (my_h_component_iterator iterator,
                             const char **out_name, const char **out_urn));

  /**
    Advances specified iterator to next element. Will succeed but return true if
    it reaches one-past-last element.

    @param iterator Component iterator handle.
    @return Status of performed operation and validity of iterator after
      operation.
    @retval false success
    @retval true Failure or called on iterator that was on last element.
  */
  static DEFINE_BOOL_METHOD(iterator_next, (my_h_component_iterator iterator));

  /**
    Checks if specified iterator is valid, i.e. have not reached one-past-last
    element.

    @param iterator Component iterator handle.
    @return Validity of iterator
    @retval false Valid
    @retval true Invalid or reached one-past-last element.
  */
  static DEFINE_BOOL_METHOD(iterator_is_valid,
                            (my_h_component_iterator iterator));

  /* This includes metadata-related method implementations that are shared
    by registry and dynamic_loader, so we don't duplicate the code. Following
    defines set up all required symbols. Unfortunately they are not only the
    types, but also static members with different name, so usage of templates
    is not enough to reuse that part of code. */

#define OBJECT_ITERATOR my_h_component_iterator
#define METADATA_ITERATOR my_h_component_metadata_iterator

#include "registry_metadata.h.inc"

 private:
  /**
    Loads specified group of Components by URN. From URNs specified the
    scheme part of URN (part before "://") is extracted and used to acquire
    Service Implementation of scheme Component loader Service for specified
    scheme. In case of failure rollbacks all changes, i.e. unloads loaded
    Components.

    @param urns List of URNs of Components to load.
    @param component_count Number of Components on list to load.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool load_do_load_component_by_scheme(const char *urns[],
                                               int component_count);

  /**
    Prepares a list of all Services that are provided by specified Components.
    This will enable us in next step to check if these may be used to satisfy
    other Components dependencies.

    @param loaded_components List of Components to continue load of.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool load_do_collect_services_provided(
      std::vector<std::unique_ptr<mysql_component>> &loaded_components);

  /**
    Checks if all dependencies can be satisfied with existing or to be added
    Services.

    @param loaded_components List of Components to continue load of.
    @param services_provided List of Services that are being provided by
      Components to be loaded.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool load_do_check_dependencies(
      std::vector<std::unique_ptr<mysql_component>> &loaded_components,
      const std::set<my_string> &services_provided);

  /**
    Registers all Services that are provided by specified Components.
    In case of failure rollbacks all changes, i.e. unregister registered Service
    Implementations.

    @param loaded_components List of Components to continue load of.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool load_do_register_services(
      std::vector<std::unique_ptr<mysql_component>> &loaded_components);

  /**
    Acquires Service Implementations for all dependencies of Components.
    In case of failure rollbacks all changes, i.e. release Services that were
    acquired.

    @param loaded_components List of Components to continue load of.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool load_do_resolve_dependencies(
      std::vector<std::unique_ptr<mysql_component>> &loaded_components);

  /**
    Calls Components initialization method to make Components ready to function.
    In case of failure rollbacks all changes, i.e. calls deinitialization
    methods on initialized Components.

    @param loaded_components List of Components to continue load of.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool load_do_initialize_components(
      std::vector<std::unique_ptr<mysql_component>> &loaded_components);

  /**
    Adds all Components to main list of loaded Components. Marks changes done by
    all previous steps as not to be rolled back.

    @param loaded_components List of Components to continue load of.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool load_do_commit(
      std::vector<std::unique_ptr<mysql_component>> &loaded_components);

  /**
    Unloads all Components specified in list. It does not acquire a write lock
    on dynamic loader, but requires it to be acquired by caller.

    @param urns List of URNs of Components to unload.
    @param component_count Number of Components on list to unload.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool unload_do_list_components(const char *urns[],
                                        int component_count);

  /**
    Orders components in a order that would allow allow deinitialization to be
    done always for components that have all their dependencies still not
    deinitialized. It also creates a graph of dependencies between the Service
    Implementations provided by the Components to be unloaded and Components
    that use this Service Implementation.

    @param components_to_unload List of Components to continue unload of.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool unload_do_topological_order(
      const std::vector<mysql_component *> &components_to_unload);

  /**
    Prefetch all scheme loading Services before we get a lock on a Registry.

    @param components_to_unload List of Components to continue unload of.
    @param dependency_graph A graph of dependencies between the Components
      to be unloaded.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool unload_do_get_scheme_services(
      const std::vector<mysql_component *> &components_to_unload,
      const std::map<const void *, std::vector<mysql_component *>>
          &dependency_graph);

  /**
    Takes a lock on all services that are provided by the Components to be
    unloaded, to prevent reference count from being changed.

    @param components_to_unload List of Components to continue unload of.
    @param dependency_graph A graph of dependencies between the Components
      to be unloaded.
    @param scheme_services Map of scheme loading Services prefetched with
      Service Implementations required to unload all Components to unload.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool unload_do_lock_provided_services(
      const std::vector<mysql_component *> &components_to_unload,
      const std::map<const void *, std::vector<mysql_component *>>
          &dependency_graph,
      scheme_service_map &scheme_services);

  /**
    Checks if all Service Implementations provided by the Components to be
    unloaded have no references outside the group of Components to be unloaded.
    This assures that continuing deinitialization of these Components in
    topological order, and by this also unregistration of all provided Service
    Implementations will succeed.

    @param components_to_unload List of Components to continue unload of.
    @param dependency_graph A graph of dependencies between the Components
      to be unloaded.
    @param scheme_services Map of scheme loading Services prefetched with
      Service Implementations required to unload all Components to unload.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool unload_do_check_provided_services_reference_count(
      const std::vector<mysql_component *> &components_to_unload,
      const std::map<const void *, std::vector<mysql_component *>>
          &dependency_graph,
      scheme_service_map &scheme_services);

  /**
    Deinitialize Components using their deinitialization method.
    In case of failure rollbacks all changes, i.e. calls initialization
    method again on deinitialized Components.

    @param components_to_unload List of Components to continue unload of.
    @param scheme_services Map of scheme loading Services prefetched with
      Service Implementations required to unload all Components to unload.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool unload_do_deinitialize_components(
      const std::vector<mysql_component *> &components_to_unload,
      scheme_service_map &scheme_services);

  /**
    Releases Service Implementations acquired to satisfy dependencies.
    In case of failure rollbacks all changes, i.e. acquires Services for
    released dependencies again.

    @param components_to_unload List of Components to continue unload of.
    @param scheme_services Map of scheme loading Services prefetched with
      Service Implementations required to unload all Components to unload.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool unload_do_unload_dependencies(
      const std::vector<mysql_component *> &components_to_unload,
      scheme_service_map &scheme_services);

  /**
    Unregisters all Service Implementations of specified Components.
    In case of failure rollbacks all changes, i.e. registers unregistered
    Service Implementations again.

    @param components_to_unload List of Components to continue unload of.
    @param scheme_services Map of scheme loading Services prefetched with
      Service Implementations required to unload all Components to unload.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool unload_do_unregister_services(
      const std::vector<mysql_component *> &components_to_unload,
      scheme_service_map &scheme_services);

  /**
    Uses Component URN to extract the scheme part of URN (part before "://") and
    use it to acquire Service Implementation of scheme Component loader Service
    for specified scheme, used then to unload specified Components. The unloaded
    Components are removed from the main list of all loaded Components.
    In case of failure rollbacks all changes, i.e. loads unloaded Components
    by their URN and add them to the main list of loaded Components again.

    @param components_to_unload List of Components to continue unload of.
    @param scheme_services Map of scheme loading Services prefetched with
      Service Implementations required to unload all Components to unload.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool unload_do_unload_components(
      const std::vector<mysql_component *> &components_to_unload,
      scheme_service_map &scheme_services);

  /**
    Finishes unloading process by marking changes to not be rolled back.

    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool unload_do_commit();

  /**
    Returns scheme loading Service based on given URN. It uses supplied cache to
    reuse Services. If Service is not present in cache, it will be acquired from
    registry.

    @param urn URN of Components to get scheme loader Service for.
    @param [out] out_scheme_service Pointer to store result scheme loader
      Service.
    @param [in,out] scheme_services Map of scheme loader services already
      acquired.
    @return Status of performed operation
    @retval false success
    @retval true failure
  */
  static bool get_scheme_service_from_urn(const my_string &urn,
                                          SERVICE_TYPE(dynamic_loader_scheme) *
                                              *out_scheme_service,
                                          scheme_service_map &scheme_services);
};

extern REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error);
extern my_h_service h_err_service;

#endif /* MYSQL_SERVER_DYNAMIC_LOADER_H */
