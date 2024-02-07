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

#ifndef COMPONENTS_SERVICES_EVENT_TRACKING_CONNECTION_SERVICE_H
#define COMPONENTS_SERVICES_EVENT_TRACKING_CONNECTION_SERVICE_H

#include "mysql/components/service.h"
#include "mysql/components/services/defs/event_tracking_connection_defs.h"

/**
  @file mysql/components/services/event_tracking_connection_service.h
  Connection event tracking.

  @sa @ref EVENT_TRACKING_CONNECTION_CONSUMER_EXAMPLE
*/

/**
  @ingroup event_tracking_services_inventory

  @anchor EVENT_TRACKING_CONNECTION_SERVICE

  A service to track and consume connection events.

  Producer of the event will broadcast notify all interested
  consumers of the event.

  @sa @ref EVENT_TRACKING_CONNECTION_CONSUMER_EXAMPLE
*/

BEGIN_SERVICE_DEFINITION(event_tracking_connection)

/**
  Process a connection event

  @param [in] data  Event specific data

  @returns Status of processing the event
    @retval false Success
    @retval true  Error
*/
DECLARE_BOOL_METHOD(notify, (const mysql_event_tracking_connection_data *data));

END_SERVICE_DEFINITION(event_tracking_connection)

#endif  // COMPONENTS_SERVICES_EVENT__TRACKING_CONNECTION_SERVICE_H
