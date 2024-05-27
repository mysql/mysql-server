/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_COMPONENTS_SERVICES_DYNAMIC_LOADER_SERVICE_NOTIFICATION_H
#define MYSQL_COMPONENTS_SERVICES_DYNAMIC_LOADER_SERVICE_NOTIFICATION_H

#include "mysql/components/service.h"

/**
  @file mysql/components/services/dynamic_loader_service_notification.h
  Connection event tracking.
*/

/**
  @ingroup group_components_services_inventory

  A service to notify consumers about services loaded
  through dynamic loader.

  @sa The convenience class @ref weak_service_reference
*/

BEGIN_SERVICE_DEFINITION(dynamic_loader_services_loaded_notification)

/**
  Notify about services that are recently loaded

  @param [in] services Names to services recently loaded
  @param [in] count    Number of services recently loaded

  @returns status of notification
    @retval false Success
    @retval true  Failure
*/

DECLARE_BOOL_METHOD(notify, (const char **services, unsigned int count));

END_SERVICE_DEFINITION(dynamic_loader_services_loaded_notification)

/**
  @ingroup group_components_services_inventory

  A service to notify consumers about services to be unloaded
  through dynamic loader.
*/

BEGIN_SERVICE_DEFINITION(dynamic_loader_services_unload_notification)

/**
  Notify about services that are about to be unloaded

  @param [in] services Names to services being unloaded
  @param [in] count    Number of services being unloaded

  @returns status of notification
    @retval false Success
    @retval true  Failure
*/

DECLARE_BOOL_METHOD(notify, (const char **services, unsigned int count));

END_SERVICE_DEFINITION(dynamic_loader_services_unload_notification)

#endif  // !MYSQL_COMPONENTS_SERVICES_DYNAMIC_LOADER_SERVICE_NOTIFICATION_H
