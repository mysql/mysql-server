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

#ifndef MYSQL_COMPONENTS_SERVICES_EVENT_TRACKING_AUTHENTICATION_SERVICE_H
#define MYSQL_COMPONENTS_SERVICES_EVENT_TRACKING_AUTHENTICATION_SERVICE_H

#include "mysql/components/service.h"
#include "mysql/components/services/defs/event_tracking_authentication_defs.h"

/**
  @file mysql/components/services/event_tracking_authentication_service.h
  Authentication event tracking.

  @sa @ref EVENT_TRACKING_AUTHENTICATION_CONSUMER_EXAMPLE
*/

/**
  @defgroup event_tracking_services_inventory Event tracking services
  @ingroup group_components_services_inventory
*/

/** A handle to obtain details related to authentication event */
DEFINE_SERVICE_HANDLE(event_tracking_authentication_information_handle);

/** A handle to obtain details related to authentication method */
DEFINE_SERVICE_HANDLE(event_tracking_authentication_method_handle);

/**
  @ingroup event_tracking_services_inventory

  @anchor EVENT_TRACKING_AUTHENTICATION_SERVICE

  A service to track and consume authentication events.

  Producer of the event will broadcast notify all interested
  consumers of the event.

  @sa @ref EVENT_TRACKING_AUTHENTICATION_CONSUMER_EXAMPLE
*/
BEGIN_SERVICE_DEFINITION(event_tracking_authentication)

/**
  Process a authentication event

  @param [in] common_handle  Handle for common event trackign data
  @param [in] handle         Handle to retrieve additional data
                             about event. It is guaranteed to be
                             valid for the duration of the API.
  @param [in] data           Event specific data

  @returns Status of processing the event
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(notify,
                    (const mysql_event_tracking_authentication_data *data));

END_SERVICE_DEFINITION(event_tracking_authentication)

/**
  @ingroup event_tracking_services_inventory

  @anchor EVENT_TRACKING_AUTHENTICATION_INFORMATION

  A service to fetch additional data about authentication event
*/

BEGIN_SERVICE_DEFINITION(event_tracking_authentication_information)

/**
  Initialize authentication event data handle

  @param [out] handle  Handle to authentication event data

  @returns Status of handle creation
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(init, (event_tracking_authentication_information_handle *
                           handle));

/**
  Deinitialize authentication event data handle

  @param [in, out] handle Handle to be deinitialized

  @returns Status of operation
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(deinit,
                    (event_tracking_authentication_information_handle handle));

/**
  Get information about given authentication event

  Accepted names and corresponding value type

  "authentcation_method_count" -> unsigned int *
  "new_user" -> mysql_cstring_with_length *
  "new_host" -> mysql_cstring_with_length *
  "is_role" -> boolean *
  "authentication_method_info" -> event_tracking_authentication_method_handle

  @param [in]  handle Event tracking information handle
  @param [in]  name   Data identifier
  @param [out] value  Value of the identifier

  @returns status of the operation
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(get,
                    (event_tracking_authentication_information_handle handle,
                     const char *name, void *value));

END_SERVICE_DEFINITION(event_tracking_authentication_information)

/**
  @ingroup event_tracking_services_inventory

  @anchor EVENT_TRACKING_AUTHENTICATION_METHOD

  A service to fetch additional data about authentication method
*/

BEGIN_SERVICE_DEFINITION(event_tracking_authentication_method)

/**
  Get information about authentication method

  Accepted names and corresponding value type

  "name" -> mysql_cstring_with_length *

  @param [in]  handle  Handle to authentication method structure
                       Valid until
                       @sa event_tracking_authentication_information_handle_imp
                       is valid
  @param [in]  index   Location
  @param [in]  name    Data identifier
  @param [out] value   Data value

  @returns status of the operation
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(get, (event_tracking_authentication_method_handle handle,
                          unsigned int index, const char *name, void *value));

END_SERVICE_DEFINITION(event_tracking_authentication_method)

#endif  // !MYSQL_COMPONENTS_SERVICES_EVENT_TRACKING_AUTHENTICATION_SERVICE_H
