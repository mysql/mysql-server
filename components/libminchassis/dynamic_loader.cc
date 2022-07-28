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

#include <assert.h>
#include <mysql/components/minimal_chassis.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/dynamic_loader.h>
#include <mysql/components/services/dynamic_loader_scheme_file.h>
#include <mysql/components/services/mysql_runtime_error_service.h>
#include <mysql/components/services/registry.h>
#include <mysqld_error.h>
#include <stddef.h>
#include <algorithm>  // std::find_if
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

// pfs headers
#include <mysql/components/services/mysql_rwlock.h>

#include "component_common.h"
#include "depth_first_search.h"  // depth_first_search
#include "dynamic_loader_imp.h"
#include "mysql_component_imp.h"
#include "registry_imp.h"
#include "scope_guard.h"  // create_scope_guard

/**
  @page PAGE_COMPONENTS Component Subsystem

  The component subsystem is designed to overcome some of the architectural
  issues of the plugin subsystem, namely:
  1. Plugins can only "talk" to the server and not with other plugins
  2. Plugins have access to the server symbols and can call them directly,
    i.e. no encapsulation
  3. There's no explicit set of dependencies of a plugin, thus it's
    hard to initialize them properly
  4. Plugins require a running server to operate.

  The component infrastructure aims at allowing the MySQL server subsystems
  to be encapsulated into a set of logical components. Additional components
  may be added to a running server to extend its functionality.

  Components can be linked either dynamically or statically.

  Each component will provide implementations of an extensive set of named
  APIs, services, that other components can consume. To facilitate this
  there will be a registry of all services available to all components.

  Each component will communicate with other components only through
  services and will explicitly state the services it provides and consumes.

  The infrastructure will enable components to override and complement
  other components through re-implementing the relevant service APIs.

  Once there's a critical mass of such components exposing their functionality
  and consuming the services they need through named service APIs this will
  allow additional loadable components to take advantage of all the
  functionality they need without having to carve in specialized APIs into the
  monolithic server code for each new need.

  The infrastructure described here is mostly orthogonal to the ongoing
  activity on modularizing the MySQL Server. However, properly defined modules
  and interfaces in the Server, allows server functionality to easily be
  exposed as services where it makes sense.

  @subpage PAGE_COMPONENTS_CONCEPTS

  @section sect_components_services_inventory Service Inventory

  To the @ref group_components_services_inventory
*/

/**
  @defgroup group_components_services_inventory Component Services Inventory

  This is a group of all component service APIs

  See @ref PAGE_COMPONENTS_SERVICE for explanation of what a component service
  is
*/

/**

  @page PAGE_COMPONENTS_CONCEPTS Component Subsystem Concepts

  These are the building blocks of the component subsystem:

  - @subpage PAGE_COMPONENTS_SERVICE
  - @subpage PAGE_COMPONENTS_REGISTRY
  - @subpage PAGE_COMPONENTS_DYNAMIC_LOADER
  - @subpage PAGE_COMPONENTS_COMPONENT

  To understand how the component subsystem interacts with the
  binary it's hosted in and with the server in specific read:

  - @subpage page_components_layering
  - @subpage page_components_layering_plugins
*/

