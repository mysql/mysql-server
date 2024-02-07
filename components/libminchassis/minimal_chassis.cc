/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>

#include <mysql/components/services/mysql_psi_system_service.h>
#include <mysql/components/services/mysql_runtime_error.h>
#include <mysql/components/services/mysql_rwlock_service.h>

#include <mysql/components/minimal_chassis.h>
#include <mysql/components/services/dynamic_loader.h>
#include "component_common.h"
#include "dynamic_loader_imp.h"
#include "dynamic_loader_scheme_file_imp.h"
#include "minimal_chassis_runtime_error_imp.h"
#include "registry_imp.h"
#include "registry_no_lock_imp.h"

extern SERVICE_TYPE(registry) imp_mysql_minimal_chassis_registry;

extern SERVICE_TYPE(mysql_rwlock_v1)
    SERVICE_IMPLEMENTATION(mysql_minimal_chassis, mysql_rwlock_v1);
REQUIRES_SERVICE_PLACEHOLDER(mysql_rwlock_v1);

extern SERVICE_TYPE(mysql_psi_system_v1)
    SERVICE_IMPLEMENTATION(mysql_minimal_chassis, mysql_psi_system_v1);
REQUIRES_SERVICE_PLACEHOLDER(mysql_psi_system_v1);

REQUIRES_SERVICE_PLACEHOLDER(mysql_runtime_error);
my_h_service h_err_service;

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis, registry)
mysql_registry_imp::acquire, mysql_registry_imp::acquire_related,
    mysql_registry_imp::release END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis_no_lock, registry)
mysql_registry_no_lock_imp::acquire,
    mysql_registry_no_lock_imp::acquire_related,
    mysql_registry_no_lock_imp::release END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis, registry_registration)
mysql_registry_imp::register_service, mysql_registry_imp::unregister,
    mysql_registry_imp::set_default END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis_no_lock,
                             registry_registration)
mysql_registry_no_lock_imp::register_service,
    mysql_registry_no_lock_imp::unregister,
    mysql_registry_no_lock_imp::set_default END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis, registry_query)
mysql_registry_imp::iterator_create, mysql_registry_imp::iterator_get,
    mysql_registry_imp::iterator_next, mysql_registry_imp::iterator_is_valid,
    mysql_registry_imp::iterator_release, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis, registry_metadata_enumerate)
mysql_registry_imp::metadata_iterator_create,
    mysql_registry_imp::metadata_iterator_get,
    mysql_registry_imp::metadata_iterator_next,
    mysql_registry_imp::metadata_iterator_is_valid,
    mysql_registry_imp::metadata_iterator_release, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis, registry_metadata_query)
mysql_registry_imp::metadata_get_value, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis, dynamic_loader)
mysql_dynamic_loader_imp::load,
    mysql_dynamic_loader_imp::unload END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis, dynamic_loader_query)
mysql_dynamic_loader_imp::iterator_create,
    mysql_dynamic_loader_imp::iterator_get,
    mysql_dynamic_loader_imp::iterator_next,
    mysql_dynamic_loader_imp::iterator_is_valid,
    mysql_dynamic_loader_imp::iterator_release END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis,
                             dynamic_loader_metadata_enumerate)
mysql_dynamic_loader_imp::metadata_iterator_create,
    mysql_dynamic_loader_imp::metadata_iterator_get,
    mysql_dynamic_loader_imp::metadata_iterator_next,
    mysql_dynamic_loader_imp::metadata_iterator_is_valid,
    mysql_dynamic_loader_imp::metadata_iterator_release
    END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis,
                             dynamic_loader_metadata_query)
mysql_dynamic_loader_imp::metadata_get_value, END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis, dynamic_loader_scheme_file)
mysql_dynamic_loader_scheme_file_imp::load,
    mysql_dynamic_loader_scheme_file_imp::unload END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(mysql_minimal_chassis, mysql_runtime_error)
mysql_runtime_error_imp::emit END_SERVICE_IMPLEMENTATION();

