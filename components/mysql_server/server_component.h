/* Copyright (c) 2016,2017 Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef MYSQL_SERVER_COMPONENT_H
#define MYSQL_SERVER_COMPONENT_H

#include <mysql/components/service.h>
#include <mysql/components/services/registry.h>
#include <mysql/components/services/dynamic_loader.h>
#include <mysql/components/services/persistent_dynamic_loader.h>
#include <mysql/components/services/dynamic_loader_scheme_file.h>
#include <mysql/components/services/dynamic_privilege.h>

/**
  @file components/mysql_server/server_component.h

  Defines MySQL Server Component. It contains both core Components
  infrastructure and MySQL-specific Service Implementations. It is a subject for
  dividing into separate components in future.
*/

/* Service implementation initialization/deinitialization methods for all parts
  of components subsystem that requires one. */
void registry_init();
void registry_deinit();

void dynamic_loader_init();
void dynamic_loader_deinit();

bool persistent_dynamic_loader_init(void* thd);
void persistent_dynamic_loader_deinit();

void dynamic_loader_scheme_file_init();
void dynamic_loader_scheme_file_deinit();

/* implementation of the built-in components */

/**
  Bootstraps service registry and dynamic loader and make ready all basic
  server services.

  @param [out] registry A Service implementation the the Registry Service. May
    be NULL.
  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_services_bootstrap(SERVICE_TYPE(registry)** registry);

/**
  Shutdowns service registry and dynamic loader making sure all basic services
  are unregistered. Will fail if any service implementation is in use.

  @return Status of performed operation
  @retval false success
  @retval true failure
*/
bool mysql_services_shutdown();

void mysql_components_handle_std_exception(const char* funcname);

/* A declaration of registry service required for my_service<> to work. */
extern SERVICE_TYPE(registry) imp_mysql_server_registry;

#endif /* MYSQL_SERVER_COMPONENT_H */