/**
  @page PAGE_COMPONENTS_DYNAMIC_LOADER The Dynamic Loader Service

  @section sect_components_dyloader_introduction Introduction

  The dynamic loader Service is one of the core Services of the MySQL
  Component subsystem. It is responsible for loading new components,
  register the services they implement, resolve dependencies and provide
  service implementations for these dependencies to the component.
  It is also responsible for unloading components, effectively reversing
  the effect of load operation in mostly reverse order, in unload
  method().

  Macros to help define components are listed in component_implementation.h.

  To use the provided services a reference to the required service
  implementation must be obtained from the registry: either explicitly via
  calling the registry service methods or implicitly by declaring it into the
  component's metadata so that the dynamic loader can satisfy that dependency
  at component load time.
  Once the service reference is no longer needed the reference to it must be
  released. The registry keeps reference counts for each service
  implementation it lists to track if a service implementation is used
  or not.

  Components are not reference counted, as they do not communicate with other
  components in any other way but via service references.
  And since service implementations are reference counted the dynamic loader
  can reliably detect if all service implementations a component provides
  are unused and can safely unload the component.

  @section sect_components_dyloader_component_urn Component URN and schemes.

  Component names are structured in such a way that allows components
  from multiple sources to be loaded. Each component is
  specified by a URN of "scheme://path", where "scheme" defines which
  dynamic loader scheme service to use to acquire the component definition.
  Single URNs can contain several components.

  A basic dynamic loader scheme service is implemented for the
  "file://" scheme (see @ref dynamic_loader_scheme_file.cc),
  which accepts a path to a dynamic library to load components
  from using the OS dynamic library APIs. This dynamic loader scheme
  implementation does not rely on any server functionality and hence can
  be placed into the component infrastructure core (a.k.a. the minimal
  chassis).

  An additional service implementation, the path filter dynamic Loader
  "file://" scheme service implementation, is provided for the "file://"
  scheme as wrapper on any already registered file scheme dynamic loader
  service implementation, that assumes a path argument is just a filename
  without extension and limits shared libraries to be loadable only from
  the MySQL server plug-in directory. The filter is implemented as a
  separate service implementation as it requires access to the server
  internal variables (the plugin directory path) and is hence implemented
  into the server component. This also demonstrates how other components
  can reimplement functionality provided by a component via setting a new
  default implementation of a service they want to re-implement.

  @section builtin_scheme The "builtin://" Scheme.

  The dynamic loader scheme service for "builtin://" was designed to
  include components that were meant to be statically linked into MySQL
  executable, but to comply with the components requirements, that is mainly
  to assure components do not interact with any other component in ways not
  meant by the components subsystem, i.e. not through the services in the
  service registry.
  This is easily asserted by housing components in separate dynamic
  libraries. This makes the "builtin://" scheme, a bit of a bad practice as
  it breaks the safeguard of having only one component in a OS binary.
  Thus currently the scheme name is reserved but no implementation of it is
  provided.

  @sa mysql_persistent_dynamic_loader_imp,
  mysql_dynamic_loader_scheme_file_path_filter_imp
*/
/**
  @page page_components_layering Component Infrastructure Layers

  @section sect_components_minimal_chassis The Minimal Chassis

  The core of the component infrastructure is the so called "minimal chassis".
  It consists of implementations of the registry and the dynamic loader.
  These are enough to bootstrap the subsystem and do not require any extra
  functionality.

  The minimal chassis can theoretically be embedded into any binary (be it
  an executable or a shared library) and can be used to provide extensibility
  to it. Hence it can even be isolated into a standalone library.

  However the minimal chassis is currently is statically linked into the
  server executable. Since the minimal chassis is logically independent it
  can (and is) initialized very early in the server bootstrap process.

  The minimal chassis lacks an impelmentation of persistent storage, i.e.
  every time it's bootstraped it will only contain the registry and the
  dynamic loader services. While this is a good basis it lacks a central
  functionality expected from a mysql server extension system: keep a
  persisted list of extensions that are to be loaded automatically into
  the infrastructure in an orderly way to provide the functionality
  contained in them to server users.

  Hence an extra logical layer is added on top of the minimal chassis:

  The minimal chassis library has three exposed apis those are:
  - minimal_chassis_init()
  - minimal_chassis_deinit()
  - minimal_chassis_services_refresh()
  (see @ref minimal_chassis.cc)

  Note: minimal chassis support the native implementation of rwlocks and
  trylocks and it doesn't support the prlock because the prlock native
  implementation depends on mysql server code.

  @section sect_components_layering_server_component The Server Component

  The server component is currently the whole of the server binary code.
  It's not loaded dynamically (as any ordinary component is) since it's
  embedded into the same OS binary as the minimal chassis: the mysql server
  executable binary.
  As any other component the server component too can expose any of the
  functionality it encapsulates via the service implementations it provides to
  the other components in the component infrastructure.

  The server component is initialized relatively late in the bootstrap process:
  when all of the server is initialized and it's ready to "go" (i.e. accept
  client connections). At that time the component persistency table is read
  and all of the component sets defined in it are loaded and initialized.

  Note that, albeit being in the same binary (component) the server
  component and the minimal chassis are and should remain two distinct
  layers. Mostly to allow safe reuse of the minimal chassis in other
  binaries.

  The elements of the minimal chassis are implemented in
  components/libminchassis and the implementations of services the
  server component provides are to be found in sql/server_component/
*/

/**
  @page page_components_layering_plugins Components and Plugins

  An important question to answer is what is the relationship between
  components and plugins. To the end user these both look the same: a way
  to dynamically extend the functionality of the server.
  And that is a sought-after resemblance.

  But architecturally the two are very distinct.

  Components are self-contained code containers that interact with
  other code exclusively by implementing and consuming
  services via the registry.

  Plugins do implement plugin APIs and may choose to call plugin service
  APIs exposed by the server. But in reality they have access to all of
  the server binary global symbols. So most if not all plugins choose to
  use these instead of confining themselves into the (admittedly limited
  set of) plugin APIs available.

  The above makes plugins an integral part of the server codebase, more
  specifically of the server component.

  @attention Plugins are dynamically loadable bits of the
  @ref sect_components_layering_server_component
*/

/**
  This place holder is required for the mysql_runtime_error service.
  The service is used in mysql_error_service_printf() api, which is the
  replacement for my_error() server api.
*/
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error);

extern SERVICE_TYPE(registry) imp_mysql_minimal_chassis_registry;

struct my_h_component_iterator_imp {
  my_component_registry::const_iterator m_it;
  minimal_chassis::rwlock_scoped_lock m_lock;
};

struct my_h_component_metadata_iterator_imp {
  my_metadata::const_iterator m_it;
  minimal_chassis::rwlock_scoped_lock m_lock;
};

static PSI_rwlock_key key_rwlock_LOCK_dynamic_loader;

static PSI_rwlock_info all_dynamic_loader_rwlocks[] = {
    {&key_rwlock_LOCK_dynamic_loader, "LOCK_dynamic_loader", PSI_FLAG_SINGLETON,
     0, PSI_DOCUMENT_ME}};

static void init_dynamic_loader_psi_keys(void) {
  const char *category = "components";
  int count;

  count = static_cast<int>(array_elements(all_dynamic_loader_rwlocks));
  mysql_rwlock_register(category, all_dynamic_loader_rwlocks, count);
}

/**
  Initializes loader for usage. Initializes RW lock, all other structures
  should be empty. Shouldn't be called multiple times.
*/
void mysql_dynamic_loader_imp::init() {
  init_dynamic_loader_psi_keys();
  mysql_rwlock_init(key_rwlock_LOCK_dynamic_loader,
                    &mysql_dynamic_loader_imp::LOCK_dynamic_loader);
}