BEGIN_COMPONENT_PROVIDES(mysql_minimal_chassis)
PROVIDES_SERVICE(mysql_minimal_chassis, registry),
    PROVIDES_SERVICE(mysql_minimal_chassis_no_lock, registry),
    PROVIDES_SERVICE(mysql_minimal_chassis, registry_registration),
    PROVIDES_SERVICE(mysql_minimal_chassis_no_lock, registry_registration),
    PROVIDES_SERVICE(mysql_minimal_chassis, registry_query),
    PROVIDES_SERVICE(mysql_minimal_chassis, registry_metadata_enumerate),
    PROVIDES_SERVICE(mysql_minimal_chassis, registry_metadata_query),
    PROVIDES_SERVICE(mysql_minimal_chassis, dynamic_loader),
    PROVIDES_SERVICE(mysql_minimal_chassis, dynamic_loader_query),
    PROVIDES_SERVICE(mysql_minimal_chassis, dynamic_loader_metadata_enumerate),
    PROVIDES_SERVICE(mysql_minimal_chassis, dynamic_loader_metadata_query),
    PROVIDES_SERVICE(mysql_minimal_chassis, dynamic_loader_scheme_file),
    PROVIDES_SERVICE(mysql_minimal_chassis, mysql_runtime_error),
    PROVIDES_SERVICE(mysql_minimal_chassis, mysql_rwlock_v1),
    PROVIDES_SERVICE(mysql_minimal_chassis, mysql_psi_system_v1),
    END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES_WITHOUT_REGISTRY(mysql_minimal_chassis)
END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(mysql_minimal_chassis)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

DECLARE_COMPONENT(mysql_minimal_chassis, "mysql_minimal_chassis")
/* There are no initialization/deinitialization functions, they will not be
  called as this component is not a regular one. */
nullptr, nullptr END_DECLARE_COMPONENT();

/**
  This is the entry function for minimal_chassis static library, which has to be
  called by the application code.
  Bootstraps service registry and dynamic loader. And registry handle will be
  assigned, if provided empty handle address. And loads provided component
  services into the registry, if provided component reference which is
  statically linked to this library.

  @param [out] registry A service handle to registry service.
  @param [in]  comp_ref A component structure referance name.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool minimal_chassis_init(SERVICE_TYPE_NO_CONST(registry) * *registry,
                          mysql_component_t *comp_ref) {
  mysql_service_mysql_rwlock_v1 = &imp_mysql_minimal_chassis_mysql_rwlock_v1;
  mysql_service_mysql_psi_system_v1 =
      &imp_mysql_minimal_chassis_mysql_psi_system_v1;
  /* Create the registry service suite internal structure mysql_registry. */
  mysql_registry_imp::init();

  /* Seed the registry through registering the registry implementation into it,
    as well as other main bootstrap dynamic loader service implementations. */
  for (int inx = 0;
       mysql_component_mysql_minimal_chassis.provides[inx].implementation !=
       nullptr;
       ++inx) {
    if (imp_mysql_minimal_chassis_registry_registration.register_service(
            mysql_component_mysql_minimal_chassis.provides[inx].name,
            reinterpret_cast<my_h_service>(
                mysql_component_mysql_minimal_chassis.provides[inx]
                    .implementation))) {
      return true;
    }
  }

  if (registry != nullptr) {
    my_h_service registry_handle;
    if (imp_mysql_minimal_chassis_registry.acquire("registry",
                                                   &registry_handle)) {
      return true;
    }
    *registry =
        reinterpret_cast<SERVICE_TYPE_NO_CONST(registry) *>(registry_handle);
  }

  mysql_dynamic_loader_imp::init();
  imp_mysql_minimal_chassis_registry.acquire("mysql_runtime_error",
                                             &h_err_service);
  /* This service variable is used in the dynamic loader error calls.
     And this will have the minimal_chassis's mysql_runtime_error service
     implementation. This variable will be updated with mysql_server's
     component mysql_runtime_error service, when the default service
     implementation is changed. */
  mysql_service_mysql_runtime_error =
      reinterpret_cast<SERVICE_TYPE(mysql_runtime_error) *>(h_err_service);

  mysql_dynamic_loader_scheme_file_imp::init();

  if (comp_ref != nullptr) {
    for (int inx = 0; comp_ref->provides[inx].implementation != nullptr;
         ++inx) {
      if (imp_mysql_minimal_chassis_registry_registration.register_service(
              comp_ref->provides[inx].name,
              reinterpret_cast<my_h_service>(
                  comp_ref->provides[inx].implementation))) {
        return true;
      }
    }
  }
  return false;
}

