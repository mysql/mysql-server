/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#ifndef COMPONENTS_SERVICES_MYSQL_THD_STORE_SERVICE_H
#define COMPONENTS_SERVICES_MYSQL_THD_STORE_SERVICE_H

#include <mysql/components/service.h>
#include <mysql/components/services/mysql_current_thread_reader.h>  // MYSQL_THD

DEFINE_SERVICE_HANDLE(mysql_thd_store_slot);

/**
  Callback to free resource stored in THD

  returns 0 for success, 1 otherwise.
*/
typedef int (*free_resource_fn)(void *);

/**
  @file mysql/components/services/mysql_thd_store_service.h
  Connection event tracking.
*/

/**
  @ingroup event_tracking_services_inventory

  A service to store an opaque pointer in MYSQL_THD
*/

BEGIN_SERVICE_DEFINITION(mysql_thd_store)

/**
  Register a slot to store data specific to a component

  The free_resource callback is used to free the stored pointer
  before thd is destroyed

  @param [in]  name     Implementation name
  @param [in]  free_fn  Callback to free resource stored in the slot
  @param [out] slot     Key used to identify the object handle

  @returns status of registration
    @retval false Success
    @retval true  Error. This typically means all slots are full
*/
DECLARE_BOOL_METHOD(register_slot, (const char *name, free_resource_fn free_fn,
                                    mysql_thd_store_slot *slot));

/**
  Unregister a slot

  @param [in, out] slot Key allocated to component

  @returns status of operation
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(unregister_slot, (mysql_thd_store_slot slot));

/**
  Set/Reset an opaque pointer in thd store.

  @param [in] thd     Session handle. If NULL, current session will be
                      used.
  @param [in] slot    Key used to identify the object handle
  @param [in] object  Handle of the object being stored. If NULL, it will
                      be considered removal.

  @returns Status of the operation
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(set,
                    (MYSQL_THD thd, mysql_thd_store_slot slot, void *object));

/**
  Get handle to an already stored object

  @param [in]  thd     Session handle. If NULL, current session will be used.
  @param [in]  slot    Key used to identify the object handle

  @returns handle to the object if found, nullptr otherwise
*/
DECLARE_METHOD(void *, get, (MYSQL_THD thd, mysql_thd_store_slot slot));

END_SERVICE_DEFINITION(mysql_thd_store)

#endif  // !COMPONENTS_SERVICES_MYSQL_THD_STORE_SERVICE_H