/**
  De-initializes loader. De-initializes RW lock, all other structures
  doesn't require any action.
*/
void mysql_dynamic_loader_imp::deinit() {
  /* Leave scope and lock before destroying it. */
  {
    minimal_chassis::rwlock_scoped_lock lock(
        &mysql_dynamic_loader_imp::LOCK_dynamic_loader, true, __FILE__,
        __LINE__);

    /* Unload all Components that are loaded. All Components are unloaded in
      generation group wise to prevent any problems with dependencies. This on
      the other side may lead to situation where one of Components will not
      unload properly for a perticular generation group, causing all other
      components of that group to not be unloaded, leaving them all still
      loaded in and not deinitialized. There should be an error message issued
      stating a problem during unload to help detect such a problem. */
    if (mysql_dynamic_loader_imp::components_list.size() > 0) {
      for (auto it = mysql_dynamic_loader_imp::urns_with_gen_list.begin();
           it != mysql_dynamic_loader_imp::urns_with_gen_list.end();) {
        /* If we pass (*it) vector, unload_do_unload_components() function
           will iterates and modify urns_with_gen_list's vector. And both
           iterator and modify will work on same vector, which leads to
           vector iterator invalidation. So, we need to make a copy
           of the vector and pass it to below function */
        auto components_to_unload = (*it);
        if (unload_do_topological_order(components_to_unload)) {
          /* since there is a error in the deinit function, report
             the error and clear the vector elements(i.e components in that
             group and then remove the component group node */
          it->clear();
          // removes the forward_list node
          mysql_dynamic_loader_imp::urns_with_gen_list.remove(*it);
        }
        /* Updating the iterator because unload_do_unload_components
          removes the mysql_dynamic_loader_imp::urns_with_gen_list node */
        it = mysql_dynamic_loader_imp::urns_with_gen_list.begin();
      }
    }
  }
  mysql_rwlock_destroy(&mysql_dynamic_loader_imp::LOCK_dynamic_loader);
}

