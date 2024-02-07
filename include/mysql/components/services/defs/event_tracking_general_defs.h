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

#ifndef COMPONENT_SERVICES_DEFS_EVENT_TRACKING_GENERAL_DEFS_H
#define COMPONENT_SERVICES_DEFS_EVENT_TRACKING_GENERAL_DEFS_H

#include "mysql/components/services/defs/event_tracking_common_defs.h"

/**
  @file mysql/components/services/defs/event_tracking_general_defs.h
  Data for general event tracking.
*/

/** occurs before emitting to the general query log. */
#define EVENT_TRACKING_GENERAL_LOG (1 << 0)
/** occurs before transmitting errors to the user. */
#define EVENT_TRACKING_GENERAL_ERROR (1 << 1)
/** occurs after transmitting a resultset to the user. */
#define EVENT_TRACKING_GENERAL_RESULT (1 << 2)
/** occurs after transmitting a resultset or errors */
#define EVENT_TRACKING_GENERAL_STATUS (1 << 3)

#define EVENT_TRACKING_GENERAL_ALL                            \
  EVENT_TRACKING_GENERAL_LOG | EVENT_TRACKING_GENERAL_ERROR | \
      EVENT_TRACKING_GENERAL_RESULT | EVENT_TRACKING_GENERAL_STATUS

/**
  @typedef mysql_event_tracking_general_subclass_t

  Events for the General event tracking.
*/
typedef unsigned long mysql_event_tracking_general_subclass_t;

/**
  @struct mysql_event_tracking_general_data

  Structure for General event tracking.
*/
struct mysql_event_tracking_general_data {
  /** Event subclass */
  mysql_event_tracking_general_subclass_t event_subclass;
  /** Error code if any*/
  int error_code;
  /** Connection ID */
  mysql_connection_id connection_id;
  /** User name of this connection */
  mysql_cstring_with_length user;
  /** Connection host */
  mysql_cstring_with_length host;
  /** Connection IP */
  mysql_cstring_with_length ip;
};

#endif  // !COMPONENT_SERVICES_DEFS_EVENT_TRACKING_GENERAL_DEFS_H