/**
  This is the exit function for minimal_chassis static library, which has to be
  called just before the exit of the application.
  Releases the service registry and dynamic loader services.
  Releases the registry handle, which is acquired at the time of
  minimal_chassis_init(), if provided the handle address.
  And un-registers the component services, if provided component
  reference which is statically linked to this library.

  @param [in] registry A service handle to registry service.
  @param [in]  comp_ref A component structure referance name.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool minimal_chassis_deinit(SERVICE_TYPE_NO_CONST(registry) * registry,
                            mysql_component_t *comp_ref) {
  /* Dynamic loader deinitialization still needs all scheme service
    implementations to be functional. */
  mysql_dynamic_loader_imp::deinit();

  imp_mysql_minimal_chassis_registry.release(h_err_service);
  mysql_dynamic_loader_scheme_file_imp::deinit();

  if (comp_ref != nullptr) {
    for (int inx = 0; comp_ref->provides[inx].implementation != nullptr;
         ++inx) {
      if (imp_mysql_minimal_chassis_registry_registration.unregister(
              comp_ref->provides[inx].name)) {
        return true;
      }
    }
  }

  if (registry != nullptr) {
    imp_mysql_minimal_chassis_registry.release(
        reinterpret_cast<my_h_service>(registry));
    registry = nullptr;
  }
  for (int inx = 0;
       mysql_component_mysql_minimal_chassis.provides[inx].implementation !=
       nullptr;
       ++inx) {
    if (imp_mysql_minimal_chassis_registry_registration.unregister(
            mysql_component_mysql_minimal_chassis.provides[inx].name)) {
      return true;
    }
  }
  mysql_registry_imp::deinit();
  return false;
}

/**
  This function refreshes the global service handles based on the use_related
  flag.
  The global services are mysql_runtime_error, mysql_psi_system_v1 and
  mysql_rwlock_v1.
  If the use_related is ON then the globals are loaded with minimal chassis
  service implementations else they are loaded with the default service
  implementations

  @param use_related Used to decide which service implementation to load
         for globals.
*/
void minimal_chassis_services_refresh(bool use_related) {
  if (use_related) {
    mysql_service_mysql_rwlock_v1 = &imp_mysql_minimal_chassis_mysql_rwlock_v1;
    mysql_service_mysql_psi_system_v1 =
        &imp_mysql_minimal_chassis_mysql_psi_system_v1;
    mysql_service_mysql_runtime_error =
        reinterpret_cast<SERVICE_TYPE(mysql_runtime_error) *>(h_err_service);
  } else {
    my_service<SERVICE_TYPE(mysql_runtime_error)> error_service(
        "mysql_runtime_error", &imp_mysql_minimal_chassis_registry);
    mysql_service_mysql_runtime_error = error_service;

    my_service<SERVICE_TYPE(mysql_rwlock_v1)> rwlock_service(
        "mysql_rwlock_v1", &imp_mysql_minimal_chassis_registry);
    mysql_service_mysql_rwlock_v1 = rwlock_service;

    my_service<SERVICE_TYPE(mysql_psi_system_v1)> psi_system_service(
        "mysql_psi_system_v1", &imp_mysql_minimal_chassis_registry);
    mysql_service_mysql_psi_system_v1 = psi_system_service;
  }
}
