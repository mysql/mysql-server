/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_OPTION_TRACKER_H
#define MYSQL_OPTION_TRACKER_H

#include <mysql/components/service.h>
#include <cstddef>

/**
  @ingroup group_components_services_inventory

  Option tracker registration and deregistration services

  This is a service that will allow registering an option.
  Each option has a name. The name is UTF8mb4 and is unique in
  the list.
  Manipulating the option list is an "expesive" operation since there
  is a global lock involved.

  Each code container (a component or a plugin) should register its
  options during its initialization and should unregister them during
  its deinitialization.
*/
BEGIN_SERVICE_DEFINITION(mysql_option_tracker_option)

/**
  Define an option. Adds an option definition.

  If another option of the same name exists, the definition fails

  @param option            The name of the option, UTF8mb4. Must be unique.
  @param container         The container name. UTF8mb4
                            Please prefix with "plugin_" for plugins.
  @param is_enabled        non-0 if the option is marked as enabled, 0 otherwise
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(define, (const char *option, const char *container,
                             int is_enabled));
/**
  Undefine an option.

  Fails if no option is defined with the same name

  @param option            The name of the option, US ASCII
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(undefine, (const char *option));

/**
  Set enabled for an existing element

  if the option is not defined it fails

  @param option            The name of the option, US ASCII
  @param is_enabled        non-0 if the option is marked as enabled, 0 otherwise
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(set_enabled, (const char *option, int is_enabled));

END_SERVICE_DEFINITION(mysql_option_tracker_option)

/**
  @ingroup group_components_services_inventory

  Option tracker usage marker

  Sets usage data for a given option.
  Internally stores into the system table.

  Cluster ID is set to empty.

  It gets the value for server_id from the system variable
  server_uuid.
*/
BEGIN_SERVICE_DEFINITION(mysql_option_tracker_usage)
/**
  Set usage data. Sets the persisted state

  @param option            The name of the option, US ASCII
  @param usage_data_json   Usage data, JSON, zero terminated UTF-8
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(set, (const char *option, const char *usage_data_json));
/**
  Get usage data. Reads the persisted state.
  Stores a 0-terminated UTF-8 data into the supplied buffer. If the data to
  be stored are too long for the buffer the function fails.

  Reading is done in a separate auto-commit transaction.

  @param option            The name of the option, US ASCII
  @param [out] usage_data  A buffer to return the UTF-8 data in.
  @param sizeof_usage_data The size of the usage_data_buffer in bytes.
  otherwise.
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(get, (const char *option, char *usage_data,
                          size_t sizeof_usage_data));
END_SERVICE_DEFINITION(mysql_option_tracker_usage)

typedef bool (*mysql_option_tracker_usage_cache_update_callback)(
    unsigned long long new_value);
/**
  @ingroup group_components_services_inventory

  Option tracker usage cache updater callbacks registry

  Handles the reset of the intial cached values when the persisted data changes.

  The idea is that each component needs to register a callback to be called when
  there's update of the persited values coming via means different from the
  mysql_option_tracker_usage::set method. The callback is supposed to update the
  in-memory status variable cache for that option.
  Call add() after reading the status value.
  Call delete() when removing the component and the callback was added.
  Expect offline calls to the callback when data are updated via the GR
  signalling service.
*/
BEGIN_SERVICE_DEFINITION(mysql_option_tracker_usage_cache_callbacks)
/**
  Call this when the component is initalized. Pass a callback pointer
  that will, when called, set the value of the cache to the value passed

  @param option_name the name of the option to add callback to
  @param callback a function pointer to a function to be called to set the new
  value
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(
    add, (const char *option_name,
          mysql_option_tracker_usage_cache_update_callback callback));
/**
  Call this when the component is de-initalized and a callback has been added.
  Pass the same callback pointer as the one passed to add(). It will be checked.

  @param option_name the name of the option to add callback to
  @param callback for verification; the same callback function as is passed to
  add
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(
    remove, (const char *option_name,
             mysql_option_tracker_usage_cache_update_callback callback));
END_SERVICE_DEFINITION(mysql_option_tracker_usage_cache_callbacks)

#endif /* MYSQL_OPTION_TRACKER_H */