/**
  De-initializes RW lock
*/
void mysql_dynamic_loader_imp::rw_lock_deinit() {
  mysql_rwlock_destroy(&mysql_dynamic_loader_imp::LOCK_dynamic_loader);
}

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
DEFINE_BOOL_METHOD(mysql_dynamic_loader_imp::load,
                   (const char *urns[], int component_count)) {
  try {
    /* Acquire write lock for entire process, possibly this could be
      optimized, but must be done with care. */
    minimal_chassis::rwlock_scoped_lock lock(
        &mysql_dynamic_loader_imp::LOCK_dynamic_loader, true, __FILE__,
        __LINE__);

    /* This method calls a chain of methods to perform load operation.
      Each element in chain performs specific part of process, a stage, is
      named upon what stage it performs, and is responsible of calling the
      next element in the chain and performing rollback of changes it
      performed, in case the whole operation do not succeed. The following
      methods are called, in order:
      - load_do_load_component_by_scheme
      - load_do_collect_services_provided
      - load_do_check_dependencies
      - load_do_register_services
      - load_do_resolve_dependencies
      - load_do_initialize_components
      - load_do_commit */
    return mysql_dynamic_loader_imp::load_do_load_component_by_scheme(
        urns, component_count);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

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
DEFINE_BOOL_METHOD(mysql_dynamic_loader_imp::unload,
                   (const char *urns[], int component_count)) {
  try {
    minimal_chassis::rwlock_scoped_lock lock(
        &mysql_dynamic_loader_imp::LOCK_dynamic_loader, true, __FILE__,
        __LINE__);

    /* This method calls a chain of methods to perform unload operation.
      Each element in chain performs specific part of process, a stage, is
      named upon what stage it performs, and is responsible of calling the
      next element in the chain and performing rollback of changes it
      performed, in case the whole operation do not succeed. The following
      methods are called, in order:
      - unload_do_list_components
      - unload_do_topological_order
      - unload_do_get_scheme_services
      - unload_do_lock_provided_services
      - unload_do_check_provided_services_reference_count
      - unload_do_deinitialize_components
      - unload_do_unload_dependencies
      - unload_do_unregister_services
      - unload_do_unload_components
      - unload_do_commit */
    return mysql_dynamic_loader_imp::unload_do_list_components(urns,
                                                               component_count);
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

/**
  Creates iterator that iterates through all loaded Components.
  If successful it leaves read lock on dynamic loader until iterator is
  released.

  @param [out] out_iterator Pointer to Component iterator handle.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
DEFINE_BOOL_METHOD(mysql_dynamic_loader_imp::iterator_create,
                   (my_h_component_iterator * out_iterator)) {
  try {
    *out_iterator = nullptr;

    /* This read lock on whole component registry will be held, until the
      iterator is released. */
    minimal_chassis::rwlock_scoped_lock lock(
        &mysql_dynamic_loader_imp::LOCK_dynamic_loader, false, __FILE__,
        __LINE__);

    my_component_registry::const_iterator r =
        mysql_dynamic_loader_imp::components_list.cbegin();

    if (r == mysql_dynamic_loader_imp::components_list.cend()) {
      return true;
    }

    *out_iterator = new my_h_component_iterator_imp{r, std::move(lock)};
    return false;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

DEFINE_METHOD(void, mysql_dynamic_loader_imp::iterator_release,
              (my_h_component_iterator iterator)) {
  try {
    my_h_component_iterator_imp *iter =
        reinterpret_cast<my_h_component_iterator_imp *>(iterator);

    if (!iter) return;

    delete iter;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
}

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
DEFINE_BOOL_METHOD(mysql_dynamic_loader_imp::iterator_get,
                   (my_h_component_iterator iterator, const char **out_name,
                    const char **out_urn)) {
  try {
    *out_name = nullptr;
    *out_urn = nullptr;

    if (!iterator) return true;

    my_component_registry::const_iterator &iter =
        reinterpret_cast<my_h_component_iterator_imp *>(iterator)->m_it;

    if (iter != mysql_dynamic_loader_imp::components_list.cend()) {
      mysql_component *imp = iter->second.get();
      *out_name = imp->name_c_str();
      *out_urn = imp->urn_c_str();

      return false;
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

/**
  Advances specified iterator to next element. Will succeed but return true if
  it reaches one-past-last element.

  @param iterator Component iterator handle.
  @return Status of performed operation and validity of iterator after
    operation.
  @retval false success
  @retval true Failure or called on iterator that was on last element.
*/
DEFINE_BOOL_METHOD(mysql_dynamic_loader_imp::iterator_next,
                   (my_h_component_iterator iterator)) {
  try {
    if (!iterator) return true;

    my_component_registry::const_iterator &iter =
        reinterpret_cast<my_h_component_iterator_imp *>(iterator)->m_it;

    if (iter != mysql_dynamic_loader_imp::components_list.cend()) {
      ++iter;
      return iter == mysql_dynamic_loader_imp::components_list.cend();
    }
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

/**
  Checks if specified iterator is valid, i.e. have not reached one-past-last
  element.

  @param iterator Component iterator handle.
  @return Validity of iterator
  @retval false Valid
  @retval true Invalid or reached one-past-last element.
*/
DEFINE_BOOL_METHOD(mysql_dynamic_loader_imp::iterator_is_valid,
                   (my_h_component_iterator iterator)) {
  try {
    if (!iterator) return true;

    my_component_registry::const_iterator &iter =
        reinterpret_cast<my_h_component_iterator_imp *>(iterator)->m_it;

    return iter == mysql_dynamic_loader_imp::components_list.cend();
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

/* This includes metadata-related method implementations that are shared
  by registry and dynamic_loader, so we don't duplicate the code. Following
  defines set up all required symbols. Unfortunately they are not only the
  types, but also static members with different name, so usage of templates
  is not enough to reuse that part of code. */
#define REGISTRY_IMP mysql_dynamic_loader_imp
#define REGISTRY mysql_dynamic_loader_imp::components_list
#define REGISTRY_TYPE my_component_registry
#define LOCK mysql_dynamic_loader_imp::LOCK_dynamic_loader
#define ITERATOR_TYPE my_h_component_iterator_imp
#define METADATA_ITERATOR_TYPE my_h_component_metadata_iterator_imp
#define OBJECT_ITERATOR my_h_component_iterator
#define METADATA_ITERATOR my_h_component_metadata_iterator

#include "registry_metadata.cc.inc"

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
bool mysql_dynamic_loader_imp::load_do_load_component_by_scheme(
    const char *urns[], int component_count) {
  scheme_service_map scheme_services;

  /* List of components that were just loaded by scheme loader services. */
  std::vector<std::unique_ptr<mysql_component>> loaded_components;

  auto guard = create_scope_guard([&loaded_components, &scheme_services]() {
    /* In case we need to rollback we need to undo what next for loop does,
      i.e. unload all components using assigned scheme loader. */
    for (std::unique_ptr<mysql_component> &loaded_component :
         loaded_components) {
      const my_string &urn = loaded_component->get_urn();

      SERVICE_TYPE(dynamic_loader_scheme) * scheme_service;
      /* We don't want scheme_services to be in the same scope as usages
        of this deleter are. Right now these are in try{} scope, and it is
        OK. */
      if (mysql_dynamic_loader_imp::get_scheme_service_from_urn(
              urn, &scheme_service, scheme_services)) {
        return;
      }
      scheme_service->unload(urn.c_str());
    }
  });

  /* First load all components. */
  for (int it = 0; it < component_count; ++it) {
    my_string urn(urns[it]);

    SERVICE_TYPE(dynamic_loader_scheme) * scheme_service;

    /* Try to get service responsible for handling specified scheme type. */
    if (mysql_dynamic_loader_imp::get_scheme_service_from_urn(
            urn, &scheme_service, scheme_services)) {
      return true;
    }

    /* Load component using scheme service. The result is pointer to NULL
      terminated list of components. */
    mysql_component_t *loaded_component_raw;
    if (scheme_service->load(urn.c_str(), &loaded_component_raw)) {
      mysql_error_service_printf(ER_COMPONENTS_CANT_LOAD, MYF(0), urn.c_str());
      return true;
    }
    /* Here we assume loaded_component_raw will be list with only one item. */
    loaded_components.push_back(std::unique_ptr<mysql_component>(
        new mysql_component(loaded_component_raw, urn)));
  }

  bool res = mysql_dynamic_loader_imp::load_do_collect_services_provided(
      loaded_components);
  if (!res) {
    guard.commit();
  }
  return res;
}

/**
  Prepares a list of all Services that are provided by specified Components.
  This will enable us in next step to check if these may be used to satisfy
  other Components dependencies.

  @param loaded_components List of Components to continue load of.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_dynamic_loader_imp::load_do_collect_services_provided(
    std::vector<std::unique_ptr<mysql_component>> &loaded_components) {
  /* Set of names of services without implementation names that
    specified components provide. */
  std::set<my_string> services_provided;

  /* Add all services this component provides to list of provided services. */
  for (const std::unique_ptr<mysql_component> &loaded_component :
       loaded_components) {
    for (const mysql_service_ref_t *service_provided :
         loaded_component->get_provided_services()) {
      const char *dot_position = strchr(service_provided->name, '.');
      if (dot_position == nullptr) {
        services_provided.insert(my_string(service_provided->name));
      } else {
        /* Insert only part before the dot, which is only the service name,
          without implementation name. */
        services_provided.insert(my_string(
            service_provided->name, dot_position - service_provided->name));
      }
    }
  }

  return mysql_dynamic_loader_imp::load_do_check_dependencies(
      loaded_components, services_provided);
}

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
bool mysql_dynamic_loader_imp::load_do_check_dependencies(
    std::vector<std::unique_ptr<mysql_component>> &loaded_components,
    const std::set<my_string> &services_provided) {
  /* Check all dependencies can be met with services already registered in
    registry or from components that are being loaded. */
  for (const std::unique_ptr<mysql_component> &loaded_component :
       loaded_components) {
    for (const mysql_service_placeholder_ref_t *service_required :
         loaded_component->get_required_services()) {
      /* Try lookup in services provided by other components being loaded. */
      if (services_provided.find(my_string(service_required->name)) !=
          services_provided.end()) {
        continue;
      }

      /* Try to lookup in services registered in registry service */
      my_h_service_iterator service_iterator;
      if (!mysql_registry_imp::iterator_create(service_required->name,
                                               &service_iterator)) {
        mysql_registry_imp::iterator_release(service_iterator);
        continue;
      }

      /* None service matches requirement, we shall fail. */
      mysql_error_service_printf(ER_COMPONENTS_CANT_SATISFY_DEPENDENCY, MYF(0),
                                 service_required->name,
                                 loaded_component->name_c_str());
      return true;
    }
  }

  return mysql_dynamic_loader_imp::load_do_register_services(loaded_components);
}

/**
  Registers all Services that are provided by specified Components.
  In case of failure rollbacks all changes, i.e. unregister registered Service
  Implementations.

  @param loaded_components List of Components to continue load of.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_dynamic_loader_imp::load_do_register_services(
    std::vector<std::unique_ptr<mysql_component>> &loaded_components) {
  /* List of services from components that were registered. */
  std::vector<const char *> registered_services;
  auto guard = create_scope_guard([&registered_services]() {
    for (const char *service_name : registered_services) {
      mysql_registry_imp::unregister(service_name);
    }
  });

  /* Register services from components. */
  for (const std::unique_ptr<mysql_component> &loaded_component :
       loaded_components) {
    /* Register all services from component. */
    for (const mysql_service_ref_t *implementation_it :
         loaded_component->get_provided_services()) {
      if (mysql_registry_imp::register_service(
              implementation_it->name,
              reinterpret_cast<my_h_service>(
                  implementation_it->implementation))) {
        mysql_error_service_printf(
            ER_COMPONENTS_LOAD_CANT_REGISTER_SERVICE_IMPLEMENTATION, MYF(0),
            implementation_it->name, loaded_component->name_c_str());
        return true;
      }
      registered_services.push_back(implementation_it->name);
    }
  }

  bool res =
      mysql_dynamic_loader_imp::load_do_resolve_dependencies(loaded_components);
  if (!res) {
    guard.commit();
  }
  return res;
}

/**
  Acquires Service Implementations for all dependencies of Components.
  In case of failure rollbacks all changes, i.e. release Services that were
  acquired.

  @param loaded_components List of Components to continue load of.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_dynamic_loader_imp::load_do_resolve_dependencies(
    std::vector<std::unique_ptr<mysql_component>> &loaded_components) {
  /* List of services acquired for component dependencies. */
  std::vector<my_h_service *> acquired_services;
  auto guard = create_scope_guard([&acquired_services]() {
    for (my_h_service *service_storage : acquired_services) {
      mysql_registry_imp::release(*service_storage);
      *service_storage = nullptr;
    }
  });

  /* Acquire services to meet component dependencies. */
  for (const std::unique_ptr<mysql_component> &loaded_component :
       loaded_components) {
    /* Meet all dependencies for all components. */
    for (mysql_service_placeholder_ref_t *implementation_it :
         loaded_component->get_required_services()) {
      if (mysql_registry_imp::acquire(implementation_it->name,
                                      reinterpret_cast<my_h_service *>(
                                          implementation_it->implementation))) {
        mysql_error_service_printf(
            ER_COMPONENTS_CANT_ACQUIRE_SERVICE_IMPLEMENTATION, MYF(0),
            implementation_it->name);
        return true;
      }
      acquired_services.push_back(
          reinterpret_cast<my_h_service *>(implementation_it->implementation));
    }
  }

  bool res = mysql_dynamic_loader_imp::load_do_initialize_components(
      loaded_components);
  if (!res) {
    guard.commit();
  }
  return res;
}

/**
  Calls Components initialization method to make Components ready to function.
  In case of failure rollbacks all changes, i.e. calls deinitialization
  methods on initialized Components.

  @param loaded_components List of Components to continue load of.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_dynamic_loader_imp::load_do_initialize_components(
    std::vector<std::unique_ptr<mysql_component>> &loaded_components) {
  /* List of components that were initialized. */
  std::vector<mysql_component *> initialized_components;

  auto guard = create_scope_guard([&initialized_components]() {
    for (mysql_component *initialized_component : initialized_components) {
      if (initialized_component->get_data()->deinit != nullptr) {
        initialized_component->get_data()->deinit();
      }
    }
  });

  /* Initialize components. */
  for (const std::unique_ptr<mysql_component> &loaded_component :
       loaded_components) {
    /* Initialize component, move to main collection of components,
      add to temporary list of components registered, in case we need to
      unregister them on failure. */
    if (loaded_component->get_data()->init != nullptr &&
        loaded_component->get_data()->init()) {
      mysql_error_service_printf(ER_COMPONENTS_LOAD_CANT_INITIALIZE, MYF(0),
                                 loaded_component->name_c_str());
      return true;
    }

    initialized_components.push_back(loaded_component.get());
  }

  bool res = mysql_dynamic_loader_imp::load_do_commit(loaded_components);
  if (!res) {
    guard.commit();
  }
  return res;
}

/**
  Adds all Components to main list of loaded Components. Marks changes done by
  all previous steps as not to be rolled back.

  @param loaded_components List of Components to continue load of.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_dynamic_loader_imp::load_do_commit(
    std::vector<std::unique_ptr<mysql_component>> &loaded_components) {
  std::vector<mysql_component *> components_to_load;
  /* Move components to the main list of components loaded. */
  for (std::unique_ptr<mysql_component> &loaded_component : loaded_components) {
    components_to_load.emplace_back(loaded_component.get());
    mysql_dynamic_loader_imp::components_list.emplace(
        loaded_component->urn_c_str(), std::move(loaded_component));
  }
  mysql_dynamic_loader_imp::urns_with_gen_list.emplace_front(
      components_to_load);

  return false;
}

/**
  Unloads all Components specified in list. It does not acquire a write lock
  on dynamic loader, but requires it to be acquired by caller.

  @param urns List of URNs of Components to unload.
  @param component_count Number of Components on list to unload.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_dynamic_loader_imp::unload_do_list_components(const char *urns[],
                                                         int component_count) {
  std::vector<mysql_component *> components_to_unload;
  /* Finds any duplicated entries in the group specified. */
  std::set<mysql_component *> components_in_group;

  /* Lookup for components by URNs specified. */
  for (int it = 0; it < component_count; ++it) {
    my_string urn = my_string(urns[it]);
    my_component_registry::iterator component_it =
        mysql_dynamic_loader_imp::components_list.find(urn.c_str());
    /* Return error if any component is not loaded. */
    if (component_it == mysql_dynamic_loader_imp::components_list.end()) {
      mysql_error_service_printf(ER_COMPONENTS_UNLOAD_NOT_LOADED, MYF(0),
                                 urn.c_str());
      return true;
    }
    components_to_unload.push_back(component_it->second.get());
    if (!components_in_group.insert(component_it->second.get()).second) {
      mysql_error_service_printf(ER_COMPONENTS_UNLOAD_DUPLICATE_IN_GROUP,
                                 MYF(0), urn.c_str());
      return true;
    }
  }

  return mysql_dynamic_loader_imp::unload_do_topological_order(
      components_to_unload);
}

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
bool mysql_dynamic_loader_imp::unload_do_topological_order(
    const std::vector<mysql_component *> &components_to_unload) {
  /* A graph of dependencies between the Components to be unloaded. For each
    Service Implementation that is provided by any of the Components to be
    unloaded a list of other Components to be unloaded is specified, which all
    have an actual dependency resolved to that particular Service
    Implementation. */
  std::map<const void *, std::vector<mysql_component *>> dependency_graph;

  /* First list all Service Implementations that are provided by the Components
    to be unloaded.*/
  for (mysql_component *component : components_to_unload) {
    for (const mysql_service_ref_t *service :
         component->get_provided_services()) {
      dependency_graph.emplace(service->implementation,
                               std::vector<mysql_component *>{});
    }
  }
  /* Iterate through all dependencies of the Components to be unloaded to check
    which were resolved using Service Implementations provided by any of listed
    above. */
  for (mysql_component *component : components_to_unload) {
    for (const mysql_service_placeholder_ref_t *service :
         component->get_required_services()) {
      std::map<const void *, std::vector<mysql_component *>>::iterator it =
          dependency_graph.find(*service->implementation);
      if (it != dependency_graph.end()) {
        it->second.push_back(component);
      }
    }
  }

  /* Iterate through all components to get the topological order of
    the Components to be unloaded. */
  std::vector<mysql_component *> components_to_unload_ordered;
  /* Set of Components that were already visited by the DFS, to keep state
    between calls to the DFS. */
  std::set<mysql_component *> visited_set;

  for (mysql_component *component : components_to_unload) {
    /* A DFS run on directional graph, possibly a cyclic-one, with
      V=(list of the Components to unload) and
      E={A->B : component B depends on component A}. A DFS post-order will
      result in a topological ordered list of components. */
    depth_first_search(
        component, [](mysql_component *) {},
        [&components_to_unload_ordered](mysql_component *visited_component) {
          components_to_unload_ordered.push_back(visited_component);
        },
        [&dependency_graph](mysql_component *visited_component) {
          /* List of neighbors, i.e. all components that are dependent on
            currently visited one. */
          std::vector<mysql_component *> component_dependencies;

          /* Iterate though all provided services to see if any other Component
            to be unloaded is not depending on that Service Implementation. */
          for (const mysql_service_ref_t *service :
               visited_component->get_provided_services()) {
            for (mysql_component *dependent_component :
                 dependency_graph[service->implementation]) {
              component_dependencies.push_back(dependent_component);
            }
          }

          return component_dependencies;
        },
        visited_set);
  }

  assert(components_to_unload.size() == components_to_unload_ordered.size());

  return mysql_dynamic_loader_imp::unload_do_get_scheme_services(
      components_to_unload_ordered, dependency_graph);
}

/**
  Prefetch all scheme loading Services before we get a lock on a Registry.

  @param components_to_unload List of Components to continue unload of.
  @param dependency_graph A graph of dependencies between the Components
    to be unloaded.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_dynamic_loader_imp::unload_do_get_scheme_services(
    const std::vector<mysql_component *> &components_to_unload,
    const std::map<const void *, std::vector<mysql_component *>>
        &dependency_graph) {
  scheme_service_map scheme_services;

  for (mysql_component *component : components_to_unload) {
    SERVICE_TYPE(dynamic_loader_scheme) * scheme_service;

    /* Try to get service responsible for handling specified scheme type. */
    if (mysql_dynamic_loader_imp::get_scheme_service_from_urn(
            my_string(component->urn_c_str()), &scheme_service,
            scheme_services)) {
      return true;
    }
  }

  return mysql_dynamic_loader_imp::unload_do_lock_provided_services(
      components_to_unload, dependency_graph, scheme_services);
}

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
bool mysql_dynamic_loader_imp::unload_do_lock_provided_services(
    const std::vector<mysql_component *> &components_to_unload,
    const std::map<const void *, std::vector<mysql_component *>>
        &dependency_graph,
    scheme_service_map &scheme_services) {
  /* We do lock the whole registry, as we don't have yet any better granulation.
   */
  minimal_chassis::rwlock_scoped_lock lock =
      mysql_registry_imp::lock_registry_for_write();
  return mysql_dynamic_loader_imp::
      unload_do_check_provided_services_reference_count(
          components_to_unload, dependency_graph, scheme_services);
}

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
bool mysql_dynamic_loader_imp::
    unload_do_check_provided_services_reference_count(
        const std::vector<mysql_component *> &components_to_unload,
        const std::map<const void *, std::vector<mysql_component *>>
            &dependency_graph,
        scheme_service_map &scheme_services) {
  /* Iterate through all Service Implementations that are provided by the
    Components to be unloaded to see if all have provided Service
    Implementations that are not used by Components outside the group of
    Components to unload.*/
  for (mysql_component *component : components_to_unload) {
    for (const mysql_service_ref_t *service :
         component->get_provided_services()) {
      uint64_t reference_count =
          mysql_registry_imp::get_service_implementation_reference_count(
              reinterpret_cast<my_h_service>(service->implementation));
      if (reference_count > 0) {
        std::map<const void *, std::vector<mysql_component *>>::const_iterator
            it = dependency_graph.find(service->implementation);
        if (it == dependency_graph.end() ||
            reference_count != it->second.size()) {
          mysql_error_service_printf(
              ER_COMPONENTS_UNLOAD_CANT_UNREGISTER_SERVICE, MYF(0),
              service->name, component->name_c_str());
          return true;
        }
      }
    }
  }
  return mysql_dynamic_loader_imp::unload_do_deinitialize_components(
      components_to_unload, scheme_services);
}

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
bool mysql_dynamic_loader_imp::unload_do_deinitialize_components(
    const std::vector<mysql_component *> &components_to_unload,
    scheme_service_map &scheme_services) {
  bool deinit_result = false;
  /* Release all Services that are used as dependencies, as there can be
    references to Services provided by other components to be unloaded. */
  for (mysql_component *component : components_to_unload) {
    if (component->get_data()->deinit != nullptr) {
      if (component->get_data()->deinit()) {
        /* In case of error we don't want to try to restore consistent state.
          This is arbitrary decision, rollback of this operation is possible,
          but it's not sure if components will be able to initialize again
          properly, causing state to be inconsistent. */
        mysql_error_service_printf(ER_COMPONENTS_UNLOAD_CANT_DEINITIALIZE,
                                   MYF(0), component->name_c_str());
        deinit_result = true;
      }
    }
  }

  return deinit_result ||
         mysql_dynamic_loader_imp::unload_do_unload_dependencies(
             components_to_unload, scheme_services);
}

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
bool mysql_dynamic_loader_imp::unload_do_unload_dependencies(
    const std::vector<mysql_component *> &components_to_unload,
    scheme_service_map &scheme_services) {
  bool unload_depends_result = false;
  /* Release all services that are used as dependencies, as there can be
    references to services provided by other components to be unloaded. */
  for (mysql_component *component : components_to_unload) {
    for (mysql_service_placeholder_ref_t *service_dependency :
         component->get_required_services()) {
      if (mysql_registry_imp::release_nolock(reinterpret_cast<my_h_service>(
              *service_dependency->implementation))) {
        /* In case of error we don't want to try to restore consistent state.
          This is arbitrary decision, rollback of this operation is possible,
          but it's not sure if components will be able to initialize again
          properly, causing state to be inconsistent. */
        mysql_error_service_printf(ER_COMPONENTS_CANT_RELEASE_SERVICE, MYF(0));
        unload_depends_result = true;
      }
    }
  }

  return mysql_dynamic_loader_imp::unload_do_unregister_services(
             components_to_unload, scheme_services) ||
         unload_depends_result;
}

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
bool mysql_dynamic_loader_imp::unload_do_unregister_services(
    const std::vector<mysql_component *> &components_to_unload,
    scheme_service_map &scheme_services) {
  bool unregister_result = false;
  /* Unregister all services that are provided by these components. */
  for (mysql_component *component : components_to_unload) {
    for (const mysql_service_ref_t *service_provided :
         component->get_provided_services()) {
      if (mysql_registry_imp::unregister_nolock(service_provided->name)) {
        /* In case of error we don't want to try to restore consistent state.
          This is arbitrary decision, rollback of this operation is possible,
          but it's not sure if components will be able to initialize again
          properly, causing state to be inconsistent. */
        mysql_error_service_printf(ER_COMPONENTS_UNLOAD_CANT_UNREGISTER_SERVICE,
                                   MYF(0), service_provided->name,
                                   component->name_c_str());
        unregister_result = true;
      }
    }
  }

  return mysql_dynamic_loader_imp::unload_do_unload_components(
             components_to_unload, scheme_services) ||
         unregister_result;
}

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
bool mysql_dynamic_loader_imp::unload_do_unload_components(
    const std::vector<mysql_component *> &components_to_unload,
    scheme_service_map &scheme_services) {
  bool unload_result = false;
  /* Unload components and remove them from main dictionary. */
  for (mysql_component *component : components_to_unload) {
    SERVICE_TYPE(dynamic_loader_scheme) * scheme_service;

    /* Try to get service responsible for handling specified scheme type. */
    if (mysql_dynamic_loader_imp::get_scheme_service_from_urn(
            my_string(component->urn_c_str()), &scheme_service,
            scheme_services)) {
      /* In case of error we don't want to try to restore consistent state.
        This is arbitrary decision, rollback of this operation is possible,
        but it's not sure if components will be able to initialize again
        properly, causing state to be inconsistent. */
      unload_result = true;
      continue;
    }

    my_string component_urn = my_string(component->urn_c_str());

    auto component_it =
        mysql_dynamic_loader_imp::components_list.find(component_urn.c_str());

    /* Find mysql_component pointer from the urns_with_gen_list's vector
       elements, if found then remove it from the vector and after removing, if
       the vector doesn't have no elements then remove the vector node from the
       forward_list and in either case after element is removed, break the loop.
       Note: urns_with_gen_list will contain the unique vector elements of
       mysql_component pointers. */
    for (auto list = mysql_dynamic_loader_imp::urns_with_gen_list.begin();
         list != mysql_dynamic_loader_imp::urns_with_gen_list.end(); ++list) {
      auto comp_ele = (*component_it).second.get();
      auto v_element =
          find_if((*list).begin(), (*list).end(),
                  [comp_ele](mysql_component *ptr) { return ptr == comp_ele; });
      if (v_element != list->end()) {
        list->erase(v_element);
        if (list->size() == 0) {
          mysql_dynamic_loader_imp::urns_with_gen_list.remove(*list);
        }
        break;
      }
    }

    mysql_dynamic_loader_imp::components_list.erase(component_it);

    if (scheme_service->unload(component_urn.c_str())) {
      /* In case of error we don't want to try to restore consistent state.
        This is arbitrary decision, rollback of this operation is possible,
        but it's not sure if components will be able to initialize again
        properly, causing state to be inconsistent. */
      mysql_error_service_printf(ER_COMPONENTS_CANT_UNLOAD, MYF(0),
                                 component_urn.c_str());
      unload_result = true;
    }
  }

  return mysql_dynamic_loader_imp::unload_do_commit() || unload_result;
}

/**
  Finishes unloading process by marking changes to not be rolled back.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_dynamic_loader_imp::unload_do_commit() {
  /* All components were successfully unloaded, we commit changes by returning
    no error. */
  return false;
}

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
bool mysql_dynamic_loader_imp::get_scheme_service_from_urn(
    const my_string &urn,
    SERVICE_TYPE(dynamic_loader_scheme) * *out_scheme_service,
    scheme_service_map &scheme_services) {
  /* Find scheme prefix. */
  size_t scheme_end = urn.find("://");
  if (scheme_end == my_string::npos) {
    mysql_error_service_printf(ER_COMPONENTS_NO_SCHEME, MYF(0), urn.c_str());
    return true;
  }
  my_string scheme(urn.begin(), urn.begin() + scheme_end, urn.get_allocator());

  /* Look for scheme loading service in cache. */
  scheme_service_map::iterator scheme_it = scheme_services.find(scheme);
  if (scheme_it != scheme_services.end()) {
    *out_scheme_service = scheme_it->second;
  } else {
    /* If not present, acquire from registry service and insert to cache. */
    my_service<SERVICE_TYPE(dynamic_loader_scheme)> service(
        (my_string("dynamic_loader_scheme_") + scheme).c_str(),
        &imp_mysql_minimal_chassis_registry);

    if (service) {
      mysql_error_service_printf(ER_COMPONENTS_NO_SCHEME_SERVICE, MYF(0),
                                 scheme.c_str(), urn.c_str());
      return true;
    }
    *out_scheme_service = service;
    scheme_services.insert(make_pair(scheme, std::move(service)));
  }

  return false;
}

/* static members for mysql_dynamic_loader_imp */
my_component_registry mysql_dynamic_loader_imp::components_list;
mysql_rwlock_t mysql_dynamic_loader_imp::LOCK_dynamic_loader;
generation_urns_list mysql_dynamic_loader_imp::urns_with_gen_list;
